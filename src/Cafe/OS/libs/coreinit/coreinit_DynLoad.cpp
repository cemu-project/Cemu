#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/coreinit/coreinit_DynLoad.h"
#include "Cafe/OS/libs/coreinit/coreinit_MEM.h"

namespace coreinit
{
	MPTR _osDynLoadFuncAlloc = MPTR_NULL;
	MPTR _osDynLoadFuncFree = MPTR_NULL;
	MPTR _osDynLoadTLSFuncAlloc = MPTR_NULL;
	MPTR _osDynLoadTLSFuncFree = MPTR_NULL;

	uint32 OSDynLoad_SetAllocator(MPTR allocFunc, MPTR freeFunc)
	{
		_osDynLoadFuncAlloc = allocFunc;
		_osDynLoadFuncFree = freeFunc;
		return 0;
	}

	void OSDynLoad_SetTLSAllocator(MPTR allocFunc, MPTR freeFunc)
	{
		_osDynLoadTLSFuncAlloc = allocFunc;
		_osDynLoadTLSFuncFree = freeFunc;
	}

	uint32 OSDynLoad_GetAllocator(betype<MPTR>* funcAlloc, betype<MPTR>* funcFree)
	{
		*funcAlloc = _osDynLoadFuncAlloc;
		*funcFree = _osDynLoadFuncFree;
		return 0;
	}

	void OSDynLoad_GetTLSAllocator(betype<MPTR>* funcAlloc, betype<MPTR>* funcFree)
	{
		*funcAlloc = _osDynLoadTLSFuncAlloc;
		*funcFree = _osDynLoadTLSFuncFree;
	}

	void* OSDynLoad_AllocatorAlloc(sint32 size, sint32 alignment)
	{
		if (_osDynLoadFuncAlloc == MPTR_NULL)
			return MPTR_NULL;
		StackAllocator<MEMPTR<void>> _ptrStorage;
		int r = PPCCoreCallback(_osDynLoadFuncAlloc, size, alignment, _ptrStorage.GetMPTR());
		if (r != 0)
		{
			cemu_assert_debug(false);
			return MPTR_NULL;
		}
		return _ptrStorage->GetPtr();
	}

	void OSDynLoad_AllocatorFree(void* mem)
	{
		if (_osDynLoadFuncFree == MPTR_NULL)
			return;
		MEMPTR<void> _mem = mem;
		PPCCoreCallback(_osDynLoadFuncFree, _mem);
	}

	uint32 OSDynLoad_Acquire(const char* libName, uint32be* moduleHandleOut)
	{
		// truncate path
		sint32 fileNameStartIndex = 0;
		sint32 tempLen = (sint32)strlen(libName);
		for (sint32 i = tempLen - 1; i >= 0; i--)
		{
			if (libName[i] == '/')
			{
				fileNameStartIndex = i + 1;
				break;
			}
		}
		// truncate file extension 
		char tempLibName[512];
		strcpy(tempLibName, libName + fileNameStartIndex);
		tempLen = (sint32)strlen(tempLibName);
		for (sint32 i = tempLen - 1; i >= 0; i--)
		{
			if (tempLibName[i] == '.')
			{
				tempLibName[i] = '\0';
				break;
			}
		}
		// search for loaded modules with matching name
		uint32 rplHandle = RPLLoader_GetHandleByModuleName(libName);
		if (rplHandle == RPL_INVALID_HANDLE && !RPLLoader_HasDependency(libName))
		{
			RPLLoader_AddDependency(libName);
			RPLLoader_UpdateDependencies();
			RPLLoader_Link();
			RPLLoader_CallEntrypoints();
			rplHandle = RPLLoader_GetHandleByModuleName(libName);
		}
		if (rplHandle == RPL_INVALID_HANDLE)
			*moduleHandleOut = 0;
		else
			*moduleHandleOut = rplHandle;
		if (rplHandle == RPL_INVALID_HANDLE)
		{
			cemuLog_logDebug(LogType::Force, "OSDynLoad_Acquire() failed to load module '{}'", libName);
			return 0xFFFCFFE9; // module not found
		}
		return 0;
	}

	void OSDynLoad_Release(uint32 moduleHandle)
	{
		if (moduleHandle == RPL_INVALID_HANDLE)
			return;
		RPLLoader_RemoveDependency(moduleHandle);
		RPLLoader_UpdateDependencies();
	}

	uint32 OSDynLoad_FindExport(uint32 moduleHandle, uint32 isData, const char* exportName, betype<MPTR>* addrOut)
	{
		if (moduleHandle == 0xFFFFFFFF)
		{
			// main module
			// Assassins Creed 4 has this handle hardcoded
			moduleHandle = RPLLoader_GetMainModuleHandle();
		}

		MPTR exportResult = RPLLoader_FindModuleOrHLEExport(moduleHandle, isData, exportName);
		*addrOut = exportResult;

		if (exportResult == MPTR_NULL)
			return 0xFFFFFFFF;
		return 0;
	}

	void InitializeDynLoad()
	{
		cafeExportRegister("coreinit", OSDynLoad_SetAllocator, LogType::Placeholder);
		cafeExportRegister("coreinit", OSDynLoad_SetTLSAllocator, LogType::Placeholder);
		cafeExportRegister("coreinit", OSDynLoad_GetAllocator, LogType::Placeholder);
		cafeExportRegister("coreinit", OSDynLoad_GetTLSAllocator, LogType::Placeholder);

		cafeExportRegister("coreinit", OSDynLoad_Acquire, LogType::Placeholder);
		cafeExportRegister("coreinit", OSDynLoad_Release, LogType::Placeholder);
		cafeExportRegister("coreinit", OSDynLoad_FindExport, LogType::Placeholder);
	}
}