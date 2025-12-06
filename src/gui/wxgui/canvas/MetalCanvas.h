#pragma once

#include "wxgui/canvas/IRenderCanvas.h"

#include <wx/frame.h>

#include <set>

class MetalCanvas : public IRenderCanvas, public wxWindow
{
public:
	MetalCanvas(wxWindow* parent, const wxSize& size, bool is_main_window);
	~MetalCanvas();

private:

	void OnPaint(wxPaintEvent& event);
	void OnResize(wxSizeEvent& event);
};
