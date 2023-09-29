#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Debugger/GDBStub.h"
#include "ExceptionHandler.h"

void DebugLogStackTrace(OSThread_t* thread, MPTR sp);

bool crashLogCreated = false;

bool CrashLog_Create()
{
    if (crashLogCreated)
        return false; // only create one crashlog
    crashLogCreated = true;
    cemuLog_createLogFile(true);
    CrashLog_SetOutputChannels(true, true);
    return true;
}

static bool s_writeToStdErr{true};
static bool s_writeToLogTxt{true};

void CrashLog_SetOutputChannels(bool writeToStdErr, bool writeToLogTxt)
{
    s_writeToStdErr = writeToStdErr;
    s_writeToLogTxt = writeToLogTxt;
}

// outputs to both stderr and log.txt
void CrashLog_WriteLine(std::string_view text, bool newLine)
{
    if(s_writeToLogTxt)
        cemuLog_writeLineToLog(text, false, newLine);
    if(s_writeToStdErr)
    {
        fwrite(text.data(), sizeof(char), text.size(), stderr);
        if(newLine)
            fwrite("\n", sizeof(char), 1, stderr);
    }
}

void CrashLog_WriteHeader(const char* header)
{
    CrashLog_WriteLine("-----------------------------------------");
    CrashLog_WriteLine("   ", false);
    CrashLog_WriteLine(header);
    CrashLog_WriteLine("-----------------------------------------");
}

