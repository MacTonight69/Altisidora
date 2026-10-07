# Project Status

Last updated: 2026-10-07

## Summary

MusicRep has a working core **and a functional GUI**: the full module
structure, build system, implementations, tests and a QML interface wired to
the controllers are in place. All tests pass.

- **Build:** working (CMake + Qt 6.11 + TagLib 2.3)
- **Tests:** 10/10 test suites passing (100%)
- **Status of functionality:** scan → browse → search → play works end to end
  with a real music folder.

## Modules

| Module | Path | Status | Notes |
|---|---|---|---|
| Models | `src/models/` | Implemented | Song, Artist, Album, Genre, Playlist (plain structs + QVariantMap conversion) |
| Database | `src/database/` | Implemented | `DatabaseManager`: SQLite, prepared statements, schema version table, CRUD. Songs are normalized (artist/album/genre rows resolved on insert); reads JOIN names back and artist/album/genre/playlist queries return `songCount` |
| Audio | `src/audio/` | Implemented | `PlayerController` wrapping QMediaPlayer/QAudioOutput; `load(url)` for raw URLs and `loadSong(map)` to keep library metadata for the UI |
| Library | `src/library/` | Implemented | `Scanner` (recursive, extension filter), `MetadataReader` (TagLib, falls back to file name for title), `LibraryController` (scan/search/refresh) |
| Application | `src/app/` | Implemented | `PlaylistController` (CRUD + song counts), `SettingsController` (QSettings persistence) |
| GUI | `resources/qml/` | Implemented | Sidebar navigation, Songs/Albums/Artists/Playlists/Settings views, search bar, player controls (seek/volume/errors), scan flow with overlay, add-to-playlist menu |
| Tests | `tests/` | In progress | Infrastructure complete; see below |
| Packaging | root | Initial | `PKGBUILD`, `.desktop`, GitHub Actions CI (Ubuntu) |

## Public API

The interfaces documented in `docs/API_CONTRACTS.md` are implemented by:

- `PlayerController` → playback control and state, current-song metadata
- `LibraryController` → scan/refresh/search and QML-friendly models
- `PlaylistController` → playlist operations
- `SettingsController` → persistent settings

QML accesses them as context properties (`Player`, `Library`, `Playlists`,
`AppSettings`) set in `src/main.cpp`.

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

- Scanning runs synchronously on the calling thread; the UI freezes while a
  large folder is scanned (the overlay is shown via a deferred timer, but the
  scan itself blocks). Background scanning (threading) is not implemented yet.
- `MetadataReader` does not extract embedded artwork yet; album cards use a
  generated placeholder color.
- Tag text from files whose ID3 tags genuinely declare a legacy encoding
  (e.g. Shift-JIS mislabeled as ISO-8859-1) is not normalized. UTF-8 and
  UTF-16 ID3v2 tags are read correctly; a real Shift-JIS tag re-decode fallback
  is not implemented yet.
- No playback queue: `next()`/`previous()` are placeholders; one song must be
  double-clicked at a time.
- `tst_playback` is a stub; a real playback workflow test requires an audio
  backend in CI.
- `LibraryController` does not load existing songs in its constructor; callers
  must invoke `refresh()` (done in `main.cpp` and `main.qml`).
- Migrations: only schema version 1 exists; `DatabaseManager::migrate()` is a
  placeholder.
- CI builds on Ubuntu, not CachyOS/Arch; the PKGBUILD is untested.

## Next steps

1. Background library scanning (threading) without blocking the UI.
2. Artwork extraction and display (embedded tags → `artwork_path`).
3. Playback queue and end-of-track handling in `PlayerController`.
4. Play an album/artist directly from the browse views.
5. Test the PKGBUILD on CachyOS.
