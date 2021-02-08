#include "gravitystarsequence_p.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusServiceWatcher>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlError>

#include <HemeraCore/CommonOperations>
#include <HemeraCore/Literals>

#include "gravityapplication.h"
#include "gravitygalaxymanager.h"
#include "gravitysatellitemanager.h"
#include "gravityconfig.h"

#include "starsequenceadaptor.h"
#include "systemdmanagerinterface.h"

#include <sys/types.h>
#include <pwd.h>
#include <systemd/sd-daemon.h>

namespace Gravity
{

OrbitReloadOperation::OrbitReloadOperation(ReloadHook hook, QObject *self, StarSequence *parent)
    : Hemera::Operation(parent)
    , m_handler(parent)
    , m_hook(hook)
    , m_self(self)
{
}

OrbitReloadOperation::~OrbitReloadOperation()
{
}

void OrbitReloadOperation::startImpl()
{
    // Unload, then hook, then load.
    qDebug() << "Reloading orbit";
    Sandbox sandbox = GalaxyManager::availableSandboxes().value(m_handler->d->activeOrbit);

    Hemera::Operation *op = m_handler->d->controlOrbitService(sandbox, ControlUnitOperation::Mode::StopMode);
    connect(op, &Hemera::Operation::finished, [this, op, sandbox] {
        if (op->isError()) {
            // If this one failed, we failed.
            setFinishedWithError(op->errorName(), op->errorMessage());
            return;
        }

        auto loadOrbit = [this, sandbox] () {
            // Now.
            Hemera::Operation *operation = m_handler->d->controlOrbitService(sandbox, ControlUnitOperation::Mode::StartMode);
            connect(operation, &Hemera::Operation::finished, [this, operation] {
                if (operation->isError()) {
                    // If this one failed, we in trouble. Just fail, we'd be in a fail loop otherwise...
                    qWarning() << "Reloading orbit failed!! Gravity is unstable!";
                    setFinishedWithError(operation->errorName(), operation->errorMessage());
                    return;
                }

                // Everything fine, not much to do.
                qDebug() << "Orbit reloaded";
                setFinished();
            });
        };

        // K, time to reload then. Hook it up first!
        Hemera::Operation *hook = m_hook(m_self);
        if (hook) {
            connect(hook, &Hemera::Operation::finished, loadOrbit);
        } else {
            loadOrbit();
        }
    });
}

OrbitSwitchOperation::OrbitSwitchOperation(const Sandbox &sandbox, StarSequence *parent)
    : Hemera::Operation(parent)
    , m_handler(parent)
    , m_sandbox(sandbox)
{
}

OrbitSwitchOperation::~OrbitSwitchOperation()
{
}

void OrbitSwitchOperation::startImpl()
{
    // Init variables
    m_previousOrbit = m_handler->activeOrbit();

    m_handler->d->setOrbit(QStringLiteral("switchingOrbit"));
    qDebug() << "Orbit turned to switching, processing request";
    m_handler->d->updateSystemdStatus();

    auto checkForErrors = [this] (Hemera::Operation *operation) -> bool {
        if (operation == Q_NULLPTR) {
            return false;
        }

        if (operation->isError()) {
            if (m_previousOrbit.isEmpty()) {
                qWarning() << "Could not initialize the startup orbit, " << m_sandbox.name() << ". No session will"
                           << "be active.";
                // We shall do a special rollback in this case.
                Hemera::Operation *op = m_handler->d->controlOrbitService(m_sandbox, ControlUnitOperation::Mode::StopMode);
                connect(op, &Hemera::Operation::finished, [this, operation] (Hemera::Operation *op) {
                    if (op->isError()) {
                        // This should really never happen, and we should abort if so.
                        qFatal("Rollback to initial state failed!! Hemera Gravity Center will abort.");
                        m_handler->d->setPhase(StarSequence::Phase::Collapse);
                        setFinishedWithError(op->errorName(), op->errorMessage());
                        return;
                    }

                    // Clean rollback. Let's move to blank.
                    qWarning() << "Star is a Nebula. No Orbit is currently running.";
                    m_handler->d->setPhase(StarSequence::Phase::Nebula);
                    m_handler->d->setOrbit(QString());
                    setFinishedWithError(operation->errorName(), operation->errorMessage());
                });

                return true;
            }

            // Hijack session change to make sure we do a clean rollback.
            m_handler->d->activeOrbit = m_sandbox.name();
            connect(new OrbitSwitchOperation(GalaxyManager::availableSandboxes().value(m_previousOrbit), m_handler), &Hemera::Operation::finished,
                    [this, operation] (Hemera::Operation *op) {
                        if (op->isError()) {
                            qWarning() << "Sequence of rollback errors, Star has Collapsed!";
                            m_handler->d->setPhase(StarSequence::Phase::Collapse);
                            setFinishedWithError(op->errorName(), op->errorMessage());
                        } else {
                            setFinishedWithError(operation->errorName(), operation->errorMessage());
                        }
                    });
            return true;
        }

        return false;
    };

    auto startNewOrbit = [this, checkForErrors] (Hemera::Operation *operation) {
        if (checkForErrors(operation)) {
            return;
        }

        // Load up all the needed units
        Hemera::Operation *op = m_handler->d->controlOrbitService(m_sandbox, ControlUnitOperation::Mode::StartMode);
        connect(op, &Hemera::Operation::finished, [this, checkForErrors] (Hemera::Operation *operation) {
            if (checkForErrors(operation)) {
                return;
            }

            // Gravity Center here, all systems are up!
            m_handler->d->setOrbit(m_sandbox.name());
            m_handler->d->setPhase(StarSequence::Phase::MainSequence);
            qDebug() << "Orbit switched";
            setFinished();
        });
    };

    if (!m_previousOrbit.isEmpty()) {
        // Unload all the loaded units.
        qDebug() << "Unloading units";
        Hemera::Operation *operation = m_handler->d->controlOrbitService(GalaxyManager::availableSandboxes().value(m_previousOrbit),
                                                                         ControlUnitOperation::Mode::StopMode);
        connect(operation, &Hemera::Operation::finished, startNewOrbit);
    } else {
        // We are probably starting up.
        startNewOrbit(Q_NULLPTR);
    }
}

void StarSequence::Private::updateSystemdStatus()
{
    if (!shouldUpdateSystemd) {
        // Don't update systemd in this case
        return;
    }

    if (activeOrbit == QStringLiteral("switchingOrbit")) {
        if (injectedOrbit.isEmpty()) {
            sd_notify(0, "STATUS=Hemera Gravity Center is switching orbits...");
        } else {
            sd_notifyf(0, "STATUS=Hemera Gravity Center is switching orbits. There is 1 injected orbit");
        }
    } else if (activeOrbit.isEmpty()) {
        if (injectedOrbit.isEmpty()) {
            sd_notify(0, "STATUS=Hemera Gravity Center active and running, but uninitialized. No orbits are running");
        } else {
            sd_notifyf(0, "STATUS=Hemera Gravity Center active and running, but uninitialized. There is 1 injected orbit");
        }
    } else {
        if (injectedOrbit.isEmpty()) {
            sd_notifyf(0, "STATUS=Hemera Gravity Center active, running orbit \"%s\"",
                       qstrdup(activeOrbit.toLatin1().constData()));
        } else {
            sd_notifyf(0, "STATUS=Hemera Gravity Center active, running orbit \"%s\". There is 1 injected orbits",
                       qstrdup(activeOrbit.toLatin1().constData()));
        }
    }
}

void StarSequence::Private::setPhase(Phase p)
{
    if (p != phase) {
        phase = p;
        Q_EMIT q->phaseChanged();
    }
}

Hemera::Operation *StarSequence::injectOrbit(const QString &orbit)
{
    // Check that we don't have any orbit with the same name already up
    if (!d->injectedOrbit.isEmpty() || d->activeOrbit == orbit || isOrbitSwitchInhibited()) {
        return Q_NULLPTR;
    }

    d->stashedActiveOrbit = d->activeOrbit;
    d->injectedOrbit = orbit;

    Hemera::Operation *op = d->requestOrbitSwitch(GalaxyManager::availableSandboxes().value(orbit));

    if (!op) {
        return Q_NULLPTR;
    }

    connect(op, &Hemera::Operation::finished, [this, op] {
        if (!op->isError()) {
            qDebug() << "Orbit injected!";
            d->setPhase(StarSequence::Phase::Injected);
            d->injectedToken = d->inhibitOrbitSwitch(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()),
                                                     QStringLiteral("An Orbit is currently injected."));
        }
    });

