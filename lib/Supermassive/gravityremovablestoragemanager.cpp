#include "gravityremovablestoragemanager.h"

#include "gravityoperations.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/RemovableStorage>

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QDebug>
#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonObject>
#include <QtCore/QSocketNotifier>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>

#include <QtDBus/QDBusConnection>

#include <libudev.h>

#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "removablestoragemanageradaptor.h"


namespace Gravity
{

class MountOperation : public Hemera::StringOperation
{
    Q_OBJECT
    Q_DISABLE_COPY(MountOperation)

public:
    explicit MountOperation(const QString &device, int options, RemovableStorageManager *parent)
        : StringOperation(parent), m_manager(parent), m_device(device), m_options(options) {}
    virtual ~MountOperation() {}

public Q_SLOTS:
    virtual void startImpl() override final;

    inline virtual QString result() const override final { return m_path; }

private:
    RemovableStorageManager *m_manager;
    QString m_device;
    int m_options;

    QString m_path;
};

void MountOperation::startImpl()
{
    connect(m_manager, &RemovableStorageManager::mountFinished, this, [this] (const QString &device) {
        if (m_device == device) {
            setFinished();
        }
    });
    connect(m_manager, &RemovableStorageManager::errorOccurred, this,
            [this] (const QString &device, const QString &errorName, const QString &errorMessage) {
        if (m_device == device) {
            setFinishedWithError(errorName, errorMessage);
        }
    });

    m_path = m_manager->Mount(m_device, m_options);
}


class UnmountOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(UnmountOperation)

public:
    explicit UnmountOperation(const QString &device, RemovableStorageManager *parent)
        : Operation(parent), m_manager(parent), m_device(device) {}
    virtual ~UnmountOperation() {}

public Q_SLOTS:
    virtual void startImpl() override final;

private:
    RemovableStorageManager *m_manager;
    QString m_device;

    QString m_path;
};

void UnmountOperation::startImpl()
{
    connect(m_manager, &RemovableStorageManager::unmountFinished, this, [this] (const QString &device) {
        if (m_device == device) {
            setFinished();
        }
    });
    connect(m_manager, &RemovableStorageManager::errorOccurred, this,
            [this] (const QString &device, const QString &errorName, const QString &errorMessage) {
        if (m_device == device) {
            setFinishedWithError(errorName, errorMessage);
        }
    });

    m_manager->Unmount(m_device);
}

class RemovableStorageManager::Private
{
public:
    Private(RemovableStorageManager *parent) : q(parent), udev(nullptr), mon(nullptr) {}

    RemovableStorageManager *q;

    struct udev *udev;
    struct udev_monitor *mon;

    QHash< QString, QJsonObject > devices;
    QHash< QString, QString > mountedDevices;
    QHash< QString, QTemporaryDir* > mountPoints;

    QDBusServiceWatcher *watcher;

    void onDeviceAdded(struct udev_device *device);
    void onDeviceRemoved(struct udev_device *device);
    void removeDeviceFromStorage(const QString &deviceId, const QString &dbusService);
    QJsonDocument devicesToJson();
};

void RemovableStorageManager::Private::onDeviceAdded(struct udev_device* device)
{
    QJsonObject deviceData;
    QString devName = QLatin1String(udev_device_get_property_value(device, "DEVNAME"));
    Hemera::RemovableStorage::Device::Type type = qstrcmp(udev_device_get_property_value(device, "ID_BUS"), "usb") == 0 ?
                                    Hemera::RemovableStorage::Device::Type::USB :
                                    Hemera::RemovableStorage::Device::Type::SDCard;

    const char* sizeInSectors = udev_device_get_property_value(device, "ID_PART_ENTRY_SIZE");
    std::stringstream strValue;
    strValue << sizeInSectors;

    qint64 intValue;
    strValue >> intValue;

    deviceData.insert(QStringLiteral("path"), devName);
    deviceData.insert(QStringLiteral("mounted"), false);
    deviceData.insert(QStringLiteral("filesystem"), QLatin1String(udev_device_get_property_value(device, "ID_FS_TYPE")));
    deviceData.insert(QStringLiteral("type"), static_cast<int>(type));
    deviceData.insert(QStringLiteral("size"), intValue*512);

    devices.insert(devName, deviceData);

    Q_EMIT q->DevicesChanged(devicesToJson().toJson(QJsonDocument::Compact));
    Q_EMIT q->DeviceAdded(QJsonDocument(deviceData).toJson(QJsonDocument::Compact));
}

