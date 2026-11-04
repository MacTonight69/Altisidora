#include "MetadataReader.h"

#include "Scanner.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStringEncoder>

namespace {

// Recovers tags that a legacy (mostly Japanese-ripper-era) tagger wrote as
// Shift-JIS bytes while declaring the ID3v2 frame ISO-8859-1 (encoding
// byte 0). TagLib then maps every raw byte to the Unicode code point
// U+00xx, e.g. "8e b5 90 6c" (七人) becomes U+008E U+00B5 U+0090 U+006C.
// The original bytes are still fully recoverable — every char is <= U+00FF
// at that point — so we take them back with toLatin1() and re-decode them
// as Shift-JIS. Rejection gates, in order (any failure returns the text
// untouched):
//
//  1. any char above U+00FF: a genuine UTF-8/UTF-16 tag, nothing to recover;
//  2. no char above U+007F: pure ASCII;
//  3. no C1 control (U+0081-U+009F): Shift-JIS double-byte lead bytes for
//     kana and common kanji are 0x81-0x9F, so real mojibake ALWAYS carries
//     at least one C1 char, while printed Latin-1 text (French/German/
//     Spanish accents, "¡¢£") never does — this is the discriminator that
//     keeps genuine Latin-1 tags safe;
//  4. fewer than 4 high bytes while the string is not ALL high bytes: a
//     sparse CP1252 smart quote/em dash, not a double-byte CJK sequence.
//
// The re-decode must then clear three more hurdles:
//  - decoder.hasError(): invalid byte pairs substitute U+001A (not the
//    usual U+FFFD) and raise the error flag;
//  - the result must contain FULL-WIDTH Japanese (hiragana, katakana or
//    CJK unified ideographs): single bytes 0xA1-0xDF also decode
//    "validly" as half-width katakana (U+FF61-U+FF9F), which would let
//    non-Japanese garbage through the gates above;
//  - re-encoding must reproduce the original bytes EXACTLY. This gate is
//    mandatory because a trailing incomplete lead byte is silently buffered
//    by the decoder (output is just truncated, hasError() stays false);
//    only the byte comparison catches it.
//
// The converters are constructed per call: this only runs for strings that
// passed all four gates (rare), and their cost is microseconds against the
// file I/O of a library scan, so caching is not worth the complexity.
QString fixShiftJisMojibake(const QString &text)
{
    const qsizetype total = text.size();
    if (total == 0)
        return text;

    qsizetype highCount = 0;
    bool hasC1 = false;
    for (const QChar ch : text) {
        const char16_t u = ch.unicode();
        if (u > 0x00FF)
            return text;                    // 1. real UTF-8/UTF-16 text
        if (u >= 0x0080)
            ++highCount;
        if (u >= 0x0081 && u <= 0x009F)     // 3. Shift-JIS double-byte lead
            hasC1 = true;
    }
    if (highCount == 0)
        return text;                        // 2. pure ASCII
    if (!hasC1)
        return text;                        // 3. accented Latin-1: no SJIS signature
    if (highCount < 4 && highCount != total)
        return text;                        // 4. sparse CP1252 quotes etc.

    // Lossless here: every char is <= U+00FF.
    const QByteArray bytes = text.toLatin1();

    QStringDecoder decoder("Shift-JIS");
    const QString decoded = decoder.decode(bytes);
    if (decoder.hasError() || decoded.isEmpty())
        return text;

    bool hasFullWidthJapanese = false;
    for (const QChar ch : decoded) {
        const char16_t u = ch.unicode();
        if ((u >= 0x3040 && u <= 0x309F)        // Hiragana
            || (u >= 0x30A0 && u <= 0x30FF)     // Katakana
            || (u >= 0x4E00 && u <= 0x9FFF)) {   // CJK Unified Ideographs
            hasFullWidthJapanese = true;
            break;
        }
    }
    if (!hasFullWidthJapanese)
        return text;                        // half-width kana is not acceptance proof

    QStringEncoder encoder("Shift-JIS");
    const QByteArray reencoded = encoder.encode(decoded);
    if (encoder.hasError() || reencoded != bytes)
        return text;                        // catches silently buffered trailing leads
    return decoded;
}

// Converts a TagLib::String to a QString using its UTF-8 representation.
// NOTE: TagLib::String::toCString() is unreliable in TagLib 2.x (it can
// return a pointer to a not-yet-initialised internal buffer, i.e. garbage
// that even starts with the ID3v2 text-encoding byte). to8Bit(true) always
// returns correct UTF-8 bytes, so we decode from that buffer by size.
// This is the single choke point for every text tag (title/artist/album/
// genre, plus the ASCII-only artwork MIME string), so the Shift-JIS
// fallback is applied here as the last step.
QString tagStringToQString(const TagLib::String &str)
{
    if (str.isEmpty())
        return {};
    const std::string utf8 = str.to8Bit(true);
    return fixShiftJisMojibake(
        QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size())));
}

// Extension for the declared MIME type (expected lower-case); empty when
// the type is unknown or not an image.
QString imageExtensionFromMime(const QString &mime)
{
    if (mime == "image/jpeg" || mime == "image/jpg") return "jpg";
    if (mime == "image/png")  return "png";
    if (mime == "image/webp") return "webp";
    if (mime == "image/gif")  return "gif";
    if (mime == "image/bmp")  return "bmp";
    return {};
}

