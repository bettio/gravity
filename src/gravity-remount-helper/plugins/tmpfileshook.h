#ifndef TMPFILESHOOK_H
#define TMPFILESHOOK_H

#include <GravityRemountHelper/Plugin>

class TmpfilesHook : public Gravity::RemountHelperPlugin
{
    Q_OBJECT
    Q_DISABLE_COPY(TmpfilesHook)
    Q_PLUGIN_METADATA(IID "com.ispirata.Hemera.GravityRemountHelper.Plugin")
    Q_INTERFACES(Gravity::RemountHelperPlugin)

public:
    explicit TmpfilesHook(QObject *parent = nullptr);
    virtual ~TmpfilesHook();

public Q_SLOTS:
    virtual void preRemountReadWriteHook() override final;
    virtual void postRemountReadWriteHook() override final;
    virtual void preRemountReadOnlyHook() override final;
    virtual void postRemountReadOnlyHook() override final;

private:
};

#endif // HEMERA_TMPFILESHOOK_H
