# Build instructions

## Windows

Prerequisites:
- git
- A recent version of Visual Studio 2022 with the following additional components:
   - C++ CMake tools for Windows
   - Windows 10/11 SDK

Instructions for Visual Studio 2022:

1. Run `git clone --recursive https://github.com/cemu-project/Cemu`
2. Open the newly created Cemu directory in Visual Studio using the "Open a local folder" option
3. In the menu select Project -> Configure CMake. Wait until it is done, this may take a long time
4. You can now build, run and debug Cemu

Any other IDE should also work as long as it has CMake and MSVC support. CLion and Visual Studio Code have been confirmed to work.

## Linux

To compile Cemu, a recent enough compiler and STL with C++20 support is required! clang-15 or higher is what we recommend.

### Installing dependencies

#### For Ubuntu and derivatives:
`sudo apt install -y cmake curl clang-15 freeglut3-dev git libgcrypt20-dev libglm-dev libgtk-3-dev libpulse-dev libsecret-1-dev libsystemd-dev nasm ninja-build`

You may also need to install `libusb-1.0-0-dev` as a workaround for an issue with the vcpkg hidapi package.

At step 3 while building, use:
   `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang-15 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-15 -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja`

#### For Arch and derivatives:
`sudo pacman -S --needed base-devel clang cmake freeglut git glm gtk3 libgcrypt libpulse libsecret linux-headers llvm nasm ninja systemd unzip zip`

#### For Fedora and derivatives:
`sudo dnf install clang cmake cubeb-devel freeglut-devel git glm-devel gtk3-devel kernel-headers libgcrypt-devel libsecret-devel libtool libusb1-devel nasm ninja-build perl-core systemd-devel zlib-devel`

### Build Cemu using cmake and clang
1. `git clone --recursive https://github.com/cemu-project/Cemu`
2. `cd Cemu`
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -G Ninja`
4. `cmake --build build`
5. You should now have a Cemu executable file in the /bin folder, which you can run using `./bin/Cemu_release`.

#### Using GCC
While we build and test Cemu using clang, using GCC might work better with your distro (they should be fairly similar performance/issues wise and should only be considered if compilation is the issue).

You can use GCC by doing the following:
- make sure you have g++ installed in your system
  - installation for Ubuntu and derivatives: `sudo apt install g++`
  - installation for Fedora and derivatives: `sudo dnf install gcc-c++`
- replace the step 3 with the following:
`cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ -G Ninja`

#### Troubleshooting steps
 - If step 3 gives you an error about not being able to find ninja, try appending `-DCMAKE_MAKE_PROGRAM=/usr/bin/ninja` to the command and running it again.
 - If step 3 fails while compiling the boost-build dependency, it means you don't have a working/good standard library installation. Check the integrity of your system headers and making sure that C++ related packages are installed and intact.
 - If step 3 gives a random error, read the `[package-name-and-platform]-out.log` and `[package-name-and-platform]-err.log` for the actual reason to see if you might be lacking the headers from a dependency.
 - If step 3 is still failing or if you're not able to find the cause, please make an issue on our Github about it!
 - If step 3 fails during rebuild after `git pull` with an error that mentions RPATH, add this to the end of step 3: `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON`
 - If step 4 gives you an error that contains something like `main.cpp.o: in function 'std::__cxx11::basic_string...`, you likely are experiencing a clang-14 issue. This can only be fixed by either lowering the clang version or using GCC, see below.
 - If step 4 gives you a different error, you could report it to this repo or try using GCC. Just make sure your standard library and compilers are updated since Cemu uses a lot of modern features!
 - If step 4 gives you undefined libdecor_xx, you are likely experiencing an issue with sdl2 package that comes with vcpkg. Delete sdl2 from vcpkg.json in source file and recompile.
 - If step 4 gives you `fatal error: 'span' file not found`, then you're either missing `libstdc++` or are using a version that's too old. Install at least v10 with your package manager, eg `sudo apt install libstdc++-10-dev`. See #644.

## macOS

To compile Cemu, a recent enough compiler and STL with C++20 support is required! LLVM 13 and 
below, built in LLVM, and Xcode LLVM don't support the C++20 feature set required. The OpenGL graphics
API isn't support on macOS, Vulkan must be used. Additionally Vulkan must be used through the 
Molten-VK compatibility layer

### On Apple Silicon Macs, Rosetta 2 and the x86_64 version of Homebrew must be used

You can skip this section if you have an Intel Mac. Every time you compile, you need to perform steps 2.

1. `softwareupdate --install-rosetta` # Install Rosetta 2 if you don't have it. This only has to be done once
2. `arch -x86_64 zsh` # run an x64 shell

### Installing brew

1. `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
2. `eval "$(/usr/local/Homebrew/bin/brew shellenv)"` # set x86_64 brew env

### Installing dependencies

`brew install boost git cmake llvm ninja nasm molten-vk automake libtool`

### Build Cemu using cmake and clang
1. `git clone --recursive https://github.com/cemu-project/Cemu`
2. `cd Cemu`
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ -G Ninja`
4. `cmake --build build`
5. You should now have a Cemu executable file in the /bin folder, which you can run using `./bin/Cemu_release`.

#### Troubleshooting steps
- If step 3 gives you an error about not being able to find ninja, try appending `-DCMAKE_MAKE_PROGRAM=/usr/local/bin/ninja` to the command and running it again.

## Updating Cemu and source code
1. To update your Cemu local repository, use the command `git pull --recurse-submodules` (run this command on the Cemu root).
    - This should update your local copy of Cemu and all of its dependencies.
2. Then, you can rebuild Cemu using the steps listed above, according to whether you use Linux or Windows.

If CMake complains about Cemu already being compiled or another similar error, try deleting the `CMakeCache.txt` file inside the `build` folder and retry building.
