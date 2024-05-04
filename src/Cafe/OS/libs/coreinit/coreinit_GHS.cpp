#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_GHS.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/RPL/rpl.h"

namespace coreinit
{
	struct iobbuf
	{
		uint32be ukn00; // lock index?
		uint32be ukn04; // ?
		uint32be ukn08; // ?
		uint32be flags; // permissions and channel
	};

	#define GHS_FOPEN_MAX	100

	struct GHSAccessibleData
	{
		iobbuf _iob[GHS_FOPEN_MAX];
		MPTR _iob_lock[GHS_FOPEN_MAX];
		uint16be __gh_FOPEN_MAX;
		MEMPTR<void> ghs_environ;
		uint32 ghs_Errno; // exposed as 'errno' data export
	};

	SysAllocator<GHSAccessibleData> g_ghs_data;

	struct ghs_flock
	{
		uint32be mutexIndex;
	};

	void __ghs_flock_create(ghs_flock* flock);
	ghs_flock* __ghs_flock_ptr(iobbuf* iob);

	std::recursive_mutex g_ghsLock;
	std::recursive_mutex g_ghsLockFlock;

	SysAllocator<coreinit::OSMutex, GHS_FOPEN_MAX> _flockMutexArray;
	bool _flockMutexMask[GHS_FOPEN_MAX]; // if set, mutex in _flockMutexArray is reserved

	#define IOB_FLAG_IN				(0x1)
	#define IOB_FLAG_OUT			(0x2)

	#define IOB_FLAG_CHANNEL(__x)	((__x)<<18)

	void PrepareGHSRuntime()
	{
		g_ghs_data->ghs_environ = nullptr;
		g_ghs_data->__gh_FOPEN_MAX = GHS_FOPEN_MAX;
		g_ghs_data->ghs_Errno = 0;

		for (sint32 i = 0; i < GHS_FOPEN_MAX; i++)
			_flockMutexMask[i] = false;
		// init stdin/stdout/stderr
		g_ghs_data->_iob[0].flags = IOB_FLAG_IN;
		g_ghs_data->_iob[1].flags = IOB_FLAG_OUT;
		g_ghs_data->_iob[1].flags = IOB_FLAG_OUT;
		g_ghs_data->_iob[0].flags |= IOB_FLAG_CHANNEL(0);
		g_ghs_data->_iob[1].flags |= IOB_FLAG_CHANNEL(1);
		g_ghs_data->_iob[2].flags |= IOB_FLAG_CHANNEL(2);
		__ghs_flock_create(__ghs_flock_ptr(g_ghs_data->_iob + 0));
		__ghs_flock_create(__ghs_flock_ptr(g_ghs_data->_iob + 1));
		__ghs_flock_create(__ghs_flock_ptr(g_ghs_data->_iob + 2));

		osLib_addVirtualPointer("coreinit", "__gh_FOPEN_MAX", memory_getVirtualOffsetFromPointer(&g_ghs_data->__gh_FOPEN_MAX));
		osLib_addVirtualPointer("coreinit", "_iob", memory_getVirtualOffsetFromPointer(g_ghs_data->_iob));
		osLib_addVirtualPointer("coreinit", "environ", memory_getVirtualOffsetFromPointer(&g_ghs_data->ghs_environ));
		osLib_addVirtualPointer("coreinit", "errno", memory_getVirtualOffsetFromPointer(&g_ghs_data->ghs_Errno));
	}

	void __ghs_flock_create(ghs_flock* flock)
	{
		g_ghsLockFlock.lock();
		// find available mutex
		sint32 mutexIndex = -1;
		for (sint32 i = 0; i < GHS_FOPEN_MAX; i++)
		{
			if (!_flockMutexMask[i])
			{
				mutexIndex = i;
				break;
			}
		}
		if (mutexIndex == -1)
		{
			cemuLog_log(LogType::Force, "__ghs_flock_create(): No flock available");
			cemu_assert(false); // no available mutex
		}
		// mark mutex as reserved
		_flockMutexMask[mutexIndex] = true;
		// init mutex
		coreinit::OSInitMutexEx(_flockMutexArray.GetPtr() + mutexIndex, NULL);
		// update flock to point to the reserved mutex
		flock->mutexIndex = mutexIndex;
		g_ghsLockFlock.unlock();
	}

