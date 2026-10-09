#include "Cafe/GamePad/GamePadSink.h"
#include "Cafe/GamePad/SyncPattern.h"
#include "Cafe/GamePad/drcbridge_ipc.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "config/GamePadBridgeConfig.h"
#include "util/helpers/helpers.h"

#if BOOST_OS_LINUX
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace GamePadSink
{
	namespace
	{
		constexpr sint64 kRetryIntervalNs = 5'000'000'000; // reconnect attempt while enabled but down
		constexpr sint64 kMirrorAfterNs = 1'000'000'000; // no GamePad frame this long: show the TV picture
		constexpr uint32 kPadWidth = 854, kPadHeight = 480;
		constexpr sint64 kInputStaleNs = 500'000'000; // the pad sends input ~180 times a second

		// latest input: written by the receiver thread, read by the input thread
		std::mutex inputMutex;
		drcb_input_state latestInput{};
		sint64 latestInputNs = 0; // 0: none since connecting

		struct State
		{
			int sock = -1;
			bool ready = false; // WELCOME received, slots mapped
			uint8* shm = nullptr;
			size_t shmSize = 0;
			uint32 slotCount = 0;
			uint32 slotSize = 0;
			uint32 maxW = 0, maxH = 0;
			std::unique_ptr<std::atomic<bool>[]> slotBusy; // true = owned by the bridge
			std::thread receiver;
			std::atomic<bool> lostConnection{false};
			std::atomic<bool> padConnected{false};
			uint64 nextFrameId = 1;
			sint64 lastAttemptNs = 0;
			bool noticeShownForOutage = false;
			bool loggedUnsupportedRenderer = false;
			// counters, reported at disconnect
			uint64 submitted = 0, droppedNoSlot = 0, droppedCapture = 0, presented = 0;
			std::atomic<uint64> presentedAtomic{0};
			std::atomic<uint64> frameCounter{0};
			sint64 lastDrcFlipNs = 0; // 0: the game hasn't drawn on the GamePad yet
			bool mirroring = false;
		} s;

		// ---- screen sync measurement: pair TV and pad display times by frame counter ----
		constexpr size_t kSyncRing = 512;
		struct SyncRecord
		{
			uint64 counter = ~0ull;
			sint64 tvPresentNs = 0;
			sint64 padPresentedNs = 0;
		};
		struct SyncState
		{
			std::mutex mutex; // render thread (TV side) vs receiver thread (pad side)
			SyncRecord rec[kSyncRing];
			uint64 frameIdToCounter[kSyncRing]{}; // DRC frame_id -> counter
			std::vector<sint64> holdNs, skewNs; // skew = TV present - pad presented (positive: TV later)
			sint64 lastReportNs = 0;
		} sync;

		SyncRecord& SyncRec(uint64 counter)
		{
			SyncRecord& r = sync.rec[counter % kSyncRing];
			if (r.counter != counter)
				r = SyncRecord{counter};
			return r;
		}

		void ReportPercentiles(const char* name, std::vector<sint64>& v)
		{
			if (v.empty())
			{
				cemuLog_log(LogType::Force, "GamePad sync: {:<34} n=0", name);
				return;
			}
			std::sort(v.begin(), v.end());
			auto pct = [&](double p) { return double(v[std::min(v.size() - 1, size_t(p / 100.0 * (v.size() - 1) + 0.5))]) / 1e6; };
			cemuLog_log(LogType::Force, "GamePad sync: {:<34} n={:<5} min {:7.2f}  p50 {:7.2f}  p95 {:7.2f}  p99 {:7.2f}  max {:7.2f} ms",
						name, v.size(), double(v.front()) / 1e6, pct(50), pct(95), pct(99), double(v.back()) / 1e6);
			v.clear();
		}

		void Notice(const std::string& text, sint32 durationMs)
		{
			cemuLog_log(LogType::Force, "GamePad Bridge: {}", text);
			LatteOverlay_pushNotification(text, durationMs);
		}

#if BOOST_OS_LINUX
		bool SendMsg(uint16 type, const void* payload, uint32 size)
		{
			uint8 buf[DRCB_MAX_PACKET];
			drcb_header h{DRCB_MAGIC, DRCB_VERSION_MAJOR, type, size, 0};
			memcpy(buf, &h, sizeof(h));
			memcpy(buf + sizeof(h), payload, size);
			return send(s.sock, buf, sizeof(h) + size, MSG_NOSIGNAL) == ssize_t(sizeof(h) + size);
		}

		// Receives one packet. Returns payload size, or -1 on close/error. fdOut gets a passed fd, if any.
		int RecvMsg(drcb_header& h, uint8* payload, int* fdOut)
		{
			uint8 buf[DRCB_MAX_PACKET];
			iovec iov{buf, sizeof(buf)};
			alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))];
			msghdr msg{};
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = control;
			msg.msg_controllen = sizeof(control);
			ssize_t n;
			do
				n = recvmsg(s.sock, &msg, MSG_CMSG_CLOEXEC);
			while (n < 0 && errno == EINTR);
			if (n < ssize_t(sizeof(drcb_header)) || (msg.msg_flags & MSG_TRUNC))
				return -1;
			for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c))
				if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS && fdOut)
					memcpy(fdOut, CMSG_DATA(c), sizeof(int));
			memcpy(&h, buf, sizeof(h));
			if (h.magic != DRCB_MAGIC || sizeof(h) + h.payload_size != size_t(n))
				return -1;
			memcpy(payload, buf + sizeof(h), h.payload_size);
			return int(h.payload_size);
		}

		void ReceiverThread()
		{
			SetThreadName("GamePadBridgeRx");
			uint8 payload[DRCB_MAX_PACKET];
			for (;;)
			{
				drcb_header h;
				const int n = RecvMsg(h, payload, nullptr);
				if (n < 0)
					break;
				switch (h.type)
				{
				case DRCB_MSG_FRAME_RELEASE:
					if (n >= int(sizeof(drcb_frame_release)))
					{
						const uint32 slot = reinterpret_cast<drcb_frame_release*>(payload)->slot;
						if (slot < s.slotCount)
							s.slotBusy[slot].store(false, std::memory_order_release);
					}
					break;
				case DRCB_MSG_FRAME_PRESENTED:
					s.presentedAtomic.fetch_add(1, std::memory_order_relaxed);
					if (n >= int(sizeof(drcb_frame_presented)))
					{
						auto* p = reinterpret_cast<drcb_frame_presented*>(payload);
						std::lock_guard lock(sync.mutex);
						const uint64 counter = sync.frameIdToCounter[p->frame_id % kSyncRing];
						SyncRecord& r = SyncRec(counter);
						r.padPresentedNs = p->t_presented_ns;
						if (r.tvPresentNs)
							sync.skewNs.push_back(r.tvPresentNs - r.padPresentedNs);
					}
					break;
				case DRCB_MSG_PAD_STATUS:
					if (n >= int(sizeof(drcb_pad_status)))
						s.padConnected = reinterpret_cast<drcb_pad_status*>(payload)->connected != 0;
					break;
				case DRCB_MSG_INPUT_STATE:
					if (n >= int(sizeof(drcb_input_state)))
					{
						std::lock_guard lock(inputMutex);
						memcpy(&latestInput, payload, sizeof(drcb_input_state));
						const sint64 now = NowNs();
						if (latestInputNs == 0 || now - latestInputNs > kInputStaleNs)
							cemuLog_log(LogType::Force, "GamePad Bridge: GamePad input arriving");
						latestInputNs = now;
					}
					break;
				case DRCB_MSG_GOODBYE:
					s.lostConnection = true;
					return;
				default:
					break;
				}
			}
			s.lostConnection = true;
		}

		void Disconnect(bool sendGoodbye)
		{
			{
				std::lock_guard lock(inputMutex);
				latestInputNs = 0;
			}
			if (s.sock < 0)
				return;
			if (sendGoodbye)
			{
				drcb_goodbye bye{DRCB_BYE_NORMAL, 0};
				SendMsg(DRCB_MSG_GOODBYE, &bye, sizeof(bye));
			}
			shutdown(s.sock, SHUT_RDWR); // unblocks the receiver
			if (s.receiver.joinable())
				s.receiver.join();
			close(s.sock);
			s.sock = -1;
			if (s.shm)
				munmap(s.shm, s.shmSize);
			s.shm = nullptr;
			s.ready = false;
			s.lostConnection = false;
			s.presented = s.presentedAtomic.exchange(0);
			cemuLog_log(LogType::Force, "GamePad Bridge: disconnected. Frames submitted {}, dropped (no free slot) {}, "
										"dropped (capture busy) {}, presented {}",
						s.submitted, s.droppedNoSlot, s.droppedCapture, s.presented);
			s.submitted = s.droppedNoSlot = s.droppedCapture = 0;
		}

		// Returns true when connected and WELCOMEd. Shows the on-screen notice on failure.
		bool TryConnect()
		{
			const char* xdg = getenv("XDG_RUNTIME_DIR");
			if (!xdg || !*xdg)
			{
				Notice("GamePad Bridge: XDG_RUNTIME_DIR is not set, can't find the bridge. Playing without the GamePad.", 10000);
				return false;
			}
			sockaddr_un addr{};
			addr.sun_family = AF_UNIX;
			snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s/%s", xdg, DRCB_SOCKET_DIR, DRCB_SOCKET_NAME);

			s.sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
			if (s.sock < 0 || connect(s.sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
			{
				if (s.sock >= 0)
					close(s.sock);
				s.sock = -1;
				return false;
			}

			drcb_hello hello{DRCB_VERSION_MAJOR, DRCB_VERSION_MINOR, uint32(getpid()), "Cemu"};
			SendMsg(DRCB_MSG_HELLO, &hello, sizeof(hello));

			// The bridge answers immediately; don't let a wedged bridge hang the render thread.
			timeval tv{2, 0};
			setsockopt(s.sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			drcb_header h;
			uint8 payload[DRCB_MAX_PACKET];
			int fd = -1;
			const int n = RecvMsg(h, payload, &fd);
			timeval none{0, 0};
			setsockopt(s.sock, SOL_SOCKET, SO_RCVTIMEO, &none, sizeof(none));

			if (n >= 0 && h.type == DRCB_MSG_REJECT && n >= int(sizeof(drcb_reject)))
			{
				auto* r = reinterpret_cast<drcb_reject*>(payload);
				Notice(fmt::format("GamePad Bridge refused the connection: {}. Playing without the GamePad.",
								   std::string(r->text, strnlen(r->text, sizeof(r->text)))), 10000);
				close(s.sock);
				s.sock = -1;
				return false;
			}
			if (n < int(sizeof(drcb_welcome)) || h.type != DRCB_MSG_WELCOME || fd < 0)
			{
				if (fd >= 0)
					close(fd);
				close(s.sock);
				s.sock = -1;
				Notice("GamePad Bridge did not answer correctly. Playing without the GamePad.", 10000);
				return false;
			}
			auto* w = reinterpret_cast<drcb_welcome*>(payload);
			s.shmSize = size_t(w->slot_count) * w->slot_size;
			void* p = mmap(nullptr, s.shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			close(fd);
			if (p == MAP_FAILED)
			{
				close(s.sock);
				s.sock = -1;
				Notice("GamePad Bridge: could not map frame memory. Playing without the GamePad.", 10000);
				return false;
			}
			s.shm = static_cast<uint8*>(p);
			s.slotCount = w->slot_count;
			s.slotSize = w->slot_size;
			s.maxW = w->max_width;
			s.maxH = w->max_height;
			s.slotBusy = std::make_unique<std::atomic<bool>[]>(s.slotCount);
			for (uint32 i = 0; i < s.slotCount; i++)
				s.slotBusy[i] = false;
			s.padConnected = w->pad_connected != 0;
			s.nextFrameId = 1;
			s.lostConnection = false;
			s.ready = true;
			s.receiver = std::thread(ReceiverThread);

			Notice(s.padConnected ? "GamePad Bridge connected" : "GamePad Bridge connected (no GamePad attached yet)", 3000);
			return true;
		}
#endif
	}

	sint64 NowNs()
	{
#if BOOST_OS_LINUX
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return sint64(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
#else
		return 0;
#endif
	}

	uint64 FrameCounter()
	{
		return s.frameCounter.load(std::memory_order_relaxed);
	}

	sint64 TvHoldNs()
	{
		const auto& cfg = GetGamePadBridgeConfig();
		if (!cfg.enabled || !s.ready)
			return 0;
		return sint64(std::clamp<sint32>(cfg.tvHoldMs, 0, GamePadBridgeConfig::kMaxTvHoldMs)) * 1'000'000;
	}

	bool PatternActive()
	{
		const auto& cfg = GetGamePadBridgeConfig();
		return cfg.enabled && cfg.syncTestPattern && s.ready;
	}

	void OnTvPresent(uint64 counter, sint64 tFlipShownNs, sint64 tPresentNs)
	{
		std::lock_guard lock(sync.mutex);
		sync.holdNs.push_back(tPresentNs - tFlipShownNs);
		SyncRecord& r = SyncRec(counter);
		r.tvPresentNs = tPresentNs;
		if (r.padPresentedNs)
			sync.skewNs.push_back(r.tvPresentNs - r.padPresentedNs);
		if (sync.lastReportNs == 0)
			sync.lastReportNs = tPresentNs;
		if (tPresentNs - sync.lastReportNs >= 10'000'000'000)
		{
			cemuLog_log(LogType::Force, "GamePad sync: TV hold setting {} ms, pattern {}", TvHoldNs() / 1'000'000,
						PatternActive() ? "on" : "off");
			ReportPercentiles("TV hold (flip -> present call)", sync.holdNs);
			// Both ends are estimates: TV = present call (FIFO adds up to a frame), pad = mock pad's
			// "drawn". The camera method (docs/SYNC.md) is the real measurement.
			ReportPercentiles("TV present - pad presented (skew)", sync.skewNs);
			sync.lastReportNs = tPresentNs;
		}
	}

#if BOOST_OS_LINUX
	// Connect / reconnect / notice handling, from any flip. True when frames can go to the bridge.
	static bool MaintainConnection()
	{
		if (!GetGamePadBridgeConfig().enabled)
		{
			if (s.sock >= 0)
				Disconnect(true);
			s.noticeShownForOutage = false;
			return false;
		}

		if (s.ready && s.lostConnection)
		{
			Disconnect(false);
			Notice("GamePad Bridge disconnected. Playing without the GamePad.", 10000);
			s.noticeShownForOutage = true;
			s.lastAttemptNs = NowNs();
		}

		if (!s.ready)
		{
			const sint64 now = NowNs();
			if (s.lastAttemptNs != 0 && now - s.lastAttemptNs < kRetryIntervalNs)
				return false;
			s.lastAttemptNs = now;
			if (!TryConnect())
			{
				// Visible, not just logged (CLAUDE.md: don't silently fall back). Once per outage.
				if (!s.noticeShownForOutage)
					Notice("GamePad Bridge is not running. Playing without the GamePad.", 10000);
				s.noticeShownForOutage = true;
				return false;
			}
			s.noticeShownForOutage = false;
		}
		return true;
	}
#endif

#if BOOST_OS_LINUX
	static void Capture(LatteTextureView* texView, uint32 fitW, uint32 fitH)
	{
		s.frameCounter.fetch_add(1, std::memory_order_relaxed);
		// Same preparation LatteRenderTarget_copyToBackbuffer does before displaying: the flipped texture
		// can be stale, with the newest pixels in an overlapping cached texture. Without this the capture
		// read only black whenever Cemu's own GamePad window was closed.
		LatteTexture_UpdateDataToLatest(texView->baseTexture);
		LatteTC_MarkTextureStillInUse(texView->baseTexture);
		if (!g_renderer->DrcCapture(texView, NowNs(), fitW, fitH))
		{
			s.droppedCapture++;
			if (!s.loggedUnsupportedRenderer && g_renderer->GetType() != RendererAPI::Vulkan)
			{
				Notice("GamePad Bridge needs the Vulkan graphics API. The GamePad screen will stay on the idle screen.", 15000);
				s.loggedUnsupportedRenderer = true;
			}
		}
	}
#endif

	void OnTvFlip(LatteTextureView* texView)
	{
#if BOOST_OS_LINUX
		// Connect from the TV flips too: some games draw nothing on the GamePad for minutes, or never
		// (Super Smash Bros. for Wii U: 0 GamePad frames through intro, menus and a match).
		if (!MaintainConnection())
			return;
		const bool mirror = GetGamePadBridgeConfig().mirrorTvWhenPadUnused &&
							(s.lastDrcFlipNs == 0 || NowNs() - s.lastDrcFlipNs > kMirrorAfterNs);
		if (mirror != s.mirroring)
		{
			cemuLog_log(LogType::Force, mirror ? "GamePad Bridge: the game draws nothing on the GamePad; showing the TV picture there"
											   : "GamePad Bridge: the game draws on the GamePad again");
			s.mirroring = mirror;
		}
		if (mirror)
			Capture(texView, kPadWidth, kPadHeight); // scaled down on the GPU: a 1080p readback per frame is 8 MB
#endif
	}

	void OnDrcFlip(LatteTextureView* texView)
	{
#if BOOST_OS_LINUX
		s.lastDrcFlipNs = NowNs();
		if (!MaintainConnection())
		{
			s.frameCounter.fetch_add(1, std::memory_order_relaxed);
			return;
		}
		Capture(texView, 0, 0);
#endif
	}

	bool BeginFrame(uint32 width, uint32 height, FrameTarget& out)
	{
#if BOOST_OS_LINUX
		if (!s.ready || s.lostConnection)
			return false;
		if (width > s.maxW || height > s.maxH || size_t(width) * height * 4 > s.slotSize)
		{
			s.droppedCapture++;
			return false;
		}
		for (uint32 i = 0; i < s.slotCount; i++)
		{
			if (!s.slotBusy[i].load(std::memory_order_acquire))
			{
				out.dst = s.shm + size_t(i) * s.slotSize;
				out.stride = width * 4;
				out.slot = i;
				return true;
			}
		}
		// Every slot is still with the bridge: drop this frame. Never wait (docs/IPC.md).
		s.droppedNoSlot++;
#endif
		return false;
	}

	void EndFrame(const FrameTarget& target, uint32 width, uint32 height, sint64 tFlipNs, uint64 counter)
	{
#if BOOST_OS_LINUX
		if (PatternActive())
			SyncPattern::Draw(target.dst, width, height, target.stride, counter);
		{
			std::lock_guard lock(sync.mutex);
			sync.frameIdToCounter[s.nextFrameId % kSyncRing] = counter;
		}
		drcb_frame_submit m{};
		m.slot = target.slot;
		m.format = DRCB_FMT_RGBA8;
		m.frame_id = s.nextFrameId++;
		m.width = width;
		m.height = height;
		m.stride = target.stride;
		m.t_flip_ns = tFlipNs;
		m.t_submit_ns = NowNs();
		s.slotBusy[target.slot].store(true, std::memory_order_release);
		if (SendMsg(DRCB_MSG_FRAME_SUBMIT, &m, sizeof(m)))
			s.submitted++;
		else
			s.slotBusy[target.slot].store(false, std::memory_order_release);
#endif
	}

	bool LatestInput(drcb_input_state& out)
	{
#if BOOST_OS_LINUX
		std::lock_guard lock(inputMutex);
		if (latestInputNs == 0 || NowNs() - latestInputNs > kInputStaleNs)
			return false;
		out = latestInput;
		return true;
#else
		return false;
#endif
	}

	void Shutdown()
	{
#if BOOST_OS_LINUX
		Disconnect(true);
		s.lastAttemptNs = 0;
		s.lastDrcFlipNs = 0;
		s.mirroring = false;
		s.noticeShownForOutage = false;
		s.loggedUnsupportedRenderer = false;
#endif
	}
}
