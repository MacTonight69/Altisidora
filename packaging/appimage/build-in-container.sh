#!/usr/bin/env bash
#
# Altisidora AppImage recipe -- runs INSIDE the container built from
# packaging/appimage/Dockerfile.
#
# The repository is bind-mounted at /work (SRC_DIR). All build state lives in
# container-local scratch space under /tmp; the only thing written back into
# the repository is the finished AppImage in $SRC_DIR/dist.
#
# Usage (normally through packaging/appimage/make.sh):
#   docker run --rm -v "$PWD":/work -w /work \
#     -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
#     altisidora-appimage bash packaging/appimage/build-in-container.sh

set -euo pipefail

SRC_DIR=${SRC_DIR:-/work}
QT_ROOT=${QT_ROOT:-/opt/Qt}
QT_VERSION=${QT_VERSION:-6.8.3}
QT_DIR=${QT_DIR:-"$QT_ROOT/$QT_VERSION/gcc_64"}
TAGLIB_ROOT=${TAGLIB_ROOT:-/opt/taglib}
ARCH=${ARCH:-x86_64}
JOBS=${JOBS:-$(nproc)}

BUILD=/tmp/altisidora-build
APPDIR=/tmp/Altisidora.AppDir
DIST="$SRC_DIR/dist"

log() { printf '\n==> %s\n' "$*"; }

# Version comes from the single project() VERSION declaration in CMakeLists.txt
# (`cmake_minimum_required(VERSION ...)` is not anchored at the line start, so
# it cannot false-match here).
if [[ -z ${VERSION:-} ]]; then
    VERSION="$(awk '/^[[:space:]]*VERSION[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+/ {print $2; exit}' \
        "$SRC_DIR/CMakeLists.txt")"
fi
if [[ -z ${VERSION:-} ]]; then
    printf 'error: VERSION is unset and could not be read from %s\n' \
        "$SRC_DIR/CMakeLists.txt" >&2
    exit 1
fi

ARTIFACT="$DIST/Altisidora-$VERSION-$ARCH.AppImage"

log "Building Altisidora $VERSION ($ARCH)"
printf '  SRC_DIR      %s\n' "$SRC_DIR"
printf '  QT_DIR       %s\n' "$QT_DIR"
printf '  TAGLIB_ROOT  %s\n' "$TAGLIB_ROOT"
printf '  JOBS         %s\n' "$JOBS"
printf '  artifact     %s\n' "$ARTIFACT"

# Container-local scratch: safe to reset, and a repeated run inside the same
# container must not reuse a stale CMake tree or a stale AppDir.
rm -rf "$BUILD" "$APPDIR"
mkdir -p "$BUILD" "$DIST"

# TagLib is installed outside the system prefix, so pkg-config has to be told
# where its taglib.pc lives (CMakeLists.txt uses pkg_check_modules(TAGLIB)).
export PKG_CONFIG_PATH="$TAGLIB_ROOT/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
# CMakeLists.txt links the bare `${TAGLIB_LIBRARIES}` names (e.g. -ltag) without
# the -L prefix from pkg-config, so the linker and the loader both need that
# directory on their search path. LIBRARY_PATH covers the link, LD_LIBRARY_PATH
# covers ldd, which is what linuxdeploy uses to discover libtag.so.2.
export LIBRARY_PATH="$TAGLIB_ROOT/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
# linuxdeploy discovers dependencies with ldd, and `cmake --install` clears the
# build rpath, so both the threaded Qt libraries and TagLib have to be
# resolvable through the loader path at deploy time.
export LD_LIBRARY_PATH="$QT_DIR/lib:$TAGLIB_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

log "Configuring"
cmake -S "$SRC_DIR" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-12 \
    -DCMAKE_CXX_COMPILER=g++-12 \
    -DCMAKE_PREFIX_PATH="$QT_DIR;$TAGLIB_ROOT" \
    -DCMAKE_INSTALL_PREFIX=/usr

log "Building"
cmake --build "$BUILD" --parallel "$JOBS"

log "Installing into the AppDir"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr

# The AppImage spec wants the desktop entry and the icon at the AppDir root as
# well as in their hicolor locations.
install -Dm644 "$SRC_DIR/altisidora.desktop" "$APPDIR/altisidora.desktop"
install -Dm644 "$SRC_DIR/images/LogoMenu.png" "$APPDIR/altisidora.png"

log "Installing AppStream metadata"
install -Dm644 "$SRC_DIR/metadata/org.altisidora.Altisidora.appdata.xml" \
    "$APPDIR/usr/share/metainfo/org.altisidora.Altisidora.appdata.xml"

# The QML is compiled into the binary through resources/qml.qrc, so the Qt
# plugin cannot find it by scanning the AppDir: QML_SOURCES_PATHS must point at
# the sources. Omitting it produces an AppImage with a blank window.
export QML_SOURCES_PATHS="$SRC_DIR/resources/qml"
export QMAKE="$QT_DIR/bin/qmake6"