void RemovableStorageManager::Private::onDeviceRemoved(struct udev_device* device)
{
    QString devName = QLatin1String(udev_device_get_property_value(device, "DEVNAME"));

    if (mountedDevices.contains(devName)) {
        // We need to unmount it
        Hemera::Operation *op = new UnmountOperation(devName, q);
        connect(op, &Hemera::Operation::finished, q, [this, op, devName] {
            if (op->isError()) {
                qWarning() << "Error unmounting removed device" << devName << ":" << op->errorMessage();
                // Unmount failed, so we have to manually clean up
                QString service = mountedDevices.take(devName);
                if (!service.isEmpty()) {
                    watcher->removeWatchedService(service);
                }
                delete mountPoints.take(devName);
            }

            // Otherwise, removeDeviceFromStorage has already cleaned up for us
            devices.remove(devName);

            Q_EMIT q->DevicesChanged(devicesToJson().toJson(QJsonDocument::Compact));
            Q_EMIT q->DeviceRemoved(devName);
       });

    } else {
        // Not mounted, we just erase it
        devices.remove(devName);

        Q_EMIT q->DevicesChanged(devicesToJson().toJson(QJsonDocument::Compact));
        Q_EMIT q->DeviceRemoved(devName);
    }
}

void RemovableStorageManager::Private::removeDeviceFromStorage(const QString &deviceId, const QString &dbusService)
{
    delete mountPoints.take(deviceId);
    mountedDevices.remove(deviceId);
    if (!dbusService.isEmpty()) {
        watcher->removeWatchedService(dbusService);
    }

    // Update device status
    QJsonObject deviceData = devices.value(deviceId);
    deviceData.insert(QStringLiteral("mounted"), false);
    deviceData.remove(QStringLiteral("mountPoint"));
    devices.insert(deviceId, deviceData);
}

QJsonDocument RemovableStorageManager::Private::devicesToJson()
{
    QJsonArray list;
    for (QHash< QString, QJsonObject >::const_iterator i = devices.constBegin(); i != devices.constEnd(); ++i) {
        list.append(i.value());
    }
    return QJsonDocument(list);
}


static RemovableStorageManager *s_instance = nullptr;

RemovableStorageManager::RemovableStorageManager(QObject* parent)
    : AsyncInitDBusObject(parent)
    , d(new Private(this))
{
    if (s_instance) {
        Q_ASSERT("Trying to create an additional instance! Only one RemovableStorageManager per process can exist.");
    }

    s_instance = this;
}

RemovableStorageManager::~RemovableStorageManager()
{
    if (d->udev) {
        udev_unref(d->udev);
    }

    if (d->mon) {
        udev_monitor_unref(d->mon);
    }

    delete d;
}

RemovableStorageManager *RemovableStorageManager::instance()
{
    if (!s_instance) {
        new RemovableStorageManager(nullptr);
    }

    return s_instance;
}

