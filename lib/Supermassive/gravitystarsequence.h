#ifndef GRAVITY_GRAVITYSTARSEQUENCE_H
#define GRAVITY_GRAVITYSTARSEQUENCE_H

#include <HemeraCore/AsyncInitDBusObject>
#include <HemeraCore/Operation>

#include <GravitySupermassive/Global>
#include <GravitySupermassive/Sandbox>

class QQmlEngine;
namespace Gravity {

typedef Hemera::Operation* (*ReloadHook)(QObject *self);

inline Hemera::Operation *nullReloadHook(QObject *self) {Q_UNUSED(self); return Q_NULLPTR;};

class HEMERA_GRAVITY_EXPORT StarSequence : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(StarSequence)

    Q_PROPERTY(QString activeOrbit READ activeOrbit NOTIFY activeOrbitChanged)
    Q_PROPERTY(bool isOrbitSwitchInhibited READ isOrbitSwitchInhibited NOTIFY isOrbitSwitchInhibitedChanged)
    Q_PROPERTY(QVariantMap inhibitionReasons READ inhibitionReasons NOTIFY inhibitionReasonsChanged)
    Q_PROPERTY(uint phase READ phaseAsUInt NOTIFY phaseChanged)

public:
    enum class Phase {
        Unknown = 0,
        Nebula = 1,
        MainSequence = 2,
        Injected = 3,
        Collapse = 99
    };
    Q_ENUMS(Phase)

    explicit StarSequence(const QString &star, const QString &activeOrbit, const QString &residentOrbit, QObject *parent = 0);
    virtual ~StarSequence();

    QDBusObjectPath busPath() const;

    QString activeOrbit() const;
    QString residentOrbit() const;
    bool isOrbitSwitchInhibited() const;
    bool hasInjectedOrbit() const;

    Phase phase() const;
    inline uint phaseAsUInt() const { return static_cast<uint>(phase()); }

    QString star() const;
    QVariantMap inhibitionReasons() const;

    Hemera::Operation *reloadCurrentOrbit(ReloadHook hook, QObject *self);

    Hemera::Operation *injectOrbit(const QString &name);
    Hemera::Operation *deinjectOrbit();

    void setShouldUpdateSystemd(bool update);

public Q_SLOTS:
    void Ignite();

    quint16 inhibitOrbitSwitch(const QString &requesterName, const QString &reason);
    void releaseOrbitSwitchInhibition(quint16 cookie);

    /// Those are here to please DBus
    void requestOrbitSwitch(const QString &orbit);
    void reloadCurrentOrbit();

    void Collapse();

protected Q_SLOTS:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void activeOrbitChanged(const QString &orbit);
    void isOrbitSwitchInhibitedChanged(bool isInhibited);
    void inhibitionReasonsChanged(const QVariantMap &inhibitionReasons);
    void phaseChanged();

    void readyForShutdown();

private:
    class Private;
    Private * const d;

    friend class ControlUnitOperation;
    friend class OrbitReloadOperation;
    friend class OrbitSwitchOperation;
    // Allow developer mode plugin to inject orbits
    friend class DeveloperModePlugin;
};

}

Q_DECLARE_METATYPE(Gravity::StarSequence::Phase)

#endif // GRAVITY_GRAVITYSTARSEQUENCE_H
