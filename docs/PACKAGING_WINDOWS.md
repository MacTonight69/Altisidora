# Packaging Altisidora for Windows

Goal: a Windows build of Altisidora, shipped as a portable ZIP and/or an
installer.

Status: **future — not started.** This page records what the port requires, so
the design choices made for the Linux work do not block it later. Nothing here
has been built or verified on Windows yet; package names and toolchain details
still need to be confirmed.

## Why this is a port, not a rewrite

| Concern | Status on Windows |
|---|---|
| Qt 6 (Core, Gui, Quick, QuickControls2, Sql, Network) | Works; `windeployqt` collects it |
| QML + bundled fonts + images | Works unchanged — they are inside the binary via `qml.qrc` |
| libmpv playback | Works; mpv ships Windows builds (`libmpv-2.dll`) |
| TagLib metadata | Works; must match the compiler's C++ ABI (see toolchain) |
| SQLite persistence | Works |
| `QStandardPaths` (`MusicLocation`, `AppDataLocation`), `QSettings` | Work (registry for `QSettings`) |
| `AA_DontUseNativeDialogs` and the QML `FolderDialog` | Work; keeps the picker consistent with Linux |
| **MPRIS over D-Bus** | **Does not exist.** This is the whole port surface. |

There are no patchwork `#ifdef`s, no external processes, no `xdg-open` and no
hardcoded Unix paths anywhere in the tree. The only Linux-only code is
`src/mpris/` (about 410 lines) and the `Qt6::DBus` dependency.

## 1. Introduce the seam (do this on Linux, now)

Doing this while the code is already being touched costs an hour and changes no
behaviour. `MprisPlayer` is already almost the right abstraction: it holds the
`PlayerController` and exposes platform-neutral state (`playbackStatus()`,
`metadata()`, `positionMicros()`), while everything D-Bus-specific — the two
`QDBusAbstractAdaptor` subclasses, `registerService()` — can move behind a base
class.

Proposed shape:

```cpp
// src/media/MediaIntegration.h  (platform-neutral)
class MediaIntegration : public QObject
{
    Q_OBJECT
public:
    explicit MediaIntegration(PlayerController *player, QObject *parent = nullptr);
    virtual bool start() = 0;   // register with the platform; false if unavailable
    virtual void stop() = 0;

    // Shared state, currently read by the D-Bus adaptors.
    QString playbackStatus() const;
    QVariantMap metadata() const;   // already produces xesam:*/mpris:* keys
    qint64 positionMicros() const;

protected:
    PlayerController *m_player;
};
```

- `src/media/MprisMediaIntegration.{h,cpp}` — the current code, with the adaptors
  moved in and `start()` doing the D-Bus registration.
- `src/media/NullMediaIntegration.{h,cpp}` (or a factory fallback) — a no-op for
  builds without a platform backend, so Windows ships on day one with no media
  keys, then gains them.

`QtQuick` and QML are untouched by this. `main.cpp` changes only in how it
constructs the object:

```cpp
auto *media = createMediaIntegration(&player);   // factory, platform-selected
...
QTimer::singleShot(0, media, [media]() { media->start(); });
```

## 2. Make DBus optional in CMake

Today `Qt6::DBus` is a hard `REQUIRED` component and `MprisPlayer.cpp` is an
unconditional entry in `CORE_SOURCES`. On Windows that must become conditional,
in `CMakeLists.txt`:

```cmake
set(ALTISIDORA_HAS_DBUS OFF)
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    set(ALTISIDORA_HAS_DBUS ON)
endif()
```

then add `src/media/MprisMediaIntegration.cpp` to `CORE_SOURCES` and
`Qt6::DBus` to `target_link_libraries` **only** when `ALTISIDORA_HAS_DBUS` is on,
and expose it to the code with
`target_compile_definitions(altisidora_core PUBLIC ALTISIDORA_HAS_DBUS)` (or a
generated config header).

`tests/CMakeLists.txt` needs the same guard: `tst_mpris` registers a real D-Bus
service over a private session bus, so it must not be added when DBus is off.

## 3. Toolchain