    return op;
}

Hemera::Operation *StarSequence::deinjectOrbit()
{
    if (d->injectedOrbit.isEmpty()) {
        return Q_NULLPTR;
    }

    d->releaseOrbitSwitchInhibition(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()), d->injectedToken);
    d->injectedToken = 0;
    Hemera::Operation *op = d->requestOrbitSwitch(GalaxyManager::availableSandboxes().value(d->stashedActiveOrbit));

    connect(op, &Hemera::Operation::finished, [this, op] {
        if (!op->isError()) {
            qDebug() << "Orbit deinjected!!";
            d->setPhase(StarSequence::Phase::MainSequence);
            d->stashedActiveOrbit.clear();
            d->injectedOrbit.clear();
        }
    });

    return op;
}

void StarSequence::Private::sendBackReply(const QDBusMessage &reply)
{
    QDBusConnection::systemBus().send(reply);
}

bool StarSequence::Private::canSwitchOrbit()
{
    return !q->isOrbitSwitchInhibited() && currentSwitchOperation.isNull();
}

void StarSequence::Private::setOrbit(const QString& newType)
{
    activeOrbit = newType;

    updateSystemdStatus();
    Q_EMIT q->activeOrbitChanged(activeOrbit);
}

Hemera::Operation *StarSequence::Private::controlOrbitService(const Sandbox &sandbox, ControlUnitOperation::Mode operationMode)
{
    return new ControlUnitOperation(sandbox.service().arg(star), QString(), operationMode, systemdManager, q);
}


