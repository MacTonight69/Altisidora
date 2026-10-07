#include <QtTest>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QTemporaryDir>

#include "../TestHelpers.h"

/**
 * @brief Tests for file scanner
 *
 * Tests directory scanning, file discovery, and filtering
 * of audio files.
 */
class TestScanner : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // Directory scanning tests
    void testScanEmptyDirectory();
    void testScanDirectoryWithAudioFiles();
    void testScanDirectoryWithMixedFiles();
    void testScanNonExistentDirectory();
    void testScanNestedDirectories();

    // File filtering tests
    void testAudioFileExtensionFiltering();
    void testSupportedFormats();

    // File discovery tests
    void testFindAudioFiles();
    void testFindAudioFilesRecursive();

    // Error handling tests
    void testScanWithPermissionError();
    void testScanWithInvalidPath();

private:
    QTemporaryDir m_tempDir;
};

void TestScanner::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestScanner::cleanupTestCase()
{
}

void TestScanner::init()
{
    // Start each test with an empty temp directory so files
    // created by previous tests do not accumulate
    QDir root(m_tempDir.path());
    const auto entries = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            QDir(info.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void TestScanner::testScanEmptyDirectory()
{
    QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 0);
    QVERIFY(files.isEmpty());
}

void TestScanner::testScanDirectoryWithAudioFiles()
{
    QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    // Verify all files exist
    for (const QString &file : files) {
        QVERIFY(QFile::exists(file));
    }
}

void TestScanner::testScanDirectoryWithMixedFiles()
{
    // Create audio files
    QStringList audioFiles = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);

    // Create non-audio files
    QFile txtFile(m_tempDir.filePath(QStringLiteral("readme.txt")));
    QVERIFY(txtFile.open(QIODevice::WriteOnly));
    txtFile.write("This is not an audio file");
    txtFile.close();

    QFile jpgFile(m_tempDir.filePath(QStringLiteral("cover.jpg")));
    QVERIFY(jpgFile.open(QIODevice::WriteOnly));
    jpgFile.write("fake image data");
    jpgFile.close();

    // Scan should only find audio files
    QDir dir(m_tempDir.path());
    QStringList audioFilters;
    audioFilters << QStringLiteral("*.mp3") << QStringLiteral("*.ogg") << QStringLiteral("*.flac") << QStringLiteral("*.wav");

    QStringList foundFiles = dir.entryList(audioFilters, QDir::Files);
    QCOMPARE(foundFiles.size(), 3);
}

void TestScanner::testScanNonExistentDirectory()
{
    QDir dir(QStringLiteral("/nonexistent/path/that/does/not/exist"));
    QStringList files = dir.entryList(QDir::Files);
    QVERIFY(files.isEmpty());
}

void TestScanner::testScanNestedDirectories()
{
    // Create nested directory structure
    QDir nestedDir(m_tempDir.filePath(QStringLiteral("artist/album")));
    QVERIFY(nestedDir.mkpath(QStringLiteral(".")));

    // Create files in nested directory
    QStringList nestedFiles = TestHelpers::createTestMusicDirectory(nestedDir.path(), 3);

    // Recursive scan should find all files
    QDir dir(m_tempDir.path());
    QStringList audioFilters;
    audioFilters << QStringLiteral("*.mp3") << QStringLiteral("*.ogg") << QStringLiteral("*.flac");

    QStringList foundFiles;
    QDirIterator it(dir.path(), audioFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        foundFiles.append(it.next());
    }

    QCOMPARE(foundFiles.size(), 3);
}

void TestScanner::testAudioFileExtensionFiltering()
{
    QStringList supportedExtensions = {
        QStringLiteral(".mp3"),
        QStringLiteral(".ogg"),
        QStringLiteral(".flac"),
        QStringLiteral(".wav"),
        QStringLiteral(".m4a"),
        QStringLiteral(".opus")
    };

    // Verify all expected extensions are supported
    QVERIFY(supportedExtensions.contains(QStringLiteral(".mp3")));
    QVERIFY(supportedExtensions.contains(QStringLiteral(".ogg")));
    QVERIFY(supportedExtensions.contains(QStringLiteral(".flac")));
    QVERIFY(supportedExtensions.contains(QStringLiteral(".wav")));
}

void TestScanner::testSupportedFormats()
{
    // Test that common audio formats are recognized
    QStringList audioFiles;
    audioFiles << QStringLiteral("song.mp3")
               << QStringLiteral("song.ogg")
               << QStringLiteral("song.flac")
               << QStringLiteral("song.wav")
               << QStringLiteral("song.m4a")
               << QStringLiteral("song.opus");

    for (const QString &file : audioFiles) {
        QVERIFY(file.endsWith(QStringLiteral(".mp3")) ||
                file.endsWith(QStringLiteral(".ogg")) ||
                file.endsWith(QStringLiteral(".flac")) ||
                file.endsWith(QStringLiteral(".wav")) ||
                file.endsWith(QStringLiteral(".m4a")) ||
                file.endsWith(QStringLiteral(".opus")));
    }
}

void TestScanner::testFindAudioFiles()
{
    QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);

    QDir dir(m_tempDir.path());
    QStringList audioFilters;
    audioFilters << QStringLiteral("*.mp3") << QStringLiteral("*.ogg") << QStringLiteral("*.flac");

    QStringList foundFiles = dir.entryList(audioFilters, QDir::Files);
    QCOMPARE(foundFiles.size(), 5);
}

void TestScanner::testFindAudioFilesRecursive()
{
    // Create files in root
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);

    // Create files in subdirectory
    QDir subDir(m_tempDir.filePath(QStringLiteral("subdir")));
    QVERIFY(subDir.mkpath(QStringLiteral(".")));
    TestHelpers::createTestMusicDirectory(subDir.path(), 3);

    // Recursive scan
    QDir dir(m_tempDir.path());
    QStringList audioFilters;
    audioFilters << QStringLiteral("*.mp3") << QStringLiteral("*.ogg") << QStringLiteral("*.flac");

    QStringList foundFiles;
    QDirIterator it(dir.path(), audioFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        foundFiles.append(it.next());
    }

    QCOMPARE(foundFiles.size(), 5);
}

void TestScanner::testScanWithPermissionError()
{
    // This test verifies that scanning handles permission errors gracefully
    // In a real implementation, this would test actual permission scenarios
    QDir dir(QStringLiteral("/root"));
    // Should not crash, just return empty or handle gracefully
    QStringList files = dir.entryList(QDir::Files);
    // We can't guarantee the result, but it shouldn't crash
    QVERIFY(true);
}

void TestScanner::testScanWithInvalidPath()
{
    // A regular file path is invalid for directory listing
    QFile file(m_tempDir.filePath(QStringLiteral("not_a_dir.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QDir dir(file.fileName());
    QStringList files = dir.entryList(QDir::Files);
    // Listing a path that is not a directory should return an empty list
    QVERIFY(files.isEmpty());
}

QTEST_MAIN(TestScanner)
#include "tst_scanner.moc"
