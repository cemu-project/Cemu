#pragma once
// Turns the pad's headerless video chunks back into a normal H.264 Annex-B stream, exactly as libdrc's
// debug dump does (libdrc src/h264-encoder.cpp: DumpH264Headers, DumpH264Frame, NalEscape). Used by the
// mock pad (to decode with libavcodec) and the encode benchmark (to write a playable .h264).
// The SPS/PPS and slice header bytes are the fixed ones the GamePad assumes; they are not ours.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace drcb {

class DrcAnnexB
{
public:
	// SPS + PPS with start codes (libdrc DumpH264Headers).
	static void append_headers(std::vector<uint8_t>& out);
	// One frame: start code + fixed slice header (IDR, or P with frame_number) + escaped slice data
	// (libdrc DumpH264Frame). `chunks` are the raw, unescaped chunk bytes concatenated.
	void append_frame(std::vector<uint8_t>& out, const uint8_t* chunks, size_t size, bool idr);

private:
	uint32_t m_frame_number = 0;
};

} // namespace drcb
