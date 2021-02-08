/*
 *
 */

#ifndef GRAVITY_GALAXYMANAGER_H
#define GRAVITY_GALAXYMANAGER_H

#include <HemeraCore/AsyncInitDBusObject>

#include <QtDBus/QDBusObjectPath>

#include <GravitySupermassive/Global>

namespace Gravity {

class Sandbox;

class StarSequence;

class HEMERA_GRAVITY_EXPORT GalaxyManager : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GalaxyManager)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.GravityCenter.GalaxyManager")

    Q_PROPERTY(bool hasGui READ hasGui)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QList<QDBusObjectPath> stars READ starPaths)

public:
    GalaxyManager(QObject *parent = 0);
    virtual ~GalaxyManager();

    static GalaxyManager *instance();

    bool hasGui() const;
    QString name() const;
    QList< QDBusObjectPath > starPaths() const;
    QHash< QDBusObjectPath, StarSequence* > stars() const;

    static QHash< QString, Sandbox > availableSandboxes();

public Q_SLOTS:
    void igniteAllStars();
    void collapseAllStars();

protected Q_SLOTS:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void readyForShutdown();

private:
    class Private;
    Private * const d;
};
}

#endif // GRAVITY_GALAXYMANAGER_H
