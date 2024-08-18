#include "gui/input/settings/DefaultControllerSettings.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/slider.h>
#include <wx/button.h>
#include <wx/gbsizer.h>
#include <wx/statline.h>
#include <wx/checkbox.h>
#include <wx/statbox.h>

#include "gui/helpers/wxControlObject.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/components/wxInputDraw.h"
#include "gui/input/InputAPIAddWindow.h"


DefaultControllerSettings::DefaultControllerSettings(wxWindow* parent, const wxPoint& position, std::shared_ptr<ControllerBase> controller)
	: wxDialog(parent, wxID_ANY, _("Controller settings"), position, wxDefaultSize,
	           wxDEFAULT_DIALOG_STYLE), m_controller(std::move(controller))
{
	m_settings = m_controller->get_settings();
	m_rumble_backup = m_settings.rumble;

	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	// options
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Options"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		// Motion
		m_use_motion = new wxCheckBox(box, wxID_ANY, _("Use motion"));
		m_use_motion->SetValue(m_settings.motion);
		m_use_motion->Enable(m_controller->has_motion());
		box_sizer->Add(m_use_motion, 0, wxEXPAND | wxALL, 5);

		// Vibration
		auto* rumbleSizer = new wxBoxSizer(wxHORIZONTAL);

		const auto rumble = (int)(m_settings.rumble * 100);
		rumbleSizer->Add(new wxStaticText(box, wxID_ANY, _("Rumble")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
		m_rumble = new wxSlider(box, wxID_ANY, rumble, 0, 100);
		rumbleSizer->Add(m_rumble, 1, wxALL | wxEXPAND, 5);

		const auto text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", rumble));
		rumbleSizer->Add(text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
		m_rumble->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_rumble_change, this, wxID_ANY, wxID_ANY, new wxControlObject(text));

		box_sizer->Add(rumbleSizer);

		sizer->Add(box_sizer, 1, wxALL|wxEXPAND, 5);
	}

	auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
	// Axis
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Axis"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		{
			auto* content_sizer = new wxFlexGridSizer(0, 3, 0, 0);

			// Deadzone
			const auto deadzone = (int)(m_settings.axis.deadzone * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_axis_deadzone = new wxSlider(box, wxID_ANY, deadzone, 0, 100);
			content_sizer->Add(m_axis_deadzone, 1, wxALL | wxEXPAND, 5);

			const auto deadzone_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", deadzone));
			content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_axis_deadzone->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_deadzone_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(deadzone_text));


			// Range
			const auto range = (int)(m_settings.axis.range * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_axis_range = new wxSlider(box, wxID_ANY, range, 50, 200);
			content_sizer->Add(m_axis_range, 1, wxALL | wxEXPAND, 5);

			const auto range_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", range));
			content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_axis_range->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_range_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(range_text));

			content_sizer->AddSpacer(1);
			m_axis_draw = new wxInputDraw(box, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_axis_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);

			box_sizer->Add(content_sizer, 1, wxEXPAND, 0);
		}

		row_sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}
	
	// Rotation
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Rotation"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		{
			auto* content_sizer = new wxFlexGridSizer(0, 3, 0, 0);

			// Deadzone
			const auto deadzone = (int)(m_settings.rotation.deadzone * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_rotation_deadzone = new wxSlider(box, wxID_ANY, deadzone, 0, 100);
			content_sizer->Add(m_rotation_deadzone, 1, wxALL | wxEXPAND, 5);

			const auto deadzone_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", deadzone));
			content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_rotation_deadzone->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_deadzone_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(deadzone_text));


			// Range
			const auto range = (int)(m_settings.rotation.range * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_rotation_range = new wxSlider(box, wxID_ANY, range, 50, 200);
			content_sizer->Add(m_rotation_range, 1, wxALL | wxEXPAND, 5);

			const auto range_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", range));
			content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_rotation_range->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_range_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(range_text));

			content_sizer->AddSpacer(1);
			m_rotation_draw = new wxInputDraw(box, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_rotation_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);

			box_sizer->Add(content_sizer, 1, wxEXPAND, 0);
		}

		row_sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}

	// Trigger
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Trigger"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		{
			auto* content_sizer = new wxFlexGridSizer(0, 3, 0, 0);

			// Deadzone
			const auto deadzone = (int)(m_settings.trigger.deadzone * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_trigger_deadzone = new wxSlider(box, wxID_ANY, deadzone, 0, 100);
			content_sizer->Add(m_trigger_deadzone, 1, wxALL | wxEXPAND, 5);

			const auto deadzone_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", deadzone));
			content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_trigger_deadzone->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_deadzone_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(deadzone_text));


			// Range
			const auto range = (int)(m_settings.trigger.range * 100);
			content_sizer->Add(new wxStaticText(box, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_trigger_range = new wxSlider(box, wxID_ANY, range, 50, 200);
			content_sizer->Add(m_trigger_range, 1, wxALL | wxEXPAND, 5);

			const auto range_text = new wxStaticText(box, wxID_ANY, wxString::Format("%d%%", range));
			content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_trigger_range->Bind(wxEVT_SLIDER, &DefaultControllerSettings::on_range_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(range_text));

			content_sizer->AddSpacer(1);
			m_trigger_draw = new wxInputDraw(box, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_trigger_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);

			box_sizer->Add(content_sizer, 1, wxEXPAND, 0);
		}

		row_sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}
	sizer->Add(row_sizer);

	{
		auto* control_sizer = new wxFlexGridSizer(0, 4, 0, 0);
		control_sizer->AddGrowableCol(3);

		auto* ok_button = new wxButton(this, wxID_ANY, _("OK"));
		ok_button->Bind(wxEVT_BUTTON, [this](auto&) { update_settings(); EndModal(wxID_OK); });
		control_sizer->Add(ok_button, 0, wxALL, 5);

		control_sizer->Add(0, 0, 0, wxEXPAND, 5);

		auto* cancel_button = new wxButton(this, wxID_ANY, _("Cancel"));
		cancel_button->Bind(wxEVT_BUTTON, [this](auto&) { EndModal(wxID_CANCEL); });
		control_sizer->Add(cancel_button, 0, wxALL, 5);

		auto* calibrate_button = new wxButton(this, wxID_ANY, _("Calibrate"));
		calibrate_button->Bind(wxEVT_BUTTON, [this](auto&) { m_controller->calibrate(); });
		control_sizer->Add(calibrate_button, 0, wxALL | wxALIGN_RIGHT, 5);

		sizer->Add(control_sizer, 0, wxEXPAND, 5);
	}

	

	this->SetSizer(sizer);
	this->Layout();
	this->Fit();

	this->Bind(wxEVT_CLOSE_WINDOW, &DefaultControllerSettings::on_close, this);

	m_timer = new wxTimer(this);
	Bind(wxEVT_TIMER, &DefaultControllerSettings::on_timer, this);
	m_timer->Start(100);
}

