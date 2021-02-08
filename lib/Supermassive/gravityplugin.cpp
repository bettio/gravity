#include "gravityplugin_p.h"

#include "gravitygalaxymanager.h"

namespace Gravity
{

Plugin::Plugin(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{

}

Plugin::~Plugin()
{
    delete d;
}

void Plugin::Private::setStatus(Status newStatus)
{
    Status oldStatus = status;
    status = newStatus;
    Q_EMIT q->statusChanged(oldStatus, newStatus);
}

void Plugin::setLoaded()
{
    d->setStatus(Loaded);
}

void Plugin::setUnloaded()
{
    d->setStatus(Unloaded);
}

Plugin::Status Plugin::status() const
{
    return d->status;
}

QString Plugin::name() const
{
    return d->name;
}

void Plugin::setName(const QString &name)
{
    d->name = name;
}

GalaxyManager *Plugin::galaxyManager()
{
    return d->applianceManager;
}

}
