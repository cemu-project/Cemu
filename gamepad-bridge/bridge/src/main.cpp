// cemu-gamepad-bridge: owns the GamePad link, takes DRC frames from Cemu, returns input.
// Skeleton stage (TASKS.md section 5): mock pad only; the real radio/libdrc link is Phase 0.
#include "cemu_link.h"
#include "clock.h"
#include "drc_encoder.h"
#include "dsu_server.h"
#include "idle_source.h"
#include "libdrc_pad_link.h"
#include "log.h"
#include "mock_pad_link.h"
#include "replay_pad_link.h"
#include "stats.h"
#include "uinput_pad.h"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <sys/signalfd.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace drcb;

namespace {

struct Config
{
	std::string pad = "mock"; // mock | none | real
	std::string video = "h264"; // h264: the pad's real encoded stream (libdrc encoder) | raw: RGBA, no encode
	std::string log_level = "info";
	double stats_interval_s = 10.0;
	uint32_t max_frame_width = 1920; // largest DRC framebuffer Cemu may hand over (graphic packs can upscale)
	uint32_t max_frame_height = 1080;
	bool allow_root = false;
	uint32_t dsu_port = 26760; // DSU/cemuhook server for Cemu (0 = off). Cemu's default port (DSUProviderSettings)
	bool uinput = false;       // virtual Linux gamepad, for apps other than Cemu
	std::string replay_file;   // pad=replay: HID replay file (TASKS.md 8.5)
	std::string tsf_source;    // pad=real: EXPERIMENTAL substitute for the Wi-Fi TSF (libdrc patch 0005)
	bool replay_loop = false;
};

void usage()
{
	fprintf(stderr,
			"usage: cemu-gamepad-bridge [--config FILE] [--pad mock|none|real] [--replay FILE.hid [--replay-loop]]\n"
			"                           [--video h264|raw] [--log-level debug|info|warn] [--tsf-source ptp:/dev/ptpN|monotonic]\n"
			"                           [--stats-interval SECONDS] [--dsu-port PORT|0] [--uinput] [--allow-root]\n"
			"Config file: key = value lines (pad, log_level, stats_interval_s, max_frame_width, max_frame_height).\n"
			"Default config: $XDG_CONFIG_HOME/cemu-gamepad/bridge.conf (optional).\n");
}

bool apply_option(Config& c, const std::string& key, const std::string& value)
{
	try
	{
		if (key == "pad")
			c.pad = value;
		else if (key == "video")
			c.video = value;
		else if (key == "log_level")
			c.log_level = value;
		else if (key == "stats_interval_s")
			c.stats_interval_s = std::stod(value);
		else if (key == "max_frame_width")
			c.max_frame_width = uint32_t(std::stoul(value));
		else if (key == "max_frame_height")
			c.max_frame_height = uint32_t(std::stoul(value));
		else if (key == "tsf_source")
			c.tsf_source = value;
		else if (key == "replay_file")
			c.replay_file = value;
		else if (key == "replay_loop")
			c.replay_loop = value == "1" || value == "true" || value == "on";
		else if (key == "dsu_port")
			c.dsu_port = uint32_t(std::stoul(value));
		else if (key == "uinput")
			c.uinput = value == "1" || value == "true" || value == "on";
		else
		{
			fprintf(stderr, "unknown config key: %s\n", key.c_str());
			return false;
		}
	}
	catch (const std::exception&)
	{
		fprintf(stderr, "bad value for %s: %s\n", key.c_str(), value.c_str());
		return false;
	}
	return true;
}

std::string trim(const std::string& s)
{
	const auto b = s.find_first_not_of(" \t\r\n");
	const auto e = s.find_last_not_of(" \t\r\n");
	return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

bool load_config_file(Config& c, const std::string& path, bool required)
{
	std::ifstream in(path);
	if (!in)
	{
		if (required)
			fprintf(stderr, "cannot read config %s\n", path.c_str());
		return !required;
	}
	std::string line;
	int n = 0;
	while (std::getline(in, line))
	{
		n++;
		line = trim(line.substr(0, line.find('#')));
		if (line.empty())
			continue;
		const auto eq = line.find('=');
		if (eq == std::string::npos || !apply_option(c, trim(line.substr(0, eq)), trim(line.substr(eq + 1))))
		{
			fprintf(stderr, "%s:%d: invalid line\n", path.c_str(), n);
			return false;
		}
	}
	return true;
}

std::string default_config_path()
{
	if (const char* x = getenv("XDG_CONFIG_HOME"); x && *x)
		return std::string(x) + "/cemu-gamepad/bridge.conf";
	if (const char* h = getenv("HOME"))
		return std::string(h) + "/.config/cemu-gamepad/bridge.conf";
	return {};
}

std::string effective_caps()
{
	std::ifstream in("/proc/self/status");
	std::string line;
	while (std::getline(in, line))
		if (line.rfind("CapEff:", 0) == 0)
			return trim(line.substr(7));
	return "?";
}

// Nearest-neighbour fit of an RGBA frame into the pad's 864x480, letterboxed and centered.
// Cemu's DRC framebuffer is typically 854x480, which lands 1:1 with 5 px borders.
void fit_into_pad(const uint8_t* src, uint32_t sw, uint32_t sh, uint32_t sstride, uint8_t* dst)
{
	const double scale = std::min(double(kPadWidth) / sw, double(kPadHeight) / sh);
	const uint32_t dw = std::max(1u, uint32_t(sw * scale)), dh = std::max(1u, uint32_t(sh * scale));
	const uint32_t ox = (kPadWidth - dw) / 2, oy = (kPadHeight - dh) / 2;
	memset(dst, 0, size_t(kPadWidth) * kPadHeight * 4);
	for (uint32_t y = 0; y < dh; y++)
	{
		const uint8_t* srow = src + size_t(std::min(sh - 1, uint32_t(y / scale))) * sstride;
		uint8_t* drow = dst + (size_t(oy + y) * kPadWidth + ox) * 4;
		if (dw == sw)
		{
			memcpy(drow, srow, size_t(dw) * 4);
			continue;
		}
		for (uint32_t x = 0; x < dw; x++)
			memcpy(drow + size_t(x) * 4, srow + size_t(std::min(sw - 1, uint32_t(x / scale))) * 4, 4);
	}
}

} // namespace

int main(int argc, char** argv)
{
	log_set_tag("bridge");

	Config cfg;
	std::string config_path = default_config_path();
	bool config_required = false;
	std::vector<std::pair<std::string, std::string>> cli;
	for (int i = 1; i < argc; i++)
	{
		const std::string a = argv[i];
		auto next = [&]() -> std::string {
			if (i + 1 >= argc)
			{
				usage();
				exit(2);
			}
			return argv[++i];
		};
		if (a == "--config")
			config_path = next(), config_required = true;
		else if (a == "--pad")
			cli.emplace_back("pad", next());
		else if (a == "--video")
			cli.emplace_back("video", next());
		else if (a == "--log-level")
			cli.emplace_back("log_level", next());
		else if (a == "--stats-interval")
			cli.emplace_back("stats_interval_s", next());
		else if (a == "--allow-root")
			cfg.allow_root = true;
		else if (a == "--dsu-port")
			cli.emplace_back("dsu_port", next());
		else if (a == "--uinput")
			cli.emplace_back("uinput", "on");
		else if (a == "--replay")
			cli.emplace_back("pad", "replay"), cli.emplace_back("replay_file", next());
		else if (a == "--tsf-source")
			cli.emplace_back("tsf_source", next());
		else if (a == "--replay-loop")
			cli.emplace_back("replay_loop", "on");
		else
		{
			usage();
			return a == "--help" || a == "-h" ? 0 : 2;
		}
	}
	if (!config_path.empty() && !load_config_file(cfg, config_path, config_required))
		return 2;
	for (auto& [k, v] : cli)
		if (!apply_option(cfg, k, v))
			return 2;

	if (cfg.log_level == "debug")
		log_set_level(LogLevel::Debug);
	else if (cfg.log_level == "warn")
		log_set_level(LogLevel::Warn);
	else if (cfg.log_level != "info")
	{
		fprintf(stderr, "log_level must be debug, info or warn\n");
		return 2;
	}

	// Privileges. The radio work will need CAP_NET_ADMIN, granted narrowly (file capability or a
	// systemd AmbientCapabilities= unit), never by running the whole bridge as root.
	if (geteuid() == 0 && !cfg.allow_root)
	{
		LOGE("refusing to run as root. Run as your user; the radio link will get CAP_NET_ADMIN on its own "
			 "(--allow-root overrides, for debugging only)");
		return 1;
	}
	LOGI("starting: pad=%s, effective capabilities %s", cfg.pad.c_str(), effective_caps().c_str());

	if (cfg.pad != "mock" && cfg.pad != "none" && cfg.pad != "replay" && cfg.pad != "real")
	{
		LOGE("pad must be mock, replay, none or real");
		return 2;
	}
	if (cfg.pad == "replay" && cfg.replay_file.empty())
	{
		LOGE("pad=replay needs replay_file (--replay FILE.hid)");
		return 2;
	}
	if (cfg.video != "h264" && cfg.video != "raw")
	{
		LOGE("video must be h264 or raw");
		return 2;
	}

	const std::string dir = runtime_dir();
	if (dir.empty())
		return 1;

	EventLoop loop;
	if (!loop.ok())
		return 1;

	// Clean shutdown on SIGINT/SIGTERM through the event loop, not from a signal handler.
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigs, nullptr);
	signal(SIGPIPE, SIG_IGN);
	const int sfd = signalfd(-1, &sigs, SFD_CLOEXEC);
	loop.add(sfd, [&] {
		signalfd_siginfo si;
		if (read(sfd, &si, sizeof(si)) == sizeof(si))
			LOGI("signal %u: shutting down", si.ssi_signo);
		loop.stop();
	});

