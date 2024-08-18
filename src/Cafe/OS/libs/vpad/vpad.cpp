#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "gui/wxgui.h"
#include "Cafe/OS/libs/vpad/vpad.h"
#include "audio/IAudioAPI.h"
#include "Cafe/OS/libs/coreinit/coreinit_Time.h"
#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/coreinit/coreinit_Alarm.h"
#include "input/InputManager.h"

#ifdef PUBLIC_RELASE
#define vpadbreak() 
#else
#define vpadbreak()// __debugbreak();
#endif

#define VPAD_READ_ERR_NONE			0
#define VPAD_READ_ERR_NO_DATA		-1
#define VPAD_READ_ERR_NO_CONTROLLER	-2
#define VPAD_READ_ERR_SETUP			-3
#define VPAD_READ_ERR_LOCKED		-4
#define VPAD_READ_ERR_INIT			-5

#define VPAD_TP_VALIDITY_VALID		0
#define VPAD_TP_VALIDITY_INVALID_X	1
#define VPAD_TP_VALIDITY_INVALID_Y	2
#define VPAD_TP_VALIDITY_INVALID_XY	(VPAD_TP_VALIDITY_INVALID_X | VPAD_TP_VALIDITY_INVALID_Y)

#define VPAD_GYRO_ZERODRIFT_LOOSE		0
#define VPAD_GYRO_ZERODRIFT_STANDARD	1
#define VPAD_GYRO_ZERODRIFT_TIGHT		2
#define VPAD_GYRO_ZERODRIFT_NONE		3

#define VPAD_PLAY_MODE_LOOSE			0
#define VPAD_PLAY_MODE_TIGHT			1

#define VPAD_BUTTON_PROC_MODE_LOOSE		0
#define VPAD_BUTTON_PROC_MODE_TIGHT		1

#define VPAD_LCD_MODE_MUTE				0
#define VPAD_LCD_MODE_GAME_CONTROLLER	1
#define VPAD_LCD_MODE_ON				0xFF

#define VPAD_MOTOR_PATTERN_SIZE_MAX		15
#define VPAD_MOTOR_PATTERN_LENGTH_MAX	120

#define VPAD_TP_1920x1080				0
#define VPAD_TP_1280x720				1
#define VPAD_TP_854x480					2

extern bool isLaunchTypeELF;

VPADDir g_vpadGyroDirOverwrite[VPAD_MAX_CONTROLLERS] =
{
		{{1.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f}, {0.0f, 0.0f, 0.1f}},
		{{1.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f}, {0.0f, 0.0f, 0.1f}}
};
uint32 g_vpadGyroZeroDriftMode[VPAD_MAX_CONTROLLERS] = { VPAD_GYRO_ZERODRIFT_STANDARD, VPAD_GYRO_ZERODRIFT_STANDARD };

struct VPACGyroDirRevise_t
{
	bool enabled;
	VPADDir vpadGyroDirReviseBase;
	float weight;
} g_vpadGyroDirRevise[VPAD_MAX_CONTROLLERS] = {};

struct VPADAccParam_t
{ // TODO P: use
	float playRadius;
	float sensitivity;
} g_vpadAccParam[VPAD_MAX_CONTROLLERS] = { {0, 1}, {0, 1} };

uint32 g_vpadPlayMode[VPAD_MAX_CONTROLLERS] = { VPAD_PLAY_MODE_TIGHT, VPAD_PLAY_MODE_TIGHT }; // TODO P: use

#define VPAD_MIN_CLAMP	(0x102)
#define VPAD_MAX_CLAMP	(0x397)

struct VPADStickClamp
{ // TODO P: use
	bool crossMode; // default is circular mode

	sint32 leftMax;
	sint32 leftMin;

	sint32 rightMax;
	sint32 rightMin;
} vpadStickClamp[VPAD_MAX_CONTROLLERS] = { {false, VPAD_MAX_CLAMP, VPAD_MIN_CLAMP}, {false, VPAD_MAX_CLAMP, VPAD_MIN_CLAMP} };

struct VPADCrossStickEmulationParams
{ // TODO P: use
	float leftRotation;
	float leftInputRange;
	float leftRadius;

	float rightRotation;
	float rightInputRange;
	float rightRadius;
} vpadCrossStickEmulationParams[VPAD_MAX_CONTROLLERS] = {};

uint8 vpadButtonProcMode[VPAD_MAX_CONTROLLERS] = { VPAD_BUTTON_PROC_MODE_TIGHT, VPAD_BUTTON_PROC_MODE_TIGHT }; // TODO P: use
uint32 vpadLcdMode[VPAD_MAX_CONTROLLERS] = { VPAD_LCD_MODE_ON, VPAD_LCD_MODE_ON };

