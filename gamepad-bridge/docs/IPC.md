# Cemu ↔ bridge IPC, version 1.1

Normative definition: `bridge/include/drcbridge/ipc.h`. This document explains it. If they disagree,
the header wins and this doc gets fixed.

## Transport

- **Unix domain socket, `SOCK_SEQPACKET`**: message boundaries are preserved, it's reliable and ordered,
  and it can pass file descriptors.
- **Path:** `$XDG_RUNTIME_DIR/cemu-gamepad/bridge.sock`. The directory is mode 0700, so only this user
  can connect. The bridge owns and creates it.
- **The bridge is the server.** Cemu connects when a game starts, if "Enable GamePad Bridge" is on
  (`DECISIONS.md`). If connecting fails, Cemu shows an on-screen notice and runs without the pad.
- **One client at a time.** A second client gets `REJECT` (reason `BUSY`).

## Framing

Every message is one packet: a 16-byte header followed by a type-specific payload.

```
struct MsgHeader { u32 magic = 'DRCB'; u16 version_major; u16 type; u32 payload_size; u32 reserved = 0; }
```

Host byte order on both sides, since it's the same machine. The receiver drops a packet whose magic
or size doesn't match its type, and logs it loudly.

## Versioning

- `HELLO` carries the client's major.minor version, and `WELCOME`/`REJECT` carry the bridge's.
- **A different major version means `REJECT` (reason `VERSION`)**, and Cemu shows the notice saying so.
- A minor version adds message types or appends payload fields. Receivers ignore unknown types and
  accept a larger `payload_size` than they know, reading only the prefix they understand.

## Frames: shared memory, not the socket

Pixels never go through the socket.

1. On `WELCOME`, the bridge passes a **memfd** (via `SCM_RIGHTS`) sized
   `slot_count × slot_size`. Cemu maps it.
2. **Slots start owned by Cemu.** Cemu writes a frame into a free slot, then sends `FRAME_SUBMIT`
   (slot index + metadata). Ownership passes to the bridge.
3. The bridge sends `FRAME_RELEASE` when it's done reading. Ownership passes back to Cemu.
4. **If no slot is free, Cemu drops the DRC frame and counts the drop.** It never blocks the emulator.
   A hidden queue would add latency (`CLAUDE.md`: no buffering in the video path without flagging it).
   With 3 slots, at most 2 frames can be in flight; anything more is a sign the bridge can't keep up.

v1 format: `RGBA8` (bytes R, G, B, A), with stride given per frame. Size comes per frame because
Cemu's DRC framebuffer size depends on the game and its graphic packs. The bridge scales and pads to
the encoder's 864×480 (`PROTOCOL.md`).

## Clocks and timestamps

- **Every timestamp is `CLOCK_MONOTONIC` in nanoseconds**, read on the same machine with one kernel
  clock (`PROJECT.md`). Mapping to the link's TSF is the bridge's job (Task 7), never Cemu's.
- `FRAME_SUBMIT` carries `t_flip` (Cemu saw the DRC flip) and `t_submit` (pixels are in the slot). The
  bridge stamps every later stage itself (`t_received`, `t_encoded`, `t_sent`, ...) and logs
  percentiles.
- `FRAME_PRESENTED` reports when the pad displayed (or is estimated to have displayed) a frame, keyed
  by `frame_id`. **This is what the present gate (Task 7) runs on.** Its `flags` say whether the time
  is measured or estimated. Until there's real hardware, it's always estimated (mock pad = the time it
  drew the frame).

## Messages (v1.0)

| Type | Dir | Name | Payload |
|---|---|---|---|
| 1 | C→B | `HELLO` | version, pid, client name |
| 2 | B→C | `WELCOME` | version, pad status, slot_count, slot_size, max w/h; **memfd attached** |
| 3 | B→C | `REJECT` | reason (`VERSION`, `BUSY`, `INTERNAL`), text |
| 10 | C→B | `FRAME_SUBMIT` | slot, frame_id, w, h, stride, format, t_flip, t_submit |
| 11 | B→C | `FRAME_RELEASE` | slot |
| 12 | B→C | `FRAME_PRESENTED` | frame_id, t_presented, flags |
| 20 | B→C | `INPUT_STATE` | seq, t_received, buttons, sticks, touch, accel, gyro |
| 40 | B→C | `PAD_STATUS` | connected, source (none / mock / real), battery |
| 99 | both | `GOODBYE` | reason |

Audio (DRC speaker) is reserved for a minor version later (Phase 3).

### Input

`INPUT_STATE` carries **logical** state, already parsed by the bridge, never raw HID:
- buttons as a bridge-defined bitmask (`ipc.h`, `DRCB_BTN_*`)
- sticks as floats in [-1, 1]
- touch as normalized [0, 1] position plus a down flag
- accel in g, gyro in degrees/second

How Cemu consumes it is Task 8's decision. Options: a native input provider, or Cemu's existing uinput
and DSU paths, with only touch coming through here. Defining it now means the mock pad can test the
round trip from day one.

## Lifecycle

```
Cemu: connect ──HELLO──▶ bridge
      ◀──WELCOME + memfd── (or REJECT, then close)
      ──FRAME_SUBMIT──▶ … ◀──FRAME_RELEASE── ◀──FRAME_PRESENTED── ◀──INPUT_STATE── …
      ──GOODBYE──▶ (or the socket just closes: game ended / Cemu crashed)
```

On disconnect for any reason, the bridge switches the pad back to the idle stream immediately
(`PROJECT.md`, "Idle streaming"). The pad must never starve because Cemu went away.

## Mock pad link (development only)

The bridge talks to `mock-pad` over `$XDG_RUNTIME_DIR/cemu-gamepad/mockpad.sock` using the **same
structs in the other direction**: the mock pad sends `HELLO`, the bridge sends `WELCOME` + memfd
(3 × 864×480 RGBA), then `FRAME_SUBMIT`s. The mock pad returns `FRAME_RELEASE`, `FRAME_PRESENTED`
(flagged `DRCB_PRESENTED_ESTIMATED`: the time its present call returned) and `INPUT_STATE`. The real
pad link replaces this with libdrc's UDP protocol. This socket isn't part of the Cemu contract.

### 1.1: encoded frames to the mock pad

`DRCB_FMT_DRC_H264` (bridge → mock pad only): the slot holds a `drcb_encoded_frame` header (IDR flag + 5
chunk sizes), then the raw chunks. The mock pad rebuilds Annex-B exactly like libdrc's debug dump
(`bridge/src/drc_annexb.cpp`) and decodes with libavcodec. Encoded frames are decoded in order and never
skipped, since each P frame references the previous one. Cemu still only sends `RGBA8`; a 1.0 Cemu works
unchanged with a 1.1 bridge.
