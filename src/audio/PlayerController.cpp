#include "PlayerController.h"

#include <QMediaPlayer>
#include <QAudioOutput>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_playbackState(PlaybackState::Stopped)
    , m_position(0)
    , m_duration(0)
    , m_volume(1.0f)
{
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(m_volume);

    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &PlayerController::handlePlaybackStateChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &PlayerController::handlePositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &PlayerController::handleDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &PlayerController::handleError);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &PlayerController::handleMediaStatusChanged);
}

PlayerController::~PlayerController() = default;

PlaybackState::State PlayerController::playbackState() const
{
    return m_playbackState;
}

QVariantMap PlayerController::currentSong() const
{
    return m_currentSong;
}

qint64 PlayerController::position() const
{
    return m_position;
}

qint64 PlayerController::duration() const
{
    return m_duration;
}

float PlayerController::volume() const
{
    return m_volume;
}

bool PlayerController::isPlaying() const
{
    return m_playbackState == PlaybackState::Playing;
}

void PlayerController::play()
{
    if (m_playbackState == PlaybackState::Error) {
        return;
    }
    m_player->play();
}

void PlayerController::pause()
{
    m_player->pause();
}

void PlayerController::stop()
{
    m_player->stop();
}

void PlayerController::next()
{
    if (!canGoNext()) {
        return;
    }
    // Keep playing if playback was in progress, otherwise just load.
    const bool wasPlaying = (m_playbackState == PlaybackState::Playing);
    loadSong(m_queue.at(++m_queueIndex));
    if (wasPlaying) {
        m_player->play();
    }
}

void PlayerController::previous()
{
    if (!canGoPrevious()) {
        return;
    }
    const bool wasPlaying = (m_playbackState == PlaybackState::Playing);
    loadSong(m_queue.at(--m_queueIndex));
    if (wasPlaying) {
        m_player->play();
    }
}

bool PlayerController::canGoNext() const
{
    return m_queueIndex >= 0 && m_queueIndex + 1 < m_queue.size();
}

bool PlayerController::canGoPrevious() const
{
    return m_queueIndex > 0;
}

void PlayerController::seek(qint64 position)
{
    if (position < 0 || position > m_duration) {
        return;
    }
    m_player->setPosition(position);
}

void PlayerController::setVolume(float volume)
{
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }

    if (qFuzzyCompare(m_volume, volume)) {
        return;
    }

    m_volume = volume;
    m_audioOutput->setVolume(m_volume);
    emit volumeChanged(m_volume);
}

void PlayerController::load(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Invalid media URL"));
        return;
    }

    // Leave the Error state before the backend starts loading so an
    // immediate play() after load() is not rejected.
    setPlaybackState(PlaybackState::Loading);
    m_player->setSource(url);
    updateCurrentSong();
}

void PlayerController::loadSong(const QVariantMap &song)
{
    const QString path = song.value(QStringLiteral("path")).toString();
    if (path.isEmpty()) {
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Invalid media URL"));
        return;
    }

    load(QUrl::fromLocalFile(path));

    // load() resets currentSong to the bare URL; restore the full
    // metadata map so the UI can display the song details.
    m_currentSong = song;

    // Locate the song inside the queue so next()/previous() can navigate.
    m_queueIndex = -1;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).value(QStringLiteral("path")).toString() == path) {
            m_queueIndex = i;
            break;
        }
    }

    emit currentSongChanged(m_currentSong);
}

void PlayerController::setQueue(const QVariantList &songs)
{
    m_queue.clear();
    m_queue.reserve(songs.size());
    for (const QVariant &entry : songs) {
        m_queue.append(entry.toMap());
    }
    m_queueIndex = -1;
}

void PlayerController::handlePlaybackStateChanged()
{
    switch (m_player->playbackState()) {
    case QMediaPlayer::StoppedState:
        setPlaybackState(PlaybackState::Stopped);
        break;
    case QMediaPlayer::PlayingState:
        setPlaybackState(PlaybackState::Playing);
        break;
    case QMediaPlayer::PausedState:
        setPlaybackState(PlaybackState::Paused);
        break;
    }
}

void PlayerController::handlePositionChanged(qint64 position)
{
    if (m_position == position) {
        return;
    }
    m_position = position;
    emit positionChanged(m_position);
}

void PlayerController::handleDurationChanged(qint64 duration)
{
    if (m_duration == duration) {
        return;
    }
    m_duration = duration;
    emit durationChanged(m_duration);
}

void PlayerController::handleError()
{
    setPlaybackState(PlaybackState::Error);
    emit errorOccurred(errorString());
}

void PlayerController::handleMediaStatusChanged()
{
    switch (m_player->mediaStatus()) {
    case QMediaPlayer::LoadingMedia:
        setPlaybackState(PlaybackState::Loading);
        break;
    case QMediaPlayer::LoadedMedia:
        // State will be updated by playbackStateChanged
        break;
    case QMediaPlayer::InvalidMedia:
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Invalid or unsupported media format"));
        break;
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        // These states don't change our playback state
        break;
    case QMediaPlayer::EndOfMedia:
        // Advance the queue automatically, continuing playback.
        if (canGoNext()) {
            next();
            m_player->play();
        }
        break;
    }
}

void PlayerController::setPlaybackState(PlaybackState::State state)
{
    if (m_playbackState == state) {
        return;
    }
    m_playbackState = state;
    emit stateChanged(m_playbackState);
}

void PlayerController::updateCurrentSong()
{
    m_currentSong.clear();
    m_currentSong[QStringLiteral("path")] = m_player->source().toString();
    emit currentSongChanged(m_currentSong);
}

QString PlayerController::errorString() const
{
    switch (m_player->error()) {
    case QMediaPlayer::NoError:
        return tr("No error");
    case QMediaPlayer::ResourceError:
        return tr("Resource error: unable to access media");
    case QMediaPlayer::FormatError:
        return tr("Format error: unsupported media format");
    case QMediaPlayer::NetworkError:
        return tr("Network error: unable to fetch media");
    case QMediaPlayer::AccessDeniedError:
        return tr("Access denied: insufficient permissions");
    // QMediaPlayer::ServiceMissingError does not exist in this Qt version
    default:
        return tr("Unknown playback error");
    }
}
