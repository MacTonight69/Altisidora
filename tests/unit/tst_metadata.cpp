#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <functional>

#include "../TestHelpers.h"
#include "library/MetadataReader.h"

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>

namespace {

// Minimal valid 1x1 PNGs (69 bytes each, red and green pixels): small
// enough to inline, real enough that image decoders accept them. The
// different pixel colours make front/back cover selection observable.
const char kRedPngHex[] =
    "89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de"
    "0000000c4944415478da63f8cfc0000003010100f70341430000000049454e44ae426082";
const char kGreenPngHex[] =
    "89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de"
    "0000000c4944415478da6360f8cf00000202010045f452d40000000049454e44ae426082";

// Attaches `art` to the tag (downcast to its real ID3v2 type, as filled by
// writeMpegFile) as a picture frame of the given ID3v2 picture type.
void attachPicture(TagLib::Tag *tag,
                   TagLib::ID3v2::AttachedPictureFrame::Type type,
                   const QByteArray &art)
{
    auto *pic = new TagLib::ID3v2::AttachedPictureFrame();
    pic->setType(type);
    pic->setMimeType("image/png");
    pic->setPicture(TagLib::ByteVector(art.constData(),
                                       static_cast<unsigned int>(art.size())));
    static_cast<TagLib::ID3v2::Tag *>(tag)->addFrame(pic);
}

} // namespace

/**
 * @brief Tests for MetadataReader (src/library/MetadataReader.cpp).
 *
 * The reader is exercised with real audio data: a valid minimal MPEG-1
 * Layer III stream (417-byte frames at 128 kbps / 44.1 kHz) is generated on
 * the fly and, where needed, tagged with a real ID3v2 tag written by TagLib.
 */
class TestMetadata : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testReadTitle();
    void testReadArtist();
    void testReadAlbum();
    void testReadGenre();
    void testReadYear();
    void testReadTrackNumber();
    void testReadDuration();
    void testReadFallbackTitleFromFileName();
    void testReadFallbackTitleForUnparseableSupportedFile();
    void testReadUtf8JapaneseTags();

    // Embedded artwork extraction
    void testReadArtworkFromFrontCover();
    void testReadArtworkPrefersFrontCover();
    void testReadArtworkIdempotent();

    // Error handling tests
    void testReadNonExistentFile();
    void testReadEmptyFile();
    void testReadCorruptFile();
    void testReadNonAudioFile();

private:
    // Writes a real 40-frame MPEG stream to filePath, optionally adding an
    // ID3v2 tag filled by `fill` (called with the mutable tag).
    bool writeMpegFile(const QString &filePath,
                       const std::function<void(TagLib::Tag *)> &fill = {});

    QTemporaryDir m_tempDir;
};

void TestMetadata::initTestCase()
{
    // Artwork extraction persists its cache under AppDataLocation; test
    // mode keeps those files out of the developer's real data directory.
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_tempDir.isValid());
}

bool TestMetadata::writeMpegFile(const QString &filePath,
                                 const std::function<void(TagLib::Tag *)> &fill)
{
    // MPEG-1 Layer III, 128 kbps, 44.1 kHz, no padding: frame size is 417.
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray header("\xFF\xFB\x90\x00", 4);
    const int frameSize = 417;
    for (int i = 0; i < 40; ++i) {
        file.write(header);
        file.write(QByteArray(frameSize - header.size(), '\0'));
    }
    file.close();

    if (!fill) {
        return QFile::exists(filePath);
    }

    TagLib::MPEG::File mf(filePath.toStdString().c_str());
    if (!mf.isValid()) {
        return false;
    }
    TagLib::ID3v2::Tag *tag = mf.ID3v2Tag(true);
    if (tag == nullptr) {
        return false;
    }
    fill(tag);
    return mf.save();
}

void TestMetadata::testReadTitle()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("title.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setTitle(TagLib::String("Test Title", TagLib::String::UTF8));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, QStringLiteral("Test Title"));
    QCOMPARE(song.path, filePath);
}

void TestMetadata::testReadArtist()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artist.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setArtist(TagLib::String("Test Artist", TagLib::String::UTF8));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.artist, QStringLiteral("Test Artist"));
}

void TestMetadata::testReadAlbum()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("album.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setAlbum(TagLib::String("Test Album", TagLib::String::UTF8));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.album, QStringLiteral("Test Album"));
}

void TestMetadata::testReadGenre()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("genre.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setGenre(TagLib::String("Rock", TagLib::String::UTF8));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.genre, QStringLiteral("Rock"));
}

void TestMetadata::testReadYear()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("year.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setYear(2019);
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.year, 2019);
}

void TestMetadata::testReadTrackNumber()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("track.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setTrack(7);
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.trackNumber, 7);
}

void TestMetadata::testReadDuration()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("duration.mp3"));
    QVERIFY(writeMpegFile(filePath));

    const Song song = MetadataReader().read(filePath);

    // 40 frames * 1152 samples / 44100 Hz ~= 1.04 s -> a real duration.
    QVERIFY(song.duration > 0);
}

void TestMetadata::testReadFallbackTitleFromFileName()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("untagged_song.mp3"));
    QVERIFY(writeMpegFile(filePath));

    // No ID3v2 tag: the reader falls back to the file base name.
    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, QStringLiteral("untagged_song"));
    QVERIFY(song.artist.isEmpty());
    QVERIFY(song.album.isEmpty());
    // And with no embedded art either, there is no cached artwork file.
    QVERIFY(song.artworkPath.isEmpty());
}

