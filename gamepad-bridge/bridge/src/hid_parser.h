#pragma once
// GamePad HID (input) packet -> logical input state. 128 bytes, 180 Hz (libdrc web/docs/re/sc-input.rst:10-13).
//
// Buttons, sticks, touch, battery: a line-for-line transcription of libdrc's
// InputReceiver::ProcessInputMessage (src/input-receiver.cpp), including its stick range, dead zone and
// default touch calibration. libdrc is the top authority (CLAUDE.md) and parses the host direction.
// IMU: libdrc doesn't parse it, and its doc's field order disagrees with Vanilla's; we follow Vanilla
// (lib/gamepad/input.c), which packs these fields for real consoles. See docs/PROTOCOL.md, "Input".
// Everything here is TODO: verify against recorded real packets (POST-HW, TASKS.md 11).
#include "drcbridge/ipc.h"

#include <cstddef>
#include <cstdint>

namespace drcb {

constexpr size_t kHidPacketSize = 128; // libdrc input-receiver.cpp: "HID packets should always be 128 bytes"

struct HidExtra
{
	uint16_t seq_id;
	uint8_t power_status;   // libdrc InputData::PowerStatus bits
	uint8_t battery_charge; // raw byte 5; scale unknown (libdrc passes it through)
	// Battery level, VanillaBatteryStatus (vanilla lib/vanilla.h:92-100): 0 charging, 1 unknown, 2 very low, 3 low,
	// 4 medium, 5 high, 6 full. Carried in the spare 3 bits of touch point 9's x coordinate (vanilla
	// lib/gamepad/input.c:214 sets it, pack_touchcoord :134-137 and the byte swap/bit reversal :236-240 put it at
	// bits 4-6 of byte 36 + 9 * 4 + 1, the same bits libdrc reads touch pressure from for points 0-1).
	uint8_t battery_level;
	uint8_t audio_volume;
	uint32_t raw_buttons;   // libdrc layout: (msg[80] << 16) | (msg[2] << 8) | msg[3]
	int touch_pressure;     // libdrc: "TODO(delroth): make meaningful"
	int32_t gyro_raw[3];    // roll, pitch, yaw (Vanilla order)
	int16_t accel_raw[3];   // x, y, z (Vanilla stores z, x, y)
};

// Touch calibration, libdrc InputReceiver: the defaults are "those that the device itself uses prior to
// loading the real ones from UIC config".
struct TouchCalibration
{
	int32_t ref_1_x = 20, ref_1_y = 20, ref_2_x = 834, ref_2_y = 460;
	int32_t raw_1_x = 195, raw_1_y = 3818, raw_2_x = 3877, raw_2_y = 373;
};

// Returns false (and leaves `out` untouched) if the packet isn't 128 bytes, as libdrc does.
bool parse_hid(const uint8_t* msg, size_t size, const TouchCalibration& cal, drcb_input_state& out, HidExtra& extra);

} // namespace drcb
