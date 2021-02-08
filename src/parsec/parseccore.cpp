#include "parseccore.h"

#include <GravitySupermassive/Application>
#include <GravitySupermassive/ApplicationHandler>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>
#include <HemeraCore/Operation>

#include <QtCore/QTimer>

#include <QtDBus/QDBusServiceWatcher>

#include <pwd.h>
#include <sys/types.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-login.h>

#include <gravityconfig.h>

#include "starsequenceinterface.h"
#include "dbusobjectinterface.h"

#include "parsecadaptor.h"

class ParsecCore::Private
{
public:
    Private(ParsecCore *q) : q(q), shuttingDown(false) {}

    ParsecCore *q;

    Gravity::ApplicationHandler *applicationHandler;
    com::ispirata::Hemera::Gravity::StarSequence *starSequenceInterface;
    com::ispirata::Hemera::DBusObject *starSequenceObjectInterface;

    QString starName;
    QString baseConfigPath;

    QString activeOrbit;
    bool isInhibited;
    QVariantMap inhibitionReasons;
    uint phase;

    bool shuttingDown;

    QPointer< QDBusServiceWatcher > busWatcher;
    QHash< QString, uint > serviceToInhibitionCookie;
};

ParsecCore::ParsecCore(QObject* parent)
    : AsyncInitDBusObject(parent)
    , d(new Private(this))
{
    sd_notify(0, "STATUS=Parsec not initialized, Core alive - idle");
}

ParsecCore::~ParsecCore()
{

}

void ParsecCore::initImpl()
{
    sd_notify(0, "STATUS=Parsec is initializing...");

    d->starName = QLatin1String(qgetenv("HEMERA_STAR"));

    // We need to connect to the Star bus. Open the connection!
    QString starBusSocketPath = QStringLiteral("unix:path=%1/%2/dbus/star_bus_socket").arg(QLatin1String(Gravity::StaticConfig::hemeraStarsRuntimeDir()), d->starName);
    QDBusConnection starBusConnection = QDBusConnection::connectToBus(starBusSocketPath, QStringLiteral("starbus"));

    if (!starBusConnection.isConnected()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::dbusObjectNotAvailable()),
                     QStringLiteral("Couldn't open a connection to the Star bus %1. This is a serious internal error.").arg(starBusSocketPath));
        return;
    }


    if (!starBusConnection.registerService(Hemera::Literals::literal(Hemera::Literals::DBus::parsecService()))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerServiceFailed()),
                    QStringLiteral("Failed to register the service on the bus"));
        return;
    }
    if (!starBusConnection.registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::parsecPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerObjectFailed()),
                    QStringLiteral("Failed to register the object on the bus"));
        return;
    }

    // Just for systemd
    if (!QDBusConnection::sessionBus().registerService(Hemera::Literals::literal(Hemera::Literals::DBus::parsecService()))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerServiceFailed()),
                    QStringLiteral("Failed to register the service on the bus"));
        return;
    }

    // Setup systemd's software watchdog, if enabled
    bool watchdogConverted;
    qulonglong watchdogTime = qgetenv("WATCHDOG_USEC").toULongLong(&watchdogConverted);
    if (watchdogConverted && watchdogTime > 0) {
        QTimer *watchdogTimer = new QTimer(this);
        watchdogTimer->setSingleShot(false);
        watchdogTimer->setInterval(watchdogTime / 1000 / 2);
        QObject::connect(watchdogTimer, &QTimer::timeout, [] {
            sd_notify(0, "WATCHDOG=1");
        });
        watchdogTimer->start();
    }

    // Bus watcher
    d->busWatcher = new QDBusServiceWatcher(this);
    d->busWatcher.data()->setConnection(starBusConnection);
    d->busWatcher.data()->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(d->busWatcher.data(), &QDBusServiceWatcher::serviceUnregistered, [this] (const QString &service) {
        if (d->serviceToInhibitionCookie.contains(service)) {
            // Ouch - the application quit or crashed without releasing its inhibitions. Let's fix that.
            uint cookie = d->serviceToInhibitionCookie.take(service);
            d->starSequenceInterface->releaseOrbitSwitchInhibition(cookie);
        }
    });

    // Async init chain
    setParts(3);

    d->applicationHandler = new Gravity::ApplicationHandler(d->starName, starBusConnection, this);

    connect(d->applicationHandler, &Gravity::ApplicationHandler::applicationRegistered, this, &ParsecCore::updateSystemdStatus);
    connect(d->applicationHandler, &Gravity::ApplicationHandler::applicationUnregistered, this, &ParsecCore::updateSystemdStatus);
    Hemera::Operation *op = d->applicationHandler->init();

    connect(op, &Hemera::Operation::finished, [this, op] {
        if (op->isError()) {
            setInitError(op->errorName(), op->errorMessage());
        } else {
            setOnePartIsReady();
            // It is now time to trigger the Star's ignition
            qDebug() << "Triggering star ignition";
            d->starSequenceInterface->Ignite();
        }
    });

    // Star sequence interface
    QString starSequencePath = QString::fromLatin1(Hemera::Literals::DBus::starSequencePath()).arg(d->starName);
    d->starSequenceInterface = new com::ispirata::Hemera::Gravity::StarSequence(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()),
                                                                                starSequencePath, QDBusConnection::systemBus(), this);
    if (!d->starSequenceInterface->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote Star Sequence is not available. The daemon is probably not running."));
        return;
    }
    d->starSequenceObjectInterface = new com::ispirata::Hemera::DBusObject(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()),
                                                                           starSequencePath, QDBusConnection::systemBus(), this);
    if (!d->starSequenceObjectInterface->isValid()) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("The remote Star Sequence is not available. The daemon is probably not running."));
        return;
    }

    // Cache properties and connect to signals
    Hemera::DBusVariantMapOperation *operation = new Hemera::DBusVariantMapOperation(d->starSequenceObjectInterface->allProperties(), this);
    connect(operation, &Hemera::Operation::finished, [this, operation] {
        if (operation->isError()) {
            setInitError(operation->errorName(), operation->errorMessage());
            return;
        }

        // Set the various properties
        QVariantMap result = operation->result();
        d->baseConfigPath = result.value(QStringLiteral("baseConfigPath")).toString();

        d->activeOrbit = result.value(QStringLiteral("activeOrbit")).toString();
        d->inhibitionReasons = result.value(QStringLiteral("inhibitionReasons")).toMap();
        d->isInhibited = result.value(QStringLiteral("isOrbitSwitchInhibited")).toBool();
        d->phase = result.value(QStringLiteral("phase")).toBool();

        setOnePartIsReady();
    });

    connect(d->starSequenceObjectInterface, &com::ispirata::Hemera::DBusObject::propertiesChanged, [this] (const QVariantMap &changed) {
        if (changed.contains(QStringLiteral("activeOrbit"))) {
            d->activeOrbit = changed.value(QStringLiteral("activeOrbit")).toString();
            Q_EMIT activeOrbitChanged();
        }
        if (changed.contains(QStringLiteral("inhibitionReasons"))) {
            d->inhibitionReasons = changed.value(QStringLiteral("inhibitionReasons")).toMap();
            Q_EMIT inhibitionChanged();
        }
        if (changed.contains(QStringLiteral("isOrbitSwitchInhibited"))) {
            d->isInhibited = changed.value(QStringLiteral("isOrbitSwitchInhibited")).toBool();
            Q_EMIT inhibitionChanged();
        }
        if (changed.contains(QStringLiteral("phase"))) {
            d->phase = changed.value(QStringLiteral("phase")).toUInt();
            Q_EMIT phaseChanged();
        }
    });

    new ParsecAdaptor(this);

    setOnePartIsReady();
}

