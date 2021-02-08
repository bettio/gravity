#ifndef GRAVITYREMOUNTHELPERCORE_H
#define GRAVITYREMOUNTHELPERCORE_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

#include <HemeraCore/AsyncInitDBusObject>
#include <HemeraCore/Operation>

namespace Gravity {
class RemountHelperPlugin;
}

class RemountFsOperation : public Hemera::Operation
{
    Q_OBJECT
    Q_DISABLE_COPY(RemountFsOperation)

public:
    explicit RemountFsOperation(const QString &mode, const QString &fs, QObject *parent = 0);
    virtual ~RemountFsOperation();

protected Q_SLOTS:
    virtual void startImpl() override final;
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QString m_mode;
    QString m_fs;
    QProcess *m_process;
};

class GravityRemountHelperCore : public Hemera::AsyncInitDBusObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GravityRemountHelperCore)

public:
    explicit GravityRemountHelperCore(const QString &mode, bool hooksOnly, QObject *parent = 0);
    virtual ~GravityRemountHelperCore();

protected Q_SLOTS:
    virtual void initImpl() override final;

private Q_SLOTS:
    void remount();
    void remountNext();
    void rollbackRemount();
    void executePreHooks();
    void executePostHooks();

private:
    QString m_mode;
    bool m_hooksOnly;
    QStringList m_filesystemsToRemount;
    QStringList m_remountedFilesystems;
    QList<Gravity::RemountHelperPlugin*> m_hooks;
};

#endif // GRAVITYREMOUNTHELPERCORE_H
