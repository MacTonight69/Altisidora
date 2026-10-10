#pragma once

#include <QObject>
#include <QMap>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <array>
#include <optional>

#include "PlaybackState.h"

struct mpv_handle;
class QTimer;

/**
 * @brief Application-level audio controller.
 *
 * Wraps a libmpv playback session (mpv_handle) behind a backend-agnostic
 * interface for playback control. This is the single interface the GUI
 * interacts with for all audio operations. In addition to the transport,
 * it owns the 10-band graphic equalizer implemented as a runtime-adjustable
 * mpv audio filter chain.
 *
 * State management:
 * - Stopped: No media loaded or playback stopped
 * - Loading: Media is being loaded/buffered
 * - Playing: Media is actively playing
 * - Paused: Playback is paused
 * - Error: An error occurred during playback
 *
 * Backend notes:
 * - The mpv session is created lazily on the first load() so headless unit
 *   tests that never load media never touch the backend.
 * - A 150 ms timer polls mpv properties (time-pos, duration, pause,
 *   idle-active) and drains its event queue (START_FILE/FILE_LOADED/END_FILE).
 * - The equalizer applies the mpv `af` property as a chain of ten lavfi
 *   `equalizer` peaking filters; it can be changed while playing.
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
    // Repeat-one: loops the current song when it reaches the end.
    Q_PROPERTY(bool repeatOne READ repeatOne WRITE setRepeatOne NOTIFY repeatOneChanged)

    // Equalizer
    Q_PROPERTY(bool eqEnabled READ eqEnabled WRITE setEqEnabled NOTIFY eqChanged)
    Q_PROPERTY(QVariantList eqGains READ eqGains NOTIFY eqChanged)
    Q_PROPERTY(QStringList eqPresets READ eqPresets NOTIFY eqChanged)
    Q_PROPERTY(QString eqPreset READ eqPreset WRITE setEqPreset NOTIFY eqChanged)

public:
    static constexpr int kEqBandCount = 10;
    static constexpr double kEqMinDb = -12.0;
    static constexpr double kEqMaxDb = 12.0;

    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    // Property getters
    PlaybackState::State playbackState() const;
    QVariantMap currentSong() const;
    qint64 position() const;
    qint64 duration() const;
    float volume() const;
    bool isPlaying() const;
    bool repeatOne() const;

    // Equalizer getters
    bool eqEnabled() const;
    QVariantList eqGains() const;
    QStringList eqPresets() const;
    QString eqPreset() const;

    /**
     * @brief Build the mpv `af` value implementing a 10-band equalizer.
     *
     * Pure function (no mpv required) so it can be unit-tested. Returns an
     * empty string when every band is 0 dB (no filter needed).
     */
    static QString buildEqFilterString(const std::array<double, kEqBandCount> &gains);

public slots:
    // Playback control
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek(qint64 position);
    void setVolume(float volume);
    void setRepeatOne(bool enabled);

    // Media loading
    /**
     * @brief Load a song keeping its library metadata.
     *
     * QML passes the song map from Library/Playlists so the UI can show
     * title, artist and highlight the current row without a DB lookup.
     */
    void loadSong(const QVariantMap &song);
    /**
     * @brief Set the context queue used by next()/previous().
     *
     * QML passes the visible song list (e.g. the songs view model) so the
     * transport controls can navigate within it. loadSong() locates the
     * loaded song inside the queue by path.
     */
    void setQueue(const QVariantList &songs);

    // Queue state
    bool canGoNext() const;
    bool canGoPrevious() const;

    // Equalizer
    Q_INVOKABLE double eqGain(int index) const;
    Q_INVOKABLE void setEqGain(int index, double valueDb);
    Q_INVOKABLE void resetEq();
    /**
     * @brief Store the current curve as a user preset.
     *
     * @return false when the name is empty or collides with a built-in
     * preset; true otherwise. Saving over an existing user preset updates it.
     */
    Q_INVOKABLE bool saveEqPreset(const QString &name);
    /**
     * @brief Remove a user preset. Built-in presets are never removed.
     */
    Q_INVOKABLE void deleteEqPreset(const QString &name);
    /**
     * @brief Whether the name refers to one of the built-in presets.
     */
    Q_INVOKABLE bool isBuiltInEqPreset(const QString &name) const;
    void setEqEnabled(bool enabled);
    void setEqPreset(const QString &name);

signals:
    void stateChanged(PlaybackState::State newState);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void currentSongChanged(const QVariantMap &song);
    void volumeChanged(float volume);
    void errorOccurred(const QString &errorMessage);
    void repeatOneChanged(bool repeatOne);
    void eqChanged();

private slots:
    void pollTick();

private:
    // Backend helpers
    void load(const QUrl &url);
    bool ensureMpv();
    void applyVolume();
    void applyEq();
    void drainMpvEvents();
    void handleEndOfMedia();
    void sendCommand(const QByteArray &a1, const QByteArray &a2 = QByteArray(),
                     const QByteArray &a3 = QByteArray());

    // State helpers
    void setPlaybackState(PlaybackState::State state);
    void updateCurrentSong(const QString &path);

    // Equalizer helpers
    void loadEqSettings();
    void saveEqSettings();
    static std::optional<std::array<double, kEqBandCount>> presetGains(const QString &name);
    /**
     * @brief Find a preset (built-in first, then user) and copy its gains.
     */
    bool loadPreset(const QString &name, std::array<double, kEqBandCount> &out) const;
    /**
     * @brief Best preset name that exactly matches a curve (built-in first,
     * then user), or an empty string for a manual curve.
     */
    QString presetNameFor(const std::array<double, kEqBandCount> &gains) const;

    mpv_handle *m_mpv = nullptr;
    QTimer *m_pollTimer = nullptr;
    bool m_pendingLoad = false;

    PlaybackState::State m_playbackState;
    QVariantMap m_currentSong;
    qint64 m_position;
    qint64 m_duration;
    float m_volume;
    bool m_repeatOne = false;

    // Context queue backing next()/previous().
    QVector<QVariantMap> m_queue;
    int m_queueIndex = -1;

    // Equalizer state.
    std::array<double, kEqBandCount> m_eqGains{};
    bool m_eqEnabled = false;
    QString m_eqPreset;
    QMap<QString, std::array<double, kEqBandCount>> m_userPresets;

    QSettings m_settings;
};