#include "gui/input/panels/WiimoteInputPanel.h"

#include <wx/button.h>
#include <wx/gbsizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/slider.h>
#include <wx/checkbox.h>

#include "gui/helpers/wxControlObject.h"
#include "input/emulated/WiimoteController.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/components/wxInputDraw.h"
#include "gui/PairingDialog.h"

constexpr WiimoteController::ButtonId g_kFirstColumnItems[] =
{
	WiimoteController::kButtonId_A, WiimoteController::kButtonId_B, WiimoteController::kButtonId_1, WiimoteController::kButtonId_2, WiimoteController::kButtonId_Plus, WiimoteController::kButtonId_Minus, WiimoteController::kButtonId_Home
};

constexpr WiimoteController::ButtonId g_kSecondColumnItems[] =
{
	WiimoteController::kButtonId_Up, WiimoteController::kButtonId_Down, WiimoteController::kButtonId_Left, WiimoteController::kButtonId_Right
};

constexpr WiimoteController::ButtonId g_kThirdColumnItems[] =
{
	WiimoteController::kButtonId_Nunchuck_C, WiimoteController::kButtonId_Nunchuck_Z,
	WiimoteController::kButtonId_None,
	WiimoteController::kButtonId_Nunchuck_Up,WiimoteController::kButtonId_Nunchuck_Down,WiimoteController::kButtonId_Nunchuck_Left,WiimoteController::kButtonId_Nunchuck_Right
};

WiimoteInputPanel::WiimoteInputPanel(wxWindow* parent)
	: InputPanel(parent)
{
	auto bold_font = GetFont();
	bold_font.MakeBold();

	auto* main_sizer = new wxBoxSizer(wxVERTICAL);
    auto* horiz_main_sizer = new wxBoxSizer(wxHORIZONTAL);

    auto* pair_button = new wxButton(this, wxID_ANY, _("Pair a Wii or Wii U controller"));
    pair_button->Bind(wxEVT_BUTTON, &WiimoteInputPanel::on_pair_button, this);
    horiz_main_sizer->Add(pair_button);
    horiz_main_sizer->AddSpacer(10);

    auto* extensions_sizer = new wxBoxSizer(wxHORIZONTAL);
    horiz_main_sizer->Add(extensions_sizer, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));

    extensions_sizer->Add(new wxStaticText(this, wxID_ANY, _("Extensions:")));
    extensions_sizer->AddSpacer(10);

	m_motion_plus = new wxCheckBox(this, wxID_ANY, _("MotionPlus"));
	m_motion_plus->Bind(wxEVT_CHECKBOX, &WiimoteInputPanel::on_extension_change, this);
	extensions_sizer->Add(m_motion_plus);

	m_nunchuck = new wxCheckBox(this, wxID_ANY, _("Nunchuck"));
	m_nunchuck->Bind(wxEVT_CHECKBOX, &WiimoteInputPanel::on_extension_change, this);
	extensions_sizer->Add(m_nunchuck);

	m_classic = new wxCheckBox(this, wxID_ANY, _("Classic"));
	m_classic->Bind(wxEVT_CHECKBOX, &WiimoteInputPanel::on_extension_change, this);
	m_classic->Hide();
	extensions_sizer->Add(m_classic);

	main_sizer->Add(horiz_main_sizer, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(new wxStaticLine(this), 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 5);

	m_item_sizer = new wxGridBagSizer();

	sint32 row = 0;
	sint32 column = 0;
	for (const auto& id : g_kFirstColumnItems)
	{
		row++;
		add_button_row(row, column, id);
	}

	m_item_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 2), wxGBSpan(11, 1), wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	row = 0;
	column += 3;

	auto text = new wxStaticText(this, wxID_ANY, _("D-pad"));
	text->SetFont(bold_font);
	m_item_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (const auto& id : g_kSecondColumnItems)
	{
		row++;
		add_button_row(row, column, id);
	}

	row = 8;
	// Volume
	text = new wxStaticText(this, wxID_ANY, _("Volume"));
	text->Disable();
	m_item_sizer->Add(text, wxGBPosition(row, column), wxDefaultSpan, wxALL, 5);

	m_volume = new wxSlider(this, wxID_ANY, 0, 0, 100);
	m_volume->Disable();
	m_item_sizer->Add(m_volume, wxGBPosition(row, column + 1), wxDefaultSpan, wxTOP | wxBOTTOM | wxEXPAND, 5);

	const auto volume_text = new wxStaticText(this, wxID_ANY, wxString::Format("%d%%", 0));
	volume_text->Disable();
	m_item_sizer->Add(volume_text, wxGBPosition(row, column + 2), wxDefaultSpan, wxALL, 5);
	m_volume->Bind(wxEVT_SLIDER, &WiimoteInputPanel::on_volume_change, this, wxID_ANY, wxID_ANY, new wxControlObject(volume_text));
	row++;

	m_item_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 3), wxGBSpan(11, 1), wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	row = 0;
	column += 4;

	text = new wxStaticText(this, wxID_ANY, _("Nunchuck"));
	text->SetFont(bold_font);
	m_item_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (const auto& id : g_kThirdColumnItems)
	{
		row++;
		if (id == WiimoteController::kButtonId_None)
			continue;

		m_item_sizer->Add(
			new wxStaticText(this, wxID_ANY, wxGetTranslation(to_wxString(WiimoteController::get_button_name(id)))),
			wxGBPosition(row, column),
			wxDefaultSpan,
			wxALL | wxALIGN_CENTER_VERTICAL, 5);

		auto* text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
		text_ctrl->SetClientData((void*)id);
		text_ctrl->SetMinSize(wxSize(150, -1));
		text_ctrl->SetEditable(false);
		text_ctrl->SetBackgroundColour(kKeyColourNormalMode);
		bind_hotkey_events(text_ctrl);
		text_ctrl->Enable(m_nunchuck->GetValue());
		m_item_sizer->Add(text_ctrl, wxGBPosition(row, column + 1), wxDefaultSpan, wxALL | wxEXPAND, 5);

		m_nunchuck_items.push_back(text_ctrl);
	}


	// input drawer
	m_draw = new wxInputDraw(this, wxID_ANY, wxDefaultPosition, { 60, 60 });
	m_draw->Enable(m_nunchuck->GetValue());
	m_item_sizer->Add(5, 0, wxGBPosition(3, column + 3), wxDefaultSpan, wxTOP | wxBOTTOM | wxEXPAND | wxALIGN_CENTER, 5);
	m_item_sizer->Add(m_draw, wxGBPosition(3, column + 4), wxGBSpan(4, 1), wxTOP | wxBOTTOM | wxEXPAND | wxALIGN_CENTER, 5);

	m_nunchuck_items.push_back(m_draw);

	//////////////////////////////////////////////////////////////////

	main_sizer->Add(m_item_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);
	
	SetSizer(main_sizer);
	Layout();
}

