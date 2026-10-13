#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>
#include <QSet>

namespace {

/**
 * SELECT that resolves artist/album/genre names from the foreign keys
 * so the Song struct fields are populated in a single query.
 */
const char *const kSongSelect =
    "SELECT s.*, a.name AS artist_name, al.title AS album_title, g.name AS genre_name "
    "FROM songs s "
    "LEFT JOIN artists a ON s.artist_id = a.id "
    "LEFT JOIN albums al ON s.album_id = al.id "
    "LEFT JOIN genres g ON s.genre_id = g.id ";

/**
 * Clause that excludes songs the user has hidden ("ignored"). An exact
 * path match hides one file; a prefix match hides everything inside an
 * ignored directory. Written to slot into a WHERE clause that already
 * references the songs table as "s" (no leading AND).
 */
const char *const kNotIgnoredClause =
    "NOT EXISTS (SELECT 1 FROM ignored_paths i "
    "WHERE s.path = i.path OR s.path LIKE i.path || '/%')";

Song songFromQuery(const QSqlQuery &query)
{
    Song song;
    song.id = query.value("id").toLongLong();
    song.path = query.value("path").toString();
    song.title = query.value("title").toString();
    song.artist = query.value("artist_name").toString();
    song.album = query.value("album_title").toString();
    song.genre = query.value("genre_name").toString();
    song.year = query.value("year").toInt();
    song.trackNumber = query.value("track_number").toInt();
    song.discNumber = query.value("disc_number").toInt();
    song.duration = query.value("duration").toLongLong();
    song.artworkPath = query.value("artwork_path").toString();
    song.dateAdded = QDateTime::fromString(query.value("date_added").toString(), Qt::ISODate);
    song.lastPlayed = QDateTime::fromString(query.value("last_played").toString(), Qt::ISODate);
    song.playCount = query.value("play_count").toInt();
    song.favorite = query.value("favorite").toInt();
    return song;
}

/** Find (or create) a row in a table keyed by a UNIQUE name column. */
qint64 resolveNamedRow(QSqlDatabase &db, const QString &table, const QString &value)
{
    if (value.isEmpty()) {
        return -1;
    }
    {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral("INSERT OR IGNORE INTO %1 (name) VALUES (?)").arg(table));
        ins.addBindValue(value);
        ins.exec();
    }
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT id FROM %1 WHERE name = ?").arg(table));
    sel.addBindValue(value);
    if (sel.exec() && sel.next()) {
        return sel.value(0).toLongLong();
    }
    return -1;
}

/** Find (or create) the album row matching title + artist. */
qint64 resolveAlbum(QSqlDatabase &db, const QString &title, qint64 artistId, int year)
{
    if (title.isEmpty()) {
        return -1;
    }
    const QVariant artistVar = artistId > 0 ? QVariant(artistId) : QVariant();
    {
        QSqlQuery sel(db);
        sel.prepare(QStringLiteral("SELECT id FROM albums WHERE title = ? AND artist_id IS ?"));
        sel.addBindValue(title);
        sel.addBindValue(artistVar);
        if (sel.exec() && sel.next()) {
            return sel.value(0).toLongLong();
        }
    }
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO albums (title, artist_id, year) VALUES (?, ?, ?)"));
    ins.addBindValue(title);
    ins.addBindValue(artistVar);
    ins.addBindValue(year > 0 ? QVariant(year) : QVariant());
    if (ins.exec()) {
        return ins.lastInsertId().toLongLong();
    }
    return -1;
}

/**
 * Resolve an artist id by name without creating the row (-1 when the name is
 * empty or unknown). The read-only counterpart of resolveNamedRow(), used
 * where creating a stray artist on a refused edit would be wrong.
 */
qint64 artistIdFor(QSqlDatabase &db, const QString &name)
{
    if (name.isEmpty()) {
        return -1;
    }
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT id FROM artists WHERE name = :name"));
    query.bindValue(":name", name);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return -1;
}

/**
 * Resolve the albums row for (title, artist) without creating it (-1 when it
 * does not exist). An empty artist addresses the album with no artist
 * (artist_id IS NULL), the same semantics as resolveAlbum().
 */
qint64 albumIdFor(QSqlDatabase &db, const QString &title, const QString &artist)
{
    if (title.isEmpty()) {
        return -1;
    }
    const qint64 artistId = artistIdFor(db, artist);
    if (!artist.isEmpty() && artistId <= 0) {
        return -1; // no such artist -> no such album
    }
    const QVariant artistVar = artistId > 0 ? QVariant(artistId) : QVariant();
    QSqlQuery find(db);
    find.prepare(QStringLiteral(
        "SELECT id FROM albums WHERE title = :title AND artist_id IS :artist"));
    find.bindValue(":title", title);
    find.bindValue(":artist", artistVar);
    if (!find.exec() || !find.next()) {
        return -1;
    }
    return find.value(0).toLongLong();
}

