#include "util/MemMapper/MemMapper.h"

#include <Windows.h>

namespace MemMapper
{
	const size_t sPageSize{ []()
		{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return (size_t)si.dwPageSize;
	}()
	};

	size_t GetPageSize()
	{
		return sPageSize;
	}

	DWORD GetPageProtection(PAGE_PERMISSION permissionFlags)
	{
		DWORD p = 0;
		if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PAGE_EXECUTE_READWRITE;
		else if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PAGE_READWRITE;
		else if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PAGE_READONLY;
		else
			cemu_assert_unimplemented();
		return p;
	}

	void* ReserveMemory(void* baseAddr, size_t size, PAGE_PERMISSION permissionFlags)
	{
		void* r = VirtualAlloc(baseAddr, size, MEM_RESERVE, GetPageProtection(permissionFlags));
		return r;
	}

	void FreeReservation(void* baseAddr, size_t size)
	{
		VirtualFree(baseAddr, size, MEM_RELEASE);
	}

	void* AllocateMemory(void* baseAddr, size_t size, PAGE_PERMISSION permissionFlags, bool fromReservation)
	{
		void* r;
		if(fromReservation)
			r = VirtualAlloc(baseAddr, size, MEM_COMMIT, GetPageProtection(permissionFlags));
		else
			r = VirtualAlloc(baseAddr, size, MEM_RESERVE | MEM_COMMIT, GetPageProtection(permissionFlags));
		return r;
	}

	void FreeMemory(void* baseAddr, size_t size, bool fromReservation)
	{
		if(fromReservation)
			VirtualFree(baseAddr, size, MEM_DECOMMIT);
		else
			VirtualFree(baseAddr, size, MEM_RELEASE);
	}

};
