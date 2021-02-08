#ifndef GRAVITY_SANDBOXMANAGER_H
#define GRAVITY_SANDBOXMANAGER_H

#include <HemeraCore/AsyncInitDBusObject>

#include <GravitySupermassive/Global>

namespace Gravity {

class Sandbox;

class HEMERA_GRAVITY_EXPORT SandboxManager : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SandboxManager)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.GravityCenter.SandboxManager")
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.GravityCenter.SatelliteManager")

    Q_PROPERTY(QStringList runningSandboxes    READ runningSandboxes    NOTIFY runningSandboxesChanged)
    Q_PROPERTY(QStringList activeSandboxes     READ activeSandboxes     NOTIFY activeSandboxesChanged)

public:
    explicit SandboxManager(QObject *parent = nullptr);
    virtual ~SandboxManager();

    QStringList runningSandboxes() const;
    QStringList activeSandboxes() const;

    void ActivateSatellites(const QStringList &satellites);
    void DeactivateSatellites(const QStringList &satellites);
    void DeactivateAllSatellites();

    void LaunchOrbitAsSatellite(const QString &satellite);
    void ShutdownSatellite(const QString &satellite);
    void ShutdownAllSatellites();

    void SetSatelliteManagerPolicy(uint policies);

protected:
    virtual void initImpl() override final;

Q_SIGNALS:
    void runningSandboxesChanged();
    void activeSandboxesChanged();

private:
    class Private;
    Private * const d;
};
}

#endif // GRAVITY_SANDBOXMANAGER_H
