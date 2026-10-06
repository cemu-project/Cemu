#pragma once

// Camera sync test pattern (docs/SYNC.md in the cemu-gamepad project). Drawn into the TV picture by the
// present gate and into the GamePad frame by the sink, keyed by the same frame counter, so a high-speed
// camera filming both screens can pair frames and read the offset.
//
// Layout (any size, RGBA8): black background; the whole frame is WHITE for 2 frames out of every 60
// (the flash); the counter in large digits; and a strip of 12 boxes showing the counter in binary
// (white = 1, LSB on the right), readable even when the digits are motion-blurred.
namespace SyncPattern
{
	void Draw(uint8* rgba, uint32 width, uint32 height, uint32 stride, uint64 counter);
	bool IsFlashFrame(uint64 counter);
}
