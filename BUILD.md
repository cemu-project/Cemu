# Build instructions

## Windows

Prerequisites:
- A recent version of Visual Studio 2022 with CMake tools component
- git

Instructions:

1) Run `git clone --recursive https://github.com/cemu-project/Cemu`
2) Launch `Cemu/generate_vs_solution.bat`. If you installed VS to a custom location you may need to manually adjust the path inside the bat file
3) Wait until done, then open `Cemu/build/Cemu.sln` in Visual Studio
4) Right click 'CemuBin' project -> Set as startup project
5) Then build the solution and once finished you can run and debug it

You can also skip steps 3-5 and open the root folder of the cloned repo directly in Visual Studio (as a folder) and use the inbuilt cmake support, but be warned that cmake support in VS can be a bit finicky.

## Linux

To compile Cemu, a recent enough compiler and STL with C++20 support is required! We use clang 12, other compilers may work as well.

For ubuntu and most derivatives:


`sudo apt install -y libgtk-3-dev libsecret-1-dev libgcrypt20-dev libsystemd-dev freeglut3-dev clang-12 nasm ninja-build`

For Arch and most derivatives:

`sudo pacman -S cmake git base-devel ninja nasm linux-headers zip`

Added zip incase the user runs KDE but with gnome zip is included.

vcpkg is not needed to be installed and provided by cemu when git cloning.

For Fedora and most derivatives:

`sudo dnf in cmake ninja-build vcpkg llvm kernel-headers gtk3-devel libsecret-devel libgcrypt-devel systemd-devel freeglut-devel nasm perl-core zlib-devel cubeb-devel`

1) Run `git clone --recursive https://github.com/cemu-project/Cemu`
2) `cd Cemu`
3) `mkdir build && cd build`
4) `cmake .. -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -G Ninja`
4.5) To build for gcc `cmake .. -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja`
6) `ninja`
