#include "drc_encoder.h"
#include "clock.h"
#include "log.h"
#include "pad_link.h"

#include <drc/internal/h264-encoder.h>
extern "C" {
#include <libswscale/swscale.h>
}

#include <cstring>

namespace drcb {

struct DrcEncoder::Impl
{
	drc::H264Encoder encoder;
	SwsContext* sws = nullptr;
	std::vector<drc::byte> yuv = std::vector<drc::byte>(size_t(kPadWidth) * kPadHeight * 3 / 2);
};

DrcEncoder::DrcEncoder() : m(std::make_unique<Impl>())
{
	m->sws = sws_getContext(kPadWidth, kPadHeight, AV_PIX_FMT_RGBA, kPadWidth, kPadHeight, AV_PIX_FMT_YUV420P,
							SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
	if (!m->sws)
		LOGE("sws_getContext failed");
}

DrcEncoder::~DrcEncoder()
{
	if (m->sws)
		sws_freeContext(m->sws);
}

bool DrcEncoder::encode(const uint8_t* rgba, bool idr, EncodedFrame& out)
{
	if (!m->sws)
		return false;
	const int64_t t0 = now_ns();
	const uint8_t* src[1] = {rgba};
	const int src_stride[1] = {int(kPadWidth * 4)};
	uint8_t* y = m->yuv.data();
	uint8_t* u = y + size_t(kPadWidth) * kPadHeight;
	uint8_t* v = u + size_t(kPadWidth / 2) * (kPadHeight / 2);
	uint8_t* dst[3] = {y, u, v};
	const int dst_stride[3] = {int(kPadWidth), int(kPadWidth / 2), int(kPadWidth / 2)};
	sws_scale(m->sws, src, src_stride, 0, kPadHeight, dst, dst_stride);
	const int64_t t1 = now_ns();

	const drc::H264ChunkArray& chunks = m->encoder.Encode(m->yuv, idr);
	const int64_t t2 = now_ns();

	out.idr = idr;
	out.data.clear();
	for (int i = 0; i < kDrcChunksPerFrame; i++)
	{
		const auto& [ptr, size] = chunks[size_t(i)];
		out.chunk_size[i] = uint32_t(size);
		out.data.insert(out.data.end(), ptr, ptr + size);
	}
	out.convert_ns = t1 - t0;
	out.encode_ns = t2 - t1;
	return true;
}

} // namespace drcb
