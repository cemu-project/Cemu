#pragma once

#include <wx/cmdargs.h>

#include "input/emulated/EmulatedController.h"
#include "Cafe/OS/libs/padscore/padscore.h"
#include "Cafe/OS/libs/vpad/vpad.h"

constexpr uint32 kWPADButtonRepeat = 0x80000000;

enum WPADDeviceType
{
	kWAPDevCore = 0,
	kWAPDevFreestyle = 1,
	kWAPDevClassic = 2,
	kWAPDevMPLS = 5,
	kWAPDevMPLSFreeStyle = 6,
	kWAPDevMPLSClassic = 7,
	kWAPDevURCC = 31,
	kWAPDevNotFound = 253,
	kWAPDevUnknown = 255,
};

// core, balanceboard
enum WPADCoreButtons
{
	kWPADButton_Left = 0x1,
	kWPADButton_Right = 0x2,
	kWPADButton_Down = 0x4,
	kWPADButton_Up = 0x8,
	kWPADButton_Plus = 0x10,
	kWPADButton_2 = 0x100,
	kWPADButton_1 = 0x200,
	kWPADButton_B = 0x400,
	kWPADButton_A = 0x800,
	kWPADButton_Minus = 0x1000,
	kWPADButton_Home = 0x8000,
};

// Nunchuck aka Freestyle
enum WPADNunchuckButtons
{
	kWPADButton_Z = 0x2000,
	kWPADButton_C = 0x4000,
};

// Classic Controller
enum WPADClassicButtons
{
	kCLButton_Up = 0x1,
	kCLButton_Left = 0x2,
	kCLButton_ZR = 0x4,
	kCLButton_X = 0x8,
	kCLButton_A = 0x10,
	kCLButton_Y = 0x20,
	kCLButton_B = 0x40,
	kCLButton_ZL = 0x80,
	kCLButton_R = 0x200,
	kCLButton_Plus = 0x400,
	kCLButton_Home = 0x800,
	kCLButton_Minus = 0x1000,
	kCLButton_L = 0x2000,
	kCLButton_Down = 0x4000,
	kCLButton_Right = 0x8000
};

// Pro Controller aka URCC
enum WPADProButtons
{
	kProButton_Up = 0x1,
	kProButton_Left = 0x2,
	kProButton_ZR = 0x4,
	kProButton_X = 0x8,
	kProButton_A = 0x10,
	kProButton_Y = 0x20,
	kProButton_B = 0x40,
	kProButton_ZL = 0x80,
	kProButton_R = 0x200,
	kProButton_Plus = 0x400,
	kProButton_Home = 0x800,
	kProButton_Minus = 0x1000,
	kProButton_L = 0x2000,
	kProButton_Down = 0x4000,
	kProButton_Right = 0x8000,
	kProButton_StickR = 0x10000,
	kProButton_StickL = 0x20000
};

enum WPADDataFormat {
	 kDataFormat_CORE = 0,
	 kDataFormat_CORE_ACC = 1,
	 kDataFormat_CORE_ACC_DPD = 2,
	 kDataFormat_FREESTYLE = 3,
	 kDataFormat_FREESTYLE_ACC = 4,
	 kDataFormat_FREESTYLE_ACC_DPD = 5,
	 kDataFormat_CLASSIC = 6,
	 kDataFormat_CLASSIC_ACC = 7,
	 kDataFormat_CLASSIC_ACC_DPD = 8,
	 kDataFormat_CORE_ACC_DPD_FULL = 9, // buttons, motion, pointing
	 kDataFormat_TRAIN = 10,
	 kDataFormat_GUITAR = 11,
	 kDataFormat_BALANCE_CHECKER = 12,
	 kDataFormat_DRUM = 15,
	 kDataFormat_MPLS = 16, // buttons, motion, pointing, motion plus
	 kDataFormat_TAIKO = 17,
	 kDataFormat_URCC = 22, // buttons, URCC aka pro
};

class WPADController : public EmulatedController
{
	using base_type = EmulatedController;
public:
	WPADController(size_t player_index, WPADDataFormat data_format);

	uint32 get_emulated_button_flag(WPADDataFormat format, uint32 id) const;

	virtual WPADDeviceType get_device_type() const = 0;

	WPADDataFormat get_data_format() const { return m_data_format; }
	void set_data_format(WPADDataFormat data_format) { m_data_format = data_format; }

	void WPADRead(WPADStatus_t* status);

	void KPADRead(KPADStatus_t& status, const BtnRepeat& repeat);
	virtual bool is_mpls_attached() { return false; }

	enum class ConnectCallbackStatus
	{
		None, // do nothing
		ReportDisconnect, // call disconnect
		ReportConnect, // call connect
	};
	ConnectCallbackStatus m_status = ConnectCallbackStatus::ReportConnect;
	ConnectCallbackStatus m_extension_status = ConnectCallbackStatus::ReportConnect;

	WPADDataFormat get_default_data_format() const;

protected:
	WPADDataFormat m_data_format;

private:
	uint32be m_last_holdvalue = 0;

	std::chrono::steady_clock::time_point m_last_hold_change{}, m_last_pulse{};

	
};
