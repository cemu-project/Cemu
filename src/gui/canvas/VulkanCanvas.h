#pragma once

#include "gui/canvas/IRenderCanvas.h"

#include <wx/frame.h>

#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include <set>



class VulkanCanvas : public IRenderCanvas, public wxWindow
{
public:
	VulkanCanvas(wxWindow* parent, bool is_main_window);
	~VulkanCanvas();

private:

	void OnPaint(wxPaintEvent& event);
	void OnResize(wxSizeEvent& event);
};
