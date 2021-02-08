#ifndef GRAVITYDBUSTYPES_H
#define GRAVITYDBUSTYPES_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusObjectPath>

void registerDBusMetaTypes();

class SessionObject
{
public:
    SessionObject();

    QString sessionId;
    uid_t userId;
    QString userName;
    QString seatId;
    QDBusObjectPath objectPath;
};

typedef QList<SessionObject> SessionObjectList;

Q_DECLARE_METATYPE(SessionObject)
Q_DECLARE_METATYPE(SessionObjectList)

QDBusArgument &operator<<(QDBusArgument &argument, const SessionObject &object);
const QDBusArgument &operator>>(const QDBusArgument &argument, SessionObject &object);

class UintPath
{
public:
    UintPath();

    quint32 userId;
    QDBusObjectPath objectPath;
};

Q_DECLARE_METATYPE(UintPath)

QDBusArgument &operator<<(QDBusArgument &argument, const UintPath &object);
const QDBusArgument &operator>>(const QDBusArgument &argument, UintPath &object);

class UintStringPath
{
public:
    UintStringPath();

    quint32 userId;
    QString userName;
    QDBusObjectPath objectPath;
};

typedef QList<UintStringPath> UintStringPathList;

Q_DECLARE_METATYPE(UintStringPath)
Q_DECLARE_METATYPE(UintStringPathList)

QDBusArgument &operator<<(QDBusArgument &argument, const UintStringPath &object);
const QDBusArgument &operator>>(const QDBusArgument &argument, UintStringPath &object);

class StringPath
{
public:
    StringPath();

    QString id;
    QDBusObjectPath objectPath;
};

typedef QList<StringPath> StringPathList;

Q_DECLARE_METATYPE(StringPath)
Q_DECLARE_METATYPE(StringPathList)

QDBusArgument &operator<<(QDBusArgument &argument, const StringPath &object);
const QDBusArgument &operator>>(const QDBusArgument &argument, StringPath &object);

#endif
