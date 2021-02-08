#ifndef APPLIANCECRYPTOSERVICE_H
#define APPLIANCECRYPTOSERVICE_H

#include <HemeraCore/AsyncInitObject>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>
#include <QtCore/QPointer>

#include <QtDBus/QDBusContext>

namespace Gravity {
class CertificateStoreProviderPlugin;
}

class ApplianceCryptoService : public Hemera::AsyncInitObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.ApplianceCrypto")

public:
    ApplianceCryptoService();
    virtual ~ApplianceCryptoService();

    QByteArray DeviceKey();
    QByteArray ClientSSLCertificate(const QByteArray &/*ohQDBusSeriously?*/);

    QByteArray LocalCA();
    QByteArray SignSSLCertificate(const QByteArray &certificateSigningRequest);

private Q_SLOTS:
    void initImpl() override final;

Q_SIGNALS:
    void activity();

private:
    void loadProviderPlugin();

    QPointer< Gravity::CertificateStoreProviderPlugin > m_provider;
    Gravity::CertificateStoreProviderPlugin *m_fallbackProvider;
};

#endif // APPLIANCECRYPTOSERVICE_H
