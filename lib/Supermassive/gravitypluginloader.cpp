#include "gravitypluginloader.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QPluginLoader>

#include "gravityplugin_p.h"

#include <gravityconfig.h>

using namespace Gravity;

class PluginLoader::Private
{
public:
    Private(PluginLoader *q) : q(q), fsWatcher(new QFileSystemWatcher(QStringList() << QLatin1String(StaticConfig::gravityCenterPluginsPath()), q)) {}

    PluginLoader *q;
    QFileSystemWatcher *fsWatcher;
    QHash<QString, QString> availablePluginDefinitions;
    QHash<QString, Gravity::Plugin*> loadedPlugins;

    GalaxyManager *applianceManager;

    // Q_PRIVATE_SLOTS
    void reloadAvailablePlugins();
};

void PluginLoader::Private::reloadAvailablePlugins()
{
    QDir pluginsDir(QLatin1String(StaticConfig::gravityCenterPluginsPath()));
    availablePluginDefinitions.clear();

    for (const QString &fileName : pluginsDir.entryList(QStringList() << QStringLiteral("*.so"), QDir::Files)) {
        availablePluginDefinitions.insert(fileName, pluginsDir.absoluteFilePath(fileName));
    }
}


PluginLoader::PluginLoader(GalaxyManager *applianceManager, QObject *parent)
    : Hemera::AsyncInitObject(parent)
    , d(new Private(this))
{
    d->applianceManager = applianceManager;
}

PluginLoader::~PluginLoader()
{
    unloadAllPlugins();
}

void PluginLoader::shutdownAndDestroy()
{
    auto checkForDeletion = [this] () {
        if (d->loadedPlugins.isEmpty()) {
            // Just delete
            deleteLater();
        }
    };

    connect(this, &Gravity::PluginLoader::pluginUnloaded, checkForDeletion);

    checkForDeletion();

    unloadAllPlugins();
}

void PluginLoader::loadAllAvailablePlugins()
{
    for (const QString &plugin : availablePlugins()) {
        loadPlugin(plugin);
    }
}

void PluginLoader::initImpl()
{
    connect(d->fsWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(reloadAvailablePlugins()));
    d->reloadAvailablePlugins();

    setReady();
}

QStringList PluginLoader::availablePlugins() const
{
    return d->availablePluginDefinitions.keys();
}

void PluginLoader::registerStaticPlugin(Plugin *staticPlugin)
{
    d->loadedPlugins.insert(staticPlugin->name(), staticPlugin);

    connect(staticPlugin, &QObject::destroyed, [this] (QObject *target) {
        Gravity::Plugin *plugin = qobject_cast< Gravity::Plugin* >(target);
        if (plugin) {
            plugin->d->applianceManager = d->applianceManager;
            d->loadedPlugins.remove(plugin->name());
            Q_EMIT pluginUnloaded(plugin->name());
        }
    });

    Q_EMIT pluginLoaded(staticPlugin->name(), staticPlugin);
}

void PluginLoader::loadPlugin(const QString& name)
{
    if (!d->availablePluginDefinitions.contains(name)) {
        return;
    }

    // Create plugin loader
    QPluginLoader* pluginLoader = new QPluginLoader(d->availablePluginDefinitions.value(name), this);
    // Load plugin
    if (!pluginLoader->load()) {
        qDebug() << "Could not init plugin loader:" << pluginLoader->errorString();
        pluginLoader->deleteLater();
        return;
    }

    QObject *plugin = pluginLoader->instance();
    if (plugin) {
        // Plugin instance created
        Gravity::Plugin* gravityPlugin  = qobject_cast<Gravity::Plugin*>(plugin);
        if (!gravityPlugin) {
            // Interface was wrong
            qDebug() << "Fail!!" << pluginLoader->errorString();
            pluginLoader->deleteLater();
            return;
        }

        gravityPlugin->d->applianceManager = d->applianceManager;
        gravityPlugin->load();
        d->loadedPlugins.insert(name, gravityPlugin);

        Q_EMIT pluginLoaded(name, gravityPlugin);
    } else {
        qWarning() << "Could not load plugin" << name << pluginLoader->errorString();
    }

    pluginLoader->deleteLater();
}

void PluginLoader::reloadPlugin(const QString& name)
{
    if (!d->loadedPlugins.contains(name)) {
        return;
    }

    unloadPlugin(name);
    loadPlugin(name);
}

void PluginLoader::unloadPlugin(const QString& name)
{
    if (!d->loadedPlugins.contains(name)) {
        return;
    }

    Gravity::Plugin *plugin = d->loadedPlugins.value(name);

    connect (plugin, &Gravity::Plugin::statusChanged, [this, plugin, name] {
            if (plugin->status() == Plugin::Unloaded) {
                d->loadedPlugins.remove(name);
                Q_EMIT pluginUnloaded(name);
                plugin->deleteLater();
            }
    });

    plugin->unload();
}

void PluginLoader::unloadAllPlugins()
{
    // Not using iterators since the list will be modified.
    QStringList loadedPlugins = d->loadedPlugins.keys();

    for (const QString &plugin : loadedPlugins) {
        unloadPlugin(plugin);
    }
}

#include "moc_gravitypluginloader.cpp"
