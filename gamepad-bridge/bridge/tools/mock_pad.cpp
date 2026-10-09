// mock-pad: stands in for the real GamePad during development (TASKS.md 5.4).
// Shows the frames the bridge sends to the pad, reports when each was drawn (FRAME_PRESENTED, flagged
// as estimated), and sends mouse/keyboard back as INPUT_STATE. Encoded frames (DRCB_FMT_DRC_H264) are
// the pad's real bitstream: rebuilt into Annex-B exactly as libdrc's debug dump does and decoded with
// libavcodec, so a stream the GamePad couldn't decode shows up here as decode errors.
//
// Mouse: left button on the screen = touch.
// Keys: arrows = D-pad, X = A, Z = B, S = X, A = Y, Q = L, W = R, 1 = ZL, 2 = ZR,
//       Enter = +, Backspace = -, H = Home, IJKL = left stick.
#include "clock.h"
#include "drc_annexb.h"
#include "log.h"
#include "shm_slots.h"
#include "socket_io.h"
#include "stats.h"

#include <SDL3/SDL.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace drcb;

namespace {

constexpr int kW = 864, kH = 480; // pad frame size, see bridge/src/pad_link.h

int connect_to(const std::string& path)
{
	int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
	if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		close(fd);
		return -1;
	}
	return fd;
}

struct KeyMap
{
	SDL_Keycode key;
	uint32_t button;
};
constexpr KeyMap kKeys[] = {
	{SDLK_UP, DRCB_BTN_UP},   {SDLK_DOWN, DRCB_BTN_DOWN},  {SDLK_LEFT, DRCB_BTN_LEFT},     {SDLK_RIGHT, DRCB_BTN_RIGHT},
	{SDLK_X, DRCB_BTN_A},     {SDLK_Z, DRCB_BTN_B},        {SDLK_S, DRCB_BTN_X},           {SDLK_A, DRCB_BTN_Y},
	{SDLK_Q, DRCB_BTN_L},     {SDLK_W, DRCB_BTN_R},        {SDLK_1, DRCB_BTN_ZL},          {SDLK_2, DRCB_BTN_ZR},
	{SDLK_RETURN, DRCB_BTN_PLUS}, {SDLK_BACKSPACE, DRCB_BTN_MINUS}, {SDLK_H, DRCB_BTN_HOME},
};

} // namespace

