#include <QtTest>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>

#include "../../src/mpris/MprisPlayer.h"
#include "../../src/audio/PlayerController.h"

/**
 * @brief Tests for the MPRIS v2 D-Bus integration.
 *
 * Runs under `dbus-run-session` (see tests/CMakeLists.txt) so a session bus
 * always exists: the test registers org.mpris.MediaPlayer2.altisidora on the
 * private bus and then talks back to it through QDBusInterface, exactly like
 * a desktop shell would (MangoWM media indicator, media keys, sound menu).
 */
class tst_mpris : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testMediaPlayer2Interface();
    void testPlayerInterfaceAndMetadata();
    void testTransportSlotsRespond();
    void testEmitsPropertiesChanged();
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    void loadFakeSong();

    PlayerController *m_player = nullptr;
    MprisPlayer *m_mpris = nullptr;
    int m_changeCount = 0;
    QVariantMap m_playerChanged;
};

void tst_mpris::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());

    m_player = new PlayerController(this);
    m_mpris = new MprisPlayer(m_player, this);
    QVERIFY(m_mpris->registerService());

    // Listen exactly like a desktop shell does: only the standard
    // org.freedesktop.DBus.Properties.PropertiesChanged signal counts.
    QVERIFY(QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))));
}

void tst_mpris::loadFakeSong()
{
    QVariantMap song;
    song[QStringLiteral("id")] = 42;
    song[QStringLiteral("path")] = QStringLiteral("/tmp/mpris-nonexistent.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Test Song");
    song[QStringLiteral("artist")] = QStringLiteral("Test Artist");
    song[QStringLiteral("album")] = QStringLiteral("Test Album");
    song[QStringLiteral("duration")] = 180000; // 3 minutes
    m_player->setQueue({QVariant(song)});
    m_player->loadSong(song);
}

void tst_mpris::testMediaPlayer2Interface()
{
    QDBusInterface iface(QStringLiteral("org.mpris.MediaPlayer2.altisidora"),
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2"),
                         QDBusConnection::sessionBus(), this);
    QVERIFY(iface.isValid());

    QCOMPARE(iface.property("Identity").toString(),
             QStringLiteral("Altisidora"));
    QCOMPARE(iface.property("DesktopEntry").toString(),
             QStringLiteral("altisidora"));
    QVERIFY(iface.property("CanQuit").toBool());
    QVERIFY(!iface.property("HasTrackList").toBool());
}

void tst_mpris::testPlayerInterfaceAndMetadata()
{
    loadFakeSong();

    QDBusInterface iface(QStringLiteral("org.mpris.MediaPlayer2.altisidora"),
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus(), this);
    QVERIFY(iface.isValid());

    // No playback started: the player reports Stopped.
    QCOMPARE(iface.property("PlaybackStatus").toString(),
             QStringLiteral("Stopped"));
    QCOMPARE(iface.property("CanControl").toBool(), true);
    QCOMPARE(iface.property("Position").toLongLong(), 0);

    // Metadata mirrors the loaded song (length in microseconds).
    const QVariantMap meta = iface.property("Metadata").toMap();
    QCOMPARE(meta.value(QStringLiteral("xesam:title")).toString(),
             QStringLiteral("Test Song"));
    QCOMPARE(meta.value(QStringLiteral("xesam:artist")).toStringList(),
             QStringList{QStringLiteral("Test Artist")});
    QCOMPARE(meta.value(QStringLiteral("xesam:album")).toString(),
             QStringLiteral("Test Album"));
    QCOMPARE(meta.value(QStringLiteral("mpris:length")).toLongLong(), 180000000);
    const QString trackId =
        meta.value(QStringLiteral("mpris:trackid")).value<QDBusObjectPath>().path();
    QCOMPARE(trackId,
             QStringLiteral("/org/mpris/MediaPlayer2/TrackList/Track/42"));
}

void tst_mpris::testTransportSlotsRespond()
{
    loadFakeSong();

    QDBusInterface iface(QStringLiteral("org.mpris.MediaPlayer2.altisidora"),
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus(), this);
    QVERIFY(iface.isValid());

    // Transport methods must be dispatched without D-Bus errors.
    QDBusReply<void> pause = iface.call(QStringLiteral("Pause"));
    QVERIFY(pause.isValid());
    QDBusReply<void> stop = iface.call(QStringLiteral("Stop"));
    QVERIFY(stop.isValid());
    QDBusReply<void> next = iface.call(QStringLiteral("Next"));
    QVERIFY(next.isValid());
    QDBusReply<void> previous = iface.call(QStringLiteral("Previous"));
    QVERIFY(previous.isValid());

    QDBusMessage seeked =
        iface.call(QStringLiteral("Seek"), QVariant::fromValue<qint64>(0));
    QCOMPARE(seeked.type(), QDBusMessage::ReplyMessage);

    const QVariant pos = QVariant::fromValue(QDBusObjectPath(
        QStringLiteral("/org/mpris/MediaPlayer2/TrackList/Track/42")));
    QDBusMessage setPosition = iface.call(QStringLiteral("SetPosition"), pos,
                                          QVariant::fromValue<qint64>(0));
    QCOMPARE(setPosition.type(), QDBusMessage::ReplyMessage);
}

// Qt demarshals a dict (a{sv}) nested inside a D-Bus variant as QDBusArgument
// instead of QVariantMap. Convert it back so the test can inspect Metadata.
static QVariant demarshalVariant(const QVariant &v)
{
    if (v.userType() != qMetaTypeId<QDBusArgument>())
        return v;
    QDBusArgument arg = v.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::MapType)
        return v;
    return qdbus_cast<QVariantMap>(arg);
}

void tst_mpris::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                    const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    ++m_changeCount;
    if (interface == QStringLiteral("org.mpris.MediaPlayer2.Player")) {
        for (auto it = changed.constBegin(); it != changed.constEnd(); ++it) {
            m_playerChanged.insert(it.key(), demarshalVariant(it.value()));
        }
    }
}

void tst_mpris::testEmitsPropertiesChanged()
{
    m_changeCount = 0;
    m_playerChanged.clear();
    loadFakeSong();
    QTRY_VERIFY_WITH_TIMEOUT(m_changeCount > 0, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_playerChanged.value(QStringLiteral("Metadata")).toMap()
                .value(QStringLiteral("xesam:title")).toString() == QStringLiteral("Test Song"),
        2000);
    QCOMPARE(m_playerChanged.value(QStringLiteral("Metadata")).toMap()
                 .value(QStringLiteral("xesam:artist")).toStringList(),
             QStringList{QStringLiteral("Test Artist")});

    m_changeCount = 0;
    m_playerChanged.clear();
    m_player->setVolume(0.25f);
    QTRY_VERIFY_WITH_TIMEOUT(m_changeCount > 0, 2000);
    QVERIFY(m_playerChanged.contains(QStringLiteral("Volume")));
    QVERIFY(qFuzzyCompare(m_playerChanged.value(QStringLiteral("Volume")).toDouble(), 0.25));
}

QTEST_MAIN(tst_mpris)
#include "tst_mpris.moc"