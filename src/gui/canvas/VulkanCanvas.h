#pragma once

#include "gui/canvas/IRenderCanvas.h"

#include <wx/frame.h>

#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include <set>
#include <util/helpers/Semaphore.h>


class VulkanCanvas : public IRenderCanvas, public wxWindow
{
public:
	VulkanCanvas(wxWindow* parent, const wxSize& size, bool is_main_window);
	~VulkanCanvas();

private:

	std::shared_ptr<Semaphore> m_readyForCloseSemaphore = std::make_shared<Semaphore>();

	void OnPaint(wxPaintEvent& event);
	void OnResize(wxSizeEvent& event);
};
