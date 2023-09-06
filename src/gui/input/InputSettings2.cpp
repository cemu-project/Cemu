#include "gui/input/InputSettings2.h"

#include <wx/gbsizer.h>

#include "input/InputManager.h"
#include "gui/helpers/wxHelpers.h"
#include "gui/helpers/wxControlObject.h"
#include "gui/helpers/wxCustomData.h"

#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/bmpbuttn.h>

#include "config/ActiveSettings.h"
#include "gui/input/InputAPIAddWindow.h"
#include "input/ControllerFactory.h"

#include "gui/input/panels/VPADInputPanel.h"
#include "gui/input/panels/ProControllerInputPanel.h"

#include "gui/input/settings/DefaultControllerSettings.h"
#include "gui/input/panels/ClassicControllerInputPanel.h"
#include "gui/input/panels/WiimoteInputPanel.h"
#include "gui/input/settings/WiimoteControllerSettings.h"
#include "util/EventService.h"

#include "resource/embedded/resources.h"

bool g_inputConfigWindowHasFocus = false;

using wxTypeData = wxCustomData<EmulatedController::Type>;
using wxControllerData = wxCustomData<ControllerPtr>;

struct ControllerPage
{
	EmulatedControllerPtr m_controller;

	// profiles
	wxComboBox* m_profiles;
	wxButton* m_profile_load, * m_profile_save, * m_profile_delete;
	wxStaticText* m_profile_status;

	// emulated controller
	wxComboBox* m_emulated_controller;

	// controller api
	wxComboBox* m_controllers;
	wxButton* m_controller_api_add, *m_controller_api_remove;

	wxButton* m_controller_settings, * m_controller_calibrate, *m_controller_clear;
	wxBitmapButton* m_controller_connected;

	// panel
	std::array<InputPanel*, EmulatedController::Type::MAX> m_panels{};
};
using wxControllerPageData = wxCustomData<ControllerPage>;


InputSettings2::InputSettings2(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Input settings"))
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	g_inputConfigWindowHasFocus = true;

	m_connected = wxBITMAP_PNG_FROM_DATA(INPUT_CONNECTED);
	m_disconnected = wxBITMAP_PNG_FROM_DATA(INPUT_DISCONNECTED);
	m_low_battery = wxBITMAP_PNG_FROM_DATA(INPUT_LOW_BATTERY);

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0);
	for(size_t i = 0; i < InputManager::kMaxController; ++i)
	{
		auto* page = new wxPanel(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
		page->SetClientObject(nullptr); // force internal type to client object
		m_notebook->AddPage(page, formatWxString(_("Controller {}"), i + 1));
	}

	m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &InputSettings2::on_controller_page_changed, this);
	sizer->Add(m_notebook, 1, wxEXPAND);

	m_notebook->SetSelection(0);
	auto* first_page = initialize_page(0);

	// init first/default page for fitting size
	auto* page_data = (wxControllerPageData*)first_page->GetClientObject();
	auto* panel = new VPADInputPanel(first_page);
	page_data->ref().m_panels[EmulatedController::Type::VPAD] = panel;

	auto* first_page_sizer = dynamic_cast<wxGridBagSizer*>(first_page->GetSizer());
	auto* panel_sizer = first_page_sizer->FindItemAtPosition(wxGBPosition(7, 0))->GetSizer();
	panel_sizer->Add(panel, 0, wxEXPAND);

	panel->Show();
	first_page->Layout();

	SetSizer(sizer);
	Layout();
	Fit();

    panel->Hide();

	update_state();

	Bind(wxEVT_TIMER, &InputSettings2::on_timer, this);

	m_timer = new wxTimer(this);
	m_timer->Start(25);

	m_controller_changed = EventService::instance().connect<Events::ControllerChanged>(&InputSettings2::on_controller_changed, this);
}

InputSettings2::~InputSettings2()
{
	m_controller_changed.disconnect();

	g_inputConfigWindowHasFocus = false;
	m_timer->Stop();
	InputManager::instance().save();
}