int main(int argc, char** argv)
{
	log_set_tag("mock-pad");
	bool vsync = true;
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--no-vsync"))
			vsync = false;
		else
		{
			fprintf(stderr, "usage: mock-pad [--no-vsync]\n(see the top of tools/mock_pad.cpp for controls)\n");
			return 2;
		}
	}

	const std::string dir = runtime_dir();
	if (dir.empty())
		return 1;
	const std::string path = dir + "/mockpad.sock";
	int sock = -1;
	for (int tries = 0; tries < 50 && sock < 0; tries++)
	{
		sock = connect_to(path);
		if (sock < 0)
			SDL_Delay(100);
	}
	if (sock < 0)
	{
		LOGE("can't connect to %s: is the bridge running with --pad mock?", path.c_str());
		return 1;
	}

	drcb_hello hello{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, uint32_t(getpid()), "mock-pad"};
	send_msg(sock, DRCB_MSG_HELLO, hello);
	Message msg;
	if (recv_msg(sock, msg) != RecvResult::Ok || msg.header.type != DRCB_MSG_WELCOME || msg.fd < 0)
	{
		LOGE("bridge did not WELCOME us (type %u)", msg.header.type);
		return 1;
	}
	auto* w = msg.as<drcb_welcome>();
	ShmSlots slots;
	if (!w || !slots.map(msg.fd, w->slot_count, w->slot_size))
		return 1;
	LOGI("attached to bridge: %u slots of %u bytes", w->slot_count, w->slot_size);

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		LOGE("SDL_Init: %s", SDL_GetError());
		return 1;
	}
	SDL_Window* win = SDL_CreateWindow("Mock GamePad (bridge)", kW, kH, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
	if (!ren)
	{
		LOGE("SDL window/renderer: %s", SDL_GetError());
		return 1;
	}
	SDL_SetRenderVSync(ren, vsync ? 1 : 0);
	SDL_SetRenderLogicalPresentation(ren, kW, kH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, kW, kH);
	// The pad has no alpha. Wii U framebuffers often carry A=0, which SDL's default alpha blending
	// would draw as black (first real-Cemu run showed only black for exactly this kind of reason).
	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
	SDL_Texture* yuv_tex = nullptr; // created on the first decoded frame (decoder output size, 854x480)

	const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	AVCodecContext* dec = codec ? avcodec_alloc_context3(codec) : nullptr;
	if (!dec)
	{
		LOGE("no H.264 decoder in this libavcodec");
		return 1;
	}
	dec->flags |= AV_CODEC_FLAG_LOW_DELAY;
	dec->thread_count = 1; // frame threading would add frames of delay
	if (avcodec_open2(dec, codec, nullptr) < 0)
	{
		LOGE("avcodec_open2 failed");
		return 1;
	}
	AVPacket* pkt = av_packet_alloc();
	AVFrame* decoded = av_frame_alloc(); // newest decoded picture
	AVFrame* scratch = av_frame_alloc(); // avcodec_receive_frame clears its frame even when it fails
	DrcAnnexB annexb;
	std::vector<uint8_t> annexb_buf;
	uint64_t decode_errors = 0, decoded_frames = 0;
	StageStats st_decode("decode");
	LOGI("window up (renderer %s, vsync %s)", SDL_GetRendererName(ren), vsync ? "on" : "off");

	drcb_input_state input{};
	bool input_dirty = true;
	int64_t last_input_send = 0;
	uint64_t frames = 0, skipped = 0;
	StageStats st_submit_to_presented("bridge submit->drawn");
	int64_t last_report = now_ns();

	bool running = true;
	while (running)
	{
		// Drain the socket; if several frames are waiting, draw only the newest (a display shows the
		// latest frame, it doesn't queue), and release the rest at once. Encoded frames are all decoded,
		// in order (each P frame references the previous one); only the newest picture is drawn.
		bool have_frame = false;
		bool have_decoded = false;
		drcb_frame_submit latest{};
		pollfd pfd{sock, POLLIN, 0};
		while (poll(&pfd, 1, have_frame ? 0 : 2) > 0)
		{
			Message m;
			const RecvResult r = recv_msg(sock, m);
			if (r != RecvResult::Ok)
			{
				LOGI("bridge went away");
				running = false;
				break;
			}
			if (m.header.type == DRCB_MSG_FRAME_SUBMIT)
			{
				auto* s = m.as<drcb_frame_submit>();
				if (!s || !slots.valid(s->slot))
					continue;
				if (s->format == DRCB_FMT_DRC_H264)
				{
					const int64_t t0 = now_ns();
					const uint8_t* p = slots.slot(s->slot);
					drcb_encoded_frame h;
					memcpy(&h, p, sizeof(h));
					size_t total = 0;
					for (uint32_t c : h.chunk_size)
						total += c;
					annexb_buf.clear();
					if (h.idr)
						DrcAnnexB::append_headers(annexb_buf);
					if (sizeof(h) + total <= s->stride)
						annexb.append_frame(annexb_buf, p + sizeof(h), total, h.idr != 0);
					send_msg(sock, DRCB_MSG_FRAME_RELEASE, drcb_frame_release{s->slot, 0});
					pkt->data = annexb_buf.data();
					pkt->size = int(annexb_buf.size());
					int got = 0;
					if (avcodec_send_packet(dec, pkt) < 0)
						decode_errors++;
					while (avcodec_receive_frame(dec, scratch) == 0)
					{
						av_frame_unref(decoded);
						av_frame_move_ref(decoded, scratch);
						got++;
					}
					if (got)
					{
						decoded_frames++;
						have_decoded = true;
						latest = *s;
						have_frame = true;
					}
					else if (decoded_frames > 0)
						decode_errors++; // low-delay decoding should give one picture per packet
					if (decode_errors && (decode_errors == 1 || decode_errors % 100 == 0))
						LOGE("decode errors so far: %llu (the real pad would show garbage or drop these)",
							 (unsigned long long)decode_errors);
					st_decode.add_ns(now_ns() - t0);
					continue;
				}
				if (have_frame)
				{
					skipped++;
					send_msg(sock, DRCB_MSG_FRAME_RELEASE, drcb_frame_release{latest.slot, 0});
				}
				latest = *s;
				have_frame = true;
			}
			else if (m.header.type == DRCB_MSG_GOODBYE)
				running = false;
		}

		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_EVENT_QUIT:
				running = false;
				break;
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP: {
				const bool down = e.type == SDL_EVENT_KEY_DOWN;
				for (auto& k : kKeys)
					if (e.key.key == k.key)
					{
						input.buttons = down ? (input.buttons | k.button) : (input.buttons & ~k.button);
						input_dirty = true;
					}
				const float v = down ? 1.0f : 0.0f;
				if (e.key.key == SDLK_J) input.stick_l[0] = -v, input_dirty = true;
				if (e.key.key == SDLK_L) input.stick_l[0] = v, input_dirty = true;
				if (e.key.key == SDLK_I) input.stick_l[1] = v, input_dirty = true;
				if (e.key.key == SDLK_K) input.stick_l[1] = -v, input_dirty = true;
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_MOTION: {
				SDL_ConvertEventToRenderCoordinates(ren, &e);
				const bool down = e.type == SDL_EVENT_MOUSE_MOTION ? (e.motion.state & SDL_BUTTON_LMASK) != 0
																   : (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
				const float x = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x;
				const float y = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y;
				if (down || input.touch_down)
				{
					input.touch_down = down;
					input.touch[0] = SDL_clamp(x / kW, 0.0f, 1.0f);
					input.touch[1] = SDL_clamp(y / kH, 0.0f, 1.0f);
					input_dirty = true;
				}
				break;
			}
			}
		}

		if (have_frame)
		{
			SDL_RenderClear(ren);
			if (have_decoded)
			{
				if (!yuv_tex)
				{
					yuv_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, decoded->width,
												decoded->height);
					LOGI("decoding the pad stream: %dx%d", decoded->width, decoded->height);
				}
				SDL_UpdateYUVTexture(yuv_tex, nullptr, decoded->data[0], decoded->linesize[0], decoded->data[1],
									 decoded->linesize[1], decoded->data[2], decoded->linesize[2]);
				// The pad shows 854 of the 864 encoded columns (its SPS crops; ffprobe reports 854x480).
				const SDL_FRect dst{float(kW - decoded->width) / 2, 0, float(decoded->width), float(decoded->height)};
				SDL_RenderTexture(ren, yuv_tex, nullptr, &dst);
			}
			else
			{
				SDL_UpdateTexture(tex, nullptr, slots.slot(latest.slot), int(latest.stride));
				// Pixels are copied into the texture; give the slot back before the (vsync-blocking) present.
				send_msg(sock, DRCB_MSG_FRAME_RELEASE, drcb_frame_release{latest.slot, 0});
				SDL_RenderTexture(ren, tex, nullptr, nullptr);
			}
			SDL_RenderPresent(ren);
			const int64_t t = now_ns();
			// "Presented" here means the present call returned: an estimate of display time, flagged as
			// such. A real pad reports something better (Task 7).
			send_msg(sock, DRCB_MSG_FRAME_PRESENTED, drcb_frame_presented{latest.frame_id, t, DRCB_PRESENTED_ESTIMATED, 0});
			st_submit_to_presented.add_ns(t - latest.t_submit_ns);
			frames++;
		}

		const int64_t t = now_ns();
		if (input_dirty || t - last_input_send > 100'000'000)
		{
			input.seq++;
			input.t_received_ns = t;
			send_msg(sock, DRCB_MSG_INPUT_STATE, input);
			input_dirty = false;
			last_input_send = t;
		}

		if (t - last_report > 10'000'000'000LL)
		{
			const double window = double(t - last_report) / 1e9;
			LOGI("drew %llu frames (%.1f/s), skipped %llu stale", (unsigned long long)frames, frames / window,
				 (unsigned long long)skipped);
			st_submit_to_presented.report_and_clear(window);
			if (decoded_frames || decode_errors)
			{
				LOGI("decoded %llu pad-stream frames, %llu decode errors", (unsigned long long)decoded_frames,
					 (unsigned long long)decode_errors);
				st_decode.report_and_clear(window);
				decoded_frames = 0;
			}
			frames = skipped = 0;
			last_report = t;
		}
	}

	send_msg(sock, DRCB_MSG_GOODBYE, drcb_goodbye{DRCB_BYE_NORMAL, 0});
	close(sock);
	SDL_Quit();
	return 0;
}
