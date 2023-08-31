#include "gui/input/settings/WiimoteControllerSettings.h"

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

#ifdef SUPPORTS_WIIMOTE

WiimoteControllerSettings::WiimoteControllerSettings(wxWindow* parent, const wxPoint& position, std::shared_ptr<NativeWiimoteController> controller)
	: wxDialog(parent, wxID_ANY, _("Controller settings"), position, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE), m_controller(std::move(controller))
{
	m_settings = m_controller->get_settings();
	m_rumble_backup = m_settings.rumble;
	m_packet_delay_backup = m_controller->get_packet_delay();

	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	// extension info
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Connected extension"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		m_extension_text = new wxStaticText(box, wxID_ANY, _("None"));
		box_sizer->Add(m_extension_text, 0, wxALL, 5);

		sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}

	// options
	{
		auto* box = new wxStaticBox(this, wxID_ANY, _("Options"));
		auto* box_sizer = new wxStaticBoxSizer(box, wxVERTICAL);

		{
			auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);
			// Rumble
			m_rumble = new wxCheckBox(box, wxID_ANY, _("Rumble"));
			m_rumble->SetValue(m_settings.rumble > 0);
			row_sizer->Add(m_rumble, 0, wxALL, 5);

			m_rumble->Bind(wxEVT_CHECKBOX, &WiimoteControllerSettings::on_rumble_change, this);

			// Motion
			m_use_motion = new wxCheckBox(box, wxID_ANY, _("Use motion"));
			m_use_motion->SetValue(m_settings.motion);
			m_use_motion->SetValue(m_settings.motion);
			m_use_motion->Enable(m_controller->has_motion());
			row_sizer->Add(m_use_motion, 0, wxALL, 5);

			box_sizer->Add(row_sizer);
		}
		{
			auto* row_sizer = new wxBoxSizer(wxHORIZONTAL);

			// Delay
			row_sizer->Add(new wxStaticText(box, wxID_ANY, _("Packet delay")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_package_delay = new wxSlider(box, wxID_ANY, m_packet_delay_backup, 1, 100);
			row_sizer->Add(m_package_delay, 1, wxALL | wxEXPAND, 5);

			const auto range_text = new wxStaticText(box, wxID_ANY, wxString::Format("%dms", m_packet_delay_backup));
			row_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_package_delay->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_delay_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(range_text));

			box_sizer->Add(row_sizer);
		}

		sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}

	// Nunchuck
	{
		m_nunchuck_settings = new wxStaticBox(this, wxID_ANY, _("Nunchuck"));
		auto* box_sizer = new wxStaticBoxSizer(m_nunchuck_settings, wxVERTICAL);

		{
			auto* content_sizer = new wxFlexGridSizer(0, 3, 0, 0);

			// Deadzone
			const auto deadzone = (int)(m_settings.axis.deadzone * 100);
			content_sizer->Add(new wxStaticText(m_nunchuck_settings, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_nunchuck_deadzone = new wxSlider(m_nunchuck_settings, wxID_ANY, deadzone, 0, 100);
			content_sizer->Add(m_nunchuck_deadzone, 1, wxALL | wxEXPAND, 5);

			const auto deadzone_text = new wxStaticText(m_nunchuck_settings, wxID_ANY, wxString::Format("%d%%", deadzone));
			content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_nunchuck_deadzone->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(deadzone_text));


			// Range
			const auto range = (int)(m_settings.axis.range * 100);
			content_sizer->Add(new wxStaticText(m_nunchuck_settings, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
			m_nunchuck_range = new wxSlider(m_nunchuck_settings, wxID_ANY, range, 50, 200);
			content_sizer->Add(m_nunchuck_range, 1, wxALL | wxEXPAND, 5);

			const auto range_text = new wxStaticText(m_nunchuck_settings, wxID_ANY, wxString::Format("%d%%", range));
			content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

			m_nunchuck_range->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
				new wxControlObject(range_text));

			content_sizer->AddSpacer(1);
			m_nunchuck_draw = new wxInputDraw(m_nunchuck_settings, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_nunchuck_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);

			box_sizer->Add(content_sizer, 1, wxEXPAND, 0);
		}

		sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}

	// Classic
	{
		m_classic_settings = new wxStaticBox(this, wxID_ANY, _("Classic"));
		auto* box_sizer = new wxStaticBoxSizer(m_classic_settings, wxVERTICAL);

		{
			auto* content_sizer = new wxFlexGridSizer(0, 6, 0, 0);

			// Deadzone
			{
				// Axis
				const auto deadzone = (int)(m_settings.axis.deadzone * 100);
				content_sizer->Add(new wxStaticText(m_classic_settings, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
				m_classic_axis_deadzone = new wxSlider(m_classic_settings, wxID_ANY, deadzone, 0, 100);
				content_sizer->Add(m_classic_axis_deadzone, 1, wxALL | wxEXPAND, 5);

				const auto deadzone_text = new wxStaticText(m_classic_settings, wxID_ANY, wxString::Format("%d%%", deadzone));
				content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

				m_classic_axis_deadzone->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
					new wxControlObject(deadzone_text));
			}
			{
				// Range
				const auto deadzone = (int)(m_settings.rotation.deadzone * 100);
				content_sizer->Add(new wxStaticText(m_classic_settings, wxID_ANY, _("Deadzone")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
				m_classic_rotation_deadzone = new wxSlider(m_classic_settings, wxID_ANY, deadzone, 0, 100);
				content_sizer->Add(m_classic_rotation_deadzone, 1, wxALL | wxEXPAND, 5);

				const auto deadzone_text = new wxStaticText(m_classic_settings, wxID_ANY, wxString::Format("%d%%", deadzone));
				content_sizer->Add(deadzone_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

				m_classic_rotation_deadzone->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
					new wxControlObject(deadzone_text));
			}

			// Range
			{
				// Axis
				const auto range = (int)(m_settings.axis.range * 100);
				content_sizer->Add(new wxStaticText(m_classic_settings, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
				m_classic_axis_range = new wxSlider(m_classic_settings, wxID_ANY, range, 50, 200);
				content_sizer->Add(m_classic_axis_range, 1, wxALL | wxEXPAND, 5);

				const auto range_text = new wxStaticText(m_classic_settings, wxID_ANY, wxString::Format("%d%%", range));
				content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

				m_classic_axis_range->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
					new wxControlObject(range_text));
			}
			{
				// Rotation
				const auto range = (int)(m_settings.rotation.range * 100);
				content_sizer->Add(new wxStaticText(m_classic_settings, wxID_ANY, _("Range")), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
				m_classic_rotation_range = new wxSlider(m_classic_settings, wxID_ANY, range, 50, 200);
				content_sizer->Add(m_classic_rotation_range, 1, wxALL | wxEXPAND, 5);

				const auto range_text = new wxStaticText(m_classic_settings, wxID_ANY, wxString::Format("%d%%", range));
				content_sizer->Add(range_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

				m_classic_rotation_range->Bind(wxEVT_SLIDER, &WiimoteControllerSettings::on_slider_change, this, wxID_ANY, wxID_ANY,
					new wxControlObject(range_text));
			}

			content_sizer->AddSpacer(1);
			m_classic_axis_draw = new wxInputDraw(m_classic_settings, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_classic_axis_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);
			content_sizer->AddSpacer(1);

			content_sizer->AddSpacer(1);
			m_classic_rotation_draw = new wxInputDraw(m_classic_settings, wxID_ANY, wxDefaultPosition, { 60, 60 });
			content_sizer->Add(m_classic_rotation_draw, 0, wxTOP | wxBOTTOM | wxALIGN_CENTER, 5);

			box_sizer->Add(content_sizer, 1, wxEXPAND, 0);
		}

		sizer->Add(box_sizer, 0, wxALL | wxEXPAND, 5);
	}

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

	this->Bind(wxEVT_CLOSE_WINDOW, &WiimoteControllerSettings::on_close, this);

	m_timer = new wxTimer(this);
	Bind(wxEVT_TIMER, &WiimoteControllerSettings::on_timer, this);
	m_timer->Start(100);
}

WiimoteControllerSettings::~WiimoteControllerSettings()
{
	m_controller->stop_rumble();
	m_timer->Stop();
}

void WiimoteControllerSettings::update_settings()
{
	// update settings
	m_controller->set_settings(m_settings);
	if (m_use_motion)
		m_controller->set_use_motion(m_use_motion->GetValue());

	m_controller->set_packet_delay(m_package_delay->GetValue());
}

void WiimoteControllerSettings::on_timer(wxTimerEvent& event)
{
	if (m_rumble_time.has_value() && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_rumble_time.value()).count() > 500)
	{
		m_controller->stop_rumble();
		m_rumble_time = {};
	}

	const auto& default_state = m_controller->get_default_state();

	auto state = m_controller->raw_state();
	m_controller->apply_axis_setting(state.axis, default_state.axis, m_settings.axis);
	m_controller->apply_axis_setting(state.rotation, default_state.rotation, m_settings.rotation);
	m_controller->apply_axis_setting(state.trigger, default_state.trigger, m_settings.trigger);

	if (m_nunchuck_settings->IsEnabled())
	{
		m_nunchuck_draw->SetDeadzone(m_settings.axis.deadzone);
		m_nunchuck_draw->SetAxisValue(state.axis);
	}

	if (m_classic_settings->IsEnabled())
	{
		m_classic_axis_draw->SetDeadzone(m_settings.axis.deadzone);
		m_classic_axis_draw->SetAxisValue(state.axis);

		m_classic_rotation_draw->SetDeadzone(m_settings.rotation.deadzone);
		m_classic_rotation_draw->SetAxisValue(state.rotation);
	}

	wxString label;
	switch (m_controller->get_extension())
	{
	case NativeWiimoteController::Nunchuck:
		label = _("Nunchuck");
		m_nunchuck_settings->Enable();
		m_classic_settings->Disable();
		break; 
	case NativeWiimoteController::Classic: 
		label = _("Classic");
		m_nunchuck_settings->Disable();
		m_classic_settings->Enable();
		break;
	default: 
		m_nunchuck_settings->Disable();
		m_classic_settings->Disable();
	}

	if(m_controller->is_mpls_attached())
	{
		const bool empty = label.empty();
		if (!empty)
			label.Append(" (");

		label.Append(_("MotionPlus"));

		if (!empty)
			label.Append(")");
	}
	
	if(label.empty())
	{
		label = _("None");
	}

	m_extension_text->SetLabelText(label);
}

void WiimoteControllerSettings::on_close(wxCloseEvent& event)
{
	if (this->GetReturnCode() == 0 || this->GetReturnCode() == wxID_OK)
		update_settings();

	event.Skip();
}

void WiimoteControllerSettings::on_slider_change(wxCommandEvent& event)
{
	update_slider_text(event);
	const auto new_value = (float)event.GetInt() / 100.0f;

	auto* obj = event.GetEventObject();
	if (obj == m_nunchuck_deadzone || obj == m_classic_axis_deadzone)
		m_settings.axis.deadzone = new_value;
	else if (obj == m_nunchuck_range || obj == m_classic_axis_range)
			m_settings.axis.range = new_value;
	else if (obj == m_classic_rotation_deadzone)
			m_settings.rotation.deadzone = new_value;
	else if (obj == m_classic_rotation_range)
			m_settings.rotation.range = new_value;
}

void WiimoteControllerSettings::on_rumble_change(wxCommandEvent& event)
{
	const auto rumble_value = m_rumble->GetValue();
	m_settings.rumble = rumble_value ? 1.0f : 0.0f;

	m_controller->set_rumble(m_settings.rumble);
	if (rumble_value)
		m_controller->start_rumble();
	else
		m_controller->stop_rumble();

	m_controller->set_rumble(m_rumble_backup);

	m_rumble_time = std::chrono::steady_clock::now();
}

void WiimoteControllerSettings::on_delay_change(wxCommandEvent& event)
{
	update_slider_text(event, "%dms");
}

#endif