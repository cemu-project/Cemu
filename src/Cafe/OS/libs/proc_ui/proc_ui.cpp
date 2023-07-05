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

struct ProcUICallback
{
	MPTR callback;
	void* data;
	sint32 priority;
};
std::unordered_map<uint32, ProcUICallback> g_Callbacks;

uint32 ProcUIRegisterCallback(uint32 message, MPTR callback, void* data, sint32 priority)
{
	g_Callbacks.insert_or_assign(message, ProcUICallback{ .callback = callback, .data = data, .priority = priority });
	return 0;
}

void ProcUI_SendBackgroundMessage()
{
	if (g_Callbacks.contains(PROCUI_STATUS_BACKGROUND))
	{
		ProcUICallback& callback = g_Callbacks[PROCUI_STATUS_BACKGROUND];
		PPCCoreCallback(callback.callback, callback.data);
	}
}

void ProcUI_SendForegroundMessage()
{
	if (g_Callbacks.contains(PROCUI_STATUS_FOREGROUND))
	{
		ProcUICallback& callback = g_Callbacks[PROCUI_STATUS_FOREGROUND];
		PPCCoreCallback(callback.callback, callback.data);
	}
}

void procui_load()
{
	cafeExportRegister("proc_ui", ProcUIRegisterCallback, LogType::ProcUi);
	cafeExportRegister("proc_ui", ProcUIProcessMessages, LogType::ProcUi);
	cafeExportRegister("proc_ui", ProcUIInForeground, LogType::ProcUi);
}