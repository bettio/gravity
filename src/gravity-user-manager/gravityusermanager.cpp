/*
 *
 */

#include "gravityusermanager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtCore/QCryptographicHash>

#include <HemeraCore/Literals>

#include <iostream>

#include <libuser/entity.h>
#include <libuser/user.h>

#include <systemd/sd-daemon.h>

#include "gravityconfig.h"

#define ORBIT_USER_BASE_UID 100000
#define STAR_USER_BASE_UID 200000
#define SYSTEMD_SYSTEM_PATH "/lib/systemd/system/"
#define SYSTEMD_USER_PATH "/usr/lib/systemd/user/"


GravityUserManager::GravityUserManager(const QStringList& files, bool create, bool debugSupport, QObject *parent)
    : AsyncInitObject(parent)
    , m_luctx(NULL)
    , m_files(files)
    , m_create(create)
    , m_debugSupport(debugSupport)
{
}

GravityUserManager::~GravityUserManager()
{
    if (m_luctx) {
        lu_end(m_luctx);
    }
}

QByteArray GravityUserManager::payload(const QString &file)
{
    QFile fileContent(file);
    if (!fileContent.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    QByteArray result = fileContent.readAll();

    fileContent.close();

    return result;
}

void GravityUserManager::notifyErrorAndQuit(const QString& error)
{
    std::cout << " ERROR: " << error.toLatin1().constData() << std::endl;
    std::cout << "Compilation terminated." << std::endl;
    QCoreApplication::exit(EXIT_FAILURE);
}

qint32 GravityUserManager::uidFor(const QString &username, qint32 baseUid, bool returnCorrectUid)
{
    lu_ent *ent = lu_ent_new();
    struct lu_error *error = NULL;

    // First of all, let's check the user actually exists
    if (lu_user_lookup_name(m_luctx, username.toLatin1().constData(), ent, &error) == TRUE) {
        // The user exists indeed.
        qint32 uid;
        if (!returnCorrectUid) {
            // Return 0, which means update.
            uid = 0;
        } else {
            // Return correct uid.
            uid = lu_ent_get_first_id(ent, LU_UIDNUMBER);
        }
        lu_ent_free(ent);
        return uid;
    }

    // Ok, let's look for the first valid uid then.
    qint32 uid = baseUid;
    while (lu_user_lookup_id(m_luctx, uid, ent, &error) == TRUE) {
        ++uid;
    }

    lu_ent_free(ent);

    return uid;
}

void GravityUserManager::initImpl()
{
    std::cout << "Gravity User Manager version " << QCoreApplication::applicationVersion().toLatin1().constData()
              << " (C) 2013-2015 Ispirata" << std::endl << std::endl;

    connect(this, &GravityUserManager::ready, this, &GravityUserManager::compileNext);

    /* Initialize libuser. */
    struct lu_error *error = NULL;
    m_luctx = lu_start(NULL, (lu_entity_type)0, NULL, NULL, lu_prompt_console_quiet, NULL, &error);
    if (m_luctx == NULL) {
        setInitError(Hemera::Literals::literal(Hemera::Literals::Errors::failedRequest()),
                     QStringLiteral("Could not initialize libuser! %1").arg(QLatin1String(lu_strerror(error))));
        return;
    }

    // Verify each file exists and it's readable
    for (const QString &file : m_files) {
        QByteArray content = payload(file);
        if (content.isEmpty()) {
            setInitError(QString::fromLatin1("Could not open %1 for reading!").arg(file), QString());
            return;
        }

        m_payloads.insert(file, QLatin1String(content));
    }

    // K
    setReady();
}

bool GravityUserManager::createUser(qint32 uid, const QString &baseGroup, const QString &user, const QStringList &groups)
{
    bool isStar = baseGroup.contains(QStringLiteral("star-"));
    // check base group
    qint32 gidNumber = createGroup(baseGroup);
    if (gidNumber < 0) {
        return false;
    }

    QString userBaseDirPath = QStringLiteral("%1/%2").arg(QLatin1String(Gravity::StaticConfig::hemeraUserHomeDir()))
                                                        .arg(isStar ? QStringLiteral("stars") : QStringLiteral("orbits"));
    QDir userBaseDir(userBaseDirPath);
    if (!userBaseDir.exists()) {
        // Create the base path.
        userBaseDir.mkpath(userBaseDirPath);
    }
    QString homeDirectory = QStringLiteral("%1/%2").arg(userBaseDirPath).arg(user.split(QLatin1Char('-')).last());

    /* Create the user record. */
    lu_ent *ent = lu_ent_new();
    lu_user_default(m_luctx, user.toLatin1().constData(), 1, ent);

    /* Set UID. */
    lu_ent_set_id(ent, LU_UIDNUMBER, uid);

    /* Set GID. */
    lu_ent_set_id(ent, LU_GIDNUMBER, gidNumber);

    lu_ent_set_string(ent, LU_HOMEDIRECTORY, homeDirectory.toLatin1().constData());
    lu_ent_set_string(ent, LU_LOGINSHELL, "/bin/false");

    /* Moment-of-truth time. */
    struct lu_error *error = NULL;
    if (lu_user_add(m_luctx, ent, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("Account creation failed for %1: %2").arg(user, QLatin1String(lu_strerror(error))));
        return false;
    }

    if (lu_homedir_populate(m_luctx, NULL, homeDirectory.toLatin1().constData(),
                            uid, gidNumber, 0700, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("Error creating %1: %2").arg(homeDirectory, QLatin1String(lu_strerror(error))));
        return false;
    }

    lu_nscd_flush_cache(LU_NSCD_CACHE_PASSWD);
    lu_ent_free(ent);

    // Create tmpfiles.d entry in /etc (not from package) and Add to groups
    return createTmpfilesEntry(user, homeDirectory) && addToGroups(user, groups);
}

bool GravityUserManager::deleteUser(const QString& user)
{
    lu_ent *ent = lu_ent_new();
    struct lu_error *error = NULL;

    if (lu_user_lookup_name(m_luctx, user.toLatin1().constData(), ent, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("Trying to delete user %1, but it does not exist. %2").arg(user, QLatin1String(lu_strerror(error))));
        return false;
    }

    if (lu_user_delete(m_luctx, ent, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("User %1 deletion failed: %2").arg(user, QLatin1String(lu_strerror(error))));
        return false;
    }

    if (lu_homedir_remove_for_user(ent, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("Could not remove home directory for %1: %2").arg(user, QLatin1String(lu_strerror(error))));
        return false;
    }

    lu_ent_free(ent);

    // Kill tmpfiles.d entry
    QFile::remove(QStringLiteral("/etc/tmpfiles.d/%1.conf").arg(user));

    return true;
}

qint32 GravityUserManager::createGroup(const QString& groupName, qint32 uid)
{
    lu_ent *ent = lu_ent_new();
    struct lu_error *error = NULL;

    // Lookup if the group already exists
    if (lu_group_lookup_name(m_luctx, groupName.toLatin1().constData(), ent, &error) == TRUE) {
        // It does.
        qint32 id = lu_ent_get_first_id(ent, LU_GIDNUMBER);
        lu_ent_free(ent);
        return id;
    }

    /* Create a group entity object holding sensible defaults for a
     * new group. */
    lu_group_default(m_luctx, groupName.toLatin1().constData(), FALSE, ent);

    /* If the user specified a particular GID number, override the
     * default. */
    if (uid > 0) {
        lu_ent_set_id(ent, LU_GIDNUMBER, uid);
    }

    /* Try to create the group. */
    if (lu_group_add(m_luctx, ent, &error) == FALSE) {
        notifyErrorAndQuit(QStringLiteral("Could not create group %1: %2").arg(groupName, QLatin1String(lu_strerror(error))));
        return -1;
    }

    qint32 id = lu_ent_get_first_id(ent, LU_GIDNUMBER);

    lu_nscd_flush_cache(LU_NSCD_CACHE_GROUP);
    lu_ent_free(ent);

    return id;
}

static bool modifyGroup(struct lu_context *luctx, const QString& user, const QStringList& groups,
                        void (*modifyFunction) (struct lu_ent *ent, const char *attr, const GValue *value))
{
    struct lu_error *error = NULL;
    GValue val;
    lu_ent *ent = lu_ent_new();

    // Initialize G-String.
    memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_STRING);
    g_value_set_string(&val, user.toLatin1().constData());

    for (const QString &group : groups) {
        // Lookup the group
        if (lu_group_lookup_name(luctx, group.toLatin1().constData(), ent, &error) == FALSE) {
            return false;
        }

        modifyFunction(ent, LU_MEMBERNAME, &val);
        if (lu_group_modify(luctx, ent, &error) == FALSE) {
            GravityUserManager::notifyErrorAndQuit(QStringLiteral("Error modifying group members for %1: %2").arg(group, QLatin1String(lu_strerror(error))));
            return false;
        }
    }

    g_value_reset(&val);
    g_value_unset(&val);

    lu_nscd_flush_cache(LU_NSCD_CACHE_GROUP);
    lu_ent_free(ent);

    return true;
}

