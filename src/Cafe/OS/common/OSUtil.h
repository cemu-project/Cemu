#pragma once
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/MMU/MMU.h"

#include <fmt/ostream.h>
#include <fmt/compile.h>
#include <fmt/ranges.h>

class cafeExportParamWrapper
{
public:
	template <typename T>
	static void getParamWrapper(PPCInterpreter_t* hCPU, int& gprIndex, int& fprIndex, T& v)
	{
		if constexpr (std::is_pointer_v<T>)
		{
			uint32be addr;
			if (gprIndex >= 8)
				addr = memory_readU32(hCPU->gpr[1] + 8 + (gprIndex - 8) * 4);
			else
				addr = hCPU->gpr[3 + gprIndex];

			using TPtr = std::remove_pointer_t<T>;
			v = MEMPTR<TPtr>(addr).GetPtr();
			gprIndex++;
		}
		else if constexpr (std::is_base_of_v<MEMPTRBase, T>)
		{
			uint32be addr;
			if (gprIndex >= 8)
				addr = memory_readU32(hCPU->gpr[1] + 8 + (gprIndex - 8) * 4);
			else
				addr = hCPU->gpr[3 + gprIndex];

			v = addr.value();
			gprIndex++;
		}
		else if constexpr (std::is_enum_v<T>)
		{
			using TEnum = std::underlying_type_t<T>;
			getParamWrapper<TEnum>(hCPU, gprIndex, fprIndex, (TEnum&)v);
		}
		else if constexpr (std::is_integral_v<T>)
		{
			if constexpr (sizeof(T) == sizeof(uint64))
			{
				gprIndex = (gprIndex + 1)&~1;
				if (gprIndex >= 8)
					v = (T)memory_readU64(hCPU->gpr[1] + 8 + (gprIndex - 8) * 4);
				else
					v = (T)(((uint64)hCPU->gpr[3 + gprIndex]) << 32) | ((uint64)hCPU->gpr[3 + gprIndex + 1]);

				gprIndex += 2;
			}
			else
			{
				if (gprIndex >= 8)
					v = (T)memory_readU32(hCPU->gpr[1] + 8 + (gprIndex - 8) * 4);
				else
					v = (T)hCPU->gpr[3 + gprIndex];

				gprIndex++;
			}
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			v = (T)hCPU->fpr[1 + fprIndex].fpr;
			fprIndex++;
		}
		else
		{
			assert_dbg();
		}
	}

	template<typename T>
	static void setReturnResult(PPCInterpreter_t* hCPU, T r)
	{
		if constexpr (std::is_pointer_v<T>)
		{
			hCPU->gpr[3] = MEMPTR(r).GetMPTR();
		}
		else if constexpr (std::is_reference_v<T>)
		{
			hCPU->gpr[3] = MEMPTR(&r).GetMPTR();
		}
		else if constexpr (std::is_enum_v<T>)
		{
			using TEnum = std::underlying_type_t<T>;
			setReturnResult<TEnum>(hCPU, (TEnum)r);
		}
		else if constexpr (std::is_integral_v<T>)
		{
			if constexpr(sizeof(T) == 8)
			{
				const auto t = static_cast<uint64>(r);
				hCPU->gpr[3] = (uint32)(t >> 32); // high
				hCPU->gpr[4] = (uint32)(t); // low
			}
			else
			{
				hCPU->gpr[3] = (uint32)r;
			}
		}
		else
		{
			cemu_assert_unimplemented();
			//static_assert(false);
		}
	}

	template<typename T>
	static auto getFormatResult(T r)
	{
		if constexpr (std::is_pointer_v<T>)
			return MEMPTR(r).GetMPTR();
		else if constexpr (std::is_enum_v<T>)
			return static_cast<std::underlying_type_t<T>>(r);
		else if constexpr(!std::is_fundamental_v<T>)
			return MEMPTR(&r).GetMPTR();
		else 
			return r;
	}
};

template<typename T>
T cafeExportGetParamWrapper(PPCInterpreter_t* hCPU, int& gprIndex, int& fprIndex)
{
	T v;
	cafeExportParamWrapper::getParamWrapper(hCPU, gprIndex, fprIndex, v);
	return v;
}

template <typename R, typename ... Args>
static std::tuple<Args...> cafeExportBuildArgTuple(PPCInterpreter_t* hCPU, R(fn)(Args...))
{
	int gprIndex = 0;
	int fprIndex = 0;
	return std::tuple<Args...>{ cafeExportGetParamWrapper<Args>(hCPU, gprIndex, fprIndex)... };
}