/**
 * @brief Whether the runtime SQLite supports INSERT ... RETURNING.
 *
 * RETURNING needs SQLite >= 3.35 (2021); older system libraries reject the
 * statement. Probed once per process and cached, so upsertSong() can pick the
 * fast path (read the row id from the statement itself) or fall back to a
 * follow-up SELECT. The connection is process-wide (see the constructor), so
 * the default one is the right place to ask.
 */
bool sqliteSupportsReturning()
{
    static const bool supported = [] {
        QSqlQuery query(QSqlDatabase::database());
        if (!query.exec(QStringLiteral("SELECT sqlite_version()")) || !query.next())
            return false;
        const QStringList parts = query.value(0).toString().split(QLatin1Char('.'));
        if (parts.size() < 2)
            return false;
        const int major = parts.at(0).toInt();
        const int minor = parts.at(1).toInt();
        return major > 3 || (major == 3 && minor >= 35);
    }();
    return supported;
}

} // namespace

DatabaseManager::DatabaseManager()
{
    // Share the process-wide default SQLite connection instead of re-adding it:
    // re-adding a connection name that already exists invalidates the other
    // manager's handle ("old connection removed"), breaking the controllers
    // (library, playlists) that each own a DatabaseManager for the same file.
    if (QSqlDatabase::contains()) {
        m_db = QSqlDatabase::database();
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    }
}

DatabaseManager::~DatabaseManager()
{
    // The connection is process-wide and shared with every other manager (see
    // the constructor). close() therefore tears it down for all of them; this
    // is safe only because the owners (the library and playlist controllers,
    // stack locals in main) are all destroyed together at shutdown. If a
    // manager ever gets a shorter lifetime, give the connection a single owner
    // and pass it in instead of sharing the default one.
    close();
}

