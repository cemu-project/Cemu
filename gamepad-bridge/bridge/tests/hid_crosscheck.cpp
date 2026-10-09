// TASKS.md 8.1, pre-hardware substitute. There are NO recorded real HID packets in any reference
// (searched 2026-10-05), and inventing packets is forbidden (CLAUDE.md). So this cross-checks two independent
// reference implementations instead: packets built by Vanilla's real packer (lib/gamepad/input.c, used against
// real consoles) are parsed by our libdrc-transcribed parser (src/hid_parser.cpp). Agreement means the
// references agree. It does NOT prove a real pad sends this. Real captures: POST-HW (TASKS.md 11).
//
// Exact asserts only where both references define the same thing (buttons, field offsets, IMU round trip).
// Sticks and touch use different empirical calibrations in each reference: those are asserted loosely
// and their numbers printed as findings.
#include "hid_parser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

extern "C" {
#include "vanilla.h"
void set_button_state(int button, int32_t value);
void set_touch_state(int x, int y);
void send_input(int socket_hid, const struct sockaddr_in* addr, size_t addr_size);
extern unsigned char g_captured[256];
extern size_t g_captured_size;
}

using namespace drcb;

static int g_failures = 0;
#define CHECK(cond, ...)                                                                                           \
	do                                                                                                             \
	{                                                                                                              \
		if (!(cond))                                                                                               \
		{                                                                                                          \
			g_failures++;                                                                                          \
			printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
			printf(__VA_ARGS__);                                                                                   \
			printf("\n");                                                                                          \
		}                                                                                                          \
	} while (0)

static void reset_vanilla()
{
	for (int b = 0; b < VANILLA_BTN_COUNT; b++)
		set_button_state(b, 0);
	set_touch_state(-1, -1);
}

static int32_t float_bits(float f)
{
	int32_t i;
	memcpy(&i, &f, 4);
	return i;
}

static bool pack_and_parse(drcb_input_state& s, HidExtra& e)
{
	g_captured_size = 0;
	send_input(0, nullptr, 0);
	if (g_captured_size != kHidPacketSize)
	{
		printf("FAIL: Vanilla produced %zu bytes, expected %zu\n", g_captured_size, kHidPacketSize);
		g_failures++;
		return false;
	}
	return parse_hid(g_captured, g_captured_size, TouchCalibration{}, s, e);
}

