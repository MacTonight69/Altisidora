#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include "../TestHelpers.h"
#include "library/Scanner.h"

/**
 * @brief Tests for the real file scanner (src/library/Scanner.cpp).
 */
class TestScanner : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void testScanEmptyDirectory();
    void testScanWithAudioFiles();
    void testScanWithMixedFiles();
    void testScanNonExistentDirectory();
    void testScanSingleAudioFile();
    void testScanRecursive();
    void testScanSkipsIgnoredDirectory();
    void testScanSkipsIgnoredFile();
    void testIsSupportedAudioFile();
    void testSupportedExtensionsCaseInsensitive();

private:
    QTemporaryDir m_tempDir;
};

// Each test starts from a clean directory so files never accumulate.
void TestScanner::init()
{
    QDir root(m_tempDir.path());
    const auto entries = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                            | QDir::Hidden | QDir::System);
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
    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path());
    QCOMPARE(found.size(), 0);
}

void TestScanner::testScanWithAudioFiles()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path());
    QCOMPARE(found.size(), 5);

    // Every found path must exist and be one of the created files.
    QSet<QString> created(files.begin(), files.end());
    for (const QString &path : found) {
        QVERIFY(QFile::exists(path));
        QVERIFY(created.contains(path));
    }
}

void TestScanner::testScanWithMixedFiles()
{
    bool ok = TestHelpers::createDummyAudioFile(m_tempDir.filePath(QStringLiteral("a.mp3")));
    ok = TestHelpers::createDummyAudioFile(m_tempDir.filePath(QStringLiteral("b.flac"))) && ok;
    ok = TestHelpers::createDummyAudioFile(m_tempDir.filePath(QStringLiteral("c.ogg"))) && ok;
    QVERIFY(ok);

    QFile txtFile(m_tempDir.filePath(QStringLiteral("readme.txt")));
    QVERIFY(txtFile.open(QIODevice::WriteOnly));
    txtFile.write("not audio");
    txtFile.close();

    QFile imgFile(m_tempDir.filePath(QStringLiteral("cover.jpg")));
    QVERIFY(imgFile.open(QIODevice::WriteOnly));
    imgFile.write("not an image");
    imgFile.close();

    // Only the supported audio extensions must be reported.
    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path());
    QCOMPARE(found.size(), 3);
    for (const QString &path : found) {
        QVERIFY(Scanner::isSupportedAudioFile(path));
    }
}

void TestScanner::testScanNonExistentDirectory()
{
    Scanner scanner;
    const auto found = scanner.scan(QStringLiteral("/nonexistent/altisidora/path"));
    QCOMPARE(found.size(), 0);
}

void TestScanner::testScanSingleAudioFile()
{
    // A single audio file at the root must be reported, and nothing else.
    QVERIFY(TestHelpers::createDummyAudioFile(
        QDir(m_tempDir.path()).filePath(QStringLiteral("song.mp3"))));

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path());
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first(), m_tempDir.filePath(QStringLiteral("song.mp3")));
}

void TestScanner::testScanRecursive()
{
    // Files in the root directory.
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);

    // Files in nested directories.
    QDir nested(m_tempDir.filePath(QStringLiteral("artist/album")));
    QVERIFY(nested.mkpath(QStringLiteral(".")));
    TestHelpers::createTestMusicDirectory(nested.path(), 3);

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path());
    QCOMPARE(found.size(), 5);
}

void TestScanner::testScanSkipsIgnoredDirectory()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);

    // Songs hidden inside an ignored subdirectory must never be reported.
    QDir ignoredDir(m_tempDir.filePath(QStringLiteral("hidden/collection")));
    QVERIFY(ignoredDir.mkpath(QStringLiteral(".")));
    const QStringList hidden = TestHelpers::createTestMusicDirectory(ignoredDir.path(), 2);
    QCOMPARE(hidden.size(), 2);

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {ignoredDir.path()});
    QCOMPARE(found.size(), 2);

    // An entry equal to one of the hidden files must not appear.
    for (const QString &path : found)
        QVERIFY(!path.startsWith(ignoredDir.path()));
}

void TestScanner::testScanSkipsIgnoredFile()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {files.first()});
    QCOMPARE(found.size(), 2);
    QVERIFY(!found.contains(files.first()));
}

void TestScanner::testIsSupportedAudioFile()
{
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.mp3")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.flac")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.ogg")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.m4a")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.wav")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.opus")));

    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("cover.jpg")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("readme.txt")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("song.mp4")));
}

void TestScanner::testSupportedExtensionsCaseInsensitive()
{
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("SONG.MP3")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("Song.Flac")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("Song.TXT")));
}

QTEST_MAIN(TestScanner)
#include "tst_scanner.moc"