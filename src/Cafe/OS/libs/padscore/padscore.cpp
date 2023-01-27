#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "gui/wxgui.h"
#include "Cafe/OS/libs/padscore/padscore.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "Cafe/OS/libs/coreinit/coreinit_SystemInfo.h"
#include "input/InputManager.h"

// KPAD

enum class KPAD_ERROR : sint32
{
	NONE = 0,
	NO_CONTROLLER = -2,
	NOT_INITIALIZED = -5,
};

// WPAD (Wii Controller stuff)

enum WPADRumble
{
	kStopRumble = 0,
	kStartRumble = 1,
};

enum class WPADBatteryLevel : uint8
{
	FULL = 4,
};

enum class WPADLed : uint8
{
	CHAN0 = (1 << 0),
	CHAN1 = (1 << 1),
	CHAN2 = (1 << 2),
	CHAN3 = (1 << 3),
};

namespace padscore 
{
	enum WPADState_t
	{
		kWPADStateMaster = 0,
		kWPADStateShutdown,
		kWPADStateInitializing,
		kWPADStateAcquired,
		kWPADStateReleased,
	};
	KPADUnifiedWpadStatus_t* g_kpad_ringbuffer = nullptr;
	uint32 g_kpad_ringbuffer_length = 0;
	bool g_wpad_callback_by_kpad = false;

	WPADState_t g_wpad_state = kWPADStateMaster;

	struct {
		coreinit::OSAlarm_t alarm;
		bool kpad_initialized = false;

		struct WPADData
		{
			MEMPTR<void> extension_callback;
			MEMPTR<void> connectCallback;
			MEMPTR<void> sampling_callback;
			MEMPTR<void> dpd_callback;
			bool dpd_enabled = true;

			bool disconnectCalled = false;

			BtnRepeat btn_repeat{};
		} controller_data[InputManager::kMaxWPADControllers] = {};

		int max_controllers = kWPADMaxControllers; // max bt controllers?
	} g_padscore;
}


#pragma region WPAD

void padscoreExport_WPADGetStatus(PPCInterpreter_t* hCPU)
{
	inputLog_printf("WPADGetStatus()");
	uint32 status = 1;
	osLib_returnFromFunction(hCPU, status);
}

void padscoreExport_WPADGetBatteryLevel(PPCInterpreter_t* hCPU)
{
	inputLog_printf("WPADGetBatteryLevel()");
	osLib_returnFromFunction(hCPU, (uint32)WPADBatteryLevel::FULL);
}

void padscoreExport_WPADProbe(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(type, uint32be, 1);

	inputLog_printf("WPADProbe(%d)", channel);

	if(const auto controller = InputManager::instance().get_wpad_controller(channel))
	{
		if(type)
			*type = controller->get_device_type();

		osLib_returnFromFunction(hCPU, WPAD_ERR_NONE);
	}
	else
	{
		osLib_returnFromFunction(hCPU, WPAD_ERR_NO_CONTROLLER);
	}
}

typedef struct
{
	uint32be dpd;
	uint32be speaker;
	uint32be attach;
	uint32be lowBat;
	uint32be nearempty;
	betype<WPADBatteryLevel> batteryLevel;
	betype<WPADLed> led;
	uint8 protocol;
	uint8 firmware;
}WPADInfo_t;

static_assert(sizeof(WPADInfo_t) == 0x18); // unsure

void padscoreExport_WPADGetInfoAsync(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(wpadInfo, WPADInfo_t, 1);
	ppcDefineParamMPTR(callbackFunc, 2);
	inputLog_printf("WPADGetInfoAsync(%d, 0x%08x, 0x%08x)", channel, wpadInfo, callbackFunc);

	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			wpadInfo->dpd = FALSE;
			wpadInfo->speaker = FALSE;
			wpadInfo->attach = FALSE;
			wpadInfo->lowBat = FALSE;
			wpadInfo->nearempty = FALSE;
			wpadInfo->batteryLevel = WPADBatteryLevel::FULL;
			wpadInfo->led = WPADLed::CHAN0;

			if(callbackFunc != MPTR_NULL)
				coreinitAsyncCallback_add(callbackFunc, 2, channel, (uint32)KPAD_ERROR::NONE);

			osLib_returnFromFunction(hCPU, WPAD_ERR_NONE);
			return;
		}
	}
	else
	{
		debugBreakpoint();
	}

	if (callbackFunc != MPTR_NULL)
		coreinitAsyncCallback_add(callbackFunc, 2, channel, (uint32)KPAD_ERROR::NO_CONTROLLER);

	osLib_returnFromFunction(hCPU, WPAD_ERR_NO_CONTROLLER);
}

