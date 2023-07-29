#pragma once

#include <wx/wx.h>

#include <unordered_map>
#include <sstream>

#define COLOR_BLACK				0xFF000000
#define COLOR_GREY				0xFFA0A0A0
#define COLOR_LIGHT_GREY		0xFFE0E0E0
#define COLOR_WHITE				0xFFFFFFFF
#define COLOR_RED				0xFF0000FF


class TextList : public wxControl, public wxScrollHelper
{
public:
	virtual ~TextList();
	TextList(wxWindow* parent, wxWindowID id,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize, long style = 0);

	void RefreshControl(const wxRect* update_region = nullptr);
	void RefreshLine(uint32 line);

	wxSize DoGetVirtualSize() const override;
	void DoSetSize(int x, int y, int width, int height, int sizeFlags) override;
	void SetScrollPos(int orient, int pos, bool refresh) override;
	void DrawLineBackground(wxDC& dc, uint32 line, const wxColour& colour, uint32 lines = 1) const;
	void DrawLineBackground(wxDC& dc, const wxPoint& position, const wxColour& colour, uint32 lines = 1) const;

	bool SetElementCount(size_t element_count);
	uint32 GetElementCount() const;
	bool IsKeyDown(sint32 keycode);
	
protected:
	void WriteText(wxDC& dc, const wxString& text, wxPoint& position) const;
	void WriteText(wxDC& dc, const wxString& text, wxPoint& position, const wxColour& color) const;
	void NextLine(wxPoint& position, const wxPoint* start_position = nullptr) const;

	virtual void OnDraw(wxDC& dc, sint32 start, sint32 count, const wxPoint& start_position) {};
	virtual void OnMouseMove(const wxPoint& position, uint32 line) {}
	virtual void OnKeyPressed(sint32 key_code, const wxPoint& position) {}
	virtual void OnMouseDown();
	virtual void OnMouseUp();
	virtual void OnMouseDClick(const wxPoint& position, uint32 line) {}
	virtual void OnContextMenu(const wxPoint& position, uint32 line) {}
	virtual bool OnShowTooltip(const wxPoint& position, uint32 line);
	void OnEraseBackground(wxEraseEvent&) {}

	sint32 m_line_height;
	sint32 m_char_width;
	size_t m_elements_visible = 0;
	size_t m_element_count = 0;
	bool m_scrolled_to_end = true;

	std::wstringstream m_selected_text;
	bool m_mouse_down = false;
	wxRect m_selection;
	wxPanel* m_tooltip_window;

private:
	void OnPaintEvent(wxPaintEvent& event);
	void OnMouseMoveEvent(wxMouseEvent& event);
	void OnKeyDownEvent(wxKeyEvent& event);
	void OnKeyUpEvent(wxKeyEvent& event);
	void OnMouseDownEvent(wxMouseEvent& event);
	void OnMouseUpEvent(wxMouseEvent& event);
	void OnMouseDClickEvent(wxMouseEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);
	void OnTooltipTimer(wxTimerEvent& event);

	std::unordered_map<sint32, bool> m_key_states;

	wxFont m_font;
	
	wxTimer* m_tooltip_timer;
	wxPoint m_mouse_position;
};