void ParsecCore::updateSystemdStatus()
{
    if (d->shuttingDown) {
        sd_notify(0, "STATUS=Parsec is shutting down...");
    } else {
        // We're alive and kicking
        sd_notifyf(0, "STATUS=Parsec is supervising %d applications, and taking the main stage",
                   d->applicationHandler->livingApplications().count());
    }
}

Gravity::ApplicationHandler* ParsecCore::applicationHandler() const
{
    return d->applicationHandler;
}

QString ParsecCore::activeOrbit() const
{
    return d->activeOrbit;
}

QVariantMap ParsecCore::inhibitionReasons() const
{
    return d->inhibitionReasons;
}

bool ParsecCore::isOrbitSwitchInhibited() const
{
    return d->isInhibited;
}

uint ParsecCore::phase() const
{
    return d->phase;
}

void ParsecCore::registerApplication() const
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();

    connect(d->applicationHandler->registerApplication(message().service()), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

bool ParsecCore::AmIASatellite()
{
    if (!calledFromDBus()) {
        return false;
    }

    return d->applicationHandler->appIsSatellite(message().service());
}

void ParsecCore::inhibitOrbitSwitch(const QString &reason)
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();
    Hemera::DBusUIntOperation *op = new Hemera::DBusUIntOperation(d->starSequenceInterface->inhibitOrbitSwitch(message().service(), reason));

    connect(op, &Hemera::Operation::finished, [this, m, c, op] {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            d->serviceToInhibitionCookie.insert(m.service(), op->result());
            c.send(m.createReply());
        }
    });
}

void ParsecCore::releaseOrbitSwitchInhibition()
{
    if (!calledFromDBus()) {
        return;
    }

    QDBusMessage m = message();
    QDBusConnection c = connection();

    if (!d->serviceToInhibitionCookie.contains(m.service())) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()), QStringLiteral("This service does not have any active inhibitions."));
        return;
    }

    uint cookie = d->serviceToInhibitionCookie.value(m.service());
    Hemera::DBusVoidOperation *op = new Hemera::DBusVoidOperation(d->starSequenceInterface->releaseOrbitSwitchInhibition(cookie));

    connect(op, &Hemera::Operation::finished, [this, m, c, op] {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            d->serviceToInhibitionCookie.remove(m.service());
            c.send(m.createReply());
        }
    });
}

void ParsecCore::openURL(const QString &url, bool tryActivation)
{
}

void ParsecCore::reloadCurrentOrbit()
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();
    connect(new Hemera::DBusVoidOperation(d->starSequenceInterface->reloadCurrentOrbit()), &Hemera::Operation::finished, this, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}

void ParsecCore::requestOrbitSwitch(const QString &orbit)
{
    if (!calledFromDBus()) {
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    QDBusConnection c = connection();
    connect(new Hemera::DBusVoidOperation(d->starSequenceInterface->requestOrbitSwitch(orbit)), &Hemera::Operation::finished, [this, m, c] (Hemera::Operation *op) {
        if (op->isError()) {
            c.send(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            c.send(m.createReply());
        }
    });
}