Qt, TagLib and mpv must share a C++ ABI, because TagLib is a C++ library (unlike
mpv's and SQLite's C APIs). Two coherent combinations — pick one and do not mix:

| Option | How | Trade-off |
|---|---|---|
| **MSYS2 / MinGW-w64** | `mingw-w64-x86_64-{qt6,taglib,mpv}` from one repo | Lowest friction: one ABI, one package source, matches how mpv ships. Depends on the MinGW runtime DLLs, which you bundle. |
| MSVC + vcpkg | Qt from the official installer or vcpkg, TagLib from vcpkg, mpv separately | The "native" MSVC path; more moving parts to keep ABI-consistent. |

**Verify before committing to either:** the availability and version of the mpv
dev package, and that its FFmpeg build is self-contained (mpv's Windows builds
link FFmpeg statically, so on Windows the whole ~104 MB Linux stack collapses
into one `libmpv-2.dll`).

## 4. What to ship

| Item | Source |
|---|---|
| `altisidora.exe` | this project |
| Qt 6 DLLs, `platforms/qwindows.dll`, `sqldrivers/`, `imageformats/`, QML modules | `windeployqt` |
| `libmpv-2.dll` | mpv Windows build |
| `tag.dll` (TagLib) | the chosen toolchain |
| SQLite DLL, if the Qt SQL plugin links it externally rather than bundling it | verify with `windeployqt --list` / a dependency walker |
| MinGW runtime DLLs (`libgcc`, `libstdc++`, `libwinpthread`), if MinGW | the toolchain |
| App icon (`.ico`) + application manifest | new resource, see below |
| Licence texts | see [PACKAGING.md](PACKAGING.md) Rule 3 |

New resources that do not exist today: a `.ico` and a Windows resource/manifest
(the `.desktop` file and PNG icon in `CMakeLists.txt` are Linux-only). The
existing `install()` rules and the `.desktop` target should be guarded so a
Windows configure does not try to install them.

## 5. Packaging the output

- **Portable ZIP** — the directory with the `.exe` and all DLLs. Simplest, and
  the natural first target: it proves the bundle before you add an installer.
- **Installer (NSIS or Inno Setup)** — adds Start Menu / desktop shortcuts and
  an uninstaller. Standard, low risk.
- **MSIX** — the modern Store-style package. More work (needs a package
  identity and signing); only worth it if the Microsoft Store is a goal.

## 6. Media keys and the shell (optional, later)

Windows has no MPRIS. The equivalent is **SMTC** (System Media Transport
Controls), which is what drives the media overlay and the keyboard media keys.
Two levels of effort:

- **Cheap:** register global media-key hotkeys (`RegisterHotKey`) so at least the
  keyboard works.
- **Full:** implement `WindowsMediaIntegration` on top of the SMTC API (C++/WinRT)
  to also populate the shell's media overlay with title/artist/artwork.

Until then, `NullMediaIntegration` keeps the app fully usable — playback, library
and the in-app controls do not depend on the seam.

## Verification checklist

- [x] Linux build still produces MPRIS with the seam in place (`tst_mpris` green).
- [x] `cmake` configures with DBus absent and `tst_mpris` is not added.
- [ ] Windows build launches, plays audio, scans a library, edits tags.
- [ ] QML loads (no blank window) — the `qml.qrc` path is compiler-independent,
      but plugin/QML discovery still needs `windeployqt` to run.
- [ ] Folder picker opens (the QML `FolderDialog`, not a native dialog).
- [ ] A fresh profile writes nothing outside `%USERPROFILE%` and the registry.
- [ ] Licence texts are in the ZIP/installer.
- [ ] Icon and manifest appear on the `.exe`.

## Open decisions

| Decision | Why it matters |
|---|---|
| MinGW-w64 vs MSVC | Decides the whole toolchain and how TagLib is obtained |
| ZIP first vs installer first | ZIP proves the bundle with less machinery |
| SMTC now, hotkeys now, or nothing yet | Only affects the media-key feature, not the port |
| The AppStream component id | Shared with the Linux bundle; decide once (see the Linux guide) |
