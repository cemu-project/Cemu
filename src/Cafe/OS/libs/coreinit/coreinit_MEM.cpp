#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_FG.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.h"
#include "Cafe/OS/libs/coreinit/coreinit_Memory.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_FrmHeap.h"
#include "coreinit_DynLoad.h"

// the system area is a block of memory that exists only in the emulator. It is used to simplify dynamic memory allocation for system data
// this partially overlaps with the system heap from coreinit_SysHeap.cpp -> Use SysHeap for everything
MPTR sysAreaAllocatorOffset = 0;

MPTR coreinit_allocFromSysArea(uint32 size, uint32 alignment)
{
	cemu_assert_debug(mmuRange_CEMU_AREA.isMapped());
	// deprecated, use OSAllocFromSystem instead (for everything but SysAllocator which probably should use its own allocator?)
	static std::mutex s_allocator_mutex;
	s_allocator_mutex.lock();
	sysAreaAllocatorOffset = (sysAreaAllocatorOffset+alignment-1);
	sysAreaAllocatorOffset -= (sysAreaAllocatorOffset%alignment);
	uint32 newMemOffset =  mmuRange_CEMU_AREA.getBase() + sysAreaAllocatorOffset;
	sysAreaAllocatorOffset += (size+3)&~3;
	if( sysAreaAllocatorOffset >= mmuRange_CEMU_AREA.getSize() )
	{
		cemuLog_log(LogType::Force, "Ran out of system memory");
		cemu_assert(false); // out of bounds
	}
	s_allocator_mutex.unlock();
	return newMemOffset;
}

void coreinit_freeToSysArea(MPTR mem)
{
	// todo
}

namespace coreinit
{
#define MEM_MAX_HEAP_TABLE (0x20)

	sint32 g_heapTableCount = 0;
	MEMHeapBase* g_heapTable[MEM_MAX_HEAP_TABLE] = {};

	bool g_slockInitialized = false;
	bool g_listsInitialized = false;

	MEMList g_list1;
	MEMList g_list2;
	MEMList g_list3;

	std::array<uint32, 3> gHeapFillValues{ 0xC3C3C3C3, 0xF3F3F3F3, 0xD3D3D3D3 };
	SysAllocator<OSSpinLock> gHeapGlobalLock;
	MEMHeapBase* gDefaultHeap;

	bool MEMHeapTable_Add(MEMHeapBase* heap)
	{
		if (g_heapTableCount >= MEM_MAX_HEAP_TABLE)
			return false;
		g_heapTable[g_heapTableCount] = heap;
		g_heapTableCount++;
		return true;
	}

	bool MEMHeapTable_Remove(MEMHeapBase* heap)
	{
		if (g_heapTableCount == 0)
			return false;

		if (g_heapTableCount > MEM_MAX_HEAP_TABLE)
			return false;

		for (sint32 i = 0; i < g_heapTableCount; ++i)
		{
			if (g_heapTable[i] == heap)
			{
				g_heapTable[i] = nullptr;
				g_heapTableCount--;

				if (g_heapTableCount == 0)
					return true;

				if (i >= g_heapTableCount)
					return true;

				cemu_assert_debug(i < MEM_MAX_HEAP_TABLE);
				for (; i < g_heapTableCount; ++i)
				{
					g_heapTable[i] = g_heapTable[i + 1];
				}

				cemu_assert_debug(i < MEM_MAX_HEAP_TABLE);
				g_heapTable[i] = nullptr;

				return true;
			}
		}

		return false;
	}

	MEMHeapBase* _MEMList_FindContainingHeap(MEMList* list, MEMHeapBase* heap)
	{
		for (MEMHeapBase* obj = (MEMHeapBase*)MEMGetFirstListObject(list); obj; obj = (MEMHeapBase*)MEMGetNextListObject(list, obj))
		{
			if (obj->heapStart.GetPtr() <= heap && heap < obj->heapEnd.GetPtr())
			{
				const MEMHeapHandle containHeap = _MEMList_FindContainingHeap(&obj->childList, heap);
				return containHeap ? containHeap : obj;
			}
		}

		return nullptr;
	}

