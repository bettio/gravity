#ifndef GRAVITY_GRAVITYREMOUNTHELPERPLUGIN_H
#define GRAVITY_GRAVITYREMOUNTHELPERPLUGIN_H

#include <QtCore/QObject>

#include <GravitySupermassive/Global>

namespace Gravity {

class HEMERA_GRAVITY_EXPORT RemountHelperPlugin : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(RemountHelperPlugin)

public:
    explicit RemountHelperPlugin(QObject *parent = 0);
    virtual ~RemountHelperPlugin();

public Q_SLOTS:
    virtual void preRemountReadWriteHook() = 0;
    virtual void postRemountReadWriteHook() = 0;
    virtual void preRemountReadOnlyHook() = 0;
    virtual void postRemountReadOnlyHook() = 0;


private:
    class Private;
    Private * const d;

    friend class GravityRemountHelperCore;
};

}

Q_DECLARE_INTERFACE(Gravity::RemountHelperPlugin, "com.ispirata.Hemera.Gravity.RemountHelper.Plugin")

#endif // HEMERA_GRAVITY_GRAVITYREMOUNTHELPERPLUGIN_H
