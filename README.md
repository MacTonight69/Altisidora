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
- Optional: a Japanese/CJK font such as `adobe-source-han-sans-jp-fonts`,
  `noto-fonts-cjk` (all CJK scripts) or `otf-ipafont` (smallest). The player
  reads CJK tags correctly without one, but the system needs a font that
  contains the glyphs to display them (otherwise they appear as empty
  boxes). Qt falls back to it automatically.

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
  app/                  # Application controllers
  audio/                # Audio playback module (libmpv)
  library/              # Library scanning and metadata
  database/             # Database persistence
  models/               # Domain models
  mpris/                # MPRIS v2 desktop integration
resources/
  qml/                  # QML interface files (the GUI lives here)
  icons/                # Application icons
tests/
  unit/                 # Unit tests
  integration/          # Integration tests
docs/                   # Documentation
```

## License

GPL-3.0 (declared in `PKGBUILD`).
