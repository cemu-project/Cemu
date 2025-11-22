#include "wxgui/wxgui.h"
#include "wxHelper.h"
#include "TextureRelationWindow.h"
#include "Cafe/HW/Latte/Core/LatteTexture.h"

enum
{
	// options
	REFRESH_ID,
	CLOSE_ID,
	TEX_LIST_A_ID,
	TEX_LIST_B_ID,
	CHECKBOX_SHOW_ONLY_ACTIVE,
	CHECKBOX_SHOW_VIEWS,
};

wxBEGIN_EVENT_TABLE(TextureRelationViewerWindow, wxFrame)
EVT_BUTTON(CLOSE_ID, TextureRelationViewerWindow::OnCloseButton)
EVT_BUTTON(REFRESH_ID, TextureRelationViewerWindow::OnRefreshButton)
EVT_CHECKBOX(CHECKBOX_SHOW_ONLY_ACTIVE, TextureRelationViewerWindow::OnCheckbox)
EVT_CHECKBOX(CHECKBOX_SHOW_VIEWS, TextureRelationViewerWindow::OnCheckbox)

EVT_CLOSE(TextureRelationViewerWindow::OnClose)
wxEND_EVENT_TABLE()

wxListCtrl* textureRelationListA;
bool isTextureViewerOpen = false;

void openTextureViewer(wxFrame& parentFrame)
{
	if (isTextureViewerOpen)
		return;
	auto frame = new TextureRelationViewerWindow(parentFrame);
	frame->Show(true);
}

TextureRelationViewerWindow::TextureRelationViewerWindow(wxFrame& parent)
	: wxFrame(&parent, wxID_ANY, _("Texture cache"), wxDefaultPosition, wxSize(1000, 480), wxCLOSE_BOX | wxCLIP_CHILDREN | wxCAPTION | wxRESIZE_BORDER)
{
	isTextureViewerOpen = true;

	this->showOnlyActive = false;
	this->showTextureViews = true;

	wxPanel* mainPane = new wxPanel(this);
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	textureRelationListA = new wxListCtrl(mainPane, TEX_LIST_A_ID, wxPoint(0, 0), wxSize(1008, 440), wxLC_REPORT);

	textureRelationListA->SetFont(wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Courier New"));//wxSystemSettings::GetFont(wxSYS_OEM_FIXED_FONT));

	// add columns
	wxListItem colType;
	sint32 columnIndex = 0;
	colType.SetId(columnIndex); columnIndex++;
	colType.SetText("Type");
	colType.SetWidth(85);
	textureRelationListA->InsertColumn(columnIndex-1, colType);
    wxListItem colPhysAddr;
    colPhysAddr.SetId(columnIndex); columnIndex++;
    colPhysAddr.SetText("PhysAddr");
    colPhysAddr.SetWidth(80);
    textureRelationListA->InsertColumn(columnIndex-1, colPhysAddr);
    wxListItem colPhysMipAddr;
    colPhysMipAddr.SetId(columnIndex); columnIndex++;
    colPhysMipAddr.SetText("MipPAddr");
    colPhysMipAddr.SetWidth(80);
    textureRelationListA->InsertColumn(columnIndex-1, colPhysMipAddr);
	wxListItem colDim;
	colDim.SetId(columnIndex); columnIndex++;
	colDim.SetText("Dim");
	colDim.SetWidth(80);
	textureRelationListA->InsertColumn(columnIndex-1, colDim);
	wxListItem colResolution;
	colResolution.SetId(columnIndex); columnIndex++;
	colResolution.SetText("Resolution");
	colResolution.SetWidth(110);
	textureRelationListA->InsertColumn(columnIndex-1, colResolution);
	wxListItem colFormat;
	colFormat.SetId(columnIndex); columnIndex++;
	colFormat.SetText("Format");
	colFormat.SetWidth(70);
	textureRelationListA->InsertColumn(columnIndex-1, colFormat);
	wxListItem colPitch;
	colPitch.SetId(columnIndex); columnIndex++;
	colPitch.SetText("Pitch");
	colPitch.SetWidth(80);
	textureRelationListA->InsertColumn(columnIndex-1, colPitch);
	wxListItem colTilemode;
	colTilemode.SetId(columnIndex); columnIndex++;
	colTilemode.SetText("Tilemode");
	colTilemode.SetWidth(80);
	textureRelationListA->InsertColumn(columnIndex-1, colTilemode);
	wxListItem colSliceRange;
	colSliceRange.SetId(columnIndex); columnIndex++;
	colSliceRange.SetText("SliceRange");
	colSliceRange.SetWidth(90);
	textureRelationListA->InsertColumn(columnIndex-1, colSliceRange);
	wxListItem colMipRange;
	colMipRange.SetId(columnIndex); columnIndex++;
	colMipRange.SetText("MipRange");
	colMipRange.SetWidth(90);
	textureRelationListA->InsertColumn(columnIndex-1, colMipRange);
	wxListItem colAge;
	colAge.SetId(columnIndex); columnIndex++;
	colAge.SetText("Last access");
	colAge.SetWidth(90);
	textureRelationListA->InsertColumn(columnIndex - 1, colAge);
	wxListItem colOverwriteRes;
	colOverwriteRes.SetId(columnIndex); columnIndex++;
	colOverwriteRes.SetText("OverwriteRes");
	colOverwriteRes.SetWidth(110);
	textureRelationListA->InsertColumn(columnIndex - 1, colOverwriteRes);

	wxBoxSizer* sizerBottom = new wxBoxSizer(wxHORIZONTAL);

	sizer->Add(textureRelationListA, 1, wxEXPAND | wxBOTTOM, 0);
	wxButton* button = new wxButton(mainPane, REFRESH_ID, _("Refresh"));
	sizerBottom->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM | wxTOP | wxLEFT, 10);

	wxCheckBox* checkboxShowOnlyActive = new wxCheckBox(mainPane, CHECKBOX_SHOW_ONLY_ACTIVE, _("Show only active"));
	sizerBottom->Add(checkboxShowOnlyActive, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM | wxTOP | wxLEFT, 10);

	wxCheckBox* checkboxShowViews = new wxCheckBox(mainPane, CHECKBOX_SHOW_VIEWS, _("Show views"));
	sizerBottom->Add(checkboxShowViews, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM | wxTOP | wxLEFT, 10);
	checkboxShowViews->SetValue(true);

	textureRelationListA->Bind(wxEVT_RIGHT_DOWN, &TextureRelationViewerWindow::OnTextureListRightClick, this);

	sizer->Add(
		sizerBottom,
		0,                // vertically unstretchable
		wxALIGN_LEFT);
	mainPane->SetSizer(sizer);

	RefreshTextureList();
}

