#include "gravityremounthelpercore.h"

#include <GravityRemountHelper/Plugin>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>
#include <QtCore/QPluginLoader>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Operation>

#include <systemd/sd-daemon.h>

#include <gravityconfig.h>


#define REMOUNT_CONFIG_FILE QStringLiteral("/etc/hemera/gravity/remount.conf")

Q_LOGGING_CATEGORY(LOG_REMOUNTHELPERCORE, "GravityRemountHelperCore")

RemountFsOperation::RemountFsOperation(const QString &mode, const QString &fs, QObject *parent)
    : Hemera::Operation(parent)
    , m_mode(mode)
    , m_fs(fs)
    , m_process(new QProcess(this))
{
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(onProcessFinished(int,QProcess::ExitStatus)));
}

RemountFsOperation::~RemountFsOperation()
{
}

void RemountFsOperation::startImpl()
{
    m_process->start(QStringLiteral("/bin/mount -o remount,%1 %2").arg(m_mode, m_fs));
}

void RemountFsOperation::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                             QStringLiteral("mount exited with %1").arg(exitCode));
        return;
    }

    setFinished();
}

GravityRemountHelperCore::GravityRemountHelperCore(const QString &mode, bool hooksOnly, QObject *parent)
    : AsyncInitDBusObject(parent)
    , m_mode(mode)
    , m_hooksOnly(hooksOnly)
{
    if (m_mode == QStringLiteral("rw")) {
        m_mode = QStringLiteral("rw,noatime");
    }
}

GravityRemountHelperCore::~GravityRemountHelperCore()
{
}

void GravityRemountHelperCore::initImpl()
{
    QTimer::singleShot(0, this, SLOT(executePreHooks()));

    // Load plugins
    QDir pluginsDir(QLatin1String(Gravity::StaticConfig::gravityRemountHelperHooksPath()));
    QStringList availablePluginDefinitions;

    for (const QString &fileName : pluginsDir.entryList(QStringList() << QStringLiteral("*.so"), QDir::Files)) {
        // Create plugin loader
        QPluginLoader* pluginLoader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName), this);
        // Load plugin
        if (!pluginLoader->load()) {
            qCWarning(LOG_REMOUNTHELPERCORE) << "Could not init plugin loader:" << pluginLoader->errorString();
            pluginLoader->deleteLater();
            continue;
        }

        QObject *plugin = pluginLoader->instance();
        if (plugin) {
            // Plugin instance created
            Gravity::RemountHelperPlugin *remountHelperHook  = qobject_cast<Gravity::RemountHelperPlugin*>(plugin);
            if (!remountHelperHook) {
                // Interface was wrong
                qCWarning(LOG_REMOUNTHELPERCORE) << "Fail!!" << pluginLoader->errorString();
            } else {
                m_hooks << remountHelperHook;
            }
        } else {
            qCWarning(LOG_REMOUNTHELPERCORE) << "Could not load plugin" << fileName;
        }

        pluginLoader->deleteLater();
    }

    setReady();
}

void GravityRemountHelperCore::executePreHooks()
{
    // execute pre hooks
    for (Gravity::RemountHelperPlugin *hook : m_hooks) {
        if (m_mode.startsWith(QStringLiteral("rw"))) {
            hook->preRemountReadWriteHook();
        } else {
            hook->preRemountReadOnlyHook();
        }
    }

    // Remount or post hook?
    if (m_hooksOnly) {
        QTimer::singleShot(0, this, SLOT(executePostHooks()));
    } else {
        QTimer::singleShot(0, this, SLOT(remount()));
    }
}

void GravityRemountHelperCore::executePostHooks()
{
    // execute post hooks
    for (Gravity::RemountHelperPlugin *hook : m_hooks) {
        if (m_mode.startsWith(QStringLiteral("rw"))) {
            hook->postRemountReadWriteHook();
        } else {
            hook->postRemountReadOnlyHook();
        }
    }

    // Notify systemd
    sd_notify(0, "READY=1");

    // Quit gracefully
    QCoreApplication::instance()->quit();
}

void GravityRemountHelperCore::remount()
{
    m_filesystemsToRemount.clear();
    m_remountedFilesystems.clear();
    m_filesystemsToRemount.append(QStringLiteral("/"));

    // pull other filesystems which should be remounted (if any)
    QFile remountConfig(REMOUNT_CONFIG_FILE);
    if (remountConfig.open(QIODevice::ReadOnly)) {
        while (!remountConfig.atEnd()) {
            QByteArray candidate = remountConfig.readLine();
            if (!candidate.isEmpty()) {
                m_filesystemsToRemount.append(QString::fromLatin1(candidate));
            }
        }
    }

    qCDebug(LOG_REMOUNTHELPERCORE) << "Will remount" << m_filesystemsToRemount;

    // execute
    remountNext();
}

void GravityRemountHelperCore::rollbackRemount()
{
    // Rollback in a sequential operation, and fail afterwards.
    QList<Hemera::Operation*> ops;

    // Reverse mode
    QString mode;
    if (m_mode.startsWith(QStringLiteral("rw"))) {
        mode = QStringLiteral("ro");
    } else {
        mode = QStringLiteral("rw,noatime");
    }

    for (const QString &fs : m_remountedFilesystems) {
        ops.append(new RemountFsOperation(mode, fs));
    }

    if (ops.isEmpty()) {
        QCoreApplication::exit(1);
        return;
    }

    Hemera::SequentialOperation *sop = new Hemera::SequentialOperation(ops, this);

    connect(sop, &Hemera::Operation::finished, [this, sop] {
        if (sop->isError()) {
            qCWarning(LOG_REMOUNTHELPERCORE) << "Rollback failed!";
        }

        QCoreApplication::exit(1);
    });
}

void GravityRemountHelperCore::remountNext()
{
    if (m_filesystemsToRemount.isEmpty()) {
        // Done, execute post hooks
        QTimer::singleShot(0, this, SLOT(executePostHooks()));
        return;
    }

    // Remount
    QString fs = m_filesystemsToRemount.takeFirst();
    qCDebug(LOG_REMOUNTHELPERCORE) << "Remounting" << fs;
    Hemera::Operation *op = new RemountFsOperation(m_mode, fs);
    connect(op, &Hemera::Operation::finished, [this, op, fs] {
        if (op->isError()) {
            // rollback
            qCWarning(LOG_REMOUNTHELPERCORE) << "Remount failed! Rolling back...";
            rollbackRemount();
            return;
        }

        // Good. Let's add and recurse.
        m_remountedFilesystems.append(fs);
        remountNext();
    });
}
