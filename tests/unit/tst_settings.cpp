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
    void testSetTheme();
    void testFontScale();
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
    QCOMPARE(settings.theme(), QStringLiteral("Mimy"));
    QCOMPARE(settings.fontScale(), 1.0);

    // Unknown keys fall back to the provided default
    QCOMPARE(settings.getSetting(QStringLiteral("does.not.exist"), 42).toInt(), 42);
}

void tst_settings::testSetMusicDirectory()
{
    SettingsController settings;
    settings.setMusicDirectory(QStringLiteral("/home/user/Music"));

    QCOMPARE(settings.musicDirectory(), QStringLiteral("/home/user/Music"));
}

void tst_settings::testSetTheme()
{
    SettingsController settings;

    QCOMPARE(settings.theme(), QStringLiteral("Mimy"));
    settings.setTheme(QStringLiteral("Kawai"));
    QCOMPARE(settings.theme(), QStringLiteral("Kawai"));
    settings.setTheme(QStringLiteral("The Niggest"));
    QCOMPARE(settings.theme(), QStringLiteral("The Niggest"));
    settings.setTheme(QStringLiteral("Mimy"));
    QCOMPARE(settings.theme(), QStringLiteral("Mimy"));
}

void tst_settings::testFontScale()
{
    SettingsController settings;

    QCOMPARE(settings.fontScale(), 1.0);
    settings.setFontScale(1.4);
    QCOMPARE(settings.fontScale(), 1.4);
    settings.setFontScale(1.0);
    QCOMPARE(settings.fontScale(), 1.0);
}

void tst_settings::testSettingsChangedSignal()
{
    SettingsController settings;
    QSignalSpy spy(&settings, &SettingsController::settingsChanged);
    QVERIFY(spy.isValid());

    settings.setMusicDirectory(QStringLiteral("/music"));
    QCOMPARE(spy.count(), 1);

    settings.setTheme(QStringLiteral("Vacui"));
    QCOMPARE(spy.count(), 2);

    settings.setFontScale(1.4);
    QCOMPARE(spy.count(), 3);

    settings.setSetting(QStringLiteral("volume"), 0.5);
    QCOMPARE(spy.count(), 4);
}

void tst_settings::testNoSignalWhenValueUnchanged()
{
    SettingsController settings;
    QSignalSpy spy(&settings, &SettingsController::settingsChanged);
    QVERIFY(spy.isValid());

    // Default value (empty folder), no change expected
    settings.setMusicDirectory(QString());
    QCOMPARE(spy.count(), 0);

    settings.setMusicDirectory(QStringLiteral("/music"));
    QCOMPARE(spy.count(), 1);

    // Same value again, no change expected
    settings.setMusicDirectory(QStringLiteral("/music"));
    QCOMPARE(spy.count(), 1);

    // Same fontScale value again, no change expected
    settings.setFontScale(1.0);
    QCOMPARE(spy.count(), 1);
    settings.setFontScale(1.4);
    QCOMPARE(spy.count(), 2);
    settings.setFontScale(1.4);
    QCOMPARE(spy.count(), 2);
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
        first.setTheme(QStringLiteral("Good Technic"));
        first.setFontScale(1.4);
        first.save();
    }

    // A new instance must load the values saved by the previous one
    SettingsController second;
    QCOMPARE(second.musicDirectory(), QStringLiteral("/home/user/Music"));
    QCOMPARE(second.theme(), QStringLiteral("Good Technic"));
    QCOMPARE(second.fontScale(), 1.4);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"
