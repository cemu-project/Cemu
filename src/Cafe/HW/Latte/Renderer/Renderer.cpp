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
#include <wx/log.h>

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
	if(mainWindow)
		gui_getWindowPhysSize(w, h);
	else if(gui_isPadWindowOpen())
		gui_getPadWindowPhysSize(w, h);
	else
		return false;
		
	if (w == 0 || h == 0)
		return false;

	// select the right context
	ImGui::SetCurrentContext(mainWindow ? imguiTVContext : imguiPadContext);

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

static std::optional<fs::path> GenerateScreenshotFilename(bool isDRC)
{
	fs::path screendir = ActiveSettings::GetUserDataPath("screenshots");
	// build screenshot name with format Screenshot_YYYY-MM-DD_HH-MM-SS[_GamePad].png
	// if the file already exists add a suffix counter (_2.png, _3.png etc)
	std::time_t time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm* tm = std::localtime(&time_t);

	std::string screenshotFileName = fmt::format("Screenshot_{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	if (isDRC)
		screenshotFileName.append("_GamePad");

	fs::path screenshotPath;
	for(sint32 i=0; i<999; i++)
	{
		screenshotPath = screendir;
		if (i == 0)
			screenshotPath.append(fmt::format("{}.png", screenshotFileName));
		else
			screenshotPath.append(fmt::format("{}_{}.png", screenshotFileName, i + 1));
		
		std::error_code ec;
		bool exists = fs::exists(screenshotPath, ec);
		
		if (!ec && !exists)
			return screenshotPath;
	}
	return std::nullopt;
}

std::mutex s_clipboardMutex;

static bool SaveScreenshotToClipboard(const wxImage &image)
{
	bool success = false;

	s_clipboardMutex.lock();
	if (wxTheClipboard->Open())
	{
		wxTheClipboard->SetData(new wxImageDataObject(image));
		wxTheClipboard->Close();
		success = true;
	}
	s_clipboardMutex.unlock();

	return success;
}

static bool SaveScreenshotToFile(const wxImage &image, bool mainWindow)
{
	auto path = GenerateScreenshotFilename(!mainWindow);
	if (!path) return false;

	std::error_code ec;
	fs::create_directories(path->parent_path(), ec);
	if (ec) return false;

	// suspend wxWidgets logging for the lifetime this object, to prevent a message box if wxImage::SaveFile fails
	wxLogNull _logNo;
	return image.SaveFile(path->wstring());
}

static void ScreenshotThread(std::vector<uint8> data, bool save_screenshot, int width, int height, bool mainWindow)
{
#if BOOST_OS_WINDOWS
	// on Windows wxWidgets uses OLE API for the clipboard
	// to make this work we need to call OleInitialize() on the same thread
	OleInitialize(nullptr);
#endif
	
	wxImage image(width, height, data.data(), true);

	if (mainWindow)
	{
		if(SaveScreenshotToClipboard(image))
		{
			if (!save_screenshot)
				LatteOverlay_pushNotification("Screenshot saved to clipboard", 2500);
		}
		else
		{
			LatteOverlay_pushNotification("Failed to open clipboard", 2500);
		}
	}

	if (save_screenshot)
	{
		if (SaveScreenshotToFile(image, mainWindow))
		{
			if (mainWindow)
				LatteOverlay_pushNotification("Screenshot saved", 2500);
		}
		else
		{
			LatteOverlay_pushNotification("Failed to save screenshot to file", 2500);
		}
	}
}

void Renderer::SaveScreenshot(const std::vector<uint8>& rgb_data, int width, int height, bool mainWindow) const
{
	const bool save_screenshot = GetConfig().save_screenshot;
	std::thread(ScreenshotThread, rgb_data, save_screenshot, width, height, mainWindow).detach();
}
