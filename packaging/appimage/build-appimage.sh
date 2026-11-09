#!/usr/bin/env bash
#
# Build an Altisidora AppImage from a source checkout.
#
# Designed to run on Debian 12 (bookworm) or inside the equivalent Docker
# image. It never modifies the source tree: all outputs go to WORK_DIR and
# OUT_DIR. The repository is read-only from the script's point of view.
#
# Run it from a checkout (writes to build/appimage and dist/ by default):
#
#   packaging/appimage/build-appimage.sh
#
# Or inside a container with the checkout read-only and a writable work
# directory mounted (see packaging/appimage/README.md for the exact package
# lists for Rocky Linux 9 and Debian 12):
#
#   docker run --rm -it \
#     -v /path/to/Altisidora:/src:ro \
#     -v /path/to/work:/work \
#     -w /work <image> bash -lc '
#       <install build dependencies>
#       REPO_DIR=/src WORK_DIR=/work/work OUT_DIR=/work/dist \
#         /src/packaging/appimage/build-appimage.sh'
#
# Environment overrides:
#   REPO_DIR, WORK_DIR, OUT_DIR, TOOLS_DIR, BUILD_DIR, APPDIR
#   VERSION (default: VERSION from CMakeLists.txt, then 0.1.0)
#   ARCH (default: x86_64), JOBS, CLEAN_BUILD=1
#   ICON_SOURCE, ICON_SIZE=512, DESKTOP_FILE
#   QMAKE_BIN, QML_SOURCES_PATHS
#   REQUIRE_WAYLAND=0 (skip Wayland plugins), EXTRA_QT_MODULES
#   COMPRESSION=zstd|xz|gzip, SKIP_APPSTREAM=1
#   APPSTREAM_FILE
#   NO_STRIP_LINUXDEPLOY=0 (not recommended; see report)
#   SKIP_TOOL_DOWNLOAD=1 (only if the three AppImages already exist)
#
set -euo pipefail

trap 'printf "ERROR: command failed at line %s\n" "$LINENO" >&2' ERR

log()  { printf '\n==> %s\n' "$*"; }
warn() { printf 'WARN: %s\n' "$*" >&2; }
die()  { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="${REPO_DIR:-$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)}"
WORK_DIR="${WORK_DIR:-$REPO_DIR/build/appimage}"
OUT_DIR="${OUT_DIR:-$REPO_DIR/dist}"
TOOLS_DIR="${TOOLS_DIR:-$WORK_DIR/tools}"
BUILD_DIR="${BUILD_DIR:-$WORK_DIR/build}"
APPDIR="${APPDIR:-$WORK_DIR/AppDir}"

VERSION="${VERSION:-}"
ARCH="${ARCH:-x86_64}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
CLEAN_BUILD="${CLEAN_BUILD:-0}"

APP_NAME="${APP_NAME:-Altisidora}"
TARGET_NAME="${TARGET_NAME:-Altisidora}"
BIN_NAME="${BIN_NAME:-altisidora}"
APP_ID="${APP_ID:-org.altisidora.Altisidora}"

DESKTOP_FILE="${DESKTOP_FILE:-$REPO_DIR/altisidora.desktop}"
ICON_SOURCE="${ICON_SOURCE:-$REPO_DIR/images/LogoMenu.png}"
ICON_SIZE="${ICON_SIZE:-512}"
QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-$REPO_DIR/resources/qml}"

REQUIRE_WAYLAND="${REQUIRE_WAYLAND:-1}"
EXTRA_QT_MODULES="${EXTRA_QT_MODULES:-}"
COMPRESSION="${COMPRESSION:-zstd}"
SKIP_APPSTREAM="${SKIP_APPSTREAM:-0}"
NO_STRIP_LINUXDEPLOY="${NO_STRIP_LINUXDEPLOY:-1}"

APPSTREAM_FILE="${APPSTREAM_FILE:-}"
if [[ -z "$APPSTREAM_FILE" && -f "$REPO_DIR/packaging/${APP_ID}.metainfo.xml" ]]; then
    APPSTREAM_FILE="$REPO_DIR/packaging/${APP_ID}.metainfo.xml"
fi

# Pinned, reproducible tool releases (verified 2026-11-09).
LINUXDEPLOY_URL="${LINUXDEPLOY_URL:-https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage}"
LINUXDEPLOY_SHA256="${LINUXDEPLOY_SHA256:-c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d}"
PLUGIN_QT_URL="${PLUGIN_QT_URL:-https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage}"
PLUGIN_QT_SHA256="${PLUGIN_QT_SHA256:-15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724}"
APPIMAGETOOL_URL="${APPIMAGETOOL_URL:-https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage}"
APPIMAGETOOL_SHA256="${APPIMAGETOOL_SHA256:-ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0}"

