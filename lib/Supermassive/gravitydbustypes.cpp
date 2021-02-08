#include <QtDBus/QDBusMetaType>

#include "gravitydbustypes_p.h"

void registerDBusMetaTypes()
{
    qRegisterMetaType<SessionObjectList>("SessionObjectList");
    qRegisterMetaType<UintPath>("UintPath");
    qRegisterMetaType<UintStringPath>("UintStringPath");
    qRegisterMetaType<StringPath>("StringPath");
    qRegisterMetaType<StringPathList>("StringPathList");

    qDBusRegisterMetaType<SessionObject>();
    qDBusRegisterMetaType<SessionObjectList>();
    qDBusRegisterMetaType<UintPath>();
    qDBusRegisterMetaType<UintStringPath>();
    qDBusRegisterMetaType<StringPath>();
    qDBusRegisterMetaType<StringPathList>();
}

SessionObject::SessionObject()
{
}

UintPath::UintPath()
{
}

UintStringPath::UintStringPath()
{
}

StringPath::StringPath()
{
}

QDBusArgument &operator<<(QDBusArgument &argument, const SessionObject &object)
{
    argument.beginStructure();
    argument << object.sessionId;
    argument << object.userId;
    argument << object.userName;
    argument << object.seatId;
    argument << object.objectPath;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SessionObject &object)
{
    argument.beginStructure();
    argument >> object.sessionId;
    argument >> object.userId;
    argument >> object.userName;
    argument >> object.seatId;
    argument >> object.objectPath;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const UintPath &object)
{
    argument.beginStructure();
    argument << object.userId;
    argument << object.objectPath;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, UintPath &object)
{
    argument.beginStructure();
    argument >> object.userId;
    argument >> object.objectPath;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const UintStringPath &object)
{
    argument.beginStructure();
    argument << object.userId;
    argument << object.userName;
    argument << object.objectPath;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, UintStringPath &object)
{
    argument.beginStructure();
    argument >> object.userId;
    argument >> object.userName;
    argument >> object.objectPath;
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const StringPath &object)
{
    argument.beginStructure();
    argument << object.id;
    argument << object.objectPath;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, StringPath &object)
{
    argument.beginStructure();
    argument >> object.id;
    argument >> object.objectPath;
    argument.endStructure();

    return argument;
}
