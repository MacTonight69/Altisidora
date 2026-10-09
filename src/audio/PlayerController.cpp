#include "PlayerController.h"

#include <mpv/client.h>

#include <QDebug>
#include <QLocale>
#include <QTimer>

#include <clocale>

namespace {

// Standard octave-band frequencies (Hz) of the graphic equalizer.
const char *kEqFrequencies[PlayerController::kEqBandCount] = {
    "31.5", "63", "125", "250", "500", "1000", "2000", "4000", "8000", "16000"
};

struct EqPreset {
    const char *name;
    std::array<double, PlayerController::kEqBandCount> gains;
};

const EqPreset kEqPresets[] = {
    { "Plano",   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { "Graves+", { 7, 6, 5, 3, 1, 0, 0, 0, 0, 0 } },
    { "Voz",     { 0, -1, -2, 0, 2, 3, 4, 3, 0, -1 } },
    { "Pop",     { -1, 0, 1, 3, 4, 3, 0, -1, -1, 0 } },
    { "Rock",    { 5, 4, 3, 1, 0, -1, 0, 2, 3, 4 } },
    { "Clásica", { 3, 3, 2, 1, 0, 0, 0, 1, 3, 4 } },
};

constexpr double kGainEps = 0.001;

} // namespace

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_playbackState(PlaybackState::Stopped)
    , m_position(0)
    , m_duration(0)
    , m_volume(1.0f)
    , m_eqPreset(QStringLiteral("Plano"))
    , m_settings(QStringLiteral("Altisidora"), QStringLiteral("Altisidora"))
{
    loadEqSettings();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(150);
    connect(m_pollTimer, &QTimer::timeout, this, &PlayerController::pollTick);
    m_pollTimer->start();
}

PlayerController::~PlayerController()
{
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
    }
}

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

bool PlayerController::eqEnabled() const
{
    return m_eqEnabled;
}

QVariantList PlayerController::eqGains() const
{
    QVariantList list;
    list.reserve(kEqBandCount);
    for (double gain : m_eqGains) {
        list.append(gain);
    }
    return list;
}

QStringList PlayerController::eqPresets() const
{
    QStringList names;
    names.reserve(std::size(kEqPresets));
    for (const EqPreset &preset : kEqPresets) {
        names.append(QLatin1String(preset.name));
    }
    return names;
}

QString PlayerController::eqPreset() const
{
    return m_eqPreset;
}

void PlayerController::play()
{
    // Loading/Stopped/Paused are all fine to play; Error blocks until a load
    // resets it.
    if (m_playbackState == PlaybackState::Error) {
        return;
    }
    if (!m_mpv) {
        return; // No media loaded yet.
    }
    sendCommand("set", "pause", "no");
}

void PlayerController::pause()
{
    if (!m_mpv) {
        return;
    }
    sendCommand("set", "pause", "yes");
}

void PlayerController::stop()
{
    // A stop is intentional: a later END_FILE(STOP) becomes the Stopped state
    // (and not the "replaced by loadfile" no-op).
    m_pendingLoad = false;
    if (m_mpv) {
        sendCommand("stop");
    }
    m_position = 0;
    emit positionChanged(m_position);
    setPlaybackState(PlaybackState::Stopped);
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
        play();
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
        play();
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
    if (!m_mpv) {
        return;
    }
    // mpv seeks in seconds; "absolute+exact" gives millisecond precision.
    const QString seconds = QLocale::c().toString(position / 1000.0, 'f', 3);
    sendCommand("seek", seconds.toUtf8(), "absolute+exact");
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
    applyVolume();
    emit volumeChanged(m_volume);
}

void PlayerController::load(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty()) {
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Invalid media URL"));
        return;
    }

    if (!ensureMpv()) {
        return; // ensureMpv() already moved the controller to Error.
    }

    // Capture intent before transitioning: the new file must start paused
    // unless the player was actively playing (so load() alone does not
    // autoplay, matching the old QMediaPlayer behaviour).
    const bool keepPlaying = (m_playbackState == PlaybackState::Playing);

    setPlaybackState(PlaybackState::Loading);
    m_pendingLoad = true;
    m_position = 0;
    emit positionChanged(m_position);

    const QString target = url.isLocalFile() ? url.toLocalFile() : url.toString();
    sendCommand("loadfile", target.toUtf8());
    sendCommand("set", "pause", keepPlaying ? "no" : "yes");

    updateCurrentSong(url.toString());
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

    // load() resets currentSong to the bare URL; restore the full metadata
    // map so the UI can display the song details.
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

double PlayerController::eqGain(int index) const
{
    if (index < 0 || index >= kEqBandCount) {
        return 0.0;
    }
    return m_eqGains[index];
}

