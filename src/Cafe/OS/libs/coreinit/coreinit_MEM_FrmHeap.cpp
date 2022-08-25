#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM_FrmHeap.h"

namespace coreinit
{
	bool __FrmHeapDebug_IsValid(MEMFrmHeap* frmHeap, const char* funcName)
	{
		if (!frmHeap)
		{
			cemuLog_log(LogType::APIErrors, "{}: Heap is nullptr", funcName);
			return false;
		}
		if (frmHeap->magic != MEMHeapMagic::FRAME_HEAP)
		{
			cemuLog_log(LogType::APIErrors, "{}: Heap has bad magic. Not initialized?", funcName);
			return false;
		}
		return true;
	}

	MEMFrmHeap* MEMCreateFrmHeapEx(void* memStart, uint32 size, uint32 createFlags)
	{
		cemu_assert_debug(memStart);

		uintptr_t startAddr = (uintptr_t)memStart;
		uintptr_t endAddr = startAddr + size;

		// align and pad address
		startAddr = (startAddr + 3) & ~3;
		endAddr &= ~3;

		if (startAddr == 0)
			return nullptr;
		if (startAddr > endAddr || (endAddr - startAddr) < sizeof(MEMFrmHeap))
			return nullptr;

		MEMFrmHeap* frmHeap = (MEMFrmHeap*)startAddr;
		MEMInitHeapBase(frmHeap, MEMHeapMagic::FRAME_HEAP, (void*)((uintptr_t)startAddr + sizeof(MEMFrmHeap)), (void*)endAddr, createFlags);
		frmHeap->allocationHead = frmHeap->heapStart;
		frmHeap->allocationTail = frmHeap->heapEnd;
		frmHeap->recordedStates = nullptr;

		MEMHeapTable_Add(frmHeap);
		return frmHeap;
	}

