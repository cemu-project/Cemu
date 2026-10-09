# DRC protocol: cited summary

**Every claim cites a file and line in `third_party/`.** Uncited = not allowed (`CLAUDE.md`). Where the
references are silent, it says `TODO: verify against <source>`.

Pinned sources:

| Short name | Path | Upstream | Commit |
|---|---|---|---|
| libdrc | `third_party/libdrc` | GaryOderNichts/libdrc | `fe2eb2a` (2025-01-24) |
| drc-hostap | `third_party/drc-hostap-opencma` | opencma/drc-hostap (GitHub fork of memahaxx/drc-hostap) | `418e5e2` (2017-05-31) |
| drc-hostap (vanilla) | `third_party/drc-hostap-vanilla` | vanilla-wiiu/drc-hostap (fork of opencma) | `a2f705d` |
| vanilla | `third_party/vanilla` | vanilla-wiiu/vanilla | `786e709` |
| vanilla wiki | `third_party/vanilla.wiki` | vanilla-wiiu/vanilla.wiki | `ec5a849` |
| drc-sim-c | `third_party/drc-sim-c` | rolandoislas/drc-sim-c | `3aade2f` |

**The memahaxx originals are still on Bitbucket** (checked 2026-10-05 with `git ls-remote`):
`bitbucket.org/memahaxx/libdrc` (`5c26ba6`), `memahaxx/drc-hostap` (`6b2b8ca`), `memahaxx/drc-x264` (`998dacc`).
They're gone from GitHub only. An earlier version of this doc called drc-hostap "vanished"; that was
wrong. Not vendored yet.

Sections get filled in as the code is read. So far: **radio and pairing**.

---

## Radio

- The link is 5 GHz 802.11n. (libdrc `web/docs/re/wifi.rst:4`)
- The DRC hostapd sample configs, both pairing and normal, use `hw_mode=a`, `channel=36`,
  `ieee80211n=1`. (drc-hostap `conf/wiiu_ap_pairing.conf:13-14,53`; `conf/wiiu_ap_normal.conf:13-14,53`)
  - That's the sample's channel, **not a stated requirement of the pad**. Whether the pad scans or
    accepts other channels: TODO: verify against vanilla / vanilla wiki.
  - Relevance to this laptop: ch 36 is one of the channels the AX200 currently allows for
    transmitting (no `no IR` flag; `docs/HARDWARE-NOTES.md`). That still needs checking with the card
    disassociated.

## Pairing (WPS)

- Pairing uses a modified WPS. Normal operation uses a modified WPA2. (libdrc `web/docs/re/wifi.rst:4-6`)
- **The modification:** WPS AuthKey is derived from `KDF(KDK_ROT)`, where KDK_ROT is KDK rotated 3 bytes
  left. A stock WPS stack fails at M2. (libdrc `web/docs/re/wifi.rst:18-22`)
  - Implemented in drc-hostap under `CONFIG_TENDONIN`. (`src/wps/wps_common.c:115-122`; summary in
    `README.DRC:7-12`)
- `CONFIG_TENDONIN` also enables a WPA PTK rot3, the Nintendo RSN selectors (`a4-c0-e1`), and Wii U
  pairing-AP detection. (drc-hostap `README.DRC:7-12`)
- **PIN:** a console shows 4 symbols (♠=0 ♥=1 ♦=2 ♣=3), and the last 4 digits are fixed `5678`.
  **When pairing a pad to a computer, the computer chooses the code**; it just has to match what's
  entered on the pad. (libdrc `web/docs/re/wifi.rst:26-32`)
- After WPS, the pad holds the SSID, BSSID, and PSK for normal mode. (libdrc `web/docs/re/wifi.rst:34-35`)

### Pairing without a console: supported in code, undocumented in prose

`PROJECT.md` says "libdrc documents this path." **That's only half true:**

- libdrc's network guide lists console-free pairing ("Have a computer host WPS and sync a GamePad
  directly with it") and then says **"TODO: write instructions for this method."** (libdrc
  `web/docs/network.rst:97-98`) The only written procedure needs a console. (`network.rst:43-95`)
