# Video system remake

Started 2026-10-08, after the first Cemu sessions on the real pad (`STATUS.md` item 6): the picture is blocky,
stalls and tears, and lags far behind a real console. This is the plan to replace libdrc's `VideoStreamer` with our
own, built from measurements of what a real console sends.

## What libdrc does today (third_party/libdrc/src/video-streamer.cpp, h264-encoder.cpp)

- A fixed 59.94 Hz timer, not tied to Cemu's frames. Each tick it sends the packets encoded on the **previous**
  tick, then latches the newest Cemu frame and encodes it. That adds a full tick (16.7 ms) on top of the encode,
  and the timer beating against Cemu's frame rate repeats and skips frames (judder).
- Each frame is encoded completely before any of it is sent, and then all of its packets go out in one burst.
- x264 preset `slow`, single thread, fixed QP 32, intra refresh (keyint 10-30), constrained intra, DRH mode
  (no slice headers; 5 chunks of 6 macroblock rows each, `sc-vstrm.rst`).
- Timestamp: the TSF when the frame was latched. The packets go out a tick later.

## What we know from the real pad (runs/2026-10-08)

- QP 32: 43 resync requests in ~50 s. `DRC_QP=26` (patch 0006): about 60 resyncs/s, so the pad rejected nearly
  every frame. The Wi-Fi link was fine (58-65 Mbit/s MCS, 10 failed TX in 4 min).
- `sc-vstrm.rst` lists an extended-header option 0x83 "Force decoding even if buffer is too small". So the pad
  has a decode buffer limit. The likely cause of the QP 26 failure is frame size, not QP itself; unverified.
- Vanilla (the console-to-PC direction) rebuilds the console's stream with PPS `pic_init_qp` 32 and
  `slice_qp_delta` 0 (`vanilla/lib/gamepad/video.c:49-51, 70, 126`): every console slice also starts at QP 32.
  Whether the console then varies QP per macroblock is unknown.

## Step 1: ground truth from a real console (needs the Wii U, `scripts/console-video-capture.sh`)

