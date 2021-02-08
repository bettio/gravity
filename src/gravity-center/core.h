#ifndef GRAVITY_CORE_H
#define GRAVITY_CORE_H

#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

#include <HemeraCore/AsyncInitDBusObject>

namespace Gravity {
class PluginLoader;
class GalaxyManager;
}

class Core : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Core)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.GravityCenter")

public:
    explicit Core(QObject *parent = 0);
    virtual ~Core();

    Gravity::PluginLoader *pluginLoader() const;
    Gravity::GalaxyManager *galaxyManager() const;

protected Q_SLOTS:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private:
    class Private;
    Private * const d;
};

#endif // GRAVITY_CORE_H
