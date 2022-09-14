#pragma once

#include <wx/gbsizer.h>
#include "input/emulated/ClassicController.h"
#include "gui/input/panels/InputPanel.h"

class wxInputDraw;

class ClassicControllerInputPanel : public InputPanel
{
public:
	ClassicControllerInputPanel(wxWindow* parent);

private:
	wxInputDraw* m_left_draw, * m_right_draw;

	void add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const ClassicController::ButtonId &button_id);
};

