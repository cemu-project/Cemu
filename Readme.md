# **Cemu - A Wii U emulator**

[![Build Process](https://github.com/cemu-project/Cemu/actions/workflows/build.yml/badge.svg)](https://github.com/cemu-project/Cemu/actions/workflows/build.yml)

[Website](https://cemu.info) | [Wiki](https://wiki.cemu.info/wiki/Main_Page) | [Subreddit](https://reddit.com/r/Cemu) | [Discord](https://discord.gg/5psYsup) | [Unofficial Setup Guide](https://cemu.cfw.guide) | [Translation](https://github.com/cemu-project/Cemu-Language) | [Community Graphic Packs](https://github.com/ActualMandM/cemu_graphic_packs)

Cemu is a Wii U emulator written in C++ under active development.

## System Requirements

* OS
    * Windows (7 or higher).
    * Linux.
* Architecture
    * x86_64.
* Graphics
    * Something that supports Vulkan 1.1 or OpenGL 4.5.
    * NVIDIA GPUs work best.
    * AMD GPUs struggle with OpenGL.
    * Intel GPUs have limited support, use Vulkan.

## Download

You can download the latest Cemu releases from the [Github Releases](https://github.com/cemu-project/Cemu/releases) or from [Cemu's website](http://cemu.info).

See [Current State Of Linux builds](https://github.com/cemu-project/Cemu/issues/1) for information on using Cemu natively on Linux.

Pre-2.0 releases can be found on Cemu's [changelog page](http://cemu.info/changelog.html).

## Building

The [Vulkan SDK](https://vulkan.lunarg.com) is currently required for all platforms.
Make sure to pull submodules before building:
```
git submodule update --init --recursive
```
### Building for Windows

Visual Studio 2022 with C++ CMake tools for Windows is required.

The Public Release configuration is best for a single build.
The Release configuration is recommended for regular development.
The Debug configuration is much slower, and should be used only for debugging.

### Building for Linux

A compiler and STL with good C++20 support is required. Make sure you have either cloned recursively or pulled submodules, and are located in-tree.

1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make -j $(nproc)`

#### Linux Dependencies
While Cemu currently uses vcpkg on all platforms, some dependencies must be installed via the system package manager.

For apt-based systems:
```
sudo apt install libgtk-3-dev libsecret-1-dev libgcrypt20-dev libsystemd-dev freeglut3-dev nasm
```

## Issues

Issues with the emulator should be filed using [Github Issues](https://github.com/cemu-project/Cemu/issues).
The old bug tracker can be found at [bugs.cemu.info](http://bugs.cemu.info) and still contains relevant issues and feature suggestions.

## Contributing

Pull requests are very welcome. For easier coordination you can visit the developer discussion channel on Discord.

If coding isn't your thing, testing games and making detailed bug reports or updating the (usually outdated) compatibility wiki is also appreciated!

Questions about Cemu's software architecture can also be answered on Discord. Alternative communication channels (like IRC) are being considered.

## License
Cemu is licensed under [Mozilla Public License 2.0](/LICENSE.txt). Exempt from this are all files in the dependencies directory for which the licenses of the original code apply as well as some individual files in the src folder, as specified in those file headers respectively.
