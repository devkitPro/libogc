# PKGBUILD for development work
# run (dkp-)makepkg from this folder to build a package
# use sudo (dkp-)pacman -U /path/to/libogc-<version>-any.pkg.tar.zst to install
# installing directly can and will cause issues, please don't
pkgname=libogc
# this value will change as things get committed 
pkgver=3.0.1.1.g4a950afc
pkgver() {
  git describe --tags | sed 's/^v//;s/-/./g'
}

pkgrel=1
arch=('any')

options=(!strip libtool staticlibs)

build() {
  cd ${startdir}
  catnip
}

package() {
  cd ${startdir}
  DESTDIR="${pkgdir}" catnip install
}
