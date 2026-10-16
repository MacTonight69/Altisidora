# Project Status

Last updated: 2026-10-16

## Summary

Altisidora has a working core **and a functional GUI**: the full module
structure, build system, implementations, tests and a QML interface wired to
the controllers are in place. All tests pass.

- **Build:** working (CMake + Qt 6.12 + TagLib 2.3)
- **Tests:** 13/13 test suites passing (100%)
- **Status of functionality:** scan → browse → search → play works end to end
  with a real music folder; tags (title/artist/album/year/artwork) can be
  edited individually or in batch and survive rescans.

## Modules

| Module | Path | Status | Notes |
|---|---|---|---|
| Models | `src/models/` | Implemented | Song, Artist, Album, Playlist (plain structs + QVariantMap conversion) |
| Database | `src/database/` | Implemented | `DatabaseManager`: SQLite, prepared statements, schema version table, CRUD. Songs are normalized (artist/album/genre rows resolved on insert); reads JOIN names back and artist/album/genre/playlist queries return `songCount`. Schema v4 adds an `edited` flag: `upsertSong()` CASE-guards the tag-derived columns so a rescan refreshes the file fingerprint (`mtime`/`size`) without clobbering user edits. Tag-update paths: `updateSong`/`updateSongs` (atomic batch), `renameArtist`/`setAlbumIdentity` (collision-refusing), `setArtwork` (content-hash cache), `setAlbumArtwork` (album cover) |
| Audio | `src/audio/` | Implemented | `PlayerController` wrapping a libmpv session (10-band EQ, repeat-one); `load(url)` for raw URLs and `loadSong(map)` to keep library metadata for the UI |
| System Integration | `src/media/` | Implemented | `MprisMediaIntegration`: MPRIS v2 (`org.mpris.MediaPlayer2.altisidora`) over Qt DBus behind the platform-neutral `MediaIntegration` seam; `main.cpp` only calls `createMediaIntegration()`, and a build without D-Bus gets `NullMediaIntegration` |
| Library | `src/library/` | Implemented | `Scanner` (recursive, extension filter), `MetadataReader` (TagLib, falls back to file name for title, re-decodes Shift-JIS mojibake in mislabeled Latin-1 tags, extracts embedded artwork into a content-hash cache under `AppDataLocation/artwork/`), `LibraryController` (scan/refresh, search through `setSearch()`/`searchModel`, plus QML-facing tag mutators), `SongListModel` (role-based render model for the song lists). A scan also reconciles the library: songs whose file disappeared since the last scan are dropped from the list and the database, and `scanFinished(removedCount)` reports how many (see the pruning rules under Known limitations) |
| Application | `src/app/` | Implemented | `PlaylistController` (CRUD + song counts), `SettingsController` (QSettings persistence) |
| GUI | `resources/qml/` | Implemented | Sidebar navigation, Songs/Albums/Artists/Playlists/Settings views, search bar, player controls (seek/volume/errors), scan flow with overlay, add-to-playlist menu, multi-selection of songs (right-click selects, Ctrl/Shift+right-click extend, Ctrl+A selects all) with batch add to playlist. Tag editing: a per-row actions menu ("Edit tags…", "Add to playlist…", "Delete…") edits title/artist/album/year plus artwork, adds the song to a playlist, or deletes it through the same Settings-driven flow as Ctrl+D; the selection bar batch-reassigns album or artist (field-level only), and the album/artist drill-down header renames the collection. A scan that drops songs whose file no longer exists reports how many in a transient notice above the player bar |
| Themes | `resources/qml/themes/` | Implemented | `Theme.qml` is a context property (not a QML singleton) plus the `Themes.js` catalog; `Theme.apply()` re-assigns every role live so the whole UI restyles. A theme carries a palette, `fontFamily`, `fontScale` and `sidebarWidth`; `Coco`/`Onyx` opt into 1.4, `Anarchism` into 1.9 with a 260px sidebar, and every other theme — the Baskervville `Maya` included — uses the default 1.0/210. `SettingsController` persists the selection and migrates the dead `Anarchy` key to `Anarchism` |
| Tests | `tests/` | Implemented | 11 unit + 2 integration suites, all passing; see below |
| Packaging | root | Initial | `PKGBUILD`, `.desktop` |

## Public API

The interfaces documented in `docs/API_CONTRACTS.md` are implemented by:

- `PlayerController` → playback control and state, current-song metadata
- `LibraryController` → scan/refresh, search (`setSearch`/`searchModel`) and QML-friendly models
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
| tst_mpris | unit (real MprisMediaIntegration over a private D-Bus session) | Real tests |
| tst_media | unit (MediaIntegration seam: null-backend contract, factory picks the compiled backend) | Real tests |
| tst_detail_view | unit (real album/artist drill-down view + bundled theme QML) | Real tests |
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

**Publish**

1. Push tag `v0.1.0` upstream and generate real PKGBUILD checksums (see the
   TODO in `PKGBUILD`).

**Export targets** — guides in `docs/`

2. AppImage — **done.** See [`docs/PACKAGING_LINUX.md`](PACKAGING_LINUX.md).
   `bash packaging/appimage/make.sh` builds
   `dist/Altisidora-0.1.0-x86_64.AppImage` (122,497,528 bytes, ~122 MB) in an
   `ubuntu:22.04` container with a glibc 2.35 floor, and it starts in `debian:12`
   and `ubuntu:24.04` with the desktop runtime installed. Only publishing — a
   `v0.1.0` tag and real `sha256sums` — remains.
3. Windows — see [`docs/PACKAGING_WINDOWS.md`](PACKAGING_WINDOWS.md). The
   platform seam is in place and the build already configures, builds and links
   without D-Bus (proven on Linux with `-DALTISIDORA_WITH_DBUS=OFF`), so the
   remaining work is the toolchain decision (guide §3: MSYS2 or MSVC + vcpkg),
   the Windows-only resources (`.ico`, application manifest) and the packaging
   choice (§5). None of it can be done from this Linux box.

**Review gate** — the accumulated review range cannot be reviewed as one candidate

4. Receipt-driven development is on by default, and the native gate refuses the
   accumulated range `79b9544..HEAD` with `lens_context_budget_exceeded`: the
   candidate's complete reviewer evidence exceeds the lens context budget, so no
   review authority is created, and retrying that same candidate cannot succeed
   because immutable candidate evidence is never truncated. The prescribed
   continuation is a chained sequence of smaller candidates, one review per
   slice. The range holds 27 commits (~4 460 authored lines); the largest single
   commit (`0a9615e`, 1 159 lines) may itself exceed the budget, so the chain has
   to be walked rather than divided evenly. This is a process gap, not a defect
   in the product.

Still open from Known limitations above: the now-playing refresh API, and the
absence of CI (which the AppImage pipeline still needs).
