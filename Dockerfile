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

FROM cemu-extend-base AS dev

WORKDIR /workspace/CemuExtend
COPY . .
RUN test -f dependencies/vcpkg/bootstrap-vcpkg.sh \
    && bash ./dependencies/vcpkg/bootstrap-vcpkg.sh -disableMetrics

CMD ["bash"]

FROM cemu-extend-base AS build

ARG BUILD_TYPE=Release
ARG GIT_HASH=unknown
ARG CEMU_EXTEND_COMMIT_HASH=unknown

WORKDIR /workspace/CemuExtend

# The source is a bind mount, so source changes do not create a multi-gigabyte
# image layer. The writable mount is discarded after the artifact is copied
# out, while CMake/Ninja state is retained in the cache mount.
RUN --mount=type=bind,source=.,target=/workspace/CemuExtend,rw \
    --mount=type=cache,id=cemu-extend-vcpkg,target=/root/.cache/vcpkg/archives,sharing=locked \
    --mount=type=cache,id=cemu-extend-cmake,target=/workspace/CemuExtend/build/docker,sharing=locked \
    bash ./dependencies/vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && cmake -S . -B build/docker \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DGIT_HASH=${GIT_HASH} \
        -DCEMU_EXTEND_COMMIT_HASH=${CEMU_EXTEND_COMMIT_HASH} \
        -DENABLE_VCPKG=ON \
        -DALLOW_PORTABLE=OFF \
        -DVCPKG_INSTALL_OPTIONS=--clean-after-build \
    && cmake --build build/docker --parallel \
    && ctest --test-dir build/docker --output-on-failure \
    && cp bin/Cemu_release /Cemu_release

CMD ["bash"]