	bool MEMList_ContainsHeap(MEMList* list, MEMHeapBase* heap)
	{
		for (MEMHeapBase* obj = (MEMHeapBase*)MEMGetFirstListObject(list); obj; obj = (MEMHeapBase*)MEMGetNextListObject(list, obj))
		{
			if (obj == heap)
				return true;
		}

		return false;
	}

	MEMList* MEMList_FindContainingHeap(MEMHeapBase* head)
	{
		MEMPTR<void> memBound;
		uint32be memBoundSize;
		OSGetMemBound(1, (MPTR*)memBound.GetBEPtr(), (uint32*)&memBoundSize);

		MEMPTR<void> bucket;
		uint32be bucketSize;
		coreinit::OSGetForegroundBucket(&bucket, &bucketSize);

		if ((uintptr_t)memBound.GetPtr() > (uintptr_t)head || (uintptr_t)head >= (uintptr_t)memBound.GetPtr() + (uint32)memBoundSize)
		{
			if ((uintptr_t)bucket.GetPtr() > (uintptr_t)head || (uintptr_t)head >= (uintptr_t)bucket.GetPtr() + (uint32)bucketSize)
			{
				MEMHeapBase* containHeap = _MEMList_FindContainingHeap(&g_list1, head);
				if (containHeap)
					return &containHeap->childList;

				return &g_list1;
			}

			MEMHeapBase* containHeap = _MEMList_FindContainingHeap(&g_list3, head);
			if (containHeap)
				return &containHeap->childList;

			return &g_list3;
		}

		MEMHeapBase* containHeap = _MEMList_FindContainingHeap(&g_list2, head);
		if (containHeap)
			return &containHeap->childList;

		return &g_list2;
	}

	void MEMInitHeapBase(MEMHeapBase* heap, MEMHeapMagic magic, void* heapStart, void* heapEnd, uint32 createFlags)
	{
		memset(heap, 0, sizeof(MEMHeapBase));
		heap->magic = magic;
		heap->heapStart = heapStart;
		heap->heapEnd = heapEnd;
		heap->flags = (uint8)createFlags;

		MEMInitList(&heap->childList, 4);

		if (!g_slockInitialized)
		{
			OSInitSpinLock(&gHeapGlobalLock);
			g_slockInitialized = true;
		}

		if (!g_listsInitialized)
		{
			MEMInitList(&g_list1, offsetof(MEMHeapBase, link));
			MEMInitList(&g_list2, offsetof(MEMHeapBase, link));
			MEMInitList(&g_list3, offsetof(MEMHeapBase, link));
			g_listsInitialized = true;
		}

		OSInitSpinLock(&heap->spinlock);

		OSUninterruptibleSpinLock_Acquire(&gHeapGlobalLock);

		MEMList* list = MEMList_FindContainingHeap(heap);
		MEMAppendListObject(list, heap);

		OSUninterruptibleSpinLock_Release(&gHeapGlobalLock);
	}

	void MEMBaseDestroyHeap(MEMHeapBase* heap)
	{
		OSUninterruptibleSpinLock_Acquire(&gHeapGlobalLock);

		if (HAS_FLAG(heap->flags, MEM_HEAP_OPTION_THREADSAFE))
			OSUninterruptibleSpinLock_Acquire(&heap->spinlock);

		MEMList* containHeap = MEMList_FindContainingHeap(heap);
		cemu_assert_debug(MEMList_ContainsHeap(containHeap, heap));
		MEMRemoveListObject(containHeap, heap);

		if (HAS_FLAG(heap->flags, MEM_HEAP_OPTION_THREADSAFE))
			OSUninterruptibleSpinLock_Release(&heap->spinlock);

		OSUninterruptibleSpinLock_Release(&gHeapGlobalLock);
	}

	std::array<MEMHeapBase*, 9> sHeapBaseHandle{};

	MEMHeapBase* MEMGetBaseHeapHandle(uint32 index)
	{
		if (index >= sHeapBaseHandle.size())
			return nullptr;
		return sHeapBaseHandle[index];
	}

