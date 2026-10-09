#include "dsu_server.h"
#include "clock.h"
#include "log.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace drcb {

namespace {

// cemu/src/input/api/DSU/DSUMessages.cpp
constexpr uint32_t kMagicClient = 0x43555344; // 'CUSD' as Cemu writes it: bytes "DSUC"
constexpr uint32_t kMagicServer = 0x53555344; // 'SUSD': bytes "DSUS"
constexpr uint16_t kProtocolVersion = 1001;
// cemu/src/input/api/DSU/DSUMessages.h MessageType
constexpr uint32_t kMsgVersion = 0x100000, kMsgInformation = 0x100001, kMsgData = 0x100002;
// Cemu re-requests only after each DataResponse it receives (DSUControllerProvider::reader_thread ->
// request_pad_data), so a client is gone when it stays quiet this long AFTER we sent it data. Measuring from its
// last request alone dropped Cemu whenever the pad sent no input for 5 s, e.g. before the pad connected, and Cemu
// then never asked again (runs/20261008-194137-play: subscribed 591.5, "timed out" at the first pad input 598.7).
constexpr int64_t kClientTimeoutNs = 5'000'000'000;

#pragma pack(push, 1)
struct Header // DSUMessages.h MessageHeader + Message (0x14 bytes)
{
	uint32_t magic;
	uint16_t protocol_version;
	uint16_t packet_size; // total - 16
	uint32_t crc32;
	uint32_t uid;
	uint32_t type;
};
struct PortInfoData // DSUMessages.h
{
	uint8_t index, state, model, connection;
	uint8_t mac[6];
	uint8_t battery, is_active;
};
struct TouchPoint
{
	uint8_t active, index;
	int16_t x, y;
};
struct DataResponseData // DSUMessages.h
{
	uint32_t packet_index;
	uint8_t state1, state2, ps, touch;
	uint8_t lx, ly, rx, ry;
	uint8_t dpad_left, dpad_down, dpad_right, dpad_up;
	uint8_t square, cross, circle, triangle;
	uint8_t r1, l1, r2, l2;
	TouchPoint tpad1, tpad2;
	uint64_t motion_timestamp; // microseconds (DSUControllerProvider::integrate_motion)
	float accel[3];            // g
	float gyro[3];             // degrees/s (Cemu multiplies by 0.0174533)
};
struct VersionResponse
{
	Header h;
	uint16_t version;
	uint8_t pad[2];
};
struct PortInfo
{
	Header h;
	PortInfoData info;
};
struct DataResponse
{
	Header h;
	PortInfoData info;
	DataResponseData data;
};
#pragma pack(pop)
static_assert(sizeof(Header) == 0x14);
static_assert(sizeof(VersionResponse) == 0x18); // DSUMessages.h static_assert
static_assert(sizeof(PortInfo) == 0x20);        // DSUMessages.h static_assert
static_assert(sizeof(DataResponse) == 100);

// Standard reflected CRC-32 (poly 0xEDB88320, init/xorout 0xFFFFFFFF), as cemu/src/util/crypto/crc32.cpp.
uint32_t crc32(const void* data, size_t n)
{
	static uint32_t table[256];
	static bool init = false;
	if (!init)
	{
		for (uint32_t i = 0; i < 256; i++)
		{
			uint32_t c = i;
			for (int k = 0; k < 8; k++)
				c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
		init = true;
	}
	uint32_t c = 0xFFFFFFFFu;
	const uint8_t* p = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < n; i++)
		c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

// DSUMessages.cpp Finalize: size after the 16-byte header, CRC over the whole message with crc32 = 0.
template<typename T> void finalize(T& msg, uint32_t uid, uint32_t type)
{
	msg.h.magic = kMagicServer;
	msg.h.protocol_version = kProtocolVersion;
	msg.h.packet_size = uint16_t(sizeof(T) - 16);
	msg.h.uid = uid;
	msg.h.type = type;
	msg.h.crc32 = 0;
	msg.h.crc32 = crc32(&msg, sizeof(T));
}

uint8_t stick_byte(float v) // -1..1 -> 0..255 (Cemu: v / 255 * 2 - 1)
{
	return uint8_t(std::lround(std::clamp((v + 1.0f) * 0.5f, 0.0f, 1.0f) * 255.0f));
}

PortInfoData port0(bool connected)
{
	PortInfoData p{};
	p.index = 0;
	p.state = connected ? 0x02 : 0x00; // DsState::Connected / Disconnected
	p.model = 0x02;                    // DsModel::DS4: the model with a touchpad and motion
	p.connection = 0x02;               // DsConnection::Bluetooth (it's wireless)
	const uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x57, 0x55}; // locally administered, "WU"
	memcpy(p.mac, mac, 6);
	p.battery = 0x00; // DsBattery::None: unknown (libdrc doesn't decode the battery byte)
	p.is_active = connected;
	return p;
}

} // namespace

