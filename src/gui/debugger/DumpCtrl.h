#pragma once
#include "gui/components/TextList.h"


class DumpCtrl : public TextList
{
public:
	DumpCtrl(wxWindow* parent, const wxWindowID& id, const wxPoint& pos, const wxSize& size, long style);

	void Init();
	wxSize DoGetBestSize() const override;

protected:
	void GoToAddressDialog();
	void CenterOffset(uint32 offset);
	uint32 LineToOffset(uint32 line);
	uint32 OffsetToLine(uint32 offset);

	void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position) override;
	void OnMouseMove(const wxPoint& position, uint32 line) override;
	void OnMouseDClick(const wxPoint& position, uint32 line) override;
	void OnKeyPressed(sint32 key_code, const wxPoint& position) override;
private:
	struct  
	{
		uint32 baseAddress;
		uint32 size;
	}m_memoryRegion;
	uint32 m_lastGotoOffset{0};
};
