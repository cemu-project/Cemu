#include "wxgui/components/wxInputDraw.h"

#include <wx/dcbuffer.h>
#include <wx/settings.h>

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

	glm::vec2 position = m_position;

	wxPen black = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
	wxPen red = *wxRED_PEN;
	wxPen grey = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
	wxPen green = wxSystemSettings::SelectLightDark(0x336600, 0x99FF99);

	wxBrush black_brush = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	wxBrush red_brush = *wxRED_BRUSH;
	wxBrush grey_brush = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	wxBrush green_brush = wxSystemSettings::SelectLightDark(0x336600, 0x99FF99);

	if(!IsEnabled())
	{
		position = {};
		black.SetColour(black.GetColour());
		red.SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
		grey.SetColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT).MakeDisabled());

		black_brush = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
		red_brush = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT).MakeDisabled();
		grey_brush = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT).MakeDisabled();
	}

	dc.SetBackgroundMode(wxSOLID);
	dc.SetBackground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	dc.Clear();

	const auto size = GetSize();
	const auto min_size = (float)std::min(size.GetWidth(), size.GetHeight()) - 1.0f;
	const Vector2f middle{min_size / 2.0f, min_size / 2.0f};

	// border
	const wxRect border{0, 0, (int)min_size, (int)min_size};
	dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
	dc.DrawRectangle(border);

	dc.SetPen(IsEnabled() ? green.GetColour() : grey.GetColour());
	dc.DrawCircle((int)middle.x, (int)middle.y, (int)middle.x);

	if (m_deadzone > 0)
	{
		dc.SetPen(grey);
		dc.SetBrush(grey_brush);
		const auto deadzone_size = m_deadzone * min_size / 2.0f;
		dc.DrawCircle(
			static_cast<int>(middle.x),
			static_cast<int>(middle.x),
			(int)deadzone_size);

		if (length(position) >= m_deadzone)
		{
			dc.SetPen(red);
			dc.SetBrush(red_brush);

			if (std::abs(1.0f - length(position)) < 0.05f)
			{
				dc.SetPen(green);
				dc.SetBrush(green_brush);
			}
		}
		else
		{
			dc.SetPen(black);
			dc.SetBrush(black_brush);
		}
	}
	else
	{
		dc.SetPen(red);
		dc.SetBrush(red_brush);
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
