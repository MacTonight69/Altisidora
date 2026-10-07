#include <QtTest>
#include <QUrl>
#include <QSignalSpy>

#include "../mocks/MockPlayer.h"

/**
 * @brief Tests for audio player
 *
 * Tests playback control, state management, volume, seeking,
 * and error handling using a mock player.
 */
class TestPlayer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // State tests
    void testInitialState();
    void testPlaybackStateTransitions();

    // Playback control tests
    void testPlay();
    void testPause();
    void testStop();
    void testPlayWithoutMedia();

    // Position tests
    void testSetPosition();
    void testSetPositionBeyondDuration();
    void testSetPositionNegative();

    // Volume tests
    void testSetVolume();
    void testSetVolumeOutOfRange();
    void testSetMuted();

    // Media tests
    void testSetMedia();
    void testMediaEnded();

    // Error tests
    void testErrorState();
    void testErrorRecovery();

    // Signal tests
    void testStateChangedSignal();
    void testPositionChangedSignal();

private:
    MockPlayer *m_player;
};

void TestPlayer::initTestCase()
{
    m_player = new MockPlayer(this);
}

void TestPlayer::cleanupTestCase()
{
    delete m_player;
}

void TestPlayer::init()
{
    m_player->reset();
}

void TestPlayer::testInitialState()
{
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);
    QCOMPARE(m_player->position(), qint64(0));
    QCOMPARE(m_player->duration(), qint64(0));
    QCOMPARE(m_player->volume(), 1.0f);
    QCOMPARE(m_player->playbackRate(), qreal(1.0));
    QVERIFY(!m_player->isMuted());
    QVERIFY(!m_player->hasMedia());
    QVERIFY(m_player->errorString().isEmpty());
}

void TestPlayer::testPlaybackStateTransitions()
{
    // Stopped -> Loading -> Stopped (after load)
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Loading);

    // Wait for loading to complete
    QTest::qWait(100);
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);

    // Stopped -> Playing
    m_player->play();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Playing);

    // Playing -> Paused
    m_player->pause();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Paused);

    // Paused -> Playing
    m_player->play();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Playing);

    // Playing -> Stopped
    m_player->stop();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);
}

void TestPlayer::testPlay()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->play();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Playing);
    QCOMPARE(m_player->playCount(), 1);
}

void TestPlayer::testPause()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->play();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Playing);

    m_player->pause();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Paused);
    QCOMPARE(m_player->pauseCount(), 1);
}

void TestPlayer::testStop()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->play();
    m_player->stop();

    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);
    QCOMPARE(m_player->position(), qint64(0));
    QCOMPARE(m_player->stopCount(), 1);
}

void TestPlayer::testPlayWithoutMedia()
{
    m_player->reset();
    m_player->play();

    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Error);
    QVERIFY(!m_player->errorString().isEmpty());
}

void TestPlayer::testSetPosition()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->play();
    m_player->setPosition(60000); // 1 minute

    QCOMPARE(m_player->position(), qint64(60000));
    QCOMPARE(m_player->seekCount(), 1);
}

void TestPlayer::testSetPositionBeyondDuration()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->setPosition(999999999);
    QVERIFY(m_player->position() <= m_player->duration());
}

void TestPlayer::testSetPositionNegative()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    m_player->setPosition(-1000);
    QVERIFY(m_player->position() >= 0);
}

void TestPlayer::testSetVolume()
{
    m_player->setVolume(0.5f);
    QCOMPARE(m_player->volume(), 0.5f);

    m_player->setVolume(0.0f);
    QCOMPARE(m_player->volume(), 0.0f);

    m_player->setVolume(1.0f);
    QCOMPARE(m_player->volume(), 1.0f);
}

void TestPlayer::testSetVolumeOutOfRange()
{
    m_player->setVolume(2.0f);
    QCOMPARE(m_player->volume(), 1.0f);

    m_player->setVolume(-1.0f);
    QCOMPARE(m_player->volume(), 0.0f);
}

void TestPlayer::testSetMuted()
{
    m_player->setMuted(true);
    QVERIFY(m_player->isMuted());

    m_player->setMuted(false);
    QVERIFY(!m_player->isMuted());
}

void TestPlayer::testSetMedia()
{
    QUrl url(QStringLiteral("file:///music/test.mp3"));
    m_player->setMedia(url);

    QCOMPARE(m_player->currentMedia(), url);
    QVERIFY(m_player->hasMedia());
    QCOMPARE(m_player->loadedMedia().size(), 1);
}

void TestPlayer::testMediaEnded()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    QSignalSpy spy(m_player, &MockPlayer::mediaEnded);
    QVERIFY(spy.isValid());

    m_player->simulateMediaEnded();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);
}

void TestPlayer::testErrorState()
{
    m_player->simulateError(QStringLiteral("Test error"));
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Error);
    QCOMPARE(m_player->errorString(), QStringLiteral("Test error"));
}

void TestPlayer::testErrorRecovery()
{
    m_player->simulateError(QStringLiteral("Test error"));
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Error);

    m_player->reset();
    QCOMPARE(m_player->state(), MockPlayer::PlaybackState::Stopped);
    QVERIFY(m_player->errorString().isEmpty());
}

void TestPlayer::testStateChangedSignal()
{
    QSignalSpy spy(m_player, &MockPlayer::stateChanged);
    QVERIFY(spy.isValid());

    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    // Should have Loading and Stopped transitions
    QVERIFY(spy.count() >= 2);
}

void TestPlayer::testPositionChangedSignal()
{
    m_player->setMedia(QUrl(QStringLiteral("file:///music/test.mp3")));
    QTest::qWait(100);

    QSignalSpy spy(m_player, &MockPlayer::positionChanged);
    QVERIFY(spy.isValid());

    m_player->play();
    QTest::qWait(350);

    QVERIFY(spy.count() > 0);
}

QTEST_MAIN(TestPlayer)
#include "tst_player.moc"
