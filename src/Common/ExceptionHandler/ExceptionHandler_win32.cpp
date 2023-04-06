#include "Common/precompiled.h"
#include "Cafe/CafeSystem.h"
#include "ExceptionHandler.h"

#include <Windows.h>
#include <Dbghelp.h>
#include <shellapi.h>

#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"

LONG handleException_SINGLE_STEP(PEXCEPTION_POINTERS pExceptionInfo)
{
	return EXCEPTION_CONTINUE_SEARCH;
}

#include <boost/algorithm/string.hpp>
BOOL CALLBACK MyMiniDumpCallback(PVOID pParam, const PMINIDUMP_CALLBACK_INPUT pInput, PMINIDUMP_CALLBACK_OUTPUT pOutput)
{
	if (!pInput || !pOutput)
		return FALSE;

	switch (pInput->CallbackType)
	{
	case IncludeModuleCallback:
	case IncludeThreadCallback:
	case ThreadCallback:
	case ThreadExCallback:
		return TRUE;

	case ModuleCallback:

		if (!(pOutput->ModuleWriteFlags & ModuleReferencedByMemory))
			pOutput->ModuleWriteFlags &= ~ModuleWriteModule;

		return TRUE;
	}

	return FALSE;

}

bool CreateMiniDump(CrashDump dump, EXCEPTION_POINTERS* pep)
{
	if (dump == CrashDump::Disabled)
		return true;

	fs::path p = ActiveSettings::GetUserDataPath("crashdump");

	std::error_code ec;
	fs::create_directories(p, ec);
	if (ec)
		return false;

	const auto now = std::chrono::system_clock::now();
	const auto temp_time = std::chrono::system_clock::to_time_t(now);
	const auto& time = *std::gmtime(&temp_time);

	p /= fmt::format("crash_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.dmp", 1900 + time.tm_year, time.tm_mon + 1, time.tm_mday, time.tm_year, time.tm_hour, time.tm_min, time.tm_sec);

	const auto hFile = CreateFileW(p.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	MINIDUMP_EXCEPTION_INFORMATION mdei;
	mdei.ThreadId = GetCurrentThreadId();
	mdei.ExceptionPointers = pep;
	mdei.ClientPointers = FALSE;

	MINIDUMP_CALLBACK_INFORMATION mci;
	mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)MyMiniDumpCallback;
	mci.CallbackParam = nullptr;

	MINIDUMP_TYPE mdt;
	if (dump == CrashDump::Full)
	{
		mdt = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory |
			MiniDumpWithDataSegs |
			MiniDumpWithHandleData |
			MiniDumpWithFullMemoryInfo |
			MiniDumpWithThreadInfo |
			MiniDumpWithUnloadedModules);
	}
	else
	{
		mdt = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
	}

	const auto result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, &mdei, nullptr, &mci);
	CloseHandle(hFile);
	return result != FALSE;
}

void DumpThreadStackTrace()
{
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	char dumpLine[1024 * 4];
	void* stack[100];

	const unsigned short frames = CaptureStackBackTrace(0, 40, stack, NULL);
	SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    CrashLog_WriteHeader("Stack trace");
	for (unsigned int i = 0; i < frames; i++)
	{
		DWORD64 stackTraceOffset = (DWORD64)stack[i];
		SymFromAddr(process, stackTraceOffset, 0, symbol);
		sprintf(dumpLine, "0x%016I64x ", (uint64)(size_t)stack[i]);
		cemuLog_writePlainToLog(dumpLine);
		// module name
		HMODULE stackModule;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)stackTraceOffset, &stackModule))
		{
			char moduleName[512];
			moduleName[0] = '\0';
			GetModuleFileNameA(stackModule, moduleName, 512);
			sint32 moduleNameStartIndex = std::max((sint32)0, (sint32)strlen(moduleName) - 1);
			while (moduleNameStartIndex > 0)
			{
				if (moduleName[moduleNameStartIndex] == '\\' || moduleName[moduleNameStartIndex] == '/')
				{
					moduleNameStartIndex++;
					break;
				}
				moduleNameStartIndex--;
			}

			DWORD64 moduleAddress = (DWORD64)GetModuleHandleA(moduleName);
			sint32 relativeOffset = 0;
			if (moduleAddress != 0)
				relativeOffset = stackTraceOffset - moduleAddress;

			sprintf(dumpLine, "+0x%08x %-16s", relativeOffset, moduleName + moduleNameStartIndex);
			cemuLog_writePlainToLog(dumpLine);
		}
		else
		{
			sprintf(dumpLine, "+0x00000000 %-16s", "NULL");
			cemuLog_writePlainToLog(dumpLine);
		}
		// function name
		sprintf(dumpLine, " %s\n", symbol->Name);
		cemuLog_writePlainToLog(dumpLine);
	}

	free(symbol);
}

