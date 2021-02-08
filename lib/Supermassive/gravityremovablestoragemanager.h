#ifndef GRAVITY_REMOVABLESTORAGEMANAGER_H
#define GRAVITY_REMOVABLESTORAGEMANAGER_H

#include <HemeraCore/AsyncInitDBusObject>

#include <QtDBus/QDBusContext>

#include <GravitySupermassive/Global>

class Core;

namespace Hemera {
class StringOperation;
}

namespace Gravity {

class HEMERA_GRAVITY_EXPORT RemovableStorageManager : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(RemovableStorageManager)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.RemovableStorageManager")

public:
    virtual ~RemovableStorageManager();

    static RemovableStorageManager *instance();

    // DBus methods
    QByteArray ListDevices();
    QString Mount(const QString &deviceId, int options);
    void Unmount(const QString &deviceId);

    // Internal Gravity methods
    Hemera::StringOperation *mount(const QString &deviceId, int options);
    Hemera::Operation *unmount(const QString &deviceId);
    QList< QString > devices() const;
    QHash< QString, QString > mountedDevices() const;

Q_SIGNALS:
    void DevicesChanged(const QByteArray &devices);
    void DeviceAdded(const QByteArray &device);
    void DeviceRemoved(const QString &deviceName);
    void DeviceMounted(const QByteArray &device);
    void DeviceUnmounted(const QByteArray &device);
    void mountFinished(const QString &device);
    void unmountFinished(const QString &device);
    void errorOccurred(const QString &device, const QString &errorName, const QString &errorMessage);

protected:
    virtual void initImpl() override final;

private:
    explicit RemovableStorageManager(QObject* parent);

    class Private;
    Private * const d;
};
}

#endif // GRAVITY_REMOVABLESTORAGEMANAGER_H