bool GravityUserManager::addToGroups(const QString& user, const QStringList& groups)
{
    return modifyGroup(m_luctx, user, groups, lu_ent_add);
}

bool GravityUserManager::removeFromGroups(const QString& user, const QStringList& groups)
{
    return modifyGroup(m_luctx, user, groups, lu_ent_del);
}

bool GravityUserManager::setGroups(const QString& user, const QStringList& groups)
{
    struct lu_error *error = NULL;
    QStringList groupsToAdd = groups;
    QStringList groupsToRemove;

    // Lookup groups for user.
    GValueArray *groupNames = lu_groups_enumerate_by_user(m_luctx, user.toLatin1().constData(), &error);
    if (error != NULL) {
        notifyErrorAndQuit(QStringLiteral("Trying to modify user %1, but it does not exist. %2").arg(user, QLatin1String(lu_strerror(error))));
        return false;
    }
    if (groupNames != NULL) {
        size_t i;

        for (i = 0; i < groupNames->n_values; i++) {
            GValue *value;

            value = g_value_array_get_nth(groupNames, i);
            QString groupName = QLatin1String(g_value_get_string(value));
            if (groups.contains(groupName)) {
                groupsToAdd.removeOne(groupName);
            } else {
                groupsToRemove.append(groupName);
            }
        }
        g_value_array_free(groupNames);
    }

    // Do
    bool result = false;
    result = addToGroups(user, groupsToAdd);
    if (!result) {
        return false;
    }
    result = removeFromGroups(user, groupsToRemove);

    return result;
}

