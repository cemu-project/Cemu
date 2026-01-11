#include "util/MemMapper/MemMapper.h"

#include <unistd.h>
#include <sys/mman.h>

namespace MemMapper
{
	const size_t sPageSize{ []()
		{
		return (size_t)getpagesize();
	}()
	};

	size_t GetPageSize()
	{
		return sPageSize;
	}

	int GetProt(PAGE_PERMISSION permissionFlags)
	{
		int  p = 0;
		if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PROT_READ | PROT_WRITE | PROT_EXEC;
		else if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PROT_READ | PROT_WRITE;
		else if (HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_READ) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_WRITE) && !HAS_FLAG(permissionFlags, PAGE_PERMISSION::P_EXECUTE))
			p = PROT_READ;
		else
			cemu_assert_unimplemented();
		return p;
	}

	void* ReserveMemory(void* baseAddr, size_t size, PAGE_PERMISSION permissionFlags)
	{
		return mmap(baseAddr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	}

	void FreeReservation(void* baseAddr, size_t size)
	{
		munmap(baseAddr, size);
	}

	void* AllocateMemory(void* baseAddr, size_t size,
                     PAGE_PERMISSION permissionFlags,
                     bool fromReservation)
{
    size_t pageSize = sPageSize;

    uintptr_t addr = reinterpret_cast<uintptr_t>(baseAddr);
    uintptr_t pageStart = addr & ~(pageSize - 1);

    size_t offset = addr - pageStart;
    size_t totalSize = offset + size;
    totalSize = (totalSize + pageSize - 1) & ~(pageSize - 1);

    if (fromReservation)
    {
        if (mprotect(reinterpret_cast<void*>(pageStart),
                     totalSize,
                     GetProt(permissionFlags)) != 0)
            return nullptr;

        return baseAddr;
    }

    return mmap(baseAddr,
                size,
                GetProt(permissionFlags),
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0);
}


void FreeMemory(void* baseAddr, size_t size, bool fromReservation)
{
    if (!fromReservation)
    {
        munmap(baseAddr, size);
        return;
    }

    uintptr_t addr = reinterpret_cast<uintptr_t>(baseAddr);
    uintptr_t pageStart = addr & ~(sPageSize - 1);

    size_t offset = addr - pageStart;
    size_t totalSize = offset + size;
    totalSize = (totalSize + sPageSize - 1) & ~(sPageSize - 1);

    mprotect(reinterpret_cast<void*>(pageStart),
             totalSize,
             PROT_NONE);
}


};
