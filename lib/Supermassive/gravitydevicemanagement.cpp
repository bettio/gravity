#include "gravitydevicemanagement.h"

#include "gravityoperations.h"
#include "gravitysandbox.h"
#include "gravitysandboxmanager.h"
#include "gravitygalaxymanager.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Planet>

#include <QtDBus/QDBusConnection>

#include "devicemanagementadaptor.h"
#include "systemdmanagerinterface.h"
#include "localeinterface.h"
#include "timedateinterface.h"

#include <errno.h>
#include <time.h>

#define HANDLE_OPERATION_DBUS(op)\
QDBusMessage request;\
QDBusConnection requestConnection = QDBusConnection::systemBus();\
if (calledFromDBus()) {\
    request = message();\
    requestConnection = connection();\
    setDelayedReply(true);\
}\
connect(op, &Hemera::Operation::finished, this, [this, op, request, requestConnection] {\
    if (!op->isError()) {\
        requestConnection.send(request.createReply());\
    } else {\
        requestConnection.send(request.createErrorReply(op->errorName(), op->errorMessage()));\
    }\
});

#define GPT_RESET_TYPE "d1c18431-2ce9-4d8a-8a27-ab276095c9e1"
#define MBR_RESET_TYPE "64"
#define FACTORY_RESET_CFG_PATH "/usr/share/hemera/factoryreset.json"
#define SGDISK_PATH "/usr/sbin/sgdisk"
#define SFDISK_PATH "/sbin/sfdisk"

namespace Gravity
{

class DeviceManagement::Private
{
public:
    OrgFreedesktopSystemd1ManagerInterface *systemdManager;
    OrgFreedesktopLocale1Interface *localeInterface;
    OrgFreedesktopTimedate1Interface *timedateInterface;
};

DeviceManagement *DeviceManagement::s_anyInstance;

DeviceManagement::DeviceManagement(QObject* parent)
    : AsyncInitDBusObject(parent)
    , d(new Private)
{
}

DeviceManagement::~DeviceManagement()
{
    s_anyInstance = nullptr;
}

void DeviceManagement::initImpl()
{
    d->systemdManager = new org::freedesktop::systemd1::Manager(QStringLiteral("org.freedesktop.systemd1"),
                                                                QStringLiteral("/org/freedesktop/systemd1"),
                                                                QDBusConnection::systemBus(), this);

    if (!d->systemdManager->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("Systemd manager interface could not be found."));
        return;
    }

    d->localeInterface = new org::freedesktop::locale1(QStringLiteral("org.freedesktop.locale1"),
                                                       QStringLiteral("/org/freedesktop/locale1"),
                                                       QDBusConnection::systemBus(), this);

    d->timedateInterface = new org::freedesktop::timedate1(QStringLiteral("org.freedesktop.timedate1"),
                                                       QStringLiteral("/org/freedesktop/timedate1"),
                                                       QDBusConnection::systemBus(), this);

    setParts(3);

    connect(new Hemera::DBusVoidOperation(d->systemdManager->Subscribe(), this), &Hemera::Operation::finished,
            this, &DeviceManagement::setOnePartIsReady);

    QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::deviceManagementPath()), this);
    new DeviceManagementAdaptor(this);

    connect(new RestoreFactoryResetOperation(QStringLiteral(FACTORY_RESET_CFG_PATH), QStringLiteral(SGDISK_PATH), QStringLiteral(SFDISK_PATH), this),
            &Hemera::Operation::finished, this, &DeviceManagement::setOnePartIsReady);

    setOnePartIsReady();

    s_anyInstance = this;
}

Hemera::Operation *DeviceManagement::launchRebootOperation()
{
    //TODO: We need to implement some additional logic to check if reboot is allowed at this time.
    Hemera::Operation *op = new ControlUnitOperation(QStringLiteral("reboot.target"), QStringLiteral("replace"),
                                                     ControlUnitOperation::Mode::StartMode, d->systemdManager, this);

    return op;
}

void DeviceManagement::Reboot()
{
    Hemera::Operation *op = launchRebootOperation();

    HANDLE_OPERATION_DBUS(op);
}

Hemera::Operation *DeviceManagement::launchShutdownOperation()
{
    //TODO: We need to implement some additional logic to check if shutdown is allowed at this time.
    Hemera::Operation *op = new ControlUnitOperation(QStringLiteral("poweroff.target"), QStringLiteral("replace"),
                                                     ControlUnitOperation::Mode::StartMode, d->systemdManager, this);
    return op;
}

void DeviceManagement::Shutdown()
{
    Hemera::Operation *op = launchShutdownOperation();

    HANDLE_OPERATION_DBUS(op);
}

