#pragma once
#include "wxgui/components/TextList.h"


enum {
	ID_WRITE_U8 = wxID_HIGHEST + 1,
	ID_WRITE_U16,
	ID_WRITE_U32,
	ID_WRITE_FLOAT,
	ID_WRITE_STRING
};

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
	uint32 PositionToAddress(const wxPoint& position, uint32 line);

	void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position) override;
	void OnMouseMove(const wxPoint& position, uint32 line) override;
	void OnMouseDClick(const wxPoint& position, uint32 line) override;
	void OnKeyPressed(sint32 key_code, const wxPoint& position) override;
	void OnContextMenu(const wxPoint& position, uint32 line) override;
	void OnMenuSelected(wxCommandEvent& event);

	template <typename T>
	bool WriteNumericDialog(uint32 address);
	bool WriteString(uint32 address);
private:
	struct  
	{
		uint32 baseAddress;
		uint32 size;
	}m_memoryRegion;
	uint32 m_lastGotoOffset{0};
	uint32 m_writerContextAddress {0};
	uint32 m_writerContextLine {0};
};
