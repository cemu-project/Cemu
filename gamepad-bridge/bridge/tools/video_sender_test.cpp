#include <algorithm>
// video-sender-test: offline check of DrcVideoSender (docs/VIDEO-REMAKE.md). Sends synthetic frames to
// 127.0.0.1:50120/50121, receives them on the same machine, checks the vstrm framing (5 chunks, frame begin/end,
// sequence ids, payload limit) and writes an Annex-B H.264 file decodable with the pad's SPS/PPS
// (tools/qp/gamepad-sps-pps.bin + libdrc's slice-header words, h264-encoder.cpp DumpH264Frame).
// usage: DRC_TSF_SOURCE=monotonic DRCB_VIDEO_RC=crf video-sender-test OUT.264 [frames]
#include "drc_video_sender.h"
#include "pad_link.h"

#include <arpa/inet.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace drcb;

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: video-sender-test OUT.264 [frames]\n");
		return 2;
	}
	const int frames = argc > 2 ? atoi(argv[2]) : 120;
	int rx = socket(AF_INET, SOCK_DGRAM, 0);
	int rcv = 16 << 20;
	setsockopt(rx, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(50120);
	inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
	if (bind(rx, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0)
	{
		perror("bind 127.0.0.1:50120");
		return 1;
	}
	DrcVideoOptions opt = DrcVideoSender::options_from_env();
	opt.dest_ip = "127.0.0.1";
	opt.bind_50020 = false;
	DrcVideoSender s(opt);
	if (!s.start())
		return 1;

	// synthetic content: fine detail + gradient + a moving box, so CRF/AQ has something to vary QP over
	std::vector<uint8_t> yuv(size_t(kPadWidth) * kPadHeight * 3 / 2);
	FILE* out = fopen(argv[1], "wb");
	FILE* sp = fopen(getenv("SPSPPS") ? getenv("SPSPPS") : "tools/qp/gamepad-sps-pps.bin", "rb");
	uint8_t spbuf[64];
	size_t spn = sp ? fread(spbuf, 1, sizeof(spbuf), sp) : 0;
	const uint8_t sc[4] = {0, 0, 0, 1};
	fwrite(sc, 1, 4, out);
	fwrite(spbuf, 1, spn, out);

	int bad = 0, got_frames = 0, total_pkts = 0;
	uint32_t last_ts = 0; int steps = 0, steps_exact = 0, steps_off1ms = 0;
	int expect_seq = -1;
	std::vector<uint8_t> frame;
	int chunk_ends = 0;
	bool in_frame = false, frame_idr = false;
	uint8_t frame_num = 0;
	for (int f = 0; f < frames; f++)
	{
		for (uint32_t y = 0; y < kPadHeight; y++)
			for (uint32_t x = 0; x < kPadWidth; x++)
			{
				int bx = (f * 6) % kPadWidth;
				bool box = x >= uint32_t(bx) && x < uint32_t(bx) + 120 && y > 180 && y < 300;
				uint8_t v = uint8_t(((x ^ y) & 8) ? 200 : 60) / 2 + uint8_t(x * 127 / kPadWidth);
				yuv[y * kPadWidth + x] = box ? 235 : v;
			}
		memset(yuv.data() + kPadWidth * kPadHeight, 128 + (f % 40), kPadWidth * kPadHeight / 2);
		if (getenv("TEST_PAN")) // fast camera motion: a detailed picture panning TEST_PAN px per frame (x), half that (y)
		{
			// speeding up 1 px/frame per frame up to TEST_PAN px/frame (a camera swing; x264 follows it through
			// its MV predictors, as with Cemu's camera)
			const int64_t vmax = atoi(getenv("TEST_PAN")), ramp = std::min<int64_t>(f, vmax);
			const int64_t ox = ramp * (ramp + 1) / 2 + (f - ramp) * vmax, oy = ox / 2;
			for (uint32_t y = 0; y < kPadHeight; y++)
				for (uint32_t x = 0; x < kPadWidth; x++)
				{
					const int64_t X = x + ox + 100000, Y = y + oy + 100000;
					uint32_t h = uint32_t(X / 4) * 73856093u ^ uint32_t(Y / 4) * 19349663u; // 4x4-pixel noise: one true match
					h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
					yuv[y * kPadWidth + x] = uint8_t(16 + (h & 0xFF) * 219 / 255);
				}
		}
		if (getenv("TEST_NOISE")) // busy content: moving noise, so frames get big like a game's
		{
			uint32_t r = 12345u + uint32_t(f) * 2654435761u;
			for (size_t i = 0; i < size_t(kPadWidth) * kPadHeight; i += 3)
			{
				r = r * 1103515245u + 12345u;
				yuv[i] = uint8_t(yuv[i] / 2 + ((r >> 16) & 127));
			}
		}
		if (getenv("TEST_RESYNC_EVERY") && f > 0 && f % atoi(getenv("TEST_RESYNC_EVERY")) == 0)
			s.request_idr();
		s.submit(yuv.data(), yuv.size());
		// collect until this frame's frame_end arrives (or 200 ms)
		for (;;)
		{
			pollfd p{rx, POLLIN, 0};
			if (poll(&p, 1, 200) <= 0)
			{
				fprintf(stderr, "frame %d: timeout\n", f);
				bad++;
				break;
			}
			uint8_t b[4096];
			ssize_t n = recv(rx, b, sizeof(b), 0);
			if (n < 16)
				continue;
			total_pkts++;
			const int seq = ((b[0] & 3) << 8) | b[1];
			const int size = ((b[2] & 7) << 8) | b[3];
			if (expect_seq >= 0 && seq != expect_seq)
			{
				fprintf(stderr, "seq gap: got %d expected %d\n", seq, expect_seq);
				bad++;
			}
			expect_seq = (seq + 1) % 1024;
			if (size != n - 16 || size > opt.max_payload || (b[0] & 0xF0) != 0xF0)
			{
				fprintf(stderr, "bad packet: size %d n %zd\n", size, n);
				bad++;
			}
			if (b[2] & 0x40)
			{
				if (in_frame)
					bad++;
				in_frame = true;
				frame.clear();
				const uint32_t ts = (uint32_t(b[4]) << 24) | (uint32_t(b[5]) << 16) | (uint32_t(b[6]) << 8) | b[7];
				if (got_frames > 0)
				{
					const int32_t d = int32_t(ts - last_ts);
					steps++;
					steps_exact += (d == 16683 || d == 16684);
					steps_off1ms += (d < 15683 || d > 17683);
				}
				last_ts = ts;
				chunk_ends = 0;
				frame_idr = memchr(b + 8, 0x80, 8) != nullptr;
			}
			frame.insert(frame.end(), b + 16, b + 16 + size);
			if (b[2] & 0x20)
				chunk_ends++;
			if (b[2] & 0x10)
			{
				if (chunk_ends != 5)
				{
					fprintf(stderr, "frame with %d chunks\n", chunk_ends);
					bad++;
				}
				in_frame = false;
				got_frames++;
				// libdrc DumpH264Frame: slice header word, then the NAL-escaped CABAC data
				uint8_t hdr[4];
				if (frame_idr)
				{
					frame_num = 0;
					const uint8_t h[4] = {0x25, 0xb8, 0x04, 0xff};
					memcpy(hdr, h, 4);
					fwrite(sc, 1, 4, out);
					fwrite(spbuf, 1, spn, out);
				}
				else
				{
					frame_num = uint8_t(frame_num + 1);
					const uint8_t h[4] = {0x21, uint8_t(0xe0 | (frame_num >> 3)), uint8_t(0x03 | (frame_num << 5)), 0xff};
					memcpy(hdr, h, 4);
				}
				fwrite(sc, 1, 4, out);
				fwrite(hdr, 1, 4, out);
				std::vector<uint8_t> esc;
				for (size_t i = 0; i < frame.size(); i++)
				{
					if (i >= 2 && frame[i] <= 3 && esc.size() >= 2 && esc[esc.size() - 1] == 0 && esc[esc.size() - 2] == 0)
						esc.push_back(3);
					esc.push_back(frame[i]);
				}
				fwrite(esc.data(), 1, esc.size(), out);
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	s.stop();
	fclose(out);
	printf("frames sent %d, received complete %d, packets %d, problems %d; timestamp steps %d: exact 16683/4 %d, off by >1 ms %d\n", frames, got_frames, total_pkts, bad, steps, steps_exact, steps_off1ms);
	return bad ? 1 : 0;
}