void RemovableStorageManager::initImpl()
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    int fd;

    /* Create the udev object */
    d->udev = udev_new();
    if (!d->udev) {
        qWarning() << "Can't create udev manager, this is really weird.";
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), QStringLiteral("Could not create udev manager"));
        return;
    }

    /* This section sets up a monitor which will report events when
       devices attached to the system change.  Events include "add",
       "remove", "change", "online", and "offline".
    */

    /* Set up a monitor to monitor block devices */
    d->mon = udev_monitor_new_from_netlink(d->udev, "udev");

    if (!d->mon) {
        qWarning() << "Could not create netlink udev monitor!";
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()), QStringLiteral("Could not create netlink udev monitor"));
        return;
    }

    /* Filtering for added devices without additional checks is perfectly fine. If a block partition
     * gets added at runtime it is obviously removable storage, unless something really creepy is
     * happening.
     */
    udev_monitor_filter_add_match_subsystem_devtype(d->mon, "block", "partition");
    udev_monitor_enable_receiving(d->mon);
    /* Get the file descriptor (fd) for the monitor. This fd will get passed to select() */
    fd = udev_monitor_get_fd(d->mon);


    /* Create a list of the devices in the 'block' subsystem. */
    enumerate = udev_enumerate_new(d->udev);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "ID_FS_USAGE", "filesystem");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    /* For each item enumerated, print out its information.
       udev_list_entry_foreach is a macro which expands to
       a loop. The loop will be executed for each member in
       devices, setting dev_list_entry to a list entry
       which contains the device's path in /sys. */
    udev_list_entry_foreach (dev_list_entry, devices) {
        const char *path;

        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it */
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(d->udev, path);

        // TODO: Maybe we should also support SD here? We need a more reliable filter.
        if (qstrcmp(udev_device_get_property_value(dev, "ID_BUS"), "usb") == 0) {
            qWarning() << "Enumerating!" << path;

            d->onDeviceAdded(dev);
        }

        udev_device_unref(dev);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    /* Begin polling for udev events. Events occur when devices
       attached to the system are added, removed, or change state.
       udev_monitor_receive_device() will return a device
       object representing the device which changed and what type of
       change occured.

       The monitor was set up earler in this file, and monitoring is
       already underway.
    */

    QSocketNotifier *udevMonitor = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(udevMonitor, &QSocketNotifier::activated, this, [this] {
        qWarning() << "UDEV Monitor activated!!";
        /* Make the call to receive the device. QSocketNotifier ensures that this will not block. */
        struct udev_device *dev;
        dev = udev_monitor_receive_device(d->mon);
        if (dev) {
            qWarning() << "Got device!!";
            const char* szAction = udev_device_get_action(dev);
            if (qstrcmp(szAction, "add") == 0) {
                d->onDeviceAdded(dev);
            } else {
                d->onDeviceRemoved(dev);
            }
            udev_device_unref(dev);
        } else {
            qWarning() << "No Device from receive_device(). An error occured.";
        }
    });

    QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::removableStorageManagerPath()), this);
    new RemovableStorageManagerAdaptor(this);

    // Add our QDBusServiceWatcher to monitor applications dying without releasing mount lock
    d->watcher = new QDBusServiceWatcher(this);
    d->watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    d->watcher->setConnection(QDBusConnection::systemBus());

    connect(d->watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] (const QString &service) {
        for (QHash< QString, QString >::const_iterator i = d->mountedDevices.constBegin(); i != d->mountedDevices.constEnd(); ++i) {
            if (i.value() == service) {
                qDebug() << "Service" << service << "died without unmounting. Forcing unmount.";
                Unmount(i.key());
                break;
            }
        }

        d->watcher->removeWatchedService(service);
    });


    setReady();
}

QByteArray RemovableStorageManager::ListDevices()
{
    return d->devicesToJson().toJson(QJsonDocument::Compact);
}