void padscoreExport_WPADRead(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(wpadStatus, WPADStatus_t, 1);
	inputLog_printf("WPADRead(%d, %llx)", channel, wpadStatus);

	if (channel < InputManager::kMaxWPADControllers)
	{
		if(const auto controller = InputManager::instance().get_wpad_controller(channel) )
		{
			controller->WPADRead(wpadStatus);
		}
	}
	else
	{
		debugBreakpoint();
	}
	osLib_returnFromFunction(hCPU, WPAD_ERR_NO_CONTROLLER);
}

void padscoreExport_WPADSetDataFormat(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(fmt, 1);
	inputLog_printf("WPADSetDataFormat(%d, %d)", channel, fmt);

	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
			controller->set_data_format((WPADDataFormat)fmt);
	}

	osLib_returnFromFunction(hCPU, 0);
}

void padscoreExport_WPADGetDataFormat(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	inputLog_printf("WPADGetDataFormat(%d)", channel);

	sint32 dataFormat = kDataFormat_CORE;
	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
			dataFormat = controller->get_data_format();
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, dataFormat);
}

void padscoreExport_WPADGetInfo(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(wpadInfo, WPADInfo_t, 1);
	inputLog_printf("WPADGetInfo(%d, 0x%08x)", channel, wpadInfo);

	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			wpadInfo->dpd = FALSE;
			wpadInfo->speaker = FALSE;
			wpadInfo->attach = FALSE;
			wpadInfo->lowBat = FALSE;
			wpadInfo->nearempty = FALSE;
			wpadInfo->batteryLevel = WPADBatteryLevel::FULL;
			wpadInfo->led = WPADLed::CHAN0;

			osLib_returnFromFunction(hCPU, WPAD_ERR_NONE);
			return;
		}
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, WPAD_ERR_NO_CONTROLLER);
}


void padscoreExport_WPADIsMotorEnabled(PPCInterpreter_t* hCPU)
{
	inputLog_printf("WPADIsMotorEnabled()");
	osLib_returnFromFunction(hCPU, TRUE);
}

void padscoreExport_WPADControlMotor(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(command, 1);
	inputLog_printf("WPADControlMotor(%d, %d)", channel, command);
	
	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			if( command == kStartRumble )
				controller->start_rumble();
			else
				controller->stop_rumble();
		}
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}
#pragma endregion



