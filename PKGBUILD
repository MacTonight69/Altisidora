# Maintainer: Altisidora Team
pkgname=altisidora
pkgver=0.1.0
pkgrel=1
pkgdesc="Native Linux music player built with C++20 and Qt 6"
arch=('x86_64')
url="https://github.com/altisidora/altisidora"
license=('GPL3')
depends=(
    'qt6-base'
    'qt6-declarative'
    'mpv'
    'taglib'
)
makedepends=(
    'cmake'
    'gcc'
    'pkgconf'
)

# TODO: Finalize the upstream URL and checksums before publishing.
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/$pkgname"
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build -j$(nproc)
}

package() {
    cd "$srcdir/$pkgname"
    # Installs binary (`altisidora`), desktop entry and icon (CMake install rules).
    DESTDIR="$pkgdir" cmake --install build
}