DsuServer::DsuServer(EventLoop& loop, uint16_t port) : m_loop(loop), m_port(port)
{
	m_server_id = uint32_t(now_ns()) | 1;
}

DsuServer::~DsuServer()
{
	if (m_fd >= 0)
	{
		m_loop.remove(m_fd);
		close(m_fd);
	}
}

bool DsuServer::start()
{
	m_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	sockaddr_in a{};
	a.sin_family = AF_INET;
	a.sin_port = htons(m_port);
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // local only
	if (m_fd < 0 || bind(m_fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) < 0)
	{
		LOGE("DSU server: bind 127.0.0.1:%u failed: %s (is another DSU server running?)", m_port, strerror(errno));
		return false;
	}
	m_loop.add(m_fd, [this] { on_readable(); });
	LOGI("DSU server: listening on 127.0.0.1:%u (Cemu: Input settings -> DSUController)", m_port);
	return true;
}

void DsuServer::on_readable()
{
	uint8_t buf[512];
	sockaddr_in from{};
	socklen_t fl = sizeof(from);
	ssize_t n;
	while ((n = recvfrom(m_fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fl)) > 0)
	{
		if (size_t(n) < sizeof(Header))
			continue;
		Header h;
		memcpy(&h, buf, sizeof(h));
		if (h.magic != kMagicClient || h.protocol_version > kProtocolVersion || size_t(h.packet_size) + 16 != size_t(n))
		{
			LOGD("DSU: ignoring malformed packet (%zd bytes)", n);
			continue;
		}
		const uint32_t got = h.crc32;
		memset(buf + 8, 0, 4);
		if (crc32(buf, size_t(n)) != got)
		{
			LOGD("DSU: bad CRC from client");
			continue;
		}

		if (h.type == kMsgVersion)
		{
			VersionResponse r{};
			r.version = kProtocolVersion;
			finalize(r, m_server_id, kMsgVersion);
			sendto(m_fd, &r, sizeof(r), 0, reinterpret_cast<sockaddr*>(&from), fl);
		}
		else if (h.type == kMsgInformation && size_t(n) >= sizeof(Header) + 4)
		{
			// DSUMessages.h ListPorts: u32 count, u8 indices[4]
			int32_t count;
			memcpy(&count, buf + sizeof(Header), 4);
			count = std::clamp(count, 0, 4);
			for (int i = 0; i < count && sizeof(Header) + 4 + size_t(i) < size_t(n); i++)
			{
				const uint8_t idx = buf[sizeof(Header) + 4 + i];
				PortInfo r{};
				r.info = port0(m_connected && idx == 0);
				r.info.index = idx;
				finalize(r, m_server_id, kMsgInformation);
				sendto(m_fd, &r, sizeof(r), 0, reinterpret_cast<sockaddr*>(&from), fl);
			}
		}
		else if (h.type == kMsgData)
		{
			// Subscribe (or refresh) this client; we only have port 0, so the request flags don't matter.
			auto it = std::find_if(m_clients.begin(), m_clients.end(), [&](const Client& c) {
				return c.addr.sin_addr.s_addr == from.sin_addr.s_addr && c.addr.sin_port == from.sin_port;
			});
			if (it == m_clients.end())
			{
				m_clients.push_back(Client{from, now_ns()});
				LOGI("DSU: client %s:%u subscribed", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
				send_data(m_clients.back());
			}
			else
				it->last_request_ns = now_ns();
		}
	}
}

void DsuServer::update(const drcb_input_state& s, bool pad_connected)
{
	m_state = s;
	m_connected = pad_connected;
	// A new touch gets a new id, so the client sees separate touches (DS4 touch point semantics).
	if (s.touch_down && !m_was_touching)
		m_touch_id++;
	m_was_touching = s.touch_down;

	m_clients.erase(std::remove_if(m_clients.begin(), m_clients.end(), [&](const Client& c) {
						if (c.last_sent_ns - c.last_request_ns > kClientTimeoutNs)
						{
							LOGI("DSU: client %s:%u timed out", inet_ntoa(c.addr.sin_addr), ntohs(c.addr.sin_port));
							return true;
						}
						return false;
					}),
					m_clients.end());
	for (auto& c : m_clients)
		send_data(c);
}

void DsuServer::send_data(Client& c)
{
	const drcb_input_state& s = m_state;
	DataResponse r{};
	r.info = port0(m_connected);
	DataResponseData& d = r.data;
	d.packet_index = m_packet_index++;

	// Cemu reads state1 bits 0..7 as buttons 0..7 and state2 as 8..15 (DSUController::raw_state); names follow
	// the conventional DS4 order so Cemu's default labels make sense. GamePad -> DS4: - Share, L3, R3, + Options,
	// D-pad; ZL L2, ZR R2, L L1, R R1, X triangle, A circle, B cross, Y square.
	auto bit = [&](uint32_t mask) { return (s.buttons & mask) ? 1 : 0; };
	d.state1 = uint8_t(bit(DRCB_BTN_MINUS) << 0 | bit(DRCB_BTN_STICK_L) << 1 | bit(DRCB_BTN_STICK_R) << 2 |
					   bit(DRCB_BTN_PLUS) << 3 | bit(DRCB_BTN_UP) << 4 | bit(DRCB_BTN_RIGHT) << 5 |
					   bit(DRCB_BTN_DOWN) << 6 | bit(DRCB_BTN_LEFT) << 7);
	d.state2 = uint8_t(bit(DRCB_BTN_ZL) << 0 | bit(DRCB_BTN_ZR) << 1 | bit(DRCB_BTN_L) << 2 | bit(DRCB_BTN_R) << 3 |
					   bit(DRCB_BTN_X) << 4 | bit(DRCB_BTN_A) << 5 | bit(DRCB_BTN_B) << 6 | bit(DRCB_BTN_Y) << 7);
	d.ps = uint8_t(bit(DRCB_BTN_HOME)); // Cemu doesn't read this...
	d.touch = uint8_t(bit(DRCB_BTN_HOME)); // ...so HOME also goes out as touchpad-click (Cemu button 16)

	// Our sticks are +y up; SDL controllers in Cemu report +y down, so match that (y inverted by convention).
	d.lx = stick_byte(s.stick_l[0]);
	d.ly = stick_byte(-s.stick_l[1]);
	d.rx = stick_byte(s.stick_r[0]);
	d.ry = stick_byte(-s.stick_r[1]);

	d.dpad_left = bit(DRCB_BTN_LEFT) * 255;
	d.dpad_down = bit(DRCB_BTN_DOWN) * 255;
	d.dpad_right = bit(DRCB_BTN_RIGHT) * 255;
	d.dpad_up = bit(DRCB_BTN_UP) * 255;
	d.triangle = bit(DRCB_BTN_X) * 255;
	d.circle = bit(DRCB_BTN_A) * 255;
	d.cross = bit(DRCB_BTN_B) * 255;
	d.square = bit(DRCB_BTN_Y) * 255;
	d.r1 = bit(DRCB_BTN_R) * 255;
	d.l1 = bit(DRCB_BTN_L) * 255;
	d.r2 = bit(DRCB_BTN_ZR) * 255;
	d.l2 = bit(DRCB_BTN_ZL) * 255;

	// Touch: Cemu divides by 1920 x 942 ("touchpad resolution", DSUController::get_position) and feeds the
	// 0..1 result to the emulated GamePad's touch screen.
	d.tpad1.active = s.touch_down ? 1 : 0;
	d.tpad1.index = m_touch_id;
	if (s.touch_down) // only meaningful while touching; zero otherwise rather than leak a stale/clamped value
	{
		d.tpad1.x = int16_t(std::lround(std::clamp(s.touch[0], 0.0f, 1.0f) * 1920.0f));
		d.tpad1.y = int16_t(std::lround(std::clamp(s.touch[1], 0.0f, 1.0f) * 942.0f));
	}

	// Motion. Timestamps must increase (integrate_motion drops older ones); use the input receive time.
	d.motion_timestamp = uint64_t(s.t_received_ns / 1000);
	// Axes, measured on the real pad (runs/20261008-202148-calibrate, scripts/pad-calibrate.sh): our accel/gyro use
	// x = right, y = out of the screen, z = toward the bottom edge; accel reads +1 g on the axis pointing up (flat,
	// screen up: (0.03, 0.98, -0.15)); gyro is right-handed in deg/s (top edge tilted away: x < 0; right grip down:
	// z < 0). That is SDL's sensor frame. Cemu feeds DSU motion through the same path as SDL motion
	// (DSUControllerProvider.cpp:425-431 vs SDLControllerProvider.cpp:274), but negates SDL first:
	// acc = -sdl / 9.81 (SDLControllerProvider.cpp:231-233), gyro = (x, -y, -z) (:253-255). Do the same here, or the
	// game sees gravity upside down and yaw/roll reversed ("tilt inverted", Ninja Castle wouldn't throw).
	d.accel[0] = -s.accel[0];
	d.accel[1] = -s.accel[1];
	d.accel[2] = -s.accel[2];
	d.gyro[0] = s.gyro[0];
	d.gyro[1] = -s.gyro[1];
	d.gyro[2] = -s.gyro[2];

	finalize(r, m_server_id, kMsgData);
	sendto(m_fd, &r, sizeof(r), 0, reinterpret_cast<const sockaddr*>(&c.addr), sizeof(c.addr));
	c.last_sent_ns = now_ns();
}

} // namespace drcb