	std::unique_ptr<PadLink> pad;
	if (cfg.pad == "mock")
	{
		auto mock = std::make_unique<MockPadLink>(loop, dir + "/mockpad.sock");
		if (!mock->start())
			return 1;
		pad = std::move(mock);
	}
	else if (cfg.pad == "real")
	{
		// The real GamePad through libdrc (Phase 0). Preflight fails loudly without the pad network or a TSF.
		if (!cfg.tsf_source.empty())
		{
			// For Wi-Fi drivers without get_tsf (the AX200): libdrc patch 0005 reads this instead of the TSF.
			LOGW("EXPERIMENTAL: packet timestamps from %s, NOT the Wi-Fi TSF the pad syncs to (docs/PROTOCOL.md, TSF)",
				 cfg.tsf_source.c_str());
			setenv("DRC_TSF_SOURCE", cfg.tsf_source.c_str(), 1);
		}
		auto real = std::make_unique<LibdrcPadLink>(loop);
		if (!real->start())
			return 1;
		pad = std::move(real);
	}
	std::unique_ptr<ReplayPadLink> replay; // started after the callbacks are wired

	CemuLink cemu(loop, dir + "/" + DRCB_SOCKET_NAME, cfg.max_frame_width, cfg.max_frame_height);
	if (!cemu.start())
		return 1;

