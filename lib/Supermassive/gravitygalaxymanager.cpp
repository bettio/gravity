/*
 *
 */

#include "gravitygalaxymanager.h"

#include "gravitysandbox.h"
#include "gravitystarsequence.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QFileSystemWatcher>

#include <HemeraCore/Literals>

#include <gravityconfig.h>

#include "galaxymanageradaptor.h"

namespace Gravity
{

class GalaxyManager::Private
{
public:
    Private() {}

    bool hasGui;
    QString name;
    QHash< QDBusObjectPath, StarSequence* > stars;
    QHash< QString, Sandbox > sandboxPool;

    int shutdownCounter;

    void reloadSandboxPool();
};

void GalaxyManager::Private::reloadSandboxPool()
{
    // Clear the previous list.
    sandboxPool.clear();

    auto loadSandbox = [this] (const QString &file) {
        QSettings orbit(file, QSettings::NativeFormat);
        orbit.beginGroup(QStringLiteral("Sandbox")); {
            Sandbox sandbox(orbit.value(QStringLiteral("Name"), QString()).toString(),
                            orbit.value(QStringLiteral("Service"), QString()).toString());
            if (!sandbox.isValid()) {
                // Let's move forward, but...
            } else {
                sandboxPool.insert(orbit.value(QStringLiteral("Name"), QString()).toString(), sandbox);
            }
        } orbit.endGroup();
    };

    // Let's load all the orbits we have.
    QDir orbits(QString::fromLatin1(StaticConfig::configOrbitPath()));
    orbits.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QString &file : orbits.entryList(QStringList() << QStringLiteral("*.conf"))) {
        qDebug() << "Loading sandbox for " << orbits.absoluteFilePath(file);
        loadSandbox(orbits.absoluteFilePath(file));
    }
}


static GalaxyManager *s_instance = 0;

GalaxyManager::GalaxyManager(QObject* parent)
    : Hemera::AsyncInitDBusObject(parent)
    , d(new Private)
{
    if (s_instance) {
        Q_ASSERT("Trying to create an additional instance! Only one GalaxyManager per process can exist.");
    }

    s_instance = this;
}

GalaxyManager::~GalaxyManager()
{
    delete d;
}

GalaxyManager *GalaxyManager::instance()
{
    return s_instance;
}

void GalaxyManager::initImpl()
{
    // Reload the sandboxes
    d->reloadSandboxPool();

    // And watch over them.
    QFileSystemWatcher *watcher = new QFileSystemWatcher(this);
    watcher->addPaths(QStringList() << QString::fromLatin1(StaticConfig::configOrbitPath())
                                    << QString::fromLatin1("%1/orbits").arg(QString::fromLatin1(StaticConfig::hemeraServicesPath())));
    connect(watcher, &QFileSystemWatcher::directoryChanged, [this] { d->reloadSandboxPool(); });

    // Let's load the appliance file.
    QSettings galaxy(QString::fromLatin1("%1/galaxy.conf").arg(QLatin1String(StaticConfig::configGravityPath())), QSettings::NativeFormat);
    QStringList stars;

    galaxy.beginGroup(QStringLiteral("Galaxy")); {
        d->hasGui = galaxy.value(QStringLiteral("HasGui"), false).toBool();
        d->name = galaxy.value(QStringLiteral("Name"), QStringLiteral("Unnamed Hemera Galaxy")).toString();
        stars = galaxy.value(QStringLiteral("Stars"), QStringList()).toStringList();

        // Let's create the main orbit handler for the appliance
        galaxy.beginGroup(QStringLiteral("BaseOrbit")); {
            // First of all, we don't necessarily need to instantiate a base orbit if no such thing as an active or resident orbit are not to be found.
            if (galaxy.contains(QStringLiteral("ActiveOrbit")) || galaxy.contains(QStringLiteral("ResidentOrbit"))) {
                StarSequence *handler = new StarSequence(QStringLiteral("Headless"),
                                                         galaxy.value(QStringLiteral("ActiveOrbit"), QString()).toString(),
                                                         galaxy.value(QStringLiteral("ResidentOrbit"), QString()).toString(),
                                                         this);
                d->stars.insert(handler->busPath(), handler);
            }
        } galaxy.endGroup();
    } galaxy.endGroup();

    if (d->hasGui) {
        // We will need to create a sandbox for each declared screen
        galaxy.beginGroup(QStringLiteral("Stars"));

        for (const QString &star : stars) {
            galaxy.beginGroup(star); {
                // Now the handler
                StarSequence *handler = new StarSequence(star,
                                                         galaxy.value(QStringLiteral("ActiveOrbit"), QString()).toString(),
                                                         galaxy.value(QStringLiteral("ResidentOrbit"), QString()).toString(),
                                                         this);
                d->stars.insert(handler->busPath(), handler);
            } galaxy.endGroup();
        }
        galaxy.endGroup();
    }

    if (d->stars.isEmpty()) {
        // Uh-oh...
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::interfaceNotAvailable()),
                     QStringLiteral("No valid Stars have been found!"));
        return;
    }

    // Now we have to initialize all of the star sequences. We do this in parallel.
    setParts(d->stars.count() + 1);
    qDebug() << "Parts " << d->stars.count() + 1;
    for (StarSequence *handler : d->stars) {
        connect(handler->init(), &Hemera::Operation::finished, [this] (Hemera::Operation *op) {
            if (op->isError()) {
                // Fail utterly...
                setInitError(op->errorName(), op->errorMessage());
            } else {
                // Up
                setOnePartIsReady();
            }
        });
    }

    // In the meanwhile, it's our turn to get up on DBus
    QDBusConnection::systemBus().registerObject(QLatin1String(Hemera::Literals::DBus::gravityGalaxyManagerPath()), this);
    new GalaxyManagerAdaptor(this);

    setOnePartIsReady();
}

void GalaxyManager::igniteAllStars()
{
    for (StarSequence *handler : d->stars) {
        handler->Ignite();
    }
}

void GalaxyManager::collapseAllStars()
{
    d->shutdownCounter = d->stars.count();

    for (StarSequence *handler : d->stars) {
        connect(handler, &StarSequence::readyForShutdown, [this, handler] {
            --d->shutdownCounter;
            handler->deleteLater();
            if (d->shutdownCounter == 0) {
                // Done.
                Q_EMIT readyForShutdown();
            }
        });
        handler->Collapse();
    }

    d->stars.clear();
}

QHash< QString, Sandbox > GalaxyManager::availableSandboxes()
{
    return instance()->d->sandboxPool;
}

bool GalaxyManager::hasGui() const
{
    return d->hasGui;
}

QString GalaxyManager::name() const
{
    return d->name;
}

QHash< QDBusObjectPath, StarSequence* > GalaxyManager::stars() const
{
    return d->stars;
}

QList< QDBusObjectPath > GalaxyManager::starPaths() const
{
    return d->stars.keys();
}

}
