#pragma once

#include <QObject>
#include <QVariantMap>

class PlayerController;

/**
 * @brief Platform-neutral media-session integration.
 *
 * Owns the playback state a platform backend publishes to the desktop
 * (playback status, current track metadata, playback position) and the
 * start/stop lifecycle. The Linux backend registers it as an MPRIS v2 D-Bus
 * service (MprisMediaIntegration); other platforms can ship a no-op backend.
 */
class MediaIntegration : public QObject
{
    Q_OBJECT

public:
    explicit MediaIntegration(PlayerController *player, QObject *parent = nullptr);

    /**
     * @brief Register with the platform media service.
     * @return true when the integration is live, false if unavailable
     */
    virtual bool start() = 0;

    /**
     * @brief Unregister from the platform media service.
     */
    virtual void stop() = 0;

    // Data accessors used by the D-Bus adaptors.
    PlayerController *player() const { return m_player; }
    QString playbackStatus() const;
    QVariantMap metadata() const;
    qint64 positionMicros() const;

protected:
    PlayerController *m_player;
};

/**
 * @brief Construct the media integration for the current platform.
 *
 * The single construction point the application calls; the platform backend is
 * selected here rather than at the call site.
 */
MediaIntegration *createMediaIntegration(PlayerController *player, QObject *parent = nullptr);
