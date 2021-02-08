#include "gravityoperations.h"

#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtCore/QLoggingCategory>

#include <HemeraCore/Literals>
#include <HemeraCore/CommonOperations>

#include <libudev.h>
#include <sys/stat.h>

#include "systemdmanagerinterface.h"

#define GPT_DEFAULT_TYPE "8300"
#define MBR_DEFAULT_TYPE "0x83"
#define GPT_FACTORY_RESET_TYPE "d1c18431-2ce9-4d8a-8a27-ab276095c9e1"
#define MBR_FACTORY_RESET_TYPE "0x64"

Q_LOGGING_CATEGORY(LOG_TOOLOPERATION, "Gravity::ToolOperation")
Q_LOGGING_CATEGORY(LOG_FACTORYRESETOPERATION, "Gravity::RestoreFactoryResetOperation")

namespace Gravity
{

class ControlUnitOperation::Private
{
public:
    Private() : jobId(0) {}

    QString unit;
    QString mode;

    org::freedesktop::systemd1::Manager *manager;

    uint jobId;

    ControlUnitOperation::Mode operationMode;
};

ControlUnitOperation::ControlUnitOperation(const QString& unit, const QString& mode, Gravity::ControlUnitOperation::Mode operationMode,
                                           OrgFreedesktopSystemd1ManagerInterface *manager, QObject *parent)
    : Operation(parent)
    , d(new Private)
{
    d->manager = manager;
    d->unit = unit;
    d->mode = mode;
    d->operationMode = operationMode;
}

ControlUnitOperation::~ControlUnitOperation()
{
    delete d;
}

void ControlUnitOperation::startImpl()
{
    if (!d->manager) {
        // Create a custom interface for the purpose.
        d->manager = new org::freedesktop::systemd1::Manager(QStringLiteral("org.freedesktop.systemd1"),
                                                             QStringLiteral("/org/freedesktop/systemd1"),
                                                             QDBusConnection::systemBus(), this);

        if (!d->manager->isValid()) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                                 QStringLiteral("Systemd manager interface could not be found."));
            return;
        }
    }

    // React on job finished
    connect(d->manager, &org::freedesktop::systemd1::Manager::JobRemoved,
            this, &ControlUnitOperation::checkResult);

    // Go
    QDBusPendingReply<QDBusObjectPath> jobPath;
    switch (d->operationMode) {
        case ControlUnitOperation::Mode::StartMode:
            jobPath = d->manager->StartUnit(d->unit, d->mode);
            break;
        case ControlUnitOperation::Mode::StopMode:
            jobPath = d->manager->StopUnit(d->unit, d->mode);
            break;
        case ControlUnitOperation::Mode::RestartMode:
            jobPath = d->manager->TryRestartUnit(d->unit, d->mode);
            break;
        default:
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                 QStringLiteral("The library supplied an unknown operation mode for ControlUnitOperation."));
                                 return;
    }

    auto onJobPathFinished = [this] (QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            setFinishedWithError(reply.error());
            return;
        }

        d->jobId = reply.value().path().split(QLatin1Char('/')).last().toUInt();
    };

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(jobPath);
    if (watcher->isFinished()) {
        onJobPathFinished(watcher);
    } else {
        connect(watcher, &QDBusPendingCallWatcher::finished, onJobPathFinished);
    }
}

void ControlUnitOperation::checkResult(uint id, const QDBusObjectPath& , const QString& , const QString& result)
{
    if (id == d->jobId) {
        if (result == QStringLiteral("done")) {
            // Success
            setFinished();
        } else if (result == QStringLiteral("timeout")) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::timeout()),
                                 QStringLiteral("Application start operation timed out"));
        } else if (result == QStringLiteral("dependency")) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::applicationStartFailed()),
                                 QStringLiteral("A dependency of the application failed to start"));
        } else if (result == QStringLiteral("canceled")) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::canceled()),
                                 QStringLiteral("The application startup was canceled"));
        } else if (result == QStringLiteral("failed")) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::applicationStartFailed()),
                                 QStringLiteral("The application startup failed"));
        } else if (result == QStringLiteral("skipped")) {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::canceled()),
                                 QStringLiteral("The application startup was skipped"));
        } else {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                 QStringLiteral("Systemd returned an unknown job result: ") + result);
        }
    }
}

