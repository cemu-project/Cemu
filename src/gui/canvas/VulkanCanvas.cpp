#include "gui/canvas/VulkanCanvas.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "gui/guiWrapper.h"

#include <wx/msgdlg.h>

VulkanCanvas::VulkanCanvas(wxWindow* parent, const wxSize& size, bool is_main_window)
	: IRenderCanvas(is_main_window), wxWindow(parent, wxID_ANY, wxDefaultPosition, size, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	Bind(wxEVT_PAINT, &VulkanCanvas::OnPaint, this);
	Bind(wxEVT_SIZE, &VulkanCanvas::OnResize, this);

	if(is_main_window)
		gui_initHandleContextFromWxWidgetsWindow(gui_getWindowInfo().canvas_main, this);
	else
		gui_initHandleContextFromWxWidgetsWindow(gui_getWindowInfo().canvas_pad, this);

	cemu_assert(g_vulkan_available);

	try
	{
		if (is_main_window)
			g_renderer = std::make_unique<VulkanRenderer>();

		auto vulkan_renderer = VulkanRenderer::GetInstance();
		vulkan_renderer->Initialize({size.x, size.y}, is_main_window);
	}
	catch(const std::exception& ex)
	{
		const auto msg = fmt::format(fmt::runtime(_("Error when initializing Vulkan renderer:\n{}").ToStdString()), ex.what());
		forceLog_printf(const_cast<char*>(msg.c_str()));
		wxMessageDialog dialog(this, msg, _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
		dialog.ShowModal();
		exit(0);
	}

	wxWindow::EnableTouchEvents(wxTOUCH_PAN_GESTURES);
}

VulkanCanvas::~VulkanCanvas()
{
	Unbind(wxEVT_PAINT, &VulkanCanvas::OnPaint, this);
	Unbind(wxEVT_SIZE, &VulkanCanvas::OnResize, this);
}

void VulkanCanvas::OnPaint(wxPaintEvent& event)
{
}

void VulkanCanvas::OnResize(wxSizeEvent& event)
{
	const wxSize size = GetSize();
	if (size.GetWidth() == 0 || size.GetHeight() == 0) 
		return;

	const wxRect refreshRect(size);
	RefreshRect(refreshRect, false);

	if (g_renderer == nullptr)
		return;

	auto vulkan_renderer = VulkanRenderer::GetInstance();
	vulkan_renderer->ResizeSwapchain({ size.x, size.y }, m_is_main_window);
}