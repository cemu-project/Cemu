#pragma once
#include "gui/components/TextList.h"

class DisasmCtrl : public TextList
{
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
	// code region info
	uint32 currentCodeRegionStart;
	uint32 currentCodeRegionEnd;
	// line to address mapping
	std::vector<std::optional<MPTR>> m_lineToAddress;
};