# The type2 runtime is a moving "continuous" artifact. The checksum below is
# the one observed on 2026-10-19. Override RUNTIME_SHA256= to skip checking,
# or point RUNTIME_FILE at a locally pinned runtime.
RUNTIME_URL="${RUNTIME_URL:-https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64}"
RUNTIME_SHA256="${RUNTIME_SHA256:-617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46}"

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-$ARCH.AppImage"
PLUGIN_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-$ARCH.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-$ARCH.AppImage"
RUNTIME_FILE="${RUNTIME_FILE:-$TOOLS_DIR/runtime-$ARCH}"

export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH
if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
    export SOURCE_DATE_EPOCH
fi

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------
[[ -f "$REPO_DIR/CMakeLists.txt" ]] || die "REPO_DIR does not look like the Altisidora checkout: $REPO_DIR"
[[ -f "$DESKTOP_FILE" ]] || die "Desktop file not found: $DESKTOP_FILE"
[[ -f "$ICON_SOURCE" ]] || die "Icon not found: $ICON_SOURCE"
[[ -d "$QML_SOURCES_PATHS" ]] || die "QML sources directory not found: $QML_SOURCES_PATHS"

if [[ -z "$VERSION" ]]; then
    VERSION="$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\+\([0-9][0-9A-Za-z._-]*\).*/\1/p' \
        "$REPO_DIR/CMakeLists.txt" | head -n 1)"
fi
VERSION="${VERSION:-0.1.0}"
[[ "$VERSION" =~ ^[0-9] ]] || die "Invalid VERSION: $VERSION"

TARGET="$TARGET_NAME"
OUTPUT="$OUT_DIR/${APP_NAME}-${VERSION}-${ARCH}.AppImage"
ICON_512="$WORK_DIR/${BIN_NAME}-${ICON_SIZE}.png"
APPRUN="$WORK_DIR/AppRun"

mkdir -p "$WORK_DIR" "$OUT_DIR" "$TOOLS_DIR"

# ---------------------------------------------------------------------------
# Dependency checks. These are the Debian 12 package names. If a command or
# Qt file is missing, fail early with the exact apt command to install.
# ---------------------------------------------------------------------------
APT_PACKAGES=(
    build-essential cmake ninja-build pkg-config curl ca-certificates
    file binutils imagemagick desktop-file-utils appstream
    qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools
    qt6-wayland qt6-image-formats-plugins qt6-translations-l10n
    libqt6sql6-sqlite
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts
    qml6-module-qtquick-dialogs qml6-module-qtquick-templates
    qml6-module-qtquick-window qml6-module-qtquick-shapes
    qml6-module-qt-labs-folderlistmodel qml6-module-qtqml
    qml6-module-qtqml-models qml6-module-qtqml-workerscript
    libmpv-dev libtag1-dev libgl1-mesa-dev libegl1-mesa-dev
)
APT_HINT="apt-get update && apt-get install -y --no-install-recommends ${APT_PACKAGES[*]}"

missing_cmds=()
for cmd in cmake pkg-config curl sha256sum strip file; do
    command -v "$cmd" >/dev/null 2>&1 || missing_cmds+=("$cmd")
done
if ! command -v ninja >/dev/null 2>&1; then
    warn "ninja not found; CMake will use the default generator"