bool GravityUserManager::replaceInFile(const QString& filename, const QHash< QByteArray, QByteArray >& replacementHash)
{
    if (!QFile::exists(filename)) {
        notifyErrorAndQuit(QStringLiteral("Trying to modify file %1, but it does not exist.").arg(filename));
        return false;
    }

    // Read payload
    QByteArray payload;
    {
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly)) {
            notifyErrorAndQuit(QStringLiteral("Could not open file %1 for modification.").arg(filename));
            return false;
        }

        payload = f.readAll();
        f.close();
    }

    // Replace in payload
    for (QHash< QByteArray, QByteArray >::const_iterator i = replacementHash.constBegin(); i != replacementHash.constEnd(); ++i) {
        payload.replace(i.key(), i.value());
    }

    // Write payload
    {
        QFile f(filename);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            notifyErrorAndQuit(QStringLiteral("Could not open file %1 for modification.").arg(filename));
            return false;
        }

        if (f.write(payload) != payload.size()) {
            notifyErrorAndQuit(QStringLiteral("Error writing to %1.").arg(filename));
            return false;
        }

        f.flush();
        f.close();
    }

    return true;
}


bool GravityUserManager::replaceInFile(const QString& filename, const QByteArray& toReplace, const QByteArray& replacement)
{
    QHash< QByteArray, QByteArray > rh;
    rh.insert(toReplace, replacement);
    return replaceInFile(filename, rh);
}

bool GravityUserManager::createTmpfilesEntry(const QString &user, const QString &homeDirectory)
{
    QFile tmpfileEntry(QStringLiteral("/etc/tmpfiles.d/%1.conf").arg(user));
    if (!tmpfileEntry.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        notifyErrorAndQuit(QStringLiteral("Could not create tmpfiles.d record for %1! Something is really wrong.").arg(user));
        return false;
    }

    tmpfileEntry.write(QStringLiteral("d %1 0700 %2 hemera-orbits -").arg(homeDirectory, user).toLatin1());
    tmpfileEntry.flush();

    return true;
}