	MEMHeapBase* MEMSetBaseHeapHandle(uint32 index, MEMHeapBase* heapBase)
	{
		if (index >= sHeapBaseHandle.size())
			return nullptr;
		if (sHeapBaseHandle[index] != nullptr)
		{
			cemuLog_log(LogType::Force, "MEMSetBaseHeapHandle(): Trying to assign heap to non-empty slot");
			return sHeapBaseHandle[index];
		}
		sHeapBaseHandle[index] = heapBase;
		return nullptr;
	}

	uint32 MEMSetFillValForHeap(HEAP_FILL_TYPE type, uint32 value)
	{
		uint32 idx = (uint32)type;
		cemu_assert(idx < gHeapFillValues.size());
		OSUninterruptibleSpinLock_Acquire(&gHeapGlobalLock);
		const uint32 oldValue = gHeapFillValues[idx];
		gHeapFillValues[idx] = value;
		OSUninterruptibleSpinLock_Release(&gHeapGlobalLock);
		return oldValue;
	}

	uint32 MEMGetFillValForHeap(HEAP_FILL_TYPE type)
	{
		uint32 idx = (uint32)type;
		cemu_assert(idx < gHeapFillValues.size());
		OSUninterruptibleSpinLock_Acquire(&gHeapGlobalLock);
		const uint32 value = gHeapFillValues[idx];
		OSUninterruptibleSpinLock_Release(&gHeapGlobalLock);
		return value;
	}

	MEMHeapBase* MEMFindContainHeap(const void* memBlock)
	{
		MEMPTR<void> memBound;
		uint32be memBoundSize;
		OSGetMemBound(1, (MPTR*)memBound.GetBEPtr(), (uint32*)&memBoundSize);

		MEMPTR<void> bucket;
		uint32be bucketSize;
		coreinit::OSGetForegroundBucket(&bucket, &bucketSize);

		OSUninterruptibleSpinLock_Acquire(&gHeapGlobalLock);

		MEMHeapBase* result;
		if ((uintptr_t)memBound.GetPtr() > (uintptr_t)memBlock || (uintptr_t)memBlock >= (uintptr_t)memBound.GetPtr() + (uint32)memBoundSize)
		{
			if ((uintptr_t)bucket.GetPtr() > (uintptr_t)memBlock || (uintptr_t)memBlock >= (uintptr_t)bucket.GetPtr() + (uint32)bucketSize)
			{
				result = _MEMList_FindContainingHeap(&g_list1, (MEMHeapBase*)memBlock);
			}
			else
			{
				if (coreinit::OSGetForegroundBucket(nullptr, nullptr) == 0)
				{
					cemu_assert_unimplemented(); // foreground required
					result = nullptr;
				}
				else
				{
					result = _MEMList_FindContainingHeap(&g_list3, (MEMHeapBase*)memBlock);
				}
			}
		}
		else
		{
			if (coreinit::OSGetForegroundBucket(nullptr, nullptr) == 0)
			{
				cemu_assert_unimplemented();  // foreground required
				result = nullptr;
			}
			else
			{
				result = _MEMList_FindContainingHeap(&g_list2, (MEMHeapBase*)memBlock);
			}
		}

		OSUninterruptibleSpinLock_Release(&gHeapGlobalLock);

		return result;
	}

	void* MEMCreateUserHeapHandle(void* heapAddress, uint32 heapSize)
	{
		MEMInitHeapBase((MEMHeapBase*)heapAddress, MEMHeapMagic::USER_HEAP, (uint8*)heapAddress + 0x40, (uint8*)heapAddress + heapSize, 0);
		return heapAddress;
	}

	/* MEM Heap list */

#define GET_MEM_LINK(__obj__,__offset__) ((MEMLink*)((uintptr_t)__obj__ + (uint16)__offset__))

	void* MEMGetFirstListObject(MEMList* list)
	{
		return MEMGetNextListObject(list, nullptr);
	}

