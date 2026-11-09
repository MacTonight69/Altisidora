#include <QtTest>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <functional>

#include "library/MetadataReader.h"
#include "../TestHelpers.h"

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>

namespace {

// Minimal valid 1x1 green PNG (69 bytes): small enough to inline, real
// enough that image decoders accept it. The red counterpart lives in
// TestHelpers and the different pixel colours make front/back cover
// selection observable.
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

// Writes `bytes` into an ID3v2 text frame (e.g. "TIT2") whose encoding
// byte declares ISO-8859-1. Fed with raw Shift-JIS bytes this reproduces
// the legacy mislabeled tags; fed with genuine Latin-1 bytes it
// reproduces real accented tags. Because the TagLib::String is built
// from the bytes as Latin-1, every char stays <= U+00FF and the frame
// really renders with the Latin-1 encoding byte. addFrame() takes
// ownership of the frame (TagLib contract), hence the heap allocation -
// same pattern as attachPicture() above.
void addLatin1TextFrame(TagLib::Tag *tag, const char *frameId,
                        const QByteArray &bytes)
{
    auto *frame = new TagLib::ID3v2::TextIdentificationFrame(
        TagLib::ByteVector(frameId), TagLib::String::Latin1);
    frame->setText(TagLib::String(
        TagLib::ByteVector(bytes.constData(), static_cast<unsigned int>(bytes.size())),
        TagLib::String::Latin1));
    frame->setTextEncoding(TagLib::String::Latin1);
    static_cast<TagLib::ID3v2::Tag *>(tag)->addFrame(frame);
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

    // Legacy-encoding fallback: Shift-JIS bytes mislabeled as ISO-8859-1.
    void testReadMislabeledLatin1ShiftJisTags();
    void testReadLatin1AccentedTagsNotConverted();
    void testReadCp1252SmartQuotesNotConverted();
    void testShiftJisMojibakeCorpus_data();
    void testShiftJisMojibakeCorpus();

    // Embedded artwork extraction
    void testReadArtworkFromFrontCover();
    void testReadArtworkPrefersFrontCover();
    void testReadArtworkIdempotent();
    void testReadArtworkFromFlac();

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

    // Writes a minimal valid FLAC stream (STREAMINFO only) and attaches a
    // front-cover picture, exercising the FLAC branch of artwork extraction.
    bool writeFlacFile(const QString &filePath, const QByteArray &art);

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

bool TestMetadata::writeFlacFile(const QString &filePath, const QByteArray &art)
{
    // Minimal FLAC stream: the "fLaC" marker plus a single STREAMINFO
    // block. No audio frames are needed to read tags or pictures.
    QByteArray data("fLaC", 4);
    data.append('\x80'); // last metadata block, type STREAMINFO
    data.append('\x00');
    data.append('\x00');
    data.append('\x22'); // 34-byte payload
    data.append('\x10'); data.append('\x00'); // min block size 4096
    data.append('\x10'); data.append('\x00'); // max block size 4096
    data.append(QByteArray(6, '\0'));         // min/max frame size
    // 20-bit sample rate 44100, 3-bit channels-1 = 1, 5-bit bps-1 = 15,
    // 36-bit total samples = 0.
    const quint64 packed = (quint64(44100) << 44) | (quint64(1) << 41)
                         | (quint64(15) << 36);
    for (int i = 7; i >= 0; --i)
        data.append(static_cast<char>((packed >> (i * 8)) & 0xFF));
    data.append(QByteArray(16, '\0')); // MD5 signature

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()) {
        return false;
    }
    file.close();

    TagLib::FLAC::File flac(filePath.toStdString().c_str());
    if (!flac.isValid()) {
        return false;
    }
    auto *picture = new TagLib::FLAC::Picture();
    picture->setType(TagLib::FLAC::Picture::FrontCover);
    picture->setMimeType("image/png");
    picture->setData(TagLib::ByteVector(art.constData(),
                                        static_cast<unsigned int>(art.size())));
    flac.addPicture(picture);
    return flac.save();
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

// A legacy (mostly Japanese-ripper-era) tagger wrote Shift-JIS bytes while
// declaring the frame ISO-8859-1 (encoding byte 0). TagLib maps each byte
// to a U+00xx code point, producing pure mojibake; MetadataReader must
// detect the C1 lead-byte signature and re-decode the original bytes as
// Shift-JIS, restoring the Japanese title exactly.
void TestMetadata::testReadMislabeledLatin1ShiftJisTags()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("sjis_mislabeled.mp3"));
    // Shift-JIS encoding of "七人のティータイム"; every double byte is one
    // character (8e b5 = 七, 82 cc = の, 81 5b = ー, 83 80 = ム).
    const QByteArray sjis =
        QByteArray::fromHex("8eb5906c82cc83658342815b835e83438380");
    QVERIFY(writeMpegFile(filePath, [&sjis](TagLib::Tag *tag) {
        addLatin1TextFrame(tag, "TIT2", sjis);
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, QStringLiteral("七人のティータイム"));
    QVERIFY(!song.title.contains(QChar(0x001A))); // no SJIS substitution char
    QVERIFY(!song.title.contains(QChar(0xFFFD))); // no Unicode replacement char
}

// False-positive pin that must never break: genuine accented Latin-1
// tags come back byte-identical. Accented characters live in
// U+00A0-U+00FF, never in the C1 range U+0081-U+009F, so the Shift-JIS
// fallback must not fire for them.
void TestMetadata::testReadLatin1AccentedTagsNotConverted()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("latin1_accents.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        addLatin1TextFrame(tag, "TIT2", QByteArrayLiteral("Beyonc\xe9"));
        addLatin1TextFrame(tag, "TPE1", QByteArrayLiteral("Ch\xe2teau"));
        addLatin1TextFrame(tag, "TALB", QByteArrayLiteral("caf\xe9"));
        addLatin1TextFrame(tag, "TCON", QByteArrayLiteral("se\xf1or"));
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, QString::fromLatin1("Beyonc\xe9"));
    QCOMPARE(song.artist, QString::fromLatin1("Ch\xe2teau"));
    QCOMPARE(song.album, QString::fromLatin1("caf\xe9"));
    QCOMPARE(song.genre, QString::fromLatin1("se\xf1or"));

    // The reader exposes only four text fields, so the remaining accents
    // (including the two U+00C0-U+00FF extremes) go into a second file.
    const QString filePath2 = m_tempDir.filePath(QStringLiteral("latin1_accents2.mp3"));
    QVERIFY(writeMpegFile(filePath2, [](TagLib::Tag *tag) {
        addLatin1TextFrame(tag, "TIT2", QByteArrayLiteral("Gr\xf6\xdf" "e"));
        addLatin1TextFrame(tag, "TPE1", QByteArrayLiteral("\xa1\xa2\xa3"));
        addLatin1TextFrame(tag, "TCON", QByteArrayLiteral("\xc0\xc9\xce\xd5\xdc"));
    }));

    const Song song2 = MetadataReader().read(filePath2);
    QCOMPARE(song2.title, QString::fromLatin1("Gr\xf6\xdf" "e"));
    QCOMPARE(song2.artist, QString::fromLatin1("\xa1\xa2\xa3"));
    QCOMPARE(song2.genre, QString::fromLatin1("\xc0\xc9\xce\xd5\xdc"));
}

// A CP1252 smart apostrophe (byte 0x92) becomes the C1 control U+0092 when
// the Latin-1 frame is read back. It carries a C1 char, but a single high
// byte inside a 10-char title is sparse, so the density gate must leave it
// untouched instead of mangling the title.
void TestMetadata::testReadCp1252SmartQuotesNotConverted()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("cp1252_quotes.mp3"));
    QVERIFY(writeMpegFile(filePath, [](TagLib::Tag *tag) {
        addLatin1TextFrame(tag, "TIT2", QByteArrayLiteral("Don\x92t stop"));
    }));

    const Song song = MetadataReader().read(filePath);
    // Un-converted on purpose: the U+0092 mojibake apostrophe is preserved.
    QCOMPARE(song.title, QString::fromLatin1("Don\x92t stop"));
}

