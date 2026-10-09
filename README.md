# Cemu with Wii U GamePad support

Play Wii U games in Cemu with a **real Wii U GamePad**: the GamePad screen on the GamePad, plus its buttons, sticks,
touchscreen and motion. **No Wii U console needed.**

This is a fork of [Cemu](https://github.com/cemu-project/Cemu), shared with the Cemu team's permission. Everything
else works exactly like normal Cemu (the original README is further down).

## What you need

- **A Linux PC.** Windows and macOS can't host the GamePad's Wi-Fi network (see [Why Linux only](#why-linux-only)).
- **A Wii U GamePad**, charged. Any GamePad works; it doesn't need to have been paired with a console.
- **A USB Wi-Fi adapter with an RT5572 chip** (driver `rt2800usb`). The PC uses it to host the GamePad's network,
  the same way a Wii U does. Your normal Wi-Fi stays free for the internet. Built-in laptop Wi-Fi usually can't do
  this. The setup window tells you whether an adapter will work.

## Set up (once)

1. Plug in the USB Wi-Fi adapter.
2. In Cemu: **Options > General settings > GamePad > Set up GamePad**.
3. Choose the adapter and press **Next**. Enter your password when asked (hosting a Wi-Fi network needs it once).
4. Press the **SYNC** button on the back of the GamePad and enter the four symbols shown in Cemu.
5. **All set.** The GamePad shows "GAMEPAD CONNECTED".

## Play

Turn the GamePad on and start a game, from Cemu or from a frontend like ES-DE.

- The GamePad controls player 1's GamePad automatically, **alongside** any controller you've mapped there.
- Games that draw nothing on the GamePad (Super Smash Bros. for Wii U) show the TV picture on it instead. You can turn
  that off in the GamePad tab.
- Input settings has a **Wii U GamePad** controller and profile, if you want the GamePad somewhere else.
- **Reset** in the GamePad tab forgets the GamePad and the adapter.

## Status

Early release. Tested on one PC (Bazzite) with one GamePad, an RT5572 adapter, Nintendo Land, Pokkén Tournament and
Super Smash Bros. for Wii U. Not done yet: GamePad audio and microphone, the camera, and a single download (for now
the GamePad bridge is built from source, see its repository).

## Why Linux only

The PC has to act as the GamePad's console: host a 5 GHz Wi-Fi network the way a Wii U does, and stamp every video
packet with the Wi-Fi adapter's internal clock, which the GamePad syncs to. Linux lets programs do both (hostapd, and
reading the adapter's clock). Windows and macOS don't.

## Credits

Built on years of GamePad reverse engineering by others: [libdrc](https://github.com/GaryOderNichts/libdrc)
(memahaxx), [Vanilla](https://github.com/vanilla-wiiu/vanilla), [drc-sim-c](https://github.com/rolandoislas/drc-sim-c),
the drc-hostap forks, and drc-x264. Most of this fork's code was written with an AI assistant (Claude), directed and
tested on real hardware by the maintainer.

---

# **Cemu - Wii U emulator**

[![Build Process](https://github.com/cemu-project/Cemu/actions/workflows/build.yml/badge.svg)](https://github.com/cemu-project/Cemu/actions/workflows/build.yml)
[![Discord](https://img.shields.io/discord/286429969104764928?label=Cemu&logo=discord&logoColor=FFFFFF)](https://discord.gg/5psYsup)
[![Matrix Server](https://img.shields.io/matrix/cemu:cemu.info?server_fqdn=matrix.cemu.info&label=cemu:cemu.info&logo=matrix&logoColor=FFFFFF)](https://matrix.to/#/#cemu:cemu.info)

This is the code repository of Cemu, a Wii U emulator that is able to run most Wii U games and homebrew in a playable state.
It's written in C/C++ and is being actively developed with new features and fixes.

Cemu is currently only available for 64-bit Windows, Linux & macOS devices.

### Links:
 - [Open Source Announcement](https://www.reddit.com/r/cemu/comments/wwa22c/cemu_20_announcement_linux_builds_opensource_and/)
 - [Official Website](https://cemu.info)
 - [Compatibility List/Wiki](https://wiki.cemu.info/wiki/Main_Page)
 - [Official Subreddit](https://reddit.com/r/Cemu)
 - [Official Discord](https://discord.gg/5psYsup)
 - [Official Matrix Server](https://matrix.to/#/#cemu:cemu.info)
 - [Setup Guide](https://cemu.cfw.guide)

#### Other relevant repositories:
 - [Cemu-Language](https://github.com/cemu-project/Cemu-Language)
 - [Cemu's Community Graphic Packs](https://github.com/cemu-project/cemu_graphic_packs)

## Download

You can download the latest Cemu releases for Windows, Linux and Mac from the [GitHub Releases](https://github.com/cemu-project/Cemu/releases/). For Linux you can also find Cemu on [Flathub](https://flathub.org/apps/info.cemu.Cemu).

On Windows, Cemu is available both as an installer and in a portable format, where no installation is required besides extracting it in a safe place.

The native macOS build is currently purely experimental and should not be considered stable or ready for issue-free gameplay. There are also known issues with degraded performance due to the use of MoltenVK and Rosetta for ARM Macs. We appreciate your patience while we improve Cemu for macOS.

Pre-2.0 releases can be found on Cemu's [changelog page](https://cemu.info/changelog.html).

## Build Instructions

To compile Cemu yourself on Windows, Linux or macOS, view [BUILD.md](/BUILD.md).

## Issues

Issues with the emulator should be filed using [GitHub Issues](https://github.com/cemu-project/Cemu/issues).  
The old bug tracker can be found at [bugs.cemu.info](https://bugs.cemu.info) and still contains relevant issues and feature suggestions.

## Contributing

If you want to contribute you can take a look at our [contribution guidelines](/CONTRIBUTING.md).

## License
Cemu is licensed under [Mozilla Public License 2.0](/LICENSE.txt). Exempt from this are all files in the dependencies directory for which the licenses of the original code apply as well as some individual files in the src folder, as specified in those file headers respectively.
