#include "MouseControllerSettings.h"

#include "input/api/Mouse/MouseController.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/statbox.h>

#include "wxgui/helpers/wxHelpers.h"
#include "wxgui/input/InputAPIAddWindow.h"


MouseControllerSettings::MouseControllerSettings(wxWindow* parent, const wxPoint& position, std::shared_ptr<MouseController> controller)
	: wxDialog(parent, wxID_ANY, _("Controller settings"), position, wxDefaultSize,
	           wxDEFAULT_DIALOG_STYLE), m_controller(std::move(controller))
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	// options
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Options"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		// Position (wiimote pointer)
		m_usePosition = new wxCheckBox(box, wxID_ANY, _("Use position"));
		m_usePosition->SetValue(m_controller->IsPositionEnabled());
		m_usePosition->Enable(m_controller->has_position());
		box_sizer->Add(m_usePosition, 0, wxEXPAND | wxALL, 5);

		sizer->Add(box_sizer, 1, wxALL|wxEXPAND, 5);
	}

	{
		auto* control_sizer = new wxFlexGridSizer(0, 3, 0, 0);
		control_sizer->AddGrowableCol(2);

		auto* ok_button = new wxButton(this, wxID_ANY, _("OK"));
		ok_button->Bind(wxEVT_BUTTON, [this](auto&) { UpdateSettings(); EndModal(wxID_OK); });
		control_sizer->Add(ok_button, 0, wxALL, 5);

		control_sizer->Add(0, 0, 0, wxEXPAND, 5);

		auto* cancel_button = new wxButton(this, wxID_ANY, _("Cancel"));
		cancel_button->Bind(wxEVT_BUTTON, [this](auto&) { EndModal(wxID_CANCEL); });
		control_sizer->Add(cancel_button, 0, wxALL, 5);

		sizer->Add(control_sizer, 0, wxEXPAND, 5);
	}
	
	this->SetSizer(sizer);
	this->Layout();
	this->Fit();

	this->Bind(wxEVT_CLOSE_WINDOW, &MouseControllerSettings::OnClose, this);
}

void MouseControllerSettings::UpdateSettings()
{
	if (m_usePosition)
		m_controller->SetPositionEnabled(m_usePosition->GetValue());
}

void MouseControllerSettings::OnClose(wxCloseEvent& event)
{
	if (this->GetReturnCode() == 0 || this->GetReturnCode() == wxID_OK)
		UpdateSettings();

	event.Skip();
}
