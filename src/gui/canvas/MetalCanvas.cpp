#include "gui/canvas/MetalCanvas.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "gui/guiWrapper.h"

#include <wx/msgdlg.h>
#include <helpers/wxHelpers.h>

MetalCanvas::MetalCanvas(wxWindow* parent, const wxSize& size, bool is_main_window)
	: IRenderCanvas(is_main_window), wxWindow(parent, wxID_ANY, wxDefaultPosition, size, wxNO_FULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	Bind(wxEVT_PAINT, &MetalCanvas::OnPaint, this);
	Bind(wxEVT_SIZE, &MetalCanvas::OnResize, this);

	WindowHandleInfo& canvas = is_main_window ? gui_getWindowInfo().canvas_main : gui_getWindowInfo().canvas_pad;
	gui_initHandleContextFromWxWidgetsWindow(canvas, this);

	try
	{
		if (is_main_window)
			g_renderer = std::make_unique<MetalRenderer>();

		auto metal_renderer = MetalRenderer::GetInstance();
		metal_renderer->InitializeLayer({size.x, size.y}, is_main_window);
	}
	catch(const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "Error when initializing Metal renderer: {}", ex.what());
		auto msg = formatWxString(_("Error when initializing Metal renderer:\n{}"), ex.what());
		wxMessageDialog dialog(this, msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		dialog.ShowModal();
		exit(0);
	}

	wxWindow::EnableTouchEvents(wxTOUCH_PAN_GESTURES);
}

MetalCanvas::~MetalCanvas()
{
	Unbind(wxEVT_PAINT, &MetalCanvas::OnPaint, this);
	Unbind(wxEVT_SIZE, &MetalCanvas::OnResize, this);

	MetalRenderer* mtlr = (MetalRenderer*)g_renderer.get();
	if (mtlr)
		mtlr->ShutdownLayer(m_is_main_window);
}

void MetalCanvas::OnPaint(wxPaintEvent& event)
{
}

void MetalCanvas::OnResize(wxSizeEvent& event)
{
	const wxSize size = GetSize();
	if (size.GetWidth() == 0 || size.GetHeight() == 0)
		return;

	const wxRect refreshRect(size);
	RefreshRect(refreshRect, false);

	auto metal_renderer = MetalRenderer::GetInstance();
	metal_renderer->ResizeLayer({size.x, size.y}, m_is_main_window);
}
