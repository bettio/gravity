#ifndef GRAVITY_DEVICEMANAGEMENT_H
#define GRAVITY_DEVICEMANAGEMENT_H

#include <HemeraCore/AsyncInitDBusObject>

#include <QtDBus/QDBusContext>

#include <GravitySupermassive/Global>

namespace Hemera {
    class Operation;
}

namespace Gravity {

class HEMERA_GRAVITY_EXPORT DeviceManagement : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DeviceManagement)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.DeviceManagement")

public:
    explicit DeviceManagement(QObject* parent);
    virtual ~DeviceManagement();

    Hemera::Operation *launchRebootOperation();
    Hemera::Operation *launchShutdownOperation();

    void Reboot();
    void Shutdown();
    void FactoryReset();

    void SetGlobalLocale(const QString &locale);
    void SetGlobalTimeZone(const QString &timeZone);

    void SetSystemDateTime(qlonglong timeAsTime_t);

    static DeviceManagement *instance();

protected:
    virtual void initImpl() override final;

private:
    class Private;
    Private * const d;

    static DeviceManagement *s_anyInstance;
};
}

#endif // GRAVITY_DEVICEMANAGEMENT_H
