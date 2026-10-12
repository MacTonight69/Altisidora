# Altisidora — Optimization Plan (implementation tasks)

Goal: make the app faster (scan, search), lighter (RAM, build) and cleaner
(queries) without changing user-visible behaviour. The status table below
reflects what is implemented; the plan documents the three independent,
cumulative options that got us there:

- **Option A — Quick wins** (scan transaction, DB indexes, search debounce,
  scanner extension lookup). Low risk, surgical.
- **Option B — Controller caching + N+1 removal** (cache `Library.songs /
  albums / artists`; resolve album/artist "play all" in one pass). Medium
  risk.
- **Option C — `QAbstractListModel` foundation** (role-based delegates +
  a real list model for the main songs view). Architectural, higher risk,
  best done on top of A + B.
- **C4 (partially done) — search through the model + incremental updates**.
  The search half is implemented (`perf: render search results through the
  song role model`); the row-level incremental-update half remains deferred
  (see C4 section).

Baseline verified at the time of writing: `e2b7692`, clean tree, fresh
`cmake --build` exit 0 with 0 warnings, `ctest` 12/12.

## Quick path (read order)

1. Read the **Global verification** block once; every option reuses it.
2. Option A first (independent, unblocks nothing but pays immediately).
3. Option B next (builds on A1/A2 semantics, touches different files).
4. Option C last, per step, with a commit after each step.

## Summary

| Option | What | Files touched | Risk | Status |
|--------|------|---------------|------|--------|
| A | Scan transaction (A1/A1b), DB indexes + pragmas (A2), search debounce (A3), scanner ext set (A4) | `DatabaseManager.*`, `LibraryController.*`, `Scanner.cpp`, `main.qml` | Low | [x] (A1, A2, A3, A4; A1b not needed) |
| B | Cache `songs/albums/artists` reads (B1); one-pass `collectSongs()` (B2) | `LibraryController.*`, `AlbumsView.qml`, `ArtistsView.qml` | Medium | [x] |
| C | `SongListModel` + role-based delegates (C1–C4) | `SongListModel.*` (new), `CMakeLists.txt`, `tests/CMakeLists.txt`, `SongsView.qml`, `SongListDetailView.qml`, `PlaylistsView.qml`, `main.qml` | High | [x] C1–C4 |

A, B and C are independent; the plan only assumes you do them in order.

**Status: A–C (C4 included) and the backlog are implemented.**
Branch chain (base `e2b7692`): `perf/quick-wins` (A) → `perf/controller-cache`
(B, TASKS.md, C, theme fonts) → `perf/backlog-microopts` (backlog) →
`build/static-core-lib` (static core + DB note, scan transaction commit, C4
search model `fd2ed0d`, hide-by-id fix `b1885be`). After the C4 search work a
pre-existing QML bug surfaced and was fixed: the song/playlist delegates were
missing `required property var model` under `pragma ComponentBehavior: Bound`,
so rows never rendered (`fix(qml): declare required model binding…`, `099b359`).
C4 was then finished: `applySnapshot()` gives the songs model row-level
incremental updates (no more `beginResetModel()` on a library change), and the
model grew `PlayCountRole` / `LastPlayedRole` plus `sort(role, order)` so
ordering lives in C++. Verified after every commit: build exit 0 (0 warnings),
`ctest` 12/12, smoke OK. Nothing is deferred from this plan.

---

## Global verification (after every option)

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"          # expect exit 0
ctest --test-dir build --output-on-failure  # expect 12/12
```

Smoke run (isolated HOME; must stay alive until timeout with empty stderr):

```bash
rm -rf /tmp/opencode/alt-run && mkdir -p /tmp/opencode/alt-run/home
cd /home/bautistaac/Proyectos/Altisidora
env HOME=/tmp/opencode/alt-run/home XDG_DATA_HOME=/tmp/opencode/alt-run/data \
    XDG_CONFIG_HOME=/tmp/opencode/alt-run/config QT_QPA_PLATFORM=offscreen \
    timeout 6 ./build/altisidora; echo "exit=$?  (124 = OK)"