struct VPADTPCalibrationParam
{ // TODO P: use
	uint16be offsetX;
	uint16be offsetY;
	float32be scaleX;
	float32be scaleY;
} vpadTPCalibrationParam[VPAD_MAX_CONTROLLERS] = { {92, 254, (1280.0f / 3883.0f), (720.0f / 3694.0f)}, {92, 254, (1280.0f / 3883.0f), (720.0f / 3694.0f)} };

void _tpRawToResolution(sint32 x, sint32 y, sint32* outX, sint32* outY, sint32 width, sint32 height)
{
	x -= 92;
	y = 4095.0 - y - 254;

	x = std::max(x, 0);
	y = std::max(y, 0);

	*outX = (sint32)(((double)x / 3883.0) * (double)width);
	*outY = (sint32)(((double)y / 3694.0) * (double)height);
}


namespace vpad
{
	enum class PlayMode : sint32
	{
		Loose = 0,
		Tight = 1
	};

	enum class LcdMode
	{
		Off = 0,
		ControllerOnly = 1,
		On = 0xFF,
	};

	struct VPADTPCalibrationParam
	{
		sint16be x, y;
		float32be scale_x, scale_y;
	};

	struct VPADTPData
	{
		uint16be x, y, touch, valid;
	};

	enum class VPADTPResolution
	{
		_1920x1080 = 0,
		_1280x720 = 1,
		_854_480 = 2,
	};

	enum class ButtonProcMode : uint8
	{
		Loose = 0,
		Tight = 1,
	};
	
	struct
	{
		SysAllocator<coreinit::OSAlarm_t> alarm;

		struct
		{
			uint64 drcLastCallTime = 0;

			struct AccParam
			{
				float radius, sensitivity;
			} acc_param;
			BtnRepeat btn_repeat;

			MEMPTR<void> sampling_callback;
			PlayMode acc_play_mode;

			VPADTPCalibrationParam tp_calibration_param{};
			uint16 tp_size = 0xc;

			struct
			{
				float rotation, range, radius;
			}cross_stick_emulation_l{}, cross_stick_emulation_r{};

			ButtonProcMode button_proc_mode;

			struct
			{
				bool enabled = false;
				struct
				{
					sint32 min = 0x102, max = 0x397;
				} left{}, right{};
			}stick_cross_clamp{};

		}controller_data[VPAD_MAX_CONTROLLERS]{};
	} g_vpad;

	

