#pragma once

namespace MemMapper
{
	enum class PAGE_PERMISSION : uint32
	{
		P_READ = (0x01),
		P_WRITE = (0x02),
		P_EXECUTE = (0x04),
		// combined
		P_NONE = 0,
		P_RW = (0x03),
		P_RWX = (0x07)
	};
	DEFINE_ENUM_FLAG_OPERATORS(PAGE_PERMISSION);

	size_t GetPageSize();

	void* ReserveMemory(void* baseAddr, size_t size, PAGE_PERMISSION permissionFlags);
	void FreeReservation(void* baseAddr, size_t size);

	void* AllocateMemory(void* baseAddr, size_t size, PAGE_PERMISSION permissionFlags, bool fromReservation = false);
	void FreeMemory(void* baseAddr, size_t size, bool fromReservation = false);
};