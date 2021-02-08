#ifndef GRAVITY_GRAVITYAPPLICATION_H
#define GRAVITY_GRAVITYAPPLICATION_H

#include <QtDBus/QDBusConnection>

#include <HemeraCore/AsyncInitObject>
#include <HemeraCore/Application>

#include <GravitySupermassive/Global>

namespace Gravity {

class HEMERA_GRAVITY_EXPORT Application : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Application)

public:
    explicit Application(const QString &id, bool isSatellite, const QDBusConnection &connection = QDBusConnection::sessionBus(), QObject* parent = 0);
    virtual ~Application();

    QString id() const;
    Hemera::Application::ApplicationStatus status() const;
    bool isSatellite() const;

public Q_SLOTS:
    Hemera::Operation *start();
    Hemera::Operation *stop();
    Hemera::Operation *quit();

protected Q_SLOTS:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

Q_SIGNALS:
    void applicationStatusChanged(Hemera::Application::ApplicationStatus status);

private:
    class Private;
    Private * const d;
};

}

#endif // GRAVITY_GRAVITYAPPLICATION_H
