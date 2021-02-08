/*
 *
 */

#include "gravityapplicationhandler.h"

#include "gravityapplication.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Operation>
#include <HemeraCore/Planet>
#include <HemeraCore/ServiceManager>

#include <QtDBus/QDBusServiceWatcher>

#include "fdodbusinterface.h"
#include "applicationhandleradaptor.h"
#include "satellitemanagerinterface.h"
#include "dbusobjectinterface.h"

namespace Gravity
{

class ApplicationHandler::Private
{
public:
    Private(const QDBusConnection &dbus) : dbus(dbus) {}

    QString star;

    QDBusConnection dbus;
    QHash< QString, Application* > livingApplications;

    Hemera::ServiceManager *serviceManager;

    QStringList applicationsInSatellites;
    QStringList satellites;
    QStringList activeSatellites;
    Hemera::Planet::ActivationPolicies activationPolicies;

    org::freedesktop::DBus *fdoDBus;

    com::ispirata::Hemera::Gravity::SatelliteManager *satelliteManagerInterface;
    com::ispirata::Hemera::DBusObject *satelliteManagerObjectInterface;
};

ApplicationHandler::ApplicationHandler(const QString &starName, const QDBusConnection& connection, QObject* parent)
    : AsyncInitDBusObject(parent)
    , d(new Private(connection))
{
    d->star = starName;
}

ApplicationHandler::~ApplicationHandler()
{
    delete d;
}

void ApplicationHandler::initImpl()
{
    setParts(3);

    d->fdoDBus = new org::freedesktop::DBus(QStringLiteral("org.freedesktop.DBus"), QString(),
                                            d->dbus, this);

    if (!d->fdoDBus->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote DBus interface is not available."));
        return;
    }

    // Register our object
    if (!d->dbus.registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::applicationHandlerPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerObjectFailed()),
                    QStringLiteral("Failed to register the object on the bus"));
        return;
    }

    // Create connection to the Satellite Manager
    QString satelliteManagerPath = QString::fromLatin1(Hemera::Literals::DBus::satelliteManagerPath()).arg(d->star);
    d->satelliteManagerInterface = new com::ispirata::Hemera::Gravity::SatelliteManager(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()),
                                                                                        satelliteManagerPath, QDBusConnection::systemBus(), this);
    if (!d->satelliteManagerInterface->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote Satellite Manager is not available. The daemon is probably not running."));
        return;
    }
    d->satelliteManagerObjectInterface = new com::ispirata::Hemera::DBusObject(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()),
                                                                               satelliteManagerPath, QDBusConnection::systemBus(), this);
    if (!d->satelliteManagerObjectInterface->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote Satellite Manager is not available. The daemon is probably not running."));
        return;
    }

    // Cache properties and connect to signals
    Hemera::DBusVariantMapOperation *operation = new Hemera::DBusVariantMapOperation(d->satelliteManagerObjectInterface->allProperties(), this);
    connect(operation, &Hemera::Operation::finished, [this, operation] {
        if (operation->isError()) {
            setInitError(operation->errorName(), operation->errorMessage());
            return;
        }

        // Set the various properties
        QVariantMap result = operation->result();

        d->satellites = result.value(QStringLiteral("LaunchedSatellites")).toStringList();
        d->activeSatellites = result.value(QStringLiteral("ActiveSatellites")).toStringList();
        d->applicationsInSatellites.clear();
        for (const QString & satellite : d->satellites) {
            d->applicationsInSatellites << d->serviceManager->applicationsForService(d->serviceManager->findServiceById(satellite));
        }

        setOnePartIsReady();
    });

    connect(d->satelliteManagerObjectInterface, &com::ispirata::Hemera::DBusObject::propertiesChanged, [this] (const QVariantMap &changed) {
        if (changed.contains(QStringLiteral("ActiveSatellites"))) {
                d->activeSatellites = changed.value(QStringLiteral("ActiveSatellites")).toStringList();
                Q_EMIT activeSatellitesChanged();
            }
            if (changed.contains(QStringLiteral("LaunchedSatellites"))) {
                d->satellites = changed.value(QStringLiteral("LaunchedSatellites")).toStringList();
                // Update list of applications as satellites
                d->applicationsInSatellites.clear();
                for (const QString & satellite : d->satellites) {
                    d->applicationsInSatellites << d->serviceManager->applicationsForService(d->serviceManager->findServiceById(satellite));
                }
                Q_EMIT launchedSatellitesChanged();
            }
    });

    d->serviceManager = new Hemera::ServiceManager(this);
    connect(d->serviceManager->init(), &Hemera::Operation::finished, [this] (Hemera::Operation *op) {
        if (op->isError()) {
            setInitError(op->errorName(), op->errorMessage());
        } else {
            setOnePartIsReady();
        }
    });

    new ApplicationHandlerAdaptor(this);

    // Watch over for Planets
    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(Hemera::Literals::literal(Hemera::Literals::DBus::planetService()),
                                                           QDBusConnection(Hemera::Literals::DBus::starBus()), QDBusServiceWatcher::WatchForUnregistration,
                                                           this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, [this] {
        // If any, shutdown all satellites.
        if (!d->satellites.isEmpty()) {
            d->satelliteManagerInterface->ShutdownAllSatellites();
        }
    });

    // And we need to check whether existing applications are up. Maybe we crashed before.
    // Blocking the bus seems the only options, but we should be rather safe during the init phase.
    // TBD
    for (const QString &busName : d->dbus.interface()->registeredServiceNames().value()) {
        if (busName.startsWith(QLatin1Char(':'))) {
            // It's not a well known name, not interesting. Save some CPU cycles and return.
            continue;
        }

//        checkApplicationRegistered(busName, QString(), QStringLiteral(":1.x"));
    }

    setOnePartIsReady();
}

