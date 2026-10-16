# Packaging and export targets

Altisidora ships to Linux and, later, to Windows. Each target has its own guide;
this page is the map, plus the rules that both targets share.

## Targets

| Target | Artifact | Platform | Status | Guide |
|--------|----------|----------|--------|-------|
| Arch package | `altisidora-<version>-1-x86_64.pkg.tar.zst` | Arch / CachyOS | working | `PKGBUILD` |
| AppImage | `Altisidora-0.1.0-x86_64.AppImage` (122,497,528 bytes, ~122 MB) | any Linux with glibc ≥ 2.35 | working | [PACKAGING_LINUX.md](PACKAGING_LINUX.md) |
| Windows | portable ZIP and/or installer | Windows 10/11 | **future** | [PACKAGING_WINDOWS.md](PACKAGING_WINDOWS.md) |

The Arch package is the native path and is already verified end to end
(`makepkg`, `pacman -Qp`, `desktop-file-validate`, packaged-binary smoke run).
The AppImage pipeline is also implemented: `bash packaging/appimage/make.sh`
builds `dist/Altisidora-0.1.0-x86_64.AppImage` (122,497,528 bytes, ~122 MB) in
an `ubuntu:22.04` container with a glibc 2.35 floor. The Windows guide describes
work that is **not implemented yet**; it records the intended pipeline and the
decisions it depends on.

## What the codebase already gives you

These existing properties are what make both exports tractable. Do not undo them.

| Property | Why it matters for packaging |
|---|---|
| QML, fonts and images are compiled into the binary (`qt_add_resources` + `resources/qml.qrc`) | Nothing external to ship; no missing `.qml` or font at the target |
| `install()` rules exist for the binary, the `.desktop` and the 512×512 icon | `DESTDIR=... cmake --install` already lays down most of the AppDir / install tree |
| All state lives under XDG paths (`QSettings`, `QStandardPaths::AppDataLocation`) | Nothing is written next to the executable, so the bundle stays read-only and portable |
| Playback is audio-only (`vo=null`, `vid=no`, `audio-display=no` in `PlayerController`) | The bundled FFmpeg can be built audio-only — the single biggest size lever |
| Only `QtQuick`, `QtQuick.Controls`, `QtQuick.Layouts`, `QtQuick.Dialogs` are imported | A short, known list of QML modules to bundle |
| No platform `#ifdef`s, no `QProcess` / `QDesktopServices` / `xdg-open`, no absolute paths | The port surface is one isolated module (see Rule 2) |

## Rule 1 — the glibc floor is set by the build host, not by the code

An AppImage does **not** bundle glibc; it uses the host's. A bundle built on a
host with glibc X runs only on systems with glibc ≥ X.

The floor is **not** set by the app binary alone: every bundled `.so` carries
its own requirement. Pulling a single library from a newer distribution (for
example the host's `libmpv`) silently raises the floor for the whole bundle and
it will fail to start on older systems.

Consequence: build the **entire** stack — Qt, mpv/FFmpeg, TagLib — on the oldest
host you intend to support, inside a container. Never build the AppImage on the
development machine if that machine is newer than the target.

The compatibility table and the verification command are in
[PACKAGING_LINUX.md](PACKAGING_LINUX.md).

## Rule 2 — all platform-specific code sits behind one seam

The only Linux-only surface is MPRIS over D-Bus: `src/mpris/MprisPlayer.{h,cpp}`
(about 410 lines) plus the `Qt6::DBus` component in `CMakeLists.txt`. Everything
else is portable Qt: no `#ifdef`s, no external processes, no hardcoded paths.

That is the entire Windows port surface. The Windows guide describes the seam to
introduce _now_, while the code is already being touched, so the future port
becomes an addition instead of surgery.

## Rule 3 — bundling third-party code has licence obligations

The bundle ships other people's binaries, so their licences travel with it. The
project is GPL-3.0, which is compatible with all of the following; the
obligation is to ship the licence texts and respect the linking terms.

| Component | Typical licence | Obligation when bundled |
|---|---|---|
| Qt 6 | LGPL-3.0 (or GPL) | Keep it **dynamically linked and replaceable**; ship the licence text. Do not static-link Qt into a proprietary binary. |
| mpv / FFmpeg | mpv: GPL-2.0+ or LGPL; FFmpeg: LGPL-2.1, **GPL if built with `--enable-gpl`** (x264/x265) | An audio-only FFmpeg build can usually avoid GPL components entirely; verify before publishing. |
| TagLib | LGPL-2.1 / MPL-1.1 | Ship the licence text. |
| SQLite | Public domain | None. |

Verify the actual licences of the exact binaries you bundle rather than trusting
this table — a distro's build flags decide them.

## Next step

The Linux compatibility floor is decided (glibc 2.35) and the AppImage pipeline
builds and runs; the remaining item for Linux is publishing a release — a
`v0.1.0` tag and real `sha256sums`. See
[PACKAGING_LINUX.md](PACKAGING_LINUX.md).
