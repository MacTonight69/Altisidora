#include <QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <array>

#include "audio/PlayerController.h"
#include "audio/PlaybackState.h"

/**
 * @brief Tests for PlayerController (src/audio/PlayerController.cpp).
 *
 * These tests exercise the controller's own logic only and deliberately avoid
 * asserting anything that depends on an actual media backend, so they run
 * deterministically on CI (including headless environments). The mpv session
 * is created lazily and again is only touched when loadSong() receives a
 * valid file path.
 *
 * QSettings is redirected into a QTemporaryDir so the equalizer persistence
 * under test never touches the real user configuration.
 */
class TestPlayer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // State tests
    void testInitialState();

    // Playback control tests
    void testPlayInErrorStateIsNoOp();
    void testPauseAndStopDoNotCrash();

    // Seek tests
    void testSeekBeyondDurationIsIgnored();
    void testSeekNegativeIsIgnored();

    // Volume tests
    void testSetVolumeClamps();
    void testSetVolumeEmitsSignal();
    void testSetVolumeSameValueNoSignal();

    // Repeat-one tests
    void testRepeatOneDefaults();
    void testRepeatOneEmitsSignal();
    void testRepeatOnePersists();

    // Load tests
    void testLoadSongEmptyPathEmitsError();
    void testLoadSongKeepsMetadata();

    // Queue tests
    void testSetQueueAndNavigate();
    void testQueueBoundaries();

    // Error handling
    void testErrorStateBlocksPlay();

    // Equalizer tests
    void testEqInitialDefaults();
    void testEqGainClamps();
    void testEqSignalOnChange();
    void testEqPresetApplies();
    void testEqEnableToggle();
    void testEqFilterStringPure();
    void testEqUnicodePresetName();
    void testEqSaveCustomPreset();
    void testEqSaveCustomPresetRejectsInvalid();
    void testEqDeleteCustomPreset();

private:
    PlayerController *m_player = nullptr;
    QTemporaryDir *m_settingsDir = nullptr;
};

// Redirect QSettings so the equalizer persistence never touches the real
// user configuration on the developer machine or CI.
void TestPlayer::initTestCase()
{
    m_settingsDir = new QTemporaryDir();
    QVERIFY(m_settingsDir->isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir->path());
}

void TestPlayer::cleanupTestCase()
{
    delete m_settingsDir;
    m_settingsDir = nullptr;
}

// A fresh controller per test keeps every case independent.
void TestPlayer::init()
{
    m_player = new PlayerController(this);
}

void TestPlayer::cleanup()
{
    delete m_player;
    m_player = nullptr;
}

void TestPlayer::testInitialState()
{
    QCOMPARE(m_player->playbackState(), PlaybackState::Stopped);
    QCOMPARE(m_player->position(), qint64(0));
    QCOMPARE(m_player->duration(), qint64(0));
    QCOMPARE(m_player->volume(), 1.0f);
    QVERIFY(!m_player->isPlaying());
    QVERIFY(m_player->currentSong().isEmpty());
}

void TestPlayer::testPlayInErrorStateIsNoOp()
{
    // Force the controller into the Error state.
    QSignalSpy errorSpy(m_player, &PlayerController::errorOccurred);
    m_player->loadSong(QVariantMap());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);

    // play() must be rejected while in the Error state.
    m_player->play();
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);
}

void TestPlayer::testPauseAndStopDoNotCrash()
{
    // pausing/stopping with no media is safe and does not change the state.
    m_player->pause();
    m_player->stop();
    QCOMPARE(m_player->playbackState(), PlaybackState::Stopped);
}

void TestPlayer::testSeekBeyondDurationIsIgnored()
{
    // duration() is 0 with no media loaded, so any positive seek is ignored
    // and the position must remain unchanged.
    m_player->seek(1000);
    QCOMPARE(m_player->position(), qint64(0));
}

void TestPlayer::testSeekNegativeIsIgnored()
{
    m_player->seek(-10);
    QCOMPARE(m_player->position(), qint64(0));
}

void TestPlayer::testSetVolumeClamps()
{
    m_player->setVolume(2.0f);
    QCOMPARE(m_player->volume(), 1.0f);

    m_player->setVolume(-1.0f);
    QCOMPARE(m_player->volume(), 0.0f);
}

