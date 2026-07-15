#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "WindowSystem.h"

#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"
#include <png.h>

#include "config/ActiveSettings.h"

std::unique_ptr<Renderer> g_renderer;

bool Renderer::GetVRAMInfo(int& usageInMB, int& totalInMB) const
{
	usageInMB = totalInMB = -1;
	
#if BOOST_OS_WINDOWS
	if (m_dxgi_wrapper)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		if (m_dxgi_wrapper->QueryVideoMemoryInfo(info))
		{
			totalInMB = (info.Budget / 1000) / 1000;
			usageInMB = (info.CurrentUsage / 1000) / 1000;
			return true;
		}
	}
#endif

	return false;
}


void Renderer::Initialize()
{
	// imgui
	imguiFontAtlas = new ImFontAtlas();
	imguiFontAtlas->AddFontDefault();

	auto setupContext = [](ImGuiContext* context){
		ImGui::SetCurrentContext(context);
		ImGuiIO& io = ImGui::GetIO();
		io.WantSaveIniSettings = false;
		io.IniFilename = nullptr;
	};

	imguiTVContext = ImGui::CreateContext(imguiFontAtlas);
	imguiPadContext = ImGui::CreateContext(imguiFontAtlas);
	setupContext(imguiTVContext);
	setupContext(imguiPadContext);
}

void Renderer::Shutdown()
{
	// imgui
	ImGui::DestroyContext(imguiTVContext);
	ImGui::DestroyContext(imguiPadContext);
    ImGui_ClearFonts();
	delete imguiFontAtlas;
}

bool Renderer::ImguiBegin(bool mainWindow)
{
	sint32 w = 0, h = 0;
	if (mainWindow)
		WindowSystem::GetWindowPhysSize(w, h);
	else if (WindowSystem::IsPadWindowOpen())
		WindowSystem::GetPadWindowPhysSize(w, h);
	else
		return false;
		
	if (w == 0 || h == 0)
		return false;

	// select the right context
	ImGui::SetCurrentContext(mainWindow ? imguiTVContext : imguiPadContext);

	const Vector2f window_size{(float)w, (float)h};
	auto& io = ImGui::GetIO();
	io.DisplaySize = {window_size.x, window_size.y}; // should be only updated in the renderer and only when needed

	ImGui_PrecacheFonts();
	return true;
}

uint8 Renderer::SRGBComponentToRGB(uint8 ci)
{
	const float cs = (float)ci / 255.0f;
	float cl;
	if (cs <= 0.04045)
		cl = cs / 12.92f;
	else
		cl = std::pow((cs + 0.055f) / 1.055f, 2.4f);
	cl = std::min(cl, 1.0f);
	return (uint8)(cl * 255.0f);
}

uint8 Renderer::RGBComponentToSRGB(uint8 cli)
{
	const float cl = (float)cli / 255.0f;
	float cs;
	if (cl < 0.0031308)
		cs = 12.92f * cl;
	else
		cs = 1.055f * std::pow(cl, 0.41666f) - 0.055f;
	cs = std::max(std::min(cs, 1.0f), 0.0f);
	return (uint8)(cs * 255.0f);
}

std::optional<Renderer::ScreenshotRequestId> Renderer::RequestScreenshot(
	ScreenshotSaveFunction onSaveScreenshot, std::optional<bool> mainWindow)
{
	std::lock_guard lock(m_screenshot_mutex);
	if (!onSaveScreenshot || m_screenshot_active_request_id != 0 ||
		m_screenshot_requested.load(std::memory_order_acquire) ||
		m_screenshot_state.load(std::memory_order_acquire) != ScreenshotState::None)
		return std::nullopt;
	auto requestId = m_screenshot_next_request_id++;
	if (requestId == 0)
		requestId = m_screenshot_next_request_id++;
	m_screenshot_active_request_id = requestId;
	m_on_save_screenshot = std::move(onSaveScreenshot);
	m_screenshot_main_window = mainWindow;
	m_screenshot_requested = true;
	return requestId;
}

bool Renderer::CancelScreenshotRequest(ScreenshotRequestId requestId)
{
	std::lock_guard lock(m_screenshot_mutex);
	if (requestId == 0 || m_screenshot_active_request_id != requestId)
		return false;
	m_screenshot_requested = false;
	m_on_save_screenshot = {};
	m_screenshot_main_window.reset();
	m_screenshot_active_request_id = 0;
	return true;
}

void Renderer::CancelScreenshotRequest()
{
	std::lock_guard lock(m_screenshot_mutex);
	m_screenshot_requested = false;
	m_on_save_screenshot = {};
	m_screenshot_main_window.reset();
	m_screenshot_active_request_id = 0;
}


Renderer::ScreenshotRequestId Renderer::GetActiveScreenshotRequestId()
{
	std::lock_guard lock(m_screenshot_mutex);
	return m_screenshot_active_request_id;
}

void Renderer::SaveScreenshot(ScreenshotRequestId requestId, const std::vector<uint8>& rgb_data,
	int width, int height, bool mainWindow)
{
	ScreenshotSaveFunction onSaveScreenshot;
	{
		std::lock_guard lock(m_screenshot_mutex);
		if (requestId == 0 || requestId != m_screenshot_active_request_id)
			return;
		if (m_screenshot_main_window.has_value() && m_screenshot_main_window.value() != mainWindow)
			return;
		m_screenshot_main_window.reset();
		m_screenshot_requested = false;
		onSaveScreenshot = std::move(m_on_save_screenshot);
		m_screenshot_active_request_id = 0;
	}
	std::thread(
		[=, onSaveScreenshot = std::move(onSaveScreenshot)]() {
			if (onSaveScreenshot)
			{
				auto notificationMessage = onSaveScreenshot(rgb_data, width, height, mainWindow);
				if (notificationMessage.has_value())
					LatteOverlay_pushNotification(notificationMessage.value(), 2500);
			}
		})
		.detach();
}
