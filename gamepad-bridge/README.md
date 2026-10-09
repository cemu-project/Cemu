# GamePad bridge

Everything Cemu needs to use a real Wii U GamePad, besides Cemu itself: the bridge daemon, the patched libraries
it uses, and the scripts that host the GamePad's Wi-Fi network and pair the GamePad. Linux only.

For what you need and how to play, see the [main README](../README.md).

## Build

You need a C/C++ toolchain, CMake, Ninja, yasm, pkg-config, patch, rsync, the libnl3 and OpenSSL development files
(hostapd) and the FFmpeg development files (libswscale, libavutil; libavcodec and SDL3 for the optional mock-pad).
On Fedora:

```
sudo dnf install gcc-c++ make cmake ninja-build yasm pkgconf patch rsync libnl3-devel openssl-devel ffmpeg-free-devel
```

Then, from this folder (Cemu itself is built as usual, see [BUILD.md](../BUILD.md)):

```
./build.sh
./scripts/install.sh
```

`install.sh` installs the GamePad's network service (asks for your password once) and the bridge's user service, and
links the setup helper so Cemu's **Set up GamePad** window can find it. Then set up the GamePad from Cemu:
**Options > General settings > GamePad > Set up GamePad**.

Programs the scripts use at run time: `ip`, `iw`, `nmcli` (NetworkManager), `dnsmasq`, `ethtool`, `udevadm`,
`systemctl`, `pkexec` (polkit), `python3`, `lspci`; `firewall-cmd` if firewalld is running.

## How it fits together

- **Cemu** (this fork) hands each GamePad frame to the bridge and takes the GamePad's input back (`docs/IPC.md`).
- **The bridge** encodes the frames with a patched x264, sends them on the console's schedule, and streams the
  GamePad's input to Cemu (`docs/PROTOCOL.md`, `docs/VIDEO-REMAKE.md`).
- **The GamePad network service** (`scripts/pad-net.sh`, `cemu-gamepad-net.service`) runs a patched hostapd on the
  USB adapter as the GamePad's access point whenever the adapter is plugged in.
- **Setup** (`scripts/gamepad-setup.sh`, run by Cemu's window) checks an adapter, gives this PC its own console
  identity and Wi-Fi key (`/etc/cemu-gamepad/pad.conf`, readable by root only), pairs the GamePad over WPS and installs
  the service.

Every video packet carries the Wi-Fi adapter's internal clock (TSF), which the GamePad syncs to; a substitute clock
makes it reject every frame. RT5572 (`rt2800usb`) adapters work on stock kernels: the clock is read over USB
(`patches/libdrc/0008`).

## Troubleshooting

- Setup logs: `/var/log/cemu-gamepad/*-setup.log`. Network logs: `journalctl -u cemu-gamepad-net`.
- Bridge log: `journalctl --user -u cemu-gamepad-bridge`. Cemu's log (`log.txt`) has `GamePad Bridge:` lines.
- Start over: **Reset** in Cemu's GamePad tab, or `sudo ~/.local/bin/cemu-gamepad-setup reset`.

## License

GPL-2.0-or-later (`LICENSE`), because the bridge links x264. Cemu itself stays under MPL-2.0. Third-party sources
in `third_party/` keep their own licenses: libdrc (BSD-2-Clause), drc-x264 (GPL-2.0), drc-hostap (BSD).
