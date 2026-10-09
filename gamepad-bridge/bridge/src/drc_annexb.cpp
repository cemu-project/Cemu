#include "drc_annexb.h"

namespace drcb {

namespace {
// libdrc src/h264-encoder.cpp, DumpH264Headers.
constexpr uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};
constexpr uint8_t kGamepadSps[] = {0x67, 0x64, 0x00, 0x20, 0xac, 0x2b, 0x40, 0x6c, 0x1e, 0xf3, 0x68};
constexpr uint8_t kGamepadPps[] = {0x68, 0xee, 0x06, 0x0c, 0xe8};

void put(std::vector<uint8_t>& out, const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); }

// libdrc src/h264-encoder.cpp, NalEscape: emulation prevention.
void append_escaped(std::vector<uint8_t>& out, const uint8_t* src, size_t size)
{
	const size_t start = out.size();
	for (size_t i = 0; i < size; i++)
	{
		const size_t n = out.size() - start;
		if (n >= 2 && src[i] <= 0x03 && out[out.size() - 2] == 0 && out[out.size() - 1] == 0)
			out.push_back(0x03);
		out.push_back(src[i]);
	}
}
} // namespace

void DrcAnnexB::append_headers(std::vector<uint8_t>& out)
{
	put(out, kStartCode, sizeof(kStartCode));
	put(out, kGamepadSps, sizeof(kGamepadSps));
	put(out, kStartCode, sizeof(kStartCode));
	put(out, kGamepadPps, sizeof(kGamepadPps));
}

void DrcAnnexB::append_frame(std::vector<uint8_t>& out, const uint8_t* chunks, size_t size, bool idr)
{
	// libdrc src/h264-encoder.cpp, DumpH264Frame.
	uint8_t idr_header[] = {0x25, 0xb8, 0x04, 0xff};
	uint8_t p_header[] = {0x21, 0xe0, 0x03, 0xff};
	put(out, kStartCode, sizeof(kStartCode));
	if (idr)
	{
		m_frame_number = 0;
		put(out, idr_header, sizeof(idr_header));
	}
	else
	{
		m_frame_number = (m_frame_number + 1) & 0xFF;
		p_header[1] |= uint8_t(m_frame_number >> 3);
		p_header[2] |= uint8_t(m_frame_number << 5);
		put(out, p_header, sizeof(p_header));
	}
	append_escaped(out, chunks, size);
}

} // namespace drcb
