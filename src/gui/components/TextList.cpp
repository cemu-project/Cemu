#include "gui/wxgui.h"
#include "TextList.h"
#include <wx/setup.h>
#include <wx/tooltip.h>

TextList::~TextList()
{
	m_tooltip_timer->Stop();

	this->Unbind(wxEVT_MOTION, &TextList::OnMouseMoveEvent, this);
	this->Unbind(wxEVT_KEY_DOWN, &TextList::OnKeyDownEvent, this);
	this->Unbind(wxEVT_KEY_UP, &TextList::OnKeyUpEvent, this);
	this->Unbind(wxEVT_PAINT, &TextList::OnPaintEvent, this);
	this->Unbind(wxEVT_LEFT_DOWN, &TextList::OnMouseDownEvent, this);
	this->Unbind(wxEVT_LEFT_UP, &TextList::OnMouseUpEvent, this);
	this->Unbind(wxEVT_LEFT_DCLICK, &TextList::OnMouseDClickEvent, this);
	this->Unbind(wxEVT_CONTEXT_MENU, &TextList::OnContextMenu, this);
	this->Unbind(wxEVT_ERASE_BACKGROUND, &TextList::OnEraseBackground, this);
}

TextList::TextList(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxControl(parent, id, pos, size, style), wxScrollHelper(this)
{
	m_font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
	wxWindowBase::SetBackgroundStyle(wxBG_STYLE_PAINT);

	wxClientDC dc(this);
	m_line_height = dc.GetCharHeight();
	m_char_width = dc.GetCharWidth();

	m_yScrollPixelsPerLine = m_line_height;

	this->ShowScrollbars(wxSHOW_SB_DEFAULT, wxSHOW_SB_DEFAULT);

	m_tooltip = new wxToolTip(wxEmptyString);

	this->Bind(wxEVT_MOTION, &TextList::OnMouseMoveEvent, this);
	this->Bind(wxEVT_KEY_DOWN, &TextList::OnKeyDownEvent, this);
	this->Bind(wxEVT_KEY_UP, &TextList::OnKeyUpEvent, this);
	this->Bind(wxEVT_PAINT, &TextList::OnPaintEvent, this);
	this->Bind(wxEVT_LEFT_DOWN, &TextList::OnMouseDownEvent, this);
	this->Bind(wxEVT_LEFT_UP, &TextList::OnMouseUpEvent, this);
	this->Bind(wxEVT_LEFT_DCLICK, &TextList::OnMouseDClickEvent, this);
	this->Bind(wxEVT_CONTEXT_MENU, &TextList::OnContextMenu, this);
	this->Bind(wxEVT_ERASE_BACKGROUND, &TextList::OnEraseBackground, this);

	m_tooltip_window = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
	m_tooltip_window->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
	m_tooltip_window->Hide();

	m_tooltip_timer = new wxTimer(this);
	this->Bind(wxEVT_TIMER, &TextList::OnTooltipTimer, this);
}

void TextList::RefreshControl(const wxRect* update_region)
{
	if(update_region)
		Refresh(true, update_region);
	else
	{
		wxRect region = GetClientRect();
		update_region = &region;
		Refresh(true, update_region);
	}
}

void TextList::RefreshLine(uint32 line)
{
	wxRect update_region = GetClientRect();
	update_region.y = (sint32)line * m_line_height;
	update_region.height = m_line_height;
	CalcScrolledPosition(0, update_region.y, nullptr, &update_region.y);
	// debug_printf("update: <%x, %x>\n", update_region.y, update_region.height);
	Refresh(true, &update_region);
}

wxSize TextList::DoGetVirtualSize() const
{
	return {wxDefaultCoord, (int)(m_element_count + 1) * m_line_height};
}

void TextList::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
	wxControl::DoSetSize(x, y, width, height, sizeFlags);

	m_elements_visible = (height + m_line_height - 1) / m_line_height;
	Refresh();
}

void TextList::SetScrollPos(int orient, int pos, bool refresh)
{
	wxControl::SetScrollPos(orient, pos, refresh);
}

void TextList::DrawLineBackground(wxDC& dc, uint32 line, const wxColour& colour, uint32 lines) const
{
	wxRect rect;
	rect.x = GetPosition().x;
	rect.y = line * m_line_height;
	rect.width = GetSize().x;
	rect.height = m_line_height * lines;

	dc.SetBrush(colour);
	dc.DrawRectangle(rect);
}

void TextList::DrawLineBackground(wxDC& dc, const wxPoint& position, const wxColour& colour, uint32 lines) const
{
	wxRect rect;
	rect.x = position.x;
	rect.y = position.y;
	rect.width = this->GetSize().x;
	rect.height = m_line_height * lines;

	dc.SetBrush(colour);
	dc.DrawRectangle(rect);
}

bool TextList::SetElementCount(size_t element_count)
{
	if (m_element_count == element_count)
		return false;

	if (element_count > 0x7FFFFFFF)
		element_count = 0x7FFFFFFF;

	m_element_count = element_count;
	this->AdjustScrollbars();
	return true;
}

uint32 TextList::GetElementCount() const
{
	return m_element_count;
}

bool TextList::IsKeyDown(sint32 keycode)
{
	return m_key_states[keycode];
}