QString RemovableStorageManager::Mount(const QString &deviceId, int options)
{
    QDBusMessage dbusMessage;
    uid_t ownerUid = 0; // root

    if (!calledFromDBus()) {
        qDebug() << "Mount called straight from Gravity!";
    } else {
        dbusMessage = message();
        ownerUid = QDBusConnection::systemBus().interface()->serviceUid(message().service());
        qDebug() << "Will mount for uid" << ownerUid;
        setDelayedReply(true);
    }

    // Can we do this?
    if (!d->devices.contains(deviceId)) {
        qWarning() << deviceId << "does not exist!";
        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::noSuchDevice(),
                                                                           QStringLiteral("Device %1 does not exist!").arg(deviceId)));
        }
        Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::noSuchDevice(),
                             QStringLiteral("Device %1 does not exist!").arg(deviceId));
        return QString();
    }
    if (d->mountPoints.contains(deviceId)) {
        qWarning() << deviceId << "is already mounted!";
        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::alreadyMounted(),
                                                                           QStringLiteral("Device %1 is already mounted!").arg(deviceId)));
        }
        Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::alreadyMounted(),
                             QStringLiteral("Device %1 is already mounted!").arg(deviceId));
        return QString();
    }

    QTemporaryDir *mountPoint = new QTemporaryDir(QStringLiteral("%1%2hemera_removable_storage-XXXXXX").arg(QDir::tempPath(), QDir::separator()));

    QProcess *mountProcess = new QProcess(this);
    mountProcess->setProgram(QStringLiteral("mount"));

    QString mountOptions = QStringLiteral("uid=%1").arg(ownerUid);
    Hemera::RemovableStorage::MountOptions requestedMountOptions = static_cast<Hemera::RemovableStorage::MountOptions>(options);
    if (requestedMountOptions & Hemera::RemovableStorage::ReadOnly) {
        mountOptions.append(QStringLiteral(",ro"));
    }
    mountProcess->setArguments(QStringList{ QStringLiteral("-o"), mountOptions, deviceId, mountPoint->path() });
    Hemera::ProcessOperation *op = new Hemera::ProcessOperation(mountProcess, this);

    connect(op, &Hemera::Operation::finished, this, [this, dbusMessage, ownerUid, mountPoint, deviceId, mountProcess, requestedMountOptions] {
        if (mountProcess->exitStatus() == QProcess::CrashExit) {
            qWarning() << "Mount crashed!";
            if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                QDBusConnection::systemBus().send(dbusMessage.createErrorReply(QLatin1String(Hemera::Literals::Errors::unhandledRequest()),
                                                                               QStringLiteral("Process crashed")));
            }
            Q_EMIT errorOccurred(deviceId, QLatin1String(Hemera::Literals::Errors::unhandledRequest()), QStringLiteral("Process crashed"));
            delete mountPoint;
            mountProcess->deleteLater();
            return;
        }

        if (mountProcess->exitCode() != 0) {
            qWarning() << "Mount failed!";
            QString mountOutput = QLatin1String(mountProcess->readAll());
            qWarning() << mountOutput;

            if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                if (mountOutput.contains(QStringLiteral("fs type"))) {
                    QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::unsupportedFilesystem(),
                                                                                   QStringLiteral("Unsupported filesystem")));
                } else {
                    QDBusConnection::systemBus().send(dbusMessage.createErrorReply(QLatin1String(Hemera::Literals::Errors::failedRequest()),
                                                                                   QStringLiteral("Process exited with %1").arg(mountProcess->exitCode())));
                }
            }
            if (mountOutput.contains(QStringLiteral("fs type"))) {
                Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::unsupportedFilesystem(), QStringLiteral("Unsupported filesystem"));
            } else {
                Q_EMIT errorOccurred(deviceId, QLatin1String(Hemera::Literals::Errors::failedRequest()),
                                     QStringLiteral("Process exited with %1").arg(mountProcess->exitCode()));
            }
            delete mountPoint;
            mountProcess->deleteLater();
            return;
        }

        // Mount successful! Let's register the change.
        d->mountedDevices.insert(deviceId, dbusMessage.service());
        d->mountPoints.insert(deviceId, mountPoint);
        if (!dbusMessage.service().isEmpty()) {
            d->watcher->addWatchedService(dbusMessage.service());
        }

        // Update device status
        QJsonObject deviceData = d->devices.value(deviceId);
        deviceData.insert(QStringLiteral("mounted"), true);
        deviceData.insert(QStringLiteral("mountPoint"), mountPoint->path());
        d->devices.insert(deviceId, deviceData);

        if (!(requestedMountOptions & Hemera::RemovableStorage::ReadOnly) && access(mountPoint->path().toLatin1().constData(), R_OK | W_OK) < 0) {
            if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::notWriteable(),
                                                                               QStringLiteral("Could not mount filesystem for writing.")));
            }
            Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::notWriteable(), QStringLiteral("Could not mount filesystem for writing."));
            // Unmount
            Unmount(deviceId);
        }

        Q_EMIT mountFinished(deviceId);
        Q_EMIT DevicesChanged(d->devicesToJson().toJson(QJsonDocument::Compact));
        Q_EMIT DeviceMounted(QJsonDocument(d->devices.value(deviceId)).toJson(QJsonDocument::Compact));

        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createReply(QVariantList{ mountPoint->path() }));
        }

        mountProcess->deleteLater();
    });

    return mountPoint->path();
}