Vanilla (built in `build/vanilla`) turns the RT5572 into a GamePad; the console's UDP is recorded decrypted, along
with the RT5572's TSF, which follows the console's clock in station mode. From that recording:
- frame sizes (bytes per frame and per chunk, at rest and in heavy motion) and the console's bitrate;
- IDR frequency and whether intra refresh is used;
- the extended-header options the console sets (0x83? 0x85?);
- how the vstrm timestamp relates to the console's TSF at send time (presentation delay);
- packet pacing (are a frame's packets sent in one burst or spread out?);
- per-macroblock QP variation (decode the H.264 with Vanilla's headers).

Build: `cmake -S third_party/vanilla -B build/vanilla -G Ninja
-DCMAKE_BUILD_TYPE=Release -DVANILLA_BUILD_USE_POLKIT=OFF && cmake --build build/vanilla`.

## Step 1 results (`runs/20261008-205950-consolevid`, 39 s of the Wii U's "Paired the Wii U GamePad" screen)

`scripts/vstrm-stats.py` on the console's stream and on ours (`runs/20261008-193641-padday/pad-udp.pcap`):

| | Real console | Ours (libdrc) |
|---|---|---|
| **Per-macroblock QP** | varies: 12-32 coded blocks (q12, q18, q24, q30, q32...) | flat 32 |
| **Frame send spread** (first to last packet) | p50 11.1 ms | 0.1 ms (one burst) |
| **Chunk-to-chunk gap** | p50 2.9 ms (each chunk sent as encoded) | 0.0 ms |
| **Max payload per packet** | 1694 bytes | 1251 (libdrc limit 1400) |
| **Frame size** | p50 1.8 KB, p99 15.8 KB, max 24 KB | p50 0.3 KB, max 4.3 KB (IDR) |
| **IDR frames** | none in 39 s (no IDR flag) | 5 (pad resync requests) |
| **Ext options** | framerate 0 (59.94), force-decode (0x83), 6 MB rows | same |
| **Timestamp step** | 16683 us, exact | 16685 p50, 4307-28914 |

QP per macroblock: `qpdump` (decode with Vanilla's SPS/PPS + slice-header words, `vanilla/lib/gamepad/video.c`,
then libavcodec `AV_FRAME_DATA_VIDEO_ENC_PARAMS`). So the pad decodes `mb_qp_delta` fine: our QP 26 failure was
not QP itself. The leading suspect is now the burst: the console spreads a frame over ~11 ms, and a QP 26 frame
sent in 0.1 ms may overflow the pad's receive side. The console also sends frames up to 24 KB, so size alone is
not the limit.

The TSF samples were empty (`/sys/class/net/<if>/tsf` reads failed in station mode), so the timestamp-vs-TSF
relation is still unknown. Content was a static screen; a capture with motion is still worth getting.

## Step 2: our own streamer in the bridge

Keep libdrc for input, commands, audio and the packet structs (`VstrmPacket`); replace `VideoStreamer`:
- **Driven by Cemu's frames**, not a free-running timer: encode as soon as a frame arrives, so no extra tick.
- **Send each chunk as soon as it is encoded** (drc-x264 hands out chunks through `nalu_process`), so the pad
  can start decoding the top of the frame while the bottom is still being encoded.
- **Timestamps and pacing as the console does them** (from step 1).
- **Rate control to the console's frame-size budget** (from step 1): sharper when the picture is simple, never
  over the size the pad accepts.
- **Encoder speed:** measure presets; keep the encode under a few ms even when the CPU is hot.

Every stage keeps its timestamps and percentile stats (CLAUDE.md), and nothing ships without a before/after
number in `MEASUREMENTS.md`.

## Step 2 status (2026-10-08): built, tested offline, not yet on the pad

`bridge/src/drc_video_sender.cpp`, used by `--pad real` unless `DRCB_VIDEO=libdrc`.
- Frame-driven on its own encoder thread; chunk k is sent when chunk k+1 appears (a CABAC carry can still change
  a chunk's last byte until then: sending each chunk instantly corrupted the rows after every chunk boundary,
  `bridge/tools/video_sender_test.cpp` + ffmpeg found it), the last chunk when the encode returns.
- Default: x264 `veryfast`, CRF 20 + variance AQ, per-macroblock QP 12-40, VBV cap 24 KB per frame. `cqp` gives
  libdrc's fixed QP.
- Knobs (env): `DRCB_VIDEO_RC`, `_CRF`, `_QP`, `_QPMIN`, `_QPMAX`, `_MAXKB`, `_PRESET`, `_PAYLOAD`, `_GAPUS`.
- Offline (`video-sender-test`, synthetic moving content, 60 fps paced): 0 decode errors in both modes; encode p50
  10.2 ms (veryfast) vs 13.1 ms (slow); first chunk on the wire ~5.7 ms after submit. The CPU governor is
  `powersave`, so paced runs are slower than encode-bench's flat-out 6.5 ms.

## First pad run of the new sender (`runs/20261008-212054-play`) and the fix

Input fine; picture as bad as before: frames grew to 15-24 KB (CRF 20 on Nintendo Land) and the pad sent ~2100
resync requests (~50/s, so about every other frame was an IDR). Cause, reproduced offline: **x264's VBV re-encodes
macroblock rows that overshoot, rewinding the bitstream after the previous chunk was already sent**, so every frame
that hit the size cap reached the pad corrupted. (With a tight cap, video-sender-test showed ffmpeg errors at
chunk boundaries; libdrc never sees this because it reads the chunks after the whole encode, and its fixed QP 32
never triggers VBV anyway.) Fix: no VBV; `crf_control()` adjusts the CRF between frames to keep them under
`max_frame_kb` (x264_encoder_reconfig). Offline with CRF 4 (frames up to 16 KB) and caps of 24/8/4 KB: 0 decode
errors. `DRCB_VIDEO_SWEEP=1` cycles cap / timestamp offset / packet spacing every 40 s and logs resyncs per phase,
in case the pad still rejects frames.

## Sweep run 1 (`runs/20261008-215859-play`): even libdrc's own encoder settings were rejected (~35 resyncs/s)

So the encoder settings were not the problem; the sender's reaction to resync requests was. The pad repeats
"\1\0\0\0" until it has decoded a recovery frame, and we answered each repeat with another IDR (Cemu IDRs up to
54 KB at QP 32), so it never caught up. libdrc drops requests that arrive while its IDR is still queued
(video-streamer.cpp: `if (send_idr && resync_requested) resync_requested = false`), and the real console sent no
IDR at all in 39 s. Now: requests within `resync_holdoff_ms` (500) of the last recovery are ignored, and the
default recovery is x264's on-demand intra refresh instead of an IDR (`DRCB_VIDEO_RESYNC=idr` for libdrc's way).

## Sweep run 2 (`runs/20261008-220817-play`): every phase rejected, 40-60 resyncs/s, libdrc settings included

Holdoff and intra refresh made no difference, so the pad rejects nearly every frame our sender produces, whatever
the encoder does. libdrc's VideoStreamer (43 resyncs in ~50 s, runs/20261008-194137-play) and ours now differ only
in transport: libdrc sends a frame's packets in one burst one tick after encoding (so they arrive ~1 frame after
their timestamp) from an ephemeral port; ours streamed chunks immediately from 50020. Packet headers compared byte
for byte against libdrc's real packets (runs/20261008-193641-padday/pad-udp.pcap): identical layout. Sweep 3
starts with an exact-libdrc control (`DRCB_VIDEO_SEND=next`, ephemeral port, IDR per request) and changes one
thing per phase.

