#pragma once

#include <wx/frame.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/slider.h>

#include "Cafe/GameProfile/GameProfile.h"

class GameProfileWindow : public wxFrame
{
public:
	GameProfileWindow(wxWindow* parent, uint64_t title_id);
	~GameProfileWindow();

private:
	uint64_t m_title_id;
	GameProfile m_game_profile;

	void OnStreamoutSizeChange(wxCommandEvent& event);
	void OnControllerProfileDropdown(wxCommandEvent& event);

	void SetSliderValue(wxSlider* slider, sint32 new_value) const;
	void SetProfileInt(gameProfileIntegerOption_t& option, wxCheckBox* checkbox, sint32 value) const;

	void ApplyProfile();
	void SaveProfile();

	// general
	wxCheckBox* m_load_libs, *m_start_with_padview;

	// cpu
	wxChoice *m_cpu_mode;
	wxChoice* m_thread_quantum;

	// gpu
	//wxCheckBox* m_extended_texture_readback;
	//wxChoice* m_precompiled;
	wxChoice* m_graphic_api;

	wxChoice* m_shader_mul_accuracy;
	//wxChoice* m_cache_accuracy;

	// audio
	//wxCheckBox* m_disable_audio;

	// controller
	wxComboBox* m_controller_profile[8];
};