	void MEMList_SetSingleObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);
		cemu_assert_debug(object != nullptr);

		MEMLink* link = GET_MEM_LINK(object, list->offset);
		link->prev = nullptr;
		link->next = nullptr;

		list->numObjects = list->numObjects + 1;
		list->head = object;
		list->tail = object;
	}

	void MEMInitList(MEMList* list, uint32 offset)
	{
		cemu_assert_debug(list != nullptr);
		cemu_assert(offset <= 0xFFFF);
		memset(list, 0x00, sizeof(MEMLink));
		list->offset = (uint16)offset;
	}

	void MEMAppendListObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);
		cemu_assert_debug(object != nullptr);

		if (list->head)
		{
			list->numObjects = list->numObjects + 1;

			MEMLink* link = GET_MEM_LINK(object, list->offset);
			link->prev = list->tail;
			link->next = nullptr;

			MEMLink* tailLink = GET_MEM_LINK(list->tail.GetPtr(), list->offset);
			tailLink->next = object;

			list->tail = object;
		}
		else
			MEMList_SetSingleObject(list, object);
	}

	void MEMPrependListObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);
		cemu_assert_debug(object != nullptr);

		MEMPTR<void> headObject = list->head;
		if (headObject)
		{
			list->numObjects = list->numObjects + 1;

			MEMLink* link = GET_MEM_LINK(object, list->offset);
			link->prev = nullptr;
			link->next = headObject;

			MEMLink* headLink = GET_MEM_LINK(headObject.GetPtr(), list->offset);
			headLink->prev = object;

			list->head = object;
		}
		else
			MEMList_SetSingleObject(list, object);
	}

	void MEMRemoveListObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);
		cemu_assert_debug(object != nullptr);

		MEMLink* link = GET_MEM_LINK(object, list->offset);

		MEMPTR<void> prevObject = link->prev;
		if (prevObject)
		{
			MEMLink* prevLink = GET_MEM_LINK(prevObject.GetPtr(), list->offset);
			prevLink->next = link->next;
		}
		else
			list->head = link->next;

		MEMPTR<void> nextObject = link->next;
		if (nextObject)
		{
			MEMLink* nextLink = GET_MEM_LINK(nextObject.GetPtr(), list->offset);
			nextLink->prev = prevObject;
		}
		else
			list->tail = prevObject;

		link->prev = nullptr;
		link->next = nullptr;

		list->numObjects = list->numObjects - 1;
	}

	void* MEMGetNextListObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);

		if (!object)
			return list->head.GetPtr();

		MEMLink* result = GET_MEM_LINK(object, list->offset);
		return result->next.GetPtr();
	}

	void* MEMGetPrevListObject(MEMList* list, void* object)
	{
		cemu_assert_debug(list != nullptr);

		if (!object)
			return list->tail.GetPtr();

		MEMLink* result = GET_MEM_LINK(object, list->offset);
		return result->prev.GetPtr();
	}

	void* MEMGetNthListObject(MEMList* list, uint32 index)
	{
		cemu_assert_debug(list != nullptr);

		void* result = MEMGetNextListObject(list, nullptr);
		for (uint32 i = 0; i != index; ++i)
		{
			result = MEMGetNextListObject(list, result);
		}

		return result;
	}

	/* Default allocators */

	MEMHeapBase* MEMDefaultHeap_Init(void* mem, uint32 size)
	{
		MEMHeapBase* heap = MEMCreateExpHeapEx(mem, size, 4);
		gDefaultHeap = heap;
		cemu_assert_debug(heap);
		return heap;
	}

	void* default_MEMAllocFromDefaultHeap(uint32 size)
	{
		void* mem = MEMAllocFromExpHeapEx(gDefaultHeap, size, 0x40);
		cemuLog_logDebug(LogType::CoreinitMem, "MEMAllocFromDefaultHeap(0x{:08x}) Result: 0x{:08x}", size, memory_getVirtualOffsetFromPointer(mem));
		return mem;
	}

	void export_default_MEMAllocFromDefaultHeap(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(size, 0);
		MEMPTR<void> mem = default_MEMAllocFromDefaultHeap(size);
		osLib_returnFromFunction(hCPU, mem.GetMPTR());
	}

	void* default_MEMAllocFromDefaultHeapEx(uint32 size, sint32 alignment)
	{
		void* mem = MEMAllocFromExpHeapEx(gDefaultHeap, size, alignment);
		cemuLog_logDebug(LogType::CoreinitMem, "MEMAllocFromDefaultHeap(0x{:08x},{}) Result: 0x{:08x}", size, alignment, memory_getVirtualOffsetFromPointer(mem));
		return mem;
	}

	void export_default_MEMAllocFromDefaultHeapEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(size, 0);
		ppcDefineParamS32(alignment, 1);
		MEMPTR<void> mem = default_MEMAllocFromDefaultHeapEx(size, alignment);
		osLib_returnFromFunction(hCPU, mem.GetMPTR());
	}

	void default_MEMFreeToDefaultHeap(void* mem)
	{
		MEMFreeToExpHeap(gDefaultHeap, mem);
	}

	void export_default_MEMFreeToDefaultHeap(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(mem, void, 0);
		default_MEMFreeToDefaultHeap(mem.GetPtr());
		osLib_returnFromFunction(hCPU, 0);
	}

	void default_DynLoadAlloc(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(size, 0);
		ppcDefineParamS32(alignment, 1);
		ppcDefineParamMEMPTR(memResultPtr, uint32be, 2);
		MPTR mem = PPCCoreCallback(gCoreinitData->MEMAllocFromDefaultHeapEx.GetMPTR(), size, alignment);
		*memResultPtr = mem;
		osLib_returnFromFunction(hCPU, 0);
	}

	void default_DynLoadFree(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(mem, void, 0);
		PPCCoreCallback(gCoreinitData->MEMFreeToDefaultHeap.GetMPTR(), mem);
		osLib_returnFromFunction(hCPU, 0);
	}

	void* _weak_MEMAllocFromDefaultHeapEx(uint32 size, sint32 alignment)
	{
		MEMPTR<void> r{ PPCCoreCallback(gCoreinitData->MEMAllocFromDefaultHeapEx.GetMPTR(), size, alignment) };
		return r.GetPtr();
	}

	void* _weak_MEMAllocFromDefaultHeap(uint32 size)
	{
		MEMPTR<void> r{ PPCCoreCallback(gCoreinitData->MEMAllocFromDefaultHeap.GetMPTR(), size) };
		return r.GetPtr();
	}

	void _weak_MEMFreeToDefaultHeap(void* ptr)
	{
		PPCCoreCallback(gCoreinitData->MEMFreeToDefaultHeap.GetMPTR(), ptr);
	}

	void coreinitExport_MEMAllocFromAllocator(PPCInterpreter_t* hCPU)
	{
		MEMAllocator* memAllocator = (MEMAllocator*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		// redirect execution to allocator alloc callback
		hCPU->instructionPointer = memAllocator->func->funcAlloc.GetMPTR();
	}

	void coreinitExport_MEMFreeToAllocator(PPCInterpreter_t* hCPU)
	{
		MEMAllocator* memAllocator = (MEMAllocator*)memory_getPointerFromVirtualOffset(hCPU->gpr[3]);
		// redirect execution to allocator free callback
		hCPU->instructionPointer = memAllocator->func->funcFree.GetMPTR();
	}

	void _DefaultHeapAllocator_Alloc(PPCInterpreter_t* hCPU)
	{
		hCPU->gpr[3] = hCPU->gpr[4];
		hCPU->instructionPointer = gCoreinitData->MEMAllocFromDefaultHeap.GetMPTR();
	}

	void _DefaultHeapAllocator_Free(PPCInterpreter_t* hCPU)
	{
		hCPU->gpr[3] = hCPU->gpr[4];
		hCPU->instructionPointer = gCoreinitData->MEMFreeToDefaultHeap.GetMPTR();
	}

	SysAllocator<MEMAllocatorFunc> gDefaultHeapAllocator;

	void coreinitExport_MEMInitAllocatorForDefaultHeap(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(memAllocator, MEMAllocator, 0);

		gDefaultHeapAllocator->funcAlloc = PPCInterpreter_makeCallableExportDepr(_DefaultHeapAllocator_Alloc);
		gDefaultHeapAllocator->funcFree = PPCInterpreter_makeCallableExportDepr(_DefaultHeapAllocator_Free);

		memAllocator->func = gDefaultHeapAllocator.GetPtr();
		memAllocator->heap = MEMPTR<void>(MEMGetBaseHeapHandle(1));
		memAllocator->param1 = 0;
		memAllocator->param2 = 0;

		osLib_returnFromFunction(hCPU, 0);
	}

	void InitDefaultHeaps(MEMPTR<MEMHeapBase>& mem1Heap, MEMPTR<MEMHeapBase>& memFGHeap, MEMPTR<MEMHeapBase>& mem2Heap)
	{
		mem1Heap = nullptr;
		memFGHeap = nullptr;
		mem2Heap = nullptr;

		gCoreinitData->MEMAllocFromDefaultHeap = RPLLoader_MakePPCCallable(export_default_MEMAllocFromDefaultHeap);
		gCoreinitData->MEMAllocFromDefaultHeapEx = RPLLoader_MakePPCCallable(export_default_MEMAllocFromDefaultHeapEx);
		gCoreinitData->MEMFreeToDefaultHeap = RPLLoader_MakePPCCallable(export_default_MEMFreeToDefaultHeap);

		if (OSGetForegroundBucket(nullptr, nullptr))
		{
			MEMPTR<void> memBound;
			uint32be memBoundSize;
			OSGetMemBound(1, (MPTR*)memBound.GetBEPtr(), (uint32*)&memBoundSize);
			mem1Heap = MEMCreateFrmHeapEx(memBound.GetPtr(), (uint32)memBoundSize, 0);

			OSGetForegroundBucketFreeArea((MPTR*)memBound.GetBEPtr(), (MPTR*)&memBoundSize);
			memFGHeap = MEMCreateFrmHeapEx(memBound.GetPtr(), (uint32)memBoundSize, 0);
		}

		MEMPTR<void> memBound;
		uint32be memBoundSize;
		OSGetMemBound(2, (MPTR*)memBound.GetBEPtr(), (uint32*)&memBoundSize);
		mem2Heap = MEMDefaultHeap_Init(memBound.GetPtr(), (uint32)memBoundSize);

		// set DynLoad allocators
		OSDynLoad_SetAllocator(RPLLoader_MakePPCCallable(default_DynLoadAlloc), RPLLoader_MakePPCCallable(default_DynLoadFree));
		OSDynLoad_SetTLSAllocator(RPLLoader_MakePPCCallable(default_DynLoadAlloc), RPLLoader_MakePPCCallable(default_DynLoadFree));
	}

	void CoreInitDefaultHeap(/* ukn */)
	{
		cemu_assert_unimplemented();
	}

    void MEMResetToDefaultState()
    {
        for (auto& it : sHeapBaseHandle)
            it = nullptr;

        g_heapTableCount = 0;
        g_slockInitialized = false;
        g_listsInitialized = false;
        gDefaultHeap = nullptr;

        memset(&g_list1, 0, sizeof(g_list1));
        memset(&g_list2, 0, sizeof(g_list2));
        memset(&g_list3, 0, sizeof(g_list3));
    }

	void InitializeMEM()
	{
        MEMResetToDefaultState();

		cafeExportRegister("coreinit", CoreInitDefaultHeap, LogType::CoreinitMem);

		osLib_addFunction("coreinit", "MEMInitAllocatorForDefaultHeap", coreinitExport_MEMInitAllocatorForDefaultHeap);
		osLib_addFunction("coreinit", "MEMAllocFromAllocator", coreinitExport_MEMAllocFromAllocator);
		osLib_addFunction("coreinit", "MEMFreeToAllocator", coreinitExport_MEMFreeToAllocator);

		cafeExportRegister("coreinit", MEMGetBaseHeapHandle, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMSetBaseHeapHandle, LogType::CoreinitMem);

		cafeExportRegister("coreinit", MEMFindContainHeap, LogType::CoreinitMem);

		cafeExportRegister("coreinit", MEMGetFillValForHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMSetFillValForHeap, LogType::CoreinitMem);

		cafeExportRegister("coreinit", MEMCreateUserHeapHandle, LogType::CoreinitMem);

		cafeExportRegister("coreinit", MEMInitList, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMPrependListObject, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMAppendListObject, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMRemoveListObject, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMGetNextListObject, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMGetNthListObject, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMGetPrevListObject, LogType::CoreinitMem);
	}

}
