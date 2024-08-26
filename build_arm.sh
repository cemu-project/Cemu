#! /bin/env bash
# rm -rf build_arm
set -e
cmake -S . -B build_arm -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DENABLE_VCPKG=OFF \
    -DCMAKE_TOOLCHAIN_FILE=./cmake/debian-arm64toolchain.cmake \
    -DENABLE_WAYLAND=OFF \
    -DENABLE_HIDAPI=OFF \
    -DENABLE_NSYSHID_LIBUSB=OFF \
    -G Ninja
cmake --build build_arm