bool DatabaseManager::open(const QString &path)
{
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    // SQLite per-connection pragmas: enforce foreign keys (the code already
    // keeps the manual cascade order, so this is pure safety) and tune the
    // journal for the fast single-writer local database. WAL + NORMAL cut
    // the per-commit fsync cost that dominated scan commits (see A1).
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));

    if (!createTables()) {
        qWarning() << "Failed to create tables:" << m_db.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::beginTransaction()
{
    return m_db.transaction();
}

bool DatabaseManager::commitTransaction()
{
    return m_db.commit();
}

bool DatabaseManager::rollbackTransaction()
{
    return m_db.rollback();
}

bool DatabaseManager::execute(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qWarning() << "SQL error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::createTables()
{
    // Artists
    execute("CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE NOT NULL)");

    // Albums
    execute("CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title TEXT NOT NULL, "
            "artist_id INTEGER, "
            "year INTEGER, "
            "artwork_path TEXT, "
            "FOREIGN KEY(artist_id) REFERENCES artists(id))");

    // Genres
    execute("CREATE TABLE IF NOT EXISTS genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE NOT NULL)");

    // Songs
    execute("CREATE TABLE IF NOT EXISTS songs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE NOT NULL, "
            "title TEXT, "
            "artist_id INTEGER, "
            "album_id INTEGER, "
            "genre_id INTEGER, "
            "year INTEGER, "
            "track_number INTEGER, "
            "disc_number INTEGER, "
            "duration INTEGER, "
            "artwork_path TEXT, "
            "mtime INTEGER, "
            "size INTEGER, "
            "date_added TEXT, "
            "last_played TEXT, "
            "play_count INTEGER DEFAULT 0, "
            "favorite INTEGER DEFAULT 0, "
            "edited INTEGER NOT NULL DEFAULT 0, "
            "FOREIGN KEY(artist_id) REFERENCES artists(id), "
            "FOREIGN KEY(album_id) REFERENCES albums(id), "
            "FOREIGN KEY(genre_id) REFERENCES genres(id))");

    // Playlists
    execute("CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "description TEXT, "
            "date_created TEXT)");

    // Playlist songs
    execute("CREATE TABLE IF NOT EXISTS playlist_songs ("
            "playlist_id INTEGER, "
            "song_id INTEGER, "
            "position INTEGER, "
            "PRIMARY KEY(playlist_id, song_id), "
            "FOREIGN KEY(playlist_id) REFERENCES playlists(id), "
            "FOREIGN KEY(song_id) REFERENCES songs(id))");

    // Hidden items. The user can hide files or whole directories without
    // deleting them: the rows stay in the database, only the queries and
    // the scanner exclude them. is_dir is informational (the filter works
    // on plain path prefixes).
    execute("CREATE TABLE IF NOT EXISTS ignored_paths ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE NOT NULL, "
            "is_dir INTEGER DEFAULT 0, "
            "date_added TEXT)");

    // Schema version
    execute("CREATE TABLE IF NOT EXISTS schema_version ("
            "version INTEGER)");
    // Read the version stored by a previous open BEFORE overwriting it: the
    // v3 artwork backfill below must fire exactly once per legacy database.
    // Running it on every open would keep artwork-less files in a permanent
    // rescan loop (their artwork_path stays empty forever, so the fingerprint
    // would be reset again and again).
    int storedVersion = 0;
    {
        QSqlQuery stored(m_db);
        if (stored.exec(QStringLiteral("SELECT version FROM schema_version"))
            && stored.next()) {
            storedVersion = stored.value(0).toInt();
        }
    }
    // Schema v2 adds the file-change columns below; record it as a single row
    // so repeated opens do not accumulate rows (the old INSERT OR IGNORE had
    // no UNIQUE constraint and therefore inserted one row per launch).
    execute("DELETE FROM schema_version");
    execute("INSERT INTO schema_version (version) VALUES (4)");

    // Schema migration: an existing database predating v2 lacks mtime/size
    // (and one predating v4 lacks edited). PRAGMA table_info lists the
    // current columns; ALTER TABLE ADD COLUMN is only issued for the ones
    // missing, so this is idempotent on every open. Fresh databases
    // already have all of them from the CREATE above.
    {
        QSet<QString> columns;
        QSqlQuery info(m_db);
        if (info.exec(QStringLiteral("PRAGMA table_info(songs)"))) {
            while (info.next())
                columns.insert(info.value(QStringLiteral("name")).toString());
        }
        if (!columns.contains(QStringLiteral("mtime")))
            execute(QStringLiteral("ALTER TABLE songs ADD COLUMN mtime INTEGER"));
        if (!columns.contains(QStringLiteral("size")))
            execute(QStringLiteral("ALTER TABLE songs ADD COLUMN size INTEGER"));
        // Schema v4 (tag editing): rows the user has curated carry
        // edited = 1 so upsertSong() (the rescan path) skips their
        // tag-derived columns and only refreshes mtime/size. The non-null
        // default is required by SQLite for ADD COLUMN; existing rows
        // backfill to 0 (file tags win, the current behavior). Like the
        // v2 columns above this is presence-checked, so it is idempotent
        // on every open and fresh databases (which already have the
        // column from the CREATE above) skip it.
        if (!columns.contains(QStringLiteral("edited")))
            execute(QStringLiteral("ALTER TABLE songs ADD COLUMN edited INTEGER NOT NULL DEFAULT 0"));
    }

    // Schema v3 (embedded artwork): rows written before artwork extraction
    // existed carry an empty artwork_path even when the file embeds a cover.
    // Zeroing mtime breaks the scan fingerprint check (mtime = 0 never
    // matches a real file), so the next scan re-reads those files once and
    // backfills artwork_path. Guarded by the stored version to run only when
    // upgrading from a pre-v3 database; fresh databases have no songs yet.
    if (storedVersion < 3) {
        execute("UPDATE songs SET mtime = 0 "
                "WHERE artwork_path IS NULL OR artwork_path = ''");
    }

    // Query accelerators: the JOINs in kSongSelect and the aggregates in
    // getAllAlbums()/getAllArtists() walk songs by foreign key, so each FK
    // needs an index; playlist_songs.song_id serves removeSong() lookups.
    // Idempotent, so existing databases pick them up on the next launch.
    execute("CREATE INDEX IF NOT EXISTS idx_songs_artist_id ON songs(artist_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_songs_album_id  ON songs(album_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_songs_genre_id  ON songs(genre_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_playlist_songs_song_id ON playlist_songs(song_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_ignored_paths_path ON ignored_paths(path)");
    // The artists/albums LEFT JOIN in getAllArtists() probes albums by
    // artist_id on every (artist, song) row; without this index each probe
    // is a full table scan.
    execute("CREATE INDEX IF NOT EXISTS idx_albums_artist_id ON albums(artist_id)");

    return true;
}

bool DatabaseManager::addSong(Song &song)
{
    if (song.path.isEmpty()) {
        return false;
    }

    // Reject duplicate paths before touching the lookup tables so that
    // rescanning a directory does not leave orphan artist/album rows.
    {
        QSqlQuery dup(m_db);
        dup.prepare(QStringLiteral("SELECT id FROM songs WHERE path = ?"));
        dup.addBindValue(song.path);
        if (dup.exec() && dup.next()) {
            return false;
        }
    }

    const qint64 artistId = resolveNamedRow(m_db, QStringLiteral("artists"), song.artist);
    const qint64 albumId = resolveAlbum(m_db, song.album, artistId, song.year);
    const qint64 genreId = resolveNamedRow(m_db, QStringLiteral("genres"), song.genre);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO songs (path, title, artist_id, album_id, genre_id, year, track_number, disc_number, duration, artwork_path, date_added, last_played, play_count, favorite) "
                  "VALUES (:path, :title, :artist_id, :album_id, :genre_id, :year, :track_number, :disc_number, :duration, :artwork_path, :date_added, :last_played, :play_count, :favorite)");
    query.bindValue(":path", song.path);
    query.bindValue(":title", song.title);
    query.bindValue(":artist_id", artistId > 0 ? QVariant(artistId) : QVariant());
    query.bindValue(":album_id", albumId > 0 ? QVariant(albumId) : QVariant());
    query.bindValue(":genre_id", genreId > 0 ? QVariant(genreId) : QVariant());
    query.bindValue(":year", song.year);
    query.bindValue(":track_number", song.trackNumber);
    query.bindValue(":disc_number", song.discNumber);
    query.bindValue(":duration", song.duration);
    query.bindValue(":artwork_path", song.artworkPath);
    query.bindValue(":date_added", song.dateAdded.toString(Qt::ISODate));
    query.bindValue(":last_played", song.lastPlayed.toString(Qt::ISODate));
    query.bindValue(":play_count", song.playCount);
    query.bindValue(":favorite", song.favorite ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Failed to add song:" << query.lastError().text();
        return false;
    }
    song.id = query.lastInsertId().toLongLong();
    return true;
}

bool DatabaseManager::upsertSong(Song &song)
{
    if (song.path.isEmpty()) {
        return false;
    }

    const qint64 artistId = resolveNamedRow(m_db, QStringLiteral("artists"), song.artist);
    const qint64 albumId = resolveAlbum(m_db, song.album, artistId, song.year);
    const qint64 genreId = resolveNamedRow(m_db, QStringLiteral("genres"), song.genre);

    const bool useReturning = sqliteSupportsReturning();

    QString sql = QStringLiteral(
        "INSERT INTO songs (path, title, artist_id, album_id, genre_id, year, "
        "track_number, disc_number, duration, artwork_path, mtime, size, "
        "date_added, last_played, play_count, favorite) "
        "VALUES (:path, :title, :artist_id, :album_id, :genre_id, :year, "
        ":track_number, :disc_number, :duration, :artwork_path, :mtime, :size, "
        ":date_added, :last_played, :play_count, :favorite) "
        // Refresh only the tag-derived columns on conflict. date_added,
        // last_played, play_count and favorite are intentionally absent so a
        // rescan can never clobber the user's data. Rows the user has curated
        // (edited = 1, set by updateSong()/setArtwork()) keep their
        // tag-derived columns too: every CASE keeps the stored value for an
        // edited row and takes the file's (excluded) value otherwise, so the
        // user's corrections survive rescans of a changed file. A plain
        // WHERE edited = 0 on the DO UPDATE would be wrong: it would also
        // skip the mtime/size refresh below, so an edited file would look
        // "changed forever" and every scan would re-read it again (the
        // permanent rescan loop documented with the v3 artwork backfill).
        // mtime and size therefore always refresh.
        "ON CONFLICT(path) DO UPDATE SET "
        "title = CASE edited WHEN 1 THEN title ELSE excluded.title END, "
        "artist_id = CASE edited WHEN 1 THEN artist_id ELSE excluded.artist_id END, "
        "album_id = CASE edited WHEN 1 THEN album_id ELSE excluded.album_id END, "
        "genre_id = CASE edited WHEN 1 THEN genre_id ELSE excluded.genre_id END, "
        "year = CASE edited WHEN 1 THEN year ELSE excluded.year END, "
        "track_number = CASE edited WHEN 1 THEN track_number ELSE excluded.track_number END, "
        "disc_number = CASE edited WHEN 1 THEN disc_number ELSE excluded.disc_number END, "
        "duration = CASE edited WHEN 1 THEN duration ELSE excluded.duration END, "
        "artwork_path = CASE edited WHEN 1 THEN artwork_path ELSE excluded.artwork_path END, "
        "mtime = excluded.mtime, size = excluded.size");
    if (useReturning) {
        sql += QStringLiteral(" RETURNING id");
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(":path", song.path);
    query.bindValue(":title", song.title);
    query.bindValue(":artist_id", artistId > 0 ? QVariant(artistId) : QVariant());
    query.bindValue(":album_id", albumId > 0 ? QVariant(albumId) : QVariant());
    query.bindValue(":genre_id", genreId > 0 ? QVariant(genreId) : QVariant());
    query.bindValue(":year", song.year);
    query.bindValue(":track_number", song.trackNumber);
    query.bindValue(":disc_number", song.discNumber);
    query.bindValue(":duration", song.duration);
    query.bindValue(":artwork_path", song.artworkPath);
    query.bindValue(":mtime", song.mtimeMs);
    query.bindValue(":size", song.size);
    query.bindValue(":date_added", song.dateAdded.toString(Qt::ISODate));
    query.bindValue(":last_played", song.lastPlayed.toString(Qt::ISODate));
    query.bindValue(":play_count", song.playCount);
    query.bindValue(":favorite", song.favorite ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Failed to upsert song:" << query.lastError().text();
        return false;
    }

    // Read the row id: from RETURNING when the SQLite build supports it, or
    // with a follow-up lookup otherwise (and as a safety net if the driver
    // does not expose RETURNING rows).
    if (useReturning && query.next()) {
        song.id = query.value(0).toLongLong();
        return true;
    }

    QSqlQuery idQuery(m_db);
    idQuery.prepare(QStringLiteral("SELECT id FROM songs WHERE path = ?"));
    idQuery.addBindValue(song.path);
    if (idQuery.exec() && idQuery.next()) {
        song.id = idQuery.value(0).toLongLong();
        return true;
    }
    return false;
}

bool DatabaseManager::updateSong(const Song &song)
{
    if (!beginTransaction()) {
        qWarning() << "updateSong: failed to start transaction";
        return false;
    }
    if (!updateSongRow(song)) {
        rollbackTransaction();
        return false;
    }
    // Album cards roll up from the freshly edited songs' artwork and the
    // entity rows the edit may have orphaned are dropped, both in the
    // same transaction (like the scan commit path).
    refreshAlbumArtwork();
    cleanupOrphanEntities();
    return commitTransaction();
}

bool DatabaseManager::updateSongs(const QList<Song> &songs)
{
    if (songs.isEmpty()) {
        return false;
    }
    if (!beginTransaction()) {
        qWarning() << "updateSongs: failed to start transaction";
        return false;
    }
    for (const Song &song : songs) {
        if (!updateSongRow(song)) {
            // One bad row rolls the whole batch back: a half-applied
            // batch edit would leave the library inconsistent.
            rollbackTransaction();
            return false;
        }
    }
    // The rollup and the cleanup are batch-wide concerns: run them once
    // after the last row, not once per song.
    refreshAlbumArtwork();
    cleanupOrphanEntities();
    return commitTransaction();
}

bool DatabaseManager::updateSongRow(const Song &song)
{
    const qint64 artistId = resolveNamedRow(m_db, QStringLiteral("artists"), song.artist);
    const qint64 albumId = resolveAlbum(m_db, song.album, artistId, song.year);
    const qint64 genreId = resolveNamedRow(m_db, QStringLiteral("genres"), song.genre);

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE songs SET title = :title, artist_id = :artist_id, "
        "album_id = :album_id, genre_id = :genre_id, year = :year, "
        "duration = :duration, artwork_path = :artwork_path, edited = 1 "
        "WHERE id = :id"));
    query.bindValue(":title", song.title);
    query.bindValue(":artist_id", artistId > 0 ? QVariant(artistId) : QVariant());
    query.bindValue(":album_id", albumId > 0 ? QVariant(albumId) : QVariant());
    query.bindValue(":genre_id", genreId > 0 ? QVariant(genreId) : QVariant());
    query.bindValue(":year", song.year);
    query.bindValue(":duration", song.duration);
    query.bindValue(":artwork_path", song.artworkPath);
    query.bindValue(":id", song.id);
    if (!query.exec()) {
        qWarning() << "Failed to update song:" << query.lastError().text();
        return false;
    }
    // Zero affected rows means the id does not exist. track_number and
    // disc_number are deliberately not written (not user-editable) and
    // neither are mtime/size (the file did not change, so the next scan
    // must keep fingerprint-skipping it).
    return query.numRowsAffected() == 1;
}

