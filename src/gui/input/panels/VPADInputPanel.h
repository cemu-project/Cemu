#pragma once

#include <wx/gbsizer.h>
#include "gui/input/panels/InputPanel.h"
#include "input/emulated/VPADController.h"

class wxInputDraw;
class wxCheckBox;
class wxSpinCtrl;

class VPADInputPanel : public InputPanel
{
public:
	VPADInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller) override;
	virtual void load_controller(const EmulatedControllerPtr& controller) override;

private:
	void OnVolumeChange(wxCommandEvent& event);

	wxInputDraw* m_left_draw, * m_right_draw;
	wxCheckBox* m_togglePadViewCheckBox;
	std::array<wxSpinCtrl*, GamePadTouch::kBindingCount> m_touchX{}, m_touchY{};
	std::weak_ptr<VPADController> m_touchController;

	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const VPADController::ButtonId &button_id);
	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const VPADController::ButtonId &button_id, const wxString &label);
};
