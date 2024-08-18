#pragma once
#include "PPCState.h"

struct PPCCoreCallbackData_t
{
	sint32 gprCount = 0;
	sint32 floatCount = 0;
	sint32 stackCount = 0;
};

inline void _PPCCoreCallback_writeGPRArg(PPCCoreCallbackData_t& data, PPCInterpreter_t* hCPU, uint32 value)
{
	if (data.gprCount < 8)
	{
		hCPU->gpr[3 + data.gprCount] = value;
		data.gprCount++;
	}
	else
	{
		uint32 stackOffset = 8 + data.stackCount * 4;

		// PPCCore_executeCallbackInternal does -16*4 to save the current stack area
		stackOffset -= 16 * 4;

		memory_writeU32(hCPU->gpr[1] + stackOffset, value);
		data.stackCount++;
	}
}

// callback functions
inline uint32 PPCCoreCallback(MPTR function, const PPCCoreCallbackData_t& data)
{
	return PPCCore_executeCallbackInternal(function)->gpr[3];
}

template <typename T, typename... TArgs>
uint32 PPCCoreCallback(MPTR function, PPCCoreCallbackData_t& data, T currentArg, TArgs... args)
{
	// TODO float arguments on stack
	cemu_assert_debug(data.floatCount < 8);

	PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
	if constexpr (std::is_pointer_v<T>)
	{
		_PPCCoreCallback_writeGPRArg(data, hCPU, MEMPTR(currentArg).GetMPTR());
	}
	else if constexpr (std::is_base_of_v<MEMPTRBase, std::remove_reference_t<T>>)
	{
		_PPCCoreCallback_writeGPRArg(data, hCPU, currentArg.GetMPTR());
	}
	else if constexpr (std::is_reference_v<T>)
	{
		_PPCCoreCallback_writeGPRArg(data, hCPU, MEMPTR(&currentArg).GetMPTR());
	}
	else if constexpr(std::is_enum_v<T>)
	{
		using TEnum = typename std::underlying_type<T>::type;
		return PPCCoreCallback<TEnum>(function, data, (TEnum)currentArg, std::forward<TArgs>(args)...);
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		hCPU->fpr[1 + data.floatCount].fpr = (double)currentArg;
		data.floatCount++;
	}
	else if constexpr (std::is_integral_v<T> && sizeof(T) == sizeof(uint64))
	{
		hCPU->gpr[3 + data.gprCount] = (uint32)(currentArg >> 32); // high
		hCPU->gpr[3 + data.gprCount + 1] = (uint32)currentArg; // low

		data.gprCount += 2;
	}
	else
	{
		_PPCCoreCallback_writeGPRArg(data, hCPU, (uint32)currentArg);
	}
	
	return PPCCoreCallback(function, data, args...);
}

template <typename... TArgs>
uint32 PPCCoreCallback(MPTR function, TArgs... args)
{
	PPCCoreCallbackData_t data{};
	return PPCCoreCallback(function, data, std::forward<TArgs>(args)...);
}

template <typename... TArgs>
uint32 PPCCoreCallback(void* functionPtr, TArgs... args)
{
	MEMPTR<void> _tmp{ functionPtr };
	PPCCoreCallbackData_t data{};
	return PPCCoreCallback(_tmp.GetMPTR(), data, std::forward<TArgs>(args)...);
}