StarSequence::StarSequence(const QString &star, const QString &activeOrbit, const QString &residentOrbit, QObject* parent)
    : Hemera::AsyncInitDBusObject(parent)
    , d(new Private(this))
{
    d->initialActiveOrbit = activeOrbit;
    d->residentOrbit = residentOrbit;
    d->busPath = QString::fromLatin1(Hemera::Literals::DBus::starSequencePath()).arg(star);
    d->star = star;
}

StarSequence::~StarSequence()
{
    delete d;
}

QDBusObjectPath StarSequence::busPath() const
{
    return QDBusObjectPath(d->busPath);
}

void StarSequence::setShouldUpdateSystemd(bool update)
{
    d->shouldUpdateSystemd = update;
}

void StarSequence::Collapse()
{
    if (d->isShuttingDown) {
        return;
    }

    if (d->activeOrbit == QStringLiteral("switchingOrbit")) {
        connect(this, &StarSequence::activeOrbitChanged, this, &StarSequence::Collapse, Qt::UniqueConnection);
        return;
    }

    d->isShuttingDown = true;

    // Release pwnam's memory
    endpwent();

    auto stopRunningOrbit = [this] () {
        // There might not be a orbit on. Check this before anything else.
        if (!GalaxyManager::availableSandboxes().contains(d->activeOrbit)) {
            // Nothing to do, let's roll
            Q_EMIT readyForShutdown();
            return;
        }

        Hemera::Operation *op = d->controlOrbitService(GalaxyManager::availableSandboxes().value(d->activeOrbit), ControlUnitOperation::Mode::StopMode);
        connect(op, &Hemera::Operation::finished, [this] {
                d->activeOrbit.clear();
                d->updateSystemdStatus();
                Q_EMIT readyForShutdown();
        });
    };

    // First of all, shutdown the resident orbit, if any.
    if (!d->residentOrbit.isEmpty()) {
        Hemera::Operation *op = d->controlOrbitService(GalaxyManager::availableSandboxes().value(d->residentOrbit), ControlUnitOperation::Mode::StopMode);
        connect(op, &Hemera::Operation::finished, stopRunningOrbit);
    } else {
        stopRunningOrbit();
    }
}