class ToolOperation::Private
{
public:
    Private()
        : process(nullptr),
          success(false)
    {}
    QString toolPath;
    QStringList toolArgs;
    QProcess *process;
    bool success;
};

ToolOperation::ToolOperation(const QString &path, const QStringList &args, QObject *parent)
    : Operation(Operation::ExplicitStartOption, parent)
    , d(new Private)
{
    d->toolPath = path;
    d->toolArgs = args;
}

ToolOperation::~ToolOperation()
{
    delete d;
}

void ToolOperation::startImpl()
{
    if (!QFile::exists(d->toolPath)) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                             QStringLiteral("Error: %1 is missing.").arg(d->toolPath));
        return;
    }
    d->process = new QProcess(this);

    connect(d->process, &QProcess::readyReadStandardOutput, this, [this] () {
        qCDebug(LOG_TOOLOPERATION) << d->process->readAllStandardOutput();
    });
    connect(d->process, &QProcess::readyReadStandardError, this, [this] () {
        qCDebug(LOG_TOOLOPERATION) << d->process->readAllStandardError();
    });
    QObject::connect(d->process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [this] (int exitCode, QProcess::ExitStatus exitStatus) {
        d->success = ((exitStatus == QProcess::NormalExit) && (exitCode == 0));
        if (d->success) {
            setFinished();
        } else {
            setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                 QStringLiteral("Failed to execute %s.").arg(d->toolPath));
                                 return;
        }
    });
    qCDebug(LOG_TOOLOPERATION) << "Launching: " << d->toolPath << " " << d->toolArgs;
    d->process->start(d->toolPath, d->toolArgs);
}

class RestoreFactoryResetOperation::Private
{
public:
    Private(){}

    QString configPath;
    QString sgdiskPath;
    QString sfdiskPath;
};

RestoreFactoryResetOperation::RestoreFactoryResetOperation(const QString &configPath, const QString &sgdiskPath, const QString &sfdiskPath, QObject *parent)
    : Operation(parent)
    , d(new Private)
{
    d->configPath = configPath;
    d->sgdiskPath = sgdiskPath;
    d->sfdiskPath = sfdiskPath;
}

RestoreFactoryResetOperation::~RestoreFactoryResetOperation()
{
    delete d;
}