int main()
{
	drcb_input_state s;
	HidExtra e;

	// 1) Buttons: one at a time, exact (both references use the same bit values).
	struct
	{
		int vanilla;
		uint32_t drcb;
		const char* name;
	} buttons[] = {
		{VANILLA_BTN_A, DRCB_BTN_A, "A"},          {VANILLA_BTN_B, DRCB_BTN_B, "B"},
		{VANILLA_BTN_X, DRCB_BTN_X, "X"},          {VANILLA_BTN_Y, DRCB_BTN_Y, "Y"},
		{VANILLA_BTN_L, DRCB_BTN_L, "L"},          {VANILLA_BTN_R, DRCB_BTN_R, "R"},
		{VANILLA_BTN_ZL, DRCB_BTN_ZL, "ZL"},       {VANILLA_BTN_ZR, DRCB_BTN_ZR, "ZR"},
		{VANILLA_BTN_MINUS, DRCB_BTN_MINUS, "-"},  {VANILLA_BTN_PLUS, DRCB_BTN_PLUS, "+"},
		{VANILLA_BTN_HOME, DRCB_BTN_HOME, "HOME"}, {VANILLA_BTN_LEFT, DRCB_BTN_LEFT, "LEFT"},
		{VANILLA_BTN_RIGHT, DRCB_BTN_RIGHT, "RIGHT"}, {VANILLA_BTN_UP, DRCB_BTN_UP, "UP"},
		{VANILLA_BTN_DOWN, DRCB_BTN_DOWN, "DOWN"}, {VANILLA_BTN_L3, DRCB_BTN_STICK_L, "L3"},
		{VANILLA_BTN_R3, DRCB_BTN_STICK_R, "R3"},  {VANILLA_BTN_TV, DRCB_BTN_TV, "TV"},
	};
	for (auto& b : buttons)
	{
		reset_vanilla();
		set_button_state(b.vanilla, 1);
		if (pack_and_parse(s, e))
			CHECK(s.buttons == b.drcb, "button %s: got 0x%05x, want 0x%05x", b.name, s.buttons, b.drcb);
	}
	reset_vanilla();
	if (pack_and_parse(s, e))
		CHECK(s.buttons == 0 && !s.touch_down, "idle packet: buttons 0x%x touch %u", s.buttons, s.touch_down);

	// 2) Sticks: Vanilla maps -1..1 to 2048 +- 1024; libdrc assumes 900..3200 (centre 2050). Different
	//    empirical ranges, so: centre must read 0 (dead zone), full deflection must keep its sign and be large.
	struct
	{
		int axis;
		float* out;
		const char* name;
		int sign; // expected sign of the parsed value for +32767 input
	} axes[] = {
		{VANILLA_AXIS_L_X, &s.stick_l[0], "L x", +1}, {VANILLA_AXIS_L_Y, &s.stick_l[1], "L y", -1},
		{VANILLA_AXIS_R_X, &s.stick_r[0], "R x", +1}, {VANILLA_AXIS_R_Y, &s.stick_r[1], "R y", -1},
	};
	printf("finding: sticks at full deflection (Vanilla range 1024..3072 vs libdrc 900..3200):\n");
	for (auto& a : axes)
	{
		reset_vanilla();
		if (pack_and_parse(s, e))
			CHECK(*a.out == 0.0f, "stick %s centre: %f", a.name, *a.out);
		for (int v : {32767, -32768})
		{
			reset_vanilla();
			set_button_state(a.axis, v);
			if (!pack_and_parse(s, e))
				continue;
			const int want_sign = (v > 0 ? 1 : -1) * a.sign;
			printf("  %s input %+6d -> %+.3f\n", a.name, v, *a.out);
			CHECK(*a.out * want_sign > 0.75f, "stick %s input %d: got %f (sign %d expected)", a.name, v, *a.out, want_sign);
		}
	}

	// 3) Touch: Vanilla takes screen pixels (0..854 / 0..480), libdrc parses with the pad's default calibration.
	printf("finding: touch round trip (Vanilla margins vs libdrc default calibration):\n");
	struct
	{
		int x, y;
	} touches[] = {{427, 240}, {50, 50}, {800, 430}, {0, 0}, {853, 479}};
	float max_err = 0;
	for (auto& t : touches)
	{
		reset_vanilla();
		set_touch_state(t.x, t.y);
		if (!pack_and_parse(s, e))
			continue;
		CHECK(s.touch_down, "touch at (%d,%d): not pressed (pressure %d)", t.x, t.y, e.touch_pressure);
		const float ex = t.x / 853.0f, ey = t.y / 479.0f;
		const float err = std::max(std::fabs(s.touch[0] - ex), std::fabs(s.touch[1] - ey));
		max_err = std::max(max_err, err);
		printf("  pixel (%3d,%3d) -> (%.3f, %.3f), ideal (%.3f, %.3f), err %.3f\n", t.x, t.y, s.touch[0], s.touch[1], ex, ey, err);
		CHECK(err < 0.10f, "touch (%d,%d): error %.3f > 0.10", t.x, t.y, err);
	}
	printf("  max error %.3f of screen size\n", max_err);

	// 4) IMU: our parser inverts Vanilla's packing, so this checks offsets, field order and scale exactly.
	reset_vanilla();
	const float rad_s = 1.0f; // 57.2958 deg/s
	set_button_state(VANILLA_SENSOR_GYRO_YAW, float_bits(rad_s));
	set_button_state(VANILLA_SENSOR_GYRO_PITCH, float_bits(-0.5f));
	set_button_state(VANILLA_SENSOR_GYRO_ROLL, float_bits(0.25f));
	set_button_state(VANILLA_SENSOR_ACCEL_X, float_bits(1.0f));
	set_button_state(VANILLA_SENSOR_ACCEL_Y, float_bits(-2.0f));
	set_button_state(VANILLA_SENSOR_ACCEL_Z, float_bits(9.80665f));
	if (pack_and_parse(s, e))
	{
		const float deg = 180.0f / float(M_PI);
		printf("IMU: gyro pitch/yaw/roll %.2f %.2f %.2f deg/s, accel x/y/z %.3f %.3f %.3f g\n", s.gyro[0], s.gyro[1],
			   s.gyro[2], s.accel[0], s.accel[1], s.accel[2]);
		// Vanilla truncates to int32 raw units (~0.0078 deg/s) and int16 accel units: allow one unit.
		CHECK(std::fabs(s.gyro[1] - rad_s * deg) < 0.02f, "gyro yaw %f", s.gyro[1]);
		CHECK(std::fabs(s.gyro[0] - (-0.5f) * deg) < 0.02f, "gyro pitch %f", s.gyro[0]);
		CHECK(std::fabs(s.gyro[2] - 0.25f * deg) < 0.02f, "gyro roll %f", s.gyro[2]);
		CHECK(std::fabs(s.accel[0] - 1.0f / 9.80665f) < 0.002f, "accel x %f", s.accel[0]);
		CHECK(std::fabs(s.accel[1] - (-2.0f) / 9.80665f) < 0.002f, "accel y %f", s.accel[1]);
		CHECK(std::fabs(s.accel[2] - 1.0f) < 0.002f, "accel z %f", s.accel[2]);
	}

	// 5) Size check exactly as libdrc: anything but 128 bytes is rejected.
	uint8_t short_pkt[127] = {};
	CHECK(!parse_hid(short_pkt, sizeof(short_pkt), TouchCalibration{}, s, e), "127-byte packet accepted");

	printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures, g_failures == 1 ? "" : "s");
	return g_failures ? 1 : 0;
}