void createCrashlog(EXCEPTION_POINTERS* e, PCONTEXT context)
{
    if(!CrashLog_Create())
        return; // give up if crashlog was already created

	const auto crash_dump = GetConfig().crash_dump.GetValue();
	const auto dump_written = CreateMiniDump(crash_dump, e);
	if (!dump_written)
		cemuLog_writeLineToLog(fmt::format("couldn't write minidump {:#x}", GetLastError()), false, true);

	char dumpLine[1024 * 4];

	// info about Cemu version
	sprintf(dumpLine, "\nCrashlog for %s\n", BUILD_VERSION_WITH_NAME_STRING);
	cemuLog_writePlainToLog(dumpLine);

	SYSTEMTIME sysTime;
	GetSystemTime(&sysTime);
	sprintf(dumpLine, "Date: %02d-%02d-%04d %02d:%02d:%02d\n\n", (sint32)sysTime.wDay, (sint32)sysTime.wMonth, (sint32)sysTime.wYear, (sint32)sysTime.wHour, (sint32)sysTime.wMinute, (sint32)sysTime.wSecond);
	cemuLog_writePlainToLog(dumpLine);

	DumpThreadStackTrace();
	// info about exception
	if (e->ExceptionRecord)
	{
		HMODULE exceptionModule;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)(e->ExceptionRecord->ExceptionAddress), &exceptionModule))
		{
			char moduleName[512];
			moduleName[0] = '\0';
			GetModuleFileNameA(exceptionModule, moduleName, 512);
			sint32 moduleNameStartIndex = std::max((sint32)0, (sint32)strlen(moduleName) - 1);
			while (moduleNameStartIndex > 0)
			{
				if (moduleName[moduleNameStartIndex] == '\\' || moduleName[moduleNameStartIndex] == '/')
				{
					moduleNameStartIndex++;
					break;
				}
				moduleNameStartIndex--;
			}
			sprintf(dumpLine, "Exception 0x%08x at 0x%I64x(+0x%I64x) in module %s\n", (uint32)e->ExceptionRecord->ExceptionCode, (uint64)e->ExceptionRecord->ExceptionAddress, (uint64)e->ExceptionRecord->ExceptionAddress - (uint64)exceptionModule, moduleName + moduleNameStartIndex);
			cemuLog_writePlainToLog(dumpLine);
		}
		else
		{
			sprintf(dumpLine, "Exception 0x%08x at 0x%I64x\n", (uint32)e->ExceptionRecord->ExceptionCode, (uint64)e->ExceptionRecord->ExceptionAddress);
			cemuLog_writePlainToLog(dumpLine);
		}

	}
	sprintf(dumpLine, "cemu.exe at 0x%I64x\n", (uint64)GetModuleHandle(NULL));
	cemuLog_writePlainToLog(dumpLine);
	// register info
	sprintf(dumpLine, "\n");
	cemuLog_writePlainToLog(dumpLine);
	sprintf(dumpLine, "RAX=%016I64x RBX=%016I64x RCX=%016I64x RDX=%016I64x\n", context->Rax, context->Rbx, context->Rcx, context->Rdx);
	cemuLog_writePlainToLog(dumpLine);
	sprintf(dumpLine, "RSP=%016I64x RBP=%016I64x RDI=%016I64x RSI=%016I64x\n", context->Rsp, context->Rbp, context->Rdi, context->Rsi);
	cemuLog_writePlainToLog(dumpLine);
	sprintf(dumpLine, "R8 =%016I64x R9 =%016I64x R10=%016I64x R11=%016I64x\n", context->R8, context->R9, context->R10, context->R11);
	cemuLog_writePlainToLog(dumpLine);
	sprintf(dumpLine, "R12=%016I64x R13=%016I64x R14=%016I64x R15=%016I64x\n", context->R12, context->R13, context->R14, context->R15);
	cemuLog_writePlainToLog(dumpLine);

    CrashLog_SetOutputChannels(false, true);
    ExceptionHandler_LogGeneralInfo();
    CrashLog_SetOutputChannels(true, true);

	cemuLog_waitForFlush();

	// save log with the dump
	if (dump_written && crash_dump != CrashDump::Disabled)
	{
		const auto now = std::chrono::system_clock::now();
		const auto temp_time = std::chrono::system_clock::to_time_t(now);
		const auto& time = *std::gmtime(&temp_time);

		fs::path p = ActiveSettings::GetUserDataPath("crashdump");
		p /= fmt::format("log_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.txt", 1900 + time.tm_year, time.tm_mon + 1, time.tm_mday, time.tm_year, time.tm_hour, time.tm_min, time.tm_sec);

		std::error_code ec;
		fs::copy_file(ActiveSettings::GetUserDataPath("log.txt"), p, ec);
	}

	exit(0);

	return;
}

bool logCrashlog;

int crashlogThread(void* exceptionInfoRawPtr)
{
	PEXCEPTION_POINTERS pExceptionInfo = (PEXCEPTION_POINTERS)exceptionInfoRawPtr;
	createCrashlog(pExceptionInfo, pExceptionInfo->ContextRecord);
	logCrashlog = true;
	return 0;
}

void debugger_handleSingleStepException(uint64 dr6);


LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
	{
		LONG r = handleException_SINGLE_STEP(pExceptionInfo);
		if (r != EXCEPTION_CONTINUE_SEARCH)
			return r;

		if (GetBits(pExceptionInfo->ContextRecord->Dr6, 0, 1) || GetBits(pExceptionInfo->ContextRecord->Dr6, 1, 1))
			debugger_handleSingleStepException(pExceptionInfo->ContextRecord->Dr6);
		else if (GetBits(pExceptionInfo->ContextRecord->Dr6, 2, 1) || GetBits(pExceptionInfo->ContextRecord->Dr6, 3, 1))
			g_gdbstub->HandleAccessException(pExceptionInfo->ContextRecord->Dr6);
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI cemu_unhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo)
{
	createCrashlog(pExceptionInfo, pExceptionInfo->ContextRecord);
	return EXCEPTION_NONCONTINUABLE_EXCEPTION;
}

void ExceptionHandler_Init()
{
	SetUnhandledExceptionFilter(cemu_unhandledExceptionFilter);
	AddVectoredExceptionHandler(1, VectoredExceptionHandler);
	SetErrorMode(SEM_FAILCRITICALERRORS);
}
