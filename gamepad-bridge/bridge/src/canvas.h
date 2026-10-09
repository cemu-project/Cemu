#pragma once
// Minimal RGBA8 software drawing for the idle screen and test patterns. 5x7 bitmap font, uppercase.
#include <cstdint>
#include <string_view>

namespace drcb {

struct Rgba
{
	uint8_t r, g, b, a = 255;
};

class Canvas
{
public:
	Canvas(uint8_t* pixels, uint32_t width, uint32_t height, uint32_t stride)
		: m_px(pixels), m_w(width), m_h(height), m_stride(stride) {}

	uint32_t width() const { return m_w; }
	uint32_t height() const { return m_h; }

	void fill(Rgba c) { rect(0, 0, int(m_w), int(m_h), c); }
	void rect(int x, int y, int w, int h, Rgba c);
	// Draws text with each font pixel as a scale x scale block. Returns the width drawn.
	int text(int x, int y, int scale, std::string_view s, Rgba c);
	// Alpha-blends a w x h RGBA8 image (rows packed) at x, y.
	void image(int x, int y, int w, int h, const uint8_t* rgba);
	static int text_width(int scale, std::string_view s) { return int(s.size()) * 6 * scale; }

private:
	uint8_t* m_px;
	uint32_t m_w, m_h, m_stride;
};

} // namespace drcb
