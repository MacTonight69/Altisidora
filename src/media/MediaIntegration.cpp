#include "MediaIntegration.h"

#include <QDBusObjectPath>
#include <QUrl>

#include "../audio/PlayerController.h"
#include "../audio/PlaybackState.h"

#include "MprisMediaIntegration.h"

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

QVariantMap MediaIntegration::metadata() const
{
    QVariantMap map;
    const QVariantMap song = m_player->currentSong();
    if (song.isEmpty()) {
        return map;
    }

    map[QStringLiteral("mpris:trackid")] =
        QVariant::fromValue(QDBusObjectPath(
            QStringLiteral("/org/mpris/MediaPlayer2/TrackList/Track/%1")
                .arg(song.value(QStringLiteral("id")).toLongLong())));

    map[QStringLiteral("xesam:title")] =
        song.value(QStringLiteral("title")).toString();

    const QString artist = song.value(QStringLiteral("artist")).toString();
    if (!artist.isEmpty()) {
        map[QStringLiteral("xesam:artist")] = QStringList{artist};
    }

    const QString album = song.value(QStringLiteral("album")).toString();
    if (!album.isEmpty()) {
        map[QStringLiteral("xesam:album")] = album;
    }

    const qint64 durationMs = song.value(QStringLiteral("duration")).toLongLong();
    if (durationMs > 0) {
        map[QStringLiteral("mpris:length")] = durationMs * 1000; // µs
    }

    const QString path = song.value(QStringLiteral("path")).toString();
    if (!path.isEmpty()) {
        map[QStringLiteral("xesam:url")] = QUrl::fromLocalFile(path).toString();
    }

    const QString artPath = song.value(QStringLiteral("artwork")).toString();
    if (!artPath.isEmpty()) {
        map[QStringLiteral("mpris:artUrl")] = QUrl::fromLocalFile(artPath).toString();
    }

    return map;
}

qint64 MediaIntegration::positionMicros() const
{
    return m_player->position() * 1000;
}

MediaIntegration *createMediaIntegration(PlayerController *player, QObject *parent)
{
    return new MprisMediaIntegration(player, parent);
}
