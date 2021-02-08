/*
 *
 */

#ifndef GRAVITY_SANDBOX_H
#define GRAVITY_SANDBOX_H

#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>

#include <GravitySupermassive/Global>

namespace Gravity {

class SandboxData;

class HEMERA_GRAVITY_EXPORT Sandbox
{
public:
    /// Creates an invalid sandbox
    Sandbox();
    Sandbox(const Sandbox &other);
    explicit Sandbox(const QString &name, const QString &service);

    ~Sandbox();

    Sandbox &operator=(const Sandbox &rhs);
    bool operator==(const Sandbox &other) const;
    inline bool operator!=(const Sandbox &other) const { return !operator==(other); }

    bool isValid() const;

    QString name() const;
    QString service() const;

private:
    QSharedDataPointer<SandboxData> d;

};
}

#endif // GRAVITY_SANDBOX_H
