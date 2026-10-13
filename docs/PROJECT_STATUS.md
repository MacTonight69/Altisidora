# Project Status

Last updated: 2026-10-12

## Summary

Altisidora has a working core **and a functional GUI**: the full module
structure, build system, implementations, tests and a QML interface wired to
the controllers are in place. All tests pass.

- **Build:** working (CMake + Qt 6.12 + TagLib 2.3)
- **Tests:** 12/12 test suites passing (100%)
- **Status of functionality:** scan → browse → search → play works end to end
  with a real music folder; tags (title/artist/album/year/artwork) can be
  edited individually or in batch and survive rescans.

## Modules

| Module | Path | Status | Notes |
|---|---|---|---|
| Models | `src/models/` | Implemented | Song, Artist, Album, Playlist (plain structs + QVariantMap conversion) |
| Database | `src/database/` | Implemented | `DatabaseManager`: SQLite, prepared statements, schema version table, CRUD. Songs are normalized (artist/album/genre rows resolved on insert); reads JOIN names back and artist/album/genre/playlist queries return `songCount`. Schema v4 adds an `edited` flag: `upsertSong()` CASE-guards the tag-derived columns so a rescan refreshes the file fingerprint (`mtime`/`size`) without clobbering user edits. Tag-update paths: `updateSong`/`updateSongs` (atomic batch), `renameArtist`/`setAlbumIdentity` (collision-refusing), `setArtwork` (content-hash cache), `setAlbumArtwork` (album cover) |
| Audio | `src/audio/` | Implemented | `PlayerController` wrapping a libmpv session (10-band EQ, repeat-one); `load(url)` for raw URLs and `loadSong(map)` to keep library metadata for the UI |
| System Integration | `src/mpris/` | Implemented | `MprisPlayer`: MPRIS v2 (`org.mpris.MediaPlayer2.altisidora`) over Qt DBus, registered in `src/main.cpp` |
| Library | `src/library/` | Implemented | `Scanner` (recursive, extension filter), `MetadataReader` (TagLib, falls back to file name for title, re-decodes Shift-JIS mojibake in mislabeled Latin-1 tags, extracts embedded artwork into a content-hash cache under `AppDataLocation/artwork/`), `LibraryController` (scan/search/refresh + QML-facing tag mutators). A scan also reconciles the library: songs whose file disappeared since the last scan are dropped from the list and the database (see the pruning rules under Known limitations) |
| Application | `src/app/` | Implemented | `PlaylistController` (CRUD + song counts), `SettingsController` (QSettings persistence) |
| GUI | `resources/qml/` | Implemented | Sidebar navigation, Songs/Albums/Artists/Playlists/Settings views, search bar, player controls (seek/volume/errors), scan flow with overlay, add-to-playlist menu, multi-selection of songs (right-click selects, Ctrl/Shift+right-click extend, Ctrl+A selects all) with batch add to playlist. Tag editing: a per-row actions menu ("Edit tags…", "Add to playlist…", "Delete…") edits title/artist/album/year plus artwork, adds the song to a playlist, or deletes it through the same Settings-driven flow as Ctrl+D; the selection bar batch-reassigns album or artist (field-level only), and the album/artist drill-down header renames the collection |
| Tests | `tests/` | Implemented | 10 unit + 2 integration suites, all passing; see below |
| Packaging | root | Initial | `PKGBUILD`, `.desktop` |

## Public API

The interfaces documented in `docs/API_CONTRACTS.md` are implemented by:

- `PlayerController` → playback control and state, current-song metadata
- `LibraryController` → scan/refresh/search and QML-friendly models
- `PlaylistController` → playlist operations
- `SettingsController` → persistent settings

QML accesses them as context properties (`Player`, `Library`, `Playlists`,
`AppSettings`, `Theme`) set in `src/main.cpp`.

## Tests

Run with `ctest --output-on-failure` from the build directory.

| Suite | Type | Status |
|---|---|---|
| tst_database | unit (real DatabaseManager + SQLite) | Real tests |
| tst_models | unit (Song/Album/Artist/Playlist) | Real tests |
| tst_player | unit (real PlayerController, backend-independent) | Real tests |
| tst_scanner | unit (real Scanner) | Real tests |
| tst_metadata | unit (real MetadataReader + TagLib) | Real tests |
| tst_library | unit (real LibraryController) | Real tests |
| tst_playlist | unit (real PlaylistController) | Real tests |
| tst_settings | unit (real SettingsController) | Real tests |
| tst_theme_fonts | unit (real SettingsController + bundled theme QML) | Real tests |
| tst_mpris | unit (real MprisPlayer over a private D-Bus session) | Real tests |
| tst_library_scan | integration (real scan + persistence across restart) | Real tests |
| tst_playback | integration (scan → library → `loadSong` data flow) | Actual audio playback needs a media backend, not exercised by the suite |

