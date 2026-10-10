# Project Status

Last updated: 2026-10-09

## Summary

Altisidora has a working core **and a functional GUI**: the full module
structure, build system, implementations, tests and a QML interface wired to
the controllers are in place. All tests pass.

- **Build:** working (CMake + Qt 6.12 + TagLib 2.3)
- **Tests:** 12/12 test suites passing (100%)
- **Status of functionality:** scan → browse → search → play works end to end
  with a real music folder.

## Modules

| Module | Path | Status | Notes |
|---|---|---|---|
| Models | `src/models/` | Implemented | Song, Artist, Album, Playlist (plain structs + QVariantMap conversion) |
| Database | `src/database/` | Implemented | `DatabaseManager`: SQLite, prepared statements, schema version table, CRUD. Songs are normalized (artist/album/genre rows resolved on insert); reads JOIN names back and artist/album/genre/playlist queries return `songCount` |
| Audio | `src/audio/` | Implemented | `PlayerController` wrapping a libmpv session (10-band EQ, repeat-one); `load(url)` for raw URLs and `loadSong(map)` to keep library metadata for the UI |
| System Integration | `src/mpris/` | Implemented | `MprisPlayer`: MPRIS v2 (`org.mpris.MediaPlayer2.altisidora`) over Qt DBus, registered in `src/main.cpp` |
| Library | `src/library/` | Implemented | `Scanner` (recursive, extension filter), `MetadataReader` (TagLib, falls back to file name for title), `LibraryController` (scan/search/refresh) |
| Application | `src/app/` | Implemented | `PlaylistController` (CRUD + song counts), `SettingsController` (QSettings persistence) |
| GUI | `resources/qml/` | Implemented | Sidebar navigation, Songs/Albums/Artists/Playlists/Settings views, search bar, player controls (seek/volume/errors), scan flow with overlay, add-to-playlist menu, multi-selection of songs (click / Ctrl / Shift / Ctrl+A) with batch add to playlist |
| Tests | `tests/` | Implemented | 10 unit + 2 integration suites, all passing; see below |
| Packaging | root | Initial | `PKGBUILD`, `.desktop`, GitHub Actions CI (Ubuntu) |

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
| tst_playback | integration (scan → library → `loadSong` data flow) | Actual audio playback needs a media backend, out of CI scope |

All test binaries run headless (`QT_QPA_PLATFORM=offscreen` set via CTest).

Test isolation:

- `QStandardPaths::setTestModeEnabled(true)` redirects the database away from
  the user's real data.
- `QSettings::setPath` redirects settings to a temporary directory.
- Each test run removes the test database files before starting.

## Known limitations

- `MetadataReader` does not extract embedded artwork yet; album cards use a
  generated placeholder color.
- Tag text from files whose ID3 tags genuinely declare a legacy encoding
  (e.g. Shift-JIS mislabeled as ISO-8859-1) is not normalized. UTF-8 and
  UTF-16 ID3v2 tags are read correctly; a real Shift-JIS tag re-decode fallback
  is not implemented yet.
- The playback queue is set from the visible list in the Songs, Playlists and
  album/artist drill-down views (double-click or "Play all" loads that list as
  the navigation context for `next()`/`previous()`).
- `tst_playback` covers the data flow up to `PlayerController::loadSong()`;
  actual audio playback (needs a media backend with decoders) is not exercised
  in CI.
- `LibraryController` does not load existing songs in its constructor; callers
  must invoke `refresh()` (done once in `src/main.cpp`; QML does not call it).
- Migrations: only schema version 1 exists; there is no migration runner yet
  (`DatabaseManager` creates the tables and records the version in the
  `schema_version` table).
- CI builds on Ubuntu, not CachyOS/Arch; the PKGBUILD is untested.

## Next steps

1. Artwork extraction and display (embedded tags → `artworkPath`).
2. Test the PKGBUILD on CachyOS.
