#include "gui/canvas/VulkanCanvas.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "gui/guiWrapper.h"

#if BOOST_OS_LINUX && HAS_WAYLAND
#include "gui/helpers/wxWayland.h"
#endif

#include <wx/msgdlg.h>
#include <helpers/wxHelpers.h>

VulkanCanvas::VulkanCanvas(wxWindow* parent, const wxSize& size, bool is_main_window)
	: IRenderCanvas(is_main_window), wxWindow(parent, wxID_ANY, wxDefaultPosition, size, wxNO_FULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
	Bind(wxEVT_PAINT, &VulkanCanvas::OnPaint, this);
	Bind(wxEVT_SIZE, &VulkanCanvas::OnResize, this);

	WindowHandleInfo& canvas = is_main_window ? gui_getWindowInfo().canvas_main : gui_getWindowInfo().canvas_pad;
	gui_initHandleContextFromWxWidgetsWindow(canvas, this);
	#if BOOST_OS_LINUX && HAS_WAYLAND
	if (canvas.backend == WindowHandleInfo::Backend::WAYLAND)
	{
		m_subsurface = std::make_unique<wxWlSubsurface>(this);
		canvas.surface = m_subsurface->getSurface();
	}
	#endif

	cemu_assert(g_vulkan_available);

	try
	{
		if (is_main_window)
			g_renderer = std::make_unique<VulkanRenderer>();

		auto vulkan_renderer = VulkanRenderer::GetInstance();
		vulkan_renderer->InitializeSurface({size.x, size.y}, is_main_window);
	}
	catch(const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "Error when initializing Vulkan renderer: {}", ex.what());
		auto msg = formatWxString(_("Error when initializing Vulkan renderer:\n{}"), ex.what());
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

	if(!m_is_main_window)
	{
		VulkanRenderer* vkr = (VulkanRenderer*)g_renderer.get();
		if(vkr)
			vkr->StopUsingPadAndWait();
	}
}

void VulkanCanvas::OnPaint(wxPaintEvent& event)
{
}

void VulkanCanvas::OnResize(wxSizeEvent& event)
{
	const wxSize size = GetSize();
	if (size.GetWidth() == 0 || size.GetHeight() == 0)
		return;

#if BOOST_OS_LINUX && HAS_WAYLAND
	if(m_subsurface)
	{
		auto sRect = GetScreenRect();
		m_subsurface->setSize(sRect.GetX(), sRect.GetY(), sRect.GetWidth(), sRect.GetHeight());
	}
#endif

	const wxRect refreshRect(size);
	RefreshRect(refreshRect, false);
}