fi
if ((${#missing_cmds[@]})); then
    die "Missing commands: ${missing_cmds[*]}. On Debian 12 run: $APT_HINT"
fi

if ! command -v magick >/dev/null 2>&1 && ! command -v convert >/dev/null 2>&1; then
    die "ImageMagick (magick or convert) is required. On Debian 12: apt-get install -y imagemagick"
fi
if ! command -v identify >/dev/null 2>&1 && ! command -v magick >/dev/null 2>&1; then
    die "ImageMagick identify is required. On Debian 12: apt-get install -y imagemagick"
fi

QMAKE_BIN="${QMAKE_BIN:-$(command -v qmake6 2>/dev/null || true)}"
[[ -n "$QMAKE_BIN" ]] || die "qmake6 not found. On Debian 12: apt-get install -y qt6-base-dev"
[[ -x "$QMAKE_BIN" ]] || die "QMAKE_BIN is not executable: $QMAKE_BIN"
export QMAKE="$QMAKE_BIN"

QT_VERSION="$("$QMAKE_BIN" -query QT_VERSION)"
[[ "$QT_VERSION" == 6.* ]] || die "Qt 6 required, qmake6 reports: $QT_VERSION"
QT_PLUGINS_DIR="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)"
QT_QML_DIR="$("$QMAKE_BIN" -query QT_INSTALL_QML)"
QT_LIBEXECS="$("$QMAKE_BIN" -query QT_INSTALL_LIBEXECS)"
QT_LIBS_DIR="$("$QMAKE_BIN" -query QT_INSTALL_LIBS)"
[[ -d "$QT_PLUGINS_DIR" ]] || die "QT_INSTALL_PLUGINS does not exist: $QT_PLUGINS_DIR"
[[ -d "$QT_QML_DIR" ]] || die "QT_INSTALL_QML does not exist: $QT_QML_DIR"
export PATH="$QT_LIBEXECS:$PATH"
command -v qmlimportscanner >/dev/null 2>&1 || die \
    "qmlimportscanner not found under $QT_LIBEXECS or PATH. On Debian 12: apt-get install -y qt6-declarative-dev-tools"

pkg-config --exists taglib || die "TagLib development files not found. On Debian 12: apt-get install -y libtag1-dev"
pkg-config --exists mpv    || die "libmpv development files not found. On Debian 12: apt-get install -y libmpv-dev"

# Runtime Qt pieces that linuxdeploy-plugin-qt must find at build time.
[[ -f "$QT_PLUGINS_DIR/platforms/libqxcb.so" ]] || die "libqxcb.so missing (libqt6gui6). $APT_HINT"
[[ -f "$QT_PLUGINS_DIR/platforms/libqoffscreen.so" ]] || die "libqoffscreen.so missing (libqt6gui6). $APT_HINT"
[[ -f "$QT_PLUGINS_DIR/platforms/libqminimal.so" ]] || die "libqminimal.so missing (libqt6gui6). $APT_HINT"
[[ -f "$QT_PLUGINS_DIR/sqldrivers/libqsqlite.so" ]] || die \
    "SQLite driver missing. On Debian 12: apt-get install -y libqt6sql6-sqlite"
[[ -f "$QT_PLUGINS_DIR/imageformats/libqjpeg.so" ]] || die "JPEG image plugin missing (libqt6gui6). $APT_HINT"

if [[ "$REQUIRE_WAYLAND" == 1 ]]; then
    # Qt 6.10 ships one unified libqwayland.so; older releases ship the
    # libqwayland-egl.so / libqwayland-generic.so pair. Accept either.
    if [[ ! -f "$QT_PLUGINS_DIR/platforms/libqwayland.so" \
          && ! ( -f "$QT_PLUGINS_DIR/platforms/libqwayland-egl.so" \
                 && -f "$QT_PLUGINS_DIR/platforms/libqwayland-generic.so" ) ]]; then
        die "Wayland platform plugin missing. Install qt6-wayland/qt6-qtwayland (or set REQUIRE_WAYLAND=0)"
    fi
fi

# QML modules actually imported by Altisidora's QML (including transitive
# imports of QtQuick.Controls and QtQuick.Dialogs).
QML_REQUIRED=(
    "QtQuick/qmldir"
    "QtQuick/Controls/qmldir"
    "QtQuick/Controls/Basic/qmldir"
    "QtQuick/Layouts/qmldir"
    "QtQuick/Dialogs/qmldir"
    "QtQuick/Dialogs/quickimpl/qmldir"
    "QtQuick/Templates/qmldir"
    "QtQuick/Window/qmldir"
    "QtQuick/Shapes/qmldir"
    "Qt/labs/folderlistmodel/qmldir"
    "QtQml/qmldir"
    "QtQml/Models/qmldir"
)
# NOTE: QtQml is a "system" module, and linuxdeploy-plugin-qt may treat it as
# built-in and not copy its directory. The fallback copy below guarantees it
# is present. QtQml.WorkerScript is not used by Altisidora and is not required.
for rel in "${QML_REQUIRED[@]}"; do
    [[ -f "$QT_QML_DIR/$rel" ]] || die "QML module file missing: $QT_QML_DIR/$rel. $APT_HINT"
done

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$DESKTOP_FILE"
else
    warn "desktop-file-validate not found; skipping desktop file validation"
fi

if [[ -n "$APPSTREAM_FILE" ]]; then
    [[ -f "$APPSTREAM_FILE" ]] || die "APPSTREAM_FILE not found: $APPSTREAM_FILE"
    if command -v appstreamcli >/dev/null 2>&1; then
        appstreamcli validate --no-net "$APPSTREAM_FILE"
    else
        warn "appstreamcli not found; skipping AppStream validation. On Debian 12: apt-get install -y appstream"
    fi
fi

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
fetch_and_verify() {
    local url="$1" sha="$2" dest="$3"
    local tmp="${dest}.part"

    if [[ -f "$dest" ]]; then
        if [[ -z "$sha" ]] || echo "$sha  $dest" | sha256sum -c --status 2>/dev/null; then
            return 0
        fi
        warn "Checksum mismatch for $dest; downloading it again"
        rm -f "$dest"
    fi

    log "Downloading $(basename "$dest")"
    rm -f "$tmp"
    curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 -o "$tmp" "$url"
    if [[ -n "$sha" ]]; then
        echo "$sha  $tmp" | sha256sum -c --status || {
            rm -f "$tmp"
            die "SHA-256 mismatch for $dest (expected $sha). Update the URL/checksum in this script."
        }
    fi
    mv "$tmp" "$dest"
    chmod +x "$dest"
}

if [[ "${SKIP_TOOL_DOWNLOAD:-0}" != 1 ]]; then
    fetch_and_verify "$LINUXDEPLOY_URL" "$LINUXDEPLOY_SHA256" "$LINUXDEPLOY"
    fetch_and_verify "$PLUGIN_QT_URL" "$PLUGIN_QT_SHA256" "$PLUGIN_QT"
    fetch_and_verify "$APPIMAGETOOL_URL" "$APPIMAGETOOL_SHA256" "$APPIMAGETOOL"
    fetch_and_verify "$RUNTIME_URL" "$RUNTIME_SHA256" "$RUNTIME_FILE"
else
    for f in "$LINUXDEPLOY" "$PLUGIN_QT" "$APPIMAGETOOL" "$RUNTIME_FILE"; do
        [[ -f "$f" ]] || die "SKIP_TOOL_DOWNLOAD=1 but $f does not exist"
    done
fi

[[ -x "$LINUXDEPLOY" ]] || die "linuxdeploy is not executable: $LINUXDEPLOY"
[[ -x "$PLUGIN_QT" ]] || die "linuxdeploy-plugin-qt is not executable: $PLUGIN_QT"
[[ -x "$APPIMAGETOOL" ]] || die "appimagetool is not executable: $APPIMAGETOOL"

log "Tool versions"
"$LINUXDEPLOY" --version
"$PLUGIN_QT" --plugin-version >/dev/null 2>&1 || true
"$APPIMAGETOOL" --version > "$WORK_DIR/appimagetool-version.txt" 2>&1 || true
head -n 1 "$WORK_DIR/appimagetool-version.txt" || true

# The plugin is discovered in the directory containing linuxdeploy, or in PATH.
export PATH="$TOOLS_DIR:$PATH"
"$LINUXDEPLOY" --list-plugins > "$WORK_DIR/linuxdeploy-plugins.txt" 2>&1 || true
if ! grep -q 'qt' "$WORK_DIR/linuxdeploy-plugins.txt"; then
    cat "$WORK_DIR/linuxdeploy-plugins.txt" >&2
    die "linuxdeploy does not see the Qt plugin. Put $PLUGIN_QT next to $LINUXDEPLOY or in PATH."
fi

# ---------------------------------------------------------------------------
# Icon (the repository icon is 2000x2000; linuxdeploy only accepts standard
# icon sizes, and 512x512 is what CMake claims to install).
# ---------------------------------------------------------------------------
log "Preparing ${ICON_SIZE}x${ICON_SIZE} icon"
if command -v magick >/dev/null 2>&1; then
    magick "$ICON_SOURCE" -resize "${ICON_SIZE}x${ICON_SIZE}" -strip \
        -define png:compression-level=9 "$ICON_512"
else
    convert "$ICON_SOURCE" -resize "${ICON_SIZE}x${ICON_SIZE}" -strip \
        -define png:compression-level=9 "$ICON_512"
fi
if command -v magick >/dev/null 2>&1; then
    ICON_DIMS="$(magick identify -format '%wx%h' "$ICON_512" 2>/dev/null || true)"
else
    ICON_DIMS="$(identify -format '%wx%h' "$ICON_512" 2>/dev/null || true)"
fi
[[ "$ICON_DIMS" == "${ICON_SIZE}x${ICON_SIZE}" ]] || die \
    "Icon resize produced ${ICON_DIMS:-unknown} instead of ${ICON_SIZE}x${ICON_SIZE}"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if [[ "$CLEAN_BUILD" == 1 ]]; then
    log "Removing build directory (CLEAN_BUILD=1)"
    rm -rf "$BUILD_DIR"
fi

log "Configuring CMake (Release)"
CMAKE_GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR_ARGS=(-G Ninja)
fi
cmake -S "$REPO_DIR" -B "$BUILD_DIR" \
    "${CMAKE_GENERATOR_ARGS[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DQt6_DIR="$QT_LIBS_DIR/cmake/Qt6"

log "Building target $TARGET (jobs=$JOBS)"
cmake --build "$BUILD_DIR" --target "$TARGET" --parallel "$JOBS"

log "Installing into AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --strip

[[ -x "$APPDIR/usr/bin/$BIN_NAME" ]] || die "Installed binary not found: $APPDIR/usr/bin/$BIN_NAME"

# License and documentation needed for a GPLv3 binary distribution.
if [[ -f "$REPO_DIR/LICENSE" ]]; then
    install -Dm644 "$REPO_DIR/LICENSE" "$APPDIR/usr/share/doc/$BIN_NAME/LICENSE"
fi

# CMake installs the icon as 512x512 even though the source is 2000x2000.
# Overwrite it with the correctly sized image before linuxdeploy runs.
install -Dm644 "$ICON_512" "$APPDIR/usr/share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps/${BIN_NAME}.png"

# Make sure the desktop entry is where CMake would put it (CMake does this,
# but keeping it explicit makes the script robust if the install rules change).
install -Dm644 "$DESKTOP_FILE" "$APPDIR/usr/share/applications/${BIN_NAME}.desktop"

if [[ -n "$APPSTREAM_FILE" ]]; then
    install -Dm644 "$APPSTREAM_FILE" "$APPDIR/usr/share/metainfo/${APP_ID}.metainfo.xml"
fi

# ---------------------------------------------------------------------------
# Custom AppRun. linuxdeploy-plugin-qt creates usr/bin/qt.conf for Qt 6 and
# relies on it; this wrapper additionally pins the bundled paths and the
# Controls style, so a host QT_* environment cannot leak into the bundle.
# ---------------------------------------------------------------------------
log "Writing AppRun"
sed "s|@BIN_NAME@|$BIN_NAME|g" > "$APPRUN" <<'APPRUN_EOF'
#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APPDIR=${APPDIR:-$HERE}
export APPDIR

# Bundle paths first. Do not append host Qt paths: the whole point is to use
# the libraries and QML modules shipped inside this AppImage.
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$APPDIR/usr/lib/x86_64-linux-gnu"
export QT_PLUGIN_PATH="$APPDIR/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/usr/plugins/platforms"
export QML_IMPORT_PATH="$APPDIR/usr/qml"
export QML2_IMPORT_PATH="$APPDIR/usr/qml"

# Pin the Controls style. Basic is deployed by linuxdeploy-plugin-qt and does
# not depend on the host's QT_QUICK_CONTROLS_STYLE.
export QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-Basic}"

exec "$APPDIR/usr/bin/@BIN_NAME@" "$@"
APPRUN_EOF
chmod 0755 "$APPRUN"

# ---------------------------------------------------------------------------
# linuxdeploy + Qt plugin
# ---------------------------------------------------------------------------
PLATFORM_PLUGINS="libqoffscreen.so;libqminimal.so"
if [[ "$REQUIRE_WAYLAND" == 1 ]]; then
    for wayland_plugin in libqwayland.so libqwayland-egl.so libqwayland-generic.so; do
        if [[ -f "$QT_PLUGINS_DIR/platforms/$wayland_plugin" ]]; then
            PLATFORM_PLUGINS="$PLATFORM_PLUGINS;$wayland_plugin"
        fi
    done
fi

export QMAKE="$QMAKE_BIN"
export QML_SOURCES_PATHS
export EXTRA_PLATFORM_PLUGINS="$PLATFORM_PLUGINS"
if [[ -n "$EXTRA_QT_MODULES" ]]; then
    export EXTRA_QT_MODULES
else
    unset EXTRA_QT_MODULES || true
fi

# The bundled strip in linuxdeploy 1-alpha-20251107-1 is old and fails on
# binaries containing .relr.dyn (seen on Arch/Ubuntu 24.04). The app binary
# was already stripped by `cmake --install --strip`; Debian's Qt/ffmpeg libs
# are stripped by the distro. NO_STRIP=1 avoids the tool's strip entirely.
if [[ "$NO_STRIP_LINUXDEPLOY" == 1 ]]; then
    export NO_STRIP=1
else
    unset NO_STRIP || true
fi

log "Running linuxdeploy with the Qt plugin"
LD_ARGS=(
    --appdir "$APPDIR"
    --executable "$APPDIR/usr/bin/$BIN_NAME"
    --desktop-file "$DESKTOP_FILE"
    --icon-file "$ICON_512"
    --icon-filename "$BIN_NAME"
    --custom-apprun "$APPRUN"
    --plugin qt
)
if [[ "${DEBUG:-0}" == 1 ]]; then
    LD_ARGS=(-v0 "${LD_ARGS[@]}")
    export DEBUG=1
fi
"$LINUXDEPLOY" "${LD_ARGS[@]}"

# Defensive fallback: if qmlimportscanner missed a module that we know the app
# needs, copy that module straight from QT_INSTALL_QML. The plugin normally
# does this itself; the fallback makes the build independent of scanner
# version quirks. Missing modules are reported loudly.
for rel in "${QML_REQUIRED[@]}"; do
    module_dir="${rel%/qmldir}"
    dest="$APPDIR/usr/qml/$module_dir"
    if [[ ! -f "$dest/qmldir" ]]; then
        warn "QML module was not deployed by linuxdeploy-plugin-qt: $module_dir"
        warn "Copying it from $QT_QML_DIR/$module_dir"
        mkdir -p "$dest"
        cp -a "$QT_QML_DIR/$module_dir/." "$dest/"
    fi
done

# The defensive QML copies above may bring plugins whose Qt library was not
# part of the original dependency graph (for example Qt.labs.folderlistmodel
# needs libQt6LabsFolderListModel.so.6). Copy any still-missing libQt6*
# library from the Qt installation until the QML plugins resolve. The loop
# handles Qt libraries that themselves pull in other Qt libraries.
for _ in 1 2 3; do
    copied=0
    while IFS= read -r -d '' so_file; do
        # Read NEEDED directly instead of parsing ldd: on the build host the
        # library is resolvable from /usr/lib, so ldd would not report it as
        # missing even though it is absent from the AppDir.
        while IFS= read -r lib; do
            [[ "$lib" == libQt6* ]] || continue
            if [[ ! -e "$APPDIR/usr/lib/$lib" && -e "$QT_LIBS_DIR/$lib" ]]; then
                install -Dm755 "$(readlink -f "$QT_LIBS_DIR/$lib")" \
                    "$APPDIR/usr/lib/$lib"
                copied=1
            fi
        done < <(readelf -d "$so_file" 2>/dev/null \
                 | awk '/NEEDED/ { gsub(/\[|\]/, "", $NF); print $NF }')
    done < <(find "$APPDIR/usr/qml" -type f -name '*.so' -print0)
    [[ "$copied" == 1 ]] || break
done

# linuxdeploy-plugin-qt deploys the Wayland platform plugins but not always
# the xdg-shell integration that the Wayland platform needs to map windows.
# Copy it explicitly when the distro provides it.
if [[ "$REQUIRE_WAYLAND" == 1
      && ! -f "$APPDIR/usr/plugins/wayland-shell-integration/libxdg-shell.so" ]]; then
    xdg_shell="$QT_PLUGINS_DIR/wayland-shell-integration/libxdg-shell.so"
    if [[ -f "$xdg_shell" ]]; then
        warn "Wayland xdg-shell integration was not deployed; copying it explicitly"
        install -Dm755 "$xdg_shell" \
            "$APPDIR/usr/plugins/wayland-shell-integration/libxdg-shell.so"
    else
        warn "Wayland xdg-shell integration not found in $QT_PLUGINS_DIR"
    fi
fi

# linuxdeploy deliberately excludes some system libraries. libmpv has hard
# NEEDED entries on libjack and libusb even when the host does not run JACK or
# use USB audio; libjack itself needs Berkeley DB. Bundle all three so the
# AppImage does not fail at loader time on otherwise complete desktops.
EXTRA_BUNDLED_LIBS=(
    libjack.so.0 libdb-5.3.so libusb-1.0.so.0
    libopus.so.0 libsamplerate.so.0 libpipewire-0.3.so.0
)
for lib in "${EXTRA_BUNDLED_LIBS[@]}"; do
    # Prefer QT_INSTALL_LIBS: on systems that offer both PipeWire's JACK
    # shim and classic JACK2, the classic library lives in the main lib
    # directory while the shim lives in a pipewire-0.3/jack subdirectory.
    # ldconfig may list the shim first, and the shim needs a matching
    # (newer) libpipewire than many hosts provide.
    if [[ -e "$QT_LIBS_DIR/$lib" ]]; then
        lib_path="$QT_LIBS_DIR/$lib"
    else
        lib_path="$(ldconfig -p 2>/dev/null | awk -v name="$lib" '$1 == name { print $NF; exit }')"
    fi
    if [[ -n "$lib_path" && -e "$lib_path" ]]; then
        install -Dm755 "$(readlink -f "$lib_path")" "$APPDIR/usr/lib/$lib"
    else
        warn "Optional library not found on the build system: $lib"
    fi
done

# linuxdeploy normally creates root symlinks. Use regular files at the root:
# some desktop integration tools do not follow symlinks in an AppImage.
rm -f "$APPDIR/${BIN_NAME}.desktop" "$APPDIR/${BIN_NAME}.png"
install -m644 "$DESKTOP_FILE" "$APPDIR/${BIN_NAME}.desktop"
install -m644 "$ICON_512" "$APPDIR/${BIN_NAME}.png"

# ---------------------------------------------------------------------------
# AppDir sanity checks
# ---------------------------------------------------------------------------
log "Verifying AppDir"

check_file() {
    local path="$1"
    [[ -f "$path" ]] || die "Missing expected file: $path"
}
check_file "$APPDIR/AppRun"
check_file "$APPDIR/${BIN_NAME}.desktop"
check_file "$APPDIR/${BIN_NAME}.png"
check_file "$APPDIR/usr/bin/$BIN_NAME"
check_file "$APPDIR/usr/bin/qt.conf"
check_file "$APPDIR/usr/plugins/platforms/libqxcb.so"
check_file "$APPDIR/usr/plugins/platforms/libqoffscreen.so"
check_file "$APPDIR/usr/plugins/sqldrivers/libqsqlite.so"
check_file "$APPDIR/usr/plugins/imageformats/libqjpeg.so"

for qml_dir in \
    "QtQuick" \
    "QtQuick/Controls" \
    "QtQuick/Controls/Basic" \
    "QtQuick/Layouts" \
    "QtQuick/Dialogs" \
    "QtQuick/Dialogs/quickimpl" \
    "QtQuick/Templates" \
    "QtQuick/Window" \
    "QtQuick/Shapes" \
    "Qt/labs/folderlistmodel" \
    "QtQml" \
    "QtQml/Models"; do
    check_file "$APPDIR/usr/qml/$qml_dir/qmldir"
done
# QtQml.WorkerScript is not used by Altisidora's QML and is not hard-required.

if [[ "$REQUIRE_WAYLAND" == 1 ]]; then
    wayland_deployed=0
    for wayland_plugin in libqwayland.so libqwayland-egl.so libqwayland-generic.so; do
        if [[ -f "$APPDIR/usr/plugins/platforms/$wayland_plugin" ]]; then
            wayland_deployed=1
        fi
    done
    [[ "$wayland_deployed" == 1 ]] || die "Wayland platform plugin was not deployed"
    # Without the xdg-shell integration Wayland cannot map windows. It is a
    # separate plugin in Qt <= 6.9 and is still shipped as one in Qt 6.10.
    if [[ -f "$QT_PLUGINS_DIR/wayland-shell-integration/libxdg-shell.so" ]]; then
        check_file "$APPDIR/usr/plugins/wayland-shell-integration/libxdg-shell.so"
    fi
fi

# libmpv and TagLib must be present, together with libmpv's FFmpeg stack.
compgen -G "$APPDIR/usr/lib/libmpv.so.2*" >/dev/null || die "libmpv.so.2 was not deployed"
compgen -G "$APPDIR/usr/lib/libtag.so.1*" >/dev/null || die "libtag.so.1 was not deployed"
for pattern in 'libavcodec.so.*' 'libavformat.so.*' 'libavutil.so.*' 'libswresample.so.*' 'libswscale.so.*'; do
    compgen -G "$APPDIR/usr/lib/$pattern" >/dev/null || die "libmpv dependency not deployed: $pattern"
done

# Broken or absolute symlinks are fatal: they would resolve on the build host
# but not inside the AppImage.
find "$APPDIR" -xtype l -print > "$WORK_DIR/broken-symlinks.txt" || true
if [[ -s "$WORK_DIR/broken-symlinks.txt" ]]; then
    cat "$WORK_DIR/broken-symlinks.txt" >&2
    die "Broken symlink(s) found in AppDir"
fi
while IFS= read -r -d '' link; do
    target="$(readlink "$link")"
    if [[ "$target" == /* ]]; then
        die "Absolute symlink in AppDir: $link -> $target"
    fi
done < <(find "$APPDIR" -type l -print0)

# The executable must not need libraries that are missing at runtime.
LDD_ENV=("LD_LIBRARY_PATH=$APPDIR/usr/lib:$APPDIR/usr/lib/x86_64-linux-gnu")
env "${LDD_ENV[@]}" ldd "$APPDIR/usr/bin/$BIN_NAME" > "$WORK_DIR/ldd-app.txt" 2>&1 || true
if grep -q 'not found' "$WORK_DIR/ldd-app.txt"; then
    cat "$WORK_DIR/ldd-app.txt" >&2
    die "The application has unresolved shared library dependencies"
fi
if [[ -e "$APPDIR/usr/lib/libmpv.so.2" ]]; then
    env "${LDD_ENV[@]}" ldd "$APPDIR/usr/lib/libmpv.so.2" > "$WORK_DIR/ldd-libmpv.txt" 2>&1 || true
    if grep -q 'not found' "$WORK_DIR/ldd-libmpv.txt"; then
        cat "$WORK_DIR/ldd-libmpv.txt" >&2
        die "libmpv has unresolved shared library dependencies"
    fi
fi

# QML plugin shared objects can also pull in Qt libraries (or a missing
# dependency introduced by the fallback copy above). Check them as well.
QML_LDD_LOG="$WORK_DIR/ldd-qml.txt"
: > "$QML_LDD_LOG"
while IFS= read -r -d '' so_file; do
    env "${LDD_ENV[@]}" ldd "$so_file" > "$WORK_DIR/ldd-qml-one.txt" 2>&1 || true
    if grep -q 'not found' "$WORK_DIR/ldd-qml-one.txt"; then
        {
            printf '== %s\n' "$so_file"
            cat "$WORK_DIR/ldd-qml-one.txt"
        } >> "$QML_LDD_LOG"
    fi
done < <(find "$APPDIR/usr/qml" -type f -name '*.so' -print0)
if [[ -s "$QML_LDD_LOG" ]]; then
    cat "$QML_LDD_LOG" >&2
    die "QML plugin(s) have unresolved shared library dependencies"
fi

if command -v readelf >/dev/null 2>&1; then
    readelf -d "$APPDIR/usr/bin/$BIN_NAME" > "$WORK_DIR/readelf-dynamic.txt" 2>&1 || true
    if ! grep -qE 'RPATH|RUNPATH' "$WORK_DIR/readelf-dynamic.txt"; then
        warn "No RPATH/RUNPATH in the application binary; AppRun's LD_LIBRARY_PATH is the fallback"
    fi
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$APPDIR/${BIN_NAME}.desktop"
fi

if [[ -n "$APPSTREAM_FILE" ]] && command -v appstreamcli >/dev/null 2>&1; then
    appstreamcli validate --no-net "$APPDIR/usr/share/metainfo/${APP_ID}.metainfo.xml"
fi

# ---------------------------------------------------------------------------
# AppImage
# ---------------------------------------------------------------------------
log "Generating AppImage: $OUTPUT"
rm -f "$OUTPUT" "$OUTPUT.sha256"
APPIMAGETOOL_ARGS=(--comp "$COMPRESSION")
if [[ -n "$RUNTIME_FILE" ]]; then
    APPIMAGETOOL_ARGS+=(--runtime-file "$RUNTIME_FILE")
fi
if [[ "$SKIP_APPSTREAM" == 1 ]]; then
    APPIMAGETOOL_ARGS+=(--no-appstream)
fi
ARCH="$ARCH" "$APPIMAGETOOL" "${APPIMAGETOOL_ARGS[@]}" "$APPDIR" "$OUTPUT"

[[ -x "$OUTPUT" ]] || die "appimagetool did not produce an executable AppImage: $OUTPUT"
chmod 0755 "$OUTPUT"
sha256sum "$OUTPUT" > "$OUTPUT.sha256"

log "Done"
printf 'AppImage: %s\n' "$OUTPUT"
printf 'Size:     %s\n' "$(du -h "$OUTPUT" | cut -f1)"
printf 'SHA256:   %s\n' "$(cut -d' ' -f1 "$OUTPUT.sha256")"
printf '\nSmoke test (no FUSE, no display):\n'
printf '  APPIMAGE_EXTRACT_AND_RUN=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \\\n'
printf '    timeout 20s %s\n' "$OUTPUT"
