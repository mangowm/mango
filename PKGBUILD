# Maintainer: Another & Antigravity
pkgname=mangowm-hdr-git
pkgver=0.16.2.r1
pkgrel=1
pkgdesc="MangoWM with Valve frog-color-management-v1 & EDID HDR Mastering Engine"
url="https://github.com/another-hubgit/mango"
arch=("x86_64")
license=("GPL-3.0")
depends=(
  glibc
  'wayland>=1.23.1'
  'libinput>=1.27.1'
  libdrm
  libdisplay-info
  pixman
  libxkbcommon
  pcre2
  pango
  cjson
  libxcb
  xorg-xwayland
  'libwlroots-0.20.so'
)

makedepends=(
  meson
  ninja
  'wayland-protocols>=1.41'
)

provides=(mangowm wayland-compositor mangowm-wlonly)
conflicts=(mangowm mangowm-git mangowm-wlonly)

build() {
  arch-meson "$startdir" build
  ninja -C build
}

package() {
  DESTDIR="$pkgdir/" ninja -C build install
}
