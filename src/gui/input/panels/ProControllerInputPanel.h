#pragma once

#include "gui/components/wxInputDraw.h"
#include "gui/input/panels/InputPanel.h"

class ProControllerInputPanel : public InputPanel
{
  public:
	ProControllerInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller,
				  const ControllerPtr& controller) override;

  private:
	wxInputDraw *m_left_draw, *m_right_draw;
};