void RemovableStorageManager::Unmount(const QString &deviceId)
{
    QDBusMessage dbusMessage;

    if (!calledFromDBus()) {
        qDebug() << "Unmount called straight from Gravity! Considering this as a force unmount, skipping sanity checks.";
    } else {
        dbusMessage = message();
        setDelayedReply(true);
    }

    // Can we do this?
    if (!d->devices.contains(deviceId)) {
        qWarning() << deviceId << "does not exist!";
        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::noSuchDevice(),
                                                                           QStringLiteral("Device %1 does not exist!").arg(deviceId)));
        }
        Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::noSuchDevice(),
                             QStringLiteral("Device %1 does not exist!").arg(deviceId));
        return;
    }
    if (!d->mountPoints.contains(deviceId)) {
        qWarning() << deviceId << "is not mounted!";
        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::notMounted(),
                                                                           QStringLiteral("Device %1 is not mounted!").arg(deviceId)));
        }
        Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::notMounted(),
                             QStringLiteral("Device %1 is not mounted!").arg(deviceId));
        return;
    }
    // Check if the service caller matches
    if (dbusMessage.service() != d->mountedDevices.value(deviceId) && !dbusMessage.service().isEmpty()) {
        qWarning() << deviceId << "was not mounted by service" << dbusMessage.service();
        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createErrorReply(Hemera::RemovableStorage::Errors::notOwnedByApplication(),
                                                                           QStringLiteral("Device %1 was not mounted by this service!").arg(deviceId)));
        }
        Q_EMIT errorOccurred(deviceId, Hemera::RemovableStorage::Errors::notOwnedByApplication(),
                             QStringLiteral("Device %1 was not mounted by this service!").arg(deviceId));
        return;
    }

    // Ok, let's unmount.
    QProcess *unmountProcess = new QProcess(this);
    unmountProcess->setProgram(QStringLiteral("umount"));
    unmountProcess->setArguments(QStringList{ deviceId });
    Hemera::ProcessOperation *op = new Hemera::ProcessOperation(unmountProcess, this);

    connect(op, &Hemera::Operation::finished, this, [this, dbusMessage, deviceId, unmountProcess] {
        if (unmountProcess->exitStatus() == QProcess::CrashExit) {
            unmountProcess->deleteLater();
            qWarning() << "Umount crashed!";
            if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                QDBusConnection::systemBus().send(dbusMessage.createErrorReply(QLatin1String(Hemera::Literals::Errors::unhandledRequest()),
                                                                               QStringLiteral("Process crashed")));
            }
            Q_EMIT errorOccurred(deviceId, QLatin1String(Hemera::Literals::Errors::unhandledRequest()), QStringLiteral("Process crashed"));
            return;
        }

        if (unmountProcess->exitCode() != 0) {
            qWarning() << "Umount failed! Trying a force umount";
            qWarning() << unmountProcess->readAll();

            // Don't block gravity on sync()!
            QFutureWatcher< int > *forceUmountWatcher = new QFutureWatcher< int >(this);
            connect(forceUmountWatcher, &QFutureWatcher< int >::finished, this, [this, deviceId, forceUmountWatcher, dbusMessage] {
                if (forceUmountWatcher->result() != 0) {
                    qWarning() << "Force umount failed! Giving up.";
                    if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                        QDBusConnection::systemBus().send(dbusMessage.createErrorReply(QLatin1String(Hemera::Literals::Errors::failedRequest()),
                                                                                       QStringLiteral("Process exited with %1").arg(forceUmountWatcher->result())));
                    }

                    Q_EMIT errorOccurred(deviceId, QLatin1String(Hemera::Literals::Errors::failedRequest()),
                                         QStringLiteral("Process exited with %1").arg(forceUmountWatcher->result()));
                } else {
                    qDebug() << "Force umount was successful!";
                    // Umount successful! Let's register the change.
                    d->removeDeviceFromStorage(deviceId, dbusMessage.service());
                    Q_EMIT unmountFinished(deviceId);
                    Q_EMIT DevicesChanged(d->devicesToJson().toJson(QJsonDocument::Compact));
                    Q_EMIT DeviceUnmounted(QJsonDocument(d->devices.value(deviceId)).toJson(QJsonDocument::Compact));

                    if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
                        QDBusConnection::systemBus().send(dbusMessage.createReply());
                    }
                }

                forceUmountWatcher->deleteLater();
            });

            QFuture< int > forceUmountFuture = QtConcurrent::run([this, deviceId] () -> int {
                ::sync();
                QProcess forceUmountProcess;
                forceUmountProcess.setProgram(QStringLiteral("umount"));
                forceUmountProcess.setArguments(QStringList{ QStringLiteral("-l"), QStringLiteral("-f"), deviceId });
                forceUmountProcess.start();
                if (!forceUmountProcess.waitForFinished()) {
                    return -1;
                }
                if (forceUmountProcess.exitCode() != 0) {
                    qDebug() << "umount failed!" << forceUmountProcess.readAll();
                }

                return forceUmountProcess.exitCode();
            });
            forceUmountWatcher->setFuture(forceUmountFuture);
            unmountProcess->deleteLater();
            return;
        }

        // Umount successful! Let's register the change.
        d->removeDeviceFromStorage(deviceId, dbusMessage.service());
        Q_EMIT unmountFinished(deviceId);
        Q_EMIT DevicesChanged(d->devicesToJson().toJson(QJsonDocument::Compact));
        Q_EMIT DeviceUnmounted(QJsonDocument(d->devices.value(deviceId)).toJson(QJsonDocument::Compact));

        if (dbusMessage.type() != QDBusMessage::InvalidMessage) {
            QDBusConnection::systemBus().send(dbusMessage.createReply());
        }

        unmountProcess->deleteLater();
    });
}

QList< QString > RemovableStorageManager::devices() const
{
    return d->devices.keys();
}

QHash< QString, QString > RemovableStorageManager::mountedDevices() const
{
    return d->mountedDevices;
}

Hemera::StringOperation *RemovableStorageManager::mount(const QString &deviceId, int options)
{
    return new MountOperation(deviceId, options, this);
}

Hemera::Operation *RemovableStorageManager::unmount(const QString &deviceId)
{
    return new UnmountOperation(deviceId, this);
}

}

#include "gravityremovablestoragemanager.moc"