	void VPADSetAccParam(sint32 channel, float radius, float sensitivity)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetAccParam({}, {}, {})", channel, radius, sensitivity);
		vpadbreak();
		g_vpad.controller_data[channel].acc_param.radius = radius;
		g_vpad.controller_data[channel].acc_param.sensitivity = sensitivity;
	}

	void VPADGetAccParam(sint32 channel, float* radius, float* sensitivity)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetAccParam({}, {}, {})", channel, (void*)radius, (void*)sensitivity);
		vpadbreak();
		*radius = g_vpad.controller_data[channel].acc_param.radius;
		*sensitivity = g_vpad.controller_data[channel].acc_param.sensitivity;
	}

	sint32 VPADRead(sint32 channel, VPADStatus* status, uint32 length, sint32be* error) 
	{
		//printf("VPADRead(%d,0x%08X,%d,0x%08x)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
		/*ppcDefineParamU32(channel, 0);
		ppcDefineParamStructPtr(status, VPADStatus_t, 1);
		ppcDefineParamU32(length, 2);
		ppcDefineParamPtr(error, uint32be, 3);
		cemuLog_log(LogType::InputAPI, "VPADRead({}, _, {})", channel, length);*/
	
		// default init which should be always set
		memset(status, 0x00, sizeof(VPADStatus_t));

		// default misc
		status->batteryLevel = 0xC0; // full battery
		status->slideVolume = status->slideVolume2 = (uint8)((g_padVolume * 0xFF) / 100);

		// default touch
		status->tpData.validity = VPAD_TP_VALIDITY_INVALID_XY;
		status->tpProcessed1.validity = VPAD_TP_VALIDITY_INVALID_XY;
		status->tpProcessed2.validity = VPAD_TP_VALIDITY_INVALID_XY;

		const auto controller = InputManager::instance().get_vpad_controller(channel);
		if (!controller)
		{
			// most games expect the Wii U GamePad to be connected, so even if the user has not set it up we should still return empty samples for channel 0
			if(channel != 0)
			{
				if (error)
					*error = VPAD_READ_ERR_NO_CONTROLLER;
				if (length > 0)
					status->vpadErr = -1;
				return 0;
			}
			if (error)
				*error = VPAD_READ_ERR_NONE;
			return 1;
		}

		const bool vpadDelayEnabled = ActiveSettings::VPADDelayEnabled();

		if (isLaunchTypeELF)
		{
			// hacky workaround for homebrew games calling VPADRead in an infinite loop
			PPCCore_switchToScheduler();
		}

		if (!g_inputConfigWindowHasFocus)
		{
			if (channel <= 1 && vpadDelayEnabled)
			{
				uint64 currentTime = coreinit::coreinit_getOSTime();
				const auto dif = currentTime - vpad::g_vpad.controller_data[channel].drcLastCallTime;
				if (dif <= (ESPRESSO_TIMER_CLOCK / 60ull))
				{
					// not ready yet
					if (error)
						*error = VPAD_READ_ERR_NONE;
					return 0;
				}
				else if (dif <= ESPRESSO_TIMER_CLOCK)
				{
					vpad::g_vpad.controller_data[channel].drcLastCallTime += (ESPRESSO_TIMER_CLOCK / 60ull);
				}
				else
				{
					vpad::g_vpad.controller_data[channel].drcLastCallTime = currentTime;
				}
			}
			controller->VPADRead(*status, vpad::g_vpad.controller_data[channel].btn_repeat);
			if (error)
				*error = VPAD_READ_ERR_NONE;
			return 1;
		}
		else
		{
			if (error)
				*error = VPAD_READ_ERR_NONE;

			return 1;
		}
	}

	void VPADSetBtnRepeat(sint32 channel, float delay, float pulse)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetBtnRepeat({}, {}, {})", channel, delay, pulse);
		if(pulse == 0)
		{
			g_vpad.controller_data[channel].btn_repeat.delay = 40000;
			g_vpad.controller_data[channel].btn_repeat.pulse = 0;
		}
		else
		{
			g_vpad.controller_data[channel].btn_repeat.delay = (sint32)((delay * 200.0f) + 0.5f);
			g_vpad.controller_data[channel].btn_repeat.pulse = (sint32)((pulse * 200.0f) + 0.5f);
		}
	}


	void VPADSetAccPlayMode(sint32 channel, PlayMode play_mode)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetAccPlayMode({}, {})", channel, (int)play_mode);
		vpadbreak();
		g_vpad.controller_data[channel].acc_play_mode = play_mode;
	}

	PlayMode VPADGetAccPlayMode(sint32 channel)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetAccPlayMode({})", channel);
		vpadbreak();
		return g_vpad.controller_data[channel].acc_play_mode;
	}

	void* VPADSetSamplingCallback(sint32 channel, void* callback)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetSamplingCallback({}, 0x{:x})", channel, MEMPTR(callback).GetMPTR());
		vpadbreak();

		void* result = g_vpad.controller_data[channel].sampling_callback;
		g_vpad.controller_data[channel].sampling_callback = callback;
		return result;
	}

	sint32 VPADCalcTPCalibrationParam(VPADTPCalibrationParam* p, uint16 raw_x1, uint16 raw_y1, uint16 x1, uint16 y1, uint16 raw_x2, uint16 raw_y2, uint16 x2, uint16 y2)
	{
		cemu_assert_unimplemented();
		return 1;
	}

	void VPADGetTPCalibrationParam(sint32 channel, VPADTPCalibrationParam* param)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetTPCalibrationParam({}, 0x{:x})", channel, MEMPTR(param).GetMPTR());
		vpadbreak();

		*param = g_vpad.controller_data[channel].tp_calibration_param;
	}

	void VPADSetTPCalibrationParam(sint32 channel, VPADTPCalibrationParam* param)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetTPCalibrationParam({}, 0x{:x})", channel, MEMPTR(param).GetMPTR());
		vpadbreak();

		g_vpad.controller_data[channel].tp_calibration_param = *param;
	}

	void VPADGetTPCalibratedPoint(sint32 channel, VPADTPData* data, VPADTPData* raw)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetTPCalibratedPoint({}, 0x{:x}, 0x{:x})", channel, MEMPTR(data).GetMPTR(), MEMPTR(raw).GetMPTR());
		vpadbreak();

		const auto& controller_data = g_vpad.controller_data[channel];
		uint16 x = (uint16)((float)raw->x - ((float)controller_data.tp_calibration_param.x * controller_data.tp_calibration_param.scale_x));
		uint16 y = (uint16)((float)raw->x - ((float)controller_data.tp_calibration_param.y * controller_data.tp_calibration_param.scale_y));

		const int tp_size = (int)controller_data.tp_size;

		int tmpx = x;
		if(x <= (int)controller_data.tp_size)
			tmpx = (int)controller_data.tp_size;

		int tmpy = y;
		if(y <= (int)controller_data.tp_size)
			tmpy = (int)controller_data.tp_size;

		if((0x500 - tp_size) <= tmpx)
			x = (0x500 - tp_size);

		if((0x2d0 - tp_size) <= tmpy)
			y = (0x2d0 - tp_size);

		data->x = x;
		data->y = y;
		data->touch = raw->touch;
		data->valid = raw->valid;
	}

	void VPADGetTPCalibratedPointEx(sint32 channel, VPADTPResolution resolution, VPADTPData* data, VPADTPData* raw)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetTPCalibratedPointEx({}, {}, 0x{:x}, 0x{:x})", channel, (int)resolution, MEMPTR(data).GetMPTR(), MEMPTR(raw).GetMPTR());
		vpadbreak();

	}

	void VPADSetCrossStickEmulationParamsL(sint32 channel, float rotation, float range, float radius)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetCrossStickEmulationParamsL({}, {}, {}, {})", channel, rotation, range, radius);
		vpadbreak();

		if (range < 0 || 90.0f < range) 
			return;

		if (radius < 0 || 1.0f < radius) 
			return;
  
		g_vpad.controller_data[channel].cross_stick_emulation_l.rotation = rotation;
		g_vpad.controller_data[channel].cross_stick_emulation_l.range = range;
		g_vpad.controller_data[channel].cross_stick_emulation_l.radius = radius;
	}

	void VPADSetCrossStickEmulationParamsR(sint32 channel, float rotation, float range, float radius)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetCrossStickEmulationParamsR({}, {}, {}, {})", channel, rotation, range, radius);
		vpadbreak();

		if (range < 0 || 90.0f < range) 
			return;

		if (radius < 0 || 1.0f < radius) 
			return;
  
		g_vpad.controller_data[channel].cross_stick_emulation_r.rotation = rotation;
		g_vpad.controller_data[channel].cross_stick_emulation_r.range = range;
		g_vpad.controller_data[channel].cross_stick_emulation_r.radius = radius;
	}

	void VPADGetCrossStickEmulationParamsL(sint32 channel, float* rotation, float* range, float* radius)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetCrossStickEmulationParamsL({}, 0x{:x}, 0x{:x}, 0x{:x})", channel, MEMPTR(rotation).GetMPTR(),  MEMPTR(range).GetMPTR(),  MEMPTR(radius).GetMPTR());
		vpadbreak();

		*rotation = g_vpad.controller_data[channel].cross_stick_emulation_l.rotation;
		*range = g_vpad.controller_data[channel].cross_stick_emulation_l.range;
		*radius = g_vpad.controller_data[channel].cross_stick_emulation_l.radius;
	}

	void VPADGetCrossStickEmulationParamsR(sint32 channel, float* rotation, float* range, float* radius)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetCrossStickEmulationParamsR({}, 0x{:x}, 0x{:x}, 0x{:x})", channel, MEMPTR(rotation).GetMPTR(),  MEMPTR(range).GetMPTR(),  MEMPTR(radius).GetMPTR());
		vpadbreak();

		*rotation = g_vpad.controller_data[channel].cross_stick_emulation_r.rotation;
		*range = g_vpad.controller_data[channel].cross_stick_emulation_r.range;
		*radius = g_vpad.controller_data[channel].cross_stick_emulation_r.radius;
	}

	ButtonProcMode VPADGetButtonProcMode(sint32 channel)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetButtonProcMode({})", channel);
		vpadbreak();

		return g_vpad.controller_data[channel].button_proc_mode;
	}

	void VPADSetButtonProcMode(sint32 channel, ButtonProcMode mode)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetButtonProcMode({}, {})", channel, (int)mode);
		vpadbreak();

		g_vpad.controller_data[channel].button_proc_mode = mode;
	}

	void VPADEnableStickCrossClamp(sint32 channel)
	{
		cemuLog_log(LogType::InputAPI, "VPADEnableStickCrossClamp({})", channel);
		vpadbreak();
		g_vpad.controller_data[channel].stick_cross_clamp.enabled = true;
	}

	void VPADDisableStickCrossClamp(sint32 channel)
	{
		cemuLog_log(LogType::InputAPI, "VPADDisableStickCrossClamp({})", channel);
		vpadbreak();
		g_vpad.controller_data[channel].stick_cross_clamp.enabled = false;
	}

	void VPADSetLStickClampThreshold(sint32 channel, sint32 max, sint32 min)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetLStickClampThreshold({}, {}, {})", channel, max, min);
		vpadbreak();
		g_vpad.controller_data[channel].stick_cross_clamp.left.max = std::min(0x397, max);
		g_vpad.controller_data[channel].stick_cross_clamp.left.min = std::max(0x102, min);
	}

	void VPADSetRStickClampThreshold(sint32 channel, sint32 max, sint32 min)
	{
		cemuLog_log(LogType::InputAPI, "VPADSetRStickClampThreshold({}, {}, {})", channel, max, min);
		vpadbreak();
		g_vpad.controller_data[channel].stick_cross_clamp.right.max = std::min(0x397, max);
		g_vpad.controller_data[channel].stick_cross_clamp.right.min = std::max(0x102, min);
	}

	void VPADGetLStickClampThreshold(sint32 channel, sint32* max, sint32* min)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetLStickClampThreshold({}, 0x{:x}, 0x{:x})", channel, MEMPTR(max).GetMPTR(), MEMPTR(min).GetMPTR());
		vpadbreak();
		*max = g_vpad.controller_data[channel].stick_cross_clamp.left.max;
		*min = g_vpad.controller_data[channel].stick_cross_clamp.left.min;
	}

	void VPADGetRStickClampThreshold(sint32 channel, sint32* max, sint32* min)
	{
		cemuLog_log(LogType::InputAPI, "VPADGetRStickClampThreshold({}, 0x{:x}, 0x{:x})", channel, MEMPTR(max).GetMPTR(), MEMPTR(min).GetMPTR());
		vpadbreak();
		*max = g_vpad.controller_data[channel].stick_cross_clamp.right.max;
		*min = g_vpad.controller_data[channel].stick_cross_clamp.right.min;
	}
}


