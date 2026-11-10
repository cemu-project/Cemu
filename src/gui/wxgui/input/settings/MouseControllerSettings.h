#pragma once

#include "input/api/Controller.h"
#include "wx/dialog.h"

#include <memory>

class wxCheckBox;
class wxPoint;
class wxWindow;
class MouseController;

class MouseControllerSettings : public wxDialog
{
public:
	MouseControllerSettings(wxWindow* parent, const wxPoint& position, std::shared_ptr<MouseController> controller);

private:
	std::shared_ptr<MouseController> m_controller;

	wxCheckBox* m_usePosition = nullptr;

	void UpdateSettings();
	void OnClose(wxCloseEvent& event);
};