## The clock (2026-10-08, late): the difference that explains every rejection run

Timestamp steps (`vstrm-stats.py`-style count): the console steps exactly 16683/16684 us on 1159 of 1164 frames;
libdrc's 59.94 Hz timer is within 1 ms on all but 12 of 3665; our frame-driven sender stamped each frame at
Cemu's delivery time, off by more than 1 ms on every step (video-sender-test: 399/399). The pad schedules
display by those timestamps. Now `pacing = "clock"` (default): one frame every 16683.35 us on the TSF, timestamp =
the exact grid value, newest Cemu frame encoded each tick (the last one again if nothing new, as the console
does), sent right after the encode from x264's final NAL list (`send_mode = "after"`).

Also fixed on the way: (1) chunk-callback streaming could send bytes before x264 had finished them (tiny chunks in
static regions leave CABAC bytes outstanding): truncated frames offline; "after" sends from the final list.
(2) x264 folds a chunk with no new bytes into the next (static regions): we spread the bytes so the pad still gets
5 chunks; libdrc instead resends the previous frame's pointer for the missing chunk (stale bytes).
Defaults are back to libdrc's encoder (slow, QP 32, IDR on resync with 250 ms holdoff) until a pad run shows 0
rejections; `pad-net.sh` now records our video stream (`pad-video.pcap`) to check it against the console's.

## First run with the console clock (`runs/20261008-225712-play`): "so much better"

On the air (`pad-video.pcap`): timestamp steps 16683/16684 us on every frame (max 115 ms where ticks were missed).
Stretches of 40+ s with zero resyncs. The rest: resync storms driven by IDRs: `slow` IDRs were 45-58 KB and took
~35 ms to encode (two ticks late), were rejected, and the next resync brought another (271 IDRs, ~4100 resyncs in
80 s). Now: `veryfast`, recovery by intra refresh (no IDR after the first frame, like the console), and ticks
already past are skipped instead of sending frames stamped in the past. `DRCB_VIDEO_SWEEP=1` is now a 60 s A/B:
QP 32 vs CRF 20 + AQ (console-like per-macroblock QP).

## Run `runs/20261008-230543-play`: intra-refresh-only recovery loses the pad

With resync answered by intra refresh only, the pad kept requesting (recoveries every 250 ms) and dropped the link
after ~22 s ("input stopped", reconnect, again). So after a decode error the pad waits for an IDR. Back to IDR on
resync, but cheaper: `ip_factor` 0.5 puts IDRs 6 QP above P frames (x264 encoder.c CQP: qp_i = qp_p -
6*log2(ip)), about half the bytes and encode time of the 45-58 KB / ~35 ms IDRs that arrived late before.

## Stepwise sweep (`runs/20261008-231346-play`): libdrc's own VideoStreamer failed too, so the radio, not the sender

L (libdrc itself) 42.8 and 68.4 resyncs/s; our steps 1-5 61-71/s; pad connected throughout (125-178 input packets/s).
Every sender failed alike, so the cause was outside the encoder and sender: the laptop's AX200 had moved to
the home router on channel 36 at **160 MHz** (5170-5330), the same channel as the pad's AP, next to four of its access points
and two more networks; during the first good runs it was on 149. `pad-net.sh` now uses channel 165 and warns
when another local interface overlaps the pad's channel.

## Channel 165 run (`runs/20261008-232039-padnet`): pad connected fine, still ~60 resyncs/s from the start

