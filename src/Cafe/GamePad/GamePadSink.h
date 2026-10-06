#pragma once

// DRC sink: hands the GamePad (DRC) framebuffer to the cemu-gamepad bridge and takes its replies.
// Fork-only. Protocol: drcbridge_ipc.h (IPC v1). Settings: config/GamePadBridgeConfig.h.
//
// Threading: OnDrcFlip / BeginFrame / EndFrame / Shutdown run on the render thread. A receiver
// thread reads bridge messages; it never touches the video path except to mark slots free.

class LatteTextureView;

namespace GamePadSink
{
	// Called on every DRC flip, whether or not Cemu's own GamePad window is open.
	// Manages the bridge connection and asks the renderer to capture the frame.
	void OnDrcFlip(LatteTextureView* texView);

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
	// Frames to hold the TV picture: round(tvHoldMs / 16.67), 0 unless enabled and connected.
	sint32 TvHoldFrames();
	// Camera sync pattern on both screens (enabled, connected and turned on in settings).
	bool PatternActive();
	// The gate just presented the TV picture of frame `counter`, flipped at tFlipShownNs.
	void OnTvPresent(uint64 counter, sint64 tFlipShownNs, sint64 tPresentNs);

	// Monotonic clock shared with the bridge (CLOCK_MONOTONIC, ns).
	sint64 NowNs();

	// Title stopped: say goodbye and disconnect.
	void Shutdown();
}
