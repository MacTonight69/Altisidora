#include "NullMediaIntegration.h"

NullMediaIntegration::NullMediaIntegration(PlayerController *player, QObject *parent)
    : MediaIntegration(player, parent)
{
}

bool NullMediaIntegration::start()
{
    // No platform media service exists here, which the base contract calls
    // "unavailable"; the only caller (main.cpp) ignores the value.
    return false;
}

void NullMediaIntegration::stop()
{
}

QVariantMap NullMediaIntegration::metadata() const
{
    return {};
}
