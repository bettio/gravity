#ifndef GRAVITY_GRAVITYOPERATIONS_H
#define GRAVITY_GRAVITYOPERATIONS_H

#include <HemeraCore/AsyncInitDBusObject>
#include <HemeraCore/Operation>

#include <libudev.h>

#include <GravitySupermassive/Global>

class OrgFreedesktopSystemd1ManagerInterface;

namespace Gravity
{

class HEMERA_GRAVITY_EXPORT ControlUnitOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(ControlUnitOperation)

public:
    enum class Mode : quint8 {
        StartMode,
        StopMode,
        RestartMode
    };

    explicit ControlUnitOperation(const QString& unit, const QString& mode, Mode operationMode,
                                  OrgFreedesktopSystemd1ManagerInterface *manager = nullptr, QObject *parent = Q_NULLPTR);
    virtual ~ControlUnitOperation();

protected:
    virtual void startImpl();

private Q_SLOTS:
    void checkResult(uint id, const QDBusObjectPath &, const QString &, const QString &result);

private:
    class Private;
    Private * const d;
};

class HEMERA_GRAVITY_EXPORT ToolOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(ToolOperation)

public:
    explicit ToolOperation(const QString &path, const QStringList &args, QObject *parent = nullptr);
    virtual ~ToolOperation();

protected:
    virtual void startImpl() override final;

private:
    class Private;
    Private * const d;
};

class HEMERA_GRAVITY_EXPORT RestoreFactoryResetOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(RestoreFactoryResetOperation)

public:
    explicit RestoreFactoryResetOperation(const QString &configPath, const QString &sgdiskPath, const QString &sfdiskPath, QObject *parent = nullptr);
    virtual ~RestoreFactoryResetOperation();

    static QPair<QString, QString> diskNameAndPartitionFromLabel(const QString &label);

Q_SIGNALS:
    void deviceWasFactoryReset(const QString &devicePath);

protected:
    virtual void startImpl();

private:
    class Private;
    Private * const d;

    bool checkFactoryReset(const QString &devicePath, struct udev *udevContext);
};

}

#endif // GRAVITY_GRAVITYOPERATIONS_H
