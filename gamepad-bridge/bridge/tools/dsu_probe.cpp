// dsu-probe: a DSU client that talks to the bridge exactly the way Cemu's client does
// (cemu/src/input/api/DSU), validates every reply's CRC, and prints input changes. TASKS.md 8.5.
// usage: dsu-probe [--port 26760] [--seconds N]
#include "clock.h"

#include <arpa/inet.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
#pragma pack(push, 1)
struct Header
{
	uint32_t magic;
	uint16_t version, size;
	uint32_t crc, uid, type;
};
struct DataResponse
{
	Header h;
	uint8_t index, state, model, connection, mac[6], battery, active;
	uint32_t packet_index;
	uint8_t state1, state2, ps, touch, lx, ly, rx, ry;
	uint8_t analog[12];
	uint8_t t1_active, t1_id;
	int16_t t1_x, t1_y;
	uint8_t t2[6];
	uint64_t motion_ts;
	float accel[3], gyro[3];
};
#pragma pack(pop)
static_assert(sizeof(DataResponse) == 100);

uint32_t crc32(const uint8_t* p, size_t n)
{
	uint32_t c = 0xFFFFFFFFu;
	for (size_t i = 0; i < n; i++)
	{
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
	}
	return c ^ 0xFFFFFFFFu;
}

// Client messages as Cemu builds them (DSUMessages.cpp): magic 'CUSD', version 1001, CRC with field zeroed.
void send_client(int fd, const sockaddr_in& to, uint32_t type, const uint8_t* body, size_t body_len)
{
	uint8_t buf[64]{};
	Header h{0x43555344, 1001, uint16_t(4 + body_len), 0, 0x1234, type};
	memcpy(buf, &h, sizeof(h));
	memcpy(buf + sizeof(h), body, body_len);
	const size_t n = sizeof(h) + body_len;
	const uint32_t c = crc32(buf, n);
	memcpy(buf + 8, &c, 4);
	sendto(fd, buf, n, 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
}
} // namespace

int main(int argc, char** argv)
{
	int port = 26760;
	double seconds = 6;
	for (int i = 1; i + 1 < argc; i += 2)
	{
		if (!strcmp(argv[i], "--port"))
			port = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--seconds"))
			seconds = atof(argv[i + 1]);
	}
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in to{};
	to.sin_family = AF_INET;
	to.sin_port = htons(uint16_t(port));
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	send_client(fd, to, 0x100000, nullptr, 0); // version
	const uint8_t list[8] = {1, 0, 0, 0, 0, 0, 0, 0};
	send_client(fd, to, 0x100001, list, sizeof(list)); // ports: 1 port, index 0
	const uint8_t req[8] = {0x01, 0, 0, 0, 0, 0, 0, 0}; // DataRequest: by index 0 (RegisterFlag::Index)

	const int64_t t0 = drcb::now_ns(), t_end = t0 + int64_t(seconds * 1e9);
	int64_t last_req = 0;
	uint64_t packets = 0, bad_crc = 0;
	DataResponse prev{};
	bool have_prev = false;
	while (drcb::now_ns() < t_end)
	{
		if (drcb::now_ns() - last_req > 1'000'000'000)
		{
			send_client(fd, to, 0x100002, req, sizeof(req));
			last_req = drcb::now_ns();
		}
		pollfd p{fd, POLLIN, 0};
		if (poll(&p, 1, 50) <= 0)
			continue;
		uint8_t buf[256];
		const ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n < ssize_t(sizeof(Header)))
			continue;
		Header h;
		memcpy(&h, buf, sizeof(h));
		const uint32_t got = h.crc;
		memset(buf + 8, 0, 4);
		if (crc32(buf, size_t(n)) != got || h.magic != 0x53555344)
		{
			bad_crc++;
			continue;
		}
		const double t = double(drcb::now_ns() - t0) / 1e9;
		if (h.type == 0x100000)
			printf("%6.3f version response: %u\n", t, unsigned(buf[20] | (buf[21] << 8)));
		else if (h.type == 0x100001)
			printf("%6.3f port info: index %u state %u model %u\n", t, buf[20], buf[21], buf[22]);
		else if (h.type == 0x100002 && n == sizeof(DataResponse))
		{
			static double last_print = -1;
			DataResponse d;
			memcpy(&d, buf, sizeof(d));
			packets++;
			const float gyro_mag = std::sqrt(d.gyro[0] * d.gyro[0] + d.gyro[1] * d.gyro[1] + d.gyro[2] * d.gyro[2]);
			const float prev_gyro = std::sqrt(prev.gyro[0] * prev.gyro[0] + prev.gyro[1] * prev.gyro[1] + prev.gyro[2] * prev.gyro[2]);
			if (!have_prev || d.state1 != prev.state1 || d.state2 != prev.state2 || d.touch != prev.touch ||
				d.t1_active != prev.t1_active || d.lx != prev.lx || d.ly != prev.ly || d.rx != prev.rx || d.ry != prev.ry ||
				(d.t1_active && (d.t1_x != prev.t1_x || d.t1_y != prev.t1_y)) || std::fabs(gyro_mag - prev_gyro) > 1.0f ||
				t - last_print > 0.25)
			{
				last_print = t;
				printf("%6.3f state1 0x%02x state2 0x%02x touchclick %u | L (%3u,%3u) R (%3u,%3u) | touch %s (%4d,%4d) | "
					   "gyro p/y/r %6.1f %6.1f %6.1f deg/s | accel %5.2f %5.2f %5.2f g\n",
					   t, d.state1, d.state2, d.touch, d.lx, d.ly, d.rx, d.ry, d.t1_active ? "ON " : "off", d.t1_x, d.t1_y,
					   d.gyro[0], d.gyro[1], d.gyro[2], d.accel[0], d.accel[1], d.accel[2]);
				fflush(stdout);
			}
			prev = d;
			have_prev = true;
		}
	}
	printf("received %llu data packets, %llu rejected (bad CRC/magic)\n", (unsigned long long)packets,
		   (unsigned long long)bad_crc);
	close(fd);
	return packets > 0 && bad_crc == 0 ? 0 : 1;
}
