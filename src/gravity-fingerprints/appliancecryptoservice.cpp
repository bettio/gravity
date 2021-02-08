#include "appliancecryptoservice.h"

#include "filesystemcertificatestoreprovider.h"

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

#include <gravityconfig.h>

#include "appliancecryptoadaptor.h"

ApplianceCryptoService::ApplianceCryptoService()
    : Hemera::AsyncInitObject(Q_NULLPTR)
{
}

ApplianceCryptoService::~ApplianceCryptoService()
{
}

void ApplianceCryptoService::initImpl()
{
    if (!QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::applianceCryptoPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("Could not register the object on DBus. This means either a wrong installation or a corrupted instance."));
        return;
    }
    new ApplianceCryptoAdaptor(this);

    m_fallbackProvider = new FilesystemCertificateStoreProvider(this);

    setReady();
}

void ApplianceCryptoService::loadProviderPlugin()
{
    // Init provider
    m_provider.clear();

    QDir pluginsDir(QLatin1String(Gravity::StaticConfig::hemeraApplianceCryptoPluginsPath()));

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
            m_provider = qobject_cast<Gravity::CertificateStoreProviderPlugin*>(plugin);
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

QByteArray ApplianceCryptoService::DeviceKey()
{
    Q_EMIT activity();
    if (m_provider) {
        if (!m_provider->deviceKey().isEmpty()) {
            return m_provider->deviceKey();
        }
    }

    return m_fallbackProvider->deviceKey();
}

QByteArray ApplianceCryptoService::ClientSSLCertificate(const QByteArray &/*ohQDBusSeriously?*/)
{
    // Cope with QDBus.
    setDelayedReply(true);

    QDBusMessage request = message();
    QDBusConnection requestConnection = connection();

    QByteArray privateKey;
    QByteArray certificate;

    Q_EMIT activity();
    if (m_provider) {
        if (!m_provider->privateKey().isEmpty() && !m_provider->certificate().isEmpty()) {
            certificate = m_provider->certificate();
            privateKey = m_provider->privateKey();
        }
    }

    if (certificate.isEmpty()) {
        certificate = m_fallbackProvider->certificate();
        privateKey = m_fallbackProvider->privateKey();
    }

    requestConnection.send(request.createReply(QList<QVariant>() << privateKey << certificate));

    // Blah.
    return QByteArray();
}

QByteArray ApplianceCryptoService::LocalCA()
{
    // TODO: Implement me! :(
    return QByteArray();
}

QByteArray ApplianceCryptoService::SignSSLCertificate(const QByteArray &certificateSigningRequest)
{
    // TODO: Implement me! :(
    return QByteArray();
}
