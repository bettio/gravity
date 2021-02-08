#ifndef GRAVITY_GRAVITYPLUGIN_P_H
#define GRAVITY_GRAVITYPLUGIN_P_H

#include "gravityplugin.h"

namespace Gravity {

/**
 * @brief Interface for building GRAVITY Plugins
 */
class Plugin::Private
{
public:
    Private(Plugin *parent) : q(parent), status(Unloaded), applianceManager(Q_NULLPTR) {}

    void setStatus(Status newStatus);

    Plugin *q;
    Status status;
    QString name;

    GalaxyManager *applianceManager;
};

}

#endif
