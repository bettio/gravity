#ifndef PARSECCORE_H
#define PARSECCORE_H

#include <HemeraCore/AsyncInitDBusObject>

namespace Gravity {
class ApplicationHandler;
}

class ParsecCore : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ParsecCore)
    Q_CLASSINFO("D-Bus Interface", "com.ispirata.Hemera.Parsec")

    Q_PROPERTY(QString activeOrbit READ activeOrbit NOTIFY activeOrbitChanged)
    Q_PROPERTY(bool isOrbitSwitchInhibited READ isOrbitSwitchInhibited NOTIFY inhibitionChanged)
    Q_PROPERTY(QVariantMap inhibitionReasons READ inhibitionReasons NOTIFY inhibitionChanged)
    Q_PROPERTY(uint phase READ phase NOTIFY phaseChanged)

public:
    explicit ParsecCore(QObject* parent = nullptr);
    virtual ~ParsecCore();

    Gravity::ApplicationHandler *applicationHandler() const;

    QString activeOrbit() const;
    bool isOrbitSwitchInhibited() const;
    QVariantMap inhibitionReasons() const;
    uint phase() const;

public Q_SLOTS:
    // DBus
    void registerApplication() const;
    void inhibitOrbitSwitch(const QString &reason);
    void releaseOrbitSwitchInhibition();

    bool AmIASatellite();

    void openURL(const QString &url, bool tryActivation);

    /// Those are here to please DBus
    void reloadCurrentOrbit();
    void requestOrbitSwitch(const QString &orbit);

protected:
    virtual void initImpl() override final;

private Q_SLOTS:
    void updateSystemdStatus();

Q_SIGNALS:
    void activeOrbitChanged();
    void inhibitionChanged();
    void phaseChanged();

private:
    class Private;
    Private * const d;
};

#endif // PARSECCORE_H