- **However**, drc-hostap ships `conf/wiiu_ap_pairing.conf`, described as a "hostapd config to simulate
  pairing a Gamepad to a device" (`README.DRC:17-18`). It runs an open AP named like a Wii U pairing AP
  with WPS enabled (`wiiu_ap_pairing.conf:62,70`).

So the pieces for console-free pairing exist in code. No reference documents the procedure end to end,
and nothing here proves it works. Next: check whether Vanilla or its wiki have a host-side pairing
flow, since they're the most recent. TODO: verify against vanilla / vanilla wiki.

### Pairing symbols come from the console MAC, they are NOT free

libdrc says a computer "get[s] to choose the code" (`wifi.rst:29-32`). For a real pad that's wrong. The console derives
the symbols from the **last byte of its (DRH) MAC address**, two bits per symbol, high bits first. It starts WPS with
PIN = those 4 digits + `5678` (GaryOderNichts `recovery_menu` `ios_mcp/source/ccr.c:119-145`, `CCRSysGetPincode` /
`CCRSysStartPairing`, commit 837690b). The same MAC is in the pairing SSID, which is how the pad can tell which
`_STA1` network belongs to the symbols the user entered (`drc_pin_generator` `index.html:107,124-129`: reads the last two hex
digits of the SSID, symbol order 0..3 = ♠ ♥ ♦ ♣). Pad day 2026-10-06/07: with an AP MAC ending in `e1` (= ♣♦♠♥) and the
user entering ♠♦♥♣, the pad never approached the AP, and sent nothing on ch 36 during sync. `padday-batch1.sh` now
computes PIN and symbols from the AP MAC (`...:27` = ♠♦♥♣).

Run 20261007-003658-chanscan (pairing AP on 9 channels, one sync cycle each, MAC-derived symbols): on every channel the
pad transmits until SYNC is pressed, then **nothing at all** until "Could not connect". So in sync mode the pad only
listens, and it ignored our beacon on every channel. At that point the beacon's WPS element was byte-identical to
the captured console's (MAC substituted). The SSID format, f5/Broadcom/WMM elements and their order matched too. The
only remaining difference was HT Capabilities/Operation, now written as the console's bytes (patch 0002).

### Pairing AP: what a real console's beacon looks like

