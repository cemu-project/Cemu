#pragma once

#include "gui/input/panels/InputPanel.h"

class wxInputDraw;

class ClassicControllerInputPanel : public InputPanel
{
  public:
	ClassicControllerInputPanel(wxWindow* parent);

  private:
	wxInputDraw *m_left_draw, *m_right_draw;
};
