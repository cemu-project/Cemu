#include "idle_source.h"

#include "cemu_logo.h"

#include <cmath>
#include <cstdio>

namespace drcb {

void render_idle_frame(Canvas& c, const IdleInfo& info)
{
	const Rgba bg{16, 18, 24}, fg{235, 235, 235}, dim{110, 115, 125}, accent{0, 170, 235}, red{235, 70, 60};
	c.fill(bg);
	const int w = int(c.width()), h = int(c.height());
	auto centered = [&](int y, int scale, const std::string& s, Rgba col) {
		c.text((w - Canvas::text_width(scale, s)) / 2, y, scale, s, col);
	};

	c.image((w - kCemuLogoSize) / 2, 70, kCemuLogoSize, kCemuLogoSize, kCemuLogoRgba);
	centered(260, 4, info.status_line, fg);

	// Waiting dots: one lit at a time, stepping every 20 frames (3 per second).
	const int step = int(info.frame_number / 20 % 3);
	for (int i = 0; i < 3; i++)
		c.rect(w / 2 - 40 + i * 32, 320, 16, 16, i == step ? accent : Rgba{45, 50, 60});

	// Battery, top right: outline, tip, up to 5 bars (levels 2-6), label underneath.
	const int lvl = info.battery_level;
	const int bx = w - 110, by = 24, bw = 80, bh = 34;
	const Rgba frame = lvl == 2 ? red : fg;
	c.rect(bx, by, bw, 4, frame);
	c.rect(bx, by + bh - 4, bw, 4, frame);
	c.rect(bx, by, 4, bh, frame);
	c.rect(bx + bw - 4, by, 4, bh, frame);
	c.rect(bx + bw, by + 10, 6, bh - 20, frame);
	const int bars = lvl >= 2 && lvl <= 6 ? lvl - 1 : 0;
	for (int i = 0; i < bars; i++)
		c.rect(bx + 8 + i * 14, by + 8, 10, bh - 16, lvl == 2 ? red : accent);
	if (lvl == 0)
		c.rect(bx + 8, by + 8, bw - 16, bh - 16, accent);
	const char* label = lvl == 0 ? "CHARGING" : lvl == 2 ? "LOW" : (lvl < 2 || lvl > 6) ? "BATTERY ?" : "";
	if (*label)
		c.text(bx + bw + 6 - Canvas::text_width(2, label), by + bh + 10, 2, label, lvl == 2 ? red : dim);

	centered(h - 40, 2, "WII U GAMEPAD BRIDGE FOR CEMU", dim);
}


void render_busy_frame(Canvas& c, uint64_t f)
{
	const int w = int(c.width()), h = int(c.height());
	for (int y = 0; y < h; y += 4)
	{
		const uint8_t g = uint8_t((y + f * 2) % 256);
		c.rect(0, y, w, 4, {uint8_t(g / 2), g, uint8_t(255 - g)});
	}
	const int off = int(f * 3 % 64);
	for (int y = 40; y < 200; y += 16)
		for (int x = -64; x < w; x += 16)
			if (((x + y) / 16) % 2 == 0)
				c.rect(x + off, y, 16, 16, {240, 240, 240});
	const int bx = int((std::sin(double(f) / 30.0) * 0.5 + 0.5) * (w - 120));
	const int by = int((std::cos(double(f) / 23.0) * 0.5 + 0.5) * (h - 120));
	c.rect(bx, by, 120, 120, {255, 60, 30});
	char line[48];
	snprintf(line, sizeof(line), "PAD VIDEO TEST FRAME %llu", (unsigned long long)f);
	c.text(30, 230, 4, line, {255, 255, 255});
	uint32_t seed = uint32_t(f * 2654435761u);
	for (int x = 0; x < w; x += 2)
	{
		seed = seed * 1664525u + 1013904223u;
		const uint8_t v = uint8_t(seed >> 24);
		c.rect(x, 440, 2, 30, {v, v, v});
	}
}


const char* busy_part_name(int part)
{
	switch (part)
	{
	case 1: return "scrolling gradient (whole screen moves 2 px/frame)";
	case 2: return "still gradient (no motion)";
	case 3: return "moving checkerboard (3 px/frame)";
	case 4: return "bouncing box (sub-pixel motion)";
	case 5: return "noise strip (new random pixels every frame)";
	default: return "?";
	}
}

void render_busy_part(Canvas& c, uint64_t f, int part)
{
	const int w = int(c.width()), h = int(c.height());
	c.rect(0, 0, w, h, {20, 20, 30});
	if (part == 1 || part == 2)
	{
		const uint64_t ff = part == 1 ? f : 0;
		for (int y = 0; y < h; y += 4)
		{
			const uint8_t g = uint8_t((y + ff * 2) % 256);
			c.rect(0, y, w, 4, {uint8_t(g / 2), g, uint8_t(255 - g)});
		}
	}
	else if (part == 3)
	{
		const int off = int(f * 3 % 64);
		for (int y = 40; y < 440; y += 16)
			for (int x = -64; x < w; x += 16)
				if (((x + y) / 16) % 2 == 0)
					c.rect(x + off, y, 16, 16, {240, 240, 240});
	}
	else if (part == 4)
	{
		const int bx = int((std::sin(double(f) / 30.0) * 0.5 + 0.5) * (w - 120));
		const int by = int((std::cos(double(f) / 23.0) * 0.5 + 0.5) * (h - 120));
		c.rect(bx, by, 120, 120, {255, 60, 30});
	}
	else if (part == 5)
	{
		uint32_t seed = uint32_t(f * 2654435761u);
		for (int x = 0; x < w; x += 2)
		{
			seed = seed * 1664525u + 1013904223u;
			const uint8_t v = uint8_t(seed >> 24);
			c.rect(x, 220, 2, 30, {v, v, v});
		}
	}
}

} // namespace drcb
