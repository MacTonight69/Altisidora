# Packaging Altisidora as an AppImage

Goal: one self-contained `Altisidora-<version>-x86_64.AppImage` that runs on
current Linux distributions without the user installing Qt, mpv or TagLib.

Status: **working**. The container pipeline builds, and the artifact starts on
distributions both older and newer than the build host (evidence in
[Verification](#verification)). The measured artifact is
`dist/Altisidora-0.1.0-x86_64.AppImage`, 122,497,528 bytes (~117 MiB,
~122 MB); it is a build output, gitignored and deliberately not committed. What
still needs the user is **publishing**: a `v0.1.0` tag and real `sha256sums`.

## Quick path

1. Run the host entry point: `bash packaging/appimage/make.sh`. It builds the
   image and runs the recipe with the repository bind-mounted. Docker, no sudo.
2. The recipe configures, builds, installs into an AppDir, deploys with
   `linuxdeploy`, and packs with `appimagetool`.
3. The artifact lands in `dist/`.
4. Verify with [Verification](#verification) — install the desktop runtime, run
   under `timeout`, expect exit 124.

Files:

| File | Role |
|---|---|
| `packaging/appimage/Dockerfile` | Build environment: `ubuntu:22.04` (glibc 2.35), `g++-12`, Qt 6.8.3 (official binaries via `aqtinstall`), TagLib 2.0.2 (source), `libmpv-dev` 0.34.1, pre-extracted `linuxdeploy` + `linuxdeploy-plugin-qt` + `appimagetool`; CMake from the distro (3.22) |
| `packaging/appimage/build-in-container.sh` | The recipe that runs **inside** the container |
| `packaging/appimage/make.sh` | Host entry point: `docker build` then `docker run` |
| `metadata/org.altisidora.Altisidora.appdata.xml` | Minimal AppStream metainfo |
| `.gitignore` | Three lines keeping `dist/` out of the repo |

There is no `packaging/appimage/README.md`; the recipe's header comments are the
reference.

## 1. Compatibility floor

An AppImage takes glibc from the host, so a bundle built on glibc X runs only on
glibc ≥ X. The floor is set by every bundled `.so`, not just the app.

**Shipped floor: glibc 2.35** (the `ubuntu:22.04` build base). The bundle's
highest requirement is exactly **GLIBC_2.35**, and it comes from the bundled
FFmpeg / Cairo / slang libraries. The application binary's own requirement tops
out at **GLIBC_2.34**.

| Distribution | glibc |
|---|---|
| RHEL / Rocky / Alma 8 | 2.28 |
| Debian 11 · Ubuntu 20.04 · openSUSE Leap 15.5 | 2.31 |
| RHEL / Rocky / Alma 9 | 2.34 |
| Ubuntu 22.04 (**build base**) | 2.35 |
| Debian 12 | 2.36 |
| Ubuntu 24.04 · Fedora 40 | 2.39 |
| CachyOS / Arch (dev machine) | **2.44** |

Building on the dev machine (2.44) is not an option — it would produce a bundle
that starts on almost nothing. The whole stack is built inside the container.

### What the floor covers, and what it does not

In scope: Ubuntu 22.04 and newer, Debian 12 and newer, Fedora 36 and newer,
Linux Mint 21 and newer, openSUSE Leap 15.6, RHEL / Rocky / Alma 10 and newer,
Arch and its derivatives.

Out of scope at this floor: RHEL / Rocky / Alma **9** (glibc 2.34) — the
strongest argument for a lower floor, because it is still supported upstream —
together with Ubuntu 20.04 and Debian 11, both end-of-life. Covering RHEL 9
means rebuilding on a glibc 2.34 base with its own C++20 compiler and mpv
development packages. That is a future option, not a defect in the current
bundle.

### Why 2.35 and not 2.31

2.31 would only add Ubuntu 20.04 (EOL May 2025) and Debian 11 (EOL Aug 2026) —
both end-of-life — at the cost of a self-built toolchain and a fragile Qt
install. 2.35 is therefore the shipped floor; **2.31 stays a documented future
option only**.

Its consequence is explicit: at 2.31 the **C++ runtime would have to be bundled
again**, because a supported host could then ship a `libstdc++` older than the
build one. That reintroduces the newer-host failure described in
[§3](#3-what-the-host-must-provide), which is exactly why it is not done now.

## 2. What the bundle ships

Measured on the extracted bundle:

| Item | Measured |
|---|---|
| Total artifact | **122,497,528 bytes** (~117 MiB, ~122 MB) |
| Libraries in `usr/lib` | **250** |
| glibc floor | **GLIBC_2.35** |
| Highest C++ runtime requirement | **GLIBCXX_3.4.29** (GCC 11) |
| Platform plugins | `libqxcb.so`, `libqwayland-egl.so`, `libqwayland-generic.so`, `libqoffscreen.so` |
| SQL driver | `libqsqlite.so` only |

The application is compiled with `g++-12`, but uses no C++ library symbol newer
than GCC 11's runtime (`GLIBCXX_3.4.29`) anywhere in the bundle.

`linuxdeploy-plugin-qt` deploys `libqxcb.so` and nothing else by default. The
recipe forces `libqwayland-egl.so`, `libqwayland-generic.so` and
`libqoffscreen.so` through `EXTRA_PLATFORM_PLUGINS`: Wayland so the bundle works
on a Wayland session without XWayland (the default on Fedora 40+ and Ubuntu
24.04+), offscreen so the headless smoke run is possible.

`libqsqlite.so` is the only SQL driver shipped. Qt's official `libqsqlite.so`
**statically embeds SQLite**, so no separate `libsqlite3.so.0` is needed or
expected beside it.

> **Because the QML lives in `resources/qml.qrc`** it is compiled into the
> binary and is invisible to the Qt plugin's runtime scan. The recipe points the
> plugin at the sources explicitly: `QML_SOURCES_PATHS=$PWD/resources/qml`.
> Missing this is the classic cause of an AppImage that starts and then shows a
> blank window.

## 3. What the host must provide

The applied `linuxdeploy` excludelist (**50 entries**, read out of the tool's
own binary) leaves the **driver stack and the desktop stack** to the host:

- C library — `libc.so.6`, `libm.so.6`
- C++ runtime — `libstdc++.so.6`, `libgcc_s.so.1`
- GL / EGL / X11 / xcb — `libGL.so.1`, `libEGL.so.1`, `libGLX.so.0`,
  `libOpenGL.so.0`, `libGLdispatch.so.0`, `libglapi.so.0`, `libdrm.so.2`,
  `libgbm.so.1`, `libX11.so.6`, `libX11-xcb.so.1`, `libxcb.so.1`
- Text and fonts — `libfontconfig.so.1`, `libfreetype.so.6`,
  `libharfbuzz.so.0`, `libfribidi.so.0`
- Audio — `libasound.so.2`, `libjack.so.0`, `libpipewire-0.3.so.0`,
  `libusb-1.0.so.0`
- Wayland client — `libwayland-client.so.0`

**Convention: driver stack and desktop stack from the host; loaders bundled.**
The excludelist does **not** contain `libvulkan.so.1` and it does not contain
the VA-API loaders (`libva*.so*`), and the bundle ships them on purpose. The
distinction that matters is **loader vs driver**:

- `libvulkan.so.1` is the Vulkan **ICD loader**; the drivers themselves live on
  the host in `/usr/share/vulkan/icd.d`.
- `libva*.so` are the VA-API **loaders**.

Both arrive transitively through `libmpv.so.2` → libplacebo / VA-API. The
application is audio-only (mpv is configured with `vo=null` and `vid=no`), so
they are never initialised. Excluding them would make `libmpv.so.2` fail to load
on any host without `libvulkan1`, and the application would not start at all.
Shipping the loaders is what guarantees the application's own libraries always
load.

### The C++ runtime must NOT be bundled

An earlier plan copied `libstdc++.so.6` and `libgcc_s.so.1` into `usr/lib`
manually. That is wrong: the AppDir precedes the host on `LD_LIBRARY_PATH`, so a
bundled `libstdc++` **shadows the host's**, and a C++ library the host supplies
afterwards then fails to resolve against the older bundled symbols.

Measured on **Ubuntu 24.04 (GCC 13)**: the host's `libjack.so.0` requires
`GLIBCXX_3.4.32`, while the build container's libstdc++ (GCC 12) stops at
`GLIBCXX_3.4.30`. The result is
`version GLIBCXX_3.4.32 not found (required by /lib/x86_64-linux-gnu/libjack.so.0)`
and the application does not start at all. Removing both files fixed it.

Bundling would only be needed if a supported host's C++ runtime could be
**older than the build one** — a direct consequence of the glibc floor. At 2.35
every supported distribution ships GCC ≥ 12 (`GLIBCXX_3.4.30`), and the bundle
only needs `GLIBCXX_3.4.29`, so the host runtime always suffices.

## 4. Size budget and the one big lever

| Item | Size |
|---|---|
| Artifact | **122,497,528 bytes** (~117 MiB, ~122 MB) |
| Libraries in `usr/lib` | **250** |

The app is audio-only (`vo=null`, `vid=no`), yet the distro's FFmpeg still links
every video codec: x264, x265, libjxl, dav1d, libplacebo, shaderc, glslang,
vulkan. All of it is dead weight.

Building mpv + FFmpeg audio-only —
`--disable-everything --enable-demuxer=mp3,flac,ogg,matroska,wav,mov
--enable-decoder=mp3,flac,vorbis,opus,aac,pcm_*` — remains the big size lever,
expected to remove roughly **50 MB** from the artifact. It is deferred: the
Docker VM has only ~1.8 GB of RAM, which makes a full FFmpeg + mpv build
unreliable. `libmpv` cannot simply be dropped — the 10-band EQ is built on mpv's
audio filters.

## 5. AppDir layout

```
Altisidora.AppDir/
  AppRun                                  # generated by linuxdeploy
  altisidora.desktop                      # copied to the AppDir root
  altisidora.png                          # copied to the AppDir root
  usr/
    bin/altisidora
    lib/                                  # Qt, mpv/FFmpeg, TagLib (250 libraries)
    plugins/{platforms,sqldrivers,imageformats,iconengines,platformthemes,...}
    qml/                                  # QtQuick, QtQuick.Controls, ...
    share/
      applications/altisidora.desktop
      icons/hicolor/512x512/apps/altisidora.png
      metainfo/org.altisidora.Altisidora.appdata.xml
```

The recipe builds this from `cmake --install` output, copies the `.desktop` and
the icon to the AppDir root, installs the metainfo, then hands the tree to
`linuxdeploy` (`--plugin qt`, `--executable`, `--desktop-file`, `--icon-file`)
and finally packs it with `appimagetool`.

### AppStream metadata

`metadata/org.altisidora.Altisidora.appdata.xml` carries the id
`org.altisidora.Altisidora` and is installed to
`usr/share/metainfo/org.altisidora.Altisidora.appdata.xml`. The id is a
placeholder reverse-DNS value and must be reused for the Windows target if the
project is ever published. The licence string should be checked against
`LICENSE`/`PKGBUILD` before publishing.

## Verification

Two checks decide whether the bundle is usable. Run both after every build.

**1. The floor is what you think it is.** The highest glibc symbol any bundled
library demands:

```bash
./Altisidora-0.1.0-x86_64.AppImage --appimage-extract >/dev/null
find squashfs-root -name '*.so*' -exec sh -c \
  'objdump -T "$1" | grep -o "GLIBC_[0-9.]*"' _ {} \; \
  | sort -uV | tail -5
```

Expected: the top line is `GLIBC_2.35`.

**2. It runs on a distribution you did not build on.** This is the only test
that proves portability. Install the desktop runtime **first** — the excludelist
leaves the whole desktop stack to the host, so a bare container has none of it:

```bash
# Desktop runtime (Debian/Ubuntu package names).
apt-get update && apt-get install -y \
  libgl1 libglx0 libglvnd0 libegl1 libopengl0 libdrm2 libgbm1 \
  libx11-6 libx11-xcb1 libxcb1 libxcb-cursor0 libfontconfig1 libfreetype6 \
  libharfbuzz0b libfribidi0 libusb-1.0-0 libwayland-client0 libxkbcommon0 \
  libjack-jackd2-0 libasound2 libpipewire-0.3-0

# The exact shape used for both positive runs.
docker run --rm -v "$PWD/dist":/dist:ro <image> bash -c \
  'export HOME=/tmp QT_QPA_PLATFORM=offscreen; timeout 20 /dist/Altisidora-0.1.0-x86_64.AppImage --appimage-extract-and-run'
```

On Ubuntu 24.04 the audio packages are named `libasound2t64` and
`libpipewire-0.3-0t64`.

**Expected result: exit 124** — `timeout` killed an application that was still
running, i.e. it started successfully. The output carries a benign UTF-8 locale
notice and `MPRIS: no session bus available`, which is the integration's
designed fallback.

Measured runs:

| Image | Result |
|---|---|
| `debian:12` | exit **124** |
| `ubuntu:24.04` (newer than the build container: GCC 13, glibc 2.39) | exit **124** |

`ubuntu:24.04` is the run that exposed the bundled-`libstdc++` bug before the
fix. On bare `debian:12` and bare `debian:11` the run instead fails with
`libGLX.so.0: cannot open shared object file` — the expected consequence of
leaving the desktop stack to the host, not a defect.

A real desktop run additionally checks audio and MPRIS, which the container
cannot.

## Caveats

- `aqtinstall` is unpinned and the three packaging tools come from their
  `continuous` release, so the pipeline is reproducible in its **steps**, not
  bit-for-bit.
- `add_subdirectory(tests)` in `CMakeLists.txt` is unconditional, so the test
  binaries are compiled during packaging as well.
- The recipe needs a `LIBRARY_PATH`/`LD_LIBRARY_PATH` workaround around the bare
  `${TAGLIB_LIBRARIES}` reference in `CMakeLists.txt`. A clean fix would be an
  application source change, out of scope here.
- The AppStream licence string should be checked against `LICENSE`/`PKGBUILD`
  before publishing.
- An audio-only FFmpeg/mpv build remains the big size lever (~50 MB) and is
  deferred, because the Docker VM has only ~1.8 GB of RAM.

## Checklist

- [x] Base image chosen and its `glibc` recorded (2.35, `ubuntu:22.04`).
- [x] Qt, mpv/FFmpeg and TagLib built **in that image** — nothing from the dev host.
- [x] `QML_SOURCES_PATHS` points at `resources/qml`.
- [x] Excludelist respected: `usr/lib` holds no `libc`/`libGL`/`libEGL`/`libdrm`,
      and does **not** hold `libstdc++.so.6` or `libgcc_s.so.1`.
- [x] `libqxcb.so`, the Wayland plugins and `libqoffscreen.so` deployed.
- [x] `libqsqlite.so` is the only SQL driver; no separate `libsqlite3.so.0`.
- [x] Glibc scan shows the intended floor (2.35).
- [x] Runs in `debian:12` and `ubuntu:24.04` with the desktop runtime installed.
- [ ] Licence texts shipped (see [PACKAGING.md](PACKAGING.md) Rule 3).
- [ ] `altisidora.desktop` validates (`desktop-file-validate`).
- [ ] mpv/FFmpeg built audio-only, or the size accepted and recorded here.
- [ ] Publish: `v0.1.0` tag and real `sha256sums`.

## Open decisions

| Decision | Status |
|---|---|
| glibc floor | **Decided: 2.35**; 2.31 is a documented future option |
| Which Qt to bundle | **Decided:** Qt 6.8.3 official binaries (glibc 2.28 floor) |
| FFmpeg audio-only vs full | Open — deferred, ~50 MB lever, needs more RAM than the Docker VM has |
| AppStream component id | **`org.altisidora.Altisidora`** (placeholder); verify the licence string before publishing |
| CI provider | None exists in the repo today |