```

Work on a feature branch per option:
`git switch -c perf/quick-wins`, `git switch -c perf/controller-cache`,
`git switch -c refactor/song-list-model`.

---

## Option A — Quick wins

### A1 — Batch the scan commit in one transaction `[required]`

**Why.** `LibraryController::handleScanFinished()` calls `m_db.addSong()`
per song. Each `addSong()` runs ~6 statements (dup-SELECT, artist/genre
`INSERT OR IGNORE` + SELECT, album SELECT + INSERT, song INSERT). In SQLite
autocommit every statement is its own fsync: ~30 000 disk syncs for 5 000
songs.

**Files:** `src/database/DatabaseManager.h`, `src/database/DatabaseManager.cpp`,
`src/library/LibraryController.cpp`.

**Do:**

1. `DatabaseManager.h` (public section) — add:

```cpp
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
```

2. `DatabaseManager.cpp` (next to `execute()`) — add:

```cpp
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
```

3. `LibraryController.cpp`, `handleScanFinished()` — wrap the loop:

```cpp
void LibraryController::handleScanFinished()
{
    const QList<Song> found = m_scanWatcher.result();
    if (!found.isEmpty()) {
        if (!m_db.beginTransaction())
            qWarning() << "LibraryController: failed to start scan transaction";
        for (const Song &song : found) {
            Song mutableSong = song; // addSong() stores the generated id
            m_db.addSong(mutableSong);
        }
        if (!m_db.commitTransaction())
            qWarning() << "LibraryController: failed to commit scan transaction";
    }
    m_songs = m_db.getAllSongs().toVector();
    m_scanRunning = false;
    emit libraryChanged();
    emit scanFinished();
}
```

**Verify.** `ctest --test-dir build` (tst_database, tst_library,
tst_library_scan). Manual: scan a large folder — the "Scanning…" overlay now
ends quickly after the last file is read.

**Commit.** `perf(database): commit scan inserts in one transaction`

### A1b — Cache name→id lookups during the scan `[evaluated twice: not worth it]`

> **Measured verdict (2026-10-11, bench of the real pipeline at
> /tmp/opencode/upsert-bench, 5000 songs / 200 artists / 400 albums / 20
> genres):** the per-scan cache cuts the commit phase 45% (30 403 → 6 243
> statements) but saves only ~250 ms per 5000 songs — invisible next to
> TagLib I/O on real libraries — and a stale cache silently corrupts scans
> (FK violations, 0 songs committed; reproduced in the bench). The
> `INSERT OR IGNORE ... RETURNING` variant is a measured **pessimization**
> (+18–22% commit time: SQLite builds ephemeral-table/trigger machinery per
> prepared statement, and this code re-prepares per call; it also saves zero
> statements on warm names). Do not retry this without new evidence.

**Why.** Inside the transaction, repeated artists/albums still re-run
`INSERT OR IGNORE` + `SELECT` on every song. Caching turns those into hash
hits.

**Files:** `src/database/DatabaseManager.cpp`, `DatabaseManager.h`,
`LibraryController.cpp`.

**Do:**

1. `resolveNamedRow()` (anonymous namespace) gains an optional cache
   parameter:

```cpp
qint64 resolveNamedRow(QSqlDatabase &db, const QString &table, const QString &value,
                       QHash<QString, qint64> *cache = nullptr)
{
    if (value.isEmpty())
        return -1;
    if (cache) {
        const auto it = cache->constFind(value);
        if (it != cache->constEnd())
            return it.value();
    }
    // ...existing INSERT OR IGNORE + SELECT...
    const qint64 id = sel.next() ? sel.value(0).toLongLong() : -1;
    if (cache && id > 0)
        cache->insert(value, id);
    return id;
}
```

2. New `DatabaseManager` method (single transaction + caches +
   `INSERT OR IGNORE` for the path, dropping the per-song dup-SELECT):

```cpp
// DatabaseManager.h
    bool addScanBatch(const QList<Song> &songs);
