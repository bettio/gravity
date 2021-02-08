#ifndef FINGERPRINTSSERVICE_H
#define FINGERPRINTSSERVICE_H

#include <HemeraCore/AsyncInitObject>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>
#include <QtCore/QPointer>

#include <QtDBus/QDBusContext>

namespace Gravity {
class FingerprintProviderPlugin;
}

class FingerprintsService : public Hemera::AsyncInitObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.Fingerprints")

public:
    FingerprintsService();
    virtual ~FingerprintsService();

    QByteArray HardwareSerialNumber(const QByteArray &seed);
    QByteArray StoredSerialNumber(const QByteArray &seed);

    QByteArray GlobalHardwareId();
    QByteArray GlobalSystemId();

    QByteArray calculateHardwareFingerprint(const QByteArray &seed, const QByteArray &seed2);
    QByteArray calculateStoredFingerprint(const QByteArray &seed, const QByteArray &seed2);

private Q_SLOTS:
    void initImpl() override final;

Q_SIGNALS:
    void activity();

private:
    QString usernameFromUid(uid_t uid);
    void loadProviderPlugin();

    static void createMissingPath();

    QByteArray initStoredSerialNumber();

    QPointer< Gravity::FingerprintProviderPlugin > m_provider;
    Gravity::FingerprintProviderPlugin *m_fallbackProvider;
};

#endif // FINGERPRINTSSERVICE_H
