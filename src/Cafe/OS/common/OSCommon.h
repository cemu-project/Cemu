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

// va_list
struct ppc_va_list
{
	uint8be gprIndex;
	uint8be fprIndex;
	uint8be _padding2[2];
	MEMPTR<uint8be> overflow_arg_area;
	MEMPTR<uint8be> reg_save_area;
};
static_assert(sizeof(ppc_va_list) == 0xC);

struct ppc_va_list_reg_storage
{
	uint32be gpr_save_area[8]; // 32 bytes, r3 to r10
	float64be fpr_save_area[8]; // 64 bytes, f1 to f8
	ppc_va_list vargs;
	uint32be padding;
};
static_assert(sizeof(ppc_va_list_reg_storage) == 0x70);

// Equivalent of va_start for PPC HLE functions. Must be called before any StackAllocator<> definitions
#define ppc_define_va_list(__gprIndex, __fprIndex) \
	MPTR vaOriginalR1 = PPCInterpreter_getCurrentInstance()->gpr[1]; \
	StackAllocator<ppc_va_list_reg_storage> va_list_storage; \
	for(int i=3; i<=10; i++) va_list_storage->gpr_save_area[i-3] = PPCInterpreter_getCurrentInstance()->gpr[i]; \
	for(int i=1; i<=8; i++) va_list_storage->fpr_save_area[i-1] = PPCInterpreter_getCurrentInstance()->fpr[i].fp0; \
	va_list_storage->vargs.gprIndex = __gprIndex; \
	va_list_storage->vargs.fprIndex = __fprIndex; \
	va_list_storage->vargs.reg_save_area = (uint8be*)&va_list_storage; \
	va_list_storage->vargs.overflow_arg_area = {vaOriginalR1 + 8}; \
	ppc_va_list& vargs = va_list_storage->vargs;

enum class ppc_va_type
{
	INT32 = 1,
	INT64 = 2,
	FLOAT_OR_DOUBLE = 3,
};

static void* _ppc_va_arg(ppc_va_list* vargs, ppc_va_type argType)
{
	void* r;
	switch ( argType )
	{
	default:
		cemu_assert_suspicious();
	case ppc_va_type::INT32:
		if ( vargs[0].gprIndex < 8u )
		{
			r = &vargs->reg_save_area[4 * vargs->gprIndex];
			vargs->gprIndex++;
			return r;
		}
		r = vargs->overflow_arg_area;
		vargs->overflow_arg_area += 4;
		return r;
	case ppc_va_type::INT64:
		if ( (vargs->gprIndex & 1) != 0 )
			vargs->gprIndex++;
		if ( vargs->gprIndex < 8 )
		{
			r = &vargs->reg_save_area[4 * vargs->gprIndex];
			vargs->gprIndex += 2;
			return r;
		}
		vargs->overflow_arg_area = {(vargs->overflow_arg_area.GetMPTR()+7) & 0xFFFFFFF8};
		r = vargs->overflow_arg_area;
		vargs->overflow_arg_area += 8;
		return r;
	case ppc_va_type::FLOAT_OR_DOUBLE:
		if ( vargs->fprIndex < 8 )
		{
			r = &vargs->reg_save_area[0x20 + 8 * vargs->fprIndex];
			vargs->fprIndex++;
			return r;
		}
		vargs->overflow_arg_area = {(vargs->overflow_arg_area.GetMPTR()+7) & 0xFFFFFFF8};
		r = vargs->overflow_arg_area;
		vargs->overflow_arg_area += 8;
		return r;
	}
}