#include "gravityapplication.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>

#include "applicationinterface.h"
#include "dbusobjectinterface.h"

#include <gravityconfig.h>

using namespace Gravity;

class Application::Private
{
public:
    Private(const QDBusConnection &connection) : dbus(connection) {}

    com::ispirata::Hemera::Application *interface;
    com::ispirata::Hemera::DBusObject *dbusObject;

    QDBusConnection dbus;
    bool isSatellite;

    QString id;

    // Remote properties
    Hemera::Application::ApplicationStatus status;
};

Application::Application(const QString &name, bool isSatellite, const QDBusConnection &connection, QObject* parent)
    : AsyncInitObject(parent)
    , d(new Private(connection))
{
    d->id = name;
    d->isSatellite = isSatellite;
}

Application::~Application()
{
    delete d;
}

void Application::initImpl()
{
    setParts(3);

    d->interface = new com::ispirata::Hemera::Application(d->id,
                                                          Hemera::Literals::literal(Hemera::Literals::DBus::applicationPath()),
                                                          d->dbus, this);

    if (!d->interface->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote DBus application interface is not available."));
        return;
    }

    d->dbusObject = new com::ispirata::Hemera::DBusObject(d->id,
                                                          Hemera::Literals::literal(Hemera::Literals::DBus::applicationPath()),
                                                          d->dbus, this);

    if (!d->dbusObject->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::dbusObjectNotAvailable()),
                     QStringLiteral("The remote DBus object is not available. The daemon is probably not running."));
        return;
    }

    // Cache properties and connect to signals
    Hemera::DBusVariantMapOperation *operation = new Hemera::DBusVariantMapOperation(d->dbusObject->allProperties(), this);
    connect(operation, &Hemera::Operation::finished, [this, operation] {
        if (operation->isError()) {
            setInitError(operation->errorName());
            return;
        }

        // Readiness warning: if our status was already set by propertiesChanged, do not advertise readiness.
        Hemera::Application::ApplicationStatus currentStatus = d->status;

        // Set the various properties
        QVariantMap result = operation->result();
        d->status = static_cast<Hemera::Application::ApplicationStatus>(result.value(QStringLiteral("applicationStatus")).toUInt());

        // Verify if we are initialized already
        if (d->status != Hemera::Application::ApplicationStatus::NotInitialized && d->status != Hemera::Application::ApplicationStatus::Initializing &&
            d->status != currentStatus) {
            // Add another ready part. Otherwise, propertiesChanged will trigger it
            setOnePartIsReady();
        } else if (d->status == Hemera::Application::ApplicationStatus::Failed) {
            setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::applicationStartFailed()),
                                                   QStringLiteral("The application failed to initialize!"));
            return;
        }

        setOnePartIsReady();
    });

    connect(d->dbusObject, &com::ispirata::Hemera::DBusObject::propertiesChanged, [this] (const QVariantMap &changed) {
        if (changed.contains(QStringLiteral("applicationStatus"))) {
            d->status = static_cast<Hemera::Application::ApplicationStatus>(changed.value(QStringLiteral("applicationStatus")).toUInt());

            // Verify if we are initialized
            if (!isReady() && d->status == Hemera::Application::ApplicationStatus::Stopped) {
                setOnePartIsReady();
            } else if (!isReady() && d->status == Hemera::Application::ApplicationStatus::Failed) {
                setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::applicationStartFailed()),
                                                       QStringLiteral("The application failed to initialize!"));
                return;
            }

            Q_EMIT applicationStatusChanged(d->status);
        }
    });

    setOnePartIsReady();
}

QString Application::id() const
{
    return d->id;
}

Hemera::Application::ApplicationStatus Application::status() const
{
    return d->status;
}

bool Application::isSatellite() const
{
    return d->isSatellite;
}

Hemera::Operation *Application::start()
{
    return new Hemera::DBusVoidOperation(d->interface->start());
}

Hemera::Operation *Application::stop()
{
    return new Hemera::DBusVoidOperation(d->interface->stop());
}

Hemera::Operation *Application::quit()
{
    return new Hemera::DBusVoidOperation(d->interface->quit());
}
