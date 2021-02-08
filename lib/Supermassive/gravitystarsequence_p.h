#ifndef GRAVITY_GRAVITYSTARSEQUENCE_P_H
#define GRAVITY_GRAVITYSTARSEQUENCE_P_H

#include "gravitystarsequence.h"

#include "gravityoperations.h"

#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusServiceWatcher>

#include <QtQml/QQmlComponent>

class OrgFreedesktopSystemd1ManagerInterface;

namespace Gravity
{

struct Orbit
{
    QString name;
    quint32 uid;

    QObject *object;
};

class OrbitReloadOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(OrbitReloadOperation)
public:
    explicit OrbitReloadOperation(ReloadHook hook, QObject *m_self, StarSequence *parent);
    virtual ~OrbitReloadOperation();

    virtual void startImpl();

private:
    StarSequence *m_handler;
    ReloadHook m_hook;
    QObject *m_self;
};

class OrbitSwitchOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(OrbitSwitchOperation)
public:
    explicit OrbitSwitchOperation(const Sandbox &sandbox, StarSequence *parent);
    virtual ~OrbitSwitchOperation();

    virtual void startImpl();

private:
    StarSequence *m_handler;
    Sandbox m_sandbox;

    QString m_previousOrbit;
};

class StarSequence::Private
{
public:
    Private(StarSequence *q) : q(q), phase(Phase::Unknown), lastCookie(0),
                               isShuttingDown(false), shouldUpdateSystemd(true) {}

    StarSequence *q;

    Phase phase;

    QString star;

    QString residentOrbit;
    QString injectedOrbit;

    QString initialActiveOrbit;
    QString activeOrbit;

    // holds the name of the orbit active before an orbit is injected
    QString stashedActiveOrbit;

    QPointer<Hemera::Operation> currentSwitchOperation;

    QHash< quint16, QPair<QString, QString> > cookieToInhibition;

    quint16 lastCookie;

    QString busPath;

    OrgFreedesktopSystemd1ManagerInterface *systemdManager;

    bool isShuttingDown;
    bool shouldUpdateSystemd;

    void setPhase(Phase status);

    bool canSwitchOrbit();

    void setOrbit(const QString &newType);
    Hemera::Operation *controlOrbitService(const Sandbox &sandbox, ControlUnitOperation::Mode operationMode);

    Hemera::Operation *requestOrbitSwitch(const Sandbox &sandbox);

    quint16 inhibitOrbitSwitch(const QString &dbusService, const QString &reason);
    void releaseOrbitSwitchInhibition(const QString &dbusService, quint16 cookie);

    static void sendBackReply(const QDBusMessage &reply);

    void updateSystemdStatus();

    quint16 injectedToken;
};

}

#endif // GRAVITY_GRAVITYSTARSEQUENCE_P_H
