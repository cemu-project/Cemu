#include "Cafe/OS/common/OSCommon.h"
#include "proc_ui.h"

#define PROCUI_STATUS_FOREGROUND	0
#define PROCUI_STATUS_BACKGROUND	1
#define PROCUI_STATUS_RELEASING		2
#define PROCUI_STATUS_EXIT			3

uint32 ProcUIProcessMessages()
{
	return PROCUI_STATUS_FOREGROUND;
}


uint32 ProcUIInForeground(PPCInterpreter_t* hCPU)
{
	return 1; // true means application is in foreground
}

uint32 ProcUIRegisterCallback(uint32 message, MPTR callback, void* data, sint32 ukn)
{
	return 0;
}

void procui_load()
{
	cafeExportRegister("proc_ui", ProcUIRegisterCallback, LogType::ProcUi);
	cafeExportRegister("proc_ui", ProcUIProcessMessages, LogType::ProcUi);
	cafeExportRegister("proc_ui", ProcUIInForeground, LogType::ProcUi);
}