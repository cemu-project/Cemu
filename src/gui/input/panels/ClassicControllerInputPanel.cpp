#include "gui/input/panels/ClassicControllerInputPanel.h"

#include <wx/gbsizer.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textctrl.h>
#include <wx/slider.h>

#include "gui/helpers/wxControlObject.h"
#include "input/emulated/ClassicController.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/components/wxInputDraw.h"

constexpr ClassicController::ButtonId g_kFirstColumnItems[] = { ClassicController::kButtonId_A, ClassicController::kButtonId_B, ClassicController::kButtonId_X, ClassicController::kButtonId_Y, ClassicController::kButtonId_L, ClassicController::kButtonId_R, ClassicController::kButtonId_ZL, ClassicController::kButtonId_ZR, ClassicController::kButtonId_Plus, ClassicController::kButtonId_Minus };
constexpr ClassicController::ButtonId g_kSecondColumnItems[] = { ClassicController::kButtonId_StickL_Up, ClassicController::kButtonId_StickL_Down, ClassicController::kButtonId_StickL_Left, ClassicController::kButtonId_StickL_Right };
constexpr ClassicController::ButtonId g_kThirdColumnItems[] = { ClassicController::kButtonId_StickR_Up, ClassicController::kButtonId_StickR_Down, ClassicController::kButtonId_StickR_Left, ClassicController::kButtonId_StickR_Right };
constexpr ClassicController::ButtonId g_kFourthRowItems[] = { ClassicController::kButtonId_Up, ClassicController::kButtonId_Down, ClassicController::kButtonId_Left, ClassicController::kButtonId_Right };


ClassicControllerInputPanel::ClassicControllerInputPanel(wxWindow* parent)
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

	SetSizer(main_sizer);
	Layout();
}

void ClassicControllerInputPanel::add_button_row(wxGridBagSizer *sizer, sint32 row, sint32 column, const ClassicController::ButtonId &button_id) {
	sizer->Add(
		new wxStaticText(this, wxID_ANY, wxGetTranslation(to_wxString(ClassicController::get_button_name(button_id)))),
		wxGBPosition(row, column),
		wxDefaultSpan,
		wxALL | wxALIGN_CENTER_VERTICAL, 5);

	auto* text_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	text_ctrl->SetClientData(reinterpret_cast<void*>(button_id));
	text_ctrl->SetMinSize(wxSize(150, -1));
	text_ctrl->SetEditable(false);
	text_ctrl->SetBackgroundColour(kKeyColourNormalMode);
	bind_hotkey_events(text_ctrl);
	sizer->Add(text_ctrl, wxGBPosition(row, column + 1), wxDefaultSpan, wxALL | wxEXPAND, 5);
}
