#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

#include "PlaybackState.h"

class QMediaPlayer;
class QAudioOutput;

/**
 * @brief Application-level audio controller.
 *
 * Wraps QMediaPlayer and QAudioOutput to provide a clean, backend-agnostic
 * interface for playback control. This is the single interface the GUI
 * interacts with for all audio operations.
 *
 * State management:
 * - Stopped: No media loaded or playback stopped
 * - Loading: Media is being loaded/buffered
 * - Playing: Media is actively playing
 * - Paused: Playback is paused
 * - Error: An error occurred during playback
 */
class PlayerController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(PlaybackState::State playbackState READ playbackState NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap currentSong READ currentSong NOTIFY currentSongChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY stateChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    // Property getters
    PlaybackState::State playbackState() const;
    QVariantMap currentSong() const;
    qint64 position() const;
    qint64 duration() const;
    float volume() const;
    bool isPlaying() const;

public slots:
    // Playback control
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 position);
    void setVolume(float volume);

    // Media loading
    void load(const QUrl &url);
    /**
     * @brief Load a song keeping its library metadata.
     *
     * QML passes the song map from Library/Playlists so the UI can show
     * title, artist and highlight the current row without a DB lookup.
     */
    void loadSong(const QVariantMap &song);

signals:
    void stateChanged(PlaybackState::State newState);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void currentSongChanged(const QVariantMap &song);
    void volumeChanged(float volume);
    void errorOccurred(const QString &errorMessage);

private slots:
    void handlePlaybackStateChanged();
    void handlePositionChanged(qint64 position);
    void handleDurationChanged(qint64 duration);
    void handleError();
    void handleMediaStatusChanged();

private:
    void setPlaybackState(PlaybackState::State state);
    void updateCurrentSong();
    QString errorString() const;

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;

    PlaybackState::State m_playbackState;
    QVariantMap m_currentSong;
    qint64 m_position;
    qint64 m_duration;
    float m_volume;
};
