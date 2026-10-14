# API Contracts

This document defines the public interfaces shared between project modules.

Interfaces should remain stable whenever possible.

Major changes require architectural review.

---

## PlayerController

The application-level audio interface should expose operations conceptually equivalent to:

```text
play()
pause()
stop()
next()
previous()
setQueue(songList)
seek(position)
setVolume(volume)
load(url)
loadSong(songMap)
```

`loadSong()` receives the QVariantMap of a library song (as exposed by
`LibraryController`/`PlaylistController`) so the UI can display title, artist
and highlight the current row without touching the database.

`setQueue()` receives the visible song list (as exposed by the Songs/Playlists
views) and makes it the navigation context for `next()`/`previous()`;
`loadSong()` locates the loaded song inside the queue by its `path`. When the
last track ends (`EndOfMedia`) the next queued song starts automatically.

State information should include:

```text
Stopped
Loading
Playing
Paused
Error
```

The GUI should interact with the PlayerController rather than directly with the audio backend.

---

## LibraryController

Responsibilities:

```text
scan(directory)
refresh()
setSearch(query) / searchModel / searchActive
songs        (Q_PROPERTY)
albums       (Q_PROPERTY)
artists      (Q_PROPERTY)
```

The controller exposes models suitable for consumption by QML: properties are
arrays of QVariantMap keyed with the `Song`/`Album`/`Artist` field
names (`id`, `path`, `title`, `artist`, `album`, `duration`, `songCount`, …)
and are notified through `libraryChanged()`.

`scan(directory)` is asynchronous: the filesystem walk and metadata extraction
run on a worker thread, results are committed on the main thread, and QML is
notified through `scanStarted()`/`scanFinished(removedCount)`/`libraryChanged()`.
`scanFinished(removedCount)` also carries how many rows the scan dropped
because their file is gone; QML reports that count in a transient notice, so
the removal is never silent. The count is 0 when the database refused the
removal batch.

A scan is also a reconciliation: a song whose file no longer exists inside the
scanned directory is dropped from the library (list and database). The removal
is deliberately conservative — it requires the row's path to be inside the
scanned root, the walk to have missed it, the file to be confirmed absent, and
its parent directory to still exist — so an unavailable volume (unmounted
drive, unplugged disk) never costs the user its metadata. See
`LibraryController::ScanResult` and the pruning notes in `LibraryController.cpp`.

Artist, album and genre names stored on songs are resolved from normalized
tables (`artists`, `albums`, `genres`) at scan time and joined back on read.

---

## PlaylistController

Responsibilities:

```text
createPlaylist()
deletePlaylist()
renamePlaylist()
addSong()
addSongs(playlistId, songIds)
removeSong()
getPlaylistSongs(playlistId)
playlists            (Q_PROPERTY)
```

`addSongs()` inserts a batch of song ids into a playlist in order and notifies
`playlistsChanged()` once for the whole batch. The GUI uses it to add the
multi-selected songs in the Songs view (the selection bar passes the array of
ids; the per-row "+ Playlist" button passes a single-element array).

`playlists` is exposed as a `Q_PROPERTY` (list of playlist maps) rather than a
`getPlaylists()` method. Reordering a song inside a playlist (`moveSong`) is not
implemented.

---

## SettingsController

Responsibilities:

```text
musicDirectory       (Q_PROPERTY)
theme                (Q_PROPERTY)
fontScale            (Q_PROPERTY)
deleteFilesFromDisk  (Q_PROPERTY)
save()
load()
```

Settings must persist between application sessions.

---

## Song

Conceptually contains:

```text
id
path
title
artist
album
genre
year
trackNumber
discNumber
duration
artworkPath
dateAdded
lastPlayed
playCount
favorite
```

The C++ struct field is `artworkPath`; `LibraryController` exposes it to QML
under the key `artwork`.

`artwork` is populated at scan time from the audio file's embedded
front-cover picture (falling back to the first picture found): it holds the
absolute path of the extracted image, cached under a content-hash name in
`AppDataLocation/artwork/`, or an empty string when the file embeds no art.

---

## Error handling

Public APIs should communicate failures without exposing unnecessary implementation details.

The specific mechanism may evolve as the architecture develops.

---

## API change policy

Before changing a public interface:

1. Identify dependent modules.
2. Evaluate compatibility.
3. Update this document.
4. Update affected implementations.
5. Update tests.
6. Verify the complete project.