The drc-hostap sample isn't enough. On 2026-10-06 a real GamePad in sync mode, next to the laptop, never sent a
frame to our sample-based pairing AP (`runs/20261006-225103-padday`). The only pad frames were ordinary probes for
its old console, sent before the user started sync. What a real console sends (dvdhrm, "Wii U Gamepad Connection", 2013-03-23,
`iw scan` of a real pairing AP, <https://dvdhrm.wordpress.com/2013/03/23/wii-u-gamepad-connection/>):

- BSS `34:af:2c:5f:a6:1c`, ch 36 (5180), HT20, open (no Privacy), beacon interval 100.
- **SSID `WiiU34af2c5fa6134af2c5fa61c_STA1`** = `WiiU` + the first 11 hex digits of the BSSID + the full BSSID +
  `_STA1` (libdrc calls these `<mac2><mac>`, `wifi.rst:11-13`). drc-hostap's sample `WiiUaabbccddeeffaabbccddeef_STA1`
  has the parts the other way round, with a fake MAC.
- **No WFA WPS element.** The WPS attributes come in a vendor element `a4:c0:e1` type `f4` (version, state 02, …,
  "Broadcom", "SoftAP"), plus a second vendor element `a4:c0:e1 f5 00`. drc-hostap's wpa_supplicant agrees: under
  `CONFIG_TENDONIN` it accepts a WPS IE under `WPS_DEV_OUI_NIN` = `a4c0e1f4` (`wpa_supplicant/wps_supplicant.c:1733-1763`,
  `src/wps/wps_defs.h:284`). drc-hostap's *hostapd* still sends the WFA element, which is fixed by our
  `patches/hostapd/0002` (`wps_nintendo_ie=1`).
- Normal mode, same capture: SSID hidden (all zero bytes), Privacy, RSN with `a4-c0-e1` suites. The SSID string
  itself is `WiiU` + the BSSID: our pad probes for `WiiU182a7b93f9da` (its old console) in the run above.

Still unknown: whether the pad also checks that the MAC in the SSID matches the BSSID (we now make them match
anyway), and which element of the beacon it actually keys on.

### Normal mode (after pairing)

- The sample normal-mode AP: `wpa=2`, `wpa_key_mgmt=WPA-PSK`, `wpa_pairwise=CCMP`. (drc-hostap
  `conf/wiiu_ap_normal.conf:76-80`) With `CONFIG_TENDONIN`, the RSN selectors are Nintendo's
  (`README.DRC:10`; libdrc `web/docs/re/wifi.rst:40-46`).
- **Discrepancy:** libdrc says the console's normal-mode AP has an **empty SSID** (`web/docs/re/wifi.rst:40-41`).
  drc-hostap's sample normal config sets `ssid=WiiUaabbccddeeff` (`conf/wiiu_ap_normal.conf:64`).
  **Resolved:** the same sample config sets `ignore_broadcast_ssid=2` (`conf/wiiu_ap_normal.conf:72`,
  in both drc-hostap copies), which in hostapd blanks the SSID in beacons while keeping it configured.
  That matches libdrc's "empty SSID". A client therefore has to probe for the hidden SSID
  (`scan_ssid=1` in `harness/wpas-pad-normal.conf`).

## Link encryption: the "802.11n obfuscation" (Vanilla Pipe page summary)

- **What it is:** the PTK (derived from the PSK in the standard WPA 4-way handshake, then used to encrypt
  all traffic) is **rotated 3 bytes left**. A stock WPA implementation derives the wrong PTK and can't
  talk to the other side. (vanilla wiki `Pipe.md:1`)
- **Why it constrains hardware:** the PSK can't be adjusted to compensate. The PTK derivation itself
  has to be modified, so the OS must do the handshake. That works on Linux only if the **wireless
  hardware doesn't handle the PTK itself**. Broadcom chips are named as notorious for this.
  (vanilla wiki `Pipe.md:3`)
- **It's already implemented for the host side.** drc-hostap applies the rotation in
  `wpa_pmk_to_ptk()` under `CONFIG_TENDONIN` (`src/common/wpa_common.c:141,187-192`). That function is
  shared code, and hostapd's authenticator calls it (`src/ap/wpa_auth.c:2039`). The WPS KDK rotation
  for pairing is separate (see Pairing).
- The RSN selectors become Nintendo's `a4:c0:e1` for CCMP and PSK key management
  (drc-hostap `src/common/wpa_common.h`, `RSN_CIPHER_SUITE_CCMP` / `RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X`
  under `CONFIG_TENDONIN`).
- Vanilla's "pipe" itself is architecture, not protocol. Vanilla splits the privileged radio/connection
  process from its frontend so a non-Linux frontend can use a Linux machine as the radio
  (`Pipe.md:5`). That's the same split as our bridge vs. Cemu.
- drc-hostap-vanilla rebases the same `CONFIG_TENDONIN` patches onto hostap 2.12 (its log: "Merge tag
  'hostap_2_12'"). Its `conf/wiiu_ap_pairing.conf` and `conf/wiiu_ap_normal.conf` are byte-identical to
  opencma's (checked with `diff`).

**Consequences for this project:**

- The obfuscation is **not** something we reimplement. It's a compile flag in a hostapd we build.
  `TASKS.md` 3.4's "the part we reimplement host-side" overstates the work.
- The real hardware question is narrower than "SoftMAC vs FullMAC": **does the AX200 in AP mode let
  hostapd do the 4-way handshake and install the key it derived?** Not stated in any reference. TODO:
  verify on hardware (POST-HW), or earlier with an AP-mode association test once Ethernet is connected.
- The Nintendo OUI cipher selectors are only advertised in the RSN IE. Whether the AX200 firmware objects
  to a non-standard RSN IE in AP beacons: TODO: verify on hardware.
- Build from drc-hostap-vanilla (hostap 2.12, maintained) instead of opencma (hostap 2.6, 2017) unless
  a difference shows up.

## Video encoding (libdrc's H.264 encoder settings)

All from libdrc `src/h264-encoder.cpp` unless noted. **These matter for the NVENC plan.**

- **Encoded size 864×480** (`include/drc/screen.h:31-32`; used at `h264-encoder.cpp:140,233`), not
  854×480 as `TASKS.md` 6.1 assumed. 864 = 54 macroblocks. **Displayed size 854×480:** the fixed
  GamePad SPS libdrc writes in its dump (`h264-encoder.cpp`, `DumpH264Headers`) decodes as 854×480 High
  profile (ffprobe on our reconstructed stream, 2026-10-05), so the SPS carries frame cropping.
- Main profile (`:190`), CABAC on (`:154`), no B-frames (`:156`), 1 reference frame (`:158`),
  constrained intra (`:159`), **no 8x8 transform** (`:163`), no P sub-16x16 partitions (`:143`).
- Constant QP 32 (`:168`).
- No SPS/PPS/SEI/AUD in the stream (`:172-173`), so the pad must use fixed headers. The ones libdrc uses to
  make its streams decodable are in `DumpH264Headers` / `DumpH264Frame` (SPS, PPS, and fixed IDR/P slice
  headers with an 8-bit frame number). **Validated:** our stream, rebuilt with exactly those bytes
  (`bridge/src/drc_annexb.cpp`), decodes with 0 errors in libavcodec and ffmpeg (600 synthetic frames;
  6661 real Nintendo Land frames). That proves the headers match the encoder's output. It doesn't prove the
  real pad uses those headers.
- Single slice, single thread, frame produced serially (`:181-183`).
- **Requires a patched x264** (`b_drh_mode`, `:175-178`; checked by `configure:101-114`; source named in
  `web/docs/installation.rst:23-24` as `memahaxx/drc-x264`). The comment at `:175-177` says the patch
  makes x264 **return macroblock rows instead of NAL units**, and that x264 "must also be modified to
  not produce macroblocks utilizing **planar prediction**".

### What the drc-x264 patch does

Vendored at `third_party/drc-x264` (Bitbucket `memahaxx/drc-x264`, `998dacc`). Three commits on top of
stock 2013 x264: `3bb4aa7d`, `dc6d017c` (Pierre Bourdon, Nov 2013), and `998dacc1` (Henrik Gramner,
Jan 2014). Summary in `README.DRC:7-13`.

1. **No slice header.** With `b_drh_mode`, the slice header isn't written at all; only the CABAC slice
   data goes out (`encoder/encoder.c:2584-2585`). The pad must assume the header contents.
2. **The fixed QP that implies.** Slice QP is forced to 32 (`encoder/encoder.c:2581`) and PPS
   `pic_init_qp` to 32 (`encoder/set.c:434`), so `slice_qp_delta` is always 0 (`README.DRC:13`).
   That's consistent with item 1: with no header, the pad can't be told any other QP.
3. **"DRH-style slicing".** It's **one** slice per frame, but its CABAC byte stream is cut into separate
   chunks, with a boundary at the first point where CABAC output has grown after each 6th macroblock row
   (`encoder/encoder.c:2856-2884`; `README.DRC:8`: "emit a NAL unit every 6 rows of macroblocks").
   These chunks aren't independent slices; they're pieces of one continuous CABAC stream, handed to
   libdrc through `nalu_process` as they're produced. That's also what makes per-chunk, low-latency
   sending possible.
4. **No planar intra prediction**, for 16x16 luma and for chroma: the planar mode is removed from the
   candidate tables (`encoder/analyse.c:608-609,617-618`). README: "not working on DRC"
   (`README.DRC:10`).

### Consequences for the NVENC plan: HIGH risk, now specific

For NVENC output to work on the pad, the bridge would need all of the following, and **none is
established**:

- **Strip the slice header** from each NVENC frame, so that only the CABAC data starts the payload.
  It's probably doable (we know our own SPS/PPS, so the header can be parsed and skipped), but whether
  the CABAC alignment bits must match what the pad expects is untested.
- **Slice QP exactly 32 and no per-MB QP changes**, i.e. NVENC constant-QP 32 with no adaptive
  quantization. NVENC's actual QP behavior in that mode: TODO: verify against NVENC API docs/headers.
- **Never use planar prediction.** **Checked 2026-10-05:** `NV_ENC_CONFIG_H264` in NVENC API 13.0
  (`/usr/include/ffnvcodec/nvEncodeAPI.h` lines 1805–1909, Fedora `nv-codec-headers`) has controls for
  CABAC, constrained intra, the 8x8 transform, slice mode, SPS/PPS, and intra refresh, but **nothing that
  selects or restricts intra prediction modes**. There's no supported way to keep planar out of NVENC
  output. NVENC is ruled out for the pad stream.
- **Cut chunks at 6-macroblock-row boundaries inside one CABAC slice.** Finding those byte positions in
  someone else's output means decoding the CABAC stream in the bridge, which costs latency and is
  complex. Multiple real slices are a different bitstream, and nothing says the pad accepts them.
  Whether the pad *requires* the chunking or libdrc just uses it for latency:
  TODO: verify against libdrc `vstrm-packet.cpp` / `video-streamer.cpp`.

**Decided (`DECISIONS.md`, 2026-10-05):** the pad's encoder is **drc-x264 on the CPU**. The frame path becomes
GPU render → readback (864x480 I420 ≈ 0.6 MB/frame) → x264 → libdrc packetization. Zero-copy NVENC
stays a research question, not the plan. Costs to measure in Task 6:
- Readback latency on the GTX 1650.
- x264 encode time per frame at libdrc's settings (CQP 32, single thread, `h264-encoder.cpp:181`).
- CPU load on a CPU that already runs at 95 °C and throttles (`MEASUREMENTS.md`).

## Input (HID)

- 128-byte packets, about 180 per second, pad port 50122 → console port 50022
  (libdrc `web/docs/re/sc-input.rst:4-13`; `services.rst`).
- **Parser:** `bridge/src/hid_parser.cpp`. Buttons, sticks, touch, battery, and volume are a line-for-line
  transcription of libdrc `InputReceiver::ProcessInputMessage` (`src/input-receiver.cpp`), with libdrc's
  stick range (900..3200), dead zone (0.1), and default touch calibration ("those that the device itself
  uses prior to loading the real ones from UIC config").
- **Buttons:** `(msg[80] << 16) | (msg[2] << 8) | msg[3]`, bit values as in libdrc `include/drc/input.h`.
  The power button is `msg[4] & 0x02` (`sc-input.rst` PowerStatusMask).
- **Touch:** 10 points from byte 36, each coordinate a little-endian u16 with the value in bits 0–11 and
  "extra" in bits 12–14 (libdrc's read). The doc's bitfield (`sc-input.rst:102-112`) is drawn from the
  bit-reversed view. Vanilla's packer double-reverses into the same bytes, and the cross-check confirms
  it. Pressure comes from the extra bits of points 0–1; "pressed" = pressure ≠ 0 (libdrc).
- **IMU: the references disagree.** `sc-input.rst:76-86` lists accel x,y,z and gyro roll,yaw,pitch.
  Vanilla's packer (`lib/gamepad/input.c:24-36`) writes accel **z,x,y** and gyro **roll,pitch,yaw**
  (s24 LE). libdrc never parses the IMU, so its struct order is unverified, while Vanilla's is used with
  real consoles. **We follow Vanilla**, scale included: gyro raw = deg/s ÷ (1200/154000); accel raw =
  m/s² × −800 (x, y), × 800 (z) (`input.c:276-282`). Axis signs relative to the real pad: TODO: verify
  on hardware.

### Cross-check (no recorded packets exist)

No reference contains recorded HID traffic (searched 2026-10-05). `bridge/tests/hid_crosscheck.cpp` feeds
packets from **Vanilla's real packer** into our parser instead. Result: 0 failures.
- All 18 buttons are exact, the IMU round trip is exact, and 127-byte packets are rejected.
- Finding: touch agrees within **2.4% of screen size**. That's the two references' different calibrations
  (Vanilla's margins vs libdrc's defaults).
- Finding: **sticks top out at ±0.89**. Vanilla packs ±1 as 2048±1024, while libdrc assumes 900..3200. The
  real pad's range decides which is right. Calibrate on hardware.

This shows the two references agree. **It doesn't show what a real pad sends.** Recorded-packet tests
are post-hardware (`TASKS.md` 11).

## TSF (timestamp clock)

- libdrc stamps every vstrm/astrm packet with the **low 32 bits of the Wi-Fi TSF** (`src/video-streamer.cpp:41-45`,
  `src/audio-streamer.cpp:50-54`). It reads the TSF as 8 raw bytes from `/sys/class/net/<if>/tsf` (or
  `.../device/tsf`) of the interface holding 192.168.1.10 (`src/tsf-linux.cpp`). **If that fails, GetTsf returns
  -1 and the callers ignore it, sending an uninitialised timestamp.** The bridge's `--pad real` refuses to start
  without a readable TSF.
- Stock kernels don't have that file: it comes from **memahaxx/drc-mac80211** (`third_party/drc-mac80211`, commit
  `4a9823c`, Linux 3.12.3), which adds a sysfs attribute returning `drv_get_tsf()` (`README.DRC`).
- On this machine (2026-10-06):
  - mac80211's debugfs has a `tsf` file **only for IBSS and mesh interfaces** (OGC 7.2.4 `net/mac80211/debugfs_netdev.c:868-877`),
    and none for client or AP interfaces (checked on wlo1 and on an hwsim AP).
  - **Kernel source verified:** OGC tag `v7.2.4-ogc3` (`86a9082d`) rebuilt with the same GCC 16.2.1 gives a mac80211
    whose `.text` (949,422 bytes) is **byte-identical** to the installed module's; string differences are path
    prefixes only, with line numbers matching. Verification needed because the kernel has no MODVERSIONS and mac80211
    has no srcversion, so a mismatch would load silently.
  - **Port:** `patches/mac80211/0001-export-tsf-to-sysfs-for-libdrc.patch` (iface.c, +45). Modernised: trylock the
    wiphy mutex (the removal paths hold it, so blocking could deadlock sysfs removal), -ENODEV when not in the driver,
    -EOPNOTSUPP if the driver has no get_tsf. Builds; built **without BTF** (container pahole 1.32 vs the kernel's
    1.31, so Kbuild skips it).
  - **Tested 2026-10-06** (user ran the swap by hand): the patched module **loads cleanly** (no BTF/signature
    complaints, Wi-Fi re-associated) and `/sys/class/net/wlo1/tsf` exists, but **every read returns EOPNOTSUPP**:
    drv_get_tsf returned -1, i.e. **iwlmvm has no get_tsf**. Confirmed in source: no `.get_tsf` anywhere in OGC 7.2.4
    `drivers/net/wireless/intel/iwlwifi/mvm/`; the TSF is firmware-internal (`mac-ctxt.c:67-150` allocates "TSF
    resources" in firmware), never read back except a remote AP's TSF in FTM ranging (`ftm-initiator.c:191-206,1348`).
  - The AX200 does expose a PTP clock, `/dev/ptp0` "iwlwifi-PTP" (`mvm/ptp.c`), built on the device's GP2 microsecond
    timer, **not the TSF**. Whether the firmware's AP TSF is GP2 + a constant isn't documented anywhere we can cite.
  - **Drivers that do implement get_tsf** (OGC 7.2.4 source): `rt2800usb` (`ralink/rt2x00/rt2800usb.c:655`, the one
    libdrc's docs list as working, `network.rst:21`), `rt73usb`, `ath9k`/`ath9k_htc`, and MediaTek
    `mt7615/mt7915/mt7921/mt7925/mt7996`. **Not** `mt76x0u`/`mt76x2u` (MT7610U/MT7612U, the fallback PROJECT.md suggests).
  - **So with the AX200, libdrc can't get a real TSF.** What the pad does with a timestamp in a different clock base is
    unknown (libdrc notes ath9k "might desync more often because of a TSF drifting issue", `network.rst:22-23`, so the
    pad does care). Options: test a substitute timestamp (GP2/PTP or monotonic) on the real pad, or use an adapter
    whose driver implements get_tsf.
