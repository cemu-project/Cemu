#pragma once
// The idle stream: what the pad shows whenever Cemu isn't attached. The pad powers off if starved
// (PROJECT.md, "Idle streaming"), so the bridge always sends something.
#include "canvas.h"

#include <cstdint>
#include <string>

namespace drcb {

struct IdleInfo
{
	std::string status_line; // e.g. "START A GAME IN CEMU"
	int64_t uptime_ns = 0;
	uint64_t frame_number = 0;
	int battery_level = -1; // PadLink::battery_level(): VanillaBatteryStatus 0-6, -1 unknown
};

// Draws one idle frame: Cemu logo, status, the pad's battery. Three dots step every 20 frames, so a stalled
// stream is visible by eye.
void render_idle_frame(Canvas& c, const IdleInfo& info);

// Busy moving test content (the encode-bench pattern: scrolling gradient, moving checkerboard, bouncing box,
// noise strip), for testing what the pad decodes without Cemu. DRCB_IDLE_PATTERN=busy.
void render_busy_frame(Canvas& c, uint64_t f);
// One part of the busy pattern on a plain dark background (part 1-5), to find which content the pad rejects.
const char* busy_part_name(int part);
void render_busy_part(Canvas& c, uint64_t f, int part);

} // namespace drcb
