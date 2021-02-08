/*
 *
 */

#ifndef GRAVITY_APPLICATIONHANDLER_H
#define GRAVITY_APPLICATIONHANDLER_H

#include <HemeraCore/AsyncInitDBusObject>

#include <QtDBus/QDBusConnection>

namespace Gravity {

class Application;

class ApplicationHandler : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ApplicationHandler)

    Q_PROPERTY(QStringList ActiveSatellites READ activeSatellites NOTIFY activeSatellitesChanged)
    Q_PROPERTY(QStringList LaunchedSatellites READ launchedSatellites NOTIFY launchedSatellitesChanged)
    Q_PROPERTY(uint ActivationPolicies READ activationPolicies NOTIFY activationPoliciesChanged)

public:
    explicit ApplicationHandler(const QString &starName, const QDBusConnection &connection = QDBusConnection(QStringLiteral("starbus")),
                                QObject *parent = Q_NULLPTR);
    virtual ~ApplicationHandler();

    QHash< QString, Application* > livingApplications() const;

    bool appIsSatellite(const QString &app) const;

    QStringList activeSatellites() const;
    QStringList launchedSatellites() const;
    uint activationPolicies() const;

public Q_SLOTS:
    Hemera::Operation *setActive(Application *application, bool active);
    Hemera::Operation *registerApplication(const QString &service);

    void ActivateSatellites(const QStringList &satellites);
    void DeactivateSatellites(const QStringList &satellites);
    void DeactivateAllSatellites();

    void LaunchOrbitAsSatellite(const QString &orbit);
    void ShutdownSatellite(const QString &orbit);
    void ShutdownAllSatellites();

    void SetActivationPolicies(uint policies);

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void applicationRegistered(const QString &id, Gravity::Application *application);
    void applicationUnregistered(const QString &id);

    void activeSatellitesChanged();
    void launchedSatellitesChanged();
    void activationPoliciesChanged();

private:
    class Private;
    Private * const d;
};

}

#endif // GRAVITY_APPLICATIONHANDLER_H
