# Packaging Altisidora as an AppImage

Goal: one self-contained `Altisidora-<version>-x86_64.AppImage` that runs on
current Linux distributions without the user installing Qt, mpv or TagLib.

Status: **planned**. The pipeline below does not exist yet. The measurements and
the dependency inventory were taken on the development machine
(CachyOS, glibc 2.44, Qt 6.12.0); the steps are the intended recipe and each one
that still needs proving is called out.

## Quick path

1. Decide the **glibc floor** (next section). This drives everything else.
2. Build Qt, mpv/FFmpeg and TagLib **inside** a container with that floor.
3. Assemble an `AppDir` from `cmake --install` output.
4. Run `linuxdeploy` with the Qt plugin to collect Qt libs, plugins and QML.
5. Verify with the two checks in [Verification](#verification), then publish.

## 1. Compatibility floor

An AppImage takes glibc from the host, so a bundle built on glibc X runs only on
glibc ≥ X. Current distributions:

| Distribution | glibc |
|---|---|
| RHEL / Rocky / Alma 8 | 2.28 |
| Debian 11 · Ubuntu 20.04 · openSUSE Leap 15.5 | 2.31 |
| RHEL / Rocky / Alma 9 | 2.34 |
| Ubuntu 22.04 | 2.35 |
| Debian 12 | 2.36 |
| Ubuntu 24.04 · Fedora 40 | 2.39 |
| CachyOS / Arch (this dev machine) | **2.44** |

**Target: glibc ≤ 2.31** to cover everything above. Building on this CachyOS
machine (2.44) is not an option — it would produce a bundle that starts on
almost nothing.

Two things make this more than a `FROM` line:

- **The floor is set by every bundled `.so`, not just the app.** The host's
  `libmpv.so.2`, `libQt6*.so.6` and `libtag.so.2` are all built against
  glibc 2.44 here. Bundling any of them leaks 2.44 symbols in. The whole stack
  must be rebuilt in the old container.
- **The project needs C++20**, so an old distro needs a modern compiler
  (`gcc-toolset` on RHEL-family, or backports). Pick the base image and the
  compiler together.

Suggested staging, so the pipeline is proven before the floor is lowered:
**Debian 12 (glibc 2.36) first** to get a working build, then move the base image
down to Debian 11 / Ubuntu 20.04 / Rocky 8 and re-verify. Do not skip the second
step — an AppImage that only runs on new systems misses the point.

### Open decision

Which Qt do you bundle? Debian 12 ships Qt 6.4, Ubuntu 20.04 ships none. Either
use the distro's Qt (older than the 6.12 used for development — check nothing
regresses) or bring a Qt build with a known-low floor and verify it. **This is
the one item that needs a spike before the rest of the pipeline is worth
writing.**

## 2. What must be bundled

| Group | What | Notes |
|---|---|---|
| App | `altisidora` | plus its `.desktop` and icon |
| Qt libraries | the `libQt6*.so.6` the binary links (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `QuickTemplates2`, `QmlMeta`, `QmlModels`, `QmlWorkerScript`, `Sql`, `Concurrent`, `DBus`, `Network`, `OpenGL`) | `linuxdeploy-plugin-qt` collects these |
| Qt platform plugins | `platforms/libqxcb.so`, `libqwayland*.so`, `wayland-*` integration plugins | both X11 and Wayland, so the same file works everywhere |
| Qt SQL driver | `sqldrivers/libqsqlite.so` **and `libsqlite3.so.0`** | the driver links the system SQLite; do not forget the second file |
| Qt image formats | `imageformats/` (png at minimum; jpeg/webp if covers need them) | |
| Qt misc | `iconengines`, `platformthemes`, `xcbglintegrations`, `egldeviceintegrations`, `networkinformation`, `tls` | |
| QML modules | `QtQuick`, `QtQuick.Controls` (**plus the style actually used**), `QtQuick.Templates`, `QtQuick.Layouts`, `QtQuick.Dialogs`, `QtQuick.Window`, `QtQml`, `QtQml.Models`, `QtQml.WorkerScript`, `QtCore` | derived from the four `import` statements in `resources/qml/` |
| mpv / FFmpeg | `libmpv.so.2` and its FFmpeg stack (**audio-only build — see the size lever**) | the big one |
| TagLib | `libtag.so.2` | |

> **Because the QML lives in `resources/qml.qrc`**, it is compiled into the
> binary and is invisible to the Qt plugin's runtime scan. You must point the
> plugin at the sources explicitly:
> `QML_SOURCES_PATHS=$PWD/resources/qml`. Missing this is the classic cause of
> an AppImage that starts and then shows a blank window.

## 3. What must NOT be bundled

These must come from the host, because they are tied to its kernel, drivers and
C library:

- **glibc and friends** — `libc`, `libm`, `libpthread`, `libdl`, `librt`, the
  loader itself.
- **Graphics driver stack** — `libGL`, `libEGL`, `libGLX`, `libOpenGL`,
  `libGLdispatch`, `libdrm`, `libgbm`, `libvulkan`, and the VA-API / VDPAU
  drivers.
