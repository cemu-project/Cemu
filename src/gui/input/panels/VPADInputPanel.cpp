#include "gui/input/panels/VPADInputPanel.h"

#include <wx/gbsizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/slider.h>
#include <wx/checkbox.h>


#include "gui/helpers/wxControlObject.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/components/wxInputDraw.h"
#include "input/emulated/VPADController.h"

constexpr VPADController::ButtonId g_kFirstColumnItems[] =
{
	VPADController::kButtonId_A, VPADController::kButtonId_B, VPADController::kButtonId_X, VPADController::kButtonId_Y,
	VPADController::kButtonId_L, VPADController::kButtonId_R, VPADController::kButtonId_ZL, VPADController::kButtonId_ZR,
	VPADController::kButtonId_Plus, VPADController::kButtonId_Minus
};

constexpr VPADController::ButtonId g_kSecondColumnItems[] =
{
	VPADController::kButtonId_StickL, VPADController::kButtonId_StickL_Up, VPADController::kButtonId_StickL_Down, VPADController::kButtonId_StickL_Left, VPADController::kButtonId_StickL_Right
};

constexpr VPADController::ButtonId g_kThirdColumnItems[] =
{
	VPADController::kButtonId_StickR, VPADController::kButtonId_StickR_Up, VPADController::kButtonId_StickR_Down, VPADController::kButtonId_StickR_Left, VPADController::kButtonId_StickR_Right
};

constexpr VPADController::ButtonId g_kFourthRowItems[] =
{
	VPADController::kButtonId_Up, VPADController::kButtonId_Down, VPADController::kButtonId_Left, VPADController::kButtonId_Right
};


VPADInputPanel::VPADInputPanel(wxWindow* parent)
	: InputPanel(parent)
{
	auto bold_font = GetFont();
	bold_font.MakeBold();

	auto* main_sizer = new wxGridBagSizer();

	sint32 row = 0;
	sint32 column = 0;
	for (const auto& id : g_kFirstColumnItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	//////////////////////////////////////////////////////////////////

	main_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 2), wxGBSpan(11, 1), wxALL | wxEXPAND, 5);

	row = 0;
	column += 3;

	auto text = new wxStaticText(this, wxID_ANY, _("Left Axis"));
	text->SetFont(bold_font);
	main_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (const auto& id : g_kSecondColumnItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	row++;
	
	// input drawer
	m_left_draw = new wxInputDraw(this, wxID_ANY, wxDefaultPosition, { 60, 60 });
	main_sizer->Add(m_left_draw, wxGBPosition(row, column + 1), wxGBSpan(2, 1), wxTOP | wxBOTTOM | wxEXPAND | wxALIGN_CENTER, 5);

	main_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 3), wxGBSpan(11, 1), wxALL | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	row = 0;
	column += 4;

	text = new wxStaticText(this, wxID_ANY, _("Right Axis"));
	text->SetFont(bold_font);
	main_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (const auto& id : g_kThirdColumnItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	row++;

	// input drawer
	m_right_draw = new wxInputDraw(this, wxID_ANY, wxDefaultPosition, { 60, 60 });
	main_sizer->Add(m_right_draw, wxGBPosition(row, column + 1), wxGBSpan(2, 1), wxTOP | wxBOTTOM | wxEXPAND | wxALIGN_CENTER, 5);

	// Volume
	row = 10;

	text = new wxStaticText(this, wxID_ANY, _("Volume"));
	text->Disable();
	main_sizer->Add(text, wxGBPosition(row, column), wxDefaultSpan, wxALL, 5);

	auto*m_volume = new wxSlider(this, wxID_ANY, 0, 0, 100);
	m_volume->Disable();
	main_sizer->Add(m_volume, wxGBPosition(row, column + 1), wxDefaultSpan, wxTOP | wxBOTTOM | wxEXPAND, 5);

	const auto volume_text = new wxStaticText(this, wxID_ANY, wxString::Format("%d%%", 0));
	volume_text->Disable();
	main_sizer->Add(volume_text, wxGBPosition(row, column + 2), wxDefaultSpan, wxALL, 5);
	m_volume->Bind(wxEVT_SLIDER, &VPADInputPanel::OnVolumeChange, this, wxID_ANY, wxID_ANY, new wxControlObject(volume_text));

	main_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 3), wxGBSpan(11, 1), wxALL | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	row = 0;
	column += 4;

	text = new wxStaticText(this, wxID_ANY, _("D-pad"));
	text->SetFont(bold_font);
	main_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (const auto& id : g_kFourthRowItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	// Blow Mic
	row = 8;
	add_button_row(main_sizer, row, column, VPADController::kButtonId_Mic, _("blow mic"));
	row++;

	add_button_row(main_sizer, row, column, VPADController::kButtonId_Screen, _("show screen"));
	row++;

	auto toggleScreenText = new wxStaticText(this, wxID_ANY, _("toggle screen"));
	main_sizer->Add(toggleScreenText,
		wxGBPosition(row, column),
		wxDefaultSpan,
		wxALL | wxALIGN_CENTER_VERTICAL, 5);
	m_togglePadViewCheckBox = new wxCheckBox(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	wxString toggleScreenTT = _("Makes the \"show screen\" button toggle between the TV and gamepad screens");
	m_togglePadViewCheckBox->SetToolTip(toggleScreenTT);
	toggleScreenText->SetToolTip(toggleScreenTT);
	main_sizer->Add(m_togglePadViewCheckBox, wxGBPosition(row,column+1), wxDefaultSpan, wxALL | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	SetSizer(main_sizer);
	Layout();
}

void VPADInputPanel::add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const VPADController::ButtonId &button_id) {
	add_button_row(sizer, row, column, button_id, wxGetTranslation(to_wxString(VPADController::get_button_name(button_id))));
}

void VPADInputPanel::add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column,
                                    const VPADController::ButtonId &button_id, const wxString &label) {
	sizer->Add(
		new wxStaticText(this, wxID_ANY, label),
		wxGBPosition(row, column),
		wxDefaultSpan,
		wxALL | wxALIGN_CENTER_VERTICAL, 5);

	auto* text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	text_ctrl->SetClientData((void*)button_id);
	text_ctrl->SetMinSize(wxSize(150, -1));
	text_ctrl->SetEditable(false);
	text_ctrl->SetBackgroundColour(kKeyColourNormalMode);
	bind_hotkey_events(text_ctrl);
	sizer->Add(text_ctrl, wxGBPosition(row, column + 1), wxDefaultSpan, wxALL | wxEXPAND, 5);
}

void VPADInputPanel::on_timer(const EmulatedControllerPtr& emulated_controller, const ControllerPtr& controller_base)
{
	InputPanel::on_timer(emulated_controller, controller_base);

	static_cast<VPADController*>(emulated_controller.get())->set_screen_toggle(m_togglePadViewCheckBox->GetValue());

	if(emulated_controller)
	{
		const auto axis = emulated_controller->get_axis();
		const auto rotation = emulated_controller->get_rotation();

		m_left_draw->SetAxisValue(axis);
		m_right_draw->SetAxisValue(rotation);
	}
}

void VPADInputPanel::OnVolumeChange(wxCommandEvent& event)
{

}
void VPADInputPanel::load_controller(const EmulatedControllerPtr& controller)
{
	InputPanel::load_controller(controller);

	const bool isToggle = static_cast<VPADController*>(controller.get())->is_screen_active_toggle();
	m_togglePadViewCheckBox->SetValue(isToggle);
}
