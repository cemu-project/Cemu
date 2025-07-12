#pragma once

#include <wx/wx.h>

class TextureRelationViewerWindow : public wxFrame
{
public:
	TextureRelationViewerWindow(wxFrame& parent);
	~TextureRelationViewerWindow();

	void OnCloseButton(wxCommandEvent& event);
	void OnRefreshButton(wxCommandEvent& event);
	void OnCheckbox(wxCommandEvent& event);
	void OnClose(wxCloseEvent& event);
	void RefreshTextureList();
	void OnTextureListRightClick(wxMouseEvent& event);

	void Close();

private:

	wxDECLARE_EVENT_TABLE();

	void _setTextureRelationListItemTexture(wxListCtrl* uiList, sint32 rowIndex, struct LatteTextureInformation* texInfo);
	void _setTextureRelationListItemView(wxListCtrl* uiList, sint32 rowIndex, struct LatteTextureInformation* texInfo, struct LatteTextureViewInformation* viewInfo);

	bool showOnlyActive;
	bool showTextureViews;
};

void openTextureViewer(wxFrame& parentFrame);