	// Input outputs. DSU is how Cemu gets buttons, sticks, touch and motion (no Cemu changes needed).
	std::unique_ptr<DsuServer> dsu;
	if (cfg.dsu_port)
	{
		dsu = std::make_unique<DsuServer>(loop, uint16_t(cfg.dsu_port));
		if (!dsu->start())
			return 1; // asked for it and can't have it: fail loudly
	}
	std::unique_ptr<UinputPad> uinput;
	if (cfg.uinput)
	{
		uinput = std::make_unique<UinputPad>();
		if (!uinput->open())
			return 1;
	}

	// ---- routing state ----
	const int64_t t_start = now_ns();
	std::vector<uint8_t> pad_frame(size_t(kPadWidth) * kPadHeight * 4);
	uint64_t next_pad_frame_id = 1;
	uint64_t idle_frames = 0;
	int64_t last_cemu_frame_ns = 0;
	bool have_cemu_frame = false;
	int64_t pad_connected_ns = 0; // the idle screen confirms a (re)connection for a few seconds
	struct InFlight
	{
		uint64_t cemu_frame_id;
		int64_t t_flip_ns;
		int64_t t_pad_submit_ns;
	};
	std::unordered_map<uint64_t, InFlight> in_flight; // pad frame id -> origin
	int64_t last_pad_submit_ns = 0;
	uint64_t drops_pad = 0, cemu_frames = 0, repeats = 0;

	StageStats st_flip_to_submit("cemu flip->submit");
	StageStats st_submit_to_rx("cemu submit->bridge rx");
	StageStats st_rx_to_pad("bridge rx->fit");
	StageStats st_pad_to_presented("pad submit->presented");
	StageStats st_flip_to_presented("cemu flip->presented");
	StageStats st_pad_interval("pad frame interval");
	StageStats st_convert("rgba->yuv420p");
	StageStats st_encode("x264 encode");

	// Encoding runs synchronously on the event loop (~7 ms p50 with asm x264, docs/MEASUREMENTS.md).
	// Cost: input/control messages can wait up to one encode. No thread hop in the video path.
	// Only links that carry our bitstream get it encoded here; the real pad link encodes inside libdrc.
	const bool encode = cfg.video == "h264" && pad && pad->wants_encoded();
	std::unique_ptr<DrcEncoder> encoder;
	if (encode)
		encoder = std::make_unique<DrcEncoder>();
	EncodedFrame encoded;
	bool need_idr = true; // libdrc: IDR first, then only on resync (src/video-streamer.cpp:193)
	LOGI("video: %s", encode ? "h264 (libdrc encoder, drc-x264)" : (cfg.pad == "real" ? "RGBA -> libdrc VideoStreamer (encodes itself)" : "raw RGBA (no encode)"));

