#include "tmpfileshook.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(LOG_TMPFILESHOOK, "TmpfilesHook")

TmpfilesHook::TmpfilesHook(QObject *parent)
    : Gravity::RemountHelperPlugin(parent)
{
}

TmpfilesHook::~TmpfilesHook()
{
}

void TmpfilesHook::preRemountReadOnlyHook()
{
    QStringList relevantTmpfiles;
    // Backup tmpfiles!
    QDir etctmpfiles(QStringLiteral("/etc/tmpfiles.d"));
    QDir usrtmpfiles(QStringLiteral("/usr/lib/tmpfiles.d"));
    QStringList fileFilter = QStringList() << QStringLiteral("hemera-*.conf") << QStringLiteral("gravity-*.conf");

    for (const QString &fileName : etctmpfiles.entryList(fileFilter, QDir::Files)) {
        relevantTmpfiles << etctmpfiles.absoluteFilePath(fileName);
    }
    for (const QString &fileName : usrtmpfiles.entryList(fileFilter, QDir::Files)) {
        relevantTmpfiles << usrtmpfiles.absoluteFilePath(fileName);
    }

    qCDebug(LOG_TMPFILESHOOK) << "Considering files:" << relevantTmpfiles;

    QHash< QString, QString > toCopy;

    for (const QString &fileName : relevantTmpfiles) {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(LOG_TMPFILESHOOK) << "Could not open" << fileName;
            continue;
        }

        while (!file.atEnd()) {
            QString line = QString::fromLatin1(file.readLine());
            line.remove(QLatin1Char('\n'));

            if (line.startsWith(QLatin1Char('C'))) {
                QStringList entry = line.split(QLatin1Char(' '));
                toCopy.insert(entry.at(1), entry.last());
            }
        }
    }

    qCDebug(LOG_TMPFILESHOOK) << "Will copy:" << toCopy;

    QDir mkpathHelper;
    for (QHash< QString, QString >::const_iterator i = toCopy.constBegin(); i != toCopy.constEnd(); ++i) {
        // Do we have a previous backup?
        if (QFile::exists(i.value()) && QFile::exists(QStringLiteral("%1.md5sum").arg(i.value()))) {
            // Verify checksum
            QFile f(i.key());
            QFile v(QStringLiteral("%1.md5sum").arg(i.value()));
            if (f.open(QFile::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Md5);
                if (hash.addData(&f)) {
                    v.open(QFile::ReadOnly);
                    if (hash.result() == v.readAll()) {
                        qCInfo(LOG_TMPFILESHOOK) << "Hash is the same, skipping copy for " << i.key();
                        continue;
                    }
                }
            }
        }

        // Make sure the directory is up.
        QString dirPath = i.value().mid(0, i.value().lastIndexOf(QLatin1Char('/')));
        if (!mkpathHelper.mkpath(dirPath)) {
            qCWarning(LOG_TMPFILESHOOK) << "Could not create path for" << dirPath;
        }

        if (!QFile::copy(i.key(), i.value())) {
            qCWarning(LOG_TMPFILESHOOK) << "Could not copy" << i.key() << "to" << i.value();

            // Remove temporary leftover, if any
            if (QFile::exists(i.value())) {
                if (!QFile::remove(i.value())) {
                    qCWarning(LOG_TMPFILESHOOK) << "Could remove partial copy of" << i.value() << ", your system might have corrupt backup files.";
                }
            }
        }

        // Write hash
        QFile f(i.key());
        QFile v(QStringLiteral("%1.md5sum").arg(i.value()));
        if (f.open(QFile::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Md5);
            if (hash.addData(&f)) {
                v.open(QFile::WriteOnly | QFile::Truncate);
                v.write(hash.result());
            }
        }
    }
}

void TmpfilesHook::postRemountReadOnlyHook()
{
    // Nothing to do
}

void TmpfilesHook::preRemountReadWriteHook()
{
    // Nothing to do
}

void TmpfilesHook::postRemountReadWriteHook()
{
    // Nothing to do
}

