#ifndef GRAVITY_SATELLITEMANAGER_H
#define GRAVITY_SATELLITEMANAGER_H

#include <HemeraCore/AsyncInitDBusObject>

#include <GravitySupermassive/Global>

namespace Gravity {

class HEMERA_GRAVITY_EXPORT SatelliteManager : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SatelliteManager)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.Gravity.SatelliteManager")

    Q_PROPERTY(QStringList LaunchedSatellites READ launchedSatellites        NOTIFY launchedSatellitesChanged)
    Q_PROPERTY(QStringList ActiveSatellites   READ activeSatellites          NOTIFY activeSatellitesChanged)

public:
    explicit SatelliteManager(const QString &star, QObject* parent);
    virtual ~SatelliteManager();

    QStringList launchedSatellites() const;
    QStringList activeSatellites() const;

    void LaunchOrbitAsSatellite(const QString &satellite);
    void ShutdownSatellite(const QString &satellite);
    void ShutdownAllSatellites();

protected:
    virtual void initImpl() override final;

Q_SIGNALS:
    void launchedSatellitesChanged();
    void activeSatellitesChanged();
    void policiesChanged();

private:
    class Private;
    Private * const d;
};
}

#endif // GRAVITY_SATELLITEMANAGER_H