void DatabaseManager::cleanupOrphanEntities()
{
    // Children first: albums.artist_id references artists, so with
    // PRAGMA foreign_keys = ON an orphan artist referenced by a (soon to
    // be deleted) orphan album cannot be removed while that album row
    // still exists. Deleting albums first releases the reference and the
    // artist cleanup actually succeeds. genres reference nothing beyond
    // songs.
    QSqlQuery cleanup(m_db);
    cleanup.exec("DELETE FROM albums WHERE id NOT IN "
                 "(SELECT DISTINCT album_id FROM songs WHERE album_id IS NOT NULL)");
    cleanup.exec("DELETE FROM artists WHERE id NOT IN "
                 "(SELECT DISTINCT artist_id FROM songs WHERE artist_id IS NOT NULL)");
    cleanup.exec("DELETE FROM genres WHERE id NOT IN "
                 "(SELECT DISTINCT genre_id FROM songs WHERE genre_id IS NOT NULL)");
}

bool DatabaseManager::renameArtist(const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty() || from == to) {
        return false;
    }
    {
        // Renaming onto an existing artist would merge the two (a
        // separate, explicit feature), so refuse up front. artists.name
        // UNIQUE is the safety net; the single shared connection and the
        // main-thread-only rule mean nothing can insert between the check
        // and the UPDATE.
        QSqlQuery check(m_db);
        check.prepare(QStringLiteral("SELECT 1 FROM artists WHERE name = :to"));
        check.bindValue(":to", to);
        if (check.exec() && check.next()) {
            qWarning() << "renameArtist: target name already exists:" << to;
            return false;
        }
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE artists SET name = :to WHERE name = :from"));
    query.bindValue(":to", to);
    query.bindValue(":from", from);
    if (!query.exec()) {
        qWarning() << "renameArtist: update failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool DatabaseManager::setAlbumIdentity(const QString &title, const QString &artist,
                                       const QString &newTitle, const QString &newArtist)
{
    if (title.isEmpty() || newTitle.isEmpty() || newArtist.isEmpty()) {
        return false;
    }
    // Read-only lookup: an empty artist addresses the album with no artist
    // (artist_id IS NULL), the same semantics as resolveAlbum().
    const qint64 albumId = albumIdFor(m_db, title, artist);
    if (albumId <= 0) {
        return false; // no such album (or no such artist)
    }
    if (title == newTitle && artist == newArtist) {
        return false; // unchanged identity
    }
    if (!beginTransaction()) {
        qWarning() << "setAlbumIdentity: failed to start transaction";
        return false;
    }
    // Resolve-or-create the target artist inside the transaction, so a
    // refusal or a failed update below leaves no orphan artist row behind.
    const qint64 newArtistId = resolveNamedRow(m_db, QStringLiteral("artists"), newArtist);
    if (newArtistId <= 0) {
        rollbackTransaction();
        return false;
    }
    {
        // No database constraint enforces uniqueness on (title, artist_id),
        // so the collision check is code only: landing on an existing album
        // of the target artist would merge the two rows. The check is on the
        // FINAL identity and excludes the edited row, so a case-only change
        // of either name is never blocked by finding this very album.
        QSqlQuery clash(m_db);
        clash.prepare(QStringLiteral(
            "SELECT 1 FROM albums WHERE title = :title AND artist_id = :artist "
            "AND id != :id"));
        clash.bindValue(":title", newTitle);
        clash.bindValue(":artist", newArtistId);
        clash.bindValue(":id", albumId);
        if (clash.exec() && clash.next()) {
            qWarning() << "setAlbumIdentity: the target artist already has an album titled"
                       << newTitle;
            rollbackTransaction();
            return false;
        }
    }
    {
        QSqlQuery album(m_db);
        album.prepare(QStringLiteral(
            "UPDATE albums SET title = :title, artist_id = :artist WHERE id = :id"));
        album.bindValue(":title", newTitle);
        album.bindValue(":artist", newArtistId);
        album.bindValue(":id", albumId);
        if (!album.exec() || album.numRowsAffected() != 1) {
            qWarning() << "setAlbumIdentity: album update failed:"
                       << album.lastError().text();
            rollbackTransaction();
            return false;
        }
    }
    {
        // The album's tracks follow the album row. Both sides must agree:
        // the drill-down and the song rows resolve the artist from
        // songs.artist_id (getSongsByAlbum filters on it and kSongSelect
        // joins artists on s.artist_id), while the album card reads
        // albums.artist_id — leaving them apart would show a card with no
        // songs under it. edited = 1 keeps the change across a rescan,
        // exactly like updateSong().
        QSqlQuery songs(m_db);
        songs.prepare(QStringLiteral(
            "UPDATE songs SET artist_id = :artist, edited = 1 WHERE album_id = :id"));
        songs.bindValue(":artist", newArtistId);
        songs.bindValue(":id", albumId);
        if (!songs.exec() || songs.numRowsAffected() == 0) {
            // An album with no songs is not reachable from the UI
            // (getAllAlbums only returns albums that have songs, via its
            // HAVING clause), and moving one would be a silent no-op that
            // cleanupOrphanEntities() then deletes: report the failure
            // instead of a phantom success.
            qWarning() << "setAlbumIdentity: song update failed:"
                       << songs.lastError().text();
            rollbackTransaction();
            return false;
        }
    }
    // Moving the last album of the old artist can orphan that artist row.
    cleanupOrphanEntities();
    return commitTransaction();
}

bool DatabaseManager::setAlbumArtwork(const QString &title, const QString &artist,
                                      const QString &imagePath)
{
    const qint64 albumId = albumIdFor(m_db, title, artist);
    if (albumId <= 0) {
        return false; // no such album
    }
    if (!beginTransaction()) {
        qWarning() << "setAlbumArtwork: failed to start transaction";
        return false;
    }
    QSqlQuery query(m_db);
    // An empty path is the explicit "remove the cover" intent, the same shape
    // a single-song removal uses (artwork_path = '').
    query.prepare(QStringLiteral(
        "UPDATE songs SET artwork_path = :path, edited = 1 WHERE album_id = :id"));
    query.bindValue(":path", imagePath);
    query.bindValue(":id", albumId);
    if (!query.exec()) {
        qWarning() << "setAlbumArtwork: update failed:" << query.lastError().text();
        rollbackTransaction();
        return false;
    }
    if (query.numRowsAffected() < 1) {
        rollbackTransaction();
        return false; // the album has no songs to point at the cover
    }
    // Album cards roll up from the song artwork in the same transaction,
    // exactly like setArtwork().
    refreshAlbumArtwork();
    return commitTransaction();
}

bool DatabaseManager::setArtwork(qint64 songId, const QString &imagePath)
{
    if (songId <= 0) {
        return false;
    }
    if (!beginTransaction()) {
        qWarning() << "setArtwork: failed to start transaction";
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE songs SET artwork_path = :path, edited = 1 WHERE id = :id"));
    query.bindValue(":path", imagePath);
    query.bindValue(":id", songId);
    if (!query.exec()) {
        qWarning() << "setArtwork: update failed:" << query.lastError().text();
        rollbackTransaction();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        rollbackTransaction();
        return false; // unknown id
    }
    // Album cards roll up from the song artwork in the same transaction,
    // exactly like the scan commit path.
    refreshAlbumArtwork();
    return commitTransaction();
}

QHash<QString, QPair<qint64, qint64>> DatabaseManager::songFileStats() const
{
    QHash<QString, QPair<qint64, qint64>> stats;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT path, mtime, size FROM songs"))) {
        while (query.next()) {
            stats.insert(query.value(0).toString(),
                         qMakePair(query.value(1).toLongLong(),
                                   query.value(2).toLongLong()));
        }
    }
    return stats;
}

QList<QPair<qint64, QString>> DatabaseManager::songIdsAndPaths() const
{
    QList<QPair<qint64, QString>> rows;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT id, path FROM songs"))) {
        while (query.next()) {
            rows.append(qMakePair(query.value(0).toLongLong(),
                                  query.value(1).toString()));
        }
    }
    return rows;
}