void ExceptionHandler_LogGeneralInfo()
{
    char dumpLine[1024];
    // info about game
    CrashLog_WriteLine("");
    CrashLog_WriteHeader("Game info");
    if (CafeSystem::IsTitleRunning())
    {
        CrashLog_WriteLine("Game: ", false);
        CrashLog_WriteLine(CafeSystem::GetForegroundTitleName());
        // title id
        CrashLog_WriteLine(fmt::format("TitleId: {:016x}", CafeSystem::GetForegroundTitleId()));
        // rpx hash
        sprintf(dumpLine, "RPXHash: %08x (Update: %08x)", CafeSystem::GetRPXHashBase(), CafeSystem::GetRPXHashUpdated());
        CrashLog_WriteLine(dumpLine);
    }
    else
    {
        CrashLog_WriteLine("Not running");
    }
    // info about active PPC instance:
    CrashLog_WriteLine("");
    CrashLog_WriteHeader("Active PPC instance");
	PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
    if (hCPU)
    {
        OSThread_t* currentThread = coreinit::OSGetCurrentThread();
        uint32 threadPtr = memory_getVirtualOffsetFromPointer(coreinit::OSGetCurrentThread());
        sprintf(dumpLine, "IP 0x%08x LR 0x%08x Thread 0x%08x", hCPU->instructionPointer, hCPU->spr.LR, threadPtr);
        CrashLog_WriteLine(dumpLine);

        // GPR info
        CrashLog_WriteLine("");
        auto gprs = hCPU->gpr;
        sprintf(dumpLine, "r0 =%08x r1 =%08x r2 =%08x r3 =%08x r4 =%08x r5 =%08x r6 =%08x r7 =%08x", gprs[0], gprs[1], gprs[2], gprs[3], gprs[4], gprs[5], gprs[6], gprs[7]);
        CrashLog_WriteLine(dumpLine);
        sprintf(dumpLine, "r8 =%08x r9 =%08x r10=%08x r11=%08x r12=%08x r13=%08x r14=%08x r15=%08x", gprs[8], gprs[9], gprs[10], gprs[11], gprs[12], gprs[13], gprs[14], gprs[15]);
        CrashLog_WriteLine(dumpLine);
        sprintf(dumpLine, "r16=%08x r17=%08x r18=%08x r19=%08x r20=%08x r21=%08x r22=%08x r23=%08x", gprs[16], gprs[17], gprs[18], gprs[19], gprs[20], gprs[21], gprs[22], gprs[23]);
        CrashLog_WriteLine(dumpLine);
        sprintf(dumpLine, "r24=%08x r25=%08x r26=%08x r27=%08x r28=%08x r29=%08x r30=%08x r31=%08x", gprs[24], gprs[25], gprs[26], gprs[27], gprs[28], gprs[29], gprs[30], gprs[31]);
        CrashLog_WriteLine(dumpLine);

        // stack trace
        MPTR currentStackVAddr = hCPU->gpr[1];
        CrashLog_WriteLine("");
        CrashLog_WriteHeader("PPC stack trace");
        DebugLogStackTrace(currentThread, currentStackVAddr);

        // stack dump
        CrashLog_WriteLine("");
        CrashLog_WriteHeader("PPC stack dump");
        for (uint32 i = 0; i < 16; i++)
        {
            MPTR lineAddr = currentStackVAddr + i * 8 * 4;
            if (memory_isAddressRangeAccessible(lineAddr, 8 * 4))
            {
                sprintf(dumpLine, "[0x%08x] %08x %08x %08x %08x - %08x %08x %08x %08x", lineAddr, memory_readU32(lineAddr + 0), memory_readU32(lineAddr + 4), memory_readU32(lineAddr + 8), memory_readU32(lineAddr + 12), memory_readU32(lineAddr + 16), memory_readU32(lineAddr + 20), memory_readU32(lineAddr + 24), memory_readU32(lineAddr + 28));
                CrashLog_WriteLine(dumpLine);
            }
            else
            {
                sprintf(dumpLine, "[0x%08x] ?", lineAddr);
                CrashLog_WriteLine(dumpLine);
            }
        }
    }
    else
    {
        CrashLog_WriteLine("Not active");
    }

    // PPC thread log
    CrashLog_WriteLine("");
    CrashLog_WriteHeader("PPC threads");
    if (activeThreadCount == 0)
    {
        CrashLog_WriteLine("None active");
    }
    for (sint32 i = 0; i < activeThreadCount; i++)
    {
        MPTR threadItrMPTR = activeThread[i];
        OSThread_t* threadItrBE = (OSThread_t*)memory_getPointerFromVirtualOffset(threadItrMPTR);

        // get thread state
        OSThread_t::THREAD_STATE threadState = threadItrBE->state;
        const char* threadStateStr = "UNDEFINED";
        if (threadItrBE->suspendCounter != 0)
            threadStateStr = "SUSPENDED";
        else if (threadState == OSThread_t::THREAD_STATE::STATE_NONE)
            threadStateStr = "NONE";
        else if (threadState == OSThread_t::THREAD_STATE::STATE_READY)
            threadStateStr = "READY";
        else if (threadState == OSThread_t::THREAD_STATE::STATE_RUNNING)
            threadStateStr = "RUNNING";
        else if (threadState == OSThread_t::THREAD_STATE::STATE_WAITING)
            threadStateStr = "WAITING";
        else if (threadState == OSThread_t::THREAD_STATE::STATE_MORIBUND)
            threadStateStr = "MORIBUND";
        // generate log line
        uint8 affinity = threadItrBE->attr;
        sint32 effectivePriority = threadItrBE->effectivePriority;
        const char* threadName = "NULL";
        if (!threadItrBE->threadName.IsNull())
            threadName = threadItrBE->threadName.GetPtr();
        sprintf(dumpLine, "%08x Ent %08x IP %08x LR %08x %-9s Aff %d%d%d Pri %2d Name %s", threadItrMPTR, _swapEndianU32(threadItrBE->entrypoint), threadItrBE->context.srr0, _swapEndianU32(threadItrBE->context.lr), threadStateStr, (affinity >> 0) & 1, (affinity >> 1) & 1, (affinity >> 2) & 1, effectivePriority, threadName);
        // write line to log
        CrashLog_WriteLine(dumpLine);
    }
}
