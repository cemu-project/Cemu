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

1) `sudo apt install -y libgtk-3-dev libsecret-1-dev libgcrypt20-dev libsystemd-dev freeglut3-dev clang-12 nasm`
2) Run `git clone --recursive https://github.com/cemu-project/Cemu`
3) `cd Cemu`
4) `mkdir build && cd build`
5) `cmake .. -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang-12 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-12 -G Ninja
6) `ninja`

Build instructions for other distributions will be added in the future!
