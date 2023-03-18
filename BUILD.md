# Build instructions

## Table of Contents

* [Windows](#windows)
* [Linux](#linux)
* [Mac](#macos)

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
`sudo apt install -y cmake curl freeglut3-dev git libgcrypt20-dev libgtk-3-dev libpulse-dev libsecret-1-dev libsystemd-dev nasm ninja-build` 

*Additionally, for Ubuntu 22.04 only:*
 - `sudo apt install -y clang-12`
 - At step 3 while building, use  
   `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang-12 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-12 -G Ninja -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja`

#### For Arch and derivatives:
`sudo pacman -S --needed base-devel clang cmake freeglut git gtk3 libgcrypt libpulse libsecret linux-headers llvm nasm ninja systemd unzip zip`

#### For Fedora and derivatives:
`sudo dnf install clang cmake cubeb-devel freeglut-devel git gtk3-devel kernel-headers libgcrypt-devel libsecret-devel nasm ninja-build perl-core systemd-devel zlib-devel`

### Build Cemu using cmake and clang
1. `git clone --recursive https://github.com/cemu-project/Cemu`
2. `cd Cemu`
3. `cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -G Ninja`
4. `cmake --build build`
5. You should now have a Cemu executable file in the /bin folder, which you can run using `./bin/Cemu_release`.

#### Using GCC
While we use and test Cemu using clang, using GCC might work better with your distro (they should be fairly similar performance/issues wise and should only be considered if compilation is the issue).  
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

The rest of this section will walk you through the process of setting up and building Cemu on a Mac, whether it's an Intel or Apple Silicon machine.

### First time setup before compiling Cemu for Apple Silicon Macs

1. Install Rosetta 2.

   ```
   softwareupdate --install-rosetta
   ```

1. Run an x86_64 shell.

   ```
   arch -x86_64 zsh
   ```

1. Unload your arm64-specific brew from your `PATH` so that it doesn't confuse your x86_64-specific installation of brew.

   ```
   export PATH=`printf '%s:' $(echo $PATH | tr ':' '\n' | grep -iv "^\/opt\/homebrew\/")`
   ```

   * **Note:** This step is only necessary if you have an arm64-specific brew installed. To check, do `echo $PATH | tr ':' '\n'`, and check if there's any mention of `/opt/homebrew`.
1. Now continue onto the `First time setup for Intel Macs` section. Note that you should definitely do the `Install brew` step **even if** you have an arm64-specific brew already installed. This will install a second one, specific to x86_64.

### First time setup before compiling Cemu for either kind of Mac

1. Install brew.

   ```
   curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh | /bin/bash
   ```

1. Initialize brew.

   ```
   eval "$(/usr/local/bin/brew shellenv)"
   ```

1. Install dependencies.

   ```
   brew install boost git cmake llvm nasm ninja pkg-config molten-vk
   ```

1. Clone the Cemu repository with the `--recursive` flag to also clone the dependencies that it submodules.

   ```
   git clone --recursive https://github.com/cemu-project/Cemu
   ```

1. Change into the cloned repository directory.

   ```
   cd Cemu
   ```

1. Run cmake to generate the build files.

   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ -G Ninja
   ```

### Every time setup before compiling Cemu for either kind of Mac

Some of the following steps should be done every time you wish to compile Cemu, others should be done on an as-needed basis. The latter are marked with a :soap: emoji.

1. If you have an Apple Silicon Mac, run an x86_64 shell.

   ```
   arch -x86_64 zsh
   ```

1. If you have an Apple Silicon Mac, and you have an arm64-specific brew installed, unload your arm64-specific brew from your `PATH` so that it doesn't confuse your x86_64-specific installation of brew.

   ```
   export PATH=`printf '%s:' $(echo $PATH | tr ':' '\n' | grep -iv "^\/opt\/homebrew\/")`
   ```

1. Initialize brew.

   ```
   eval "$(/usr/local/bin/brew shellenv)"
   ```

1. :soap: Update dependencies.

   ```
   brew update && brew upgrade
   ```

   * **Note:** This step is optional and only needs to be done if there are updates to the dependencies installed via brew.
1. :soap: Update the Cemu repository and its submodules.

   ```
   git pull --recurse-submodules
   ```

   * **Note:** This step is optional and only needs to be done if there are updates to the Cemu repository or its submodules.

1. :soap: If during the previous step you see that you pulled in changes to `CMakeLists.txt`, rerun cmake to regenerate the build files.

   ```
   cmake -S . -B build -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/local/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ -G Ninja
   ```

### Compile Cemu using cmake and clang for either kind of Mac

1. Run cmake to build Cemu using clang.

   ```
   cmake --build build
   ```

1. You should now have a Cemu executable file in the /bin folder, which you can run using:

   ```
   ./bin/Cemu_release
   ```

## Updating Cemu and source code
1. To update your Cemu local repository, use the command `git pull --recurse-submodules` (run this command on the Cemu root).
    - This should update your local copy of Cemu and all of its dependencies.
2. Then, you can rebuild Cemu using the steps listed above, according to whether you use Linux or Windows.

If CMake complains about Cemu already being compiled or another similar error, try deleting the `CMakeCache.txt` file inside the `build` folder and retry building.