DefaultControllerSettings::~DefaultControllerSettings()
{
	m_controller->stop_rumble();
	m_timer->Stop();
}

void DefaultControllerSettings::update_settings()
{
	// update settings
	m_controller->set_settings(m_settings);
	if (m_use_motion)
		m_controller->set_use_motion(m_use_motion->GetValue());
}

void DefaultControllerSettings::on_timer(wxTimerEvent& event)
{
	if (m_rumble_time.has_value() && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_rumble_time.value()).count() > 500 )
	{
		m_controller->stop_rumble();
		m_rumble_time = {};
	}

	const auto& default_state = m_controller->get_default_state();

	auto state = m_controller->raw_state();
	m_controller->apply_axis_setting(state.axis, default_state.axis, m_settings.axis);
	m_controller->apply_axis_setting(state.rotation, default_state.rotation, m_settings.rotation);
	m_controller->apply_axis_setting(state.trigger, default_state.trigger, m_settings.trigger);

	if (m_controller->api() == InputAPI::SDLController)
	{
		state.axis.y *= -1;
		state.rotation.y *= -1;
	}

	m_axis_draw->SetDeadzone(m_settings.axis.deadzone);
	m_axis_draw->SetAxisValue(state.axis);

	m_rotation_draw->SetDeadzone(m_settings.rotation.deadzone);
	m_rotation_draw->SetAxisValue(state.rotation);

	m_trigger_draw->SetDeadzone(m_settings.trigger.deadzone);
	m_trigger_draw->SetAxisValue(state.trigger);
}

void DefaultControllerSettings::on_close(wxCloseEvent& event)
{
	if (this->GetReturnCode() == 0 || this->GetReturnCode() == wxID_OK)
		update_settings();

	event.Skip();
}

void DefaultControllerSettings::on_deadzone_change(wxCommandEvent& event)
{
	update_slider_text(event);
	const auto new_value = (float)event.GetInt() / 100.0f;

	if (event.GetEventObject() == m_axis_deadzone)
		m_settings.axis.deadzone = new_value;
	else if (event.GetEventObject() == m_rotation_deadzone)
		m_settings.rotation.deadzone = new_value;
	else if (event.GetEventObject() == m_trigger_deadzone)
		m_settings.trigger.deadzone = new_value;
}

void DefaultControllerSettings::on_range_change(wxCommandEvent& event)
{
	update_slider_text(event);

	const auto new_value = (float)event.GetInt() / 100.0f;

	if (event.GetEventObject() == m_axis_range)
		m_settings.axis.range = new_value;
	else if (event.GetEventObject() == m_rotation_range)
		m_settings.rotation.range = new_value;
	else if (event.GetEventObject() == m_trigger_range)
		m_settings.trigger.range = new_value;
}

void DefaultControllerSettings::on_rumble_change(wxCommandEvent& event)
{
	update_slider_text(event);

	const auto rumble_value = event.GetInt();
	m_settings.rumble = (float)rumble_value / 100.0f;

	m_controller->set_rumble(m_settings.rumble);
	if (rumble_value != 0)
		m_controller->start_rumble();
	else
		m_controller->stop_rumble();

	m_controller->set_rumble(m_rumble_backup);

	m_rumble_time = std::chrono::steady_clock::now();
}
