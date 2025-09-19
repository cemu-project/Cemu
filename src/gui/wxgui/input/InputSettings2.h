#pragma once

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/timer.h>

#include "input/api/InputAPI.h"

#include <boost/signals2/connection.hpp>

struct ControllerPage;
class ControllerBase;

class InputSettings2 : public wxDialog
{
public:
	InputSettings2(wxWindow* parent);
	~InputSettings2();

private:
	const wxString kDefaultProfileName = _("<profile name>");

	wxNotebook* m_notebook;
	wxTimer* m_timer;

	wxBitmap m_connected, m_disconnected, m_low_battery;

	wxWindow* initialize_page(size_t index);

	// currently selected controller from active tab
	std::shared_ptr<ControllerBase> get_active_controller() const;

	bool has_settings(InputAPI::Type type);
	ControllerPage& get_current_page_data() const;
	void update_state();

	boost::signals2::connection m_controller_changed;
	void on_controller_changed();

	// events
	void on_notebook_page_changed(wxBookCtrlEvent& event);
	void on_timer(wxTimerEvent& event);

	void on_left_click(wxMouseEvent& event);

	void on_controller_page_changed(wxBookCtrlEvent& event);

	void on_profile_dropdown(wxCommandEvent& event);
	void on_profile_text_changed(wxCommandEvent& event);

	void on_profile_load(wxCommandEvent& event);
	void on_profile_save(wxCommandEvent& event);
	void on_profile_delete(wxCommandEvent& event);

	void on_emulated_controller_selected(wxCommandEvent& event);
	void on_emulated_controller_dropdown(wxCommandEvent& event);

	void on_controller_selected(wxCommandEvent& event);
	void on_controller_dropdown(wxCommandEvent& event);
	void on_controller_connect(wxCommandEvent& event);

	void on_controller_add(wxCommandEvent& event);
	void on_controller_remove(wxCommandEvent& event);

	void on_controller_calibrate(wxCommandEvent& event);
	void on_controller_clear(wxCommandEvent& event);
	void on_controller_settings(wxCommandEvent& event);

	// void on_controller_dropdown(wxCommandEvent& event);
	// void on_controllers_refreshed(wxCommandEvent& event);

};