bool DatabaseManager::removeSong(qint64 id)
{
    return removeSongs(QList<qint64>{id});
}

bool DatabaseManager::removeSongs(const QList<qint64> &ids)
{
    if (ids.isEmpty()) {
        return false;
    }
    if (!beginTransaction()) {
        return false;
    }

    // playlist_songs has no ON DELETE CASCADE and SQLite does not enforce
    // foreign keys by default, so each song's playlist rows must be removed
    // first to avoid orphaned references. The whole batch shares one
    // transaction, so a failure halfway leaves the library as it was.
    QSqlQuery playlistRows(m_db);
    playlistRows.prepare("DELETE FROM playlist_songs WHERE song_id=:id");
    QSqlQuery songRows(m_db);
    songRows.prepare("DELETE FROM songs WHERE id=:id");
    for (const qint64 id : ids) {
        playlistRows.bindValue(":id", id);
        if (!playlistRows.exec()) {
            rollbackTransaction();
            return false;
        }
        songRows.bindValue(":id", id);
        if (!songRows.exec()) {
            rollbackTransaction();
            return false;
        }
    }

    // Drop artists/albums/genres that no longer have any songs (best-effort
    // cleanup, once for the whole batch; the views already hide orphans via
    // HAVING song_count > 0).
    cleanupOrphanEntities();

    return commitTransaction();
}

