#ifndef GENERICFINGERPRINTPROVIDER_H
#define GENERICFINGERPRINTPROVIDER_H

#include <GravityFingerprints/FingerprintProviderPlugin>

class GenericFingerprintProvider : public Gravity::FingerprintProviderPlugin
{
    Q_OBJECT
    Q_INTERFACES(Gravity::FingerprintProviderPlugin)
    Q_DISABLE_COPY(GenericFingerprintProvider)

public:
    explicit GenericFingerprintProvider(QObject* parent = 0);
    virtual ~GenericFingerprintProvider();

    virtual QByteArray initHardwareSerialNumber() override final;

private:
    QStringList cpuValues() const;
};

#endif // GENERICFINGERPRINTPROVIDER_H
