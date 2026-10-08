#include <QtTest>
#include <QUrl>
#include <QSignalSpy>

#include "audio/PlayerController.h"
#include "audio/PlaybackState.h"

/**
 * @brief Tests for PlayerController (src/audio/PlayerController.cpp).
 *
 * These tests exercise the controller's own logic only and deliberately avoid
 * asserting anything that depends on an actual media backend, so they run
 * deterministically on CI (including headless environments).
 */
class TestPlayer : public QObject
{
    Q_OBJECT

private slots:
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

    // Load tests
    void testLoadEmptyUrlEmitsError();
    void testLoadSongEmptyPathEmitsError();
    void testLoadSongKeepsMetadata();

    // Queue tests
    void testSetQueueAndNavigate();
    void testQueueBoundaries();

    // Error handling
    void testErrorStateBlocksPlay();

private:
    PlayerController *m_player = nullptr;
};

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
    m_player->load(QUrl());
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

void TestPlayer::testLoadEmptyUrlEmitsError()
{
    QSignalSpy errorSpy(m_player, &PlayerController::errorOccurred);
    QSignalSpy stateSpy(m_player, &PlayerController::stateChanged);

    // An empty URL is a hard failure by design (no media backend involved).
    m_player->load(QUrl());

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);
    QCOMPARE(stateSpy.count(), 1);
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

    // loadSong() emits currentSongChanged at least once (on data change).
    QVERIFY(songSpy.count() >= 1);

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
    m_player->load(QUrl());
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);

    m_player->play();
    QCOMPARE(m_player->playbackState(), PlaybackState::Error);
}

QTEST_MAIN(TestPlayer)
#include "tst_player.moc"