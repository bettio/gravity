#include "fingerprintproviderplugin.h"

namespace Gravity {

FingerprintProviderPlugin::FingerprintProviderPlugin(QObject* parent)
    : QObject(parent)
    , d(nullptr)
{
}

FingerprintProviderPlugin::~FingerprintProviderPlugin()
{
}

}
