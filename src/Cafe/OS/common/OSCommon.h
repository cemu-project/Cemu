#pragma once

struct PPCInterpreter_t;


#define OSLIB_FUNCTIONTABLE_TYPE_FUNCTION	(1)
#define OSLIB_FUNCTIONTABLE_TYPE_POINTER	(2)

void osLib_load();
void osLib_generateHashFromName(const char* name, uint32* hashA, uint32* hashB);
sint32 osLib_getFunctionIndex(const char* libraryName, const char* functionName);
uint32 osLib_getPointer(const char* libraryName, const char* functionName);

void osLib_addFunctionInternal(const char* libraryName, const char* functionName, void(*osFunction)(PPCInterpreter_t* hCPU));
#define osLib_addFunction(__p1, __p2, __p3) osLib_addFunctionInternal((const char*)__p1, __p2, __p3)
void osLib_addVirtualPointer(const char* libraryName, const char* functionName, uint32 vPtr);

void osLib_returnFromFunction(PPCInterpreter_t* hCPU, uint32 returnValue);
void osLib_returnFromFunction64(PPCInterpreter_t* hCPU, uint64 returnValue64);

// libs
#include "Cafe/OS/libs/coreinit/coreinit.h"

// utility functions
#include "Cafe/OS/common/OSUtil.h"
