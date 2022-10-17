#pragma once

unsigned int crc32_calc(unsigned int c, const void* data, int length);

inline unsigned int crc32_calc(const void* data, int length)
{
	return crc32_calc(0, data, length);
}