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
    void testScanFindsWebmAndMatroska();
    void testScanSkipsSymlinks();

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
        if (info.isSymLink()) {
            // Unlink the symlink itself; never recurse through it (that
            // would delete content outside the temporary directory).
            QFile::remove(info.absoluteFilePath());
        } else if (info.isDir()) {
            QDir(info.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void TestScanner::testScanEmptyDirectory()
{
    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {});
    QCOMPARE(found.size(), 0);
}

void TestScanner::testScanWithAudioFiles()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {});
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
    const auto found = scanner.scan(m_tempDir.path(), {});
    QCOMPARE(found.size(), 3);
    for (const QString &path : found) {
        QVERIFY(Scanner::isSupportedAudioFile(path));
    }
}

void TestScanner::testScanNonExistentDirectory()
{
    Scanner scanner;
    const auto found = scanner.scan(QStringLiteral("/nonexistent/altisidora/path"), {});
    QCOMPARE(found.size(), 0);
}

void TestScanner::testScanSingleAudioFile()
{
    // A single audio file at the root must be reported, and nothing else.
    QVERIFY(TestHelpers::createDummyAudioFile(
        QDir(m_tempDir.path()).filePath(QStringLiteral("song.mp3"))));

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {});
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
    const auto found = scanner.scan(m_tempDir.path(), {});
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
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.webm")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.mka")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("song.mkv")));

    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("cover.jpg")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("readme.txt")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("song.mp4")));
}

void TestScanner::testSupportedExtensionsCaseInsensitive()
{
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("SONG.MP3")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("Song.Flac")));
    QVERIFY(Scanner::isSupportedAudioFile(QStringLiteral("Song.WebM")));
    QVERIFY(!Scanner::isSupportedAudioFile(QStringLiteral("Song.TXT")));
}

void TestScanner::testScanFindsWebmAndMatroska()
{
    // WebM / Matroska audio must be picked up on scans (the metadata is
    // resolved by TagLib and playback by libmpv).
    QVERIFY(TestHelpers::createDummyAudioFile(
        m_tempDir.filePath(QStringLiteral("clip.webm"))));
    QVERIFY(TestHelpers::createDummyAudioFile(
        m_tempDir.filePath(QStringLiteral("album.mka"))));
    QVERIFY(TestHelpers::createDummyAudioFile(
        m_tempDir.filePath(QStringLiteral("video.mkv"))));

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {});
    QCOMPARE(found.size(), 3);
    for (const QString &path : found) {
        QVERIFY(Scanner::isSupportedAudioFile(path));
    }
}

void TestScanner::testScanSkipsSymlinks()
{
    // The walk relies on QDir::NoSymLinks: symlinked files and directories
    // must be neither reported nor descended into. This pins that contract
    // (no content leaks in from outside the scanned tree, no recursion
    // through a link) so the redundant per-entry check cannot creep back.
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 1); // one real song

    QTemporaryDir outside;
    QVERIFY(outside.isValid());
    const QStringList outsideFiles =
        TestHelpers::createTestMusicDirectory(outside.path(), 1);
    QCOMPARE(outsideFiles.size(), 1);

    // Symlink to an external audio file: must not be reported.
    if (!QFile::link(outsideFiles.first(),
                     m_tempDir.filePath(QStringLiteral("linked.mp3")))) {
        QSKIP("filesystem does not support symlinks");
    }
    // Symlink to an external directory: must not be descended into.
    QVERIFY(QFile::link(outside.path(),
                        m_tempDir.filePath(QStringLiteral("linkeddir"))));

    Scanner scanner;
    const auto found = scanner.scan(m_tempDir.path(), {});
    // Only the real song inside the scanned tree.
    QCOMPARE(found.size(), 1);
}

QTEST_MAIN(TestScanner)
#include "tst_scanner.moc"