TextureRelationViewerWindow::~TextureRelationViewerWindow()
{
	isTextureViewerOpen = false;
}

void TextureRelationViewerWindow::OnCloseButton(wxCommandEvent& event)
{
	Close();
}

void TextureRelationViewerWindow::OnRefreshButton(wxCommandEvent& event)
{
	RefreshTextureList();
}

void TextureRelationViewerWindow::OnCheckbox(wxCommandEvent& event)
{
	if (event.GetId() == CHECKBOX_SHOW_ONLY_ACTIVE)
	{
		showOnlyActive = event.IsChecked();
		RefreshTextureList();
	}
	else if (event.GetId() == CHECKBOX_SHOW_VIEWS)
	{
		showTextureViews = event.IsChecked();
		RefreshTextureList();
	}
}

void TextureRelationViewerWindow::OnClose(wxCloseEvent& event)
{
	Close();
}

void TextureRelationViewerWindow::_setTextureRelationListItemTexture(wxListCtrl* uiList, sint32 rowIndex, struct LatteTextureInformation* texInfo)
{
	wxString typeLabel;
	// count number of alternative views for base view
	sint32 alternativeViewCount = texInfo->alternativeViewCount;
	if (texInfo->isUpdatedOnGPU)
		typeLabel = "TEXTURE*";
	else
		typeLabel = "TEXTURE";
	if (alternativeViewCount > 0)
	{
		typeLabel += wxString::Format("(%d)", alternativeViewCount + 1);
	}
	wxListItem item;
	item.SetId(rowIndex);
	item.SetText(typeLabel);
	item.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	uiList->InsertItem(item);

	sint32 columnIndex = 1;
    // phys address
    uiList->SetItem(rowIndex, columnIndex, wxString::Format("%08X", texInfo->physAddress));
    columnIndex++;
    // phys mip address
    uiList->SetItem(rowIndex, columnIndex, wxString::Format("%08X", texInfo->physMipAddress));
    columnIndex++;
	// dim
	wxString dimLabel;
	if (texInfo->dim == Latte::E_DIM::DIM_2D)
		dimLabel = "2D";
	else if (texInfo->dim == Latte::E_DIM::DIM_2D_ARRAY)
		dimLabel = "2D_ARRAY";
	else if (texInfo->dim == Latte::E_DIM::DIM_3D)
		dimLabel = "3D";
	else if (texInfo->dim == Latte::E_DIM::DIM_CUBEMAP)
		dimLabel = "CUBEMAP";
	else if (texInfo->dim == Latte::E_DIM::DIM_1D)
		dimLabel = "1D";
	else if (texInfo->dim == Latte::E_DIM::DIM_2D_MSAA)
		dimLabel = "2D_MSAA";
	else if (texInfo->dim == Latte::E_DIM::DIM_2D_ARRAY_MSAA)
		dimLabel = "2D_MS_ARRAY";
	else
		dimLabel = "UKN";
	uiList->SetItem(rowIndex, columnIndex, dimLabel);
	columnIndex++;
	// resolution
	wxString resolutionLabel;
	if (texInfo->depth == 1)
		resolutionLabel = wxString::Format("%dx%d", texInfo->width, texInfo->height);
	else
		resolutionLabel = wxString::Format("%dx%dx%d", texInfo->width, texInfo->height, texInfo->depth);
	uiList->SetItem(rowIndex, columnIndex, resolutionLabel);
	columnIndex++;
	// format
	wxString formatLabel;
	if(texInfo->isDepth)
		formatLabel = wxString::Format("%04x(d)", (uint32)texInfo->format);
	else
		formatLabel = wxString::Format("%04x", (uint32)texInfo->format);
	uiList->SetItem(rowIndex, columnIndex, formatLabel);
	columnIndex++;
	// pitch
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%d", texInfo->pitch));
	columnIndex++;
	// tilemode
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%d", (int)texInfo->tileMode));
	columnIndex++;
	// sliceRange
	uiList->SetItem(rowIndex, columnIndex, "");
	columnIndex++;
	// mipRange
	uiList->SetItem(rowIndex, columnIndex, texInfo->mipLevels == 1 ? "1 mip" : wxString::Format("%d mips", texInfo->mipLevels));
	columnIndex++;
	// last access
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%lus", (GetTickCount() - texInfo->lastAccessTick + 499) / 1000));
	columnIndex++;
	// overwrite resolution
	wxString overwriteResLabel;
	if (texInfo->overwriteInfo.hasResolutionOverwrite)
	{
		if(texInfo->overwriteInfo.depth != 1 || texInfo->depth != 1)
			overwriteResLabel = wxString::Format("%dx%dx%d", texInfo->overwriteInfo.width, texInfo->overwriteInfo.height, texInfo->overwriteInfo.depth);
		else
			overwriteResLabel = wxString::Format("%dx%d", texInfo->overwriteInfo.width, texInfo->overwriteInfo.height);
	}
	uiList->SetItem(rowIndex, columnIndex, overwriteResLabel);
	columnIndex++;
}

