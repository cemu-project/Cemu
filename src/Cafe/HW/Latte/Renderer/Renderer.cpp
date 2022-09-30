#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "gui/guiWrapper.h"

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
#if BOOST_OS_WINDOWS
	const bool save_screenshot = GetConfig().save_screenshot;
	std::thread([](std::vector<uint8> data, bool save_screenshot, int width, int height, bool mainWindow)
	{
		if (mainWindow)
		{
			// copy to clipboard
			std::vector<uint8> buffer(sizeof(BITMAPINFO) + data.size());
			auto* bmpInfo = (BITMAPINFO*)buffer.data();
			bmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmpInfo->bmiHeader.biWidth = width;
			bmpInfo->bmiHeader.biHeight = height;
			bmpInfo->bmiHeader.biPlanes = 1;
			bmpInfo->bmiHeader.biBitCount = 24;
			bmpInfo->bmiHeader.biCompression = BI_RGB;

			uint8* clipboard_image = buffer.data() + sizeof(BITMAPINFOHEADER);
			// RGB -> BGR
			for (sint32 iy = 0; iy < height; ++iy)
			{
				for (sint32 ix = 0; ix < width; ++ix)
				{
					uint8* pIn = data.data() + (ix + iy * width) * 3;
					uint8* pOut = clipboard_image + (ix + (height - iy - 1) * width) * 3;

					pOut[0] = pIn[2];
					pOut[1] = pIn[1];
					pOut[2] = pIn[0];
				}
			}

			if (OpenClipboard(nullptr))
			{
				EmptyClipboard();

				const HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, buffer.size());
				if (hGlobal)
				{
					memcpy(GlobalLock(hGlobal), buffer.data(), buffer.size());
					GlobalUnlock(hGlobal);

					SetClipboardData(CF_DIB, hGlobal);
					GlobalFree(hGlobal);
				}

				CloseClipboard();
			}

			LatteOverlay_pushNotification("Screenshot saved", 2500);
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
				if (swscanf_s(it.path().filename().c_str(), L"screenshot_%d", &tmp) == 1)
					counter = std::max(counter, tmp);
			}

			screendir /= fmt::format(L"screenshot_{}.png", ++counter);
			FileStream* fs = FileStream::createFile2(screendir);
			if (fs)
			{
				bool success = true;
				auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
				if (png_ptr)
				{
					auto info_ptr = png_create_info_struct(png_ptr);
					if (info_ptr)
					{
						if (!setjmp(png_jmpbuf(png_ptr)))
						{
							auto pngWriter = [](png_structp png_ptr, png_bytep data, png_size_t length) -> void
							{
								FileStream* fs = (FileStream*)png_get_io_ptr(png_ptr);
								fs->writeData(data, length);
							};

							//png_init_io(png_ptr, file);
							png_set_write_fn(png_ptr, (void*)fs, pngWriter, nullptr);

							png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
							             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
							png_write_info(png_ptr, info_ptr);
							for (int i = 0; i < height; ++i)
							{
								uint8* pData = data.data() + (i * width) * 3;
								png_write_row(png_ptr, pData);
							}

							png_write_end(png_ptr, nullptr);
						}
						else
							success = false;

						png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
					}

					png_destroy_write_struct(&png_ptr, nullptr);
				}
				delete fs;
				if (!success)
				{
					std::error_code ec;
					fs::remove(screendir, ec);
				}
			}
		}
	}, rgb_data, save_screenshot, width, height, mainWindow).detach();

#else
cemuLog_log(LogType::Force, "Screenshot feature not implemented");
#endif
}
