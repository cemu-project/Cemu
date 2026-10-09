// fake-cemu: the stub Cemu side (TASKS.md 5.3, "against a stub"). Connects to the bridge exactly as
// the Cemu sink will, sends synthetic 854x480 DRC frames at 60 Hz, and checks what comes back.
//
// usage: fake-cemu [--seconds N] [--width W --height H]
// Exit status 0 only if the handshake worked and FRAME_PRESENTED came back for submitted frames.
#include "canvas.h"
#include "clock.h"
#include "log.h"
#include "shm_slots.h"
#include "socket_io.h"
#include "stats.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>

using namespace drcb;

int main(int argc, char** argv)
{
	log_set_tag("fake-cemu");
	double seconds = 15;
	uint32_t fw = 854, fh = 480; // Cemu's usual DRC framebuffer size
	for (int i = 1; i + 1 < argc; i += 2)
	{
		if (!strcmp(argv[i], "--seconds"))
			seconds = atof(argv[i + 1]);
		else if (!strcmp(argv[i], "--width"))
			fw = uint32_t(atoi(argv[i + 1]));
		else if (!strcmp(argv[i], "--height"))
			fh = uint32_t(atoi(argv[i + 1]));
	}

	const std::string dir = runtime_dir();
	if (dir.empty())
		return 1;
	const std::string path = dir + "/" + DRCB_SOCKET_NAME;
	int sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	sockaddr_un addr{};
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
	if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		// This is the moment the real sink shows its on-screen notice.
		LOGE("GamePad bridge not running (%s: %s)", path.c_str(), strerror(errno));
		return 1;
	}

	drcb_hello hello{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, uint32_t(getpid()), "fake-cemu"};
	send_msg(sock, DRCB_MSG_HELLO, hello);
	Message msg;
	if (recv_msg(sock, msg) != RecvResult::Ok)
		return 1;
	if (msg.header.type == DRCB_MSG_REJECT)
	{
		auto* r = msg.as<drcb_reject>();
		LOGE("bridge rejected us: %.*s", int(sizeof(r->text)), r ? r->text : "?");
		return 1;
	}
	auto* w = msg.as<drcb_welcome>();
	ShmSlots slots;
	if (msg.header.type != DRCB_MSG_WELCOME || !w || msg.fd < 0 || !slots.map(msg.fd, w->slot_count, w->slot_size))
	{
		LOGE("bad WELCOME");
		return 1;
	}
	LOGI("attached: bridge IPC %u.%u, pad %s (source %u), %u slots", w->version_major, w->version_minor,
		 w->pad_connected ? "connected" : "not connected", w->pad_source, w->slot_count);
	if (fw > w->max_width || fh > w->max_height || size_t(fw) * fh * 4 > w->slot_size)
	{
		LOGE("frame %ux%u exceeds bridge limits %ux%u", fw, fh, w->max_width, w->max_height);
		return 1;
	}

	StageStats st_flip_to_presented("flip->presented (rtt)");
	std::unordered_map<uint64_t, int64_t> flip_times;
	uint64_t frame_id = 0, submitted = 0, dropped = 0, presented = 0, inputs = 0;
	uint32_t last_buttons = 0;
	uint32_t last_touch = 0;

	const int64_t period = 1'000'000'000 / 60;
	const int64_t t_end = now_ns() + int64_t(seconds * 1e9);
	int64_t next = now_ns();
	while (now_ns() < t_end)
	{
		// Handle everything the bridge sent until the next frame is due.
		for (;;)
		{
			const int64_t wait_ms = (next - now_ns()) / 1'000'000;
			pollfd pfd{sock, POLLIN, 0};
			if (poll(&pfd, 1, int(wait_ms > 0 ? wait_ms : 0)) <= 0)
				break;
			Message m;
			if (recv_msg(sock, m) != RecvResult::Ok)
			{
				LOGE("bridge went away");
				return 1;
			}
			switch (m.header.type)
			{
			case DRCB_MSG_FRAME_RELEASE:
				if (auto* r = m.as<drcb_frame_release>(); r && slots.valid(r->slot))
					slots.set_in_use(r->slot, false);
				break;
			case DRCB_MSG_FRAME_PRESENTED:
				if (auto* p = m.as<drcb_frame_presented>())
				{
					presented++;
					if (auto it = flip_times.find(p->frame_id); it != flip_times.end())
					{
						st_flip_to_presented.add_ns(p->t_presented_ns - it->second);
						flip_times.erase(it);
					}
				}
				break;
			case DRCB_MSG_INPUT_STATE:
				if (auto* s = m.as<drcb_input_state>())
				{
					inputs++;
					if (s->buttons != last_buttons || s->touch_down != last_touch)
						LOGI("input: buttons 0x%05x  touch %s (%.3f, %.3f)  stick L (%.1f, %.1f)", s->buttons,
							 s->touch_down ? "down" : "up  ", s->touch[0], s->touch[1], s->stick_l[0], s->stick_l[1]);
					last_buttons = s->buttons;
					last_touch = s->touch_down;
				}
				break;
			case DRCB_MSG_PAD_STATUS:
				if (auto* s = m.as<drcb_pad_status>())
					LOGI("pad status: %s (source %u)", s->connected ? "connected" : "disconnected", s->source);
				break;
			case DRCB_MSG_GOODBYE:
				LOGI("bridge said goodbye");
				return 1;
			}
		}

		// "Flip": draw a frame into a free slot, or drop it. Never wait for a slot (docs/IPC.md).
		const int64_t t_flip = now_ns();
		frame_id++;
		const int slot = slots.find_free();
		if (slot < 0)
			dropped++;
		else
		{
			Canvas c(slots.slot(uint32_t(slot)), fw, fh, fw * 4);
			c.fill({40, 10, 60});
			char line[48];
			snprintf(line, sizeof(line), "FAKE CEMU %llu", (unsigned long long)frame_id);
			c.text(40, 60, 5, line, {255, 255, 255});
			c.text(40, 140, 3, "DRC TEST PATTERN", {255, 180, 0});
			c.rect(int((frame_id * 4) % (fw + 48)) - 48, 400, 48, 24, {255, 180, 0});
			slots.set_in_use(uint32_t(slot), true);
			drcb_frame_submit s{};
			s.slot = uint32_t(slot);
			s.format = DRCB_FMT_RGBA8;
			s.frame_id = frame_id;
			s.width = fw;
			s.height = fh;
			s.stride = fw * 4;
			s.t_flip_ns = t_flip;
			s.t_submit_ns = now_ns();
			send_msg(sock, DRCB_MSG_FRAME_SUBMIT, s);
			flip_times[frame_id] = t_flip;
			submitted++;
			if (flip_times.size() > 120)
				flip_times.erase(flip_times.begin());
		}
		next += period;
	}

	LOGI("done: %llu submitted, %llu dropped (no free slot), %llu presented, %llu input messages",
		 (unsigned long long)submitted, (unsigned long long)dropped, (unsigned long long)presented,
		 (unsigned long long)inputs);
	st_flip_to_presented.report_and_clear(seconds);
	send_msg(sock, DRCB_MSG_GOODBYE, drcb_goodbye{DRCB_BYE_NORMAL, 0});
	close(sock);
	return presented > 0 ? 0 : 1;
}