Song DatabaseManager::getSong(qint64 id) const
{
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) + QStringLiteral("WHERE s.id = :id"));
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return songFromQuery(query);
    }
    return Song();
}

QList<Song> DatabaseManager::getAllSongs() const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    const QString sql = QString::fromLatin1(kSongSelect) +
                        QStringLiteral("WHERE ") + QString::fromLatin1(kNotIgnoredClause) +
                        QStringLiteral(" ORDER BY s.title, s.path");
    if (query.exec(sql)) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::searchSongs(const QString &query) const
{
    QList<Song> songs;
    QSqlQuery sqlQuery(m_db);
    sqlQuery.prepare(QString::fromLatin1(kSongSelect) +
                     QStringLiteral("WHERE (s.title LIKE :query OR s.path LIKE :query) AND ") +
                     QString::fromLatin1(kNotIgnoredClause) +
                     QStringLiteral(" ORDER BY s.title, s.path"));
    sqlQuery.bindValue(":query", "%" + query + "%");
    if (sqlQuery.exec()) {
        while (sqlQuery.next()) {
            songs.append(songFromQuery(sqlQuery));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::getSongsByAlbum(const QString &title, const QString &artist) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    QString where = QStringLiteral("WHERE al.title = :title");
    if (!artist.isEmpty())
        where += QStringLiteral(" AND a.name = :artist");
    where += QStringLiteral(" AND ") + QString::fromLatin1(kNotIgnoredClause);
    query.prepare(QString::fromLatin1(kSongSelect) + where +
                  QStringLiteral(" ORDER BY s.disc_number, s.track_number, s.title"));
    query.bindValue(":title", title);
    if (!artist.isEmpty())
        query.bindValue(":artist", artist);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::getSongsByArtist(const QString &artist) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) +
                  QStringLiteral("WHERE a.name = :artist AND ") +
                  QString::fromLatin1(kNotIgnoredClause) +
                  QStringLiteral(" ORDER BY s.album_id, s.disc_number, s.track_number, s.title"));
    query.bindValue(":artist", artist);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Artist> DatabaseManager::getAllArtists() const
{
    QList<Artist> artists;
    QSqlQuery query(m_db);
    // No ignore clause on the albums join: albums have no path column, and
    // a clause there could only re-check the already-filtered s.path from
    // the songs join (TRUE for matched songs, TRUE for NULL-extended rows)
    // — a tautology evaluated per (artist, song, album) fan-out row. Albums
    // are only reachable through their songs for visibility purposes, so
    // the songs join above is the single place filtering happens.
    if (query.exec(QStringLiteral(
            "SELECT a.id, a.name, "
            "COUNT(DISTINCT s.id) AS song_count, "
            "COUNT(DISTINCT al.id) AS album_count "
            "FROM artists a "
            "LEFT JOIN songs s ON s.artist_id = a.id AND ") +
            QString::fromLatin1(kNotIgnoredClause) +
            QStringLiteral(
            "LEFT JOIN albums al ON al.artist_id = a.id ") +
            QStringLiteral(
            "GROUP BY a.id, a.name "
            "HAVING song_count > 0 "
            "ORDER BY a.name"))) {
        while (query.next()) {
            Artist artist;
            artist.id = query.value("id").toLongLong();
            artist.name = query.value("name").toString();
            artist.songCount = query.value("song_count").toInt();
            artist.albumCount = query.value("album_count").toInt();
            artists.append(artist);
        }
    }
    return artists;
}

QList<Album> DatabaseManager::getAllAlbums() const
{
    QList<Album> albums;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT al.id, al.title, al.year, al.artwork_path, "
            "a.name AS artist_name, COUNT(s.id) AS song_count "
            "FROM albums al "
            "LEFT JOIN artists a ON al.artist_id = a.id "
            "LEFT JOIN songs s ON s.album_id = al.id AND ") +
            QString::fromLatin1(kNotIgnoredClause) +
            QStringLiteral(
            "GROUP BY al.id "
            "HAVING song_count > 0 "
            "ORDER BY al.title"))) {
        while (query.next()) {
            Album album;
            album.id = query.value("id").toLongLong();
            album.title = query.value("title").toString();
            album.artist = query.value("artist_name").toString();
            album.year = query.value("year").toInt();
            album.songCount = query.value("song_count").toInt();
            album.artworkPath = query.value("artwork_path").toString();
            albums.append(album);
        }
    }
    return albums;
}