void DeviceManagement::FactoryReset()
{
    QList<Hemera::Operation *> operations;
    QDBusMessage request;
    QDBusConnection requestConnection = QDBusConnection::systemBus();
    if (calledFromDBus()) {
        request = message();
        requestConnection = connection();
        setDelayedReply(true);
    }
    QFile configFile(QStringLiteral(FACTORY_RESET_CFG_PATH));
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Factory reset config not found.";
        requestConnection.send(request.createErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()), QStringLiteral("Factory reset config " FACTORY_RESET_CFG_PATH " not found.")));
        return;
    }
    QByteArray configData = configFile.readAll();
    QJsonDocument configDoc(QJsonDocument::fromJson(configData));
    QJsonObject json = configDoc.object();
    for (const QJsonValue &disk : json.value(QStringLiteral("disks")).toArray()) {
        if (!disk.isObject()) {
            qWarning() << "Disk is not a valid JSON object. Check " FACTORY_RESET_CFG_PATH;
            continue;
        }
        QJsonObject diskObject = disk.toObject();
        QString command;
        QString name = diskObject.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            qWarning() << "Disk name is empty. Check " FACTORY_RESET_CFG_PATH;
            continue;
        }
        QString type = diskObject.value(QStringLiteral("type")).toString();

        QJsonArray partitions = diskObject.value(QStringLiteral("partition-numbers")).toArray();

        // If the name doesn't start with /dev, assume it's a label
        if (!name.startsWith(QStringLiteral("/dev"))) {
            QPair<QString, QString> nameAndPartition = RestoreFactoryResetOperation::diskNameAndPartitionFromLabel(name);
            name = nameAndPartition.first;
            partitions = QJsonArray() << nameAndPartition.second;
        }

        if (type.toLower() == QStringLiteral("gpt")) {
            command = QStringLiteral(SGDISK_PATH);
            for (const QJsonValue &partnum : partitions) {
                QStringList args{name, QStringLiteral("-t"), partnum.toString() + QStringLiteral(":" GPT_RESET_TYPE)};
                ToolOperation *sgdiskOp = new ToolOperation(command, args, this);
                operations.append(sgdiskOp);
            }

        } else if (type.toLower() == QStringLiteral("mbr")) {
            command = QStringLiteral(SFDISK_PATH);
            for (const QJsonValue &partnum: partitions) {
                QStringList args{name, partnum.toString(), QStringLiteral("-c"), QStringLiteral(MBR_RESET_TYPE)};
                ToolOperation *sfdiskOp = new ToolOperation(command, args, this);
                operations.append(sfdiskOp);
            }
        } else {
            qWarning() << "Disks must be of type GPT or MBR. Check " FACTORY_RESET_CFG_PATH;
        }
    }
    Hemera::SequentialOperation *resetSequence = new Hemera::SequentialOperation(operations, this);
    connect(resetSequence, &Hemera::Operation::finished, this, [this, request, requestConnection] (Hemera::Operation *op) {
        if (!op->isError()) {
            Hemera::Operation *rebootOperation = new ControlUnitOperation(QStringLiteral("reboot.target"), QStringLiteral("replace"),
                                                                          ControlUnitOperation::Mode::StartMode, d->systemdManager, this);

            connect(rebootOperation, &Hemera::Operation::finished, this, [this, rebootOperation, request, requestConnection] {
                if (!rebootOperation->isError()) {
                    requestConnection.send(request.createReply());
                } else {
                    requestConnection.send(request.createErrorReply(rebootOperation->errorName(), rebootOperation->errorMessage()));
                }
            });

        } else {
            requestConnection.send(request.createErrorReply(op->errorName(), op->errorMessage()));
        }
    });
}

void DeviceManagement::SetGlobalLocale(const QString &locale)
{
    Hemera::Operation *op = new Hemera::DBusVoidOperation(d->localeInterface->SetLocale(QStringList{QStringLiteral("LANG=%1").arg(locale)}, false));
    HANDLE_OPERATION_DBUS(op);
}

void DeviceManagement::SetGlobalTimeZone(const QString &timeZone)
{
    Hemera::Operation *op = new Hemera::DBusVoidOperation(d->timedateInterface->SetTimezone(timeZone, false));
    HANDLE_OPERATION_DBUS(op);
}

void DeviceManagement::SetSystemDateTime(qlonglong timeAsMsecs)
{
    Hemera::Operation *op = new Hemera::DBusVoidOperation(d->timedateInterface->SetTime(timeAsMsecs * 1000, false, false));
    HANDLE_OPERATION_DBUS(op);
}

// TODO: I HAVE NO IDEA WHAT I'M DOING HERE
DeviceManagement *DeviceManagement::instance()
{
    return s_anyInstance;
}

}
