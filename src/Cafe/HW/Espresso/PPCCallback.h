#pragma once
#include "PPCState.h"

struct PPCCoreCallbackData_t
{
	sint32 gprCount = 0;
	sint32 floatCount = 0;
};

// callback functions
inline uint32 PPCCoreCallback(MPTR function, const PPCCoreCallbackData_t& data)
{
	return PPCCore_executeCallbackInternal(function)->gpr[3];
}

template <typename T, typename... TArgs>
uint32 PPCCoreCallback(MPTR function, PPCCoreCallbackData_t& data, T currentArg, TArgs... args)
{
	cemu_assert_debug(data.gprCount <= 8);
	cemu_assert_debug(data.floatCount <= 8);
	PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
	if constexpr (std::is_pointer_v<T>)
	{
		hCPU->gpr[3 + data.gprCount] = MEMPTR(currentArg).GetMPTR();
		data.gprCount++;
	}
	else if constexpr (std::is_base_of_v<MEMPTRBase, std::remove_reference_t<T>>)
	{
		hCPU->gpr[3 + data.gprCount] = currentArg.GetMPTR();
		data.gprCount++;
	}
	else if constexpr (std::is_reference_v<T>)
	{
		hCPU->gpr[3 + data.gprCount] = MEMPTR(&currentArg).GetMPTR();
		data.gprCount++;
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
		hCPU->gpr[3 + data.gprCount] = (uint32)currentArg;
		data.gprCount++;
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