	void* MEMDestroyFrmHeap(MEMFrmHeap* frmHeap)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMDestroyFrmHeap"))
			return nullptr;
		MEMBaseDestroyHeap(frmHeap);
		MEMHeapTable_Remove(frmHeap);
		return frmHeap;
	}

	uint32 MEMGetAllocatableSizeForFrmHeapEx(MEMFrmHeap* frmHeap, sint32 alignment)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMGetAllocatableSizeForFrmHeapEx"))
			return 0;
		frmHeap->AcquireLock();
		uint32 allocatableSize = 0;
		bool negativeAlignment = alignment < 0;
		if (negativeAlignment)
			alignment = -alignment;
		uint32 bits = (uint32)alignment;
		if (bits == 0 || (bits & (bits - 1)) != 0)
		{
			cemuLog_log(LogType::APIErrors, "MEMGetAllocatableSizeForFrmHeapEx(): Invalid alignment");
			return 0;
		}
		if (negativeAlignment)
		{
			cemu_assert_unimplemented();
		}
		else
		{
			uint32 headAllocator = frmHeap->allocationHead.GetMPTR();
			uint32 tailAllocator = frmHeap->allocationTail.GetMPTR();
			uint32 allocStart = (headAllocator + alignment - 1) & ~(alignment - 1);
			if (allocStart <= tailAllocator)
			{
				allocatableSize = tailAllocator - allocStart;
			}
		}
		frmHeap->ReleaseLock();
		return allocatableSize;
	}

	void* MEMiGetFreeStartForFrmHeap(MEMFrmHeap* heap)
	{
		if (!__FrmHeapDebug_IsValid(heap, "MEMiGetFreeStartForFrmHeap"))
			return nullptr;
		return heap->allocationHead;
	}

	void* MEMiGetFreeEndForFrmHeap(MEMFrmHeap* heap)
	{
		if (!__FrmHeapDebug_IsValid(heap, "MEMiGetFreeEndForFrmHeap"))
			return nullptr;
		return heap->allocationTail;
	}

	void* __FrmHeap_AllocFromHead(MEMFrmHeap* frmHeap, uint32 size, sint32 alignment)
	{
		uint32 head = frmHeap->allocationHead.GetMPTR();
		uint32 tail = frmHeap->allocationTail.GetMPTR();
		uint32 allocStart = (head + alignment - 1) & ~(alignment - 1);
		if ((allocStart + size) <= tail)
		{
			auto prevHead = frmHeap->allocationHead;
			frmHeap->allocationHead = allocStart + size;
			if (frmHeap->HasOptionClear())
				memset(prevHead.GetPtr(), 0, frmHeap->allocationHead.GetMPTR() - prevHead.GetMPTR());
			return MEMPTR<void>(allocStart).GetPtr();
		}
		return nullptr;
	}

	void* __FrmHeap_AllocFromTail(MEMFrmHeap* frmHeap, uint32 size, sint32 alignment)
	{
		uint32 head = frmHeap->allocationHead.GetMPTR();
		uint32 tail = frmHeap->allocationTail.GetMPTR();
		uint32 allocStart = (tail - size) & ~(alignment - 1);
		if (allocStart >= head)
		{
			auto prevTail = frmHeap->allocationTail;
			frmHeap->allocationTail = allocStart;
			if (frmHeap->HasOptionClear())
				memset(frmHeap->allocationTail.GetPtr(), 0, prevTail.GetMPTR() - frmHeap->allocationTail.GetMPTR());
			return MEMPTR<void>(allocStart).GetPtr();
		}
		return nullptr;
	}

	void* MEMAllocFromFrmHeapEx(MEMFrmHeap* frmHeap, uint32 size, sint32 alignment)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMAllocFromFrmHeapEx"))
			return nullptr;
		if (size == 0)
			size = 4;
		size = (size + 3) & ~3; // pad to 4 byte alignment
		frmHeap->AcquireLock();
		void* mem;
		if (alignment >= 0)
			mem = __FrmHeap_AllocFromHead(frmHeap, size, alignment);
		else
			mem = __FrmHeap_AllocFromTail(frmHeap, size, -alignment);
		frmHeap->ReleaseLock();
		return mem;
	}

	void __FrmHeap_FreeFromHead(MEMFrmHeap* frmHeap)
	{
		cemu_assert_debug(frmHeap->recordedStates.IsNull());
		frmHeap->recordedStates = nullptr;
		frmHeap->allocationHead = frmHeap->heapStart;
	}

	void __FrmHeap_FreeFromTail(MEMFrmHeap* frmHeap)
	{
		cemu_assert_debug(frmHeap->recordedStates.IsNull());
		frmHeap->recordedStates = nullptr;
		void* heapEnd = frmHeap->heapEnd.GetPtr();
		frmHeap->allocationTail = heapEnd;
	}

	void MEMFreeToFrmHeap(MEMFrmHeap* frmHeap, FrmHeapMode mode)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMFreeToFrmHeap"))
			return;
		frmHeap->AcquireLock();

		if ((mode & FrmHeapMode::Head) != 0)
			__FrmHeap_FreeFromHead(frmHeap);

		if ((mode & FrmHeapMode::Tail) != 0)
			__FrmHeap_FreeFromTail(frmHeap);

		frmHeap->ReleaseLock();
	}

	bool MEMRecordStateForFrmHeap(MEMFrmHeap* frmHeap, uint32 id)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMRecordStateForFrmHeap"))
			return false;
		frmHeap->AcquireLock();
		auto allocationHead = frmHeap->allocationHead;
		auto allocationTail = frmHeap->allocationTail;
		MEMFrmHeapRecordedState* rState = (MEMFrmHeapRecordedState*)__FrmHeap_AllocFromHead(frmHeap, sizeof(MEMFrmHeapRecordedState), 4); // modifies memHeap->allocationHead
		cemu_assert_debug(rState);
		if (!rState)
		{
			frmHeap->ReleaseLock();
			return false;
		}
		rState->id = id;
		rState->allocationHead = allocationHead;
		rState->allocationTail = allocationTail;
		rState->prevRecordedState = frmHeap->recordedStates;
		frmHeap->recordedStates = rState;
		frmHeap->ReleaseLock();
		return true;
	}

	bool MEMFreeByStateToFrmHeap(MEMFrmHeap* frmHeap, uint32 id)
	{
		if (!__FrmHeapDebug_IsValid(frmHeap, "MEMFreeByStateToFrmHeap"))
			return false;
		frmHeap->AcquireLock();
		// find matching state
		MEMFrmHeapRecordedState* rState = frmHeap->recordedStates.GetPtr();
		while (rState)
		{
			if (id == 0)
				break;
			if (rState->id == id)
				break;
			rState = rState->prevRecordedState.GetPtr();
		}
		if (!rState)
		{
			frmHeap->ReleaseLock();
			return false;
		}
		// apply state
		frmHeap->allocationHead = rState->allocationHead;
		frmHeap->allocationTail = rState->allocationTail;
		frmHeap->recordedStates = rState->prevRecordedState;
		frmHeap->ReleaseLock();
		return true;
	}

	void InitializeMEMFrmHeap()
	{
		cafeExportRegister("coreinit", MEMCreateFrmHeapEx, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMDestroyFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMGetAllocatableSizeForFrmHeapEx, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMiGetFreeStartForFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMiGetFreeEndForFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMAllocFromFrmHeapEx, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMFreeToFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMFreeToFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMRecordStateForFrmHeap, LogType::CoreinitMem);
		cafeExportRegister("coreinit", MEMFreeByStateToFrmHeap, LogType::CoreinitMem);
	}
}