// Data-driven corpus for the Shift-JIS fallback: each row writes `bytes`
// as a Latin-1 TIT2 frame and expects `expected` back. Positive rows are
// real Shift-JIS titles (hand-verified byte encodings) that must be
// recovered; negative rows must round-trip byte-identically, pinning one
// gate each: density (sparse CP1252), C1 signature (accents / half-width
// kana), full-width-only acceptance (ideographic spaces), and the
// re-encode round trip (trailing incomplete lead byte).
void TestMetadata::testShiftJisMojibakeCorpus_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QString>("expected");

    // Real Shift-JIS text mislabeled as ISO-8859-1 -> recovered.
    QTest::newRow("mislabeledShiftJisTitle")
        << QByteArray::fromHex("8eb5906c82cc83658342815b835e83438380")
        << QStringLiteral("七人のティータイム");
    QTest::newRow("mislabeledShiftJisKanjiKatakana")
        << QByteArray::fromHex("8e638d9382c893568e6782cc8365815b835b")
        << QStringLiteral("残酷な天使のテーゼ");
    QTest::newRow("mislabeledShiftJisHiragana")
        << QByteArray::fromHex("82b382ad82e782b382ad82e7")
        << QStringLiteral("さくらさくら");

    // Pure ASCII: nothing to recover (gate 2).
    QTest::newRow("pureAscii")
        << QByteArrayLiteral("Regular Title")
        << QStringLiteral("Regular Title");

    // Accented Latin-1: no C1 controls, no Shift-JIS lead signature (gate 3).
    QTest::newRow("latin1Accents")
        << QByteArrayLiteral("caf\xe9")
        << QString::fromLatin1("caf\xe9");

    // Lone CP1252 apostrophe: sparse high bytes (gate 4).
    QTest::newRow("cp1252Apostrophe")
        << QByteArrayLiteral("Don\x92t stop")
        << QString::fromLatin1("Don\x92t stop");

    // Ideographic space (81 40 = U+3000) followed by half-width katakana
    // (B1 B2 B3 = U+FF71-FF73): passes the C1 and density gates and decodes
    // without error, but acceptance requires FULL-WIDTH Japanese script
    // (U+3000 is punctuation, U+FF71-73 are the half-width block), so the
    // title must stay unchanged. This row exercises the acceptance gate's
    // reject branch — a pure \xB1\xB2\xB3 title would die earlier, at the
    // C1 gate, and never reach it.
    QTest::newRow("halfWidthKanaBytes")
        << QByteArrayLiteral("\x81\x40\xb1\xb2\xb3")
        << QString::fromLatin1("\x81\x40\xb1\xb2\xb3");

    // Four ideographic spaces (81 40 = U+3000) pass the C1 and density gates
    // (four high bytes, half the string) and decode without error, yet
    // contain no Japanese script — the acceptance gate must reject them.
    const QByteArray fullWidthSpaces = QByteArray::fromHex("8140814081408140");
    QTest::newRow("fullWidthSpaceOnly")
        << fullWidthSpaces << QString::fromLatin1(fullWidthSpaces);

    // A trailing incomplete lead byte (0x90) makes the decoder silently
    // buffer it with hasError() == false; only the byte-exact re-encode
    // comparison can reject the conversion.
    const QByteArray trailingLead = QByteArray::fromHex("8eb5906c82cc836590");
    QTest::newRow("trailingIncompleteLeadByte")
        << trailingLead << QString::fromLatin1(trailingLead);
}