	auto submit_to_pad = [&](uint64_t cemu_frame_id, int64_t t_flip) -> bool {
		if (!pad)
			return false;
		if (!pad->can_accept())
		{
			drops_pad++; // dropped before encoding, so the reference chain stays intact
			return false;
		}
		const EncodedFrame* enc = nullptr;
		if (encode)
		{
			if (!encoder->encode(pad_frame.data(), need_idr, encoded))
			{
				LOGE("encode failed");
				return false;
			}
			need_idr = false;
			st_convert.add_ns(encoded.convert_ns);
			st_encode.add_ns(encoded.encode_ns);
			enc = &encoded;
		}
		const uint64_t id = next_pad_frame_id++;
		const int64_t t = now_ns();
		if (!pad->submit(PadFrameView{pad_frame.data(), id, t, enc}))
		{
			drops_pad++;
			if (enc)
				need_idr = true; // an encoded frame was lost after all: resync with an IDR
			return false;
		}
		if (last_pad_submit_ns)
			st_pad_interval.add_ns(t - last_pad_submit_ns);
		last_pad_submit_ns = t;
		in_flight[id] = InFlight{cemu_frame_id, t_flip, t};
		if (in_flight.size() > 64) // presented never arrived for old ones; don't grow without bound
			for (auto it = in_flight.begin(); it != in_flight.end();)
				it = (id - it->first > 32) ? in_flight.erase(it) : std::next(it);
		return true;
	};

	uint64_t content_check_counter = 0;
	cemu.on_frame = [&](const drcb_frame_submit& s, const uint8_t* px, int64_t t_rx) {
		cemu_frames++;
		// Content check (sampled, cheap): an all-black or zero-alpha stream looks "fine" in every
		// timing stat, so log what the pixels actually contain every ~5 s.
		if (content_check_counter++ % 300 == 0)
		{
			uint64_t sum[4] = {}, n = 0;
			for (uint32_t y = 0; y < s.height; y += 8)
				for (uint32_t x = 0; x < s.width; x += 8, n++)
					for (int c = 0; c < 4; c++)
						sum[c] += px[size_t(y) * s.stride + size_t(x) * 4 + c];
			LOGI("cemu frame %llu content: %ux%u mean R %.1f G %.1f B %.1f A %.1f (sampled %llu px)",
				 (unsigned long long)s.frame_id, s.width, s.height, double(sum[0]) / n, double(sum[1]) / n,
				 double(sum[2]) / n, double(sum[3]) / n, (unsigned long long)n);
		}
		st_flip_to_submit.add_ns(s.t_submit_ns - s.t_flip_ns);
		st_submit_to_rx.add_ns(t_rx - s.t_submit_ns);
		fit_into_pad(px, s.width, s.height, s.stride, pad_frame.data());
		st_rx_to_pad.add_ns(now_ns() - t_rx);
		have_cemu_frame = true;
		last_cemu_frame_ns = now_ns();
		submit_to_pad(s.frame_id, s.t_flip_ns);
	};
	cemu.on_attach_changed = [&](bool attached) {
		have_cemu_frame = false;
		if (pad)
			cemu.send_pad_status(pad->connected(), pad->source(), -1);
		LOGI(attached ? "source: cemu" : "source: idle stream");
	};

	if (pad)
	{
		pad->on_presented = [&](uint64_t pad_id, int64_t t_presented, uint32_t flags) {
			auto it = in_flight.find(pad_id);
			if (it == in_flight.end())
				return;
			st_pad_to_presented.add_ns(t_presented - it->second.t_pad_submit_ns);
			if (it->second.cemu_frame_id)
			{
				st_flip_to_presented.add_ns(t_presented - it->second.t_flip_ns);
				cemu.send_presented(it->second.cemu_frame_id, t_presented, flags);
			}
			in_flight.erase(it);
		};
		pad->on_input = [&](const drcb_input_state& s) {
			cemu.send_input(s);
			if (dsu)
				dsu->update(s, pad->connected());
			if (uinput)
				uinput->update(s);
		};
		pad->on_connection_changed = [&](bool connected) {
			if (connected)
				pad_connected_ns = now_ns();
			LOGI("pad %s (%s)", connected ? "connected" : "disconnected", pad->name());
			in_flight.clear();
			need_idr = true; // a (re)connected pad has no reference frames
			cemu.send_pad_status(connected, pad->source(), -1);
		};
	}