void WiimoteInputPanel::add_button_row(sint32 row, sint32 column, const WiimoteController::ButtonId &button_id) {
	m_item_sizer->Add(
		new wxStaticText(this, wxID_ANY, wxGetTranslation(to_wxString(WiimoteController::get_button_name(button_id)))),
		wxGBPosition(row, column),
		wxDefaultSpan,
		wxALL | wxALIGN_CENTER_VERTICAL, 5);

	auto* text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	text_ctrl->SetClientData((void*)button_id);
	text_ctrl->SetMinSize(wxSize(150, -1));
	text_ctrl->SetEditable(false);
	text_ctrl->SetBackgroundColour(kKeyColourNormalMode);
	bind_hotkey_events(text_ctrl);
	m_item_sizer->Add(text_ctrl, wxGBPosition(row, column + 1), wxDefaultSpan, wxALL | wxEXPAND, 5);
}

void WiimoteInputPanel::set_active_device_type(WPADDeviceType type)
{
	m_device_type = type;

	m_motion_plus->SetValue(type == kWAPDevMPLS || type == kWAPDevMPLSFreeStyle || type == kWAPDevMPLSClassic);
	switch(type)
	{
	case kWAPDevFreestyle: 
	case kWAPDevMPLSFreeStyle:
		m_nunchuck->SetValue(true);
		m_classic->SetValue(false);
		for (const auto& item : m_nunchuck_items)
		{
			item->Enable(true);
		}
		break;

	case kWAPDevClassic: 
	case kWAPDevMPLSClassic:
		m_nunchuck->SetValue(false);
		m_classic->SetValue(true);
		for (const auto& item : m_nunchuck_items)
		{
			item->Enable(false);
		}
		break;

	default:
		m_nunchuck->SetValue(false);
		m_classic->SetValue(false);
		for (const auto& item : m_nunchuck_items)
		{
			item->Enable(false);
		}
	}
}

void WiimoteInputPanel::on_volume_change(wxCommandEvent& event)
{
}

void WiimoteInputPanel::on_extension_change(wxCommandEvent& event)
{
	if(m_motion_plus->GetValue() && m_nunchuck->GetValue())
		set_active_device_type(kWAPDevMPLSFreeStyle);
	else if(m_motion_plus->GetValue() && m_classic->GetValue())
		set_active_device_type(kWAPDevMPLSClassic);
	else if (m_motion_plus->GetValue())
		set_active_device_type(kWAPDevMPLS);
	else if (m_nunchuck->GetValue())
		set_active_device_type(kWAPDevFreestyle);
	else if (m_classic->GetValue())
		set_active_device_type(kWAPDevClassic);
	else 
		set_active_device_type(kWAPDevCore);
}

void WiimoteInputPanel::on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller)
{
	if (emulated_controller)
	{
		const auto wiimote = std::dynamic_pointer_cast<WiimoteController>(emulated_controller);
		wxASSERT(wiimote);

		wiimote->set_device_type(m_device_type);
	}

	InputPanel::on_timer(emulated_controller, controller);

	if (emulated_controller)
	{
		const auto axis = emulated_controller->get_axis();
		m_draw->SetAxisValue(axis);
	}
}

void WiimoteInputPanel::load_controller(const EmulatedControllerPtr& emulated_controller)
{
	InputPanel::load_controller(emulated_controller);

	if (emulated_controller) {
		const auto wiimote = std::dynamic_pointer_cast<WiimoteController>(emulated_controller);
		wxASSERT(wiimote);
		set_active_device_type(wiimote->get_device_type());
	}
}

void WiimoteInputPanel::on_pair_button(wxCommandEvent& event)
{
    PairingDialog pairing_dialog(this);
    pairing_dialog.ShowModal();
}