void TestMetadata::testReadFallbackTitleForUnparseableSupportedFile()
{
    // A "WebM" the TagLib parser cannot recognise: the FileRef is null,
    // but because the scanner accepts the extension the song still keeps
    // a readable title instead of showing nothing.
    const QString filePath = m_tempDir.filePath(QStringLiteral("weird.webm"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(64, '\xAB')); // garbage, not EBML
    file.close();

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.path, filePath);
    QCOMPARE(song.title, QStringLiteral("weird"));
    QCOMPARE(song.duration, 0);
}

// Regression test: TagLib::String::toCString() returned garbage for UTF-8
// ID3v2 text (the returned buffer started with the frame's encoding byte 0x03
// followed by uninitialised memory), so Japanese/UTF-8 tags were stored as
// '^C\uFFFDn\u01A3...' in the database. MetadataReader must decode tags via
// to8Bit(true) (which yields correct UTF-8) instead of toCString().
void TestMetadata::testReadUtf8JapaneseTags()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("japanese.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        tag->setTitle(TagLib::String("七人のティータイム", TagLib::String::UTF8));
        tag->setArtist(TagLib::String("ShibayanRecords", TagLib::String::UTF8));
        tag->setAlbum(TagLib::String("TOHO BOSSA NOVA 12", TagLib::String::UTF8));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, QStringLiteral("七人のティータイム"));
    QCOMPARE(song.artist, QStringLiteral("ShibayanRecords"));
    QCOMPARE(song.album, QStringLiteral("TOHO BOSSA NOVA 12"));
    QVERIFY(!song.title.contains(QChar(0xFFFD))); // no replacement chars
    QVERIFY(!song.title.contains(QChar(0x03)));   // no raw encoding byte
}

// A front-cover picture must be extracted to the artwork cache: the song
// carries the absolute cache path, the file exists and its bytes are
// identical to the embedded picture (byte-perfect round trip).
void TestMetadata::testReadArtworkFromFrontCover()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artwork.mp3"));
    const QByteArray art = QByteArray::fromHex(kRedPngHex);
    QVERIFY(writeMpegFile(filePath, [&art](TagLib::Tag *tag) {
        attachPicture(tag, TagLib::ID3v2::AttachedPictureFrame::FrontCover, art);
    }));

    const Song song = MetadataReader().read(filePath);
    QVERIFY(!song.artworkPath.isEmpty());
    // The extension comes from the declared MIME type.
    QVERIFY(song.artworkPath.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(song.artworkPath));

    QFile cached(song.artworkPath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), art);
}

// When a back cover is added first and a front cover second, the front
// cover must win regardless of frame order.
void TestMetadata::testReadArtworkPrefersFrontCover()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artwork_order.mp3"));
    const QByteArray back = QByteArray::fromHex(kRedPngHex);
    const QByteArray front = QByteArray::fromHex(kGreenPngHex);
    QVERIFY(writeMpegFile(filePath, [&back, &front](TagLib::Tag *tag) {
        attachPicture(tag, TagLib::ID3v2::AttachedPictureFrame::BackCover, back);
        attachPicture(tag, TagLib::ID3v2::AttachedPictureFrame::FrontCover, front);
    }));

    const Song song = MetadataReader().read(filePath);
    QVERIFY(!song.artworkPath.isEmpty());
    QFile cached(song.artworkPath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), front);
}

// The cache name is derived from the picture content, so re-reading the
// same file returns the same path instead of writing a duplicate.
void TestMetadata::testReadArtworkIdempotent()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artwork_twice.mp3"));
    const QByteArray art = QByteArray::fromHex(kGreenPngHex);
    QVERIFY(writeMpegFile(filePath, [&art](TagLib::Tag *tag) {
        attachPicture(tag, TagLib::ID3v2::AttachedPictureFrame::FrontCover, art);
    }));

    const Song first = MetadataReader().read(filePath);
    const Song second = MetadataReader().read(filePath);
    QVERIFY(!first.artworkPath.isEmpty());
    QCOMPARE(second.artworkPath, first.artworkPath);
    QVERIFY(QFile::exists(first.artworkPath));
}

void TestMetadata::testReadNonExistentFile()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("missing.mp3"));
    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.path, filePath);
    QVERIFY(song.title.isEmpty());
}

void TestMetadata::testReadEmptyFile()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("empty.mp3"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    // TagLib resolves ".mp3" by extension, so the FileRef is not null even
    // with no content; the empty tag then triggers the base-name fallback.
    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.path, filePath);
    QCOMPARE(song.title, QStringLiteral("empty"));
    QCOMPARE(song.duration, 0);
}

void TestMetadata::testReadCorruptFile()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("corrupt.mp3"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(100, '\xFF')); // garbage
    file.close();

    // Same as the empty file: the extension keeps the FileRef alive, the
    // tag is empty and the fallback makes the song identifiable by name.
    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.path, filePath);
    QCOMPARE(song.title, QStringLiteral("corrupt"));
    QCOMPARE(song.duration, 0);
}

void TestMetadata::testReadNonAudioFile()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("notes.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("This is a text file, not an audio file");
    file.close();

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.path, filePath);
    QVERIFY(song.title.isEmpty());
}

QTEST_MAIN(TestMetadata)
#include "tst_metadata.moc"