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
seek(position)
setVolume(volume)
load(url)
loadSong(songMap)
```

`loadSong()` receives the QVariantMap of a library song (as exposed by
`LibraryController`/`PlaylistController`) so the UI can display title, artist
and highlight the current row without touching the database.

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
removeSong()
moveSong()
getPlaylists()
```

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
