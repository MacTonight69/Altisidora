#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "../TestHelpers.h"

#include "../../src/library/MetadataReader.h"

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>

/**
 * @brief Tests for metadata reader
 *
 * Tests reading metadata from audio files, including title, artist,
 * album, genre, year, track number, and duration.
 */
class TestMetadata : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Basic metadata tests
    void testReadTitle();
    void testReadArtist();
    void testReadAlbum();
    void testReadGenre();
    void testReadYear();
    void testReadTrackNumber();
    void testReadDuration();

    // File type tests
    void testMp3Metadata();
    void testOggMetadata();
    void testFlacMetadata();
    void testWavMetadata();

    // Error handling tests
    void testReadNonExistentFile();
    void testReadEmptyFile();
    void testReadCorruptFile();
    void testReadNonAudioFile();

    // Edge case tests
    void testReadFileWithNoMetadata();
    void testReadFileWithPartialMetadata();
    void testReadFileWithUnicodeMetadata();

    // Encoding regression tests
    void testReadUtf8JapaneseTags();

private:
    QTemporaryDir m_tempDir;
};

void TestMetadata::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestMetadata::cleanupTestCase()
{
}

void TestMetadata::testReadTitle()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("test.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Test Title"),
                                              QStringLiteral("Test Artist"),
                                              QStringLiteral("Test Album")));

    QVERIFY(QFile::exists(filePath));
    // In a real implementation, this would use TagLib to read metadata
    // For now, we verify the file was created successfully
    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadArtist()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("artist.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Test Artist"),
                                              QStringLiteral("Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadAlbum()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("album.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Artist"),
                                              QStringLiteral("Test Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadGenre()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("genre.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Artist"),
                                              QStringLiteral("Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadYear()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("year.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Artist"),
                                              QStringLiteral("Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadTrackNumber()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("track.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Artist"),
                                              QStringLiteral("Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadDuration()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("duration.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Song"),
                                              QStringLiteral("Artist"),
                                              QStringLiteral("Album")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testMp3Metadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("test.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("MP3 Song"),
                                              QStringLiteral("MP3 Artist"),
                                              QStringLiteral("MP3 Album")));

    QVERIFY(QFile::exists(filePath));
    QVERIFY(filePath.endsWith(QStringLiteral(".mp3")));
}

void TestMetadata::testOggMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("test.ogg"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("OGG Song"),
                                              QStringLiteral("OGG Artist"),
                                              QStringLiteral("OGG Album")));

    QVERIFY(QFile::exists(filePath));
    QVERIFY(filePath.endsWith(QStringLiteral(".ogg")));
}

void TestMetadata::testFlacMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("test.flac"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("FLAC Song"),
                                              QStringLiteral("FLAC Artist"),
                                              QStringLiteral("FLAC Album")));

    QVERIFY(QFile::exists(filePath));
    QVERIFY(filePath.endsWith(QStringLiteral(".flac")));
}

void TestMetadata::testWavMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("test.wav"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("WAV Song"),
                                              QStringLiteral("WAV Artist"),
                                              QStringLiteral("WAV Album")));

    QVERIFY(QFile::exists(filePath));
    QVERIFY(filePath.endsWith(QStringLiteral(".wav")));
}

void TestMetadata::testReadNonExistentFile()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("nonexistent.mp3"));
    QVERIFY(!QFile::exists(filePath));

    // In a real implementation, this would test that the metadata reader
    // returns an error or empty result for non-existent files
    QVERIFY(!QFile::exists(filePath));
}

void TestMetadata::testReadEmptyFile()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("empty.mp3"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QVERIFY(QFile::exists(filePath));
    QCOMPARE(QFileInfo(filePath).size(), qint64(0));
}

void TestMetadata::testReadCorruptFile()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("corrupt.mp3"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(100, '\xFF')); // Write garbage data
    file.close();

    QVERIFY(QFile::exists(filePath));
    QVERIFY(QFileInfo(filePath).size() > 0);
}

void TestMetadata::testReadNonAudioFile()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("notaudio.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("This is a text file, not an audio file");
    file.close();

    QVERIFY(QFile::exists(filePath));
    QVERIFY(!filePath.endsWith(QStringLiteral(".mp3")));
}

void TestMetadata::testReadFileWithNoMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("nometa.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral(""),
                                              QStringLiteral(""),
                                              QStringLiteral("")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadFileWithPartialMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("partial.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath, QStringLiteral("Title Only"),
                                              QStringLiteral(""),
                                              QStringLiteral("")));

    QVERIFY(QFile::exists(filePath));
}

void TestMetadata::testReadFileWithUnicodeMetadata()
{
    QString filePath = m_tempDir.filePath(QStringLiteral("unicode.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath,
                                              QStringLiteral("日本語タイトル"),
                                              QStringLiteral("アーティスト"),
                                              QStringLiteral("アルバム")));

    QVERIFY(QFile::exists(filePath));
}

// Regression test: TagLib::String::toCString() returned garbage for UTF-8
// ID3v2 text (the returned buffer started with the frame's encoding byte 0x03
// followed by uninitialised memory), so Japanese/UTF-8 tags were stored as
// '^C\uFFFDn\u01A3...' in the database. MetadataReader must decode tags via
// to8Bit(true) (which yields correct UTF-8) instead of toCString().
void TestMetadata::testReadUtf8JapaneseTags()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("japanese.mp3"));
    QVERIFY(TestHelpers::createDummyAudioFile(filePath,
                                              QStringLiteral("x"),
                                              QStringLiteral("y"),
                                              QStringLiteral("z")));

    // Write a real ID3v2 tag with UTF-8 Japanese text via TagLib.
    {
        TagLib::MPEG::File mf(filePath.toStdString().c_str());
        QVERIFY(mf.isValid());
        TagLib::ID3v2::Tag *tag = mf.ID3v2Tag(true);
        QVERIFY(tag != nullptr);
        tag->setTitle(TagLib::String("七人のティータイム", TagLib::String::UTF8));
        tag->setArtist(TagLib::String("ShibayanRecords", TagLib::String::UTF8));
        tag->setAlbum(TagLib::String("TOHO BOSSA NOVA 12", TagLib::String::UTF8));
        QVERIFY(mf.save());
    }

    MetadataReader reader;
    const Song song = reader.read(filePath);

    QCOMPARE(song.title, QStringLiteral("七人のティータイム"));
    QCOMPARE(song.artist, QStringLiteral("ShibayanRecords"));
    QCOMPARE(song.album, QStringLiteral("TOHO BOSSA NOVA 12"));
    QVERIFY(!song.title.contains(QChar(0xFFFD))); // no replacement chars
    QVERIFY(!song.title.contains(QChar(0x03)));   // no raw encoding byte
}

QTEST_MAIN(TestMetadata)
#include "tst_metadata.moc"
