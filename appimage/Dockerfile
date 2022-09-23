FROM centos:centos7

# Dependencies
ENV INST yum install -y --setopt=skip_missing_names_on_install=False
RUN $INST https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm centos-release-scl
RUN $INST autoconf devtoolset-11 ninja-build kernel-headers perl-core python36 python36-pip\
    libsecret-devel libgcrypt-devel systemd-devel freeglut-devel zlib-devel\
    libmount-devel bzip2-devel libselinux-devel libffi-devel gtk3-devel\
    pulseaudio-libs-devel libcurl-devel

# Environment Configuration
ENV CC=/opt/rh/devtoolset-11/root/usr/bin/gcc \
    CPP=/opt/rh/devtoolset-11/root/usr/bin/cpp \
    CXX=/opt/rh/devtoolset-11/root/usr/bin/g++ \
    PATH=/opt/rh/devtoolset-11/root/usr/bin:$PATH \
    LD_LIBRARY_PATH=/opt/rh/devtoolset-11/root/usr/lib64:/opt/rh/devtoolset-11/root/usr/lib:/opt/rh/devtoolset-11/root/usr/lib64/dyninst:/opt/rh/devtoolset-11/root/usr/lib/dyninst:/opt/rh/devtoolset-11/root/usr/lib64:/opt/rh/devtoolset-11/root/usr/lib:$LD_LIBRARY_PATH

## Git
WORKDIR /git
RUN yum remove -y git
RUN curl -L https://github.com/git/git/tarball/e9d7761bb94f20acc98824275e317fa82436c25d |\
    tar -xzC . --strip-components=1 &&\
    make configure &&\
    ./configure --prefix=/usr &&\
    make -j$(nproc)&&\
    make install

## Python
RUN python3 -m pip install --upgrade pip wheel
RUN python3 -m pip install cmake

## Cubeb
RUN git clone https://github.com/mozilla/cubeb /cubeb &&\
    cd /cubeb &&\
    git checkout 4783607ecc09e9493677a9c4e3db95f78d87409a &&\
    git submodule update --init --recursive &&\
    cmake -B ./build . &&\
    cmake --build ./build &&\
    cd ./build &&\
    make install

## nasm
WORKDIR /nasm
RUN curl -L https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz |\
    tar -xzC . --strip-components=1 &&\
    ./configure --prefix=/usr &&\
    make -j$(nproc)&&\
    make install

## Cemu
WORKDIR /cemu
COPY . .
RUN cmake -S . -B build\
    -DCMAKE_BUILD_TYPE=release\
    -DCMAKE_C_COMPILER=${CC}\
    -DCMAKE_CXX_COMPILER=${CXX}\
    -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja -G Ninja &&\
    cmake --build build &&\
    rm -rf ./dependencies/vcpkg/buildtrees

RUN python3 ./appimage/copy_dylibs.py ./bin/Cemu_release