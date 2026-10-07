# Project Status

Last updated: 2026-10-07

## Summary

MusicRep is in its **initial scaffolding stage**: the full module structure,
build system, core implementations and test infrastructure are in place and
working. All tests pass.

- **Build:** working (CMake + Qt 6.11 + TagLib 2.3)
- **Tests:** 10/10 test suites passing (100%)
- **Status of functionality:** core modules implemented with basic
  functionality; UI is a placeholder layout.

## Modules

| Module | Path | Status | Notes |
|---|---|---|---|
| Models | `src/models/` | Implemented | Song, Artist, Album, Genre, Playlist (plain structs + QVariantMap conversion) |
| Database | `src/database/` | Implemented | `DatabaseManager`: SQLite, prepared statements, schema version table, CRUD for all entities |
| Audio | `src/audio/` | Implemented | `PlayerController` wrapping QMediaPlayer/QAudioOutput; states: Stopped/Loading/Playing/Paused/Error |
| Library | `src/library/` | Implemented | `Scanner` (recursive, extension filter), `MetadataReader` (TagLib), `LibraryController` (scan/search/refresh) |
| Application | `src/app/` | Implemented | `PlaylistController` (CRUD), `SettingsController` (QSettings persistence) |
| GUI | `resources/qml/` | Placeholder | Basic layout (sidebar, content area, player controls); no real views yet |
| Tests | `tests/` | In progress | Infrastructure complete; see below |
| Packaging | root | Initial | `PKGBUILD`, `.desktop`, GitHub Actions CI (Ubuntu) |

## Public API

The interfaces documented in `docs/API_CONTRACTS.md` are implemented by:

- `PlayerController` → playback control and state
- `LibraryController` → scan/refresh/search and QML-friendly models
- `PlaylistController` → playlist operations
- `SettingsController` → persistent settings

## Tests

Run with `ctest --output-on-failure` from the build directory.

| Suite | Type | Status |
|---|---|---|
| tst_database | unit (MockDatabase) | Real tests |
| tst_models | unit | Real tests |
| tst_player | unit (MockPlayer) | Real tests |
| tst_scanner | unit | Real tests |
| tst_metadata | unit | Real tests |
| tst_library | unit (real LibraryController) | Real tests |
| tst_playlist | unit (real PlaylistController) | Real tests |
| tst_settings | unit (real SettingsController) | Real tests |
| tst_library_scan | integration (real scan + persistence across restart) | Real tests |
| tst_playback | integration | Stub (needs a real playback workflow) |

Test isolation:

- `QStandardPaths::setTestModeEnabled(true)` redirects the database away from
  the user's real data.
- `QSettings::setPath` redirects settings to a temporary directory.
- Each test run removes the test database files before starting.

## Known limitations

- The GUI is a placeholder; QML types are registered but there are no real
  views (songs/albums/artists/playlists browsing).
- `LibraryController` does not load existing songs in its constructor; callers
  must invoke `refresh()` after startup.
- Scanning runs synchronously on the calling thread; background scanning
  (threading) is not implemented yet.
- `MetadataReader` does not extract embedded artwork yet.
- `tst_playback` is a stub; a real playback workflow test requires an audio
  backend in CI.
- Migrations: only schema version 1 exists; `DatabaseManager::migrate()` is a
  placeholder.
- CI builds on Ubuntu, not CachyOS/Arch; the PKGBUILD is untested.

## Next steps

1. Real GUI views consuming the controllers (songs list, albums, artists,
   playlists, working player controls).
2. Background library scanning (threading) without blocking the UI.
3. Artwork extraction and display.
4. Playback queue and end-of-track handling in `PlayerController`.
5. Settings UI (music directory selection).
6. Test the PKGBUILD on CachyOS.