```

```cpp
bool DatabaseManager::addScanBatch(const QList<Song> &songs)
{
    if (!m_db.transaction())
        return false;

    QHash<QString, qint64> artistCache;
    QHash<QString, qint64> genreCache;
    QHash<QString, qint64> albumCache; // key: "<title>\0<artistId>"

    for (const Song &song : songs) {
        if (song.path.isEmpty())
            continue;
        const qint64 artistId = resolveNamedRow(m_db, "artists", song.artist, &artistCache);
        qint64 albumId = -1;
        if (!song.album.isEmpty()) {
            const QString key = song.album + QLatin1Char('\0') + QString::number(artistId);
            const auto it = albumCache.constFind(key);
            if (it != albumCache.constEnd())
                albumId = it.value();
            else {
                albumId = resolveAlbum(m_db, song.album, artistId, song.year);
                albumCache.insert(key, albumId);
            }
        }
        const qint64 genreId = resolveNamedRow(m_db, "genres", song.genre, &genreCache);

        QSqlQuery query(m_db);
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO songs (path, title, artist_id, album_id, genre_id, "
            "year, track_number, disc_number, duration, artwork_path, date_added, "
            "last_played, play_count, favorite) "
            "VALUES (:path, :title, :artist_id, :album_id, :genre_id, :year, "
            ":track_number, :disc_number, :duration, :artwork_path, :date_added, "
            ":last_played, :play_count, :favorite)"));
        // ...same binds as addSong(), duplicating is fine here...
        query.addBindValue(song.path);
        // (bind the remaining 13 columns)
        if (!query.exec())
            continue;
        if (query.numRowsAffected() == 0)
            continue; // duplicate path, already in the library
    }

    return m_db.commit();
}
```

3. `handleScanFinished()` calls `addScanBatch(found)` instead of the loop.

> Note: `addSong()` is kept untouched (playlists and other callers rely on
> it); only the scan path moves to `addScanBatch()`.

**Verify.** `ctest --test-dir build`; manual large-folder scan.

**Commit.** `perf(database): cache artist/album/genre lookups in scan batch`

### A2 — Schema indexes + SQLite pragmas `[required]`

**Why.** `songs.artist_id / album_id / genre_id` and
`playlist_songs.song_id` have no index: every JOIN in `kSongSelect` and the
`GROUP BY` aggregates in `getAllAlbums()/getAllArtists()` walk the whole
table.

**Files:** `src/database/DatabaseManager.cpp`.

**Do.**

1. In `createTables()`, before `return true;`, append:

```cpp
    // Query accelerators: the JOINs in kSongSelect and the aggregates walk
    // songs by FK, so each FK needs an index; playlist_songs.song_id serves
    // removeSong() lookups.
    execute("CREATE INDEX IF NOT EXISTS idx_songs_artist_id ON songs(artist_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_songs_album_id  ON songs(album_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_songs_genre_id  ON songs(genre_id)");
    execute("CREATE INDEX IF NOT EXISTS idx_playlist_songs_song_id ON playlist_songs(song_id)");