So the shared channel was not the main cause. Across all runs the pad is fine while frames are small (static
screens) and rejects constantly once frames are big. Burst shape, frames of 8+ packets: the console spreads them
over 11.8 ms (median) and never sends more than 16 packets per frame; ours (and libdrc's) went out in 0.06 ms, up
to 45 packets. Likely the pad's receive side (BCM4319) drops part of such a burst. Now: `send_mode after` +
`spread_us` 10000 sends a frame's packets evenly over 10 ms (capped to end 14 ms into the tick), with 1694-byte
payloads like the console (fewer packets). Offline: 0 decode errors, exact timestamps, last packet ~14 ms after
the tick at QP 20.

## L/G/C on channel 165 (`runs/20261008-232743-play`): all three fail, libdrc included, on a clean link

L 55/s, G 63/s, C ~60/s resyncs; link 2.7 % retries, 0 failed TX. And the 19:41 libdrc run that "worked" had a
static GamePad screen (the frame-content log is constant: mean RGB 46/97/96, the game was paused). So nothing has
yet been shown to work with busy content. Vanilla's receiver (which mimics the pad) requests an IDR on every frame
after a bad one until a complete IDR decodes, matching ~60 requests/s: the pad never accepts our recovery frames.
Leading hypothesis: a size limit in the pad's decoder. The console's largest chunk was 5.3 KB; ours reach 13.6 KB,
and IDRs more. Sweep now: fixed QP 32/38/44/41/47, 45 s each. (x264's VBV cap was tried offline and corrupts DRH
chunks: its row re-encodes restart a chunk's NAL mid-chunk, and its NAL list keeps superseded NALs.)

## No-Cemu QP sweep (`runs/20261008-233918-vidtest`): rejected at every QP, even 47 (2 KB frames)

So frame size is not the limit. Motion vectors compared with the console's (libavcodec export_mvs, `qpdump`'s
sibling `mvdump`): the console also points outside the picture and uses vectors up to 84 px; nothing obviously
different. Moving gameplay WAS accepted for long stretches in runs/20261008-225712-play (slow, QP 32, IDR as
libdrc, 1400-byte burst, console clock, channel 36); since then several things changed at once and every test
started with the pad already stuck asking for IDRs. Next test (`pad-video-test.sh`): exactly those settings,
no Cemu, plain idle screen and busy pattern alternating every 20 s, raw resync requests logged every ~10 s.

## Next: how the real console answers a resync (`scripts/console-resync-capture.sh`, `scripts/resync-analyze.py`)

Intra-mode tests (`runs/20261009-000618-vidtest`, `-001331`): default intra recovered on the gradient after a few
seconds; limiting x264 to 16x16 intra broke even the plain screen; banning 16x16 intra did not fix it either.
In every run the first seconds after connecting are clean; after one error the pad asks for a resync on every
frame (resync-analyze on our own pad-net capture: 37 requests in the 600 ms after one IDR) and rejects nearly
every IDR we answer with. The console's answer to a resync has never been observed, so the recorder makes the
console show it: Vanilla is the GamePad, its pipe runs with the RT5572 in a network namespace (veth to the
laptop, so the Vanilla window keeps its own ports), and every 10 s video packets are dropped for 60 ms inside the
namespace so Vanilla sends a real resync request. tcpdump records the console's reply before the drop;
`resync-analyze.py` lists each request and the console's frames after it (IDR or not, size, flags, timestamps)
and can write them as Annex-B for qpdump.

## How the real Wii U answers a resync (`runs/20261009-104908-resync`, 21 forced resyncs via Vanilla)

Every one identical: Vanilla sent one "\1\0\0\0"; the console left the next frame slot empty, sent ONE IDR in the
slot after (27-44 KB, 19-28 packets, per-macroblock QP mostly 12-26: sharp, decodes to the full Wii U Menu), left
the slot after the IDR empty as well (timestamp steps 33367 us on both sides), then carried on. No repeated
requests. So big IDRs are fine; the pad needs the extra frame period to decode one. Ours sent the next P frame in
the very next slot, so the pad was still busy with the IDR, failed, asked again, forever.

Now (pacing "clock"): a resync request leaves the current slot empty, the next slot is an IDR, the slot after it
stays empty; IDRs at full quality (ip_factor 1.0). Video stats log "slots left empty".

## First run with console-style resync (`runs/20261009-105822-play`): "by far the best it's ever run"

6 minutes connected, 2 recoveries in total (one IDR each, done). Both came right after P frames of 47-57 KB. The
console during real gameplay (Mario, `runs/20261009-104908-resync`): normal frames p50 3.1 KB, p99 18.5 KB; QP
12-20 where the picture changes, the rest skipped. Ours at flat QP 32: p99 23-31 KB, peaks 47-57 KB, softer. Now
default `rc crf` (CRF 20 + variance AQ, QP 12-40, crf_control keeps frames under 20 KB), IDRs CRF+4.

## Tearing is on the pad, not in our frames (`runs/20261009-112308-padnet` FULL=1)

All 5549 frames we sent decode cleanly and none is a partial update; a frame from the fastest flicking is whole.
Cemu's own window doesn't tear (user). During play the pad asked for 2 recoveries in ~90 s. Moving the timestamp
later (+8 ms) made the pad reject everything (runs/20261009-111956-play). So the pad draws frames as they arrive
and our last chunks arrive too late (encode ~4 ms + 10 ms spread = up to 14 ms after the tick; the console paces
chunks ~2.9 ms apart, about one screen-refresh fifth). Sweep: spread 10 / 0 / 0 + ultrafast / 4 ms.

## Measured: the console's chunk schedule on its own clock (runs/20261009-114650-resync)

`scripts/console-timing.py` maps laptop time to the console's TSF (from its beacons, lower envelope) and puts every
video packet next to its vstrm timestamp. 9657 frames (Wii U Menu / apps, p95 16 KB, p99 20 KB):

| arrival − timestamp (ms) | p5 | p50 | p95 |
|---|---|---|---|
| first packet | 5.31 | 6.03 | 6.42 |
| end of chunk 0 | 5.90 | 6.13 | 6.61 |
| end of chunk 1 | 8.84 | 9.10 | 9.57 |
| end of chunk 2 | 11.72 | 11.95 | 12.48 |
| end of chunk 3 | 14.63 | 14.90 | 15.45 |
| end of chunk 4 | 16.96 | 17.22 | 17.93 |
| IDR first / last (p50, 7) | | 4.76 / 18.27 | |

Each chunk is a ~0.1 ms burst at a fixed time after the timestamp, ~2.9 ms apart, whatever its size; the last one
lands after the next frame's timestamp. Ours (send "after", spread 10 ms) sent the first packet 3-8 ms after the
timestamp, depending on the encode time, and the last by 9-14 ms: earlier than the console, and jittering with the
encode, worst exactly in fast motion.

Sender now (send "console", default): timestamp = tick + 4 ms (lead_us; encode p99 ~8.5 ms finishes before
chunk 0 is due at timestamp + 6 ms), each chunk queued to a sender thread and sent at timestamp + 6.0 / 8.95 / 11.8 /
14.75 / 17.1 ms on the TSF. The stats line counts chunks that left over 1 ms late. Verify with pad-net's tsf.txt:
`scripts/console-timing.py runs/<padnet>/pad-video.pcap --tsf runs/<padnet>/tsf.txt`.

## Measured: the console's motion vector range (tools/qp/mvdump.c)

The pad's resync requests fell to ~0 with the console chunk schedule (runs/20261009-115630-play), but in fast
camera motion it still showed blocks stuck in place, cleared left to right once the picture held still: intra
refresh's column sweep repainting macroblocks the pad had decoded wrongly, with no error it noticed. Motion vectors,
console vs ours (vstrm-to-h264.py, then mvdump):

| | frames | max \|x\| px | max \|y\| px | MVs > 64 px | partitions |
|---|---|---|---|---|---|
| console, runs/20261009-104908-resync | 14773 | 70.00 | 55.00 | 1301 x, 0 y (none > 72) | 16x16, 16x8, 8x16, 8x8 |
| console, runs/20261009-114650-resync | 9656 | 70.00 | 55.00 (6 in one concealed frame) | 4038 x (none > 72) | same |
| ours, runs/20261009-112308-padnet | 5548 | 480.25 | 442.00 | 69k x, 83k y | 16x16 |

A hard window in the console's encoder, which the pad's decoder is presumably built for. patches/drc-x264/0004 caps
x264's search to |x| <= 70, |y| <= 55 px and refuses P_SKIP when the predicted MV is outside that
(DRCB_VIDEO_MVX / _MVY; 0 = off). video-sender-test TEST_PAN=200: vertical max 69 px uncapped, 52.75 capped.
