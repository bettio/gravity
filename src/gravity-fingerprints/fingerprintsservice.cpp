#include "fingerprintsservice.h"

#include "genericfingerprintprovider.h"

#include <stdio.h>
#include <unistd.h>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QPluginLoader>

#include <QtDBus/QDBusConnection>

#include <HemeraCore/Literals>

#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

#include <gravityconfig.h>

#include <sys/types.h>
#include <pwd.h>

#include "fingerprintsadaptor.h"

#define FINGERPRINTS_PATH "/var/lib/hemera/fingerprints/"
#define STOREDFINGERPRINT_PATH "/var/lib/hemera/fingerprints/stored_fingerprint"
#define HARDWAREFINGERPRINT_PATH "/var/lib/hemera/fingerprints/hardware_fingerprint"


FingerprintsService::FingerprintsService()
    : Hemera::AsyncInitObject(Q_NULLPTR)
{
}

FingerprintsService::~FingerprintsService()
{
}

void FingerprintsService::initImpl()
{
    if (!QDBusConnection::systemBus().registerService(Hemera::Literals::literal(Hemera::Literals::DBus::fingerprintsService()))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("Could not register the service on DBus. This means either a wrong installation or a corrupted instance."));
        return;
    }
    if (!QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::fingerprintsPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("Could not register the object on DBus. This means either a wrong installation or a corrupted instance."));
        return;
    }
    new FingerprintsAdaptor(this);

    m_fallbackProvider = new GenericFingerprintProvider(this);

    setReady();
}

void FingerprintsService::loadProviderPlugin()
{
    // Init provider
    m_provider.clear();

    QDir pluginsDir(QLatin1String(Gravity::StaticConfig::hemeraFingerprintsPluginsPath()));

    QFileInfoList pluginInfoList = pluginsDir.entryInfoList(QStringList() << QStringLiteral("*.so"), QDir::Files);

    if (pluginInfoList.isEmpty()) {
        qDebug() << "No providers available, will fallback to generic.";
        return;
    }

    if (pluginInfoList.size() > 1) {
        qDebug() << "More than one provider available, will try sequentially.";
    }

    for (const QFileInfo &file : pluginInfoList) {
        // Create plugin loader
        QPluginLoader* pluginLoader = new QPluginLoader(file.absoluteFilePath(), this);
        // Load plugin
        if (!pluginLoader->load()) {
            qDebug() << "Could not init plugin loader:" << pluginLoader->errorString();
            pluginLoader->deleteLater();
            continue;
        }

        QObject *plugin = pluginLoader->instance();
        if (plugin) {
            // Plugin instance created
            m_provider = qobject_cast< Gravity::FingerprintProviderPlugin* >(plugin);
            if (m_provider.isNull()) {
                // Interface was wrong
                qDebug() << "Fail!!" << pluginLoader->errorString();
                pluginLoader->deleteLater();
                continue;
            }
        } else {
            qWarning() << "Could not load plugin" << file.fileName() << pluginLoader->errorString();
            pluginLoader->deleteLater();
            continue;
        }

        pluginLoader->deleteLater();
        break;
    }
}

void FingerprintsService::createMissingPath()
{
    QDir dir;
    if (!dir.exists(QStringLiteral(FINGERPRINTS_PATH))){
        qDebug() << FINGERPRINTS_PATH " does not exist, we are going to create it.";
        if (!dir.mkpath(QStringLiteral(FINGERPRINTS_PATH))){
            qDebug() << "Error: we can't create the needed directory.";
            return;
        }
        if (!QFile::setPermissions(QStringLiteral(FINGERPRINTS_PATH), QFileDevice::ExeUser | QFileDevice::WriteUser | QFileDevice::ReadUser)){
            qDebug() << "We cannot set secure permissions, so we are going to destory the directory.";
            if (!dir.rmdir(QStringLiteral(FINGERPRINTS_PATH))){
                qFatal("Error: there are some problems with the fingerprints storage " FINGERPRINTS_PATH);
            }
        }
    }
}

QByteArray FingerprintsService::initStoredSerialNumber()
{
    QFile randomDevice(QStringLiteral("/dev/urandom"));
    randomDevice.open(QIODevice::ReadOnly);
    QByteArray randomData = randomDevice.read(8);
    //we need more data

    createMissingPath();
    QFile storedFile(QStringLiteral(STOREDFINGERPRINT_PATH ));
    storedFile.setPermissions(QFileDevice::WriteUser | QFileDevice::ReadUser);
    storedFile.open(QIODevice::WriteOnly);
    storedFile.write(randomData);
    storedFile.close();

    return randomData;
}

