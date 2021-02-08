#include "filesystemcertificatestoreprovider.h"

#include <QtCore/QFile>

#define CERTIFICATE_PATH QStringLiteral("/etc/hemera/appliance-crypto/certificate")
#define DEVICEKEY_PATH QStringLiteral("/etc/hemera/appliance-crypto/devicekey")
#define PRIVATEKEY_PATH QStringLiteral("/etc/hemera/appliance-crypto/privatekey")

FilesystemCertificateStoreProvider::FilesystemCertificateStoreProvider(QObject* parent)
    : CertificateStoreProviderPlugin(parent)
{
}

FilesystemCertificateStoreProvider::~FilesystemCertificateStoreProvider()
{
}

QByteArray FilesystemCertificateStoreProvider::loadFromFile(const QString &file) const
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QByteArray();
    }

    return f.readAll();
}


QByteArray FilesystemCertificateStoreProvider::certificate()
{
    return loadFromFile(CERTIFICATE_PATH);
}

QByteArray FilesystemCertificateStoreProvider::deviceKey()
{
    return loadFromFile(DEVICEKEY_PATH);
}

QByteArray FilesystemCertificateStoreProvider::privateKey()
{
    return loadFromFile(PRIVATEKEY_PATH);
}
