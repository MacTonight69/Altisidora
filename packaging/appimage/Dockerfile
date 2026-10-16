# Altisidora AppImage build environment.
#
# This image builds the *environment only*. The recipe that configures, builds
# and packages the application lives in build-in-container.sh and is run
# separately, with the repository bind-mounted at /work.
#
# Base image: Ubuntu 22.04, glibc 2.35. Deliberate choice -- see
# docs/PACKAGING_LINUX.md (compatibility floor). The project needs C++20, which
# the native g++-12 provides, and Qt 6.8 (official binaries) because the QML
# uses FolderDialog, added in Qt 6.3.
#
# FUSE is unavailable inside the container, so the three AppImage packaging
# tools are downloaded and pre-extracted here and exposed through thin wrapper
# scripts on PATH.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

ARG QT_VERSION=6.8.3
ARG TAGLIB_VERSION=2.0.2
ENV QT_ROOT=/opt/Qt
ENV TAGLIB_ROOT=/opt/taglib

# Toolchain and build tooling, the X11/XCB + xkb runtime the Qt xcb platform
# plugin links against, the runtime libraries the packaged app needs, and the
# packaging helpers (file, patchelf, desktop-file-utils). zlib1g-dev and
# libutfcpp-dev are TagLib build dependencies -- the TagLib 2.0.2 release
# tarball does not vendor utfcpp, and its CMake aborts with "utfcpp not found"
# without the headers. squashfs-tools is the fallback mksquashfs used by
# appimagetool.
RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc-12 \
      g++-12 \
      cmake \
      ninja-build \
      pkg-config \
      make \
      wget \
      curl \
      ca-certificates \
      xz-utils \
      file \
      patchelf \
      desktop-file-utils \
      zlib1g-dev \
      libutfcpp-dev \
      squashfs-tools \
      python3 \
      python3-pip \
      libmpv-dev \
      libgl1-mesa-dev \
      libegl1-mesa-dev \
      libfontconfig1-dev \
      libfreetype6-dev \
      libglib2.0-0 \
      libdbus-1-3 \
      libxkbcommon-x11-0 \
      libxcb-cursor0 \
      libxcb-icccm4 \
      libxcb-image0 \
      libxcb-keysyms1 \
      libxcb-randr0 \
      libxcb-render-util0 \
      libxcb-shape0 \
      libxcb-xinerama0 \
      libxcb-xkb1 \
      libxcb1 \
      libxcb-util1 \
      libxcb-xfixes0 \
      libxcb-sync1 \
      libxcb-shm0 \
      libxcb-glx0 \
      libx11-6 \
      libx11-xcb1 \
      libxext6 \
      libxrender1 \
      libsm6 \
      libice6 \
      libxi6 \
    && rm -rf /var/lib/apt/lists/*

# Qt 6.8 official Linux binaries. The distro Qt (6.2.4) is too old for the QML
# in resources/qml.
RUN python3 -m pip install --no-cache-dir aqtinstall
# Only qtimageformats is an installable module here. `aqt list-qt linux desktop
# --modules 6.8.3 linux_gcc_64` does not list qtsvg: SVG support is already
# inside the base qt6 archive, and asking for it makes aqt abort with
# "The packages ['qtsvg'] were not found".
RUN aqt install-qt linux desktop "${QT_VERSION}" linux_gcc_64 \
      -O "${QT_ROOT}" \
      -m qtimageformats
# Prove what actually shipped: the platform plugins decide whether the AppImage
# opens at all. Expect libqxcb.so and, on this archive, the Wayland plugins.
RUN ls -1 "${QT_ROOT}/${QT_VERSION}/gcc_64/plugins/platforms/"
# The image formats come from the base archive plus qtimageformats; the JPEG,
# PNG and SVG plugins are what the covers and icons need.
RUN ls -1 "${QT_ROOT}/${QT_VERSION}/gcc_64/plugins/imageformats/"
# Note: the archive also ships MySQL, ODBC, PostgreSQL, Mimer, CUPS and GTK3
# plugins whose client libraries are not part of this image. They cannot be
# removed here -- Qt's CMake package configs validate that every plugin file
# exists, so find_package(Qt6) fails -- so the recipe parks them around the
# linuxdeploy call instead. See build-in-container.sh.

# TagLib 2.x built from source: src/library/MetadataReader.cpp uses
# FileRef::complexProperties("PICTURE"), a TagLib 2.x API. No supported base
# image ships TagLib 2.x.
RUN set -eux; \
    wget -q -O /tmp/taglib.tar.gz \
      "https://github.com/taglib/taglib/releases/download/v${TAGLIB_VERSION}/taglib-${TAGLIB_VERSION}.tar.gz"; \
    tar -xzf /tmp/taglib.tar.gz -C /tmp; \
    cmake -S "/tmp/taglib-${TAGLIB_VERSION}" -B /tmp/taglib-build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DBUILD_TESTING=OFF \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_BINDINGS=OFF \
      -DCMAKE_C_COMPILER=gcc-12 \
      -DCMAKE_CXX_COMPILER=g++-12 \
      -DCMAKE_INSTALL_PREFIX="${TAGLIB_ROOT}"; \
    cmake --build /tmp/taglib-build --parallel "$(nproc)"; \
    cmake --install /tmp/taglib-build; \
    rm -rf /tmp/taglib-build "/tmp/taglib-${TAGLIB_VERSION}" /tmp/taglib.tar.gz

# Packaging tools, pre-extracted: --appimage-extract needs no FUSE, but running
# a mounted AppImage does. Each extraction produces squashfs-root/, renamed to
# <tool>.AppDir. The wrappers exist so that linuxdeploy can find its Qt plugin
# by the bare name `linuxdeploy-plugin-qt` on PATH.
RUN set -eux; \
    mkdir -p /opt/linuxdeploy; \
    cd /opt/linuxdeploy; \
    wget -q -O linuxdeploy.tool \
      "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"; \
    chmod +x linuxdeploy.tool; \
    ./linuxdeploy.tool --appimage-extract >/dev/null; \
    mv squashfs-root linuxdeploy.AppDir; \
    rm -f linuxdeploy.tool; \
    wget -q -O plugin-qt.tool \
      "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"; \
    chmod +x plugin-qt.tool; \
    ./plugin-qt.tool --appimage-extract >/dev/null; \
    mv squashfs-root linuxdeploy-plugin-qt.AppDir; \
    rm -f plugin-qt.tool; \
    wget -q -O appimagetool.tool \
      "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"; \
    chmod +x appimagetool.tool; \
    ./appimagetool.tool --appimage-extract >/dev/null; \
    mv squashfs-root appimagetool.AppDir; \
    rm -f appimagetool.tool

RUN set -eux; \
    for tool in linuxdeploy linuxdeploy-plugin-qt appimagetool; do \
      printf '#!/bin/sh\nexec /opt/linuxdeploy/%s.AppDir/AppRun "$@"\n' "$tool" \
        > "/usr/local/bin/$tool"; \
      chmod +x "/usr/local/bin/$tool"; \
    done; \
    command -v linuxdeploy linuxdeploy-plugin-qt appimagetool; \
    appimagetool --version