QByteArray FingerprintsService::calculateStoredFingerprint(const QByteArray &seed, const QByteArray &seed2)
{
    if (seed.isEmpty() || seed2.isEmpty()){
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("STORED SERIAL NUMBER");
    hash.addData(seed);
    hash.addData(seed2);

    QByteArray storedData;
    QFile stored(QStringLiteral(STOREDFINGERPRINT_PATH));
    if (!stored.open(QIODevice::ReadOnly)/* || stored.size() != 256*/) {
        qDebug() << "We need to create a new stored number";
        storedData = initStoredSerialNumber();
    } else {
        storedData = stored.read(256);
        stored.close();
    }
    hash.addData(storedData);

    QByteArray result = hash.result();

    //wipe software key from memory
    storedData.fill(0, storedData.count());

    return result.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray FingerprintsService::calculateHardwareFingerprint(const QByteArray &seed, const QByteArray &seed2)
{
    if (seed.isEmpty()){
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("HARDWARE SERIAL NUMBER");
    hash.addData(seed);
    hash.addData(seed2);

    QByteArray hardwareData;
    QFile hardware(QStringLiteral(HARDWAREFINGERPRINT_PATH));
    if (!hardware.open(QIODevice::ReadOnly) /*|| hardware.size() != 256*/){
        qDebug() << "We need to create a new hardware number";
        if (m_provider.isNull()) {
            // Try loading the plugin
            loadProviderPlugin();
        }

        if (m_provider.isNull()) {
            qDebug() << "No provider plugin available. Falling back to generic.";
            // We should not fail. Instead, let's just use the fallback.
            hardwareData = m_fallbackProvider->initHardwareSerialNumber();
        } else {
            hardwareData = m_provider->initHardwareSerialNumber();
        }

        if (hardwareData.isEmpty()) {
            qWarning() << "Provider failed in creating a hardware fingerprint! Falling back to generic.";
            // We should not fail. Instead, let's just use the fallback.
            hardwareData = m_fallbackProvider->initHardwareSerialNumber();
        }

        // Cache it.
        createMissingPath();
        QFile hardwareFile(QStringLiteral(HARDWAREFINGERPRINT_PATH ));
        hardwareFile.setPermissions(QFileDevice::WriteUser | QFileDevice::ReadUser);
        hardwareFile.open(QIODevice::WriteOnly);
        hardwareFile.write(hardwareData);
        hardwareFile.close();
    } else {
        hardwareData = hardware.readAll();
        hardware.close();
    }

    hash.addData(hardwareData);
    QByteArray result = hash.result();

    //wipe hardware key from memory
    hardwareData.fill(0, hardwareData.count());

    // Encode to base64 (URL Friendly) to save bytes and make it readable.
    return result.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString FingerprintsService::usernameFromUid(uid_t uid)
{
    struct passwd pwd;
    struct passwd *result;
    char *buf;
    size_t bufsize;
    int s;

    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize <= 0) {        /* Value was indeterminate */
        bufsize = 16384;        /* Should be more than enough */
    }

    buf = static_cast<char*>(malloc(bufsize));
    if (buf == NULL) {
        qWarning() << "Could not allocate memory!";
        return QString();
    }

    s = getpwuid_r(uid, &pwd, buf, bufsize, &result);
    if (result == NULL) {
        if (s == 0) {
            qWarning() << "Caller user not found! Refusing to create a fingerprint.";
        } else {
            qWarning() << "getpwuid_r failed," << s;
        }
        free(buf);
        return QString();
    }

    QString ret = QString::fromLatin1(pwd.pw_name);

    free(buf);

    return ret;
}

QByteArray FingerprintsService::StoredSerialNumber(const QByteArray &seed)
{
    if (!calledFromDBus()) {
        return QByteArray();
    }

    Q_EMIT activity();
    QString username = usernameFromUid(QDBusConnection::systemBus().interface()->serviceUid(message().service()));
    if (username.isEmpty()) {
        setDelayedReply(true);
        QDBusConnection::systemBus().send(message().createErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                                                     QStringLiteral("Could not retrieve username!")));
        return QByteArray();
    }

    QByteArray f = calculateStoredFingerprint(seed, username.toLatin1());

    sd_journal_send("MESSAGE=A stored serial number has been generated",
                    "MESSAGE_ID=a663a41892a34842a09fb81dbd99e594",
                    "PRIORITY=5",
                    "REQUEST_SERVICE=%s", message().service().toLatin1().constData(),
                    NULL);

    return f;
}

QByteArray FingerprintsService::HardwareSerialNumber(const QByteArray &seed)
{
    if (!calledFromDBus()) {
        return QByteArray();
    }

    QString username = usernameFromUid(QDBusConnection::systemBus().interface()->serviceUid(message().service()));
    if (username.isEmpty()) {
        setDelayedReply(true);
        QDBusConnection::systemBus().send(message().createErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                                                                     QStringLiteral("Could not retrieve username!")));
        return QByteArray();
    }

    QByteArray f = calculateHardwareFingerprint(seed, username.toLatin1());

    sd_journal_send("MESSAGE=A hardware serial number has been generated",
                    "MESSAGE_ID=58e62cc7dc29494386f767521eb4b82b",
                    "PRIORITY=5",
                    "REQUEST_SERVICE=%s", message().service().toLatin1().constData(),
                    NULL);

    return f;
}

QByteArray FingerprintsService::GlobalHardwareId()
{
    Q_EMIT activity();
    QByteArray f = calculateHardwareFingerprint(QByteArray("ThisHardwareIdWillBePublicAndEveryoneWillSeeIt!"), QByteArray("HEMERA1_HWID1"));

    return f;
}

QByteArray FingerprintsService::GlobalSystemId()
{
    Q_EMIT activity();
    QByteArray f = calculateStoredFingerprint(QByteArray("ThisSoftwareIdWillBePublicAndEveryoneWillSeeIt!!!"), QByteArray("HEMERA1_SYSID1"));

    return f;
}
