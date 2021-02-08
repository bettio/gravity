#ifndef GRAVITY_GRAVITYPLUGIN_H
#define GRAVITY_GRAVITYPLUGIN_H

#include <QtCore/QtPlugin>

#include <GravitySupermassive/Global>

namespace Gravity {

class GalaxyManager;
class PluginLoader;

/**
 * @brief Interface for building GRAVITY Plugins
 */
class HEMERA_GRAVITY_EXPORT Plugin : public QObject
{
    Q_OBJECT
public:
    enum Status : quint8 {
        Unloaded = 0,
        Loaded
    };

    explicit Plugin(QObject *parent = 0);
    virtual ~Plugin();

    Status status() const;
    QString name() const;

    GalaxyManager *galaxyManager();
protected:
    // Protected, and not a slot. Only the PluginLoader can load plugins.
    virtual void load() = 0;
    // Protected, and not a slot. Only the core can unload plugins.
    virtual void unload() = 0;

    void setName(const QString &name);

protected Q_SLOTS:
    void setLoaded();
    void setUnloaded();

Q_SIGNALS:
    void statusChanged(Gravity::Plugin::Status oldStatus, Gravity::Plugin::Status newStatus);

private:
    class Private;
    Private * const d;

    friend class Gravity::PluginLoader;
};

}

Q_DECLARE_INTERFACE(Gravity::Plugin, "com.ispirata.Hemera.GravityCenter.Plugin")

#endif // HEMERA_GRAVITY_GRAVITYPLUGIN_H
