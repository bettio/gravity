#ifndef FINGERPRINTPROVIDERPLUGIN_H
#define FINGERPRINTPROVIDERPLUGIN_H

#include <QtCore/QObject>

#include <GravitySupermassive/Global>

namespace Gravity {

class HEMERA_GRAVITY_EXPORT FingerprintProviderPlugin : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(FingerprintProviderPlugin)

public:
    explicit FingerprintProviderPlugin(QObject* parent = 0);
    virtual ~FingerprintProviderPlugin();

    virtual QByteArray initHardwareSerialNumber() = 0;

private:
    class Private;
    Private * const d;
};

}

#define FingerprintProviderInterface_iid "com.ispirata.Hemera.Fingerprints.Provider"

Q_DECLARE_INTERFACE(Gravity::FingerprintProviderPlugin, FingerprintProviderInterface_iid)

#endif // FINGERPRINTPROVIDERPLUGIN_H