void DatabaseManager::refreshAlbumArtwork()
{
    // First track that has artwork, in disc/track order — the common pattern.
    execute("UPDATE albums SET artwork_path = ("
            "SELECT s.artwork_path FROM songs s "
            "WHERE s.album_id = albums.id AND s.artwork_path IS NOT NULL "
            "AND s.artwork_path != '' "
            "ORDER BY s.disc_number, s.track_number, s.id LIMIT 1)");
}

bool DatabaseManager::addPlaylist(Playlist &playlist)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO playlists (name, description, date_created) VALUES (:name, :description, :date_created)");
    query.bindValue(":name", playlist.name);
    query.bindValue(":description", playlist.description);
    query.bindValue(":date_created", playlist.dateCreated.toString(Qt::ISODate));
    if (query.exec()) {
        playlist.id = query.lastInsertId().toLongLong();
        return true;
    }
    return false;
}

bool DatabaseManager::updatePlaylist(const Playlist &playlist)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE playlists SET name=:name, description=:description WHERE id=:id");
    query.bindValue(":name", playlist.name);
    query.bindValue(":description", playlist.description);
    query.bindValue(":id", playlist.id);
    return query.exec();
}

bool DatabaseManager::removePlaylist(qint64 id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE playlist_id=:id");
    query.bindValue(":id", id);
    query.exec();

    query.prepare("DELETE FROM playlists WHERE id=:id");
    query.bindValue(":id", id);
    return query.exec();
}