```

2. In `open()`, right after `m_db.open()` succeeds:

```cpp
    // SQLite per-connection pragmas: enforce foreign keys (the code already
    // keeps manual cascade order) and tune the journal for the fast
    // single-writer local DB.
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
```

**Verify.** `ctest --test-dir build` (tst_database reopens the schema via
`CREATE TABLE IF NOT EXISTS`; the `IF NOT EXISTS` indexes are idempotent).
No user-visible change.

**Commit.** `perf(database): add FK indexes and enable WAL/synchronous=NORMAL`

### A3 — Debounce the search `[required]`

**Why.** `SearchBar.onTextChanged` → `main.qml.applySearch()` runs, per
keystroke: `Library.search()` (SQL `LIKE`), `filterAlbums()` and
`filterArtists()` (two more SQL reads). 3 queries per keypress plus a full
`ListView` model reset.

**Files:** `resources/qml/main.qml`.

**Do.**

1. Add a timer next to `scanTimer`:

```qml
    // Debounce search: a keystroke only (re)schedules the re-filter, which
    // then runs Library.search() + the album/artist JS filters once the
    // user pauses for 200 ms instead of on every keystroke.
    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: root.applySearch(root.searchQuery)
    }
```

2. Change the `SearchBar` handler from running the search immediately to
   scheduling it:

```qml
                            onQueryChanged: function (query) {
                                root.searchQuery = query
                                searchDebounce.restart()
                            }
```

Behaviour preserved: `applySearch("")` still clears the filters, and the
existing `onLibraryChanged` hook in the `Connections` block still re-applies
an active query immediately when a scan lands.

**Verify.** Smoke run + manual: type in the search bar, confirm results
update ~200 ms after pausing. No test covers this path (UI-only).

**Commit.** `perf(ui): debounce search re-filter in main.qml`

### A4 — Static extension lookup in Scanner `[required]`

**Why.** `supportedExtensions()` builds a fresh `QStringList` on every call
and `isSupportedAudioFile()` does a linear `.contains()`. It is called 2–3
times per file during a scan (~15 000 allocations for 5 000 files).

**Files:** `src/library/Scanner.cpp`.

**Do.**

1. In the anonymous namespace at the top of the file, replace the bare
   list construction with two statics (the set is built once):

```cpp
// Constructed once, reused by every scan/isSupportedAudioFile call.
static const QStringList kSupportedExtensions = {
    "mp3", "flac", "ogg", "m4a", "wav", "opus", "aac", "wma", "aiff",
    // WebM / Matroska carry audio (Opus/Vorbis) that libmpv plays back;
    // the video track, if any, is simply not displayed.
    "webm", "mka", "mkv"
};

static const QSet<QString> kSupportedExtensionSet(kSupportedExtensions.cbegin(),
                                                 kSupportedExtensions.cend());
```

2. Rewrite the two methods:

```cpp
QStringList Scanner::supportedExtensions()
{
    return kSupportedExtensions; // API preserved for callers/tests
}

bool Scanner::isSupportedAudioFile(const QString &filePath)
{
    return kSupportedExtensionSet.contains(QFileInfo(filePath).suffix().toLower());
}
```

3. Add `#include <QSet>` to the includes. Do **not** touch the public API:
   `tst_scanner` (tst_scanner.cpp lines 98–205) exercises
   `isSupportedAudioFile()` with all extensions and case variants.

**Verify.** `ctest --test-dir build` — `tst_scanner` must still pass 100%.

**Commit.** `perf(library): use a static extension set in Scanner`

---

## Option B — Controller caching + N+1 removal

### B1 — Cache `songs / albums / artists` across property reads `[required]`

**Why.** `Q_PROPERTY(albums READ albums NOTIFY libraryChanged)` executes
`getAllAlbums()` (GROUP BY + NOT EXISTS) on **every** read, and QML reads
`Library.albums` several times per binding evaluation. `songs()` rebuilds N
`QVariantMap` per read. These lists change only when the library changes, so
they can be cached and rebuilt there.

**Files:** `src/library/LibraryController.h`, `src/library/LibraryController.cpp`.

**Do.**

1. `LibraryController.h`, private section — add:

```cpp
    // QML-facing views of the library (QVariant wrappers). Built once per
    // library change instead of on every property read; implicitly shared,
    // so returning them is cheap.
    mutable QVector<QVariant> m_cachedSongs;
    mutable QVector<QVariant> m_cachedAlbums;
    mutable QVector<QVariant> m_cachedArtists;
    mutable bool m_cachesStale = true;

    void rebuildCaches() const;
```