void TestPlayer::testSetVolumeEmitsSignal()
{
    QSignalSpy spy(m_player, &PlayerController::volumeChanged);
    QVERIFY(spy.isValid());

    m_player->setVolume(0.5f);
    QCOMPARE(spy.count(), 1);
    QVERIFY(qFuzzyCompare(spy.first().at(0).toReal(), 0.5));

    m_player->setVolume(0.25f);
    QCOMPARE(spy.count(), 2);
}

void TestPlayer::testSetVolumeSameValueNoSignal()
{
    m_player->setVolume(0.5f);
    QSignalSpy spy(m_player, &PlayerController::volumeChanged);

    // Repeating the change must not emit again.
    m_player->setVolume(0.5f);
    QCOMPARE(spy.count(), 0);
}

void TestPlayer::testRepeatOneDefaults()
{
    // Force a clean baseline in case a previous test left it enabled.
    m_player->setRepeatOne(false);
    QVERIFY(!m_player->repeatOne());
}

void TestPlayer::testRepeatOneEmitsSignal()
{
    QSignalSpy spy(m_player, &PlayerController::repeatOneChanged);
    QVERIFY(spy.isValid());

    // Same value: no change expected.
    m_player->setRepeatOne(false);
    QCOMPARE(spy.count(), 0);

    m_player->setRepeatOne(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(m_player->repeatOne());

    // Toggling back and forth keeps emitting only on real changes.
    m_player->setRepeatOne(true);
    QCOMPARE(spy.count(), 1);

    m_player->setRepeatOne(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!m_player->repeatOne());
}

void TestPlayer::testRepeatOnePersists()
{
    {
        PlayerController first;
        first.setRepeatOne(true);
    }

    // A fresh instance must restore the persisted state (QSettings sync
    // happens inside setRepeatOne()).
    PlayerController second;
    QVERIFY(second.repeatOne());
    second.setRepeatOne(false);
    QVERIFY(!second.repeatOne());
}

void TestPlayer::testLoadSongEmptyPathEmitsError()
{
    QSignalSpy errorSpy(m_player, &PlayerController::errorOccurred);

    m_player->loadSong(QVariantMap());

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);
}

void TestPlayer::testLoadSongKeepsMetadata()
{
    QVariantMap song;
    song[QStringLiteral("path")] = QStringLiteral("/tmp/nonexistent-track.mp3");
    song[QStringLiteral("title")] = QStringLiteral("My Track");
    song[QStringLiteral("artist")] = QStringLiteral("My Artist");
    song[QStringLiteral("album")] = QStringLiteral("My Album");
    song[QStringLiteral("duration")] = qint64(180000);

    QSignalSpy songSpy(m_player, &PlayerController::currentSongChanged);

    m_player->loadSong(song);

    // loadSong() emits currentSongChanged exactly once: the internal load()
    // sets the bare path without signalling, and loadSong() then emits the
    // full metadata map. This guards against reintroducing the duplicate
    // emission (which doubled every MPRIS Metadata update).
    QCOMPARE(songSpy.count(), 1);

    // The full metadata map must be preserved for the UI.
    const QVariantMap current = m_player->currentSong();
    QCOMPARE(current.value(QStringLiteral("title")).toString(), QStringLiteral("My Track"));
    QCOMPARE(current.value(QStringLiteral("artist")).toString(), QStringLiteral("My Artist"));
    QCOMPARE(current.value(QStringLiteral("album")).toString(), QStringLiteral("My Album"));
    QCOMPARE(current.value(QStringLiteral("duration")).toLongLong(), qint64(180000));
}

void TestPlayer::testSetQueueAndNavigate()
{
    QVariantMap first;
    first[QStringLiteral("path")] = QStringLiteral("/music/1.mp3");
    first[QStringLiteral("title")] = QStringLiteral("One");
    QVariantMap second;
    second[QStringLiteral("path")] = QStringLiteral("/music/2.mp3");
    second[QStringLiteral("title")] = QStringLiteral("Two");
    QVariantMap third;
    third[QStringLiteral("path")] = QStringLiteral("/music/3.mp3");
    third[QStringLiteral("title")] = QStringLiteral("Three");

    QVariantList queue{first, second, third};
    m_player->setQueue(queue);

    // Nothing loaded yet: navigation is not possible.
    QVERIFY(!m_player->canGoPrevious());
    QVERIFY(!m_player->canGoNext());

    // Loading the middle song positions the cursor in the queue.
    m_player->loadSong(second);
    QVERIFY(m_player->canGoPrevious());
    QVERIFY(m_player->canGoNext());
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("Two"));

    // next() advances; previous() goes back.
    m_player->next();
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("Three"));
    QVERIFY(!m_player->canGoNext());

    m_player->previous();
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("Two"));

    m_player->previous();
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("One"));
    QVERIFY(!m_player->canGoPrevious());

    // At the boundaries navigation is a no-op.
    m_player->previous();
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("One"));
}