void TextureRelationViewerWindow::_setTextureRelationListItemView(wxListCtrl* uiList, sint32 rowIndex, struct LatteTextureInformation* texInfo, struct LatteTextureViewInformation* viewInfo)
{
	// count number of alternative views
	sint32 alternativeViewCount = 0; // todo
	// set type string
	// find and handle highlight entry
	wxListItem item;
	item.SetId(rowIndex);
	item.SetText(alternativeViewCount == 0 ? "> VIEW" : wxString::Format("> VIEW(%d)", alternativeViewCount+1));
	item.SetBackgroundColour(wxHelper::CalculateAccentColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
	uiList->InsertItem(item);
	//uiList->SetItemPtrData(item, (wxUIntPtr)viewInfo);
	sint32 columnIndex = 1;
    // phys address
    uiList->SetItem(rowIndex, columnIndex, "");
    columnIndex++;
    // phys mip address
    uiList->SetItem(rowIndex, columnIndex, "");
    columnIndex++;
	// dim
	wxString dimLabel;
	if (viewInfo->dim == Latte::E_DIM::DIM_2D)
		dimLabel = "2D";
	else if (viewInfo->dim == Latte::E_DIM::DIM_2D_ARRAY)
		dimLabel = "2D_ARRAY";
	else if (viewInfo->dim == Latte::E_DIM::DIM_3D)
		dimLabel = "3D";
	else if (viewInfo->dim == Latte::E_DIM::DIM_CUBEMAP)
		dimLabel = "CUBEMAP";
	else if (viewInfo->dim == Latte::E_DIM::DIM_1D)
		dimLabel = "1D";
	else if (viewInfo->dim == Latte::E_DIM::DIM_2D_MSAA)
		dimLabel = "2D_MSAA";
	else if (viewInfo->dim == Latte::E_DIM::DIM_2D_ARRAY_MSAA)
		dimLabel = "2D_ARRAY_MSAA";
	else
		dimLabel = "UKN";
	uiList->SetItem(rowIndex, columnIndex, dimLabel);
	columnIndex++;
	// resolution
	uiList->SetItem(rowIndex, columnIndex, "");
	columnIndex++;
	// format
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%04x", (uint32)viewInfo->format));
	columnIndex++;
	// pitch
	uiList->SetItem(rowIndex, columnIndex, "");
	columnIndex++;
	// tilemode
	uiList->SetItem(rowIndex, columnIndex, "");
	columnIndex++;
	// sliceRange
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%d-%d", viewInfo->firstSlice, viewInfo->firstSlice+ viewInfo->numSlice-1));
	columnIndex++;
	// mipRange
	uiList->SetItem(rowIndex, columnIndex, wxString::Format("%d-%d", viewInfo->firstMip, viewInfo->firstMip + viewInfo->numMip - 1));
	columnIndex++;
	// last access
	uiList->SetItem(rowIndex, columnIndex, "");
	columnIndex++;
}

void TextureRelationViewerWindow::RefreshTextureList()
{
	int scrollPos = textureRelationListA->GetScrollPos(wxVERTICAL);
	textureRelationListA->DeleteAllItems();

	std::vector<LatteTextureInformation> texCache = LatteTexture_QueryCacheInfo();

	// sort by physAddr in ascending order
	for (sint32 i1 = 0; i1 < texCache.size(); i1++)
	{
		for (sint32 i2 = i1+1; i2 < texCache.size(); i2++)
		{
			if (texCache[i1].physAddress > texCache[i2].physAddress)
			{
				std::swap(texCache[i1], texCache[i2]);
			}
		}
	}

	textureRelationListA->Freeze();

	sint32 rowIndex = 0;
	uint32 currentTick = GetTickCount();
	for (auto& tex : texCache)
	{
		uint32 timeSinceLastAccess = currentTick - tex.lastAccessTick;
		if (showOnlyActive && timeSinceLastAccess > 3000)
			continue; // hide textures which haven't been updated in more than 3 seconds

		_setTextureRelationListItemTexture(textureRelationListA, rowIndex, &tex);
		rowIndex++;
		if (showTextureViews)
		{
			for (auto& view : tex.views)
			{
				_setTextureRelationListItemView(textureRelationListA, rowIndex, &tex, &view);
				rowIndex++;
			}
		}
	}
	textureRelationListA->Thaw();
	long itemCount = textureRelationListA->GetItemCount();
	if (itemCount > 0)
		textureRelationListA->EnsureVisible(std::min<long>(itemCount - 1, scrollPos + textureRelationListA->GetCountPerPage() - 1));
}

void TextureRelationViewerWindow::OnTextureListRightClick(wxMouseEvent& event)
{
}

void TextureRelationViewerWindow::Close()
{
	this->Destroy();
}