2. `LibraryController.cpp` — make the three getters lazy over the cache:

```cpp
QVector<QVariant> LibraryController::songs() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedSongs;
}

QVector<QVariant> LibraryController::albums() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedAlbums;
}

QVector<QVariant> LibraryController::artists() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedArtists;
}

void LibraryController::rebuildCaches() const
{
    m_cachedSongs.clear();
    m_cachedSongs.reserve(m_songs.size());
    for (const Song &song : m_songs)
        m_cachedSongs.append(song.toVariantMap());

    const QList<Album> albums = m_db.getAllAlbums();
    m_cachedAlbums.clear();
    m_cachedAlbums.reserve(albums.size());
    for (const Album &album : albums)
        m_cachedAlbums.append(albumToVariant(album));

    const QList<Artist> artists = m_db.getAllArtists();
    m_cachedArtists.clear();
    m_cachedArtists.reserve(artists.size());
    for (const Artist &artist : artists)
        m_cachedArtists.append(artistToVariant(artist));

    m_cachesStale = false;
}
```

3. Mark the cache stale wherever `m_songs` is refreshed:
   - `handleScanFinished()`: right after `m_songs = m_db.getAllSongs().toVector();`
   - `refresh()`:

```cpp
void LibraryController::refresh()
{
    m_songs = m_db.getAllSongs().toVector();
    m_cachesStale = true;
    emit libraryChanged();
}
```

`removeSongs`, `ignorePath` and `unignorePath` already funnel through
`refresh()`, so nothing else needs touching. `search()` stays DB-backed.

**Verify.** `ctest --test-dir build` (tst_library, tst_library_scan). Manual:
switch sections; switching no longer runs an aggregate SQL per property read.

**Commit.** `perf(library): cache QML-facing library views between changes`

### B2 — One-pass `collectSongs()` in Albums/Artists views `[required]`

**Why.** `AlbumsView.collectSongs()` calls `Library.songsByAlbum()` once per
album and `ArtistsView.collectSongs()` calls `Library.songsByArtist()` once
per artist (an N+1 query pattern: one SQL query per album/artist on
"Play all"/"Shuffle").

**Files:** `resources/qml/views/AlbumsView.qml`, `resources/qml/views/ArtistsView.qml`.

**Do.** Replace the body of `collectSongs()` (no signature change):

`AlbumsView.qml`:

```qml
    // Resolve the visible albums' songs in ONE pass over the already-loaded
    // library (Library.songs excludes hidden songs) instead of a SQL query
    // per album.
    function collectSongs() {
        var buckets = {}
        var all = Library.songs
        for (var i = 0; i < all.length; ++i) {
            var s = all[i]
            var key = String(s.album ? s.album : "") + "\u0000"
                    + String(s.artist ? s.artist : "")
            if (!buckets[key])
                buckets[key] = []
            buckets[key].push(s)
        }
        var out = []
        var list = albumsView.albums
        for (var j = 0; j < list.length; ++j) {
            var album = list[j]
            var k2 = String(album.title ? album.title : "") + "\u0000"
                   + String(album.artist ? album.artist : "")
            var songs = buckets[k2]
            if (!songs)
                continue
            // Restore songsByAlbum() ordering: disc, track, title.
            songs.sort(function (a, b) {
                return (a.discNumber - b.discNumber)
                       || (a.trackNumber - b.trackNumber)
                       || String(a.title).localeCompare(String(b.title))
            })
            for (var m = 0; m < songs.length; ++m)
                out.push(songs[m])
        }
        return out
    }
```

`ArtistsView.qml`:

```qml
    function collectSongs() {
        var buckets = {}
        var all = Library.songs
        for (var i = 0; i < all.length; ++i) {
            var s = all[i]
            var name = String(s.artist ? s.artist : "")
            if (!buckets[name])
                buckets[name] = []
            buckets[name].push(s)
        }
        var out = []
        var list = artistsView.artists
        for (var j = 0; j < list.length; ++j) {
            var songs = buckets[String(list[j].name ? list[j].name : "")]
            if (!songs)
                continue
            // songsByArtist() ordered by album_id, disc, track, title.
            songs.sort(function (a, b) {
                return String(a.album).localeCompare(String(b.album))
                       || (a.discNumber - b.discNumber)
                       || (a.trackNumber - b.trackNumber)
                       || String(a.title).localeCompare(String(b.title))
            })
            for (var m = 0; m < songs.length; ++m)
                out.push(songs[m])
        }
        return out
    }
```

> Ordering note: the old `songsByArtist()` ordered by the DB `album_id`
> (insertion order); the JS version sorts alphabetically by album title —
> arguably nicer, but a deliberate, minor difference. If exact DB order is
> required, add `albumId` to `Song::toVariantMap()` (`src/models/Song.cpp`)
> and sort on that instead. Note this in the commit message either way.

**Verify.** `ctest --test-dir build`; smoke run; manual: Albums → Play all /
Shuffle, Artists → Play all — finished instantly and order looks right.

**Commit.** `perf(ui): resolve album/artist play lists in a single pass`

---

## Option C — `QAbstractListModel` foundation

Honest expectation: unlike A/B this is **not** an immediate user-visible win;
its value is architectural — role-based delegates (also valid for plain JS
arrays), a snapshot vector without per-read `QVariantMap` churn, and the base
for future delta updates / C++-side filtering. Do it on top of A + B, one
commit per step, each step green.

### C1 — `SongListModel` (new class) `[step 1]`

**Files:** new `src/library/SongListModel.h` / `.cpp`; `CMakeLists.txt`;
`tests/CMakeLists.txt`; `tests/unit/tst_models.cpp`.

`SongListModel.h`:

```cpp
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "../models/Song.h"

class SongListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationRole,
        ArtworkRole,
        FavoriteRole,
    };

    explicit SongListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSongs(const QVector<Song> &songs);

    // For QML scripts that still need a map (e.g. pushing into Player queue).
    Q_INVOKABLE QVariantMap songData(int row) const;
    Q_INVOKABLE int rowOfId(int id) const;

private:
    QVector<Song> m_songs;
};
```

`SongListModel.cpp`:

```cpp
#include "SongListModel.h"

SongListModel::SongListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_songs.size();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return {};
    const Song &song = m_songs.at(index.row());
    switch (role) {
    case IdRole:             return song.id;
    case PathRole:           return song.path;
    case TitleRole:          return song.title;
    case ArtistRole:         return song.artist;
    case AlbumRole:          return song.album;
    case GenreRole:          return song.genre;
    case YearRole:           return song.year;
    case TrackNumberRole:    return song.trackNumber;
    case DiscNumberRole:     return song.discNumber;
    case DurationRole:       return song.duration;
    case ArtworkRole:        return song.artworkPath;
    case FavoriteRole:       return song.favorite;
    default:                 return {};
    }
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
    return {
        { IdRole, "id" },          { PathRole, "path" },
        { TitleRole, "title" },    { ArtistRole, "artist" },
        { AlbumRole, "album" },    { GenreRole, "genre" },
        { YearRole, "year" },      { TrackNumberRole, "trackNumber" },
        { DiscNumberRole, "discNumber" }, { DurationRole, "duration" },
        { ArtworkRole, "artwork" },{ FavoriteRole, "favorite" },
    };
}

void SongListModel::setSongs(const QVector<Song> &songs)
{
    beginResetModel();
    m_songs = songs;
    endResetModel();
}

QVariantMap SongListModel::songData(int row) const
{
    if (row < 0 || row >= m_songs.size())
        return {};
    return m_songs.at(row).toVariantMap();
}

int SongListModel::rowOfId(int id) const
{
    for (int i = 0; i < m_songs.size(); ++i)
        if (m_songs.at(i).id == id)
            return i;
    return -1;
}
```

