#pragma once
// Our GamePad video sender, replacing libdrc's VideoStreamer (docs/VIDEO-REMAKE.md). Differences, each measured
// against a real Wii U's stream (runs/20261008-205950-consolevid):
//  - frame-driven: a frame is encoded as soon as it is submitted, on our own encoder thread; no 59.94 Hz timer
//    and no extra tick (libdrc sends a frame one tick after encoding it);
//  - each of the frame's 5 chunks (6 macroblock rows, sc-vstrm.rst) is packetized and sent the moment x264 hands
//    it out (nalu_process), like the console (chunk gaps ~2.9 ms) instead of one burst per frame;
//  - optional spacing between packets, larger payloads (the console uses up to 1694 bytes; libdrc 1400);
//  - rate control: libdrc's fixed QP 32 by default (bit-identical settings), or x264 CRF + adaptive quantization
//    within a QP range (the console varies QP per macroblock, 12-32 seen).
// Everything else follows libdrc src/video-streamer.cpp + src/h264-encoder.cpp + src/vstrm-packet.cpp.
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace drcb {

struct DrcVideoOptions
{
	// "veryfast", not libdrc's "slow": with the console clock, slow's IDRs (45-58 KB) took ~35 ms to encode, two
	// ticks, so they arrived late, were rejected, and the next resync brought another IDR (runs/20261008-225712-play:
	// 271 IDRs, ~4100 resyncs in 80 s, then 40+ s with none once it broke out).
	std::string preset = "veryfast";
	// "crf": per-macroblock QP like the console (Mario on the real Wii U, runs/20261009-104908-resync: QP 12-20 where
	// the picture changes, the rest skipped; normal frames p99 18.5 KB). "cqp": libdrc's flat QP 32, which made
	// both softer AND bigger frames (p99 23-31 KB, peaks 47-57 KB; the two resyncs of runs/20261009-105822-play
	// came right after such frames).
	std::string rc = "crf";
	int qp = 32;                  // cqp
	float crf = 20.0f;            // crf (crf_control raises it while frames run over max_frame_kb)
	bool aq = true;               // crf: variance adaptive quantization
	// What to do when the pad asks for a resync (msg "\1\0\0\0"): "refresh" = x264 intra refresh (the console sent
	// no IDR in 39 s, runs/20261008-205950-consolevid), "idr" = an IDR like libdrc. Requests within
	// resync_holdoff_ms of the last recovery are ignored: the pad repeats the request until it has decoded the
	// recovery, and answering every repeat with another IDR never let it catch up (runs/20261008-215859-play).
	// "after" (default): all chunks right after the encode returns, from x264's final NAL list. "progressive":
	// each chunk from x264's chunk callback (unsafe: a small chunk can leave the previous one's last bytes
	// unwritten; video-sender-test found truncated frames with static content). "next": libdrc's way: a frame's packets are
	// held and sent in one burst right before the NEXT frame is encoded (video-streamer.cpp timer: send the
	// previous tick's packets, then encode), so they leave about one frame after their timestamp.
	// "console" (default): the real Wii U's schedule, measured on its own clock (scripts/console-timing.py,
	// runs/20261009-114650-resync, 9657 frames): each chunk goes out as one back-to-back burst at a fixed time
	// after the frame's timestamp, whatever its size: chunk 0 at +6.0 ms, then every ~2.9 ms, chunk 4 at
	// +17.1 ms (p5-p95 within about +-0.5 ms). Chunk k is sent at timestamp + kConsoleChunkUs[k] on the TSF.
	std::string send_mode = "console";
	// "console" mode: the timestamp is this far after the tick the frame was encoded at, so the encode (p99
	// ~8.5 ms) is done before chunk 0 is due at timestamp + 6 ms. Timestamps stay on the exact 59.94 Hz grid.
	int lead_us = 4000;
	// "clock": like the console, one frame every 16683.35 us (59.94 Hz) on the pad's clock (TSF), timestamps on
	// that exact grid; each tick encodes the newest Cemu frame (the last one again if none is new). The console's
	// timestamps step by exactly 16683/16684 us (1159 of 1164 frames); libdrc's timer stays within 1 ms; stamping
	// at Cemu's delivery time ("frame") jittered by up to ~16 ms and the pad rejected nearly every frame.
	std::string pacing = "clock";
	// "idr": after an error the pad waits for an IDR; with intra refresh only it kept asking and dropped the link
	// after ~20 s (runs/20261008-230543-play). The IDR is kept small and quick by ip_factor below.
	std::string resync = "idr";
	// IDR quality relative to P frames (x264 f_ip_factor; I-frame QP = P QP - 6*log2(ip)). libdrc uses 1.0; 0.5
	// makes IDRs 6 QP coarser: roughly half the bytes and encode time (slow QP 32 IDRs of Cemu's picture were
	// 45-58 KB and ~35 ms, two ticks late). Intra refresh then sharpens the picture again within ~0.5 s.
	float ip_factor = 1.0f; // the console's IDRs are full quality (QP mostly 12-26, runs/20261009-104908-resync)
	// x264 b_drh_chunk_slices (patches/drc-x264/0001): no prediction across 6-row chunk boundaries. Off: Vanilla
	// decodes the console's busy streams as one ordinary slice, so the console doesn't do this. For tests only.
	bool chunk_slices = false;
	// Intra restrictions for tests (patches/drc-x264/0002): no 4x4 intra blocks; allowed Intra16x16 / chroma
	// directions (bit per V,H,DC / DC,H,V; 0 = all).
	bool i4x4 = true;
	int i16x16_mask = 0, chroma_mask = 0;
	bool no_i16x16 = false; // patches/drc-x264/0003: 4x4 intra only
	// Longest motion vector, full pixels (patches/drc-x264/0004; 0 = H.264's limits). The console never exceeds
	// 70 px horizontally / 55 px vertically (24,430 frames, tools/qp/mvdump.c); x264 reached 480 px in fast
	// motion, and there the pad showed blocks stuck until intra refresh repainted them.
	int mv_range_x = 70, mv_range_y = 55;
	int resync_holdoff_ms = 250;
	int qp_min = 12, qp_max = 40; // crf: allowed per-macroblock QP range
	float idr_crf_offset = 4.0f;  // crf: IDRs this much coarser than normal frames
	int max_frame_kb = 20;        // crf: frame size target for crf_control (console normal frames: p99 18.5 KB)
	// x264 VBV hard cap per frame in KB (0 = off). Safe only when chunks are sent after the encode (send_mode
	// "after"/"next"): VBV re-encodes rows, which corrupted chunks already sent in progressive mode.
	int vbv_kb = 0;
	int max_payload = 1694;       // bytes per vstrm packet: the console's largest (libdrc kMaxVstrmPayloadSize 1400); fewer packets per frame
	int packet_gap_us = 0;        // spacing between packets of a chunk (0 = as fast as possible)
	// "after" mode: spread a frame's packets evenly over this many us, like the console (big frames: 11.8 ms
	// first to last packet, never more than 16 packets; we burst up to 45 packets in 0.06 ms, and the pad
	// rejected nearly every big frame, libdrc's bursts included). 0 = burst.
	int spread_us = 10000;
	bool bind_50020 = true;       // send from the console's video port (192.168.1.10:50020); libdrc: ephemeral
	int ts_offset_us = 0;         // added to the vstrm/astrm timestamp (presentation time vs TSF; console unknown)
	bool sweep = false;           // DRCB_VIDEO_SWEEP=1: cycle cap/offset/gap phases, log resyncs per phase
	std::string dest_ip = "192.168.1.11"; // the pad (libdrc Streamer::kDefaultVideoDest); tests use 127.0.0.1
};

struct DrcVideoStats
{
	int64_t encode_ns = 0;   // x264 time for the last frame
	int64_t first_chunk_ns = 0; // submit -> first chunk on the wire
	int64_t last_chunk_ns = 0;  // submit -> last chunk on the wire
	uint32_t frame_bytes = 0;
	uint32_t packets = 0;
	bool idr = false;
};

class DrcVideoSender
{
public:
	explicit DrcVideoSender(const DrcVideoOptions& opt);
	~DrcVideoSender();
	bool start(); // socket + encoder thread
	void stop();
	// yuv: YUV420P kPadWidth x kPadHeight (864x480, libdrc screen.h). Copied; the newest frame wins if the encoder is busy.
	void submit(const uint8_t* yuv, size_t size);
	void request_idr(); // the pad asked for a resync (msg port "\1\0\0\0")
	// false: encode and send nothing (no GamePad on the network); true again: restart with an init IDR
	void set_active(bool active);
	// Sweep only: during an "L" phase the caller feeds libdrc's own VideoStreamer instead (in-session control).
	bool libdrc_turn() const;
	// Sweep: the pad's input packet counter, so each phase also reports whether the pad was connected at all.
	void set_input_counter(const std::atomic<uint64_t>* c);
	// Called on the encoder thread after each frame is fully sent.
	void set_on_frame(void (*cb)(void* user, const DrcVideoStats&), void* user);
	static DrcVideoOptions options_from_env(); // DRCB_VIDEO_* overrides, see drc_video_sender.cpp

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace drcb
