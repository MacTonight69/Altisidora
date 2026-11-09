# AppImage packaging

`build-appimage.sh` produces a self-contained x86_64 AppImage from a source
checkout. By default it builds in `build/appimage/` and writes
`dist/Altisidora-<version>-x86_64.AppImage` plus a `.sha256` file.

## Recommended build base

Use a distribution with **Qt >= 6.5**. The application uses
`Control.focusPolicy` and other Qt Quick Controls features that are not
available in Qt 6.4 (Debian 12 / Ubuntu 24.04), so a Qt 6.4 build compiles but
fails at QML load time.

The reference build is **Rocky Linux 9.3** with Qt 6.10.3 from EPEL:

- glibc 2.34 (the AppImage runs on Rocky 9+, Debian 12+, Ubuntu 22.04+,
  Fedora 36+ and Arch)
- Qt 6.10.3, TagLib 1.12, libmpv 0.39
- Wayland and X11 platform plugins

## Build inside Docker (reference recipe)

```bash
mkdir -p dist
docker run --rm -it \
  -v "$PWD":/src:ro \
  -v "$PWD/dist":/out \
  rockylinux:9.3.20231119-minimal bash -lc '
    set -e
    microdnf install -y epel-release
    sed -i "/\[crb\]/,/enabled=/s/enabled=0/enabled=1/" /etc/yum.repos.d/rocky.repo
    microdnf makecache
    microdnf install -y \
      cmake gcc-c++ make ninja-build pkgconf-pkg-config \
      curl ca-certificates file binutils findutils gawk \
      ImageMagick desktop-file-utils appstream \
      qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland \
      taglib-devel mpv-devel
    REPO_DIR=/src WORK_DIR=/tmp/work OUT_DIR=/out \
      /src/packaging/appimage/build-appimage.sh
  '
```

The script downloads pinned `linuxdeploy`, `linuxdeploy-plugin-qt`,
`appimagetool` and the type-2 runtime, verifies their SHA-256 checksums, and
uses `APPIMAGE_EXTRACT_AND_RUN=1` so FUSE is not required inside the
container.

## Native build

On a writable checkout with the dependencies already installed:

```bash
packaging/appimage/build-appimage.sh
```

Environment overrides: `REPO_DIR`, `WORK_DIR`, `OUT_DIR`, `TOOLS_DIR`,
`BUILD_DIR`, `APPDIR`, `VERSION`, `ARCH`, `JOBS`, `CLEAN_BUILD`, `ICON_SIZE`,
`QMAKE_BIN`, `QML_SOURCES_PATHS`, `REQUIRE_WAYLAND`, `EXTRA_QT_MODULES`,
`COMPRESSION`, `SKIP_TOOL_DOWNLOAD`.

## What is bundled

- Qt 6 libraries, platform plugins (xcb, offscreen, minimal, Wayland),
  SQLite driver, image format plugins and the QML modules imported by the app.
- libmpv and its FFmpeg/libplacebo/libass stack.
- TagLib.
- The matching `libpipewire-0.3` and classic JACK2 `libjack` that libmpv links
  against, so the loader does not require a newer host PipeWire than the one
  the AppImage was built with.
- The desktop entry, a 512x512 icon and the GPLv3 license.

The application binary, QML, images and theme fonts are compiled into Qt
resources; no external data directory is needed.

## Runtime requirements (provided by the host)

The AppImage intentionally uses the host's graphics and audio stack:

- OpenGL/EGL/GLX (`libOpenGL.so.0`, `libGLX.so.0`, `libEGL.so.1`) and a
  working Mesa/DRI driver
- X11/xcb libraries, or Wayland client libraries and an XDG-shell compositor
- ALSA, PulseAudio or PipeWire
- fontconfig, FreeType and HarfBuzz

These are part of any normal desktop installation. A Japanese/CJK font is
optional for displaying CJK metadata, as documented in the main README.

## Verification

Without FUSE or a display:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=offscreen \
  timeout 15s dist/Altisidora-0.1.0-x86_64.AppImage
# exit 124 is expected: the app stayed alive until timeout
```

With X11 (Xvfb):

```bash
timeout 20s xvfb-run -a -s "-screen 0 1280x800x24" \
  dist/Altisidora-0.1.0-x86_64.AppImage
```

Check the artifact:

```bash
sha256sum -c dist/Altisidora-0.1.0-x86_64.AppImage.sha256

work=$(mktemp -d)
(cd "$work" && /path/to/dist/Altisidora-0.1.0-x86_64.AppImage \
  --appimage-extract >/dev/null)
desktop-file-validate "$work/squashfs-root/altisidora.desktop"
# inspect $work/squashfs-root/usr/lib, usr/plugins and usr/qml
```

The build script itself verifies that no `libc.so.6` is bundled, that there are
no broken or absolute symlinks, and that the binary, libmpv and QML plugins
resolve their shared libraries.