#pragma region KPAD
void padscoreExport_KPADGetUnifiedWpadStatus(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(status, KPADUnifiedWpadStatus_t, 1);
	ppcDefineParamU32(count, 2);

	inputLog_printf("KPADGetUnifiedWpadStatus(%d, 0x%llx, 0x%x)", channel, status, count);

	if (channel < InputManager::kMaxWPADControllers)
	{
		memset(status, 0x00, sizeof(KPADUnifiedWpadStatus_t) * count);

		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			switch (controller->get_data_format())
			{
			case WPAD_FMT_CORE:
			case WPAD_FMT_CORE_ACC:
			case WPAD_FMT_CORE_ACC_DPD:
			{
				status->fmt = controller->get_data_format();
				controller->WPADRead(&status->u.core);
				break;
			}
			default:
				debugBreakpoint();
			}
		}
		else
		{
			status->u.core.err = WPAD_ERR_NO_CONTROLLER;
		}
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void padscoreExport_KPADSetBtnRepeat(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	float delaySec = hCPU->fpr[1].fpr;
	float pulseSec = hCPU->fpr[2].fpr;
	inputLog_printf("KPADSetBtnRepeat(%d, %f, %f)", channel, delaySec, pulseSec);

	if (channel < InputManager::kMaxWPADControllers)
	{
		padscore::g_padscore.controller_data[channel].btn_repeat = { (int)delaySec, (int)pulseSec };
	}

	osLib_returnFromFunction(hCPU, 0);
}

void padscoreExport_KPADSetSamplingCallback(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamMPTR(callback, 1);

	inputLog_printf("KPADSetSamplingCallback(%d, 0x%x)", channel, callback);

	if (channel >= InputManager::kMaxWPADControllers)
	{
		debugBreakpoint();
		osLib_returnFromFunction(hCPU, MPTR_NULL);
		return;
	}

	const auto old_callback = padscore::g_padscore.controller_data[channel].sampling_callback;
	padscore::g_padscore.controller_data[channel].sampling_callback = callback;
	osLib_returnFromFunction(hCPU, old_callback.GetMPTR());
}

void padscoreExport_WPADControlDpd(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(command, 1);
	ppcDefineParamMPTR(callback, 2);

	inputLog_printf("WPADControlDpd(%d, %d, 0x%x)", channel, command, callback);

	if (channel < InputManager::kMaxWPADControllers)
	{
		if (const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			padscore::g_padscore.controller_data[channel].dpd_callback = callback;
			if (callback)
			{
				coreinitAsyncCallback_add(callback, 2, channel, WPAD_ERR_NONE);
			}

			osLib_returnFromFunction(hCPU, WPAD_ERR_NONE);
			return;
		}
	}

	osLib_returnFromFunction(hCPU, WPAD_ERR_NO_CONTROLLER);
}

void padscoreExport_WPADSetExtensionCallback(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamMPTR(callback, 1);

	inputLog_printf("WPADSetExtensionCallback(%d, 0x%x)", channel, callback);

	if (channel >= InputManager::kMaxWPADControllers)
	{
		debugBreakpoint();
		osLib_returnFromFunction(hCPU, MPTR_NULL);
		return;
	}

	const auto old_callback = padscore::g_padscore.controller_data[channel].extension_callback;
	padscore::g_padscore.controller_data[channel].extension_callback = callback;
	osLib_returnFromFunction(hCPU, old_callback.GetMPTR());
}

void padscoreExport_KPADSetConnectCallback(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamMPTR(callback, 1);

	inputLog_printf("KPADSetConnectCallback(%d, 0x%x)",channel, callback);

	if (channel >= InputManager::kMaxWPADControllers)
	{
		debugBreakpoint();
		osLib_returnFromFunction(hCPU, MPTR_NULL);
		return;
	}

	const auto old_callback = padscore::g_padscore.controller_data[channel].connectCallback;
	padscore::g_padscore.controller_data[channel].connectCallback = callback;
	osLib_returnFromFunction(hCPU, old_callback.GetMPTR());
}

bool g_kpadIsInited = true;
sint32 _KPADRead(uint32 channel, KPADStatus_t* samplingBufs, uint32 length, betype<KPAD_ERROR>* errResult)
{
	if (channel >= InputManager::kMaxWPADControllers)
	{
		debugBreakpoint();
		return 0;
	}
	
	if (g_kpadIsInited == false)
	{
		if (errResult)
			*errResult = KPAD_ERROR::NOT_INITIALIZED;

		return 0;
	}

	const auto controller = InputManager::instance().get_wpad_controller(channel);
	if (!controller)
	{
		if (errResult)
			*errResult = KPAD_ERROR::NO_CONTROLLER;

		return 0;
	}

	memset(samplingBufs, 0x00, sizeof(KPADStatus_t));
	samplingBufs->wpadErr = WPAD_ERR_NONE;
	samplingBufs->data_format = controller->get_data_format();
	samplingBufs->devType = controller->get_device_type();
	if(!g_inputConfigWindowHasFocus)
	{
		const auto btn_repeat = padscore::g_padscore.controller_data[channel].btn_repeat;
		controller->KPADRead(*samplingBufs, btn_repeat);
	}

	if (errResult)
		*errResult = KPAD_ERROR::NONE;

	return 1;
}

void padscoreExport_KPADReadEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(kpadStatus, KPADStatus_t, 1);
	ppcDefineParamU32(length, 2);
	ppcDefineParamPtr(errResult, betype<KPAD_ERROR>, 3);
	inputLog_printf("KPADReadEx(%d, 0x%x)", channel, length);

	sint32 samplesRead = _KPADRead(channel, kpadStatus, length, errResult);
	osLib_returnFromFunction(hCPU, samplesRead);
}

