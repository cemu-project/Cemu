#pragma once

#include <wx/frame.h>
#include <wx/dialog.h>
#include <wx/scrolwin.h>
#include <wx/infobar.h>

#include "wxcomponents/checktree.h"

#include "Cafe/GraphicPack/GraphicPack2.h"

class wxSplitterWindow;
class wxPanel;
class wxButton;
class wxChoice;

class GraphicPacksWindow2 : public wxDialog
{
public:
	GraphicPacksWindow2(wxWindow* parent, uint64_t title_id_filter);
	~GraphicPacksWindow2();

	static void RefreshGraphicPacks();
	void UpdateTitleRunning(bool running);

private:
	std::string m_filter;
	bool m_filter_installed_games;
	std::vector<uint64_t> m_installed_games;

	void ClearPresets();
	void FillGraphicPackList() const;
	void GetChildren(const wxTreeItemId& id, std::vector<wxTreeItemId>& children) const;
	void ExpandChildren(const std::vector<wxTreeItemId>& ids, size_t& counter) const;
	
	wxSplitterWindow * m_splitter_window;

	wxPanel* m_right_panel;
	wxScrolled<wxPanel>* m_gp_options;
	
	wxCheckTree * m_graphic_pack_tree;
	wxTextCtrl* m_filter_text;
	wxCheckBox* m_installed_games_only;

	wxStaticText* m_graphic_pack_name, *m_graphic_pack_description;
	wxBoxSizer* m_preset_sizer;
	std::vector<wxChoice*> m_active_preset;
	wxButton* m_reload_shaders;
	wxButton* m_update_graphicPacks;
	wxInfoBar* m_info_bar;

	GraphicPackPtr m_shown_graphic_pack;
	std::string m_gp_name, m_gp_description;

	float m_ratio = 0.55f;

	wxTreeItemId FindTreeItem(const wxTreeItemId& root, const wxString& text) const;
	void LoadPresetSelections(const GraphicPackPtr& gp);

	void OnTreeSelectionChanged(wxTreeEvent& event);
	void OnTreeChoiceChanged(wxTreeEvent& event);
	void OnActivePresetChanged(wxCommandEvent& event);
	void OnReloadShaders(wxCommandEvent& event);
	void OnCheckForUpdates(wxCommandEvent& event);
	void OnSizeChanged(wxSizeEvent& event);
	void SashPositionChanged(wxEvent& event);
	void OnFilterUpdate(wxEvent& event);
	void OnInstalledGamesChanged(wxCommandEvent& event);

	void SaveStateToConfig();

	void ReloadPack(const GraphicPackPtr& graphic_pack) const;
	void DeleteShadersFromRuntimeCache(const GraphicPackPtr& graphic_pack) const;

};