wxWindow* InputSettings2::initialize_page(size_t index)
{
	auto* page = m_notebook->GetPage(index);
	if (page->GetClientObject()) // already initialized
		return page;

	page->Bind(wxEVT_LEFT_UP, &InputSettings2::on_left_click, this);

	ControllerPage page_data{};
	const auto emulated_controller = InputManager::instance().get_controller(index);
	page_data.m_controller = emulated_controller;

	wxWindowUpdateLocker lock(page);
	auto* sizer = new wxGridBagSizer();

	{
		// profile
		sizer->Add(new wxStaticText(page, wxID_ANY, _("Profile"), wxDefaultPosition, wxDefaultSize, 0), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
		auto* profiles = new wxComboBox(page, wxID_ANY, kDefaultProfileName);
		sizer->Add(profiles, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5);

#if BOOST_OS_LINUX
		// We rely on the wxEVT_COMBOBOX_DROPDOWN event to trigger filling the profile list,
		// but on wxGTK the dropdown button cannot be clicked if the list is empty
		// so as a quick and dirty workaround we fill the list here
		wxCommandEvent tmpCmdEvt;
		tmpCmdEvt.SetEventObject(profiles);
		on_profile_dropdown(tmpCmdEvt);
#endif

		if (emulated_controller && emulated_controller->has_profile_name())
		{
			profiles->SetValue(emulated_controller->get_profile_name());
		}

		auto* load_bttn = new wxButton(page, wxID_ANY, _("Load"));
		load_bttn->Disable();
		sizer->Add(load_bttn, wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* save_bttn = new wxButton(page, wxID_ANY, _("Save"));
		save_bttn->Disable();
		sizer->Add(save_bttn, wxGBPosition(0, 3), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* delete_bttn = new wxButton(page, wxID_ANY, _("Delete"));
		delete_bttn->Disable();
		sizer->Add(delete_bttn, wxGBPosition(0, 4), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* profile_status = new wxStaticText(page, wxID_ANY, _("controller set by gameprofile. changes won't be saved permanently!"), wxDefaultPosition, wxDefaultSize, 0);
		profile_status->SetMinSize(wxSize(200, -1));
		profile_status->Wrap(200);
		sizer->Add(profile_status, wxGBPosition(0, 5), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL | wxRESERVE_SPACE_EVEN_IF_HIDDEN, 5);

		if(InputManager::instance().is_gameprofile_set(index))
		{
			profile_status->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
		}
		else
		{
			profile_status->Hide();
		}

		load_bttn->Bind(wxEVT_BUTTON, &InputSettings2::on_profile_load, this);
		save_bttn->Bind(wxEVT_BUTTON, &InputSettings2::on_profile_save, this);
		delete_bttn->Bind(wxEVT_BUTTON, &InputSettings2::on_profile_delete, this);

		profiles->Bind(wxEVT_COMBOBOX_DROPDOWN, &InputSettings2::on_profile_dropdown, this);
		profiles->Bind(wxEVT_TEXT, &InputSettings2::on_profile_text_changed, this);

		page_data.m_profiles = profiles;
		page_data.m_profile_load = load_bttn;
		page_data.m_profile_save = save_bttn;
		page_data.m_profile_delete = delete_bttn;
		page_data.m_profile_status = profile_status;
	}

	sizer->Add(new wxStaticLine(page), wxGBPosition(1, 0), wxGBSpan(1, 6), wxEXPAND);

	{
		// emulated controller
		sizer->Add(new wxStaticText(page, wxID_ANY, _("Emulated controller")), wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* econtroller_box = new wxComboBox(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
		econtroller_box->SetMinSize(wxSize(200, -1));
		econtroller_box->Bind(wxEVT_COMBOBOX_DROPDOWN, &InputSettings2::on_emulated_controller_dropdown, this);
		econtroller_box->Bind(wxEVT_COMBOBOX, &InputSettings2::on_emulated_controller_selected, this);

		econtroller_box->AppendString(_("Disabled"));
		econtroller_box->SetSelection(0);

		sizer->Add(econtroller_box, wxGBPosition(2, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5);
		page_data.m_emulated_controller = econtroller_box;
	}

	sizer->Add(new wxStaticLine(page), wxGBPosition(3, 0), wxGBSpan(1, 6), wxEXPAND);

	{
		// controller api
		sizer->Add(new wxStaticText(page, wxID_ANY, _("Controller")), wxGBPosition(4, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

		auto* controllers = new wxComboBox(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
		controllers->Bind(wxEVT_COMBOBOX, &InputSettings2::on_controller_selected, this);
		controllers->Bind(wxEVT_COMBOBOX_DROPDOWN, &InputSettings2::on_controller_dropdown, this);
		controllers->SetMinSize(wxSize(300, -1));

		page_data.m_controllers = controllers;

		sizer->Add(controllers, wxGBPosition(4, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 5);

		{
			// add/remove buttons
			auto* bttn_sizer = new wxBoxSizer(wxHORIZONTAL);

			auto* add_api = new wxButton(page, wxID_ANY, wxT(" + "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
			add_api->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_add, this);
			bttn_sizer->Add(add_api, 0, wxALL, 5);

			auto* remove_api = new wxButton(page, wxID_ANY, wxT("  -  "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
			remove_api->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_remove, this);
			bttn_sizer->Add(remove_api, 0, wxALL, 5);

			sizer->Add(bttn_sizer, wxGBPosition(4, 2), wxDefaultSpan, wxEXPAND, 5);

			page_data.m_controller_api_add = add_api;
			page_data.m_controller_api_remove = remove_api;
		}

		// controller
		auto* controller_bttns = new wxBoxSizer(wxHORIZONTAL);
		auto* settings = new wxButton(page, wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize, 0);
		settings->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_settings, this);
		settings->Disable();
		controller_bttns->Add(settings, 0, wxALL, 5);

		auto* calibrate = new wxButton(page, wxID_ANY, _("Calibrate"), wxDefaultPosition, wxDefaultSize, 0);
		calibrate->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_calibrate, this);
		calibrate->Disable();
		controller_bttns->Add(calibrate, 0, wxALL, 5);

		auto* clear = new wxButton(page, wxID_ANY, _("Clear"), wxDefaultPosition, wxDefaultSize, 0);
		clear->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_clear, this);
		controller_bttns->Add(clear, 0, wxALL, 5);

		sizer->Add(controller_bttns, wxGBPosition(5, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

		auto* connected_button = new wxBitmapButton(page, wxID_ANY, m_disconnected);
		connected_button->Bind(wxEVT_BUTTON, &InputSettings2::on_controller_connect, this);
		connected_button->SetToolTip(_("Test if the controller is connected"));
		sizer->Add(connected_button, wxGBPosition(5, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5); // TODO replace with icon

		page_data.m_controller_settings = settings;
		page_data.m_controller_calibrate = calibrate;
		page_data.m_controller_clear = clear;
		page_data.m_controller_connected = connected_button;

	}

	sizer->Add(new wxStaticLine(page), wxGBPosition(6, 0), wxGBSpan(1, 6), wxEXPAND);

	auto* panel_sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(panel_sizer, wxGBPosition(7, 0), wxGBSpan(1, 6), wxEXPAND | wxALL, 5);

	page->SetSizer(sizer);
	page->Layout();

	page->SetClientObject(new wxCustomData(page_data));

	return page;
}

std::pair<size_t, size_t> InputSettings2::get_emulated_controller_types() const
{
	size_t vpad = 0, wpad = 0;
	for(size_t i = 0; i < m_notebook->GetPageCount(); ++i)
	{
		auto* page = m_notebook->GetPage(i);
		auto* page_data = (wxControllerPageData*)page->GetClientObject();
		if (!page_data)
			continue;
		
		if (!page_data->ref().m_controller) // = disabled
			continue;

		const auto api_type = page_data->ref().m_controller->type();
		if (api_type) 
			continue;

		if (api_type == EmulatedController::VPAD)
			++vpad;
		else
			++wpad;
	}

	return std::make_pair(vpad, wpad);
}

std::shared_ptr<ControllerBase> InputSettings2::get_active_controller() const
{
	auto& page_data = get_current_page_data();

	const auto selection = page_data.m_controllers->GetSelection();
	if(selection != wxNOT_FOUND)
	{
		if(auto* controller = (wxControllerData*)page_data.m_controllers->GetClientObject(selection))
			return controller->ref();
	}

	return {};
}

bool InputSettings2::has_settings(InputAPI::Type type)
{
	switch(type)
	{
	case InputAPI::Keyboard:
		return false;
	default:
		return true;
	}
}

void InputSettings2::update_state()
{
	auto* page = m_notebook->GetCurrentPage();
	wxWindowUpdateLocker lock(page);

	auto* page_data_ptr = (wxControllerPageData*)page->GetClientObject();
	wxASSERT(page_data_ptr);
	auto& page_data = page_data_ptr->ref();

	page_data.m_profile_status->Hide();

	EmulatedControllerPtr emulated_controller = page_data.m_controller;
	auto has_controllers = false;

	// update emulated
	if(emulated_controller)
	{
		has_controllers = !emulated_controller->get_controllers().empty();

		const auto emulated_type = emulated_controller->type();
		int index = page_data.m_emulated_controller->Append(to_wxString(emulated_controller->type_to_string(emulated_type)));
		page_data.m_emulated_controller->SetSelection(index);

		const auto controller_selection = page_data.m_controllers->GetStringSelection();
		page_data.m_controllers->Clear();
		if (has_controllers)
		{
			for (const auto& c : emulated_controller->get_controllers())
			{
				page_data.m_controllers->Append(fmt::format("{} [{}]", c->display_name(), c->api_name()), new wxCustomData(c));
			}

			if (page_data.m_controllers->GetCount() > 0)
			{
				page_data.m_controllers->SetSelection(0);
				if (!controller_selection.empty())
					page_data.m_controllers->SetStringSelection(controller_selection);
			}
		}
	}
	else
	{
		page_data.m_emulated_controller->SetValue(_("Disabled"));
	}

	ControllerPtr controller;
	if (page_data.m_controllers->GetSelection() != wxNOT_FOUND)
	{
		if (const auto data = (wxControllerData*)page_data.m_controllers->GetClientObject(page_data.m_controllers->GetSelection()))
			controller = data->ref();
	}

	if (controller && controller->is_connected())
		page_data.m_controller_connected->SetBitmap(m_connected);
	else
		page_data.m_controller_connected->SetBitmap(m_disconnected);

	// update controller
	page_data.m_controller_calibrate->Enable(has_controllers);
	page_data.m_controller_api_remove->Enable(has_controllers);
	page_data.m_controller_settings->Enable(controller && has_settings(controller->api()));

	// update settings
	// update panel
	// test if we need to update to correct panel
	std::optional<EmulatedController::Type> active_api{};
	for(auto i = 0; i < EmulatedController::Type::MAX; ++i)
	{
		if(page_data.m_panels[i] && page_data.m_panels[i]->IsShown())
		{
			active_api = (EmulatedController::Type)i;
			break;
		}
	}

	// disabled and no emulated controller
	if (!active_api && !emulated_controller)
		return;

	// enabled correct panel for active controller
	if (active_api && emulated_controller && emulated_controller->type() == active_api.value())
	{
		// same controller type panel already shown, refresh content of panels
		for (auto* panel : page_data.m_panels)
		{
			if (panel)
				panel->load_controller(page_data.m_controller);
		}
		return;
	}

	// hide all panels
	for (auto* panel : page_data.m_panels)
	{
		if (panel)
			panel->Hide();
	}

	// show required panel
	if (emulated_controller)
	{
		auto* sizer = dynamic_cast<wxGridBagSizer*>(page->GetSizer());
		wxASSERT(sizer);

		const auto type = page_data.m_controller->type();
		InputPanel* panel = page_data.m_panels[type];
		if (!panel)
		{
			switch (type)
			{
			case EmulatedController::Type::VPAD:
				panel = new VPADInputPanel(page);
				break;
			case EmulatedController::Pro:
				panel = new ProControllerInputPanel(page);
				break;
			case EmulatedController::Classic:
				panel = new ClassicControllerInputPanel(page);
				break;
			case EmulatedController::Wiimote:
				panel = new WiimoteInputPanel(page);
				break;
			default:
				cemu_assert_debug(false);
				return;
			}

			page_data.m_panels[type] = panel;

			auto* panel_sizer = sizer->FindItemAtPosition(wxGBPosition(7, 0))->GetSizer();
			wxASSERT(panel_sizer);
			panel_sizer->Add(panel, 0, wxEXPAND);
		}

		panel->load_controller(page_data.m_controller);
		if (has_controllers)
			panel->set_selected_controller(emulated_controller, emulated_controller->get_controllers()[0]);

		panel->Show();
		page->wxWindowBase::Layout();
		page->wxWindow::Update();
	}
}

void InputSettings2::on_controller_changed()
{
	for(auto i = 0 ; i < m_notebook->GetPageCount(); ++i)
	{
		auto* page = m_notebook->GetPage(i);
		if (!page)
			continue;

		auto* page_data_ptr = (wxControllerPageData*)page->GetClientObject();
		if (!page_data_ptr)
			continue;

		const auto& page_data = page_data_ptr->ref();
		if (page_data.m_controllers->GetSelection() != wxNOT_FOUND)
		{
			if (const auto data = (wxControllerData*)page_data.m_controllers->GetClientObject(page_data.m_controllers->GetSelection()))
			{
				if (const auto controller = data->ref())
				{
					if (controller->connect())
						page_data.m_controller_connected->SetBitmap(m_connected);
					else
						page_data.m_controller_connected->SetBitmap(m_disconnected);
				}
			}
		}
	}
}

void InputSettings2::on_notebook_page_changed(wxBookCtrlEvent& event)
{
	initialize_page(event.GetSelection());
	update_state();
	event.Skip();
}

void InputSettings2::on_timer(wxTimerEvent&)
{
	auto& page_data = get_current_page_data();
	if (!page_data.m_controller) {
		return;
	}

	auto* panel = page_data.m_panels[page_data.m_controller->type()];
	if (!panel)
		return;

	auto controller = get_active_controller();
	if (controller) {
		panel->on_timer(page_data.m_controller, controller);
	}
}

void InputSettings2::on_left_click(wxMouseEvent& event)
{
	event.Skip();

	auto& page_data = get_current_page_data();
	if (!page_data.m_controller) {
		return;
	}

	auto* panel = page_data.m_panels[page_data.m_controller->type()];
	if (!panel)
		return;

	panel->on_left_click(event);
}

void InputSettings2::on_profile_dropdown(wxCommandEvent& event)
{
	auto* profile_names = dynamic_cast<wxComboBox*>(event.GetEventObject());
	wxASSERT(profile_names);
	wxWindowUpdateLocker lock(profile_names);
	
	const auto selected_value = profile_names->GetStringSelection();
	profile_names->Clear();

	for(const auto& profile : InputManager::get_profiles())
	{
		profile_names->Append(wxString::FromUTF8(profile));
	}

	profile_names->SetStringSelection(selected_value);
}

void InputSettings2::on_profile_text_changed(wxCommandEvent& event)
{
	auto* profile_names = dynamic_cast<wxComboBox*>(event.GetEventObject());
	wxASSERT(profile_names);

	auto& page_data = get_current_page_data();

	// load_bttn, save_bttn, delete_bttn, profile_status
	const auto text = event.GetString();
	const bool valid_name = InputManager::is_valid_profilename(text.utf8_string());
	const bool name_exists = profile_names->FindString(text) != wxNOT_FOUND;

	page_data.m_profile_load->Enable(name_exists);
	page_data.m_profile_save->Enable(valid_name);
	page_data.m_profile_delete->Enable(name_exists);
	page_data.m_profile_status->Hide();
}

void InputSettings2::on_profile_load(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	auto* profile_names = page_data.m_profiles;
	auto* text = page_data.m_profile_status;

	const auto selection = profile_names->GetValue().utf8_string();
	text->Show();
	if (selection.empty() || !InputManager::is_valid_profilename(selection))
	{
		text->SetLabelText(_("invalid profile name"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
		text->Refresh();
		return;
	}

	const auto page_index = m_notebook->GetSelection();
	if (InputManager::instance().load(page_index, selection))
	{
		text->SetLabelText(_("profile loaded"));
		text->SetForegroundColour(wxTheColourDatabase->Find("SUCCESS"));
	}
	else
	{
		text->SetLabelText(_("couldn't load profile"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
	}

	text->Refresh();

	// update controller info
	page_data.m_controller = InputManager::instance().get_controller(page_index);
	update_state();
}

void InputSettings2::on_profile_save(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	auto* profile_names = page_data.m_profiles;
	auto* text = page_data.m_profile_status;

	const auto selection = profile_names->GetValue().utf8_string();
	text->Show();
	if (selection.empty() || !InputManager::is_valid_profilename(selection))
	{
		text->SetLabelText(_("invalid profile name"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
		text->Refresh();
		return;
	}

	if(InputManager::instance().save(m_notebook->GetSelection(), selection))
	{
		text->SetLabelText(_("profile saved"));
		text->SetForegroundColour(wxTheColourDatabase->Find("SUCCESS"));
	}
	else
	{
		text->SetLabelText(_("couldn't save profile"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
	}

	text->Refresh();
}

void InputSettings2::on_profile_delete(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	auto* profile_names = page_data.m_profiles;
	auto* text = page_data.m_profile_status;

	const auto selection = profile_names->GetStringSelection().utf8_string();

	text->Show();
	if (selection.empty() || !InputManager::is_valid_profilename(selection))
	{
		text->SetLabelText(_("invalid profile name"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
		text->Refresh();
		return;
	}
	try
	{
		const fs::path old_path = ActiveSettings::GetConfigPath("controllerProfiles/{}.txt", selection);
		fs::remove(old_path);

		const fs::path path = ActiveSettings::GetConfigPath("controllerProfiles/{}.xml", selection);
		fs::remove(path);

		profile_names->ChangeValue(kDefaultProfileName);
		text->SetLabelText(_("profile deleted"));
		text->SetForegroundColour(wxTheColourDatabase->Find("SUCCESS"));

		page_data.m_profile_load->Disable();
		page_data.m_profile_save->Disable();
		page_data.m_profile_delete->Disable();
	}
	catch (const std::exception&)
	{
		text->SetLabelText(_("can't delete profile"));
		text->SetForegroundColour(wxTheColourDatabase->Find("ERROR"));
	}
	text->Refresh();
}


void InputSettings2::on_controller_page_changed(wxBookCtrlEvent& event)
{
	initialize_page(event.GetSelection());
	update_state();
	event.Skip();
}

void InputSettings2::on_emulated_controller_selected(wxCommandEvent& event)
{
	const auto page_index = m_notebook->GetSelection();
	auto& page_data = get_current_page_data();

	const auto selection = event.GetSelection();
	if(selection == 0) // disabled selected
	{
		page_data.m_controller = {};
		InputManager::instance().delete_controller(page_index, true);
	}
	else
	{
		try
		{
			const auto type = EmulatedController::type_from_string(event.GetString().utf8_string());
			// same has already been selected
			if (page_data.m_controller && page_data.m_controller->type() == type)
				return;

			// set new controller
			const auto new_controller = InputManager::instance().set_controller(page_index, type);
			page_data.m_controller = new_controller;

			// append controllers if some were already added before
			if (new_controller->get_controllers().empty())
			{
				// test if we had no emulated controller before but still assigned controllers we want to transfer now
				for (uint32 i = 0; i < page_data.m_controllers->GetCount(); ++i)
				{
					if (auto* controller = (wxControllerData*)page_data.m_controllers->GetClientObject(i))
					{
						new_controller->add_controller(controller->ref());
					}
				}
			}

			// set default mappings if any controllers available
			for(const auto& c: new_controller->get_controllers())
			{
				new_controller->set_default_mapping(c);
			}
		}
		catch (const std::exception&)
		{
			cemu_assert_debug(false);
		}
		
	}

	update_state();
}

void InputSettings2::on_emulated_controller_dropdown(wxCommandEvent& event)
{
	auto* emulated_controllers = dynamic_cast<wxComboBox*>(event.GetEventObject());
	wxASSERT(emulated_controllers);

	wxWindowUpdateLocker lock(emulated_controllers);

	bool is_gamepad_selected = false;
	const auto selected = emulated_controllers->GetSelection();
	const auto selected_value = emulated_controllers->GetStringSelection();
	if(selected != wxNOT_FOUND)
	{
		is_gamepad_selected = selected_value == to_wxString(EmulatedController::type_to_string(EmulatedController::Type::VPAD));
	}

	const auto [vpad_count, wpad_count] = get_emulated_controller_types();

	emulated_controllers->Clear();
	emulated_controllers->AppendString(_("Disabled"));

	if (vpad_count < InputManager::kMaxVPADControllers || is_gamepad_selected)
		emulated_controllers->Append(to_wxString(EmulatedController::type_to_string(EmulatedController::Type::VPAD)));

	if (wpad_count < InputManager::kMaxWPADControllers || !is_gamepad_selected)
	{
		emulated_controllers->AppendString(to_wxString(EmulatedController::type_to_string(EmulatedController::Type::Pro)));
		emulated_controllers->AppendString(to_wxString(EmulatedController::type_to_string(EmulatedController::Type::Classic)));
		emulated_controllers->AppendString(to_wxString(EmulatedController::type_to_string(EmulatedController::Type::Wiimote)));
	}

	emulated_controllers->SetStringSelection(selected_value);
}

void InputSettings2::on_controller_selected(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	const auto enabled = event.GetSelection() != wxNOT_FOUND;
	page_data.m_controller_api_remove->Enable(enabled);
	// page_data->ref().m_controller_list->Clear();
	if(enabled)
	{
		// get selected controller if any todo
		if (auto* controller = (wxControllerData*)page_data.m_controllers->GetClientObject(event.GetSelection()))
		{
			page_data.m_controller_settings->Enable(has_settings(controller->ref()->api()));

			if(page_data.m_controller)
			{
				page_data.m_panels[page_data.m_controller->type()]->set_selected_controller(page_data.m_controller, controller->ref());
			}
		}
		
	}
}

void InputSettings2::on_controller_dropdown(wxCommandEvent& event)
{
	if(auto* controllers = dynamic_cast<wxComboBox*>(event.GetEventObject()))
	{
		if(controllers->GetCount()== 0)
		{
			on_controller_add(event);
			controllers->SetSelection(0);
		}
	}
}

ControllerPage& InputSettings2::get_current_page_data() const
{
	auto* page = m_notebook->GetCurrentPage();
	auto* page_data_ptr = (wxControllerPageData*)page->GetClientObject();
	wxASSERT(page_data_ptr);
	return page_data_ptr->ref();
}

void InputSettings2::on_controller_connect(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	if (page_data.m_controllers->GetSelection() != wxNOT_FOUND)
	{
		if (const auto data = (wxControllerData*)page_data.m_controllers->GetClientObject(page_data.m_controllers->GetSelection()))
		{
			if(const auto controller = data->ref())
			{
				if(controller->connect())
					page_data.m_controller_connected->SetBitmap(m_connected);
				else
					page_data.m_controller_connected->SetBitmap(m_disconnected);
			}
		}
	}
}

void InputSettings2::on_controller_add(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	std::vector<ControllerPtr> controllers;
	controllers.reserve(page_data.m_controllers->GetCount());
	for(uint32 i = 0; i < page_data.m_controllers->GetCount(); ++i)
	{
		if (auto* controller = (wxControllerData*)page_data.m_controllers->GetClientObject(i))
			controllers.emplace_back(controller->ref());
	}

	InputAPIAddWindow wnd(this, wxGetMousePosition() + wxSize(5, 5), controllers);
	if (wnd.ShowModal() != wxID_OK)
		return;

	wxASSERT(wnd.is_valid());

	const auto controller = wnd.get_controller();

	const auto api_type = wnd.get_type();
	controller->connect();
	const int index = page_data.m_controllers->Append(fmt::format("{} [{}]", controller->display_name(), to_string(api_type)), new wxCustomData(controller));

	page_data.m_controllers->Select(index);
	
	if(page_data.m_controller)
	{
		page_data.m_controller->add_controller(controller);

		const auto type = page_data.m_controller->type();
		// if first controller and we got no mappings, add default mappings
		if(page_data.m_controller->set_default_mapping(controller))
			page_data.m_panels[type]->load_controller(page_data.m_controller);
		
		page_data.m_panels[type]->set_selected_controller(page_data.m_controller, controller);
	}

	update_state();
}

void InputSettings2::on_controller_remove(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();

	auto* api_box = page_data.m_controllers;
	int selection = api_box->GetSelection();
	if (selection == wxNOT_FOUND)
		return;

	if (page_data.m_controller) {
		if (auto* controller = (wxControllerData*)page_data.m_controllers->GetClientObject(selection))
		{
			page_data.m_controller->remove_controller(controller->ref());
			page_data.m_panels[page_data.m_controller->type()]->load_controller(page_data.m_controller);
		}
	}

	page_data.m_panels[page_data.m_controller->type()]->reset_colours();

	api_box->Delete(selection);
	api_box->Refresh();

	update_state();

	if (api_box->GetCount() > 0)
	{
		selection = selection > 0 ? (selection - 1) : 0;
		api_box->SetSelection(selection);
	}

	update_state();
}

void InputSettings2::on_controller_calibrate(wxCommandEvent& event)
{
	if(const auto controller = get_active_controller())
		controller->calibrate();
}

void InputSettings2::on_controller_clear(wxCommandEvent& event)
{
	auto& page_data = get_current_page_data();
	if (page_data.m_controller) {
		const auto type = page_data.m_controller->type();

		page_data.m_panels[type]->reset_configuration();
		page_data.m_controller->clear_mappings();
	}
}

void InputSettings2::on_controller_settings(wxCommandEvent& event)
{
	auto controller = get_active_controller();
	if (!controller)
		return;

	switch(controller->api())
	{
	
	case InputAPI::DirectInput:
	case InputAPI::XInput:
	case InputAPI::GameCube:
	case InputAPI::WGIGamepad:
	case InputAPI::WGIRawController:
	case InputAPI::SDLController:
	case InputAPI::DSUClient:
	{
		DefaultControllerSettings wnd(this, wxGetMousePosition() + wxSize(5, 5), controller);
		wnd.ShowModal();
		break;
	}

	case InputAPI::Keyboard: break;

	#ifdef SUPPORTS_WIIMOTE
	case InputAPI::Wiimote: {
		const auto wiimote = std::dynamic_pointer_cast<NativeWiimoteController>(controller);
		wxASSERT(wiimote);
		WiimoteControllerSettings wnd(this, wxGetMousePosition() + wxSize(5, 5), wiimote);
		wnd.ShowModal();
		break;
	}
	#endif
	}
}