	void __ghs_flock_destroy(uint32 index)
	{
		g_ghsLockFlock.lock();
		cemu_assert_debug(index > 2); // stdin/stdout/stderr should never be released?
		cemu_assert(index < GHS_FOPEN_MAX);
		cemu_assert_debug(_flockMutexMask[index]);
		_flockMutexMask[index] = false;
		g_ghsLockFlock.unlock();
	}

	ghs_flock* __ghs_flock_ptr(iobbuf* iob)
	{
		size_t streamIndex = iob - g_ghs_data->_iob;
		return (ghs_flock*)&(g_ghs_data->_iob_lock[streamIndex]);
	}

	void __ghs_flock_file(uint32 index)
	{
		cemu_assert(index < GHS_FOPEN_MAX);
		OSLockMutex(_flockMutexArray.GetPtr() + index);
	}

	void __ghs_funlock_file(uint32 index)
	{
		cemu_assert(index < GHS_FOPEN_MAX);
		OSUnlockMutex(_flockMutexArray.GetPtr() + index);
	}

	void __ghsLock()
	{
		while (!g_ghsLock.try_lock())
		{
			PPCCore_switchToScheduler();
		}
	}

	void __ghsUnlock()
	{
		g_ghsLock.unlock();
	}

	void* __get_eh_init_block()
	{
		return nullptr;
	}

