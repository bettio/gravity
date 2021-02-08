#ifndef FILESYSTEMCERTIFICATESTOREPROVIDER_H
#define FILESYSTEMCERTIFICATESTOREPROVIDER_H

#include <GravityFingerprints/CertificateStoreProviderPlugin>

class FilesystemCertificateStoreProvider : public Gravity::CertificateStoreProviderPlugin
{
    Q_OBJECT
    Q_INTERFACES(Gravity::CertificateStoreProviderPlugin)
    Q_DISABLE_COPY(FilesystemCertificateStoreProvider)

public:
    explicit FilesystemCertificateStoreProvider(QObject* parent = 0);
    virtual ~FilesystemCertificateStoreProvider();

    virtual QByteArray deviceKey() override final;
    virtual QByteArray privateKey() override final;
    virtual QByteArray certificate() override final;

private:
    QByteArray loadFromFile(const QString &file) const;
};

#endif // FILESYSTEMCERTIFICATESTOREPROVIDER_H