void TextList::WriteText(wxDC& dc, const wxString& text, wxPoint& position) const
{
	dc.ResetBoundingBox();
	dc.DrawText(text, position);
	position.x += dc.MaxX() - dc.MinX();
}

void TextList::WriteText(wxDC& dc, const wxString& text, wxPoint& position, const wxColour& color) const
{
	dc.SetTextForeground(color);
	WriteText(dc, text, position);
}

void TextList::NextLine(wxPoint& position, const wxPoint* start_position) const
{
	position.y += m_line_height;

	if (start_position)
		position.x = start_position->x;
}

void TextList::OnMouseDown() {}
void TextList::OnMouseUp() {}

bool TextList::OnShowTooltip(const wxPoint& position, uint32 line)
{
	return false;
}

void TextList::OnMouseMoveEvent(wxMouseEvent& event)
{
	m_tooltip_timer->Stop();
	m_tooltip_timer->StartOnce(250);
	
	wxPoint position = event.GetPosition();
	CalcUnscrolledPosition(position.x, position.y, &position.x, &position.y);

	m_mouse_position = position;

	if(m_mouse_down)
		m_selection.SetBottomRight(position);

	const sint32 line = position.y / m_line_height;
	OnMouseMove(position, line);
}

void TextList::OnKeyDownEvent(wxKeyEvent& event)
{
	const auto key_code = event.GetKeyCode();
	const auto it = m_key_states.find(key_code);
	if(it == m_key_states.end() || !it->second)
	{
		m_key_states[key_code] = true;
		OnKeyPressed(key_code, event.GetPosition());
	}

	event.Skip();
}

void TextList::OnKeyUpEvent(wxKeyEvent& event)
{
	m_key_states[event.GetKeyCode()] = false;
}

void TextList::OnMouseDownEvent(wxMouseEvent& event)
{
	m_mouse_down = true;

	wxPoint position = event.GetPosition();
	CalcUnscrolledPosition(position.x, position.y, &position.x, &position.y);
	m_selection.SetPosition(position);
	m_selection.SetBottomRight(position);

	OnMouseDown();

	event.Skip();
}

void TextList::OnMouseUpEvent(wxMouseEvent& event)
{
	m_mouse_down = false;
	OnMouseUp();

	event.Skip();
}

void TextList::OnMouseDClickEvent(wxMouseEvent& event)
{
	wxPoint position = event.GetPosition();
	CalcUnscrolledPosition(position.x, position.y, &position.x, &position.y);
	m_selection.SetPosition(position);
	m_selection.SetBottomRight(position);

	const uint32 line = position.y / m_line_height;
	OnMouseDClick(position, line);
}

void TextList::OnContextMenu(wxContextMenuEvent& event)
{
	wxPoint position = event.GetPosition();
	if (position == wxDefaultPosition)
		return;

	wxPoint clientPosition = ScreenToClient(position);

	CalcUnscrolledPosition(clientPosition.x, clientPosition.y, &clientPosition.x, &clientPosition.y);
	m_selection.SetPosition(clientPosition);
	m_selection.SetBottomRight(clientPosition);

	const uint32 line = clientPosition.y / m_line_height;
	OnContextMenu(clientPosition, line);
}


void TextList::OnTooltipTimer(wxTimerEvent& event)
{
	m_tooltip_window->Hide();

	const auto cursor_position = wxGetMousePosition();

	const auto position = GetScreenPosition();
	if (cursor_position.x < position.x || cursor_position.y < position.y)
		return;

	const auto size = GetSize();
	if (position.x + size.x < cursor_position.x || position.y + size.y < cursor_position.y)
		return;

	const sint32 line = position.y / m_line_height;
	if(OnShowTooltip(m_mouse_position, line))
	{
		m_tooltip_window->SetPosition(wxPoint(m_mouse_position.x + 15, m_mouse_position.y + 15));
		m_tooltip_window->SendSizeEvent();
		m_tooltip_window->Show();
	}
}

void TextList::OnPaintEvent(wxPaintEvent& event)
{
	wxBufferedPaintDC dc(m_targetWindow, wxBUFFER_VIRTUAL_AREA);
	dc.SetFont(m_font);

	// get window position
	auto position = GetPosition();

	// get current real position
	wxRect rect_update = GetUpdateRegion().GetBox();
	const auto count = (uint32)std::ceil((float)rect_update.GetHeight() / m_line_height);

	position.y = (rect_update.y / m_line_height) * m_line_height;

	// paint background
	const wxColour window_colour = COLOR_WHITE;
	dc.SetBrush(window_colour);
	dc.SetPen(window_colour);
	dc.DrawRectangle(rect_update);

	//// paint selection
	//if (!m_selected_text.eof())
	//{
	//	dc.SetBrush(*wxBLUE_BRUSH);
	//	dc.SetPen(*wxBLUE_PEN);
	//	dc.DrawRectangle(m_selection);
	//}

	sint32 start;
	CalcUnscrolledPosition(rect_update.x, rect_update.y, nullptr, &start);

	start /= m_line_height;
	m_scrolled_to_end = (start + count) >= m_element_count;

	OnDraw(dc, start, count, position);


	this->Update();
}