QList<Playlist> DatabaseManager::getAllPlaylists() const
{
    QList<Playlist> playlists;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT p.id, p.name, p.description, p.date_created, "
            "COUNT(ps.song_id) AS song_count "
            "FROM playlists p "
            "LEFT JOIN playlist_songs ps ON ps.playlist_id = p.id "
            "GROUP BY p.id "
            "ORDER BY p.name"))) {
        while (query.next()) {
            Playlist playlist;
            playlist.id = query.value("id").toLongLong();
            playlist.name = query.value("name").toString();
            playlist.description = query.value("description").toString();
            playlist.dateCreated = QDateTime::fromString(query.value("date_created").toString(), Qt::ISODate);
            playlist.songCount = query.value("song_count").toInt();
            playlists.append(playlist);
        }
    }
    return playlists;
}

bool DatabaseManager::addSongToPlaylist(qint64 playlistId, qint64 songId, int position)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (:playlist_id, :song_id, :position)");
    query.bindValue(":playlist_id", playlistId);
    query.bindValue(":song_id", songId);
    query.bindValue(":position", position);
    return query.exec();
}

bool DatabaseManager::removeSongFromPlaylist(qint64 playlistId, qint64 songId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE playlist_id=:playlist_id AND song_id=:song_id");
    query.bindValue(":playlist_id", playlistId);
    query.bindValue(":song_id", songId);
    return query.exec();
}

QList<Song> DatabaseManager::getPlaylistSongs(qint64 playlistId) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) +
                  QStringLiteral("JOIN playlist_songs ps ON s.id = ps.song_id "
                                 "WHERE ps.playlist_id = :playlist_id AND ") +
                  QString::fromLatin1(kNotIgnoredClause) +
                  QStringLiteral(" ORDER BY ps.position, ps.rowid"));
    query.bindValue(":playlist_id", playlistId);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

bool DatabaseManager::addIgnoredPath(const QString &path, bool isDir)
{
    if (path.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO ignored_paths (path, is_dir, date_added) "
                  "VALUES (:path, :is_dir, :date_added)");
    query.bindValue(":path", path);
    query.bindValue(":is_dir", isDir ? 1 : 0);
    query.bindValue(":date_added", QDateTime::currentDateTime().toString(Qt::ISODate));
    return query.exec();
}

bool DatabaseManager::removeIgnoredPath(const QString &path)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM ignored_paths WHERE path = :path");
    query.bindValue(":path", path);
    return query.exec();
}

QStringList DatabaseManager::ignoredPaths() const
{
    QStringList paths;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT path FROM ignored_paths ORDER BY path"))) {
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }
    return paths;
}