All test binaries run headless (`QT_QPA_PLATFORM=offscreen` set via CTest).

Test isolation:

- `QStandardPaths::setTestModeEnabled(true)` redirects the database away from
  the user's real data.
- `QSettings::setPath` redirects settings to a temporary directory.
- Each test run removes the test database files before starting.

## Known limitations

- Legacy-encoding tags: ID3v2 text mislabeled as ISO-8859-1 that actually
  contains Shift-JIS bytes (a common legacy ripper bug for Japanese music)
  is now re-decoded. When tag text carries the Shift-JIS lead-byte
  signature — C1 controls U+0081-U+009F, which genuine Latin-1 accents
  never contain — the reader recovers the original bytes, decodes them as
  Shift-JIS, and accepts the result only if it contains full-width Japanese
  and re-encodes back to the exact original bytes. Genuine Latin-1 text
  (French/German/Spanish accents, CP1252 smart quotes) is never converted.
  Residual limitations: mojibake composed only of E0-zone lead bytes
  (rare kanji without any 0x81-0x9F lead byte) is not converted, and tags
  in other legacy encodings (EUC-KR, Big5, KOI8-R) are not handled.
- The playback queue is set from the visible list in the Songs, Playlists and
  album/artist drill-down views (double-click or "Play all" loads that list as
  the navigation context for `next()`/`previous()`).
- `tst_playback` covers the data flow up to `PlayerController::loadSong()`;
  actual audio playback (needs a media backend with decoders) is not exercised
  by the suite.
- `LibraryController` does not load existing songs in its constructor; callers
  must invoke `refresh()` (done once in `src/main.cpp`; QML does not call it).
- Scan pruning: a scan forgets the rows whose file disappeared since the last
  one (deleted in a file manager, moved, or renamed), so a rescan cannot leave
  phantom entries behind. Removal is deliberately conservative and happens only
  when all of these hold: the row's path is inside the scanned root (scanning
  folder A never forgets the songs of folder B), the walk did not see the path,
  the file itself does not exist, and its parent directory still exists. The
  last rule separates a real deletion from an unavailable volume: a whole folder
  that vanished at once (unmounted drive, unplugged disk) looks like "every file
  missing" and is left untouched until the volume is back, and a scan of a root
  that does not exist prunes nothing. Hidden songs are kept too — their file is
  still on disk, the walk merely skips it.
- Migrations: there is no standalone migration runner; `DatabaseManager`
  applies versioned in-place migrations in `createTables()` (v2 file-stat
  columns, v3 artwork backfill, v4 `edited` flag) and records the version in
  the `schema_version` table. The v4 migration is purely additive and was
  verified against a copy of a real 498-song library (byte-identical data
  before/after).
- Tag editing: an edited row is protected from rescans by the `edited` flag
  (`upsertSong()` CASE-guards every tag-derived column), so user edits survive
  a rescan even when the file on disk still carries the old tags; `mtime` and
  `size` always refresh so the rescan never loops.
- Tag editing, single-song artwork: the save path is not atomic.
  `Library.updateSong` commits the metadata first and `Library.setArtwork`
  runs after it; if the artwork write fails, the metadata is already
  persisted while the dialog reports a failure and stays open.
- Tag editing, now-playing staleness: editing the currently playing song
  updates the library but the now-playing bar keeps the pre-edit metadata
  until the player reloads the song (needs a C++ refresh API; out of scope).
- Continuous integration (GitHub Actions) was removed and is not present in the
  repo; validate changes by running `ctest` headless locally. The PKGBUILD
  builds and packages correctly on CachyOS (verified end-to-end: makepkg,
  `pacman -Qp`, contents, `desktop-file-validate`, packaged-binary smoke run);
  publishing still needs tag `v0.1.0` pushed upstream and real checksums (see
  the TODO in `PKGBUILD`).

## Next steps

1. Publish: push tag `v0.1.0` upstream and generate real PKGBUILD checksums
   (see the TODO in `PKGBUILD`).
