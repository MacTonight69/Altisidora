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
search(query)
songs        (Q_PROPERTY)
albums       (Q_PROPERTY)
artists      (Q_PROPERTY)
genres       (Q_PROPERTY)
```

The controller exposes models suitable for consumption by QML: properties are
arrays of QVariantMap keyed with the `Song`/`Album`/`Artist`/`Genre` field
names (`id`, `path`, `title`, `artist`, `album`, `duration`, `songCount`, …)
and are notified through `libraryChanged()`.

`scan(directory)` is asynchronous: the filesystem walk and metadata extraction
run on a worker thread, results are committed on the main thread, and QML is
notified through `scanStarted()`/`scanFinished()`/`libraryChanged()`.

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
moveSong()
getPlaylists()
```

`addSongs()` inserts a batch of song ids into a playlist in order and notifies
`playlistsChanged()` once for the whole batch. The GUI uses it to add the
multi-selected songs in the Songs view (the selection bar passes the array of
ids; the per-row "+ Playlist" button passes a single-element array).

---

## SettingsController

Responsibilities:

```text
getSetting()
setSetting()
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
artwork
dateAdded
lastPlayed
playCount
favorite
```

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