void PlayerController::setEqGain(int index, double valueDb)
{
    if (index < 0 || index >= kEqBandCount) {
        return;
    }
    valueDb = qBound(kEqMinDb, valueDb, kEqMaxDb);
    if (qAbs(valueDb - m_eqGains[index]) < kGainEps) {
        return;
    }

    m_eqGains[index] = valueDb;
    m_eqPreset = presetNameFor(m_eqGains);

    applyEq();
    saveEqSettings();
    emit eqChanged();
}

void PlayerController::resetEq()
{
    const auto flat = presetGains(QStringLiteral("Plano"));
    if (!flat) {
        return; // Unreachable: "Plano" is always defined.
    }

    bool samePreset = (m_eqPreset == QLatin1String("Plano"));
    bool sameGains = true;
    for (int i = 0; i < kEqBandCount; ++i) {
        if (qAbs(m_eqGains[i] - (*flat)[i]) > kGainEps) {
            sameGains = false;
            break;
        }
    }
    if (samePreset && sameGains) {
        return;
    }

    m_eqGains = *flat;
    m_eqPreset = QStringLiteral("Plano");
    applyEq();
    saveEqSettings();
    emit eqChanged();
}

void PlayerController::setEqEnabled(bool enabled)
{
    if (m_eqEnabled == enabled) {
        return;
    }
    m_eqEnabled = enabled;
    applyEq();
    saveEqSettings();
    emit eqChanged();
}

void PlayerController::setEqPreset(const QString &name)
{
    const auto gains = presetGains(name);
    if (!gains) {
        return; // Unknown preset: ignore.
    }

    bool samePreset = (m_eqPreset == name);
    bool sameGains = true;
    for (int i = 0; i < kEqBandCount; ++i) {
        if (qAbs(m_eqGains[i] - (*gains)[i]) > kGainEps) {
            sameGains = false;
            break;
        }
    }
    if (samePreset && sameGains) {
        return;
    }

    m_eqGains = *gains;
    m_eqPreset = name;
    applyEq();
    saveEqSettings();
    emit eqChanged();
}

QString PlayerController::buildEqFilterString(const std::array<double, kEqBandCount> &gains)
{
    bool flat = true;
    for (double gain : gains) {
        if (qAbs(gain) > 0.001) {
            flat = false;
            break;
        }
    }
    if (flat) {
        return QString();
    }

    QStringList bands;
    bands.reserve(kEqBandCount);
    for (int i = 0; i < kEqBandCount; ++i) {
        bands.append(QStringLiteral("equalizer=f=%1:t=o:w=1:g=%2")
                         .arg(QLatin1String(kEqFrequencies[i]))
                         .arg(QLocale::c().toString(gains[i], 'f', 2)));
    }
    return bands.join(QLatin1Char(','));
}

void PlayerController::pollTick()
{
    if (!m_mpv) {
        return;
    }

    drainMpvEvents();

    int pause = 0;
    int idle = 0;
    double positionSeconds = -1.0;
    double durationSeconds = -1.0;
    mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause);
    mpv_get_property(m_mpv, "idle-active", MPV_FORMAT_FLAG, &idle);
    mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &positionSeconds);
    mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &durationSeconds);

    if (durationSeconds > 0.0) {
        const qint64 durationMs = qRound64(durationSeconds * 1000.0);
        if (m_duration != durationMs) {
            m_duration = durationMs;
            emit durationChanged(m_duration);
        }
    }
    if (positionSeconds >= 0.0) {
        const qint64 positionMs = qRound64(positionSeconds * 1000.0);
        if (m_position != positionMs) {
            m_position = positionMs;
            emit positionChanged(m_position);
        }
    }

    if (m_playbackState == PlaybackState::Error) {
        return;
    }

    // While a file is being loaded, Loading wins over Paused/Playing so the
    // UI does not flicker between states.
    if (m_pendingLoad) {
        setPlaybackState(PlaybackState::Loading);
        return;
    }

    if (idle) {
        setPlaybackState(PlaybackState::Stopped);
        return;
    }
    setPlaybackState(pause ? PlaybackState::Paused : PlaybackState::Playing);
}

bool PlayerController::ensureMpv()
{
    if (m_mpv) {
        return true;
    }

    // libmpv requires the C locale for all its internal number parsing
    // (av_strtod-based parsers fail with comma decimal separators). Qt pulls
    // the environment locale into the C runtime on startup (e.g. es_AR), which
    // made mpv_create() return NULL. Set it once before creating the session;
    // it stays active for the whole process.
    if (std::setlocale(LC_NUMERIC, "C") == nullptr) {
        qWarning() << "PlayerController: could not set LC_NUMERIC to C";
    }

    m_mpv = mpv_create();
    if (!m_mpv) {
        qWarning() << "PlayerController: mpv_create failed";
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Audio engine failed to start"));
        return false;
    }

    // Isolate the session: no user config/scripts, headless audio pipeline.
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "load-scripts", "no");
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "vid", "no");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "audio-client-name", "altisidora");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-builtin-bindings", "no");

    const int initRc = mpv_initialize(m_mpv);
    if (initRc < 0) {
        qWarning() << "PlayerController: mpv_initialize failed rc=" << initRc
                   << "error=" << mpv_error_string(initRc);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        setPlaybackState(PlaybackState::Error);
        emit errorOccurred(tr("Audio engine failed to start"));
        return false;
    }

    applyVolume();
    applyEq();
    return true;
}

