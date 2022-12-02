#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "gui/guiWrapper.h"

#include "config/CemuConfig.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"
#include <png.h>

#include "config/ActiveSettings.h"

#include <wx/image.h>
#include <wx/dataobj.h>
#include <wx/clipbrd.h>

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

bool Renderer::ImguiBegin(bool mainWindow)
{
	sint32 w = 0, h = 0;
	if(mainWindow)
		gui_getWindowSize(&w, &h);
	else if(gui_isPadWindowOpen())
		gui_getPadWindowSize(&w, &h);
	else
		return false;
		
	if (w == 0 || h == 0)
		return false;

	const Vector2f window_size{ (float)w,(float)h };
	auto& io = ImGui::GetIO();
	io.DisplaySize = { window_size.x, window_size.y }; // should be only updated in the renderer and only when needed

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

void Renderer::SaveScreenshot(const std::vector<uint8>& rgb_data, int width, int height, bool mainWindow) const
{
	const bool save_screenshot = GetConfig().save_screenshot;
	std::thread([](std::vector<uint8> data, bool save_screenshot, int width, int height, bool mainWindow)
		{
#if BOOST_OS_WINDOWS
		// on Windows wxWidgets uses OLE API for the clipboard
		// to make this work we need to call OleInitialize() on the same thread
		OleInitialize(nullptr);
#endif

		wxImage image(width, height, data.data(), true);

		if (mainWindow)
		{
			if (wxTheClipboard->Open())
			{
				wxTheClipboard->SetData(new wxImageDataObject(image));
				wxTheClipboard->Close();
				if(!save_screenshot && mainWindow)
					LatteOverlay_pushNotification("Screenshot saved to clipboard", 2500);
			}
			else
			{
				LatteOverlay_pushNotification("Failed to open clipboard", 2500);
			}
		}

		// save to png file
		if (save_screenshot)
		{
			fs::path screendir = ActiveSettings::GetUserDataPath("screenshots");
			if (!fs::exists(screendir))
				fs::create_directory(screendir);

			auto counter = 0;
			for (const auto& it : fs::directory_iterator(screendir))
			{
				int tmp;
				if (swscanf(it.path().filename().wstring().c_str(), L"screenshot_%d", &tmp) == 1)
					counter = std::max(counter, tmp);
			}

			screendir /= fmt::format(L"screenshot_{}.png", ++counter);
			
			if (image.SaveFile(screendir.wstring()))
			{
				if(mainWindow)
					LatteOverlay_pushNotification("Screenshot saved", 2500);
			}
			else
			{
				LatteOverlay_pushNotification("Failed to save screenshot to file", 2500);
			}
		}
	}, rgb_data, save_screenshot, width, height, mainWindow).detach();
}
