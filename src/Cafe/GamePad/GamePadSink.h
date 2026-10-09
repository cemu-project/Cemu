#pragma once

// DRC sink: hands the GamePad (DRC) framebuffer to the cemu-gamepad bridge and takes its replies.
// Fork-only. Protocol: drcbridge_ipc.h (IPC v1). Settings: config/GamePadBridgeConfig.h.
//
// Threading: OnDrcFlip / BeginFrame / EndFrame / Shutdown run on the render thread. A receiver
// thread reads bridge messages; it never touches the video path except to mark slots free.

class LatteTextureView;
struct drcb_input_state;

namespace GamePadSink
{
	// Called on every DRC flip, whether or not Cemu's own GamePad window is open.
	// Manages the bridge connection and asks the renderer to capture the frame.
	void OnDrcFlip(LatteTextureView* texView);
	// Called on every TV flip: keeps the bridge connection up while the game draws nothing on the GamePad.
	void OnTvFlip(LatteTextureView* texView);

	// Used by the renderer's capture path once pixels are on the CPU side.
	// BeginFrame returns false if the frame must be dropped (not connected, or no free slot).
	struct FrameTarget
	{
		uint8* dst;
		uint32 stride;
		uint32 slot;
	};
	bool BeginFrame(uint32 width, uint32 height, FrameTarget& out);
	// counter: FrameCounter() at the DRC flip this frame came from (pairs it with the TV frame).
	void EndFrame(const FrameTarget& target, uint32 width, uint32 height, sint64 tFlipNs, uint64 counter);

	// ---- present gate / screen sync (render thread) ----
	// Counts DRC flips; the TV frame of the same emulated frame carries the same value.
	uint64 FrameCounter();
	// How long to hold the TV picture (ns): tvHoldMs, 0 unless enabled and connected. A time, not a frame
	// count: games change frame rate between scenes (Nintendo Land's title screen runs at 30), and a
	// frame-count gate held 117 ms instead of 67 there (docs/MEASUREMENTS.md).
	sint64 TvHoldNs();
	// Ring size the gate needs to cover the maximum hold at 60 Hz, plus the current frame.
	constexpr sint32 kGateRingSize = 8;
	// Camera sync pattern on both screens (enabled, connected and turned on in settings).
	bool PatternActive();
	// The gate just presented the TV picture of frame `counter`, flipped at tFlipShownNs.
	void OnTvPresent(uint64 counter, sint64 tFlipShownNs, sint64 tPresentNs);

	// Monotonic clock shared with the bridge (CLOCK_MONOTONIC, ns).
	sint64 NowNs();

	// ---- input (any thread) ----
	// The real GamePad's latest input, as the bridge sent it (DRCB_MSG_INPUT_STATE). False when none arrived in the
	// last 500 ms: pad off or out of range, bridge not connected, or no title running (the sink connects from flips).
	// Read by the "WiiUGamePad" input API (input/api/GamePadBridge).
	bool LatestInput(drcb_input_state& out);

	// Title stopped: say goodbye and disconnect.
	void Shutdown();
}
