#include "MprisPlayer.h"

#include <QCoreApplication>
#include <QDBusError>
#include <QUrl>

#include "../audio/PlayerController.h"
#include "../audio/PlaybackState.h"

// ---- org.mpris.MediaPlayer2 ----

MediaPlayer2Adaptor::MediaPlayer2Adaptor(MprisPlayer *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void MediaPlayer2Adaptor::Quit()
{
    QCoreApplication::quit();
}

// ---- org.mpris.MediaPlayer2.Player ----

PlayerAdaptor::PlayerAdaptor(MprisPlayer *parent)
    : QDBusAbstractAdaptor(parent)
    , m_player(parent)
{
    PlayerController *player = m_player->player();

    connect(player, &PlayerController::stateChanged, this, [this]() {
        emit playbackStatusChanged();
        // The queue may have been replaced around the state change.
        emit canGoNextChanged();
        emit canGoPreviousChanged();
    });
    connect(player, &PlayerController::currentSongChanged, this, [this]() {
        emit metadataChanged();
        emit canGoNextChanged();
        emit canGoPreviousChanged();
    });
    connect(player, &PlayerController::positionChanged, this, [this]() {
        emit positionChanged();
    });
    connect(player, &PlayerController::volumeChanged, this, [this]() {
        emit volumeChanged();
    });
}

QString PlayerAdaptor::playbackStatus() const
{
    return m_player->playbackStatus();
}

QVariantMap PlayerAdaptor::metadata() const
{
    return m_player->metadata();
}

double PlayerAdaptor::volume() const
{
    return m_player->player()->volume();
}

void PlayerAdaptor::setVolume(double volume)
{
    m_player->player()->setVolume(static_cast<float>(volume));
}

qint64 PlayerAdaptor::position() const
{
    return m_player->positionMicros();
}

bool PlayerAdaptor::canGoNext() const
{
    return m_player->player()->canGoNext();
}

bool PlayerAdaptor::canGoPrevious() const
{
    return m_player->player()->canGoPrevious();
}

void PlayerAdaptor::Next()
{
    m_player->player()->next();
}

void PlayerAdaptor::Previous()
{
    m_player->player()->previous();
}

void PlayerAdaptor::Pause()
{
    m_player->player()->pause();
}

void PlayerAdaptor::PlayPause()
{
    PlayerController *player = m_player->player();
    if (player->isPlaying()) {
        player->pause();
    } else {
        player->play();
    }
}

void PlayerAdaptor::Stop()
{
    m_player->player()->stop();
}

void PlayerAdaptor::Play()
{
    m_player->player()->play();
}

void PlayerAdaptor::Seek(qint64 offset)
{
    PlayerController *player = m_player->player();
    // MPRIS offsets are microseconds; PlayerController works in milliseconds.
    player->seek(player->position() + offset / 1000);
}

void PlayerAdaptor::SetPosition(const QDBusObjectPath &trackId, qint64 position)
{
    Q_UNUSED(trackId)
    m_player->player()->seek(position / 1000);
}

void PlayerAdaptor::OpenUri(const QString &uri)
{
    Q_UNUSED(uri)
    // URI playback is not supported: the library manages local files only.
}

// ---- MprisPlayer ----

MprisPlayer::MprisPlayer(PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
}

QString MprisPlayer::playbackStatus() const
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

QVariantMap MprisPlayer::metadata() const
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

    return map;
}

qint64 MprisPlayer::positionMicros() const
{
    return m_player->position() * 1000;
}

bool MprisPlayer::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "MPRIS: no session bus available";
        return false;
    }

    const QString service = QStringLiteral("org.mpris.MediaPlayer2.musicrep");
    if (!bus.registerService(service)) {
        qWarning() << "MPRIS: cannot register service" << service
                   << bus.lastError().message();
        return false;
    }

    // The adaptors are children of this object; RegisterAdaptors exposes
    // each one as a distinct interface on /org/mpris/MediaPlayer2.
    new MediaPlayer2Adaptor(this);
    new PlayerAdaptor(this);
    if (!bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                            QDBusConnection::ExportAdaptors)) {
        qWarning() << "MPRIS: cannot register object /org/mpris/MediaPlayer2"
                   << bus.lastError().message();
        return false;
    }

    return true;
}