**Wiring.**

1. `CMakeLists.txt`: add `src/library/SongListModel.cpp` to `SOURCES`
   (next to `LibraryController.cpp`).
2. `tests/CMakeLists.txt`: add
   `${CMAKE_SOURCE_DIR}/src/library/SongListModel.cpp` to
   `tst_models_PROD_SOURCES` (line 41).
3. `tests/unit/tst_models.cpp`: add a case — build a `SongListModel`, set 3
   songs, assert `rowCount() == 3`, `data()` per role, `rowOfId()`,
   `songData()` on an out-of-range row returns `{}`.
4. `LibraryController`: add member `SongListModel m_songsModel;` and fill it
   in `refresh()` / `handleScanFinished()` next to the cache staleness
   (`m_songsModel.setSongs(m_songs);`). Expose it (temporarily) as a
   `Q_INVOKABLE SongListModel *songsModel()` for step C2's binding.

**Verify.** Build + `ctest` (tst_models extended). Nothing QML-side uses it yet.

**Commit.** `feat(library): add SongListModel snapshot list model`

### C2 — SongsView renders through the model `[step 2]`

**Why.** Main songs list is the hot path. Role-based delegates work for BOTH
the C++ model and plain JS arrays (`model.title` proxies to the array
element), so this migration is mechanical.

**Files:** `resources/qml/main.qml`, `resources/qml/views/SongsView.qml`.

**Do.**

1. `SongsView.qml`: add `property var songsModel: null`; the `ListView`
   model becomes:

```qml
            model: songsView.songsModel ? songsView.songsModel : songsView.songs
```

2. Rewrite the delegate to role bindings (key changes; keep geometry and
   theming): drop `required property var modelData`, use
   `required property int index`, and read `model.id / model.title /
   model.artist / model.album / model.duration`:

```qml
            delegate: Rectangle {
                id: songRow
                width: songList.width
                height: 40
                radius: 6

                required property int index

                property bool isCurrent: Player.currentSong !== undefined
                                         && Player.currentSong !== null
                                         && Player.currentSong.id !== undefined
                                         && Player.currentSong.id === model.id
                property bool isSelected: songsView.selectedCount > 0
                                          && songsView.selectedIds.has(model.id)
                // ...color/border/behaviour unchanged from the current delegate...
                MouseArea {
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            songsView.onRowRightClicked(songRow.index, model.id)
                            return
                        }
                        songsView.onRowClicked(songRow.index, model.id, mouse.modifiers)
                    }
                }

                Text { text: model.title ? model.title : qsTr("Unknown title") /* ... */ }
                Text { text: model.artist ? model.artist : "" /* ... */ }
                Text { text: model.album ? model.album : "" /* ... */ }
                Text { text: songsView.formatDuration(model.duration) /* ... */ }

                ToolButton {
                    onClicked: {
                        songsView.addFromSelection = false
                        songsView.targetSongIds = [model.id]
                        addToMenu.popup()
                    }
                }
            }
```

3. `main.qml`, `songsComponent`: pass the model when not searching:

```qml
    Component {
        id: songsComponent
        SongsView {
            songs: root.filteredSongs !== null ? root.filteredSongs : Library.songs
            songsModel: root.filteredSongs !== null ? null : Library.songsModel
        }
    }
```

The JS functions (`playSong`, `playAllFromStart`, `findSongById`, selection)
keep working on the `songs` array — data flows stay unchanged; only the
render path uses the model. `Library.songsModel` becomes a context property
(see C1), or a `Q_PROPERTY(... songsModel ...)`.