	if (cfg.pad == "replay")
	{
		replay = std::make_unique<ReplayPadLink>(loop, cfg.replay_file, cfg.replay_loop);
		replay->on_input = [&](const drcb_input_state& s) {
			cemu.send_input(s);
			if (dsu)
				dsu->update(s, true);
			if (uinput)
				uinput->update(s);
		};
		if (!replay->start())
			return 1;
	}

	// 60 Hz pacing tick: keeps the pad fed whenever Cemu isn't supplying frames.
	loop.add_timer(1'000'000'000LL / 60, [&] {
		// can_accept, not connected: the real link only counts as "connected" once the pad's input flows,
		// and the pad may well wait for video before sending any. Feed whatever can take frames.
		if (!pad || !pad->can_accept())
			return;
		const int64_t t = now_ns();
		if (cemu.attached() && have_cemu_frame)
		{
			// Cemu is attached but quiet (menus, loading, pause): repeat its last frame rather than
			// flashing the idle screen. Only after 100 ms, so a normal 60 fps stream is never doubled.
			if (t - last_cemu_frame_ns > 100'000'000)
			{
				repeats++;
				submit_to_pad(0, t);
			}
			return;
		}
		IdleInfo info;
		info.status_line = pad_connected_ns && t - pad_connected_ns < 4'000'000'000 ? "GAMEPAD CONNECTED"
						   : cemu.attached()                                         ? "GAME RUNNING"
																					 : "START A GAME IN CEMU";
		info.battery_level = pad->battery_level();
		info.uptime_ns = t - t_start;
		info.frame_number = idle_frames++;
		Canvas canvas(pad_frame.data(), kPadWidth, kPadHeight, kPadWidth * 4);
		// DRCB_IDLE_PATTERN=busy: the busy test pattern; =alternate: 20 s plain idle screen, 20 s busy, ...;
		// =parts: 15 s plain, 15 s busy part 1, 15 s plain, part 2, ... part 5 (render_busy_part).
		static const char* pat = getenv("DRCB_IDLE_PATTERN");
		static int last_mode = -1;
		const bool alternate = pat && !strcmp(pat, "alternate");
		const bool parts = pat && !strcmp(pat, "parts");
		// gradtest: every 25 s, 10 s plain then 15 s still gradient (matches the sender's 25 s sweep phases)
		const bool gradtest = pat && !strcmp(pat, "gradtest");
		const int64_t slot = (t - t_start) / (parts ? 15'000'000'000 : 20'000'000'000);
		int part = parts && slot % 2 == 1 ? int((slot / 2) % 5) + 1 : 0;
		if (gradtest)
			part = ((t - t_start) % 25'000'000'000) >= 10'000'000'000 ? 2 : 0;
		const bool busy = (pat && !strcmp(pat, "busy")) || (alternate && slot % 2 == 1);
		const int mode = (parts || gradtest) ? part : int(busy);
		if ((alternate || parts || gradtest) && mode != last_mode)
		{
			last_mode = mode;
			LOGI("test pattern: %s", (parts || gradtest) ? (part ? busy_part_name(part) : "plain idle screen")
										 : (busy ? "BUSY" : "plain idle screen"));
		}
		if (part)
			render_busy_part(canvas, info.frame_number, part);
		else if (busy)
			render_busy_frame(canvas, info.frame_number);
		else
			render_idle_frame(canvas, info);
		submit_to_pad(0, t);
	});

	int64_t last_report = now_ns();
	loop.add_timer(int64_t(cfg.stats_interval_s * 1e9), [&] {
		const int64_t t = now_ns();
		const double window = double(t - last_report) / 1e9;
		last_report = t;
		LOGI("stats window %.1fs: cemu %s, pad %s, cemu frames %llu, idle frames so far %llu, repeats %llu, pad drops %llu",
			 window, cemu.attached() ? "attached" : "detached", pad && pad->connected() ? "connected" : "none",
			 (unsigned long long)cemu_frames, (unsigned long long)idle_frames, (unsigned long long)repeats,
			 (unsigned long long)drops_pad);
		cemu_frames = repeats = drops_pad = 0;
		for (StageStats* s : {&st_flip_to_submit, &st_submit_to_rx, &st_rx_to_pad, &st_convert, &st_encode,
							  &st_pad_to_presented, &st_flip_to_presented, &st_pad_interval})
			s->report_and_clear(window);
	});

	LOGI("running. Ctrl-C to stop.");
	loop.run();

	cemu.shutdown();
	LOGI("stopped");
	return 0;
}
