// encode-bench: TASKS.md 6.1/6.2. Synthetic 864x480 source -> libdrc's encoder (drc-x264, libdrc settings).
// Encodes as fast as it can (not paced), reports per-frame convert/encode percentiles and sizes, and
// writes the stream as Annex-B (libdrc's dump format) so it can be checked with ffmpeg.
//
// usage: encode-bench [--frames N] [--out FILE.h264]
#include "canvas.h"
#include "clock.h"
#include "drc_annexb.h"
#include "drc_encoder.h"
#include "log.h"
#include "pad_link.h"
#include "stats.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace drcb;

namespace {

// Deterministic content with motion everywhere and some fine detail, so the encoder has real work:
// scrolling gradient, moving checkerboard, bouncing box, text, a noise strip.
void draw_synthetic(Canvas& c, uint64_t f)
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
	snprintf(line, sizeof(line), "ENCODE BENCH FRAME %llu", (unsigned long long)f);
	c.text(30, 230, 4, line, {255, 255, 255});
	uint32_t seed = uint32_t(f * 2654435761u);
	for (int x = 0; x < w; x += 2)
	{
		seed = seed * 1664525u + 1013904223u;
		const uint8_t v = uint8_t(seed >> 24);
		c.rect(x, 440, 2, 30, {v, v, v});
	}
}

} // namespace

int main(int argc, char** argv)
{
	log_set_tag("encode-bench");
	int frames = 600;
	const char* out_path = "encode-bench.h264";
	for (int i = 1; i + 1 < argc; i += 2)
	{
		if (!strcmp(argv[i], "--frames"))
			frames = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--out"))
			out_path = argv[i + 1];
	}

	std::vector<uint8_t> rgba(size_t(kPadWidth) * kPadHeight * 4);
	Canvas canvas(rgba.data(), kPadWidth, kPadHeight, kPadWidth * 4);
	DrcEncoder enc;
	EncodedFrame ef;
	DrcAnnexB annexb;
	std::vector<uint8_t> stream;
	DrcAnnexB::append_headers(stream);

	StageStats st_convert("rgba->yuv420p"), st_encode("x264 encode"), st_total("convert+encode");
	std::vector<int64_t> sizes;
	size_t idr_size = 0;
	const int64_t t_start = now_ns();
	for (int f = 0; f < frames; f++)
	{
		draw_synthetic(canvas, uint64_t(f));
		const bool idr = f == 0; // libdrc: IDR first, then only on resync (src/video-streamer.cpp:193)
		if (!enc.encode(rgba.data(), idr, ef))
			return 1;
		st_convert.add_ns(ef.convert_ns);
		st_encode.add_ns(ef.encode_ns);
		st_total.add_ns(ef.convert_ns + ef.encode_ns);
		if (idr)
			idr_size = ef.data.size();
		else
			sizes.push_back(int64_t(ef.data.size()));
		annexb.append_frame(stream, ef.data.data(), ef.data.size(), idr);
	}
	const double wall = double(now_ns() - t_start) / 1e9;

	LOGI("%d frames in %.2fs = %.1f fps encode throughput (single thread, as libdrc configures it)", frames, wall,
		 frames / wall);
	st_convert.report_and_clear(wall);
	st_encode.report_and_clear(wall);
	st_total.report_and_clear(wall);
	std::sort(sizes.begin(), sizes.end());
	if (!sizes.empty())
		LOGI("frame size: IDR %zu B; P p50 %lld B, p99 %lld B, max %lld B -> %.2f Mbit/s at 60 fps (p50)", idr_size,
			 (long long)sizes[sizes.size() / 2], (long long)sizes[sizes.size() * 99 / 100], (long long)sizes.back(),
			 double(sizes[sizes.size() / 2]) * 8 * 60 / 1e6);

	FILE* fp = fopen(out_path, "wb");
	if (!fp || fwrite(stream.data(), 1, stream.size(), fp) != stream.size())
	{
		LOGE("could not write %s", out_path);
		return 1;
	}
	fclose(fp);
	LOGI("wrote %s (%zu bytes). Check: ffmpeg -v error -i %s -f null -", out_path, stream.size(), out_path);
	return 0;
}
