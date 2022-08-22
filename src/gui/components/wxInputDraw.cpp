#include "gui/components/wxInputDraw.h"

#include <wx/dcbuffer.h>

wxInputDraw::wxInputDraw(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size, 0, wxPanelNameStr)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	Bind(wxEVT_PAINT, &wxInputDraw::OnPaintEvent, this);
}

wxInputDraw::~wxInputDraw() { Unbind(wxEVT_PAINT, &wxInputDraw::OnPaintEvent, this); }

void wxInputDraw::OnPaintEvent(wxPaintEvent& event)
{
	wxAutoBufferedPaintDC dc(this);
	OnRender(dc);
}

void wxInputDraw::OnRender(wxDC& dc)
{
	dc.Clear();

	glm::vec2 position;
	const wxPen *black, *red, *grey;
	const wxBrush *black_brush, *red_brush, *grey_brush;
	if(IsEnabled())
	{
		position = m_position;

		black = wxBLACK_PEN;
		red = wxRED_PEN;
		grey = wxGREY_PEN;

		black_brush = wxBLACK_BRUSH;
		red_brush = wxRED_BRUSH;
		grey_brush = wxGREY_BRUSH;
	}
	else
	{
		position = {};
		black = red = wxMEDIUM_GREY_PEN;
		grey = wxLIGHT_GREY_PEN;

		black_brush = red_brush = wxMEDIUM_GREY_BRUSH;
		grey_brush = wxLIGHT_GREY_BRUSH;
	}

	dc.SetBackgroundMode(wxSOLID);
	dc.SetBackground(*wxWHITE_BRUSH);

	const auto size = GetSize();
	const auto min_size = (float)std::min(size.GetWidth(), size.GetHeight()) - 1.0f;
	const Vector2f middle{min_size / 2.0f, min_size / 2.0f};

	// border
	const wxRect border{0, 0, (int)min_size, (int)min_size};
	dc.SetPen(*black);
	dc.DrawRectangle(border);

	dc.SetPen(IsEnabled() ? wxPen(wxColour(0x336600)) : *grey); // dark green
	dc.DrawCircle((int)middle.x, (int)middle.y, (int)middle.x);

	if (m_deadzone > 0)
	{
		dc.SetPen(*grey);
		dc.SetBrush(*wxLIGHT_GREY_BRUSH);
		const auto deadzone_size = m_deadzone * min_size / 2.0f;
		dc.DrawCircle(
			static_cast<int>(middle.x),
			static_cast<int>(middle.x),
			(int)deadzone_size);

		if (length(position) >= m_deadzone)
		{
			dc.SetPen(*red);
			dc.SetBrush(*red_brush);

			if (std::abs(1.0f - length(position)) < 0.05f)
			{
				dc.SetPen(wxPen(wxColour(0x336600)));
				dc.SetBrush(wxColour(0x336600));
			}
		}
		else
		{
			dc.SetPen(*black);
			dc.SetBrush(*black_brush);
		}
	}
	else
	{
		dc.SetPen(*red);
		dc.SetBrush(*red_brush);
	}

	// draw axis
	const wxPoint pos
	{
		static_cast<int>(middle.x + position.x * middle.x),
		static_cast<int>(middle.y - position.y * middle.y)
	};
	dc.DrawCircle(pos.x, pos.y, 2);

	dc.SetBrush(*wxTRANSPARENT_BRUSH);
}

void wxInputDraw::SetAxisValue(const glm::vec2& position)
{
	m_position = position;
	Refresh();
}

void wxInputDraw::SetDeadzone(float deadzone)
{
	m_deadzone = deadzone;
	Refresh();
}
