#pragma once

#ifdef SUPPORTS_WIIMOTE

#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/slider.h>

#include "input/api/Controller.h"
#include "input/api/Wiimote/NativeWiimoteController.h"

class wxStaticBox;
class wxStaticText;
class wxCheckBox;
class wxInputDraw;

class WiimoteControllerSettings : public wxDialog
{
public:
	WiimoteControllerSettings(wxWindow* parent, const wxPoint& position, std::shared_ptr<NativeWiimoteController> controller);
	~WiimoteControllerSettings();

private:
	void update_settings();

	std::shared_ptr<NativeWiimoteController> m_controller;
	ControllerBase::Settings m_settings;
	float m_rumble_backup;
	uint32 m_packet_delay_backup;

	wxTimer* m_timer;
	std::optional<std::chrono::steady_clock::time_point> m_rumble_time{};

	wxStaticText* m_extension_text;

	wxSlider* m_package_delay;
	wxCheckBox* m_rumble = nullptr;
	wxCheckBox* m_use_motion = nullptr;

	wxStaticBox* m_nunchuck_settings;
	wxSlider* m_nunchuck_deadzone, * m_nunchuck_range;
	wxInputDraw* m_nunchuck_draw;

	wxStaticBox* m_classic_settings;
	wxSlider* m_classic_axis_deadzone, * m_classic_axis_range;
	wxInputDraw* m_classic_axis_draw;
	wxSlider* m_classic_rotation_deadzone, * m_classic_rotation_range;
	wxInputDraw* m_classic_rotation_draw;

	

	

	void on_timer(wxTimerEvent& event);
	void on_close(wxCloseEvent& event);
	void on_slider_change(wxCommandEvent& event);
	void on_rumble_change(wxCommandEvent& event);
	void on_delay_change(wxCommandEvent& event);
};

#endif