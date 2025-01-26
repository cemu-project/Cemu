#pragma once
#include "gui/components/TextList.h"

wxDECLARE_EVENT(wxEVT_DISASMCTRL_NOTIFY_GOTO_ADDRESS, wxCommandEvent); // Notify parent that goto address operation completed. Event contains the address that was jumped to.

class DisasmCtrl : public TextList
{
	enum
	{
		IDContextMenu_ToggleBreakpoint = wxID_HIGHEST + 1,
		IDContextMenu_RestoreOriginalInstructions,
		IDContextMenu_CopyAddress,
		IDContextMenu_CopyUnrelocatedAddress,
		IDContextMenu_Last
	};
public:

	DisasmCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style);

	void Init();
	wxSize DoGetBestSize() const override;

	void OnUpdateView();

	uint32 GetViewBaseAddress() const;
	std::optional<MPTR> LinePixelPosToAddress(sint32 posY);

	uint32 AddressToScrollPos(uint32 offset) const;
	void CenterOffset(uint32 offset);
	void GoToAddressDialog();

	

protected:
	void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position) override;
	void OnMouseMove(const wxPoint& position, uint32 line) override;
	void OnKeyPressed(sint32 key_code, const wxPoint& position) override;
	void OnMouseDClick(const wxPoint& position, uint32 line) override;
	void OnContextMenu(const wxPoint& position, uint32 line) override;
	void OnContextMenuEntryClicked(wxCommandEvent& event);
	bool OnShowTooltip(const wxPoint& position, uint32 line) override;
	void ScrollWindow(int dx, int dy, const wxRect* prect) override;

	void DrawDisassemblyLine(wxDC& dc, const wxPoint& linePosition, MPTR virtualAddress, struct RPLModule* rplModule);
	void DrawLabelName(wxDC& dc, const wxPoint& linePosition, MPTR virtualAddress, struct RPLStoredSymbol* storedSymbol);

	void SelectCodeRegion(uint32 newAddress);

private:
	void CopyToClipboard(std::string text);

	sint32 m_mouse_line, m_mouse_line_drawn;
	sint32 m_active_line;
	uint32 m_lastGotoTarget{};
	uint32 m_contextMenuAddress{};
	// code region info
	uint32 currentCodeRegionStart;
	uint32 currentCodeRegionEnd;
	// line to address mapping
	std::vector<std::optional<MPTR>> m_lineToAddress;
};
