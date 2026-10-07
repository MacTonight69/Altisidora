#pragma once

#include <QObject>
#include <QUrl>
#include <QTimer>

/**
 * @brief Mock audio player for testing
 *
 * Simulates audio playback behavior without requiring actual audio hardware.
 * Useful for testing player controllers and playback workflows.
 */
class MockPlayer : public QObject
{
    Q_OBJECT

public:
    enum class PlaybackState {
        Stopped,
        Loading,
        Playing,
        Paused,
        Error
    };
    Q_ENUM(PlaybackState)

    explicit MockPlayer(QObject *parent = nullptr);
    ~MockPlayer() override;

    /**
     * @brief Get the current playback state
     * @return Current playback state
     */
    PlaybackState state() const;

    /**
     * @brief Get the current media URL
     @return Current media URL
     */
    QUrl currentMedia() const;

    /**
     * @brief Get the current position in milliseconds
     * @return Current position
     */
    qint64 position() const;

    /**
     * @brief Get the total duration in milliseconds
     * @return Total duration
     */
    qint64 duration() const;

    /**
     * @brief Get the current volume (0.0 to 1.0)
     * @return Current volume
     */
    float volume() const;

    /**
     * @brief Get the current playback rate
     * @return Playback rate
     */
    qreal playbackRate() const;

    /**
     * @brief Check if the player is muted
     * @return true if muted
     */
    bool isMuted() const;

    /**
     * @brief Check if the player is seekable
     * @return true if seekable
     */
    bool isSeekable() const;

    /**
     * @brief Check if media is available
     * @return true if media is loaded
     */
    bool hasMedia() const;

    /**
     * @brief Get the error string if in error state
     * @return Error string
     */
    QString errorString() const;

    /**
     * @brief Simulate loading a media file
     * @param url Media URL to load
     */
    void setMedia(const QUrl &url);

    /**
     * @brief Simulate starting playback
     */
    void play();

    /**
     * @brief Simulate pausing playback
     */
    void pause();

    /**
     * @brief Simulate stopping playback
     */
    void stop();

    /**
     * @brief Simulate seeking to a position
     * @param position Position in milliseconds
     */
    void setPosition(qint64 position);

    /**
     * @brief Simulate setting volume
     * @param volume Volume level (0.0 to 1.0)
     */
    void setVolume(float volume);

    /**
     * @brief Simulate setting playback rate
     * @param rate Playback rate
     */
    void setPlaybackRate(qreal rate);

    /**
     * @brief Simulate muting/unmuting
     * @param muted true to mute, false to unmute
     */
    void setMuted(bool muted);

    /**
     * @brief Simulate a playback error
     * @param error Error message
     */
    void simulateError(const QString &error);

    /**
     * @brief Simulate reaching the end of media
     */
    void simulateMediaEnded();

    /**
     * @brief Reset the player to initial state
     */
    void reset();

    /**
     * @brief Get the number of times play() was called
     * @return Play call count
     */
    int playCount() const;

    /**
     * @brief Get the number of times pause() was called
     * @return Pause call count
     */
    int pauseCount() const;

    /**
     * @brief Get the number of times stop() was called
     * @return Stop call count
     */
    int stopCount() const;

    /**
     * @brief Get the number of times seek was called
     * @return Seek call count
     */
    int seekCount() const;

    /**
     * @brief Get the list of media URLs that were loaded
     * @return List of loaded media URLs
     */
    QList<QUrl> loadedMedia() const;

    /**
     * @brief Get the list of all state transitions
     * @return List of state transitions
     */
    QList<PlaybackState> stateHistory() const;

signals:
    void stateChanged(PlaybackState newState);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(float volume);
    void mutedChanged(bool muted);
    void playbackRateChanged(qreal rate);
    void mediaStatusChanged();
    void errorOccurred(const QString &error);
    void mediaEnded();

private:
    PlaybackState m_state;
    QUrl m_currentMedia;
    qint64 m_position;
    qint64 m_duration;
    float m_volume;
    qreal m_playbackRate;
    bool m_muted;
    bool m_seekable;
    bool m_hasMedia;
    QString m_errorString;

    int m_playCount;
    int m_pauseCount;
    int m_stopCount;
    int m_seekCount;

    QList<QUrl> m_loadedMedia;
    QList<PlaybackState> m_stateHistory;

    QTimer *m_positionTimer;
};
