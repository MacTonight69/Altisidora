#include <QtTest>

#include "../../src/audio/PlayerController.h"
#include "../../src/media/MediaIntegration.h"
#include "../../src/media/NullMediaIntegration.h"

#if defined(ALTISIDORA_HAS_DBUS)
#include "../../src/media/MprisMediaIntegration.h"
#endif

/**
 * @brief Tests for the platform media seam.
 *
 * The seam must hold in every configuration: the no-op backend honours the
 * MediaIntegration contract even with nothing to publish, and
 * createMediaIntegration() returns the backend the build actually compiled
 * in. The conditional below mirrors the factory's single conditional.
 */
class tst_media : public QObject
{
    Q_OBJECT

private slots:
    void testNullBackendContract();
    void testFactorySelectsCompiledBackend();
};

void tst_media::testNullBackendContract()
{
    PlayerController player;
    NullMediaIntegration null_(&player);

    // No platform media service: start() reports unavailable, stop() is
    // still a safe no-op.
    QVERIFY(!null_.start());
    null_.stop();

    // Nothing is published: empty metadata, idle player, zero position.
    QVERIFY(null_.metadata().isEmpty());
    QCOMPARE(null_.playbackStatus(), QStringLiteral("Stopped"));
    QCOMPARE(null_.positionMicros(), qint64(0));
}

void tst_media::testFactorySelectsCompiledBackend()
{
    PlayerController player;
    MediaIntegration *integration = createMediaIntegration(&player);
    QVERIFY(integration);

#if defined(ALTISIDORA_HAS_DBUS)
    QVERIFY(qobject_cast<MprisMediaIntegration *>(integration));
#else
    QVERIFY(qobject_cast<NullMediaIntegration *>(integration));
#endif

    delete integration;
}

QTEST_MAIN(tst_media)
#include "tst_media.moc"
