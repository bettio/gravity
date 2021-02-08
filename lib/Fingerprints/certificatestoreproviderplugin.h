#ifndef CERTIFICATESTOREPROVIDERPLUGIN_H
#define CERTIFICATESTOREPROVIDERPLUGIN_H

#include <QtCore/QObject>

#include <GravitySupermassive/Global>

namespace Gravity {

class HEMERA_GRAVITY_EXPORT CertificateStoreProviderPlugin : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CertificateStoreProviderPlugin)

public:
    explicit CertificateStoreProviderPlugin(QObject* parent = 0);
    virtual ~CertificateStoreProviderPlugin();

    virtual QByteArray deviceKey() = 0;
    virtual QByteArray privateKey() = 0;
    virtual QByteArray certificate() = 0;

private:
    class Private;
    Private * const d;
};

}

#define CertificateStoreProviderInterface_iid "com.ispirata.Hemera.CertificateStore.Provider"

Q_DECLARE_INTERFACE(Gravity::CertificateStoreProviderPlugin, CertificateStoreProviderInterface_iid)

#endif // CERTIFICATESTOREPROVIDERPLUGIN_H
