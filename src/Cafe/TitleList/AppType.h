#pragma once

enum class APP_TYPE : uint32
{
	GAME = 0x80000000,
	GAME_UPDATE = 0x0800001B,
	GAME_DLC = 0x0800000E,
	// data titles
	VERSION_DATA_TITLE = 0x10000015,
	DRC_FIRMWARE = 0x10000013,
	DRC_TEXTURE_ATLAS = 0x1000001A,
};

// allow direct comparison with uint32
inline bool operator==(APP_TYPE lhs, uint32 rhs)
{
	return static_cast<uint32>(lhs) == rhs;
}

inline bool operator==(uint32 lhs, APP_TYPE rhs)
{
	return lhs == static_cast<uint32>(rhs);
}