void RestoreFactoryResetOperation::startImpl()
{
    QFile configFile(d->configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qCDebug(LOG_FACTORYRESETOPERATION) << QStringLiteral("Factory reset config %1 not found.").arg(d->configPath);
        setFinished();
        return;
    }

    struct udev *udev = udev_new();
    if (!udev) {
        setFinishedWithError(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                             QStringLiteral("Can't create udev context"));
        return;
    }

    QList<Hemera::Operation *> operations;

    QByteArray configData = configFile.readAll();
    QJsonDocument configDoc(QJsonDocument::fromJson(configData));
    QJsonObject json = configDoc.object();

    for (const QJsonValue &disk : json.value(QStringLiteral("disks")).toArray()) {

        if (!disk.isObject()) {
            qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Disk is not a valid JSON object. Check %1").arg(d->configPath);
            continue;
        }

        QJsonObject diskObject = disk.toObject();
        QString name = diskObject.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Disk name is empty. Check %1").arg(d->configPath);
            continue;
        }

        QString type = diskObject.value(QStringLiteral("type")).toString();

        QJsonArray partitions = diskObject.value(QStringLiteral("partition-numbers")).toArray();

        // If the name doesn't start with /dev, assume it's a label
        if (!name.startsWith(QStringLiteral("/dev"))) {
            QPair<QString, QString> nameAndPartition = diskNameAndPartitionFromLabel(name);
            name = nameAndPartition.first;
            partitions = QJsonArray() << nameAndPartition.second;
        }

        if (type.toLower() == QStringLiteral("gpt")) {

            for (const QJsonValue &partnum : partitions) {
                QString devPath = name + (name.contains(QStringLiteral("mmcblk")) ? QStringLiteral("p") : QStringLiteral("")) + partnum.toString();
                if (checkFactoryReset(devPath, udev)) {
                    QStringList args{name, QStringLiteral("-t"), partnum.toString() + QStringLiteral(":" GPT_DEFAULT_TYPE)};
                    ToolOperation *sgdiskOp = new ToolOperation(d->sgdiskPath, args, this);
                    operations.append(sgdiskOp);
                }
            }

        } else if (type.toLower() == QStringLiteral("mbr")) {

            for (const QJsonValue &partnum: partitions) {
                QString devPath = name + (name.contains(QStringLiteral("mmcblk")) ? QStringLiteral("p") : QStringLiteral("")) + partnum.toString();
                if (checkFactoryReset(devPath, udev)) {
                    QStringList args{name, partnum.toString(), QStringLiteral("-c"), QStringLiteral(MBR_DEFAULT_TYPE)};
                    ToolOperation *sfdiskOp = new ToolOperation(d->sfdiskPath, args, this);
                    operations.append(sfdiskOp);
                }
            }

        } else {
            qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Disk %1 must be of type GPT or MBR. Check %2").arg(name, d->configPath);
        }
    }

    udev_unref(udev);

    if (!operations.isEmpty()){
        Hemera::SequentialOperation *resetDefaultsSequence = new Hemera::SequentialOperation(operations, this);
        connect(resetDefaultsSequence, &Hemera::Operation::finished, this, [this] (Hemera::Operation *op) {
            if (!op->isError()) {
                setFinished();
            } else {
                setFinishedWithError(op->errorName(), op->errorMessage());
            }
        });
    } else {
        qCDebug(LOG_FACTORYRESETOPERATION) << QStringLiteral("No partitions to restore");
        setFinished();
    }
}

bool RestoreFactoryResetOperation::checkFactoryReset(const QString &devicePath, struct udev *udevContext)
{
    struct stat deviceStat;
    if (stat(devicePath.toLatin1().constData(), &deviceStat) < 0) {
        qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Failed to stat device %1").arg(devicePath);
        return false;
    }

    struct udev_device *udevDevice = udev_device_new_from_devnum(udevContext, 'b', deviceStat.st_rdev);
    if (!udevDevice) {
        qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Failed to detect device %1").arg(devicePath);
        return false;
    }

    bool result;
    QLatin1String partitionType(udev_device_get_property_value(udevDevice, "ID_PART_ENTRY_TYPE"));
    if (partitionType == QStringLiteral(GPT_FACTORY_RESET_TYPE) || partitionType == QStringLiteral(MBR_FACTORY_RESET_TYPE)) {
        result = true;
        qCDebug(LOG_FACTORYRESETOPERATION) << QStringLiteral("Restoring %1 after factory reset").arg(devicePath);
        Q_EMIT deviceWasFactoryReset(devicePath);
    } else {
        result = false;
    }

    udev_device_unref(udevDevice);

    return result;
}

QPair<QString, QString> RestoreFactoryResetOperation::diskNameAndPartitionFromLabel(const QString &label)
{
    QString devPath = QFile::symLinkTarget(QStringLiteral("/dev/disk/by-label/%1").arg(label));
    if (!QFile::exists(devPath)) {
        qCWarning(LOG_FACTORYRESETOPERATION) << QStringLiteral("Unable to find device with label %1").arg(label);
        return qMakePair(QString(), QString());
    }
    QRegularExpression re(QStringLiteral("^(/dev/[a-zA-z]+)(\\dp)?(\\d+)$"));
    QRegularExpressionMatch match = re.match(devPath);
    qCDebug(LOG_FACTORYRESETOPERATION) << "diskNameAndPartitionFromLabel match " << match.capturedTexts();
    QString partition = match.captured(3);
    // We remove the trailing p to be consistent with the json config file
    QString mmcSuffix = match.captured(2).remove(QLatin1Char('p'));
    QString diskName = QStringLiteral("%1%2").arg(match.captured(1), mmcSuffix);
    return qMakePair(diskName, partition);
}

}
