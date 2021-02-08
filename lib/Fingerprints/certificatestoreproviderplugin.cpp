#include "certificatestoreproviderplugin.h"

namespace Gravity {

CertificateStoreProviderPlugin::CertificateStoreProviderPlugin(QObject* parent)
    : QObject(parent)
    , d(nullptr)
{
}

CertificateStoreProviderPlugin::~CertificateStoreProviderPlugin()
{
}

}
