#pragma once

#include "gui/input/panels/InputPanel.h"

class wxInputDraw;

class VPADInputPanel : public InputPanel
{
  public:
	VPADInputPanel(wxWindow* parent);

	void on_timer(const EmulatedControllerPtr& emulated_controller,
				  const ControllerPtr& controller) override;

  private:
	void OnVolumeChange(wxCommandEvent& event);

	wxInputDraw *m_left_draw, *m_right_draw;
};