void vpadExport_VPADGetAccParam(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(playRadius, float32be, 1);
	ppcDefineParamPtr(sensitivity, float32be, 2);
	cemuLog_log(LogType::InputAPI, "VPADGetAccParam({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*playRadius = g_vpadAccParam[channel].playRadius;
		*sensitivity = g_vpadAccParam[channel].sensitivity;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetAccParam(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADSetAccParam({}, {}, {})", channel, hCPU->fpr[1].fpr, hCPU->fpr[2].fpr);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadAccParam[channel].playRadius = hCPU->fpr[1].fpr;
		g_vpadAccParam[channel].sensitivity = hCPU->fpr[2].fpr;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetAccPlayMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADGetAccPlayMode({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		osLib_returnFromFunction(hCPU, g_vpadPlayMode[channel]);
	}
	else
	{
		debugBreakpoint();
		osLib_returnFromFunction(hCPU, VPAD_PLAY_MODE_TIGHT);
	}
}

void vpadExport_VPADSetAccPlayMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(playMode, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetAccPlayMode({}, {})", channel, playMode);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadPlayMode[channel] = playMode;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADEnableStickCrossClamp(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADEnableStickCrossClamp({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadStickClamp[channel].crossMode = true;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADDisableStickCrossClamp(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADDisableStickCrossClamp({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadStickClamp[channel].crossMode = false;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetLStickClampThreshold(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamS32(maxValue, 1);
	ppcDefineParamS32(minValue, 2);
	cemuLog_log(LogType::InputAPI, "VPADSetLStickClampThreshold({}, {}, {})", channel, maxValue, minValue);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadStickClamp[channel].leftMax = std::min(VPAD_MAX_CLAMP, maxValue);
		vpadStickClamp[channel].leftMin = std::max(VPAD_MIN_CLAMP, minValue);
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetRStickClampThreshold(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamS32(maxValue, 1);
	ppcDefineParamS32(minValue, 2);
	cemuLog_log(LogType::InputAPI, "VPADSetRStickClampThreshold({}, {}, {})", channel, maxValue, minValue);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadStickClamp[channel].rightMax = std::min(VPAD_MAX_CLAMP, maxValue);
		vpadStickClamp[channel].rightMin = std::max(VPAD_MIN_CLAMP, minValue);
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetLStickClampThreshold(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(maxValue, uint32be, 1);
	ppcDefineParamPtr(minValue, uint32be, 2);
	cemuLog_log(LogType::InputAPI, "VPADGetLStickClampThreshold({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*maxValue = vpadStickClamp[channel].leftMax;
		*minValue = vpadStickClamp[channel].leftMin;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetRStickClampThreshold(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(maxValue, uint32be, 1);
	ppcDefineParamPtr(minValue, uint32be, 2);
	cemuLog_log(LogType::InputAPI, "VPADGetRStickClampThreshold({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*maxValue = vpadStickClamp[channel].rightMax;
		*minValue = vpadStickClamp[channel].rightMin;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetCrossStickEmulationParamsL(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADSetCrossStickEmulationParamsL({}, {}, {}, {})", channel, hCPU->fpr[1].fpr, hCPU->fpr[2].fpr, hCPU->fpr[3].fpr);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadCrossStickEmulationParams[channel].leftRotation = hCPU->fpr[1].fpr;
		vpadCrossStickEmulationParams[channel].leftInputRange = hCPU->fpr[2].fpr;
		vpadCrossStickEmulationParams[channel].leftRadius = hCPU->fpr[3].fpr;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetCrossStickEmulationParamsR(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADSetCrossStickEmulationParamsR({}, {}, {}, {})", channel, hCPU->fpr[1].fpr, hCPU->fpr[2].fpr, hCPU->fpr[3].fpr);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadCrossStickEmulationParams[channel].rightRotation = hCPU->fpr[1].fpr;
		vpadCrossStickEmulationParams[channel].rightInputRange = hCPU->fpr[2].fpr;
		vpadCrossStickEmulationParams[channel].rightRadius = hCPU->fpr[3].fpr;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetCrossStickEmulationParamsL(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(rotation, float32be, 1);
	ppcDefineParamPtr(inputRange, float32be, 2);
	ppcDefineParamPtr(radius, float32be, 3);
	cemuLog_log(LogType::InputAPI, "VPADGetCrossStickEmulationParamsL({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*rotation = vpadCrossStickEmulationParams[channel].leftRotation;
		*inputRange = vpadCrossStickEmulationParams[channel].leftInputRange;
		*radius = vpadCrossStickEmulationParams[channel].leftRadius;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetCrossStickEmulationParamsR(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(rotation, float32be, 1);
	ppcDefineParamPtr(inputRange, float32be, 2);
	ppcDefineParamPtr(radius, float32be, 3);
	cemuLog_log(LogType::InputAPI, "VPADGetCrossStickEmulationParamsR({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*rotation = vpadCrossStickEmulationParams[channel].rightRotation;
		*inputRange = vpadCrossStickEmulationParams[channel].rightInputRange;
		*radius = vpadCrossStickEmulationParams[channel].rightRadius;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetButtonProcMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADGetButtonProcMode({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		osLib_returnFromFunction(hCPU, vpadButtonProcMode[channel]);
	}
	else
	{
		debugBreakpoint();
		osLib_returnFromFunction(hCPU, VPAD_BUTTON_PROC_MODE_TIGHT);
	}
}

void vpadExport_VPADSetButtonProcMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU8(mode, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetButtonProcMode({}, {})", channel, mode);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadButtonProcMode[channel] = mode;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetLcdMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(mode, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetLcdMode({}, {})", channel, mode);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		vpadLcdMode[channel] = mode;
	}
	else
	{
		debugBreakpoint();
	}
	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetLcdMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamPtr(mode, uint32be, 1);
	cemuLog_log(LogType::InputAPI, "VPADGetLcdMode({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*mode = vpadLcdMode[channel];
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADControlMotor(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamUStr(pattern, 1);
	ppcDefineParamU8(length, 2);
	cemuLog_log(LogType::InputAPI, "VPADControlMotor({}, _, {})", channel, length);

	if (length > 120)
	{
		cemuLog_log(LogType::InputAPI, "VPADControlMotor() - length too high with {} of 120", length);
		length = 120;
	}

	if (const auto controller = InputManager::instance().get_vpad_controller(channel))
	{
		// if length is zero -> stop vibration
		if (length == 0)
		{
			controller->clear_rumble();
		}
		else
		{
			// check for max queue length
			if (!controller->push_rumble(pattern, length))
			{
				osLib_returnFromFunction(hCPU, -1); // TODO P: not sure about the exact return value
				return;
			}
		}
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADStopMotor(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	cemuLog_log(LogType::InputAPI, "VPADStopMotor({})", channel);

	if (const auto controller = InputManager::instance().get_vpad_controller(channel))
	{
		controller->clear_rumble();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetTPCalibrationParam(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamMEMPTR(params, VPADTPCalibrationParam, 1);

	debugBreakpoint();
	if (channel < VPAD_MAX_CONTROLLERS)
	{
		VPADTPCalibrationParam* calibrationParam = params.GetPtr();

		cemuLog_log(LogType::InputAPI, "VPADSetTPCalibrationParam({}, {}, {}, {}, {})", channel, (uint16)calibrationParam->offsetX, (uint16)calibrationParam->offsetX, (float)calibrationParam->scaleX, (float)calibrationParam->scaleY);

		vpadTPCalibrationParam[channel].offsetX = calibrationParam->offsetX;
		vpadTPCalibrationParam[channel].offsetX = calibrationParam->offsetY;
		vpadTPCalibrationParam[channel].scaleX = calibrationParam->scaleX;
		vpadTPCalibrationParam[channel].scaleY = calibrationParam->scaleY;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetTPCalibrationParam(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(calibrationParam, VPADTPCalibrationParam, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetTPCalibrationParam({})", channel);

	calibrationParam->offsetX = vpadTPCalibrationParam[channel].offsetX;
	calibrationParam->offsetY = vpadTPCalibrationParam[channel].offsetY;
	calibrationParam->scaleX = vpadTPCalibrationParam[channel].scaleX;
	calibrationParam->scaleY = vpadTPCalibrationParam[channel].scaleY;

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetTPCalibratedPoint(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(outputDisplay, VPADTPData_t, 1);
	ppcDefineParamStructPtr(inputRaw, VPADTPData_t, 2);
	cemuLog_log(LogType::InputAPI, "VPADGetTPCalibratedPoint({})", channel);

	memmove(outputDisplay, inputRaw, sizeof(VPADTPData_t));

	// vpadTPCalibrationParam[channel]

	sint16 x = outputDisplay->x;
	sint16 y = outputDisplay->y;

	sint32 outputX;
	sint32 outputY;
	_tpRawToResolution(x, y, &outputX, &outputY, 1280, 720);

	outputDisplay->x = outputX;
	outputDisplay->y = outputY;
	outputDisplay->touch = inputRaw->touch;
	outputDisplay->validity = inputRaw->validity;

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetTPCalibratedPointEx(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamS32(tpResolution, 1);
	ppcDefineParamStructPtr(outputDisplay, VPADTPData_t, 2);
	ppcDefineParamStructPtr(inputRaw, VPADTPData_t, 3);
	cemuLog_log(LogType::InputAPI, "VPADGetTPCalibratedPointEx({})", channel);

	//debug_printf("TPInput: %d %d %04x %04x\n", _swapEndianU16(inputRaw->touch), _swapEndianU16(inputRaw->validity), _swapEndianU16(inputRaw->x), _swapEndianU16(inputRaw->y));
	memmove(outputDisplay, inputRaw, sizeof(VPADTPData_t));

	//debug_printf("VPADGetTPCalibratedPointEx(): Resolution %d\n", hCPU->gpr[4]);

	sint16 x = outputDisplay->x;
	sint16 y = outputDisplay->y;

	sint32 outputX = 0;
	sint32 outputY = 0;
	if (tpResolution == VPAD_TP_1920x1080)
	{
		_tpRawToResolution(x, y, &outputX, &outputY, 1920, 1080);
	}
	else if (tpResolution == VPAD_TP_1280x720)
	{
		_tpRawToResolution(x, y, &outputX, &outputY, 1280, 720);
	}
	else if (tpResolution == VPAD_TP_854x480)
	{
		_tpRawToResolution(x, y, &outputX, &outputY, 854, 480);
	}
	else
	{
		debugBreakpoint();
		debug_printf("VPADGetTPCalibratedPointEx(): Unsupported tp resolution\n");
	}
	outputDisplay->x = outputX;
	outputDisplay->y = outputY;
	outputDisplay->touch = inputRaw->touch;
	outputDisplay->validity = inputRaw->validity;

	//debug_printf("VPADGetTPCalibratedPointEx %d %d\n", _swapEndianU16(outputDisplay->x), _swapEndianU16(outputDisplay->y));

	osLib_returnFromFunction(hCPU, 0);
}


void vpadExport_VPADSetGyroDirection(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(dir, VPADDir, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetGyroDirection({}, <<{:f}, {:f}, {:f}>, <{:f}, {:f}, {:f}>, <{:f}, {:f}, {:f}>>)", channel
		, (float)dir->x.x, (float)dir->x.y, (float)dir->x.z
		, (float)dir->y.x, (float)dir->y.y, (float)dir->y.z
		, (float)dir->z.x, (float)dir->z.y, (float)dir->z.z);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadGyroDirOverwrite[channel] = *dir;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADGetGyroZeroDriftMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamMEMPTR(gyroMode, uint32be, 1);
	cemuLog_log(LogType::InputAPI, "VPADGetGyroZeroDriftMode({})", channel);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		*gyroMode = g_vpadGyroZeroDriftMode[channel];
	}
	else
	{
		debugBreakpoint();
		*gyroMode = VPAD_GYRO_ZERODRIFT_NONE;
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetGyroZeroDriftMode(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamU32(gyroMode, 1);
	cemuLog_log(LogType::InputAPI, "VPADSetGyroZeroDriftMode({}, {})", channel, gyroMode);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		if (gyroMode > VPAD_GYRO_ZERODRIFT_NONE)
		{
			debugBreakpoint();
		}
		else
		{
			g_vpadGyroZeroDriftMode[channel] = gyroMode;
		}
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetGyroDirReviseBase(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);
	ppcDefineParamStructPtr(dir, VPADDir, 1);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadGyroDirRevise[channel].vpadGyroDirReviseBase = *dir;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADDisableGyroDirRevise(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadGyroDirRevise[channel].enabled = false;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

void vpadExport_VPADSetGyroDirReviseParam(PPCInterpreter_t* hCPU)
{
	ppcDefineParamU32(channel, 0);

	if (channel < VPAD_MAX_CONTROLLERS)
	{
		g_vpadGyroDirRevise[channel].weight = (float)hCPU->fpr[1].fpr;
	}
	else
	{
		debugBreakpoint();
	}

	osLib_returnFromFunction(hCPU, 0);
}

namespace vpad
{
	void TickFunction(PPCInterpreter_t* hCPU)
	{
		// check if homebutton is pressed
		// check connection to drc
		const auto& instance = InputManager::instance();
		for (auto i = 0; i < InputManager::kMaxVPADControllers; ++i)
		{
			if (!g_vpad.controller_data[i].sampling_callback)
				continue;
			
			if(const auto controller = instance.get_vpad_controller(i))
			{
				cemuLog_log(LogType::InputAPI, "Calling VPADSamplingCallback({})", i);
				PPCCoreCallback(g_vpad.controller_data[i].sampling_callback, i);
			}
		}

		osLib_returnFromFunction(hCPU, 0);
	}

	void start()
	{
		coreinit::OSCreateAlarm(&g_vpad.alarm);
		const uint64 start_tick = coreinit::coreinit_getOSTime();
		const uint64 period_tick = coreinit::EspressoTime::GetTimerClock() * 5 / 1000;
		const MPTR handler = PPCInterpreter_makeCallableExportDepr(TickFunction);
		coreinit::OSSetPeriodicAlarm(&g_vpad.alarm, start_tick, period_tick, handler);
	}

void load()
{
	cafeExportRegister("vpad", VPADSetBtnRepeat, LogType::InputAPI);
	cafeExportRegister("vpad", VPADSetSamplingCallback, LogType::InputAPI);
	cafeExportRegister("vpad", VPADRead, LogType::InputAPI);
	
	osLib_addFunction("vpad", "VPADGetAccParam", vpadExport_VPADGetAccParam);
	osLib_addFunction("vpad", "VPADSetAccParam", vpadExport_VPADSetAccParam);
	osLib_addFunction("vpad", "VPADGetAccPlayMode", vpadExport_VPADGetAccPlayMode);
	osLib_addFunction("vpad", "VPADSetAccPlayMode", vpadExport_VPADSetAccPlayMode);

	osLib_addFunction("vpad", "VPADEnableStickCrossClamp", vpadExport_VPADEnableStickCrossClamp);
	osLib_addFunction("vpad", "VPADDisableStickCrossClamp", vpadExport_VPADDisableStickCrossClamp);
	osLib_addFunction("vpad", "VPADSetLStickClampThreshold", vpadExport_VPADSetLStickClampThreshold);
	osLib_addFunction("vpad", "VPADSetRStickClampThreshold", vpadExport_VPADSetRStickClampThreshold);
	osLib_addFunction("vpad", "VPADGetLStickClampThreshold", vpadExport_VPADGetLStickClampThreshold);
	osLib_addFunction("vpad", "VPADGetRStickClampThreshold", vpadExport_VPADGetRStickClampThreshold);

	osLib_addFunction("vpad", "VPADSetCrossStickEmulationParamsL", vpadExport_VPADSetCrossStickEmulationParamsL);
	osLib_addFunction("vpad", "VPADSetCrossStickEmulationParamsR", vpadExport_VPADSetCrossStickEmulationParamsR);
	osLib_addFunction("vpad", "VPADGetCrossStickEmulationParamsL", vpadExport_VPADGetCrossStickEmulationParamsL);
	osLib_addFunction("vpad", "VPADGetCrossStickEmulationParamsR", vpadExport_VPADGetCrossStickEmulationParamsR);

	osLib_addFunction("vpad", "VPADGetButtonProcMode", vpadExport_VPADGetButtonProcMode);
	osLib_addFunction("vpad", "VPADSetButtonProcMode", vpadExport_VPADSetButtonProcMode);
	osLib_addFunction("vpad", "VPADGetLcdMode", vpadExport_VPADGetLcdMode);
	osLib_addFunction("vpad", "VPADSetLcdMode", vpadExport_VPADSetLcdMode);

	osLib_addFunction("vpad", "VPADControlMotor", vpadExport_VPADControlMotor);
	osLib_addFunction("vpad", "VPADStopMotor", vpadExport_VPADStopMotor);

	osLib_addFunction("vpad", "VPADGetTPCalibrationParam", vpadExport_VPADGetTPCalibrationParam);
	osLib_addFunction("vpad", "VPADSetTPCalibrationParam", vpadExport_VPADSetTPCalibrationParam);
	osLib_addFunction("vpad", "VPADGetTPCalibratedPoint", vpadExport_VPADGetTPCalibratedPoint);
	osLib_addFunction("vpad", "VPADGetTPCalibratedPointEx", vpadExport_VPADGetTPCalibratedPointEx);

	//osLib_addFunction("vpad", "VPADRead", vpadExport_VPADRead);
	//osLib_addFunction("vpad", "VPADSetSamplingCallback", vpadExport_VPADSetSamplingCallback);
	//osLib_addFunction("vpad", "VPADSetBtnRepeat", vpadExport_VPADSetBtnRepeat);

	osLib_addFunction("vpad", "VPADGetGyroZeroDriftMode", vpadExport_VPADGetGyroZeroDriftMode);
	osLib_addFunction("vpad", "VPADSetGyroDirection", vpadExport_VPADSetGyroDirection);
	osLib_addFunction("vpad", "VPADSetGyroZeroDriftMode", vpadExport_VPADSetGyroZeroDriftMode);

	osLib_addFunction("vpad", "VPADSetGyroDirReviseBase", vpadExport_VPADSetGyroDirReviseBase);
	osLib_addFunction("vpad", "VPADDisableGyroDirRevise", vpadExport_VPADDisableGyroDirRevise);
	osLib_addFunction("vpad", "VPADSetGyroDirReviseParam", vpadExport_VPADSetGyroDirReviseParam);

}
}