void GravityUserManager::compileNext()
{
    if (m_payloads.isEmpty()) {
        // Yay!
        std::cout << std::endl << "All files successfully compiled." << std::endl;

        // Update systemd, if needed.
        if (sd_booted() > 0) {
            std::cout << "Service manager updated." << std::endl;
            QProcess::startDetached(QStringLiteral("/bin/systemctl --system daemon-reload"));
        }

        QCoreApplication::quit();
        return;
    }

    QString file = m_payloads.keys().first();
    QString content = m_payloads.take(file);

    std::cout << "\tCompiling " << file.split(QLatin1Char('/')).last().toLatin1().constData() << ":" << std::endl;

    QHash< QString, QPair< QString, QStringList > > starUsers;
    QHash< QString, QPair< QString, QStringList > > orbitUsers;

    auto normalizeUser = [] (const QString &userOrig) -> QString {
        QString user = userOrig;
        if (user.length() > 31) {
            // Hash it off
            int toRemove = userOrig.length() - 31 + 6;
            QStringList splitUser = user.split(QLatin1Char('-'));
            QString finalPart = splitUser.last();

            // Trim the first n characters.
            QString trimmed = finalPart.left(toRemove);
            QByteArray idBytes = QCryptographicHash::hash(trimmed.toLatin1(), QCryptographicHash::Sha256);
            QString checksum = QString::number(idBytes[0] | idBytes[1] << 8 | idBytes[2] << 16 | idBytes[3] << 24, 36).right(6);

            // Create!!
            user = QStringLiteral("%1-%2%3").arg(splitUser.first(), checksum, finalPart.right(finalPart.length() - toRemove));
        }
        return user;
    };

    // Analyze the file content
    for (const QString &line : content.split(QLatin1Char('\n'))) {
        if (line.isEmpty()) {
            continue;
        }

        QStringList pair = line.split(QLatin1Char(':'));
        if (pair.size() != 2) {
            // File malformed.
            notifyErrorAndQuit(QStringLiteral("File malformed! Not a key-value file."));
            return;
        }

        QStringList groups;
        // Create the split only if the string is meaningful, otherwise we'll end up with a corrupted list.
        if (!pair.last().isEmpty()) {
            groups = pair.last().split(QLatin1Char(','));
        }

        if (pair.first().startsWith(QStringLiteral("orbit-"))) {
            orbitUsers.insert(pair.first(), qMakePair(normalizeUser(pair.first()), groups));
        } else if (pair.first().startsWith(QStringLiteral("star-"))) {
            groups.append(QStringLiteral("hemera-stars"));
            starUsers.insert(pair.first(), qMakePair(normalizeUser(pair.first()), groups));
        } else {
            // File malformed.
            notifyErrorAndQuit(QStringLiteral("File malformed! It contains a user which is neither a star nor an orbit!"));
            return;
        }
    }

    auto doCreateUser = [this] (qint32 baseUid, const QString &baseGroup, const QString &user,
                                const QString &userOrig, const QStringList &groups) -> bool {
        quint32 uid = uidFor(user, baseUid);
        if (uid == 0) {
            // Update user
            std::cout << "\t\tUpdating user " << user.toLatin1().constData() << "...";
            std::cout.flush();
            QString userBaseDirPath = QStringLiteral("%1/%2").arg(QLatin1String(Gravity::StaticConfig::hemeraUserHomeDir()))
                                                             .arg(baseGroup.contains(QStringLiteral("star-")) ? QStringLiteral("stars") : QStringLiteral("orbits"));
            QString homeDirectory = QStringLiteral("%1/%2").arg(userBaseDirPath).arg(user.split(QLatin1Char('-')).last());
            if (!createTmpfilesEntry(user, homeDirectory)) {
                return false;
            }
            if (!setGroups(user, groups)) {
                return false;
            }
        } else if (uid > 0) {
            // Create user
            std::cout << "\t\tCreating user " << user.toLatin1().constData() << "...";
            std::cout.flush();

            if (!createUser(uid, baseGroup, user, groups)) {
                return false;
            }
        } else {
            notifyErrorAndQuit(QStringLiteral("Retrieving system user information failed! This is really, really wrong!"));
            return false;
        }

        // Update service files
        QString serviceFile;
        if (baseGroup == QStringLiteral("hemera-orbits")) {
            serviceFile = QString::fromLatin1("hemera-%1@.service").arg(userOrig);
        } else {
            serviceFile = QString::fromLatin1("hemera-%1.service").arg(userOrig);
        }

        if (!replaceInFile(QStringLiteral("%1%2").arg(QLatin1String(SYSTEMD_SYSTEM_PATH), serviceFile), "@USER@",
                           QString::number(uidFor(user, baseUid, true)).toLatin1())) {
            return false;
        }
        // Also, the policy files.
        if (!replaceInFile(QStringLiteral("%1policy-%2.conf").arg(QLatin1String("/etc/dbus-1/starbus.d/"), userOrig), "@USER@", user.toLatin1())) {
            return false;
        }
        // Is it a star?
        if (userOrig.startsWith(QStringLiteral("star-"))) {
            // Then fix up the allowance file as well.
            if (!replaceInFile(QStringLiteral("%1allow-%2.conf").arg(QLatin1String("/etc/dbus-1/system.d/"), userOrig), "@USER@", user.toLatin1())) {
                return false;
            }
        }

        // Shall we do this on the debug file as well?
        if (m_debugSupport) {
            QString serviceDebugFile;
            if (baseGroup == QStringLiteral("hemera-orbits")) {
                serviceDebugFile = QString::fromLatin1("hemera-%1_debug@.service").arg(userOrig);
            } else {
                serviceDebugFile = QString::fromLatin1("hemera-%1_debug.service").arg(userOrig);
            }

            if (!replaceInFile(QStringLiteral("%1%2").arg(QLatin1String(SYSTEMD_SYSTEM_PATH), serviceDebugFile), "@USER@",
                               QString::number(uidFor(user, baseUid, true)).toLatin1())) {
                return false;
            }
        }

        std::cout << " Success!" << std::endl;
        return true;
    };

    auto doDeleteUser = [this] (const QString &userOrig, const QString &user) -> bool {
        std::cout << "\t\tDeleting user " << user.toLatin1().constData() << "...";
        std::cout.flush();

        // Un-replace files
        QString serviceFile;
        if (user.startsWith(QStringLiteral("orbit-"))) {
            serviceFile = QString::fromLatin1("hemera-%1@.service").arg(userOrig);
        } else {
            serviceFile = QString::fromLatin1("hemera-%1.service").arg(userOrig);
        }

        if (!replaceInFile(QStringLiteral("%1%2").arg(QLatin1String(SYSTEMD_SYSTEM_PATH), serviceFile),
                           QString::number(uidFor(user, 0, true)).toLatin1(), "@USER@")) {
            return false;
        }
        // Also, the policy files.
        if (!replaceInFile(QStringLiteral("%1policy-%2.conf").arg(QLatin1String("/etc/dbus-1/starbus.d/"), userOrig), user.toLatin1(), "@USER@")) {
            return false;
        }
        // Is it a star?
        if (user.startsWith(QStringLiteral("star-"))) {
            // Then fix up the allowance file as well.
            if (!replaceInFile(QStringLiteral("%1allow-%2.conf").arg(QLatin1String("/etc/dbus-1/system.d/"), userOrig), user.toLatin1(), "@USER@")) {
                return false;
            }
        }
        // Shall we do this on the debug file as well?
        if (m_debugSupport) {
            QString serviceDebugFile;
            if (user.startsWith(QStringLiteral("orbit-"))) {
                serviceDebugFile = QString::fromLatin1("hemera-%1_debug@.service").arg(userOrig);
            } else {
                serviceDebugFile = QString::fromLatin1("hemera-%1_debug.service").arg(userOrig);
            }

            if (!replaceInFile(QStringLiteral("%1%2").arg(QLatin1String(SYSTEMD_SYSTEM_PATH), serviceDebugFile),
                               QString::number(uidFor(user, 0, true)).toLatin1(), "@USER@")) {
                return false;
            }
        }

        if (!deleteUser(user)) {
            return false;
        }

        std::cout << " Success!" << std::endl;
        return true;
    };

    for (QHash< QString, QPair< QString, QStringList > >::const_iterator i = starUsers.constBegin(); i != starUsers.constEnd(); ++i) {
        if (m_create) {
            // Create users
            if (!doCreateUser(STAR_USER_BASE_UID, i.value().first, i.value().first, i.key(), i.value().second)) {
                notifyErrorAndQuit(tr("Could not create user! Verify groups and user name are consistent."));
                return;
            }
        } else {
            if (!doDeleteUser(i.key(), i.value().first)) {
                notifyErrorAndQuit(tr("Could not delete user!"));
                return;
            }
        }
    }
    for (QHash< QString, QPair< QString, QStringList > >::const_iterator i = orbitUsers.constBegin(); i != orbitUsers.constEnd(); ++i) {
        if (m_create) {
            // Create users
            if (!doCreateUser(ORBIT_USER_BASE_UID, QStringLiteral("hemera-orbits"), i.value().first, i.key(), i.value().second)) {
                notifyErrorAndQuit(tr("Could not create user! Verify groups and user name are consistent."));
                return;
            }
        } else {
            if (!doDeleteUser(i.key(), i.value().first)) {
                notifyErrorAndQuit(tr("Could not delete user!"));
                return;
            }
        }
    }

    std::cout << "\tCompiled " << file.split(QLatin1Char('/')).last().toLatin1().constData() << " successfully." << std::endl;

    QTimer::singleShot(0, this, SLOT(compileNext()));
}

