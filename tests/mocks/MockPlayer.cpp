#include "MockPlayer.h"

MockPlayer::MockPlayer(QObject *parent)
    : QObject(parent)
    , m_state(PlaybackState::Stopped)
    , m_position(0)
    , m_duration(0)
    , m_volume(1.0f)
    , m_playbackRate(1.0)
    , m_muted(false)
    , m_seekable(true)
    , m_hasMedia(false)
    , m_playCount(0)
    , m_pauseCount(0)
    , m_stopCount(0)
    , m_seekCount(0)
{
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(100);
    connect(m_positionTimer, &QTimer::timeout, this, [this]() {
        if (m_state == PlaybackState::Playing) {
            m_position += 100;
            if (m_position >= m_duration && m_duration > 0) {
                m_position = m_duration;
                simulateMediaEnded();
            }
            emit positionChanged(m_position);
        }
    });
}

MockPlayer::~MockPlayer() = default;

MockPlayer::PlaybackState MockPlayer::state() const
{
    return m_state;
}

QUrl MockPlayer::currentMedia() const
{
    return m_currentMedia;
}

qint64 MockPlayer::position() const
{
    return m_position;
}

qint64 MockPlayer::duration() const
{
    return m_duration;
}

float MockPlayer::volume() const
{
    return m_volume;
}

qreal MockPlayer::playbackRate() const
{
    return m_playbackRate;
}

bool MockPlayer::isMuted() const
{
    return m_muted;
}

bool MockPlayer::isSeekable() const
{
    return m_seekable;
}

bool MockPlayer::hasMedia() const
{
    return m_hasMedia;
}

QString MockPlayer::errorString() const
{
    return m_errorString;
}

void MockPlayer::setMedia(const QUrl &url)
{
    m_currentMedia = url;
    m_loadedMedia.append(url);
    m_hasMedia = true;
    m_position = 0;
    m_duration = 300000; // 5 minutes default
    m_state = PlaybackState::Loading;
    m_stateHistory.append(m_state);
    emit stateChanged(m_state);
    emit durationChanged(m_duration);
    emit mediaStatusChanged();

    // Simulate loading completion
    QTimer::singleShot(50, this, [this]() {
        if (m_state == PlaybackState::Loading) {
            m_state = PlaybackState::Stopped;
            m_stateHistory.append(m_state);
            emit stateChanged(m_state);
        }
    });
}

void MockPlayer::play()
{
    m_playCount++;
    if (!m_hasMedia) {
        simulateError(QStringLiteral("No media loaded"));
        return;
    }
    m_state = PlaybackState::Playing;
    m_stateHistory.append(m_state);
    m_positionTimer->start();
    emit stateChanged(m_state);
}

void MockPlayer::pause()
{
    m_pauseCount++;
    if (m_state == PlaybackState::Playing) {
        m_state = PlaybackState::Paused;
        m_stateHistory.append(m_state);
        m_positionTimer->stop();
        emit stateChanged(m_state);
    }
}

void MockPlayer::stop()
{
    m_stopCount++;
    m_state = PlaybackState::Stopped;
    m_stateHistory.append(m_state);
    m_positionTimer->stop();
    m_position = 0;
    emit stateChanged(m_state);
    emit positionChanged(m_position);
}

void MockPlayer::setPosition(qint64 position)
{
    m_seekCount++;
    if (m_seekable && m_hasMedia) {
        m_position = qBound(0LL, position, m_duration);
        emit positionChanged(m_position);
    }
}

void MockPlayer::setVolume(float volume)
{
    m_volume = qBound(0.0f, volume, 1.0f);
    emit volumeChanged(m_volume);
}

void MockPlayer::setPlaybackRate(qreal rate)
{
    m_playbackRate = qBound(0.5, rate, 2.0);
    emit playbackRateChanged(m_playbackRate);
}

void MockPlayer::setMuted(bool muted)
{
    m_muted = muted;
    emit mutedChanged(m_muted);
}

void MockPlayer::simulateError(const QString &error)
{
    m_errorString = error;
    m_state = PlaybackState::Error;
    m_stateHistory.append(m_state);
    m_positionTimer->stop();
    emit stateChanged(m_state);
    emit errorOccurred(error);
}

void MockPlayer::simulateMediaEnded()
{
    m_state = PlaybackState::Stopped;
    m_stateHistory.append(m_state);
    m_positionTimer->stop();
    m_position = 0;
    emit stateChanged(m_state);
    emit mediaEnded();
}

void MockPlayer::reset()
{
    m_state = PlaybackState::Stopped;
    m_currentMedia = QUrl();
    m_position = 0;
    m_duration = 0;
    m_volume = 1.0f;
    m_playbackRate = 1.0;
    m_muted = false;
    m_seekable = true;
    m_hasMedia = false;
    m_errorString.clear();
    m_playCount = 0;
    m_pauseCount = 0;
    m_stopCount = 0;
    m_seekCount = 0;
    m_loadedMedia.clear();
    m_stateHistory.clear();
    m_positionTimer->stop();
}

int MockPlayer::playCount() const
{
    return m_playCount;
}

int MockPlayer::pauseCount() const
{
    return m_pauseCount;
}

int MockPlayer::stopCount() const
{
    return m_stopCount;
}

int MockPlayer::seekCount() const
{
    return m_seekCount;
}

QList<QUrl> MockPlayer::loadedMedia() const
{
    return m_loadedMedia;
}

QList<MockPlayer::PlaybackState> MockPlayer::stateHistory() const
{
    return m_stateHistory;
}
