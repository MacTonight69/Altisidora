#include "MediaIntegration.h"

#include "../audio/PlayerController.h"
#include "../audio/PlaybackState.h"

#if defined(ALTISIDORA_HAS_DBUS)
#include "MprisMediaIntegration.h"
#else
#include "NullMediaIntegration.h"
#endif

MediaIntegration::MediaIntegration(PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
}

QString MediaIntegration::playbackStatus() const
{
    switch (m_player->playbackState()) {
    case PlaybackState::Playing:
        return QStringLiteral("Playing");
    case PlaybackState::Paused:
        return QStringLiteral("Paused");
    default:
        return QStringLiteral("Stopped");
    }
}

qint64 MediaIntegration::positionMicros() const
{
    return m_player->position() * 1000;
}

MediaIntegration *createMediaIntegration(PlayerController *player, QObject *parent)
{
#if defined(ALTISIDORA_HAS_DBUS)
    return new MprisMediaIntegration(player, parent);
#else
    return new NullMediaIntegration(player, parent);
#endif
}
