// hid-gen: writes a scripted HID replay (TASKS.md 8.5) whose packets are built by Vanilla's real packer
// (third_party/vanilla/lib/gamepad/input.c). NOT recorded traffic; real captures replace it POST-HW.
// usage: hid-gen OUT.hid
#include "hid_replay_file.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "vanilla.h"
void set_button_state(int button, int32_t value);
void set_touch_state(int x, int y);
void send_input(int socket_hid, const struct sockaddr_in* addr, size_t addr_size);
extern unsigned char g_captured[256];
extern size_t g_captured_size;
}

using namespace drcb;

static int32_t fbits(float f)
{
	int32_t i;
	memcpy(&i, &f, 4);
	return i;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: hid-gen OUT.hid\n");
		return 2;
	}
	// Script: [start s, end s) -> state. Everything else idle. Printed so the expected sequence is on record.
	struct Step
	{
		double t0, t1;
		const char* what;
	} steps[] = {
		{0.5, 0.8, "A pressed"},          {1.0, 1.5, "touch at pixel (427,240)"},
		{1.7, 2.2, "left stick full right"}, {2.4, 3.4, "gyro yaw +1 rad/s (57.3 deg/s)"},
		{3.6, 3.9, "HOME pressed"},        {4.1, 4.4, "B + d-pad up"},
	};
	const double rate = 180.0; // sc-input.rst:13, "sent by each GamePad 180 times per second"
	const double end = 4.6;
	std::vector<HidRecord> recs;
	for (int i = 0; i < int(end * rate); i++)
	{
		const double t = i / rate;
		for (int b = 0; b < VANILLA_BTN_COUNT; b++)
			set_button_state(b, 0);
		set_touch_state(-1, -1);
		auto in = [&](int k) { return t >= steps[k].t0 && t < steps[k].t1; };
		if (in(0)) set_button_state(VANILLA_BTN_A, 1);
		if (in(1)) set_touch_state(427, 240);
		if (in(2)) set_button_state(VANILLA_AXIS_L_X, 32767);
		if (in(3)) set_button_state(VANILLA_SENSOR_GYRO_YAW, fbits(1.0f));
		if (in(4)) set_button_state(VANILLA_BTN_HOME, 1);
		if (in(5)) { set_button_state(VANILLA_BTN_B, 1); set_button_state(VANILLA_BTN_UP, 1); }
		g_captured_size = 0;
		send_input(0, nullptr, 0);
		if (g_captured_size != 128)
		{
			fprintf(stderr, "Vanilla produced %zu bytes\n", g_captured_size);
			return 1;
		}
		HidRecord r;
		r.t_us = int64_t(t * 1e6);
		memcpy(r.packet, g_captured, 128);
		recs.push_back(r);
	}
	if (!save_hid_replay(argv[1], recs))
		return 1;
	printf("wrote %zu packets (%.1f s at %.0f Hz) to %s; script:\n", recs.size(), end, rate, argv[1]);
	for (auto& s : steps)
		printf("  %.1f-%.1f s  %s\n", s.t0, s.t1, s.what);
	return 0;
}