	void* __get_eh_globals()
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		return currentThread->crt.eh_globals.GetPtr();
	}

	void* __get_eh_mem_manage()
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		return &currentThread->crt.eh_mem_manage;
	}

	void* __gh_errno_ptr()
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		return &currentThread->context.ghs_errno;
	}

	void* __get_eh_store_globals()
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		return &currentThread->crt.eh_store_globals;
	}

	void* __get_eh_store_globals_tdeh()
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();
		return &currentThread->crt.eh_store_globals_tdeh;
	}

	struct ghs_mtx_t
	{
		MEMPTR<coreinit::OSMutex> mutexPtr;
	};

	void __ghs_mtx_init(ghs_mtx_t* mtx)
	{
		mtx->mutexPtr = (coreinit::OSMutex*)coreinit::_weak_MEMAllocFromDefaultHeapEx(ppcsizeof<coreinit::OSMutex>(), 8);
		coreinit::OSInitMutex(mtx->mutexPtr.GetPtr());
	}

	void __ghs_mtx_dst(ghs_mtx_t* mtx)
	{
		coreinit::_weak_MEMFreeToDefaultHeap(mtx->mutexPtr.GetPtr());
		mtx->mutexPtr = nullptr;
	}

	void __ghs_mtx_lock(ghs_mtx_t* mtx)
	{
		coreinit::OSLockMutex(mtx->mutexPtr.GetPtr());
	}

	void __ghs_mtx_unlock(ghs_mtx_t* mtx)
	{
		coreinit::OSUnlockMutex(mtx->mutexPtr.GetPtr());
	}

	struct OSTLSBlock
	{
		MPTR addr;
		uint32 ukn04;
	};

	static_assert(sizeof(OSTLSBlock) == 8);

	struct TLS_Index
	{
		uint16 ukn00;
		uint16 tlsModuleIndex;
		MPTR   ukn04;
	};

	void* __tls_get_addr(TLS_Index* tlsIndex)
	{
		OSThread_t* currentThread = coreinit::OSGetCurrentThread();

		if (_swapEndianU16(tlsIndex->tlsModuleIndex) == 0)
			assert_dbg();

		// check if we need to allocate additional TLS blocks for this thread
		if (_swapEndianU16(tlsIndex->tlsModuleIndex) >= _swapEndianU32(currentThread->numAllocatedTLSBlocks))
		{
			uint32 allocSize = (RPLLoader_GetMaxTLSModuleIndex() + 1) * sizeof(OSTLSBlock); // __OSDynLoad_gTLSHeader.ukn00 * 8;
			MPTR allocMem = coreinit_allocFromSysArea(allocSize, 4);
			memset(memory_getPointerFromVirtualOffset(allocMem), 0, allocSize);
			if (_swapEndianU32(currentThread->numAllocatedTLSBlocks) != 0)
			{
				// keep previously allocated blocks
				memcpy(memory_getPointerFromVirtualOffset(allocMem), memory_getPointerFromVirtualOffset(_swapEndianU32(currentThread->tlsBlocksMPTR)), _swapEndianU32(currentThread->numAllocatedTLSBlocks) * 8);
			}
			currentThread->tlsBlocksMPTR = _swapEndianU32(allocMem);
			currentThread->numAllocatedTLSBlocks = _swapEndianU16(RPLLoader_GetMaxTLSModuleIndex() + 1);
		}
		// look up TLS address based on moduleIndex
		OSTLSBlock* tlsBlock = (OSTLSBlock*)memory_getPointerFromVirtualOffsetAllowNull(_swapEndianU32(currentThread->tlsBlocksMPTR) + sizeof(OSTLSBlock) * (uint32)_swapEndianU16(tlsIndex->tlsModuleIndex));
		if (tlsBlock->addr != _swapEndianU32(MPTR_NULL))
		{
			//osLib_returnFromFunction(hCPU, _swapEndianU32(tlsBlock->addr)+_swapEndianU32(tlsIndex->ukn04));
			return memory_getPointerFromVirtualOffset(_swapEndianU32(tlsBlock->addr) + _swapEndianU32(tlsIndex->ukn04));
		}
		// alloc data for TLS block
		uint8* tlsSectionData = nullptr;
		sint32 tlsSize = 0;

		bool r = RPLLoader_GetTLSDataByTLSIndex((sint16)_swapEndianU16(tlsIndex->tlsModuleIndex), &tlsSectionData, &tlsSize);
		cemu_assert(r);
		cemu_assert(tlsSize != 0);

		MPTR tlsData = coreinit_allocFromSysArea(tlsSize, 32);
		memcpy(memory_getPointerFromVirtualOffset(tlsData), tlsSectionData, tlsSize);
		tlsBlock->addr = _swapEndianU32(tlsData);
		return memory_getPointerFromVirtualOffset(_swapEndianU32(tlsBlock->addr) + _swapEndianU32(tlsIndex->ukn04));
	}

	void InitializeGHS()
	{
		cafeExportRegister("coreinit", __ghs_flock_create, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_flock_destroy, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_flock_ptr, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_flock_file, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_funlock_file, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghsLock, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghsUnlock, LogType::Placeholder);

		cafeExportRegister("coreinit", __get_eh_init_block, LogType::Placeholder);
		cafeExportRegister("coreinit", __get_eh_globals, LogType::Placeholder);
		cafeExportRegister("coreinit", __get_eh_mem_manage, LogType::Placeholder);
		cafeExportRegister("coreinit", __gh_errno_ptr, LogType::Placeholder);
		cafeExportRegister("coreinit", __get_eh_store_globals, LogType::Placeholder);
		cafeExportRegister("coreinit", __get_eh_store_globals_tdeh, LogType::Placeholder);

		cafeExportRegister("coreinit", __ghs_mtx_init, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_mtx_dst, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_mtx_lock, LogType::Placeholder);
		cafeExportRegister("coreinit", __ghs_mtx_unlock, LogType::Placeholder);

		cafeExportRegister("coreinit", __tls_get_addr, LogType::Placeholder);
	}
};