void TestMetadata::testShiftJisMojibakeCorpus()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QString, expected);

    const QString filePath = m_tempDir.filePath(QStringLiteral("corpus.mp3"));
    QVERIFY(writeMpegFile(filePath, [&bytes](TagLib::Tag *tag) {
        addLatin1TextFrame(tag, "TIT2", bytes);
    }));

    const Song song = MetadataReader().read(filePath);
    QCOMPARE(song.title, expected);
}

// A front-cover picture must be extracted to the artwork cache: the song
// carries the absolute cache path, the file exists and its bytes are
// identical to the embedded picture (byte-perfect round trip).
void TestMetadata::testReadArtworkFromFrontCover()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artwork.mp3"));
    const QByteArray art = QByteArray::fromHex(TestHelpers::kRedPngHex);
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
    const QByteArray back = QByteArray::fromHex(TestHelpers::kRedPngHex);
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

// FLAC stores pictures in PICTURE metadata blocks rather than ID3v2
// frames; TagLib 1.x and 2.x expose them through different APIs, so this
// covers the format-specific compatibility path.
void TestMetadata::testReadArtworkFromFlac()
{
    const QString filePath = m_tempDir.filePath(QStringLiteral("artwork.flac"));
    const QByteArray art = QByteArray::fromHex(TestHelpers::kRedPngHex);
    QVERIFY(writeFlacFile(filePath, art));

    const Song song = MetadataReader().read(filePath);
    QVERIFY(!song.artworkPath.isEmpty());
    QVERIFY(song.artworkPath.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(song.artworkPath));

    QFile cached(song.artworkPath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), art);
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