#include "core.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QSocketNotifier>
#include <QtCore/QTimer>

#include <QtQml/QQmlEngine>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusServiceWatcher>

#include <HemeraCore/Literals>
#include <HemeraCore/Operation>

#include <GravitySupermassive/Application>
#include <GravitySupermassive/DeviceManagement>
#include <GravitySupermassive/Plugin>
#include <GravitySupermassive/PluginLoader>
#include <GravitySupermassive/RemovableStorageManager>
#include <GravitySupermassive/GalaxyManager>

#include <systemd/sd-daemon.h>

#include <gravityconfig.h>

class Core::Private
{
public:
    Gravity::PluginLoader *pluginLoader;
    Gravity::GalaxyManager *galaxyManager;
    Gravity::DeviceManagement *deviceManagement;
    Gravity::RemovableStorageManager *removableStorageManager;
    QQmlEngine *qmlEngine;
};

Core::Core(QObject *parent)
    : AsyncInitDBusObject(parent)
    , d(new Private)
{
    sd_notify(0, "STATUS=Hemera Gravity Center not initialized, Core alive - idle");
}

Core::~Core()
{
    delete d;
}

void Core::initImpl()
{
    sd_notify(0, "STATUS=Hemera Gravity Center is initializing...");

    // Welcome to Hemera. Before we begin, we must set up a couple things we need.
    // First of all, we need to create stars' runtime dir, if it doesn't exist.
    QDir starsRuntimeDir(QLatin1String(Gravity::StaticConfig::hemeraStarsRuntimeDir()));
    if (!starsRuntimeDir.exists()) {
        if (!starsRuntimeDir.mkpath(QLatin1String(Gravity::StaticConfig::hemeraStarsRuntimeDir()))) {
            return;
        }
    }

    d->galaxyManager = new Gravity::GalaxyManager(this);
    d->pluginLoader = new Gravity::PluginLoader(d->galaxyManager, this);
    d->deviceManagement = new Gravity::DeviceManagement(this);
    d->removableStorageManager = Gravity::RemovableStorageManager::instance();

    if (!QDBusConnection::systemBus().registerService(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterService()))) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerServiceFailed()),
                    QStringLiteral("Failed to register the service on the bus"));
        return;
    }
    if (!QDBusConnection::systemBus().registerObject(Hemera::Literals::literal(Hemera::Literals::DBus::gravityCenterPath()), this)) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::registerObjectFailed()),
                    QStringLiteral("Failed to register the object on the bus"));
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

    // Async init chain: PluginLoader -> GalaxyManager -> Core.
    Hemera::Operation *op = d->pluginLoader->init();

    connect(op, &Hemera::Operation::finished, [this, op] {
            if (op->isError()) {
                setInitError(op->errorName(), op->errorMessage());
            } else {
                Hemera::Operation *mOp = d->galaxyManager->init();
                connect(mOp, &Hemera::Operation::finished, [this, mOp] {
                        if (mOp->isError()) {
                            setInitError(mOp->errorName(), mOp->errorMessage());
                        } else {
                            d->pluginLoader->loadAllAvailablePlugins();
                            // Loading chain finalized: when ready, the main function will eventually
                            // notify systemd of the daemon's successful startup.
                            setReady();
                        }
                });

                // Init DeviceManagement and RemovableStorageManager in the meanwhile.
                connect(d->deviceManagement->init(), &Hemera::Operation::finished, [this] (Hemera::Operation *dmgmtOp) {
                    if (dmgmtOp->isError()) {
                        qWarning() << "DeviceManagement could not be initialized! Some features will not work!";
                        return;
                    }

                    qDebug() << "DeviceManagement initialized successfully.";
                });
                connect(d->removableStorageManager->init(), &Hemera::Operation::finished, [this] (Hemera::Operation *rstrgOp) {
                    if (rstrgOp->isError()) {
                        qWarning() << "RemovableStorageManager could not be initialized! Some features will not work!";
                        return;
                    }

                    qDebug() << "RemovableStorageManager initialized successfully.";
                });
            }
    });
}

Gravity::PluginLoader* Core::pluginLoader() const
{
    return d->pluginLoader;
}

Gravity::GalaxyManager *Core::galaxyManager() const
{
    return d->galaxyManager;
}
