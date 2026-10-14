# Altisidora

A native Linux music player built with C++20 and Qt 6.

## Features

- Modern Qt Quick interface
- Music library management
- Playlist support
- Audio playback with libmpv
- SQLite database for persistence
- TagLib metadata extraction
- Native Linux integration (MPRIS v2, media keys)

## Building

### Dependencies

- CMake >= 3.20
- Qt 6 (Core, Gui, Quick, QuickControls2, Sql, Concurrent, DBus, Test)
- libmpv (pkg-config `mpv`)
- TagLib
- C++20 compiler (GCC 11+ or Clang 14+)

### Build instructions

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running tests

```bash
ctest --output-on-failure
```

## Project Structure

```
src/
  main.cpp              # Application entry point
  app/                  # Application controllers (playlist, settings)
  audio/                # Audio playback module (libmpv)
  library/              # Scanning, metadata and the song-list model
  database/             # Database persistence
  models/               # Domain models
  mpris/                # MPRIS v2 desktop integration
resources/
  qml/                  # QML interface files (the GUI lives here)
  qml.qrc               # Resource manifest: QML, bundled fonts and images
fonts/                  # Bundled theme typefaces
images/                 # Brand logos
tests/
  unit/                 # Unit tests
  integration/          # Integration tests
docs/                   # Documentation
```

## Documentation

| Doc | Contents |
|---|---|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Module layout and responsibilities |
| [`docs/API_CONTRACTS.md`](docs/API_CONTRACTS.md) | Controller interfaces the QML layer relies on |
| [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) | Current state, known limitations, next steps |
| [`docs/PACKAGING.md`](docs/PACKAGING.md) | Export targets: glibc floor, platform seam, licensing |
| [`docs/PACKAGING_LINUX.md`](docs/PACKAGING_LINUX.md) | Building the AppImage |
| [`docs/PACKAGING_WINDOWS.md`](docs/PACKAGING_WINDOWS.md) | The future Windows port |
| `TASKS.md` | Implementation plan and decisions log |

## Packaging

| Target | Artifact | Status |
|---|---|---|
| Arch / CachyOS | `PKGBUILD` | working |
| Any current Linux | AppImage | planned — see [`docs/PACKAGING_LINUX.md`](docs/PACKAGING_LINUX.md) |
| Windows 10/11 | ZIP / installer | future — see [`docs/PACKAGING_WINDOWS.md`](docs/PACKAGING_WINDOWS.md) |

## License

GPL-3.0 (declared in `PKGBUILD`).