void StarSequence::initImpl()
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
            this, &StarSequence::setOnePartIsReady);

    QDBusConnection::systemBus().registerObject(d->busPath, this);
    new StarSequenceAdaptor(this);

    // Bring up satellite manager
    (new SatelliteManager(d->star, this))->init();

    setOnePartIsReady();
}

void StarSequence::Ignite()
{
    qDebug() << "Igniting Star Sequence for " << d->star;
    // Before anything else, we need to verify whether the system has booted or not.
    if (d->systemdManager->progress() < 1) {
        connect(d->systemdManager, &OrgFreedesktopSystemd1ManagerInterface::StartupFinished, this, &StarSequence::Ignite, Qt::UniqueConnection);
        return;
    }

    // It's time to begin with orbit loading. First of all, do we have a resident session?
    if (!d->residentOrbit.isEmpty()) {
        qDebug() << "Initializing resident orbit";
        Hemera::Operation *op = d->controlOrbitService(GalaxyManager::availableSandboxes().value(d->residentOrbit), ControlUnitOperation::Mode::StartMode);
        connect(op, &Hemera::Operation::finished, [this] (Hemera::Operation *operation) {
            if (operation->isError()) {
                qFatal("Resident orbit could not be started!! Hemera Gravity Center will abort.");
                return;
            }

            qDebug() << "Resident orbit initialized";
            // We are ready.
            d->requestOrbitSwitch(GalaxyManager::availableSandboxes().value(d->initialActiveOrbit));
        });
    } else {
        d->requestOrbitSwitch(GalaxyManager::availableSandboxes().value(d->initialActiveOrbit));
    }
}

bool StarSequence::isOrbitSwitchInhibited() const
{
    return !d->cookieToInhibition.isEmpty();
}

bool StarSequence::hasInjectedOrbit() const
{
    return !d->injectedOrbit.isEmpty();
}

QString StarSequence::star() const
{
    return d->star;
}

QString StarSequence::activeOrbit() const
{
    return d->activeOrbit;
}

QString StarSequence::residentOrbit() const
{
    return d->residentOrbit;
}

StarSequence::Phase StarSequence::phase() const
{
    return d->phase;
}

QVariantMap StarSequence::inhibitionReasons() const
{
    // Let's build the return type correctly to please QtDBus.
    QVariantMap result;
    for (QHash< quint16, QPair< QString, QString > >::const_iterator i = d->cookieToInhibition.constBegin(); i != d->cookieToInhibition.constEnd(); ++i) {
        result.insertMulti(i.value().first, i.value().second);
    }

    return result;
}

quint16 StarSequence::inhibitOrbitSwitch(const QString& requesterName, const QString& reason)
{
    if (!calledFromDBus()) {
        qWarning() << "Trying to hijack inhibition. This function should never be called outside a DBus Context. Rejecting.";
        return 0;
    }

    if (message().service().isEmpty()) {
        qWarning() << "The service name of the context is empty. Something terrible is going on. Rejecting.";
        // Send a reply
        sendErrorReply(QDBusError::InternalError, QStringLiteral("The caller is apparently not advertising any service name."));
        return 0;
    }

    return d->inhibitOrbitSwitch(requesterName, reason);
}

void StarSequence::releaseOrbitSwitchInhibition(quint16 cookie)
{
    if (!calledFromDBus()) {
        qWarning() << "Trying to hijack inhibition release. This function should never be called outside a DBus Context. Rejecting.";
        return;
    }

    if (message().service().isEmpty()) {
        qWarning() << "The service name of the context is empty. Something terrible is going on. Rejecting.";
        // Send a reply
        sendErrorReply(QDBusError::InternalError, QStringLiteral("The caller is apparently not advertising any service name."));
        return;
    }

    d->releaseOrbitSwitchInhibition(message().service(), cookie);
}

