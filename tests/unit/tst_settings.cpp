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
    void testDeleteFilesFromDisk();
    void testSettingsChangedSignal();
    void testNoSignalWhenValueUnchanged();
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

    // Deleting songs removes the files from disk by default (active mode).
    QVERIFY(settings.deleteFilesFromDisk());
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
    settings.setTheme(QStringLiteral("Onyx"));
    QCOMPARE(settings.theme(), QStringLiteral("Onyx"));
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

void tst_settings::testDeleteFilesFromDisk()
{
    SettingsController settings;
    QSignalSpy spy(&settings, &SettingsController::settingsChanged);
    QVERIFY(spy.isValid());

    QVERIFY(settings.deleteFilesFromDisk());

    // Switching to passive mode updates the getter and notifies once.
    settings.setDeleteFilesFromDisk(false);
    QVERIFY(!settings.deleteFilesFromDisk());
    QCOMPARE(spy.count(), 1);

    // Writing the same value again must not notify.
    settings.setDeleteFilesFromDisk(false);
    QCOMPARE(spy.count(), 1);

    settings.save();

    // A fresh instance loads the persisted value.
    SettingsController reloaded;
    QVERIFY(!reloaded.deleteFilesFromDisk());
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
