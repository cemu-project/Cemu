# Build instructions

## Windows

Prerequisites:
- A recent version of Visual Studio 2022 (recommended but not required) with the following additional components:
  - C++ CMake tools for Windows
  - Windows 10/11 SDK
- git

Instructions:

1. Run `git clone --recursive https://github.com/cemu-project/Cemu`
2. Launch `Cemu/generate_vs_solution.bat`.
    - If you installed VS to a custom location or use VS 2019, you may need to manually change the path inside the .bat file.
3. Wait until it's done, then open `Cemu/build/Cemu.sln` in Visual Studio.
4. Then build the solution and once finished you can run and debug it, or build it and check the /bin folder for the final Cemu_release.exe.

You can also skip steps 3-5 and open the root folder of the cloned repo directly in Visual Studio (as a folder) and use the built-in CMake support but be warned that cmake support in VS can be a bit finicky.

## Linux

To compile Cemu, a recent enough compiler and STL with C++20 support is required! clang-12 or higher is what we recommend.

### Installing dependencies

#### For Ubuntu and derivatives:
`sudo apt install -y git curl cmake ninja-build nasm libgtk-3-dev libsecret-1-dev libgcrypt20-dev libsystemd-dev freeglut3-dev libpulse-dev` 

*Additionally, for Ubuntu 22.04 only:*
 - `sudo apt install -y clang-12`
 - At step 3 while building, use  
   `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang-12 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-12 -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja`

#### For Arch and derivatives:
`sudo pacman -S --needed git cmake llvm clang ninja nasm base-devel linux-headers gtk3 libsecret libgcrypt systemd freeglut zip unzip libpulse`

#### For Fedora and derivatives:
`sudo dnf install git cmake clang ninja-build nasm kernel-headers gtk3-devel libsecret-devel libgcrypt-devel systemd-devel freeglut-devel perl-core zlib-devel cubeb-devel`

### Build Cemu using cmake and clang
1. `git clone --recursive https://github.com/cemu-project/Cemu`
2. `cd Cemu`
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -G Ninja`
4. `cmake --build build`
5. You should now have a Cemu executable file in the /bin folder, which you can run using `./bin/Cemu_release`.

#### Using GCC
While we use and test Cemu using clang, using GCC might work better with your distro (they should be fairly similar performance/issues wise and should only be considered if compilation is the issue).  
You can use it by replacing the step 3 with the following:
`cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ -G Ninja`

#### Troubleshooting steps
 - If step 3 gives you an error about not being able to find ninja, try appending `-DCMAKE_MAKE_PROGRAM=/usr/bin/ninja` to the command and running it again.
 - If step 3 fails while compiling the boost-build dependency, it means you don't have a working/good standard library installation. Check the integrity of your system headers and making sure that C++ related packages are installed and intact.
 - If step 3 gives a random error, read the `[package-name-and-platform]-out.log` and `[package-name-and-platform]-err.log` for the actual reason to see if you might be lacking the headers from a dependency.
 - If step 3 is still failing or if you're not able to find the cause, please make an issue on our Github about it!
 - If step 4 gives you an error that contains something like `main.cpp.o: in function 'std::__cxx11::basic_string...`, you likely are experiencing a clang-14 issue. This can only be fixed by either lowering the clang version or using GCC, see below.
 - If step 4 gives you a different error, you could report it to this repo or try using GCC. Just make sure your standard library and compilers are updated since Cemu uses a lot of modern features!
- If step 4 gives you undefined libdecor_xx, you are likely experiencing an issue with sdl2 package that comes with vcpkg. Delete sdl2 from vcpkg.json in source file and recompile.

## MacOS

To compile Cemu, a recent enough compiler and STL with C++20 support is required! LLVM 13 and 
below, built in LLVM, and Xcode LLVM don't support the C++20 feature set required. Currently, 
LLVM 15 isn't supported due to compatibility issues with Boost dependency. The OpenGL graphics
API isn't support on MacOS, Vulkan must be used. Additionally Vulkan must be used through the 
Molten-VK compatibility layer.  

### Installing brew
`/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`

### Installing dependencies
`brew install git cmake llvm@14 ninja nasm molten-vk`

### Build Cemu using cmake and clang
1. `git clone --recursive https://github.com/cemu-project/Cemu`
2. `cd Cemu`
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=release
   -DCMAKE_C_COMPILER=/usr/local/opt/llvm@14/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm@14/bin/clang++ -G Ninja`
4. `cmake --build build`
5. You should now have a Cemu executable file in the /bin folder, which you can run using `./bin/Cemu_release`.

#### Troubleshooting steps
- If step 3 gives you an error about not being able to find ninja, try appending `-DCMAKE_MAKE_PROGRAM=/usr/local/bin/ninja` to the command and running it again.

## Updating Cemu and source code
1. To update your Cemu local repository, use the command `git pull --recurse-submodules` (run this command on the Cemu root).
    - This should update your local copy of Cemu and all of its dependencies.
2. Then, you can rebuild Cemu using the steps listed above, according to whether you use Linux or Windows.

If CMake complains about Cemu already being compiled or another similar error, try deleting the `CMakeCache.txt` file inside the `build` folder and retry building.
