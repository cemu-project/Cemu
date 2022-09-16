#pragma once

#include <wx/gbsizer.h>
#include "input/emulated/ProController.h"
#include "gui/input/panels/InputPanel.h"
#include "gui/components/wxInputDraw.h"

class ProControllerInputPanel : public InputPanel
{
public:
	ProControllerInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller) override;

private:
	wxInputDraw* m_left_draw, * m_right_draw;

	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const ProController::ButtonId &button_id);
};
