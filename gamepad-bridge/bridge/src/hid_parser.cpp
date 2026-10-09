#include "hid_parser.h"

#include <algorithm>

namespace drcb {

namespace {
// libdrc src/input-receiver.cpp
constexpr int16_t kDrcStickMin = 900;
constexpr int16_t kDrcStickMax = 3200;
constexpr float kStickDeadZone = 0.1f;

// libdrc InputData::ButtonMask -> DRCB_BTN_* (include/drc/input.h). SYNC has no IPC bit and is dropped.
struct ButtonMap
{
	uint32_t drc, drcb;
};
constexpr ButtonMap kButtons[] = {
	{0x2, DRCB_BTN_HOME},       {0x4, DRCB_BTN_MINUS},     {0x8, DRCB_BTN_PLUS},  {0x10, DRCB_BTN_R},
	{0x20, DRCB_BTN_L},         {0x40, DRCB_BTN_ZR},       {0x80, DRCB_BTN_ZL},   {0x100, DRCB_BTN_DOWN},
	{0x200, DRCB_BTN_UP},       {0x400, DRCB_BTN_RIGHT},   {0x800, DRCB_BTN_LEFT}, {0x1000, DRCB_BTN_Y},
	{0x2000, DRCB_BTN_X},       {0x4000, DRCB_BTN_B},      {0x8000, DRCB_BTN_A},  {0x200000, DRCB_BTN_TV},
	{0x400000, DRCB_BTN_STICK_R}, {0x800000, DRCB_BTN_STICK_L},
};

int32_t s24_le(const uint8_t* p) // Vanilla lib/gamepad/input.c: s24_le_to_int32
{
	uint32_t u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
	if (u & 0x00800000u)
		u |= 0xFF000000u;
	return int32_t(u);
}
} // namespace

bool parse_hid(const uint8_t* msg, size_t size, const TouchCalibration& cal, drcb_input_state& out, HidExtra& extra)
{
	if (size != kHidPacketSize)
		return false;

	drcb_input_state s{};

	// Buttons: libdrc `int buttons = (msg[80] << 16) | (msg[2] << 8) | msg[3];`
	const uint32_t buttons = (uint32_t(msg[80]) << 16) | (uint32_t(msg[2]) << 8) | msg[3];
	for (const auto& b : kButtons)
		if (buttons & b.drc)
			s.buttons |= b.drcb;
	if (msg[4] & 0x02) // POWER_BUTTON_PRESSED (sc-input.rst PowerStatusMask; libdrc kPowerButtonPressed)
		s.buttons |= DRCB_BTN_POWER;

	// Sticks: libdrc, verbatim logic. Order: left x, left y, right x, right y.
	float* sticks[] = {&s.stick_l[0], &s.stick_l[1], &s.stick_r[0], &s.stick_r[1]};
	for (size_t i = 0; i < 4; ++i)
	{
		int16_t val_int = int16_t((msg[7 + 2 * i] << 8) | msg[6 + 2 * i]);
		val_int = std::max(kDrcStickMin, std::min(kDrcStickMax, val_int));
		const int16_t mid = (kDrcStickMax - kDrcStickMin) / 2;
		val_int = int16_t(val_int - (kDrcStickMin + mid));
		*sticks[i] = float(val_int) / mid;
		if (*sticks[i] > -kStickDeadZone && *sticks[i] < kStickDeadZone)
			*sticks[i] = 0.0f;
	}

	// Touch: libdrc, verbatim logic (average 10 points, two-point calibration, normalize to 0..1).
	int ts_x = 0, ts_y = 0;
	for (int i = 0; i < 10; ++i)
	{
		const int base = 36 + 4 * i;
		ts_x += ((msg[base + 1] & 0xF) << 8) | msg[base];
		ts_y += ((msg[base + 3] & 0xF) << 8) | msg[base + 2];
	}
	ts_x /= 10;
	ts_y /= 10;
	const float ts_ox = float(cal.raw_2_x * cal.ref_1_x - cal.raw_1_x * cal.ref_2_x) / (cal.raw_2_x - cal.raw_1_x);
	const float ts_w = float(cal.ref_1_x - cal.ref_2_x) / (cal.raw_1_x - cal.raw_2_x);
	const float ts_oy = float(cal.raw_2_y * cal.ref_1_y - cal.raw_1_y * cal.ref_2_y) / (cal.raw_2_y - cal.raw_1_y);
	const float ts_h = float(cal.ref_1_y - cal.ref_2_y) / (cal.raw_1_y - cal.raw_2_y);
	s.touch[0] = std::max(0.0f, std::min(1.0f, (ts_ox + ts_x * ts_w) / 853.0f));
	s.touch[1] = std::max(0.0f, std::min(1.0f, (ts_oy + ts_y * ts_h) / 479.0f));
	int ts_pressure = 0;
	ts_pressure |= ((msg[37] >> 4) & 7) << 0;
	ts_pressure |= ((msg[39] >> 4) & 7) << 3;
	ts_pressure |= ((msg[41] >> 4) & 7) << 6;
	ts_pressure |= ((msg[43] >> 4) & 7) << 9;
	s.touch_down = ts_pressure != 0; // libdrc: data.ts_pressed = (ts_pressure != 0)

	// IMU, Vanilla layout (lib/gamepad/input.c InputPacket): accel at 15 as z, x, y (s16 LE);
	// gyro at 21 as roll, pitch, yaw (s24 LE). Scale: Vanilla packs gyro as
	// raw = deg/s / ((200 * 6) / 154000) and accel as raw = m/s^2 * -800 (x, y), * 800 (z).
	const int16_t az = int16_t(msg[15] | (msg[16] << 8));
	const int16_t ax = int16_t(msg[17] | (msg[18] << 8));
	const int16_t ay = int16_t(msg[19] | (msg[20] << 8));
	const int32_t roll = s24_le(msg + 21), pitch = s24_le(msg + 24), yaw = s24_le(msg + 27);
	constexpr float kGyroDegPerRaw = (200.0f * 6.0f) / 154000.0f;
	constexpr float kStandardGravity = 9.80665f;
	s.gyro[0] = float(pitch) * kGyroDegPerRaw;
	s.gyro[1] = float(yaw) * kGyroDegPerRaw;
	s.gyro[2] = float(roll) * kGyroDegPerRaw;
	s.accel[0] = float(ax) / -800.0f / kStandardGravity;
	s.accel[1] = float(ay) / -800.0f / kStandardGravity;
	s.accel[2] = float(az) / 800.0f / kStandardGravity;

	extra.seq_id = uint16_t((msg[0] << 8) | msg[1]); // Vanilla writes htons(seq_id)
	extra.power_status = msg[4];
	extra.battery_charge = msg[5];
	extra.battery_level = (msg[36 + 9 * 4 + 1] >> 4) & 7;
	extra.audio_volume = msg[14];
	extra.raw_buttons = buttons;
	extra.touch_pressure = ts_pressure;
	extra.gyro_raw[0] = roll;
	extra.gyro_raw[1] = pitch;
	extra.gyro_raw[2] = yaw;
	extra.accel_raw[0] = ax;
	extra.accel_raw[1] = ay;
	extra.accel_raw[2] = az;

	out = s;
	return true;
}

} // namespace drcb
