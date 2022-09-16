#pragma once

#include <wx/gbsizer.h>
#include "gui/input/panels/InputPanel.h"

class wxInputDraw;

class VPADInputPanel : public InputPanel
{
public:
	VPADInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller) override;

private:
	void OnVolumeChange(wxCommandEvent& event);

	wxInputDraw* m_left_draw, * m_right_draw;

	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const VPADController::ButtonId &button_id);
	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const VPADController::ButtonId &button_id, const wxString &label);
};
