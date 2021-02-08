/*
 *
 */

#ifndef GRAVITYUSERMANAGER_H
#define GRAVITYUSERMANAGER_H

#include <HemeraCore/AsyncInitObject>

#include <QtCore/QStringList>

struct lu_context;

class GravityUserManager : public Hemera::AsyncInitObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GravityUserManager)

public:
    explicit GravityUserManager(const QStringList &files, bool create, bool debugSupport, QObject *parent = nullptr);
    virtual ~GravityUserManager();

    static void notifyErrorAndQuit(const QString &error);

protected:
    virtual void initImpl() Q_DECL_OVERRIDE Q_DECL_FINAL;

private Q_SLOTS:
    void compileNext();

private:
    QByteArray payload(const QString &file);

    // Libuser helpers
    bool createUser(qint32 uid, const QString& baseGroup, const QString& user, const QStringList& groups);
    bool deleteUser(const QString& user);
    qint32 createGroup(const QString &groupName, qint32 uid = -1);
    bool addToGroups(const QString &user, const QStringList &groups);
    bool removeFromGroups(const QString &user, const QStringList &groups);
    bool setGroups(const QString &user, const QStringList &groups);
    bool createTmpfilesEntry(const QString &user, const QString &homeDirectory);

    static bool replaceInFile(const QString &file, const QHash< QByteArray, QByteArray > &replacementHash);
    static bool replaceInFile(const QString &file, const QByteArray &toReplace, const QByteArray &replacement);

    // 0 means user exists.
    qint32 uidFor(const QString &username, qint32 baseUid, bool returnCorrectUid = false);

    struct lu_context *m_luctx;

    QStringList m_files;
    QHash< QString, QString > m_payloads;
    bool m_create;
    bool m_debugSupport;
};

#endif // GRAVITYUSERMANAGER_H
