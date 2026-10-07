# MusicRep

A native Linux music player built with C++20 and Qt 6.

## Features

- Modern Qt Quick interface
- Music library management
- Playlist support
- Audio playback with Qt Multimedia
- SQLite database for persistence
- TagLib metadata extraction
- Native Linux integration

## Building

### Dependencies

- CMake >= 3.20
- Qt 6 (Core, Gui, Quick, QuickControls2, Multimedia, Sql, Test)
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
  app/                  # Application controllers
  audio/                # Audio playback module
  library/              # Library scanning and metadata
  database/             # Database persistence
  models/               # Domain models
  gui/                  # GUI components
resources/
  qml/                  # QML interface files
  icons/                # Application icons
tests/
  unit/                 # Unit tests
  integration/          # Integration tests
docs/                   # Documentation
```

## License

TODO: Add license
