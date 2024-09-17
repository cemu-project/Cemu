#pragma once

uint32 crc32_calc(uint32 c, const void* data, size_t length);

inline uint32 crc32_calc(const void* data, size_t length)
{
	return crc32_calc(0, data, length);
}