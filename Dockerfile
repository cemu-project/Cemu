# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS cemu-extend-base

ARG DEBIAN_FRONTEND=noninteractive

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    VCPKG_FORCE_SYSTEM_BINARIES=1 \
    VCPKG_DEFAULT_BINARY_CACHE=/root/.cache/vcpkg/archives

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        curl \
        file \
        freeglut3-dev \
        git \
        libasound2-dev \
        libbluetooth-dev \
        libclang-rt-dev \
        libdbus-1-dev \
        libdrm-dev \
        libegl1-mesa-dev \
        libgcrypt20-dev \
        libgbm-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libgtk-3-dev \
        libpulse-dev \
        libsecret-1-dev \
        libssl-dev \
        libsystemd-dev \
        libtool \
        libudev-dev \
        libusb-1.0-0-dev \
        libwayland-dev \
        libx11-dev \
        libx11-xcb-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxinerama-dev \
        libxkbcommon-dev \
        libxrandr-dev \
        libxrender-dev \
        libxtst-dev \
        nasm \
        ninja-build \
        pkg-config \
        python3 \
        unzip \
        wayland-protocols \
        zip \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p "$VCPKG_DEFAULT_BINARY_CACHE"

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
        autoconf \
        autoconf-archive \
        automake \
        libglm-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/CemuExtend
COPY . .

# CemuExtend keeps most third-party sources in git submodules.  Initializing
# them here also makes a checkout with empty submodule directories usable as a
# Docker build context.
RUN test -e .git \
    && git submodule update --init --recursive \
    && bash ./dependencies/vcpkg/bootstrap-vcpkg.sh -disableMetrics

FROM cemu-extend-base AS dev

CMD ["bash"]

FROM dev AS build

ARG BUILD_TYPE=Release

RUN --mount=type=cache,target=/root/.cache/vcpkg/archives \
    cmake -S . -B build/docker \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_VCPKG=ON \
        -DALLOW_PORTABLE=OFF \
        -DVCPKG_INSTALL_OPTIONS=--clean-after-build \
    && cmake --build build/docker --parallel

CMD ["bash"]