bool debugUseDRC1 = true;
void padscoreExport_KPADRead(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(kpadStatus, KPADStatus_t, 1);
	ppcDefineParamU32(length, 2);
	inputLog_printf("KPADRead(%d, 0x%x)", channel, length);

	sint32 samplesRead = _KPADRead(channel, kpadStatus, length, nullptr);
	osLib_returnFromFunction(hCPU, samplesRead);
}






#pragma endregion



namespace padscore
{
	void export_KPADEnableDPD(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(channel, 0);
		inputLog_printf("KPADEnableDPD(%d)", channel);
		cemu_assert_debug(0 <= channel && channel < InputManager::kMaxWPADControllers);

		if(const auto controller = InputManager::instance().get_wpad_controller(channel))
		{
			if (controller->get_data_format() != kDataFormat_FREESTYLE && controller->get_data_format() != 0x1F)
				g_padscore.controller_data[channel].dpd_enabled = true;
		}
		

		osLib_returnFromFunction(hCPU, 0);
	}

	void export_KPADGetMplsWorkSize(PPCInterpreter_t* hCPU)
	{
		inputLog_printf("KPADGetMplsWorkSize()");
		osLib_returnFromFunction(hCPU, 0x5FE0);
	}

	void KPADInitEx(KPADUnifiedWpadStatus_t ring_buffer[], uint32 length)
	{
		if (g_padscore.kpad_initialized)
			return;

		for (uint32 i = 0; i < InputManager::kMaxWPADControllers; i++)
		{
			g_padscore.controller_data[i].dpd_enabled = true;


		}

		g_kpad_ringbuffer = ring_buffer;
		g_kpad_ringbuffer_length = length;
		g_padscore.kpad_initialized = true;
	}

	void export_KPADInit(PPCInterpreter_t* hCPU)
	{
		inputLog_printf("KPADInit()");
		KPADInitEx(nullptr, 0);
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_KPADInitEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamMEMPTR(ring_buffer, KPADUnifiedWpadStatus_t, 0);
		ppcDefineParamU32(length, 1);
		inputLog_printf("KPADInitEx(0x%08x, 0x%x)", ring_buffer.GetMPTR(), length);
		KPADInitEx(ring_buffer.GetPtr(), length);
		osLib_returnFromFunction(hCPU, 0);
	}

	void WPADSetCallbackByKPAD(bool state)
	{
		g_wpad_callback_by_kpad = state;
	}

