#include "MprisPlayer.h"

#include <QCoreApplication>
#include <QDBusError>
#include <QDBusMessage>
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
        QVariantMap changed;
        changed[QStringLiteral("PlaybackStatus")] = playbackStatus();
        changed[QStringLiteral("CanGoNext")] = canGoNext();
        changed[QStringLiteral("CanGoPrevious")] = canGoPrevious();
        emitPropertiesChanged(changed);
    });
    connect(player, &PlayerController::currentSongChanged, this, [this]() {
        emit metadataChanged();
        emit canGoNextChanged();
        emit canGoPreviousChanged();
        QVariantMap changed;
        changed[QStringLiteral("Metadata")] = metadata();
        changed[QStringLiteral("CanGoNext")] = canGoNext();
        changed[QStringLiteral("CanGoPrevious")] = canGoPrevious();
        emitPropertiesChanged(changed);
    });
    connect(player, &PlayerController::positionChanged, this, [this](qint64 positionMs) {
        // MPRIS clients poll the Position property; the raw positionChanged
        // D-Bus signal is non-standard and would fire ~6.7/s while playing
        // (the mpv poll runs every 150 ms). Coalesce it to one signal per
        // whole second so any listener still hears about seeks and the
        // advancing second without the constant D-Bus spam.
        const qint64 second = positionMs / 1000;
        if (second == m_lastPositionSecond) {
            return;
        }
        m_lastPositionSecond = second;
        emit positionChanged();
    });
    connect(player, &PlayerController::volumeChanged, this, [this]() {
        emit volumeChanged();
        QVariantMap changed;
        changed[QStringLiteral("Volume")] = volume();
        emitPropertiesChanged(changed);
    });
}

void PlayerAdaptor::emitPropertiesChanged(const QVariantMap &changed)
{
    // Qt's QDBusAbstractAdaptor relays the Q_PROPERTY NOTIFY signals as plain
    // (non-standard) D-Bus signals and never emits the standard
    // org.freedesktop.DBus.Properties.PropertiesChanged. Desktop shells
    // (Noctalia, KDE, GNOME) only listen to PropertiesChanged, so send it
    // explicitly whenever an MPRIS property changes.
    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    signal << QStringLiteral("org.mpris.MediaPlayer2.Player");
    signal << changed;
    signal << QStringList();
    QDBusConnection::sessionBus().send(signal);
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

    const QString service = QStringLiteral("org.mpris.MediaPlayer2.altisidora");
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