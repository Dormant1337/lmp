

pkgname='lmplayer'
pkgver='1.0'
pkgrel=1
pkgdesc="A lightweight, command-line music player for Linux."
arch=('x86_64')
url="https://github.com/Zer0Flux86/lmp"
license=('GPL3') 

depends=('ncurses' 'sdl2' 'sdl2_mixer' 'mpg123')
makedepends=('gcc' 'make')
optdepends=(
    'spotdl: for downloading tracks via the webdownload command'
    'yt-dlp: required by spotdl'
    'ffmpeg: required by spotdl'
)

source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")

sha256sums=('SKIP')

build() {
  cd "lmp-$pkgver"
  make
}

package() {
  cd "lmp-$pkgver"
  make PREFIX=/usr DESTDIR="$pkgdir/" install
}