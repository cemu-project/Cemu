# Maintainer: Exzap <13877693+Exzap@users.noreply.github.com>

# This needs to be changed to a new name since Cemu is already taken on the AUR.
pkgname=cemu
pkgver=2.0.0
pkgrel=2
pkgdesc='Software to emulate Wii U games and applications on PC'
arch=('x86_64')
url='https://cemu.info/'
license=('mpl-2.0')
depends=('gtk' 'vulkan-icd-loader')
makedepends=('cmake' 'extra-cmake-modules' 'git' 'ninja' 'libsecret' 'libgcrypt' 'systemd-libs' 'freeglut' 'clang')
source=("https://github.com/cemu-project/Cemu/releases/download/$pkgver/Cemu-$pkgver.tar.gz")

build() {
    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE= \
        -DCMAKE_C_COMPILER=/usr/bin/clang-12 \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++-12 \
        -G \
        Ninja \
        -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja
    ninja
}

package() {
    cmake --install .
}