// Extension sniffed from the leading magic bytes; empty when they match
// no known image format. Shared by the scan (fallback for tags that lie
// about or omit the MIME type) and by cacheArtwork() (the only check a
// user-picked file gets).
QString imageExtensionFromMagicBytes(const QByteArray &raw)
{
    if (raw.size() >= 4 && (unsigned char)raw[0] == 0x89
        && raw[1] == 'P' && raw[2] == 'N' && raw[3] == 'G') {
        return "png";
    }
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xFF
        && (unsigned char)raw[1] == 0xD8 && (unsigned char)raw[2] == 0xFF) {
        return "jpg";
    }
    if (raw.startsWith("GIF87a") || raw.startsWith("GIF89a"))
        return "gif";
    if (raw.size() >= 2 && raw[1] == 'M' && raw[0] == 'B')
        return "bmp";
    if (raw.size() >= 12 && raw.startsWith("RIFF") && raw.mid(8, 4) == "WEBP")
        return "webp";
    return {};
}

// Writes image bytes into the artwork cache under a content-addressed
// name: identical art across an album's tracks collides into ONE file,
// and re-saving is idempotent. Shared by the scan-time extraction and by
// cacheArtwork(), so hand-picked covers deduplicate with extracted ones.
// Returns the absolute cache file path, or an empty string on failure.
QString cacheArtworkBytes(const QByteArray &raw, const QString &ext)
{
    if (raw.isEmpty() || ext.isEmpty())
        return {};

    const QString cacheDir = QStandardPaths::writableLocation(
                                 QStandardPaths::AppDataLocation) + "/artwork";
    if (!QDir().mkpath(cacheDir))
        return {};

    const QString name = QString::fromLatin1(QCryptographicHash::hash(
        raw, QCryptographicHash::Sha256).toHex().left(32)) + '.' + ext;
    const QString outPath = cacheDir + '/' + name;
    if (QFileInfo::exists(outPath))
        return outPath;

    QSaveFile f(outPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(raw) != raw.size() || !f.commit())
        return {};
    return outPath;
}

// Extracts the embedded front-cover picture (falls back to the first
// picture found) and persists it under a content-hash name. Returns the
// absolute cache file path, or an empty string when there is no artwork.
QString extractArtwork(const TagLib::FileRef &file)
{
    const TagLib::List<TagLib::VariantMap> pictures =
        file.complexProperties("PICTURE");
    if (pictures.isEmpty())
        return {};

    const TagLib::VariantMap *chosen = nullptr;
    for (const TagLib::VariantMap &pic : pictures) {
        const TagLib::String type = pic.contains("pictureType")
            ? pic["pictureType"].toString() : TagLib::String();
        if (type == "Front Cover") { chosen = &pic; break; }
        if (!chosen) chosen = &pic;
    }

    if (!chosen || !chosen->contains("data"))
        return {};
    bool ok = false;
    const TagLib::ByteVector bytes = (*chosen)["data"].toByteVector(&ok);
    if (!ok || bytes.isEmpty())
        return {};

    // Extension from the declared MIME type, with magic-byte sniffing as
    // the fallback (some tags lie or omit the MIME type). A tag that
    // claims to embed art but decodes to no known magic keeps the
    // historical "jpg" default rather than dropping the picture.
    const QByteArray raw(bytes.data(), bytes.size());
    QString ext = imageExtensionFromMime(chosen->contains("mimeType")
        ? tagStringToQString((*chosen)["mimeType"].toString()).toLower() : QString());
    if (ext.isEmpty())
        ext = imageExtensionFromMagicBytes(raw);
    if (ext.isEmpty())
        ext = QStringLiteral("jpg");

    return cacheArtworkBytes(raw, ext);
}

} // namespace

MetadataReader::MetadataReader()
{
}

QString MetadataReader::cacheArtwork(const QString &imagePath)
{
    QFile file(imagePath);
    if (imagePath.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "cacheArtwork: cannot read" << imagePath;
        return {};
    }
    const QByteArray raw = file.readAll();
    // Strict magic-byte sniff: a user-picked file must be a real image.
    // (The scan's "jpg" fallback would cache arbitrary bytes under a
    // wrong extension instead of refusing.)
    const QString ext = imageExtensionFromMagicBytes(raw);
    if (ext.isEmpty()) {
        qWarning() << "cacheArtwork: not a recognized image file:" << imagePath;
        return {};
    }
    return cacheArtworkBytes(raw, ext);
}

Song MetadataReader::read(const QString &filePath) const
{
    Song song;
    song.path = filePath;

    TagLib::FileRef file(filePath.toStdString().c_str());
    if (file.isNull()) {
        // Not parseable by TagLib (unsupported or corrupt). The file is
        // still ingested so it stays playable; only its tags are lost.
        qWarning() << "Failed to read metadata from:" << filePath;
    } else {
        TagLib::Tag *tag = file.tag();
        if (tag) {
            song.title = tagStringToQString(tag->title());
            song.artist = tagStringToQString(tag->artist());
            song.album = tagStringToQString(tag->album());
            song.genre = tagStringToQString(tag->genre());
            song.year = tag->year();
            song.trackNumber = tag->track();
        }

        TagLib::AudioProperties *properties = file.audioProperties();
        if (properties) {
            song.duration = properties->lengthInMilliseconds();
        }

        // Persist the embedded cover art (if any) to the cache at scan
        // time: the QML side only ever loads a local file:// URL.
        song.artworkPath = extractArtwork(file);
    }

    // Fall back to the file name so songs without tags are still
    // identifiable in the library view. This also covers files TagLib
    // cannot parse at all (e.g. a WebM it does not recognise): as long as
    // the file exists and the scanner would accept it, it keeps a title.
    // Unsupported extensions keep the previous contract (empty title).
    if (song.title.isEmpty()
        && QFileInfo(filePath).exists()
        && Scanner::isSupportedAudioFile(filePath)) {
        song.title = QFileInfo(filePath).completeBaseName();
    }

    return song;
}