void TestPlayer::testQueueBoundaries()
{
    QVariantMap only;
    only[QStringLiteral("path")] = QStringLiteral("/music/only.mp3");
    only[QStringLiteral("title")] = QStringLiteral("Only");

    m_player->setQueue(QVariantList{only});
    m_player->loadSong(only);

    QVERIFY(!m_player->canGoPrevious());
    QVERIFY(!m_player->canGoNext());

    // next()/previous() with a single-song queue are safe no-ops.
    m_player->next();
    m_player->previous();
    QCOMPARE(m_player->currentSong().value(QStringLiteral("title")).toString(),
             QStringLiteral("Only"));
}

void TestPlayer::testErrorStateBlocksPlay()
{
    // After an error, the controller refuses play() until a new load.
    m_player->loadSong(QVariantMap());
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);

    m_player->play();
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);
}

void TestPlayer::testEqInitialDefaults()
{
    QCOMPARE(m_player->eqEnabled(), false);
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Plano"));
    QCOMPARE(m_player->eqGains().size(), 10);
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(m_player->eqGain(i), 0.0);
    }
    QVERIFY(m_player->eqPresets().size() >= 4);
    QVERIFY(m_player->eqPresets().contains(QStringLiteral("Plano")));
}

void TestPlayer::testEqGainClamps()
{
    m_player->setEqGain(0, 99.0);
    QCOMPARE(m_player->eqGain(0), 12.0);

    m_player->setEqGain(0, -99.0);
    QCOMPARE(m_player->eqGain(0), -12.0);

    m_player->setEqGain(5, 3.5);
    QCOMPARE(m_player->eqGain(5), 3.5);

    // Out-of-range indexes are ignored and read as 0.
    m_player->setEqGain(42, 5.0);
    QCOMPARE(m_player->eqGain(42), 0.0);
    QCOMPARE(m_player->eqGain(-1), 0.0);
}

void TestPlayer::testEqSignalOnChange()
{
    QSignalSpy spy(m_player, &PlayerController::eqChanged);
    m_player->setEqGain(1, 2.0);
    QCOMPARE(spy.count(), 1);

    // Repeating the same gain must not emit again.
    m_player->setEqGain(1, 2.0);
    QCOMPARE(spy.count(), 1);
}

void TestPlayer::testEqPresetApplies()
{
    m_player->resetEq();
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Plano"));

    m_player->setEqPreset(QStringLiteral("Rock"));
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Rock"));
    QVERIFY(m_player->eqGain(0) > 0.0);  // Low shelf boosted.
    QVERIFY(m_player->eqGain(5) < 0.0);  // Mids slightly cut.

    // Unknown presets are ignored and leave the state untouched.
    m_player->setEqPreset(QStringLiteral("NoExiste"));
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Rock"));

    // Reset returns to a flat curve.
    m_player->resetEq();
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Plano"));
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(m_player->eqGain(i), 0.0);
    }
}

void TestPlayer::testEqEnableToggle()
{
    QSignalSpy spy(m_player, &PlayerController::eqChanged);
    m_player->setEqEnabled(true);
    QCOMPARE(m_player->eqEnabled(), true);
    QCOMPARE(spy.count(), 1);

    // Repeating must not emit.
    m_player->setEqEnabled(true);
    QCOMPARE(spy.count(), 1);

    m_player->setEqEnabled(false);
    QCOMPARE(m_player->eqEnabled(), false);
    QCOMPARE(spy.count(), 2);
}

