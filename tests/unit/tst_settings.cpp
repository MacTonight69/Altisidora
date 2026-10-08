#include <QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "../../src/app/SettingsController.h"

/**
 * @brief Tests for SettingsController
 *
 * Verifies defaults, setters, change notification and persistence
 * between controller instances. QSettings is redirected to a temporary
 * directory so tests never touch the user's real configuration.
 */
class tst_settings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void testDefaults();
    void testSetMusicDirectory();
    void testSetShuffleAndRepeat();
    void testSettingsChangedSignal();
    void testNoSignalWhenValueUnchanged();
    void testGenericGetSet();
    void testPersistenceAcrossInstances();

private:
    void clearSettings();

    QTemporaryDir m_tempDir;
};

void tst_settings::initTestCase()
{
    QVERIFY(m_tempDir.isValid());

    // Redirect all user-scope QSettings to the temp directory
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_tempDir.path());
    clearSettings();
}

void tst_settings::init()
{
    // Every test starts from clean settings
    clearSettings();
}

void tst_settings::clearSettings()
{
    QSettings settings(QStringLiteral("Altisidora"), QStringLiteral("Altisidora"));
    settings.clear();
    settings.sync();
}

void tst_settings::testDefaults()
{
    SettingsController settings;

    QVERIFY(settings.musicDirectory().isEmpty());
    QVERIFY(!settings.shuffle());
    QVERIFY(!settings.repeat());

    // Unknown keys fall back to the provided default
    QCOMPARE(settings.getSetting(QStringLiteral("does.not.exist"), 42).toInt(), 42);
}

void tst_settings::testSetMusicDirectory()
{
    SettingsController settings;
    settings.setMusicDirectory(QStringLiteral("/home/user/Music"));

    QCOMPARE(settings.musicDirectory(), QStringLiteral("/home/user/Music"));
}

void tst_settings::testSetShuffleAndRepeat()
{
    SettingsController settings;

    settings.setShuffle(true);
    settings.setRepeat(true);
    QVERIFY(settings.shuffle());
    QVERIFY(settings.repeat());

    settings.setShuffle(false);
    settings.setRepeat(false);
    QVERIFY(!settings.shuffle());
    QVERIFY(!settings.repeat());
}

void tst_settings::testSettingsChangedSignal()
{
    SettingsController settings;
    QSignalSpy spy(&settings, &SettingsController::settingsChanged);
    QVERIFY(spy.isValid());

    settings.setShuffle(true);
    QCOMPARE(spy.count(), 1);

    settings.setMusicDirectory(QStringLiteral("/music"));
    QCOMPARE(spy.count(), 2);

    settings.setSetting(QStringLiteral("volume"), 0.5);
    QCOMPARE(spy.count(), 3);
}

void tst_settings::testNoSignalWhenValueUnchanged()
{
    SettingsController settings;
    QSignalSpy spy(&settings, &SettingsController::settingsChanged);
    QVERIFY(spy.isValid());

    // Default value, no change expected
    settings.setShuffle(false);
    QCOMPARE(spy.count(), 0);

    settings.setShuffle(true);
    QCOMPARE(spy.count(), 1);

    // Same value again, no change expected
    settings.setShuffle(true);
    QCOMPARE(spy.count(), 1);
}

void tst_settings::testGenericGetSet()
{
    SettingsController settings;

    settings.setSetting(QStringLiteral("volume"), 0.75);
    QCOMPARE(settings.getSetting(QStringLiteral("volume"), 1.0).toDouble(), 0.75);

    settings.setSetting(QStringLiteral("lastFolder"), QStringLiteral("/tmp"));
    QCOMPARE(settings.getSetting(QStringLiteral("lastFolder"), QString()).toString(),
             QStringLiteral("/tmp"));
}

void tst_settings::testPersistenceAcrossInstances()
{
    {
        SettingsController first;
        first.setMusicDirectory(QStringLiteral("/home/user/Music"));
        first.setShuffle(true);
        first.setRepeat(true);
        first.save();
    }

    // A new instance must load the values saved by the previous one
    SettingsController second;
    QCOMPARE(second.musicDirectory(), QStringLiteral("/home/user/Music"));
    QVERIFY(second.shuffle());
    QVERIFY(second.repeat());
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"