	void export_WPADSetCallbackByKPAD(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(state, 0);
		inputLog_printf("WPADSetCallbackByKPAD(%d)", state);
		WPADSetCallbackByKPAD(state);
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_KPADGetMaxControllers(PPCInterpreter_t* hCPU)
	{
		inputLog_printf("KPADGetMaxControllers()");
		sint32 max_controllers = g_padscore.max_controllers;
		osLib_returnFromFunction(hCPU, max_controllers);
	}

	bool WPADIsUsedCallbackByKPAD()
	{
		return g_wpad_callback_by_kpad == TRUE;
	}

	void* WPADSetSamplingCallback(sint32 channel, void* callback)
	{
		cemu_assert_debug(0 <= channel && channel < kKPADMaxControllers);

		const auto result = g_padscore.controller_data[channel].sampling_callback;
		g_padscore.controller_data[channel].sampling_callback = callback;
		return result.GetPtr();
	}

	void export_KPADSetMaxControllers(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(new_max_count, 0);
		inputLog_printf("KPADSetMaxControllers(%d)", new_max_count);

		if (new_max_count != kKPADMaxControllers && new_max_count != kWPADMaxControllers)
		{
			debugBreakpoint();
			osLib_returnFromFunction(hCPU, -4);
			return;
		}


		const uint32 max_controllers = g_padscore.max_controllers;
		if (max_controllers == new_max_count)
		{
			osLib_returnFromFunction(hCPU, 0);
			return;
		}

		WPADSetCallbackByKPAD(FALSE);
		for (sint32 i = 0; i < kKPADMaxControllers; i++)
		{
			//WPADSetSamplingCallback(i, 0); TODO
		}

		g_padscore.max_controllers = new_max_count;
		
		WPADSetCallbackByKPAD(true);
		osLib_returnFromFunction(hCPU, new_max_count);
	}

	void export_WPADInit()
	{
		if (g_wpad_state != kWPADStateShutdown)
		{
			if (g_wpad_state == kWPADStateInitializing || g_wpad_state == kWPADStateAcquired)
				return;
		}
		g_wpad_state = kWPADStateInitializing;
	}


	enum class WPADStatus : sint32
	{
		Success = 0,
		NoController = -1,
		Busy = -2,
	};

	WPADStatus WPADIsMplsAttached(sint32 index, uint32be* attached, void* callback)
	{
		if (index >= kKPADMaxControllers)
			return WPADStatus::NoController;

		const auto controller = InputManager::instance().get_wpad_controller(index);
		*attached = controller && controller->is_mpls_attached();

		if (callback)
			PPCCoreCallback(MEMPTR(callback), index, controller ? WPADStatus::Success : WPADStatus::NoController);

		return WPADStatus::Success;
	}

	struct WPADAcc
	{
		sint32be x;
		sint32be y;
		sint32be z;
	};
	void WPADGetAccGravityUnit(sint32 index, uint32 type, WPADAcc* acc)
	{
		if (index >= kKPADMaxControllers)
			return;

		// default values for wiimote usually are around 99
		acc->x = 99;
		acc->y = 99;
		acc->z = 99;
	}


#pragma endregion

	void TickFunction(PPCInterpreter_t* hCPU)
	{
		auto& instance = InputManager::instance();
		// test for connected/disconnected controllers
		for (auto i = 0; i < InputManager::kMaxWPADControllers; ++i)
		{
			if (g_padscore.controller_data[i].connectCallback) 
			{
				if(!g_padscore.controller_data[i].disconnectCalled)
				{
					g_padscore.controller_data[i].disconnectCalled = true;
					inputLog_printf("Calling WPADConnectCallback(%d, %d)", i, WPAD_ERR_NO_CONTROLLER);
					PPCCoreCallback(g_padscore.controller_data[i].connectCallback, i, WPAD_ERR_NO_CONTROLLER);
					continue;
				}

				if (const auto controller = instance.get_wpad_controller(i))
				{
					if (controller->m_status == WPADController::ConnectCallbackStatus::ReportDisconnect || controller->was_home_button_down()) // fixed!
					{
						controller->m_status = WPADController::ConnectCallbackStatus::ReportConnect;

						inputLog_printf("Calling WPADConnectCallback(%d, %d)", i, WPAD_ERR_NO_CONTROLLER);
						PPCCoreCallback(g_padscore.controller_data[i].connectCallback, i, WPAD_ERR_NO_CONTROLLER);
						
					}
					else if (controller->m_status == WPADController::ConnectCallbackStatus::ReportConnect) 
					{
						controller->m_status = WPADController::ConnectCallbackStatus::None;
						inputLog_printf("Calling WPADConnectCallback(%d, %d)", i, WPAD_ERR_NONE);
						PPCCoreCallback(g_padscore.controller_data[i].connectCallback, i, WPAD_ERR_NONE);
					}
				}
			}
		}

		// test for connected/disconnected extensions
		for (auto i = 0; i < InputManager::kMaxWPADControllers; ++i)
		{
			if (g_padscore.controller_data[i].extension_callback) 
			{
				if (const auto controller = instance.get_wpad_controller(i))
				{
					if (controller->m_extension_status == WPADController::ConnectCallbackStatus::ReportConnect)
					{
						controller->m_extension_status = WPADController::ConnectCallbackStatus::None;
						inputLog_printf("Calling WPADextensionCallback(%d)", i);
						PPCCoreCallback(g_padscore.controller_data[i].extension_callback, i, controller->get_device_type());
					}
				}
			}
		}

		// call sampling callback
		for (auto i = 0; i < InputManager::kMaxWPADControllers; ++i)
		{
			if (g_padscore.controller_data[i].sampling_callback) {
				if (const auto controller = instance.get_wpad_controller(i))
				{
					inputLog_printf("Calling WPADsamplingCallback(%d)", i);
					PPCCoreCallback(g_padscore.controller_data[i].sampling_callback, i);
				}
			}
		}

		osLib_returnFromFunction(hCPU, 0);
	}
	void start()
	{
		OSCreateAlarm(&g_padscore.alarm);
		const uint64 start_tick = coreinit::coreinit_getOSTime();
		const uint64 period_tick = coreinit::EspressoTime::GetTimerClock(); // once a second
		MPTR handler = PPCInterpreter_makeCallableExportDepr(TickFunction);
		OSSetPeriodicAlarm(&g_padscore.alarm, start_tick, period_tick, handler);
	}

	void load()
	{
		cafeExportRegister("padscore", WPADIsMplsAttached, LogType::InputAPI);
		cafeExportRegister("padscore", WPADGetAccGravityUnit, LogType::InputAPI);

		// wpad
		//osLib_addFunction("padscore", "WPADInit", padscore::export_WPADInit);

		// kpad
		osLib_addFunction("padscore", "KPADSetMaxControllers", padscore::export_KPADSetMaxControllers);
		osLib_addFunction("padscore", "KPADGetMaxControllers", padscore::export_KPADGetMaxControllers);
		osLib_addFunction("padscore", "KPADEnableDPD", padscore::export_KPADEnableDPD);
		osLib_addFunction("padscore", "KPADGetMplsWorkSize", padscore::export_KPADGetMplsWorkSize);
		osLib_addFunction("padscore", "KPADInit", padscore::export_KPADInit);
		osLib_addFunction("padscore", "KPADInitEx", padscore::export_KPADInitEx);

		osLib_addFunction("padscore", "KPADSetConnectCallback", padscoreExport_KPADSetConnectCallback);
		osLib_addFunction("padscore", "KPADReadEx", padscoreExport_KPADReadEx);
		osLib_addFunction("padscore", "KPADRead", padscoreExport_KPADRead);
		osLib_addFunction("padscore", "KPADGetUnifiedWpadStatus", padscoreExport_KPADGetUnifiedWpadStatus);
		osLib_addFunction("padscore", "KPADSetSamplingCallback", padscoreExport_KPADSetSamplingCallback);
		osLib_addFunction("padscore", "KPADSetBtnRepeat", padscoreExport_KPADSetBtnRepeat);

		osLib_addFunction("padscore", "WPADGetBatteryLevel", padscoreExport_WPADGetBatteryLevel);
		osLib_addFunction("padscore", "WPADControlMotor", padscoreExport_WPADControlMotor);
		osLib_addFunction("padscore", "WPADIsMotorEnabled", padscoreExport_WPADIsMotorEnabled);
		osLib_addFunction("padscore", "WPADGetStatus", padscoreExport_WPADGetStatus);
		osLib_addFunction("padscore", "WPADProbe", padscoreExport_WPADProbe);
		osLib_addFunction("padscore", "WPADGetInfoAsync", padscoreExport_WPADGetInfoAsync);
		osLib_addFunction("padscore", "WPADGetInfo", padscoreExport_WPADGetInfo);
		osLib_addFunction("padscore", "WPADSetConnectCallback", padscoreExport_KPADSetConnectCallback);
		osLib_addFunction("padscore", "WPADSetDataFormat", padscoreExport_WPADSetDataFormat);
		osLib_addFunction("padscore", "WPADGetDataFormat", padscoreExport_WPADGetDataFormat);
		osLib_addFunction("padscore", "WPADRead", padscoreExport_WPADRead);
		osLib_addFunction("padscore", "WPADSetExtensionCallback", padscoreExport_WPADSetExtensionCallback);
		osLib_addFunction("padscore", "WPADSetSamplingCallback", padscoreExport_KPADSetSamplingCallback);
		osLib_addFunction("padscore", "WPADControlDpd", padscoreExport_WPADControlDpd);

		osLib_addFunction("padscore", "WPADSetCallbackByKPAD", padscore::export_WPADSetCallbackByKPAD);
	}

}