template<typename T>
using _CAFE_FORMAT_ARG = std::conditional_t<std::is_pointer_v<T>,
	std::conditional_t<std::is_same_v<T, char*> || std::is_same_v<T, const char*>, T, MEMPTR<T>>, T>;

template <typename R, typename... Args>
static auto cafeExportBuildFormatTuple(PPCInterpreter_t* hCPU, R(fn)(Args...))
{
	int gprIndex = 0;
	int fprIndex = 0;
	return std::tuple<_CAFE_FORMAT_ARG<Args>...>{
		cafeExportGetParamWrapper<_CAFE_FORMAT_ARG<Args>>(hCPU, gprIndex, fprIndex)...
	};
}

template<auto fn, typename TNames, LogType TLogType>
void cafeExportCallWrapper(PPCInterpreter_t* hCPU)
{
	auto tup = cafeExportBuildArgTuple(hCPU, fn);
	bool shouldLog = false;
	if (cemuLog_isLoggingEnabled(TLogType))
	{
		const auto format_tup = cafeExportBuildFormatTuple(hCPU, fn);
		if(cemuLog_advancedPPCLoggingEnabled())
		{
			MPTR threadMPTR = memory_getVirtualOffsetFromPointer(coreinit::OSGetCurrentThread());
			if constexpr (std::tuple_size<decltype(format_tup)>::value > 0)
				shouldLog = cemuLog_log(TLogType, "{}.{}{} # LR: {:#x} | Thread: {:#x}", TNames::GetLib(), TNames::GetFunc(), format_tup, hCPU->spr.LR, threadMPTR);
			else
				shouldLog = cemuLog_log(TLogType, "{}.{}() # LR: {:#x} | Thread: {:#x}", TNames::GetLib(), TNames::GetFunc(), hCPU->spr.LR, threadMPTR);
		}
		else
		{
			if constexpr (std::tuple_size<decltype(format_tup)>::value > 0)
			{
				shouldLog = cemuLog_log(TLogType, "{}.{}{}", TNames::GetLib(), TNames::GetFunc(), format_tup);
			}
			else
				shouldLog = cemuLog_log(TLogType, "{}.{}()", TNames::GetLib(), TNames::GetFunc());
		}
	}

	if constexpr (!std::is_void<decltype(std::apply(fn, tup))>::value)
	{
		// has non-void return type
		decltype(auto) result = std::apply(fn, tup);
		cafeExportParamWrapper::setReturnResult<decltype(std::apply(fn, tup))>(hCPU, result);
		if(shouldLog)
			cemuLog_log(TLogType, "\t\t{}.{} -> {}", TNames::GetLib(), TNames::GetFunc(), cafeExportParamWrapper::getFormatResult(result));
	}
	else
	{
		// return type is void
		std::apply(fn, tup);
	}
	// return from func
	hCPU->instructionPointer = hCPU->spr.LR;
}

void osLib_addFunctionInternal(const char* libraryName, const char* functionName, void(*osFunction)(PPCInterpreter_t* hCPU));

template<auto fn, typename TNames, LogType TLogType>
void cafeExportMakeWrapper(const char* libname, const char* funcname)
{
	osLib_addFunctionInternal(libname, funcname, &cafeExportCallWrapper<fn, TNames, TLogType>);
}

#define cafeExportRegister(__libname, __func, __logtype) \
	 { \
		struct StringWrapper { \
			static const char* GetLib() { return __libname; }; \
			static const char* GetFunc() { return #__func; }; \
		}; \
		cafeExportMakeWrapper<__func, StringWrapper, __logtype>(__libname, # __func);\
	}

#define cafeExportRegisterFunc(__func, __libname, __funcname, __logtype) \
	 {\
		struct StringWrapper { \
			static const char* GetLib() { return __libname; }; \
			static const char* GetFunc() { return __funcname; }; \
		}; \
		cafeExportMakeWrapper<__func, StringWrapper, __logtype>(__libname, __funcname);\
	}

template<auto fn>
MPTR makeCallableExport()
{
	return PPCInterpreter_makeCallableExportDepr(&cafeExportCallWrapper<fn, "CALLABLE_EXPORT">);
}

void osLib_addVirtualPointer(const char* libraryName, const char* functionName, uint32 vPtr);