quint16 StarSequence::Private::inhibitOrbitSwitch(const QString& dbusService, const QString& reason)
{
    ++lastCookie;

    cookieToInhibition.insert(lastCookie, qMakePair(dbusService, reason));

    qDebug() << "Added inhibition from an explicit DBus service, " << dbusService << ", with cookie " <<
            lastCookie << " with " << reason;

    if (cookieToInhibition.size() == 1) {
        Q_EMIT q->isOrbitSwitchInhibitedChanged(q->isOrbitSwitchInhibited());
    }

    Q_EMIT q->inhibitionReasonsChanged(q->inhibitionReasons());

    return lastCookie;
}

void StarSequence::Private::releaseOrbitSwitchInhibition(const QString &dbusService, quint16 cookie)
{
    // Service matching happens in the DBus context. We do not really care to do that here -
    // after all, this API is merely internal to GRAVITY. I guess nobody here wants to hijack
    // anything... do we?

    qDebug() << "Released inhibition with cookie " << cookie;
    cookieToInhibition.remove(cookie);

    if (!q->isOrbitSwitchInhibited()) {
        Q_EMIT q->isOrbitSwitchInhibitedChanged(q->isOrbitSwitchInhibited());
    }

    Q_EMIT q->inhibitionReasonsChanged(q->inhibitionReasons());
}

void StarSequence::reloadCurrentOrbit()
{
    if (!calledFromDBus()) {
        qWarning() << "You are using the wrong overload of reloadCurrentOrbit! This one is meant for DBus only.";
        return;
    }

    if (!d->canSwitchOrbit()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("Orbit switch is not allowed at the moment. The orbit is being switched or inhibited."));
        return;
    }

    setDelayedReply(true);
    QDBusMessage m = message();
    connect(reloadCurrentOrbit(&nullReloadHook, nullptr), &Hemera::Operation::finished, [this, m] (Hemera::Operation *op) {
        if (!op || op->isError()) {
            StarSequence::Private::sendBackReply(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            StarSequence::Private::sendBackReply(m.createReply());
        }
    });
}

Hemera::Operation* StarSequence::reloadCurrentOrbit(ReloadHook hook, QObject *self)
{
    if (!d->canSwitchOrbit()) {
        return Q_NULLPTR;
    }

    qDebug() << "Handle reload orbit request";
    // Go!
    d->currentSwitchOperation = new OrbitReloadOperation(hook, self, this);
    return d->currentSwitchOperation.data();
}

void StarSequence::requestOrbitSwitch(const QString &orbit)
{
    if (!calledFromDBus()) {
        qWarning() << "You are using the wrong overload of requestOrbitSwitch! This one is meant for DBus only.";
        return;
    }

    if (!d->canSwitchOrbit()) {
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::notAllowed()),
                       QStringLiteral("Orbit switch is not allowed at the moment. The orbit is being switched or inhibited."));
        return;
    }

    if (!GalaxyManager::availableSandboxes().contains(orbit)) {
        // The request simply does not make any sense
        sendErrorReply(Hemera::Literals::literal(Hemera::Literals::Errors::badRequest()),
                       QStringLiteral("The requested orbit does not exist"));
        return;
    }

    // Now the reply has to be delayed
    setDelayedReply(true);
    QDBusMessage m = message();
    connect(d->requestOrbitSwitch(GalaxyManager::availableSandboxes().value(orbit)), &Hemera::Operation::finished, [this, m] (Hemera::Operation *op) {
        if (!op || op->isError()) {
            StarSequence::Private::sendBackReply(m.createErrorReply(op->errorName(), op->errorMessage()));
        } else {
            StarSequence::Private::sendBackReply(m.createReply());
        }
    });
}

Hemera::Operation *StarSequence::Private::requestOrbitSwitch(const Sandbox &sandbox)
{
    if (!canSwitchOrbit() || !sandbox.isValid()) {
        return Q_NULLPTR;
    }

    qDebug() << "Initializing orbit switch request, from " << activeOrbit << " to " << sandbox.name();

    // Go!
    currentSwitchOperation = new OrbitSwitchOperation(sandbox, q);
    return currentSwitchOperation.data();
}

}

#include "moc_gravitystarsequence.cpp"
