/*
 *
 */

#include "gravitysandbox.h"

#include <QtCore/QSharedData>

namespace Gravity
{

class SandboxData : public QSharedData
{
public:
    SandboxData(const QString& name, const QString& service) : name(name), service(service) {}

    QString name;
    QString service;
};

Sandbox::Sandbox()
    : Sandbox(QString(), QString())
{

}

Sandbox::Sandbox(const QString& name, const QString& service)
    : d(new SandboxData(name, service))
{
}

Sandbox::Sandbox(const Sandbox& other)
    : d(other.d)
{
}

Sandbox::~Sandbox()
{
}

Sandbox& Sandbox::operator=(const Sandbox& rhs)
{
    if (this == &rhs) {
        // Protect against self-assignment
        return *this;
    }

    d = rhs.d;
    return *this;
}

bool Sandbox::operator==(const Sandbox& other) const
{
    return d->service == other.d->service;
}

bool Sandbox::isValid() const
{
    return !d->name.isEmpty() && !d->service.isEmpty();
}

QString Sandbox::name() const
{
    return d->name;
}

QString Sandbox::service() const
{
    return d->service;
}

}
