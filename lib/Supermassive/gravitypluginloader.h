#ifndef GRAVITY_PLUGINLOADER_H
#define GRAVITY_PLUGINLOADER_H

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <HemeraCore/AsyncInitObject>

#include <GravitySupermassive/Global>

class QFileSystemWatcher;
class QPluginLoader;

namespace Gravity {

class GalaxyManager;
class Plugin;

class HEMERA_GRAVITY_EXPORT PluginLoader : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PluginLoader)

    Q_PRIVATE_SLOT(d, void reloadAvailablePlugins())

public:
    explicit PluginLoader(GalaxyManager *applianceManager, QObject *parent = Q_NULLPTR);
    virtual ~PluginLoader();

    void registerStaticPlugin(Gravity::Plugin *staticPlugin);

    QStringList availablePlugins() const;

public Q_SLOTS:
    void shutdownAndDestroy();
    void loadAllAvailablePlugins();

Q_SIGNALS:
    void pluginLoaded(const QString &name, Gravity::Plugin *plugin);
    void pluginUnloaded(const QString &name);
    void readyForShutdown();

protected Q_SLOTS:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private Q_SLOTS:
    void unloadAllPlugins();

private:
    class Private;
    Private * const d;

    void loadPlugin(const QString &name);
    void reloadPlugin(const QString &name);
    void unloadPlugin(const QString &name);
};

}

#endif // GRAVITY_PLUGINLOADER_H
