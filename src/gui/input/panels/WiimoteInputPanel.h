#pragma once

#include "gui/input/panels/InputPanel.h"
#include "input/emulated/WiimoteController.h"
#include <wx/slider.h>

class wxCheckBox;
class wxGridBagSizer;
class wxInputDraw;

class WiimoteInputPanel : public InputPanel
{
public:
	WiimoteInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller) override;

	void load_controller(const EmulatedControllerPtr& emulated_controller) override;

private:
	wxInputDraw* m_draw;

	WPADDeviceType m_device_type = kWAPDevCore;
	void set_active_device_type(WPADDeviceType type);

	void on_volume_change(wxCommandEvent& event);
	void on_extension_change(wxCommandEvent& event);
    void on_pair_button(wxCommandEvent& event);

	wxGridBagSizer* m_item_sizer;

	wxCheckBox* m_nunchuck, * m_classic;
	wxCheckBox* m_motion_plus;

	wxSlider* m_volume;

	std::vector<wxWindow*> m_nunchuck_items;

	void add_button_row(sint32 row, sint32 column, const WiimoteController::ButtonId &button_id);
};




