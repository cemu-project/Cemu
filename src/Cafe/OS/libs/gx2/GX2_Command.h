#pragma once
#include "Cafe/HW/Latte/ISA/LatteReg.h"
#include "Cafe/HW/Espresso/Const.h"

struct GX2WriteGatherPipeState
{
	uint8* gxRingBuffer;
	// each core has it's own write gatherer and display list state (writing)
	uint8* writeGatherPtrGxBuffer[Espresso::CORE_COUNT];
	uint8** writeGatherPtrWrite[Espresso::CORE_COUNT];
	uint8* writeGatherPtrDisplayList[Espresso::CORE_COUNT];
	MPTR displayListStart[Espresso::CORE_COUNT];
	uint32 displayListMaxSize[Espresso::CORE_COUNT];
};

extern GX2WriteGatherPipeState gx2WriteGatherPipe;

void GX2ReserveCmdSpace(uint32 reservedFreeSpaceInU32); // move to GX2 namespace eventually

void gx2WriteGather_submitU32AsBE(uint32 v);
void gx2WriteGather_submitU32AsLE(uint32 v);
void gx2WriteGather_submitU32AsLEArray(uint32* v, uint32 numValues);

uint32 PPCInterpreter_getCurrentCoreIndex();

// gx2WriteGather_submit functions
template <typename ...Targs>
inline void gx2WriteGather_submit_(uint32 coreIndex, uint32be* writePtr)
{
	(*gx2WriteGatherPipe.writeGatherPtrWrite[coreIndex]) = (uint8*)writePtr;
}

template <typename T, typename ...Targs>
inline void gx2WriteGather_submit_(uint32 coreIndex, uint32be* writePtr, const betype<T>& arg, Targs... args)
{
	static_assert(sizeof(betype<T>) == sizeof(uint32be));
	*(betype<T>*)writePtr = arg;
	writePtr++;
	gx2WriteGather_submit_(coreIndex, writePtr, args...);
}

template <typename T, typename ...Targs>
inline
typename std::enable_if< std::is_floating_point<T>::value, void>::type
gx2WriteGather_submit_(uint32 coreIndex, uint32be* writePtr, const T& arg, Targs... args)
{
	static_assert(sizeof(T) == sizeof(uint32));
	*writePtr = *(uint32*)&arg;
	writePtr++;
	gx2WriteGather_submit_(coreIndex, writePtr, args...);
}

template <typename T, typename ...Targs>
inline
typename std::enable_if< std::is_base_of<Latte::LATTEREG, T>::value, void>::type
gx2WriteGather_submit_(uint32 coreIndex, uint32be* writePtr, const T& arg, Targs... args)
{
	static_assert(sizeof(Latte::LATTEREG) == sizeof(uint32be));
	*writePtr = arg.getRawValue();
	writePtr++;
	gx2WriteGather_submit_(coreIndex, writePtr, args...);
}

template <typename T, typename ...Targs>
inline
typename std::enable_if< !std::is_base_of<Latte::LATTEREG, T>::value && !std::is_floating_point<T>::value, void>::type
gx2WriteGather_submit_(uint32 coreIndex, uint32be* writePtr, const T& arg, Targs... args)
{
	*writePtr = arg;
	writePtr++;
	gx2WriteGather_submit_(coreIndex, writePtr, args...);
}

template <typename ...Targs>
inline void gx2WriteGather_submit(Targs... args)
{
	uint32 coreIndex = PPCInterpreter_getCurrentCoreIndex();
	if (gx2WriteGatherPipe.writeGatherPtrWrite[coreIndex] == nullptr)
		return;

	uint32be* writePtr = (uint32be*)(*gx2WriteGatherPipe.writeGatherPtrWrite[coreIndex]);
	gx2WriteGather_submit_(coreIndex, writePtr, std::forward<Targs>(args)...);
}

namespace GX2
{

	bool GX2WriteGather_isDisplayListActive();
	uint32 GX2WriteGather_getReadWriteDistance();
	void GX2WriteGather_checkAndInsertWrapAroundMark();

	void GX2BeginDisplayList(MEMPTR<void> displayListAddr, uint32 size);
	void GX2BeginDisplayListEx(MEMPTR<void> displayListAddr, uint32 size, bool profiling);
	uint32 GX2EndDisplayList(MEMPTR<void> displayListAddr);

	void GX2CallDisplayList(MPTR addr, uint32 size);
	void GX2DirectCallDisplayList(void* addr, uint32 size);

	void GX2Init_writeGather();
    void GX2CommandInit();
    void GX2CommandResetToDefaultState();
}