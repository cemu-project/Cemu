#pragma once
// The pad's video encoder: libdrc's own drc::H264Encoder (patched x264, settings in libdrc
// src/h264-encoder.cpp) fed by the same RGBA->YUV420P swscale conversion libdrc uses
// (src/video-converter.cpp: SWS_FAST_BILINEAR). Synchronous on purpose: libdrc's VideoConverter
// hands frames to a std::future, which would add a thread hop to the video path.
#include <cstdint>
#include <memory>
#include <vector>

namespace drcb {

constexpr int kDrcChunksPerFrame = 5; // libdrc include/drc/internal/h264-encoder.h kH264ChunksPerFrame

struct EncodedFrame
{
	bool idr = false;
	uint32_t chunk_size[kDrcChunksPerFrame]{};
	std::vector<uint8_t> data; // chunks back to back, raw CABAC slice data (no headers, not NAL-escaped)
	int64_t convert_ns = 0;
	int64_t encode_ns = 0;
};

class DrcEncoder
{
public:
	DrcEncoder();
	~DrcEncoder();
	// rgba: kPadWidth x kPadHeight (864x480), stride 864*4.
	bool encode(const uint8_t* rgba, bool idr, EncodedFrame& out);

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace drcb
