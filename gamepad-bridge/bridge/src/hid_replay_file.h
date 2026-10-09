#pragma once
// HID replay file: "DRCBHID1", then records of {int64 t_us (offset from start, little-endian), u8[128] packet}.
// Written by tests/hid_gen.cpp (packets from Vanilla's packer) and, POST-HW, by a capture converter.
#include <cstdint>
#include <string>
#include <vector>

namespace drcb {

struct HidRecord
{
	int64_t t_us;
	uint8_t packet[128];
};

inline constexpr char kHidReplayMagic[8] = {'D', 'R', 'C', 'B', 'H', 'I', 'D', '1'};

bool load_hid_replay(const std::string& path, std::vector<HidRecord>& out); // logs errors
bool save_hid_replay(const std::string& path, const std::vector<HidRecord>& in);

} // namespace drcb