void TestPlayer::testEqFilterStringPure()
{
    // A flat curve needs no filter at all.
    std::array<double, 10> flat{};
    QCOMPARE(PlayerController::buildEqFilterString(flat), QString());

    std::array<double, 10> gains{};
    gains[0] = 3.0;
    gains[9] = -4.0;
    const QString chain = PlayerController::buildEqFilterString(gains);

    QCOMPARE(chain.count(QStringLiteral("equalizer=")), 10);
    QCOMPARE(chain.count(QLatin1Char(',')), 9);
    QVERIFY(chain.contains(QStringLiteral("equalizer=f=31.5:t=o:w=1:g=3.00")));
    QVERIFY(chain.contains(QStringLiteral("equalizer=f=16000:t=o:w=1:g=-4.00")));
    QVERIFY(chain.contains(QStringLiteral("equalizer=f=500:t=o:w=1:g=0.00")));
}

void TestPlayer::testEqUnicodePresetName()
{
    // Regression: built-in names with non-ASCII characters must survive the
    // C++ -> QML boundary intact. Using QLatin1String over UTF-8 source
    // literals corrupted "Clásica" into "ClÃ¡sica" and broke both the label
    // and the apply/compare paths.
    QVERIFY(m_player->eqPresets().contains(QStringLiteral("Clásica")));

    m_player->setEqPreset(QStringLiteral("Clásica"));
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Clásica"));
    QVERIFY(qAbs(m_player->eqGain(0) - 3.0) < 0.001);
    QVERIFY(qAbs(m_player->eqGain(9) - 4.0) < 0.001);
}

void TestPlayer::testEqSaveCustomPreset()
{
    // Build a distinctive curve and save it under a custom name.
    m_player->setEqGain(0, 6.0);
    m_player->setEqGain(9, -3.0);

    QVERIFY(m_player->saveEqPreset(QStringLiteral("Mi curva")));
    QVERIFY(m_player->eqPresets().contains(QStringLiteral("Mi curva")));
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Mi curva"));

    // A fresh controller over the same redirected QSettings restores it.
    PlayerController second;
    QVERIFY(second.eqPresets().contains(QStringLiteral("Mi curva")));
    second.setEqPreset(QStringLiteral("Mi curva"));
    QVERIFY(qAbs(second.eqGain(0) - 6.0) < 0.001);
    QVERIFY(qAbs(second.eqGain(9) - (-3.0)) < 0.001);

    // Saving again under the same name updates the curve in place.
    second.setEqGain(1, 2.0);
    QVERIFY(second.saveEqPreset(QStringLiteral("Mi curva")));
    QVERIFY(qAbs(second.eqGain(1) - 2.0) < 0.001);

    // Clean up so no other test is polluted by this preset.
    second.deleteEqPreset(QStringLiteral("Mi curva"));
    QVERIFY(!second.eqPresets().contains(QStringLiteral("Mi curva")));
}

void TestPlayer::testEqSaveCustomPresetRejectsInvalid()
{
    // Empty and whitespace-only names are rejected.
    QVERIFY(!m_player->saveEqPreset(QString()));
    QVERIFY(!m_player->saveEqPreset(QStringLiteral("   ")));

    // Built-in names cannot be shadowed by a user preset.
    QVERIFY(!m_player->saveEqPreset(QStringLiteral("Rock")));
    QVERIFY(!m_player->saveEqPreset(QStringLiteral("Clásica")));

    // Rejected saves must not grow the list (only the 6 built-ins remain).
    QCOMPARE(m_player->eqPresets().size(), 6);
}

void TestPlayer::testEqDeleteCustomPreset()
{
    // Deleting an unknown or built-in name is a safe no-op.
    m_player->deleteEqPreset(QStringLiteral("NoExiste"));
    m_player->deleteEqPreset(QStringLiteral("Rock"));
    QCOMPARE(m_player->eqPresets().size(), 6);

    // Save, then delete: the preset disappears and the curve is unnamed.
    m_player->setEqGain(2, 4.0);
    QVERIFY(m_player->saveEqPreset(QStringLiteral("Prueba")));
    QVERIFY(m_player->eqPresets().contains(QStringLiteral("Prueba")));
    QCOMPARE(m_player->eqPreset(), QStringLiteral("Prueba"));

    m_player->deleteEqPreset(QStringLiteral("Prueba"));
    QVERIFY(!m_player->eqPresets().contains(QStringLiteral("Prueba")));
    QCOMPARE(m_player->eqPreset(), QString());  // Manual curve has no name.

    // The persistence file no longer references the removed preset.
    PlayerController second;
    QVERIFY(!second.eqPresets().contains(QStringLiteral("Prueba")));
}

QTEST_MAIN(TestPlayer)
#include "tst_player.moc"