# The Qt plugin deploys libqxcb.so and nothing else by default, which leaves the
# bundle unable to start on a Wayland session that has no XWayland -- the default
# on Fedora 40+ and Ubuntu 24.04+. Force both Wayland platform plugins: the EGL
# one for accelerated sessions, the generic one as the software fallback.
# libqoffscreen is a few kilobytes and is what makes the headless smoke run
# possible (`QT_QPA_PLATFORM=offscreen`), so it ships as well.
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so"

# linuxdeploy-plugin-qt deploys whole plugin directories and fails hard the
# moment one plugin's dependencies are missing ("Could not find dependency"),
# and it has no way to exclude a single plugin. Qt's official binaries ship
# MySQL, ODBC, PostgreSQL, Mimer, CUPS and GTK3 plugins whose client libraries
# are not installed here; libmimerapi in particular is packaged by no
# distribution, so the driver set as shipped can never be deployed. This app
# uses SQLite only (QSQLITE in DatabaseManager.cpp) and has no printing or GTK
# theme integration, so those plugins are parked for the duration of the
# linuxdeploy call. They are moved back afterwards, because Qt's CMake package
# configs validate that every plugin file exists: removing them earlier makes
# find_package(Qt6) fail with "references the file ... but this file does not
# exist", and leaving them out breaks a second run in the same container.
QT_PLUGINS_DIR="$QT_DIR/plugins"
PARKED_QT_PLUGINS=/tmp/qt-plugins-parked
UNUSED_QT_PLUGINS=(
    sqldrivers/libqsqlmimer.so
    sqldrivers/libqsqlmysql.so
    sqldrivers/libqsqlodbc.so
    sqldrivers/libqsqlpsql.so
    printsupport/libcupsprintersupport.so
    platformthemes/libqgtk3.so
)

park_unused_qt_plugins() {
    local rel
    for rel in "${UNUSED_QT_PLUGINS[@]}"; do
        [[ -e "$QT_PLUGINS_DIR/$rel" ]] || continue
        mkdir -p "$PARKED_QT_PLUGINS/$(dirname "$rel")"
        mv "$QT_PLUGINS_DIR/$rel" "$PARKED_QT_PLUGINS/$rel"
    done
}

restore_unused_qt_plugins() {
    local rel
    for rel in "${UNUSED_QT_PLUGINS[@]}"; do
        [[ -e "$PARKED_QT_PLUGINS/$rel" ]] || continue
        mv "$PARKED_QT_PLUGINS/$rel" "$QT_PLUGINS_DIR/$rel"
    done
}

log "Deploying with linuxdeploy (Qt plugin)"
park_unused_qt_plugins
linuxdeploy \
    --appdir "$APPDIR" \
    --plugin qt \
    --executable "$APPDIR/usr/bin/altisidora" \
    --desktop-file "$APPDIR/altisidora.desktop" \
    --icon-file "$APPDIR/altisidora.png"
restore_unused_qt_plugins

printf '  platform plugins deployed: %s\n' \
    "$(ls -1 "$APPDIR/usr/plugins/platforms/" 2>/dev/null | tr '\n' ' ')"
printf '  SQL drivers deployed:      %s\n' \
    "$(ls -1 "$APPDIR/usr/plugins/sqldrivers/" 2>/dev/null | tr '\n' ' ')"

# The C++ runtime is deliberately NOT bundled, and linuxdeploy's excludelist is
# right to drop it. The AppDir precedes the host on LD_LIBRARY_PATH, so a
# bundled libstdc++ shadows the host one; a C++ library that the host supplies
# afterwards then fails to resolve against the older bundled symbols. Measured
# on Ubuntu 24.04 (GCC 13): the host's libjack.so.0 needs GLIBCXX_3.4.32, the
# build container's libstdc++ (GCC 12) stops at GLIBCXX_3.4.30, and the app does
# not start at all.
#
# Bundling is only needed when the host's runtime can be OLDER than the build
# one, which is a consequence of the glibc floor: at 2.35 every supported
# distribution ships GCC >= 12, i.e. GLIBCXX_3.4.30, which is exactly what this
# app and Qt 6.8 require. Lowering the floor to 2.31 (RHEL 9, GCC 11) is the
# only reason to copy these two files back, and doing so reintroduces the
# newer-host failure above.

# appimagetool is run separately on purpose: linuxdeploy's --output appimage
# also runs its own appimagetool copy, and keeping the two steps apart makes
# failures easier to attribute.
log "Packing the AppImage"
ARCH="$ARCH" VERSION="$VERSION" appimagetool "$APPDIR" "$ARTIFACT"

# The container runs as root; hand the artifact back to the host user.
log "Restoring ownership of $DIST"
chown -R "${HOST_UID:?HOST_UID must be set to the host user id}:${HOST_GID:?HOST_GID must be set to the host group id}" "$DIST"

log "Done"
printf 'artifact: %s\n' "$ARTIFACT"
du -h "$ARTIFACT"