bool ApplicationHandler::appIsSatellite(const QString & app) const
{
    return d->applicationsInSatellites.contains(app);
}

Hemera::Operation *ApplicationHandler::setActive(Application *application, bool active)
{
    if (!livingApplications().contains(application->id())) {
        return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                                            QStringLiteral("Requested activation of a non-living application"));
    }

    // Activate/Deactivate the application
    switch (application->status()) {
        case Hemera::Application::ApplicationStatus::Stopped:
            if (active) {
                // All fine. Activate!
                Hemera::Operation *op = application->start();
                // Do we need to take some action, too?
                if (application->isSatellite() && d->activationPolicies & Hemera::Planet::ActivationPolicy::KeepAtMostOneActive) {
                    // Each active application needs to be stopped.
                    for (Application *a : livingApplications()) {
                        if (a == application) {
                            continue;
                        }

                        if (a->status() == Hemera::Application::Running || a->status() == Hemera::Application::Starting) {
                            // Deactivate
                            setActive(a, false);
                        }
                    }
                }
                return op;
            } else {
                qWarning() << "A orbit activation has been requested, but application " << application->id()
                            << "is already started. Moving on, but this is weird.";
                return new Hemera::SuccessOperation;
            }
            break;
        case Hemera::Application::ApplicationStatus::Running:
            if (!active) {
                // All fine. Deactivate!
                return application->stop();
            } else {
                qWarning() << "A orbit deactivation has been requested, but application " << application->id()
                            << "is already stopped. Moving on, but this is weird.";
                return new Hemera::SuccessOperation;
            }
            break;
        case Hemera::Application::ApplicationStatus::NotInitialized:
        case Hemera::Application::ApplicationStatus::Initializing:
            qWarning() << "The application is apparently not initialized yet. This should never happen, "
                        << "something really strange is going on!";
            return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                                QStringLiteral("The application is apparently not initialized yet. This should never happen, "
                                                               "something really strange is going on!"));
            break;
        case Hemera::Application::ApplicationStatus::Failed:
            qWarning() << "The application failed! Soon this Orbit will be dead.";
            return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                                QStringLiteral("The application failed! Soon this Orbit will be dead."));
            break;
        case Hemera::Application::ApplicationStatus::ReadyForShutdown:
        case Hemera::Application::ApplicationStatus::ShuttingDown:
            qWarning() << "The application is shutting down! Soon this Orbit will be dead.";
            return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                                QStringLiteral("The application is shutting down! Soon this Orbit will be dead."));
            break;
        case Hemera::Application::ApplicationStatus::Starting:
            if (active) {
                qWarning() << "A orbit activation has been requested, but application " << application->id()
                            << "is already starting. Moving on, but this is weird.";
                return new Hemera::SuccessOperation;
            } else {
                // TODO: We need to stack up the actions somehow
            }
            break;
        case Hemera::Application::ApplicationStatus::Stopping:
            if (!active) {
                qWarning() << "A orbit deactivation has been requested, but application " << application->id()
                            << "is already stopping. Moving on, but this is weird.";
                return new Hemera::SuccessOperation;
            } else {
                // TODO: We need to stack up the actions somehow
            }
            break;
        case Hemera::Application::ApplicationStatus::Unknown:
            qWarning() << "The application is floating in an unknown status. This should never happen, "
                        << "something really strange is going on!";
            return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()),
                                                QStringLiteral("The application is floating in an unknown status. This should never happen, "
                                                               "something really strange is going on!"));
    }

    return new Hemera::FailureOperation(Hemera::Literals::literal(Hemera::Literals::Errors::unhandledRequest()), QStringLiteral("Unhandled request"));
}

