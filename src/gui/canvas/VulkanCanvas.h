#pragma once

#include "gui/canvas/IRenderCanvas.h"

#include <wx/frame.h>

#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include <set>

class VulkanCanvas : public IRenderCanvas, public wxWindow
{
#if BOOST_OS_LINUX && HAS_WAYLAND
	std::unique_ptr<class wxWlSubsurface> m_subsurface;
#endif
public:
	VulkanCanvas(wxWindow* parent, const wxSize& size, bool is_main_window);
	~VulkanCanvas();

private:

	void OnPaint(wxPaintEvent& event);
	void OnResize(wxSizeEvent& event);
};
