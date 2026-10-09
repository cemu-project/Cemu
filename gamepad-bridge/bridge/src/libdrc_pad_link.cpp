#include "libdrc_pad_link.h"
#include "clock.h"
#include "drc_video_sender.h"
#include "log.h"

#include <drc/internal/audio-streamer.h>
#include <drc/internal/cmd-packet.h>
#include <drc/internal/cmd-protocol.h>
#include <drc/internal/device-config.h>
#include <drc/internal/tsf.h>
#include <drc/internal/udp.h>
#include <drc/internal/uvcuac-synchronizer.h>
#include <drc/internal/video-streamer.h>
#include <drc/streamer.h>
extern "C" {
#include <libswscale/swscale.h>
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace drcb {

namespace {

// libdrc src/streamer.cpp (file-local there): RunGenericAsyncCmd, transcribed.
void RunGenericAsyncCmd(drc::CmdClient* cli, int service, int method, const std::vector<drc::byte>& msg,
						drc::CmdState::ReplyCallback cb)
{
	drc::GenericCmdPacket pkt;
	pkt.SetFlags(drc::GenericCmdPacket::kQueryFlag);
	pkt.SetServiceId(service);
	pkt.SetMethodId(method);
	pkt.SetPayload(msg.data(), msg.size());
	cli->AsyncQuery(drc::CmdQueryType::kGenericCommand, pkt.GetBytes(), pkt.GetSize(), cb);
}

bool have_console_address() // libdrc binds everything to 192.168.1.10 (include/drc/streamer.h)
{
	ifaddrs* ifa = nullptr;
	if (getifaddrs(&ifa) != 0)
		return false;
	bool found = false;
	for (ifaddrs* p = ifa; p && !found; p = p->ifa_next)
	{
		if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || !(p->ifa_flags & IFF_UP))
			continue;
		char buf[NI_MAXHOST];
		if (getnameinfo(p->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST) == 0 &&
			strcmp(buf, "192.168.1.10") == 0)
			found = true;
	}
	freeifaddrs(ifa);
	return found;
}

constexpr int64_t kPadSilentNs = 500'000'000; // HID is 180 Hz (sc-input.rst:13); 0.5 s of silence = gone

} // namespace

struct LibdrcPadLink::Impl
{
	// Same parts, same addresses as drc::Streamer's defaults (include/drc/streamer.h).
	drc::UdpServer msg_server{drc::Streamer::kDefaultMsgBind};
	drc::AudioStreamer aud_streamer{drc::Streamer::kDefaultAudioDest};
	drc::CmdClient cmd_client{drc::Streamer::kDefaultCmdDest, drc::Streamer::kDefaultCmdBind};
	drc::VideoStreamer vid_streamer{drc::Streamer::kDefaultVideoDest, drc::Streamer::kDefaultAudioDest};
	drc::UdpServer hid_server{drc::Streamer::kDefaultInputBind};
	drc::UvcUacStateSynchronizer uvcuac{&cmd_client};
	drc::DeviceConfig device_config;

	// Our own video sender (docs/VIDEO-REMAKE.md) replaces vid_streamer unless DRCB_VIDEO=libdrc.
	std::unique_ptr<DrcVideoSender> own_video;
	bool video_active = true; // on_liveness_tick: own_video set_active, from the pad status file
	std::atomic<uint64_t> resyncs{0};
	bool libdrc_started = false;

	SwsContext* sws = nullptr;
	std::vector<drc::byte> yuv;

	// HID arrives on a libdrc thread; hand the newest state to the event loop through an eventfd.
	int event_fd = -1;
	std::mutex mutex;
	TouchCalibration calibration; // replaced by the pad's own once SyncUICConfig answers
	drcb_input_state latest{};
	bool have_latest = false;
	std::atomic<int64_t> last_hid_ns{0};
	std::atomic<uint64_t> hid_packets{0}, hid_rejected{0};
	std::atomic<int> battery_level{-1};
	int logged_battery = -2, battery_logs = 0;
	uint64_t seq = 0;
};

LibdrcPadLink::LibdrcPadLink(EventLoop& loop) : m(std::make_unique<Impl>()), m_loop(loop) {}

LibdrcPadLink::~LibdrcPadLink()
{
	if (m_started)
	{
		// Reverse of start, like drc::Streamer::Stop. Each step timed: a stop once took >15 s and systemd
		// aborted the bridge.
		int64_t t = now_ns();
		auto step = [&t](const char* what) {
			const int64_t n = now_ns();
			if (n - t > 200'000'000)
				LOGW("real pad: stopping %s took %.1f s", what, (n - t) / 1e9);
			t = n;
		};
		m->uvcuac.Stop();
		step("uvc/uac");
		m->hid_server.Stop();
		step("hid");
		if (m->own_video)
			m->own_video->stop();
		step("video sender");
		if (m->libdrc_started)
			m->vid_streamer.Stop();
		step("libdrc video");
		m->cmd_client.Stop();
		step("cmd client");
		m->aud_streamer.Stop();
		step("audio");
		m->msg_server.Stop();
		step("msg server");
	}
	if (m->sws)
		sws_freeContext(m->sws);
	if (m->event_fd >= 0)
	{
		m_loop.remove(m->event_fd);
		close(m->event_fd);
	}
}

bool LibdrcPadLink::start()
{
	// Preflight. Fail loudly instead of streaming something the pad can't use (CLAUDE.md).
	if (!have_console_address())
	{
		LOGE("real pad: no interface has 192.168.1.10. Bring up the pad network first (scripts/pad-net.sh up)");
		return false;
	}
	drc::u64 tsf = 0;
	if (drc::GetTsf(&tsf) != 0)
	{
		// libdrc's streamers ignore this error and would stamp packets with garbage (src/video-streamer.cpp
		// GetTimestamp). The TSF needs the drc-mac80211 patch or an equivalent (docs/BRINGUP.md).
		LOGE("real pad: can't read the Wi-Fi TSF for the 192.168.1.10 interface (/sys/class/net/<if>/tsf). "
			 "See docs/BRINGUP.md, \"TSF\"");
		return false;
	}
	LOGI("real pad: TSF readable (0x%016llx)", (unsigned long long)tsf);

	m->sws = sws_getContext(kPadWidth, kPadHeight, AV_PIX_FMT_RGBA, kPadWidth, kPadHeight, AV_PIX_FMT_YUV420P,
							SWS_FAST_BILINEAR, nullptr, nullptr, nullptr); // libdrc src/video-converter.cpp
	m->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (!m->sws || m->event_fd < 0)
		return false;
	m_loop.add(m->event_fd, [this] { on_input_event(); });

	// libdrc src/streamer.cpp Streamer::Start: a 4-byte "\1\0\0\0" on the msg port is a resync request.
	m->msg_server.SetReceiveCallback([this](const std::vector<drc::byte>& msg) {
		if (msg.size() == 4 && !memcmp(msg.data(), "\1\0\0\0", 4))
		{
			const uint64_t n = ++m->resyncs;
			if (n <= 5 || n % 100 == 0)
				LOGI("real pad: resync requested -> IDR (%llu so far)", (unsigned long long)n);
			if (m->own_video)
				m->own_video->request_idr(); // counts it; acts on it unless libdrc has the turn (sweep)
			if (!m->own_video || m->own_video->libdrc_turn())
				m->vid_streamer.ResyncStream();
		}
	});
	m->hid_server.SetReceiveCallback([this](const std::vector<drc::byte>& msg) {
		drcb_input_state s;
		HidExtra extra;
		std::lock_guard lock(m->mutex);
		if (!parse_hid(msg.data(), msg.size(), m->calibration, s, extra))
		{
			m->hid_rejected++;
			return;
		}
		m->hid_packets++;
		m->battery_level.store(extra.battery_level, std::memory_order_relaxed);
		s.t_received_ns = now_ns();
		m->latest = s;
		m->have_latest = true;
		m->last_hid_ns = s.t_received_ns;
		const uint64_t one = 1;
		[[maybe_unused]] auto n = write(m->event_fd, &one, sizeof(one));
	});

	const char* which = getenv("DRCB_VIDEO");
	if (!which || strcmp(which, "libdrc") != 0)
		m->own_video = std::make_unique<DrcVideoSender>(DrcVideoSender::options_from_env());
	if (m->own_video)
		m->own_video->set_input_counter(&m->hid_packets);
	else
		LOGI("video: libdrc VideoStreamer (DRCB_VIDEO=libdrc)");
	// The sweep's "L" phases hand frames to libdrc's VideoStreamer, so start it too then.
	const bool sweep = getenv("DRCB_VIDEO_SWEEP") && atoi(getenv("DRCB_VIDEO_SWEEP"));
	const bool video_ok = m->own_video ? m->own_video->start() && (!sweep || m->vid_streamer.Start())
									   : m->vid_streamer.Start();
	m->libdrc_started = !m->own_video || sweep;
	if (!m->msg_server.Start() || !m->aud_streamer.Start() || !m->cmd_client.Start() || !video_ok ||
		!m->hid_server.Start() || !m->uvcuac.Start())
	{
		LOGE("real pad: a libdrc component failed to start (ports 50010/50022/50023 on 192.168.1.10 busy?)");
		return false;
	}
	m_started = true;

	// libdrc Streamer::SyncUICConfig: fetch the pad's UIC config (service 5, method 0x06) and use its touch
	// calibration. Reply callback runs on a libdrc thread.
	RunGenericAsyncCmd(&m->cmd_client, 5, 0x06, {}, [this](bool ok, const std::vector<drc::byte>& reply) {
		if (!ok || reply.size() != 0x310)
		{
			LOGW("real pad: UIC config query failed (ok=%d, %zu bytes); keeping libdrc's default touch calibration", ok,
				 reply.size());
			return;
		}
		m->device_config.LoadFromBlob(reply.data() + 0x10, 0x300);
		std::lock_guard lock(m->mutex);
		auto& c = m->calibration;
		const auto& d = m->device_config;
		c.ref_1_x = d.GetPanelRef1()[0];
		c.ref_1_y = d.GetPanelRef1()[1];
		c.ref_2_x = d.GetPanelRef2()[0];
		c.ref_2_y = d.GetPanelRef2()[1];
		c.raw_1_x = d.GetPanelRaw1()[0];
		c.raw_1_y = d.GetPanelRaw1()[1];
		c.raw_2_x = d.GetPanelRaw2()[0];
		c.raw_2_y = d.GetPanelRaw2()[1];
		LOGI("real pad: touch calibration from the pad: ref (%d,%d)-(%d,%d) raw (%d,%d)-(%d,%d); gyro zero (%d,%d,%d)",
			 c.ref_1_x, c.ref_1_y, c.ref_2_x, c.ref_2_y, c.raw_1_x, c.raw_1_y, c.raw_2_x, c.raw_2_y,
			 d.GetGyroZer()[0], d.GetGyroZer()[1], d.GetGyroZer()[2]);
	});

	m_loop.add_timer(250'000'000, [this] { on_liveness_tick(); });
	LOGI("real pad: libdrc streamers running; waiting for the GamePad's input to arrive");
	return true;
}

int LibdrcPadLink::battery_level() const
{
	return m->battery_level.load(std::memory_order_relaxed);
}

void LibdrcPadLink::on_input_event()
{
	uint64_t n;
	[[maybe_unused]] auto r = read(m->event_fd, &n, sizeof(n));
	drcb_input_state s;
	{
		std::lock_guard lock(m->mutex);
		if (!m->have_latest)
			return;
		s = m->latest;
		m->have_latest = false;
	}
	s.seq = ++m->seq;
	if (const int b = m->battery_level.load(std::memory_order_relaxed); b != m->logged_battery)
	{
		static const char* const kNames[] = {"charging", "unknown", "very low", "low", "medium", "high", "full", "7?"};
		if (++m->battery_logs <= 50) // a reading that flickers would flood the log
			LOGI("real pad: battery %s (level %d)%s", kNames[b & 7], b, m->battery_logs == 50 ? "; no more battery logs" : "");
		m->logged_battery = b;
	}
	if (!m_connected)
		on_liveness_tick();
	if (on_input)
		on_input(s);
}

namespace {
// Written by scripts/pad-net.sh from hostapd's events: "connected" / "disconnected" (the GamePad's Wi-Fi link), or
// "down" once the pad network has stopped. No file: an older pad-net, stream regardless.
constexpr const char* kPadStatusFile = "/run/cemu-gamepad/pad";
std::string read_pad_status()
{
	FILE* f = fopen(kPadStatusFile, "r");
	if (!f)
		return {};
	char buf[32] = {};
	if (!fgets(buf, sizeof(buf), f))
		buf[0] = 0;
	fclose(f);
	std::string s(buf);
	while (!s.empty() && (s.back() == '\n' || s.back() == ' '))
		s.pop_back();
	return s;
}
}

void LibdrcPadLink::on_liveness_tick()
{
	const std::string status = read_pad_status();
	if (status == "down")
	{
		// The radio, its interface and the TSF file all go away with the pad network; start over with the next
		// one (the systemd user service restarts the bridge once 192.168.1.10 is back).
		LOGI("real pad: the pad network stopped (%s says down); exiting", kPadStatusFile);
		m_loop.stop();
		return;
	}
	const bool station = status.empty() || status == "connected";
	if (m->own_video && station != m->video_active)
	{
		m->video_active = station;
		m->own_video->set_active(station);
		LOGI("real pad: GamePad %s the Wi-Fi: video %s", station ? "joined" : "is not on", station ? "streaming" : "paused");
	}
	const bool alive = now_ns() - m->last_hid_ns.load() < kPadSilentNs;
	if (alive != m_connected)
	{
		m_connected = alive;
		LOGI("real pad: %s (HID packets so far %llu, rejected %llu)", alive ? "input flowing" : "input stopped",
			 (unsigned long long)m->hid_packets.load(), (unsigned long long)m->hid_rejected.load());
		if (on_connection_changed)
			on_connection_changed(alive);
	}
}

bool LibdrcPadLink::submit(const PadFrameView& frame)
{
	if (!m_started)
		return false;
	// libdrc VideoStreamer::PushFrame "Needs YUV420P at the right size (kScreenWidth x kScreenHeight)".
	m->yuv.resize(size_t(kPadWidth) * kPadHeight * 3 / 2);
	const uint8_t* src[1] = {frame.rgba};
	const int src_stride[1] = {int(kPadWidth * 4)};
	uint8_t* y = m->yuv.data();
	uint8_t* u = y + size_t(kPadWidth) * kPadHeight;
	uint8_t* v = u + size_t(kPadWidth / 2) * (kPadHeight / 2);
	uint8_t* dst[3] = {y, u, v};
	const int dst_stride[3] = {int(kPadWidth), int(kPadWidth / 2), int(kPadWidth / 2)};
	sws_scale(m->sws, src, src_stride, 0, kPadHeight, dst, dst_stride);
	if (m->own_video)
		m->own_video->submit(m->yuv.data(), m->yuv.size()); // encoded and sent right away on its own thread
	if (!m->own_video || m->own_video->libdrc_turn())
		m->vid_streamer.PushFrame(&m->yuv); // moves the buffer out; libdrc latches it on its next 59.94 Hz tick
	return true;
}

} // namespace drcb
