#include "gravitysandboxmanager.h"

#include "gravitysandbox.h"

#include <QtCore/QDir>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QSettings>

#include "gravityconfig.h"

namespace Gravity
{

class SandboxManager::Private
{
public:
    QStringList activeSandboxes;
    QStringList runningSandboxes;
};

SandboxManager::SandboxManager(QObject* parent)
    : Hemera::AsyncInitDBusObject(parent)
    , d(new Private)
{
}

SandboxManager::~SandboxManager()
{
}

void SandboxManager::initImpl()
{
    setReady();
}

QStringList SandboxManager::activeSandboxes() const
{
    return d->activeSandboxes;
}

QStringList SandboxManager::runningSandboxes() const
{
    return d->runningSandboxes;
}

}