**Verify.** Build + `ctest`; smoke run; manual: search empty/non-empty,
select, play, delete — identical behaviour with model and with array path.

**Commit.** `perf(ui): render songs list through SongListModel roles`

### C3 — Role-based delegates in drill-down and playlists `[step 3]`

**Files:** `resources/qml/views/SongListDetailView.qml`,
`resources/qml/views/PlaylistsView.qml`.

**Why / do.** The same mechanical change as C2 (delegates use
`model.id / model.title / …`). These views keep receiving JS arrays
(`Library.songsByAlbum()`, `Playlists.getPlaylistSongs()`), which still
satisfy role-style bindings. No C++ changes required here. Only convert the
delegate bodies; do not rework the array plumbing.

**Verify.** Build + `ctest`; manual: album drill-down, artist drill-down,
playlist — rows render and play identically.

**Commit.** `refactor(ui): role-based delegates in detail and playlist views`

### C4 — Search through the model + incremental updates `[done]`

- [x] Search results render through `SongListModel` (`Library.searchModel`,
  `setSearch`, `searchActive`; `main.qml` routes
  `searchActive ? searchModel : songsModel`) — `fd2ed0d`.
- [x] Row-level incremental updates. `SongListModel::applySnapshot()` diffs a
  new snapshot by id and emits `rowsInserted` / `rowsRemoved` / `dataChanged`
  instead of `beginResetModel()`; `handleScanFinished()` and `refresh()` feed
  it. A kept id whose sort key changed falls back to a full reset. Tests:
  `tst_models` `testSongListModelIncremental`.
- [x] Sort / favorite / play-count in the model. Added `PlayCountRole` /
  `LastPlayedRole` (the model now mirrors every `Song` field) and
  `SongListModel::sort(role, order)` so ordering lives in C++, not in QML.
  Tests: `tst_models` `testSongListModelSort`.

---

## Backlog (unprioritised micro-optimisations)

Not part of the three options; small, isolated, safe later. **All done**
(branch `perf/backlog-microopts` + `build/static-core-lib`):

- [x] **Double `currentSongChanged` on load.** `updateCurrentSong()` now only
  sets state; `loadSong()` keeps the single emission. `tst_player` asserts
  exactly one.
- [x] **`pollTick` always runs.** Early-returns after `drainMpvEvents()` when
  `Stopped && !m_pendingLoad`.
- [x] **MPRIS position spam.** `positionChanged` is coalesced to one relay per
  whole second in `MprisPlayer.cpp`.
- [x] **Scanner double stat.** Uses `it.fileInfo()`; the redundant
  `isSymLink()` check is gone (`NoSymLinks` filters). `testScanSkipsSymlinks`
  pins that links are neither reported nor descended into.
- [x] **Tests recompile production sources.** `altisidora_core` (STATIC) holds
  all production sources except `main.cpp`; the app and every test link it.
  Clean build 106 s → 55 s (~1.9×); production sources compiled 43× → 10×.
- [x] **Shared DB connection.** Already documented in the constructor; the
  destructor now notes the teardown-order caveat and the centralisation path.

---

## Guardrails

- Keep the public API of every method tests call (`addSong`,
  `supportedExtensions`, `isSupportedAudioFile`, …) untouched; add the new
  methods beside them.
- Do not mix options in one commit; each commit above is a reviewable work
  unit with its own verification.
- `git switch -c` before starting an option; never edit on
  `review/project-cleanup` directly.
- Update `docs/PROJECT_STATUS.md` only if a number or behaviour claim changes
  (none of A–C should).
- After each option: tree clean, build exit 0, `ctest` 12/12, smoke run OK.

## Next step

A–C (C4 included) and the backlog are **done and verified** (see Status above).
Nothing is outstanding from this plan. The next work is outside it, tracked in
`docs/PROJECT_STATUS.md`: artwork extraction/display, and testing the
`PKGBUILD` on CachyOS/Arch.