# Maintainer: Altisidora Team
pkgname=altisidora
pkgver=0.1.0
pkgrel=1
pkgdesc="Native Linux music player built with C++20 and Qt 6"
arch=('x86_64')
url="https://github.com/MacTonight69/Altisidora"
license=('GPL3')
depends=(
    'qt6-base'
    'qt6-declarative'
    'mpv'
    'taglib'
    'hicolor-icon-theme'
)
makedepends=(
    'cmake'
    'pkgconf'
)

# TODO: Before publishing, push tag v0.1.0 upstream (the remote has no tags
# yet; v1.0 exists as a local tag only)
# and replace SKIP with real checksums (updatemodules / makepkg -g).
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

prepare() {
    cd "$srcdir"
    # GitHub archives extract as <Repo>-<tag>; normalize to the name the
    # build() and package() steps expect.
    mv -T "Altisidora-$pkgver" "$pkgname-$pkgver"
}

build() {
    cd "$srcdir/$pkgname-$pkgver"
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j$(nproc)
}

package() {
    cd "$srcdir/$pkgname-$pkgver"
    # Installs binary (`altisidora`), desktop entry and icon (CMake install rules).
    DESTDIR="$pkgdir" cmake --install build
}