- **C++ runtime** — `libstdc++`, `libgcc_s` (bundling these is a known source
  of breakage; let the toolchain's excludelist rule).

`linuxdeploy` applies the AppImage excludelist automatically. Do not trust it
blindly — confirm with the `ldd` check in [Verification](#verification).

## 4. Size budget and the one big lever

Measured on the development machine, for the built binary:

| Item | Size |
|---|---|
| Libraries in the transitive tree | **230** |
| Total on disk | **~264 MB** |
| of which the mpv / FFmpeg stack | **~104 MB** |
| of which Qt 6 | **~38 MB** |

The app is audio-only (`vo=null`, `vid=no`), yet the distro's FFmpeg still links
every video codec: x264, x265, libjxl, dav1d, libplacebo, shaderc, glslang,
vulkan. All of it is dead weight.

Building mpv + FFmpeg audio-only —
`--disable-everything --enable-demuxer=mp3,flac,ogg,matroska,wav,mov
--enable-decoder=mp3,flac,vorbis,opus,aac,pcm_*` — takes that stack from
~104 MB to roughly **20–30 MB**. Expected result: an AppDir around 80–120 MB and
a compressed AppImage around **50–70 MB** instead of ~120 MB.

Cost: you own that build and must extend it when a new format is wanted.
`libmpv` cannot simply be dropped — the 10-band EQ is built on mpv's audio
filters.

## 5. AppDir layout

```
Altisidora.AppDir/
  AppRun                                  # generated by linuxdeploy
  altisidora.desktop                      # a copy, for the AppImage spec
  altisidora.png                          # a copy, for the AppImage spec
  usr/
    bin/altisidora
    lib/                                  # Qt, mpv/FFmpeg, TagLib, libsqlite3
    plugins/{platforms,sqldrivers,imageformats,iconengines,platformthemes,...}
    qml/                                  # QtQuick, QtQuick.Controls, ...
    share/
      applications/altisidora.desktop
      icons/hicolor/512x512/apps/altisidora.png
      metainfo/<component-id>.appdata.xml # AppStream, see below
```

Steps 1–3 of that tree already come from the existing `install()` rules in
`CMakeLists.txt`:

```bash
DESTDIR=$PWD/Altisidora.AppDir cmake --install build --prefix /usr
```

Then hand it to `linuxdeploy`:

```bash
QML_SOURCES_PATHS=$PWD/resources/qml \
linuxdeploy --appdir Altisidora.AppDir --plugin qt \
  --executable Altisidora.AppDir/usr/bin/altisidora \
  --desktop-file Altisidora.AppDir/usr/share/applications/altisidora.desktop \
  --icon-file Altisidora.AppDir/usr/share/icons/hicolor/512x512/apps/altisidora.png \
  --output appimage
```

### AppStream metadata

`metadata/org.altisidora.Altisidora.appdata.xml` (or whatever component id is
chosen) is recommended for desktop integration and is required by some
app stores. **Open decision:** the component id. It should be reverse-DNS and
kept consistent with the `.desktop` basename; today the app has no reverse-DNS
id (MPRIS uses `org.mpris.MediaPlayer2.altisidora`, `QSettings` org is
`Altisidora`). Decide once and reuse it for Windows.

## Verification

Two checks decide whether the bundle is usable. Run both in CI, every build.

**1. The floor is what you think it is.** The highest glibc symbol any bundled
library demands:

```bash
find Altisidora.AppDir -name '*.so*' -exec sh -c \
  'objdump -T "$1" | grep -o "GLIBC_[0-9.]*"' _ {} \; \
  | sort -uV | tail -5
```

**2. It runs on a distribution you did not build on.** The only test that proves
portability:

```bash
docker run --rm -v "$PWD:/w" -w /w debian:12 \
  ./Altisidora-0.1.0-x86_64.AppImage --appimage-extract-and-run
```

`--appimage-extract-and-run` avoids needing FUSE inside the container (FUSE is
also not installed on this dev machine). A real desktop run additionally checks
audio and MPRIS, which the container cannot.

## Checklist

- [ ] Base image chosen and its `glibc` recorded in this doc.
- [ ] Qt, mpv/FFmpeg and TagLib built **in that image** — nothing taken from the host.
- [ ] mpv/FFmpeg built audio-only (or the size is accepted and recorded here).
- [ ] `QML_SOURCES_PATHS` points at `resources/qml`.
- [ ] `libsqlite3.so.0` is present next to `libqsqlite.so`.
- [ ] Excludelist respected: `ldd` on the extracted AppDir shows no `libGL`/`libEGL`/`libdrm`/`libvulkan`/`libc` from inside the bundle.
- [ ] Glibc scan shows the intended floor.
- [ ] Runs in a `debian:12` (or older) container.
- [ ] Licence texts shipped (see [PACKAGING.md](PACKAGING.md) Rule 3).
- [ ] CI builds it; the AppImage is never produced on a dev machine.
- [ ] `altisidora.desktop` validates (`desktop-file-validate`).

## Open decisions

| Decision | Why it matters |
|---|---|
| glibc floor (target ≤ 2.31) | Drives the base image and the whole FFmpeg build |
| Which Qt to bundle, and its floor | Needs a spike; the distro Qt is older than 6.12 |
| FFmpeg audio-only vs full | ~50 MB of the final artifact |
| AppStream component id | Required for metadata; must be reused on Windows |
| CI provider | None exists in the repo today |
