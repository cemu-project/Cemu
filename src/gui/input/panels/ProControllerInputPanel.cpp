#include "gui/input/panels/ProControllerInputPanel.h"

#include <wx/gbsizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

#include "input/emulated/ProController.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/components/wxInputDraw.h"

const ProController::ButtonId g_kFirstColumnItems[] = { ProController::kButtonId_A, ProController::kButtonId_B, ProController::kButtonId_X, ProController::kButtonId_Y, ProController::kButtonId_L, ProController::kButtonId_R, ProController::kButtonId_ZL, ProController::kButtonId_ZR, ProController::kButtonId_Plus, ProController::kButtonId_Minus };
const ProController::ButtonId g_kSecondColumnItems[] = { ProController::kButtonId_StickL, ProController::kButtonId_StickL_Up, ProController::kButtonId_StickL_Down, ProController::kButtonId_StickL_Left, ProController::kButtonId_StickL_Right };
const ProController::ButtonId g_kThirdColumnItems[] = { ProController::kButtonId_StickR, ProController::kButtonId_StickR_Up, ProController::kButtonId_StickR_Down, ProController::kButtonId_StickR_Left, ProController::kButtonId_StickR_Right };
const ProController::ButtonId g_kFourthRowItems[] = { ProController::kButtonId_Up, ProController::kButtonId_Down, ProController::kButtonId_Left, ProController::kButtonId_Right };


ProControllerInputPanel::ProControllerInputPanel(wxWindow* parent)
	: InputPanel(parent)
{
	auto bold_font = GetFont();
	bold_font.MakeBold();

	auto main_sizer = new wxGridBagSizer();

	sint32 row = 0;
	sint32 column = 0;
	for (auto id : g_kFirstColumnItems)
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

	for (auto id : g_kSecondColumnItems)
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

	for (auto id : g_kThirdColumnItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	row++;

	// input drawer
	m_right_draw = new wxInputDraw(this, wxID_ANY, wxDefaultPosition, { 60, 60 });
	main_sizer->Add(m_right_draw, wxGBPosition(row, column + 1), wxGBSpan(2, 1), wxTOP | wxBOTTOM | wxEXPAND | wxALIGN_CENTER, 5);

	main_sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVERTICAL), wxGBPosition(0, column + 3), wxGBSpan(11, 1), wxALL | wxEXPAND, 5);

	//////////////////////////////////////////////////////////////////

	row = 0;
	column += 4;

	text = new wxStaticText(this, wxID_ANY, _("D-pad"));
	text->SetFont(bold_font);
	main_sizer->Add(text, wxGBPosition(row, column), wxGBSpan(1, 3), wxALL | wxEXPAND, 5);

	for (auto id : g_kFourthRowItems)
	{
		row++;
		add_button_row(main_sizer, row, column, id);
	}

	//////////////////////////////////////////////////////////////////

	SetSizerAndFit(main_sizer);
}

void ProControllerInputPanel::add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const ProController::ButtonId &button_id) {
	sizer->Add(
		new wxStaticText(this, wxID_ANY, wxGetTranslation(to_wxString(ProController::get_button_name(button_id)))),
		wxGBPosition(row, column),
		wxDefaultSpan,
		wxALL | wxALIGN_CENTER_VERTICAL, 5);

	auto text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	text_ctrl->SetClientData((void*)button_id);
	text_ctrl->SetMinSize(wxSize(150, -1));
	text_ctrl->SetEditable(false);
	text_ctrl->SetBackgroundColour(kKeyColourNormalMode);
	bind_hotkey_events(text_ctrl);
	sizer->Add(text_ctrl, wxGBPosition(row, column + 1), wxDefaultSpan, wxALL | wxEXPAND, 5);
}

void ProControllerInputPanel::on_timer(const EmulatedControllerPtr& emulated_controller,
	const ControllerPtr& controller_base)
{
	InputPanel::on_timer(emulated_controller, controller_base);

	if (emulated_controller)
	{
		const auto axis = emulated_controller->get_axis();
		const auto rotation = emulated_controller->get_rotation();

		m_left_draw->SetAxisValue(axis);
		m_right_draw->SetAxisValue(rotation);
	}
}
