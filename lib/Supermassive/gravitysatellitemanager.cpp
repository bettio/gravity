#include "gravitysatellitemanager.h"

#include "gravityoperations.h"
#include "gravitysandbox.h"
#include "gravitysandboxmanager.h"
#include "gravitygalaxymanager.h"

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Planet>

#include <QtDBus/QDBusConnection>

#include "satellitemanageradaptor.h"
#include "systemdmanagerinterface.h"

namespace Gravity
{

class SatelliteManager::Private
{
public:
    OrgFreedesktopSystemd1ManagerInterface *systemdManager;

    QString star;

    QStringList launchedSatellites;
    QStringList activeSatellites;
};

SatelliteManager::SatelliteManager(const QString &star, QObject* parent)
    : AsyncInitDBusObject(parent)
    , d(new Private)
{
    d->star = star;
}

SatelliteManager::~SatelliteManager()
{
}

void SatelliteManager::initImpl()
{
    d->systemdManager = new org::freedesktop::systemd1::Manager(QStringLiteral("org.freedesktop.systemd1"),
                                                                QStringLiteral("/org/freedesktop/systemd1"),
                                                                QDBusConnection::systemBus(), this);

    if (!d->systemdManager->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("Systemd manager interface could not be found."));
        return;
    }

    setParts(2);

    connect(new Hemera::DBusVoidOperation(d->systemdManager->Subscribe(), this), &Hemera::Operation::finished,
            this, &SatelliteManager::setOnePartIsReady);

    QDBusConnection::systemBus().registerObject(QString::fromLatin1(Hemera::Literals::DBus::satelliteManagerPath()).arg(d->star), this);
    new SatelliteManagerAdaptor(this);

    setOnePartIsReady();
}

QStringList SatelliteManager::launchedSatellites() const
{
    return d->launchedSatellites;
}

QStringList SatelliteManager::activeSatellites() const
{
    return d->activeSatellites;
}

void SatelliteManager::LaunchOrbitAsSatellite(const QString& satellite)
{
    // Do we have the associated sandboxes for the satellite?
    if (!GalaxyManager::availableSandboxes().contains(satellite)) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()), QStringLiteral("Satellite %1 does not exist!").arg(satellite));
        return;
    }

    // Ok, cool. Time to hit it.
    setDelayedReply(true);

    QDBusMessage callerMessage = message();
    QDBusConnection callerConnection = connection();

    Sandbox s = GalaxyManager::availableSandboxes().value(satellite);
    Hemera::Operation *op = new ControlUnitOperation(s.service().arg(d->star), QString(), ControlUnitOperation::Mode::StartMode, d->systemdManager, this);

    connect(op, &Hemera::Operation::finished, [this, op, satellite, callerMessage, callerConnection] {
        if (op->isError()) {
            callerConnection.send(callerMessage.createErrorReply(op->errorName(), op->errorMessage()));
            return;
        }

        // Add it to our control list.
        d->launchedSatellites.append(satellite);
        Q_EMIT launchedSatellitesChanged();

        callerConnection.send(callerMessage.createReply());
    });
}

void SatelliteManager::ShutdownSatellite(const QString& satellite)
{
    // Is it launched?
    if (!d->launchedSatellites.contains(satellite)) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notFound()), QStringLiteral("Satellite %1 has not been launched!").arg(satellite));
        return;
    }

    // Ok, cool. Time to hit it.
    setDelayedReply(true);

    QDBusMessage callerMessage = message();
    QDBusConnection callerConnection = connection();

    Sandbox s = GalaxyManager::availableSandboxes().value(satellite);
    Hemera::Operation *op = new ControlUnitOperation(s.service().arg(d->star), QString(), ControlUnitOperation::Mode::StopMode, d->systemdManager, this);

    connect(op, &Hemera::Operation::finished, [this, op, satellite, callerMessage, callerConnection] {
        if (op->isError()) {
            callerConnection.send(callerMessage.createErrorReply(op->errorName(), op->errorMessage()));
            return;
        }

        // Add it to our control list.
        d->launchedSatellites.append(satellite);
        Q_EMIT launchedSatellitesChanged();

        callerConnection.send(callerMessage.createReply());
    });
}

void SatelliteManager::ShutdownAllSatellites()
{
    if (d->launchedSatellites.isEmpty()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("No launched satellites"));
        return;
    }

    setDelayedReply(true);
    // Shut them all down
    QList< Hemera::Operation* > shutdownOperations;

    QStringList currentSatellites = d->launchedSatellites;
    for (const QString &satellite : currentSatellites) {
        Sandbox s = GalaxyManager::availableSandboxes().value(satellite);
        Hemera::Operation *op = new ControlUnitOperation(s.service().arg(d->star), QString(), ControlUnitOperation::Mode::StopMode, d->systemdManager, this);
        shutdownOperations.append(op);

        // Trigger change
        connect(op, &Hemera::Operation::finished, [this, op, satellite] {
            if (!op->isError()) {
                d->launchedSatellites.removeOne(satellite);
                Q_EMIT launchedSatellitesChanged();
            }
        });
    }

    // Create a composite operation to track reply
    Hemera::CompositeOperation *op = new Hemera::CompositeOperation(shutdownOperations, this);
    QDBusMessage callerMessage = message();
    QDBusConnection callerConnection = connection();

    connect(op, &Hemera::Operation::finished, [this, op, callerMessage, callerConnection] {
        if (op->isError()) {
            callerConnection.send(callerMessage.createErrorReply(op->errorName(), op->errorMessage()));
            return;
        }

        callerConnection.send(callerMessage.createReply());
    });
}

}