QHash< QString, Application* > ApplicationHandler::livingApplications() const
{
    return d->livingApplications;
}

Hemera::Operation *ApplicationHandler::registerApplication(const QString& service)
{
    // Verify if it is a satellite
    bool isSatellite = d->applicationsInSatellites.contains(service);

    Application *application = new Application(service, isSatellite, d->dbus, this);
    Hemera::Operation *op = application->init();
    connect(op, &Hemera::Operation::finished, [this, service, application, op] {
        if (op->isError()) {
            qWarning() << "The DBus service" << service << "asked to register an application, but the"
                       << "initialization of DBus communications failed! This is either a bug in the SDK or an hijacking attempt.";
            qWarning() << "The error reported was: " << op->errorName() << op->errorMessage();
            return;
        }

        d->livingApplications.insert(service, application);
        Q_EMIT applicationRegistered(service, application);

        // Check if we need to be active, we should start it up (it's guaranteed the app is stopped
        // at this stage due to Gravity::Application clever init)
        if (!application->isSatellite() || d->activationPolicies & Hemera::Planet::ActivationPolicy::ActivateOnLaunch) {
            qDebug() << "Session active - let's roll";
            Hemera::Operation *op = application->start();
            connect(op, &Hemera::Operation::finished, [this,op] () {
                if (op->isError()) {
                    qWarning() << "Could not start application!" << op->errorName() << op->errorMessage();
                } else {
                    qDebug() << "Application started successfully!";
                }
            });
        }
    });

    return op;
}


void ApplicationHandler::LaunchOrbitAsSatellite(const QString& orbit)
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    QStringList applicationsInService = d->serviceManager->applicationsForService(d->serviceManager->findServiceById(orbit));
    d->applicationsInSatellites << applicationsInService;

    connect(new Hemera::DBusVoidOperation(d->satelliteManagerInterface->LaunchOrbitAsSatellite(orbit)), &Hemera::Operation::finished,
            [this, m, c, applicationsInService] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
            for (const QString &app : applicationsInService) {
                d->applicationsInSatellites.removeOne(app);
            }
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::ShutdownSatellite(const QString& orbit)
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(new Hemera::DBusVoidOperation(d->satelliteManagerInterface->ShutdownSatellite(orbit)), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::ShutdownAllSatellites()
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(new Hemera::DBusVoidOperation(d->satelliteManagerInterface->ShutdownAllSatellites()), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::ActivateSatellites(const QStringList& satellites)
{
    if (!calledFromDBus()) {
        return;
    }

    QList<Hemera::Operation*> opList;
    for (const QString &satellite : satellites) {
        Application *a = livingApplications().value(satellite);
        if (!a) {
            continue;
        }
        if (!a->isSatellite()) {
            continue;
        }

        opList << setActive(a, true);
    }

    if (opList.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("No matching satellites found"));
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(new Hemera::CompositeOperation(opList, this), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::DeactivateSatellites(const QStringList& satellites)
{
    if (!calledFromDBus()) {
        return;
    }

    QList<Hemera::Operation*> opList;
    for (const QString &satellite : satellites) {
        Application *a = livingApplications().value(satellite);
        if (!a) {
            continue;
        }
        if (!a->isSatellite()) {
            continue;
        }

        opList << setActive(a, false);
    }

    if (opList.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("No matching satellites found"));
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(new Hemera::CompositeOperation(opList, this), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::DeactivateAllSatellites()
{
    // Build opList
    if (!calledFromDBus()) {
        return;
    }

    QList<Hemera::Operation*> opList;
    for (Application *a : livingApplications()) {
        if (!a->isSatellite() || (a->status() != Hemera::Application::Running && a->status() != Hemera::Application::Starting)) {
            continue;
        }

        opList << setActive(a, false);
    }

    if (opList.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("No active satellites found"));
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(new Hemera::CompositeOperation(opList, this), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ApplicationHandler::SetActivationPolicies(uint policies)
{
    d->activationPolicies = static_cast<Hemera::Planet::ActivationPolicies>(policies);
}

uint ApplicationHandler::activationPolicies() const
{
    return static_cast<uint>(d->activationPolicies);
}

QStringList ApplicationHandler::activeSatellites() const
{
    return d->activeSatellites;
}

QStringList ApplicationHandler::launchedSatellites() const
{
    return d->satellites;
}

}