void PlayerController::applyVolume()
{
    if (!m_mpv) {
        return;
    }
    double percent = m_volume * 100.0;
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &percent);
}

void PlayerController::applyEq()
{
    if (!m_mpv) {
        return;
    }
    const QString chain = m_eqEnabled ? buildEqFilterString(m_eqGains) : QString();
    const QByteArray utf8 = chain.toUtf8();
    if (mpv_set_property_string(m_mpv, "af", utf8.constData()) < 0) {
        qWarning() << "PlayerController: failed to apply eq filter chain";
    }
}

void PlayerController::sendCommand(const QByteArray &a1, const QByteArray &a2,
                                   const QByteArray &a3)
{
    if (!m_mpv) {
        return;
    }
    const char *argv[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    argv[0] = a1.constData();
    argv[1] = a2.isEmpty() ? nullptr : a2.constData();
    argv[2] = a3.isEmpty() ? nullptr : a3.constData();
    mpv_command(m_mpv, argv);
}

void PlayerController::drainMpvEvents()
{
    for (;;) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }

        switch (event->event_id) {
        case MPV_EVENT_START_FILE:
            setPlaybackState(PlaybackState::Loading);
            break;
        case MPV_EVENT_FILE_LOADED:
            m_pendingLoad = false;
            break;
        case MPV_EVENT_END_FILE: {
            const auto *endFile = static_cast<const mpv_event_end_file *>(event->data);
            switch (endFile->reason) {
            case MPV_END_FILE_REASON_EOF:
                m_pendingLoad = false;
                handleEndOfMedia();
                break;
            case MPV_END_FILE_REASON_ERROR:
                m_pendingLoad = false;
                setPlaybackState(PlaybackState::Error);
                emit errorOccurred(tr("Unable to load or play the file"));
                break;
            case MPV_END_FILE_REASON_STOP:
                // A STOP event fires both for an explicit stop() and when
                // loadfile() replaces the current file. In the latter case a
                // new media is already on its way (m_pendingLoad).
                if (!m_pendingLoad) {
                    setPlaybackState(PlaybackState::Stopped);
                }
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
}

void PlayerController::handleEndOfMedia()
{
    if (canGoNext()) {
        loadSong(m_queue.at(m_queueIndex + 1));
        play();
    } else {
        setPlaybackState(PlaybackState::Stopped);
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

void PlayerController::updateCurrentSong(const QString &path)
{
    m_currentSong.clear();
    m_currentSong[QStringLiteral("path")] = path;
    emit currentSongChanged(m_currentSong);
}

void PlayerController::loadEqSettings()
{
    m_eqEnabled = m_settings.value(QStringLiteral("eqEnabled"), false).toBool();
    m_eqPreset = m_settings.value(QStringLiteral("eqPreset"), QStringLiteral("Plano")).toString();

    const QStringList raw = m_settings.value(QStringLiteral("eqGains")).toStringList();
    if (raw.size() != kEqBandCount) {
        m_eqGains.fill(0.0);
        return;
    }

    bool ok = true;
    for (int i = 0; i < kEqBandCount; ++i) {
        const double value = raw.at(i).toDouble(&ok);
        if (!ok) {
            break;
        }
        m_eqGains[i] = value;
    }
    if (!ok) {
        m_eqGains.fill(0.0);
        m_eqPreset = QStringLiteral("Plano");
    }
}

void PlayerController::saveEqSettings()
{
    QStringList raw;
    raw.reserve(kEqBandCount);
    for (double gain : m_eqGains) {
        raw.append(QLocale::c().toString(gain, 'f', 2));
    }
    m_settings.setValue(QStringLiteral("eqEnabled"), m_eqEnabled);
    m_settings.setValue(QStringLiteral("eqGains"), raw);
    m_settings.setValue(QStringLiteral("eqPreset"), m_eqPreset);
    m_settings.sync();
}

std::optional<std::array<double, PlayerController::kEqBandCount>>
PlayerController::presetGains(const QString &name)
{
    for (const EqPreset &preset : kEqPresets) {
        if (name == QLatin1String(preset.name)) {
            return preset.gains;
        }
    }
    return std::nullopt;
}

QString PlayerController::presetNameFor(const std::array<double, kEqBandCount> &gains)
{
    for (const EqPreset &preset : kEqPresets) {
        bool match = true;
        for (int i = 0; i < kEqBandCount; ++i) {
            if (qAbs(preset.gains[i] - gains[i]) > 0.25) {
                match = false;
                break;
            }
        }
        if (match) {
            return QLatin1String(preset.name);
        }
    }
    return QString(); // Manual curve: no preset matches.
}