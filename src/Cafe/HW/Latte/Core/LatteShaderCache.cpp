#include "Cafe/CafeSystem.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cemu/FileCache/FileCache.h"
#include "Cafe/GameProfile/GameProfile.h"
#include "gui/guiWrapper.h"

#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanPipelineStableCache.h"

#include <imgui.h>
#include "imgui/imgui_extension.h"

#include "config/ActiveSettings.h"
#include "Cafe/TitleList/GameInfo.h"

#include "util/helpers/SystemException.h"
#include "Cafe/HW/Latte/Common/RegisterSerializer.h"
#include "Cafe/HW/Latte/Common/ShaderSerializer.h"
#include "util/helpers/Serializer.h"

#include <wx/msgdlg.h>

#if BOOST_OS_WINDOWS
#include <psapi.h>
#endif

#define SHADER_CACHE_COMPILE_QUEUE_SIZE		(32)

struct
{
	sint32 compiledShaderCount;
	// number of loaded shaders
	sint32 vertexShaderCount;
	sint32 geometryShaderCount;
	sint32 pixelShaderCount;
}shaderCacheScreenStats;

struct  
{
	ImTextureID textureId;
	// shader loading
	sint32 loadedShaderFiles;
	sint32 shaderFileCount;
	// pipeline loading
	uint32 loadedPipelines;
	sint32 pipelineFileCount;
}g_shaderCacheLoaderState;

FileCache* fc_shaderCacheGeneric = nullptr;	// contains hardware and Cemu version independent shader information

#define SHADER_CACHE_GENERIC_EXTRA_VERSION		2 // changing this constant will invalidate all hardware-independent cache files

#define SHADER_CACHE_TYPE_VERTEX				(0)
#define SHADER_CACHE_TYPE_GEOMETRY				(1)
#define SHADER_CACHE_TYPE_PIXEL					(2)

bool LatteShaderCache_readSeparableShader(uint8* shaderInfoData, sint32 shaderInfoSize);
void LatteShaderCache_loadVulkanPipelineCache(uint64 cacheTitleId);
bool LatteShaderCache_updatePipelineLoadingProgress();
void LatteShaderCache_ShowProgress(const std::function <bool(void)>& loadUpdateFunc, bool isPipelines);

void LatteShaderCache_handleDeprecatedCacheFiles(fs::path pathGeneric, fs::path pathGenericPre1_25_0, fs::path pathGenericPre1_16_0);

struct
{
	struct
	{
		LatteDecompilerShader* shader;
	}entry[SHADER_CACHE_COMPILE_QUEUE_SIZE];
	sint32 count;
}shaderCompileQueue;

void LatteShaderCache_initCompileQueue()
{
	shaderCompileQueue.count = 0;
}

void LatteShaderCache_addToCompileQueue(LatteDecompilerShader* shader)
{
	cemu_assert(shaderCompileQueue.count < SHADER_CACHE_COMPILE_QUEUE_SIZE);
	shaderCompileQueue.entry[shaderCompileQueue.count].shader = shader;
	shaderCompileQueue.count++;
}

void LatteShaderCache_removeFromCompileQueue(sint32 index)
{
	for (sint32 i = index; i<shaderCompileQueue.count-1; i++)
		shaderCompileQueue.entry[i].shader = shaderCompileQueue.entry[i + 1].shader;
	shaderCompileQueue.count--;
}

/*
 * Process entries from compile queue until there are equal or less entries
 * left than specified by maxRemainingEntries
 */
void LatteShaderCache_updateCompileQueue(sint32 maxRemainingEntries)
{
	while (true)
	{
		if (shaderCompileQueue.count <= maxRemainingEntries)
			break;
		auto shader = shaderCompileQueue.entry[0].shader;
		if (shader)
			LatteShader_FinishCompilation(shader);
		LatteShaderCache_removeFromCompileQueue(0);
	}
}

typedef struct
{
	unsigned char imageTypeCode;
	short int imageWidth;
	short int imageHeight;
	unsigned char bitCount;
	std::vector<uint8> imageData;
} TGAFILE;

bool LoadTGAFile(const std::vector<uint8>& buffer, TGAFILE *tgaFile)
{
	if (buffer.size() <= 18)
		return false;

	tgaFile->imageTypeCode = buffer[2];
	if (tgaFile->imageTypeCode != 2 && tgaFile->imageTypeCode != 3)
		return false;

	tgaFile->imageWidth = *(uint16*)(buffer.data() + 12);
	tgaFile->imageHeight = *(uint16*)(buffer.data() + 14);
	tgaFile->bitCount = buffer[16];

	// Color mode -> 3 = BGR, 4 = BGRA.
	const uint8 colorMode = tgaFile->bitCount / 8;
	if (colorMode != 3)
		return false;

	const uint32 imageSize = tgaFile->imageWidth * tgaFile->imageHeight * colorMode;
	if (imageSize + 18 >= buffer.size())
		return false;

	tgaFile->imageData.resize(imageSize);
	std::copy(buffer.data() + 18, buffer.data() + 18 + imageSize, tgaFile->imageData.begin());
	// Change from BGR to RGB so OpenGL can read the image data.
	for (uint32 imageIdx = 0; imageIdx < imageSize; imageIdx += colorMode)
	{
		std::swap(tgaFile->imageData[imageIdx], tgaFile->imageData[imageIdx + 2]);
	}

	return true;
}

void LatteShaderCache_finish()
{
	if (g_renderer->GetType() == RendererAPI::Vulkan)
		RendererShaderVk::ShaderCacheLoading_end();
	else if (g_renderer->GetType() == RendererAPI::OpenGL)
		RendererShaderGL::ShaderCacheLoading_end();
}

uint32 LatteShaderCache_getShaderCacheExtraVersion(uint64 titleId)
{
	// encode the titleId in the version to prevent users from swapping caches between titles
	const uint32 cacheFileVersion = 1;
	uint32 extraVersion = ((uint32)(titleId >> 32) + ((uint32)titleId) * 3) + cacheFileVersion + 0xe97af1ad;
	return extraVersion;
}

uint32 LatteShaderCache_getPipelineCacheExtraVersion(uint64 titleId)
{
	const uint32 cacheFileVersion = 1;
	uint32 extraVersion = ((uint32)(titleId >> 32) + ((uint32)titleId) * 3) + cacheFileVersion;
	return extraVersion;
}

void LatteShaderCache_load()
{
	shaderCacheScreenStats.compiledShaderCount = 0;
	shaderCacheScreenStats.vertexShaderCount = 0;
	shaderCacheScreenStats.geometryShaderCount = 0;
	shaderCacheScreenStats.pixelShaderCount = 0;

	uint64 cacheTitleId = CafeSystem::GetForegroundTitleId();

	const auto timeLoadStart = now_cached();
	// remember current amount of committed memory
#if BOOST_OS_WINDOWS
	PROCESS_MEMORY_COUNTERS pmc1;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc1, sizeof(PROCESS_MEMORY_COUNTERS));
	LONGLONG totalMem1 = pmc1.PagefileUsage;
#endif
	// init shader parallel compile queue
	LatteShaderCache_initCompileQueue();
	// create directories
	std::error_code ec;
	fs::create_directories(ActiveSettings::GetPath("shaderCache/transferable"), ec);
	fs::create_directories(ActiveSettings::GetPath("shaderCache/precompiled"), ec);
	// initialize renderer specific caches
	if (g_renderer->GetType() == RendererAPI::Vulkan)
		RendererShaderVk::ShaderCacheLoading_begin(cacheTitleId);
	else if (g_renderer->GetType() == RendererAPI::OpenGL)
		RendererShaderGL::ShaderCacheLoading_begin(cacheTitleId);
	// get cache file name
	const auto pathGeneric = ActiveSettings::GetPath("shaderCache/transferable/{:016x}_shaders.bin", cacheTitleId);
	const auto pathGenericPre1_25_0 = ActiveSettings::GetPath("shaderCache/transferable/{:016x}.bin", cacheTitleId); // before 1.25.0
	const auto pathGenericPre1_16_0 = ActiveSettings::GetPath("shaderCache/transferable/{:08x}.bin", CafeSystem::GetRPXHashBase()); // before 1.16.0

	LatteShaderCache_handleDeprecatedCacheFiles(pathGeneric, pathGenericPre1_25_0, pathGenericPre1_16_0);
	// calculate extraVersion for transferable and precompiled shader cache
	uint32 transferableExtraVersion = SHADER_CACHE_GENERIC_EXTRA_VERSION;
	fc_shaderCacheGeneric = FileCache::Open(pathGeneric.generic_wstring(), false, transferableExtraVersion); // legacy extra version (1.25.0 - 1.25.1b)
	if(!fc_shaderCacheGeneric)
		fc_shaderCacheGeneric = FileCache::Open(pathGeneric.generic_wstring(), true, LatteShaderCache_getShaderCacheExtraVersion(cacheTitleId));
	if(!fc_shaderCacheGeneric)
	{
		// no shader cache available yet
		forceLog_printfW(L"Unable to open or create shader cache file \"%s\"", pathGeneric.c_str());		
		LatteShaderCache_finish();
		return;
	}
	fc_shaderCacheGeneric->UseCompression(false);

	// load/compile cached shaders
	sint32 entryCount = fc_shaderCacheGeneric->GetMaximumFileIndex();
	g_shaderCacheLoaderState.shaderFileCount = fc_shaderCacheGeneric->GetFileCount();
	g_shaderCacheLoaderState.loadedShaderFiles = 0;

	// get game background loading image
	TGAFILE file{};
	g_shaderCacheLoaderState.textureId = nullptr;

	std::string tvTexPath = fmt::format("{}/meta/bootTvTex.tga", CafeSystem::GetMlcStoragePath(CafeSystem::GetForegroundTitleId()));
	sint32 status;
	auto fscfile = fsc_open(tvTexPath.c_str(), FSC_ACCESS_FLAG::OPEN_FILE | FSC_ACCESS_FLAG::READ_PERMISSION, &status);
	if (fscfile)
	{
		uint32 size = fsc_getFileSize(fscfile);
		if (size > 0)
		{
			std::vector<uint8> tmpData(size);
			fsc_readFile(fscfile, tmpData.data(), size);
			const bool backgroundLoaded = LoadTGAFile(tmpData, &file);

			if (backgroundLoaded)
				g_shaderCacheLoaderState.textureId = g_renderer->GenerateTexture(file.imageData, { file.imageWidth, file.imageHeight });
		}

		fsc_close(fscfile);
	}

	sint32 numLoadedShaders = 0;
	uint32 loadIndex = 0;

	auto LoadShadersUpdate = [&]() -> bool
	{
		if (loadIndex >= (uint32)fc_shaderCacheGeneric->GetMaximumFileIndex())
			return false;
		LatteShaderCache_updateCompileQueue(SHADER_CACHE_COMPILE_QUEUE_SIZE - 2);
		uint64 name1;
		uint64 name2;
		std::vector<uint8> fileData;
		if (!fc_shaderCacheGeneric->GetFileByIndex(loadIndex, &name1, &name2, fileData))
		{
			loadIndex++;
			return true;
		}
		g_shaderCacheLoaderState.loadedShaderFiles++;
		if (LatteShaderCache_readSeparableShader(fileData.data(), fileData.size()) == false)
		{
			// something is wrong with the stored shader, remove entry from shader cache files
			forceLog_printf("Shader cache entry %d invalid, deleting...", loadIndex);
			fc_shaderCacheGeneric->DeleteFile({ name1, name2 });
		}
		numLoadedShaders++;
		loadIndex++;
		return true;
	};

	LatteShaderCache_ShowProgress(LoadShadersUpdate, false);
	
	LatteShaderCache_updateCompileQueue(0);
	// write load time and RAM usage to log file (in dev build)
#if BOOST_OS_WINDOWS
	const auto timeLoadEnd = now_cached();
	const auto timeLoad = std::chrono::duration_cast<std::chrono::milliseconds>(timeLoadEnd - timeLoadStart).count();
	PROCESS_MEMORY_COUNTERS pmc2;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc2, sizeof(PROCESS_MEMORY_COUNTERS));
	LONGLONG totalMem2 = pmc2.PagefileUsage;
	LONGLONG memCommited = totalMem2 - totalMem1;
	forceLog_printf("Shader cache loaded with %d shaders. Commited mem %dMB. Took %dms", numLoadedShaders, (sint32)(memCommited/1024/1024), timeLoad);
#endif
	LatteShaderCache_finish();
	// if Vulkan then also load pipeline cache
	if (g_renderer->GetType() == RendererAPI::Vulkan)
		LatteShaderCache_loadVulkanPipelineCache(cacheTitleId);

	// clear framebuffers and clean up
	auto& io = ImGui::GetIO();
	const auto kPopupFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
	for (int i = 0; i < 2; ++i)
	{
		g_renderer->BeginFrame(true);
		if (g_renderer->ImguiBegin(true))
		{
			ImGui::SetNextWindowPos({ 0,0 }, ImGuiCond_Always);
			ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
			if (ImGui::Begin("Background texture", nullptr, kPopupFlags))
			{
				if (g_shaderCacheLoaderState.textureId)
				{
					float imageDisplayWidth = io.DisplaySize.x;
					float imageDisplayHeight = 720 * imageDisplayWidth / 1280;

					float paddingLeftAndRight = 0.0f;
					float paddingTopAndBottom = (io.DisplaySize.y - imageDisplayHeight)/2.0f;
					if (imageDisplayHeight > io.DisplaySize.y)
					{
						imageDisplayHeight = io.DisplaySize.y;
						imageDisplayWidth = 1280 * imageDisplayHeight / 720;
						paddingLeftAndRight = (io.DisplaySize.x - imageDisplayWidth)/2.0f;
						paddingTopAndBottom = 0.0f;
					}

					ImGui::GetWindowDrawList()->AddImage(g_shaderCacheLoaderState.textureId, ImVec2(paddingLeftAndRight, paddingTopAndBottom), ImVec2(io.DisplaySize.x-paddingLeftAndRight, io.DisplaySize.y-paddingTopAndBottom), { 0,1 }, { 1,0 });
				}
				ImGui::End();
			}
			ImGui::PopStyleVar(2);

			g_renderer->ImguiEnd();
		}
		g_renderer->SwapBuffers(true, true);
	}

	if (g_shaderCacheLoaderState.textureId)
		g_renderer->DeleteTexture(g_shaderCacheLoaderState.textureId);
}

void LatteShaderCache_ShowProgress(const std::function <bool(void)>& loadUpdateFunc, bool isPipelines)
{
	auto& io = ImGui::GetIO();
	const auto kPopupFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
	const auto textColor = 0xFF888888;
	
	auto lastFrameUpdate = tick_cached();

	while (true)
	{
		bool r = loadUpdateFunc();
		if (!r)
			break;

		// in order to slightly speed up shader loading, we don't update the display if little time passed
		// this also avoids delayed loading in case third party software caps the framerate at 30
		if ((tick_cached() - lastFrameUpdate) < std::chrono::milliseconds(1000 / 20)) // -> aim for 20 FPS
			continue;

		int w, h;
		gui_getWindowSize(&w, &h);
		const Vector2f window_size{ (float)w,(float)h };

		ImGui_GetFont(window_size.y / 32.0f); // = 24 by default
		ImGui_GetFont(window_size.y / 48.0f); // = 16

		g_renderer->BeginFrame(true);
		if (g_renderer->ImguiBegin(true))
		{
			const auto progress_font = ImGui_GetFont(window_size.y / 32.0f); // = 24 by default
			const auto shader_count_font = ImGui_GetFont(window_size.y / 48.0f); // = 16
			// render background texture
			if (g_shaderCacheLoaderState.textureId)
			{
				ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Always);
				ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
				if (ImGui::Begin("Background texture", nullptr, kPopupFlags | ImGuiWindowFlags_NoBringToFrontOnFocus))
				{
					float imageDisplayWidth = io.DisplaySize.x;
					float imageDisplayHeight = 720 * imageDisplayWidth / 1280;

					float paddingLeftAndRight = 0.0f;
					float paddingTopAndBottom = (io.DisplaySize.y - imageDisplayHeight) / 2.0f;
					if (imageDisplayHeight > io.DisplaySize.y)
					{
						imageDisplayHeight = io.DisplaySize.y;
						imageDisplayWidth = 1280 * imageDisplayHeight / 720;
						paddingLeftAndRight = (io.DisplaySize.x - imageDisplayWidth) / 2.0f;
						paddingTopAndBottom = 0.0f;
					}

					ImGui::GetWindowDrawList()->AddImage(g_shaderCacheLoaderState.textureId, ImVec2(paddingLeftAndRight, paddingTopAndBottom), ImVec2(io.DisplaySize.x-paddingLeftAndRight, io.DisplaySize.y-paddingTopAndBottom), { 0,1 }, { 1,0 });
					ImGui::End();
				}
				ImGui::PopStyleVar(2);
			}

			ImVec2 position = { window_size.x / 2.0f, window_size.y / 2.0f };
			ImVec2 pivot = { 0.5f, 0.5f };
			ImVec2 progress_size = { io.DisplaySize.x * 0.5f, 0 };
			ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
			ImGui::SetNextWindowSize(progress_size, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.8f);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, textColor);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
			ImGui::PushFont(progress_font);

			std::string titleText = "Shader progress";

			if (ImGui::Begin(titleText.c_str(), nullptr, kPopupFlags))
			{
				const float width = ImGui::GetWindowSize().x / 2.0f;

				std::string text;
				if (isPipelines)
				{
					text = "Loading cached Vulkan pipelines...";
				}
				else
				{
					if (shaderCacheScreenStats.compiledShaderCount >= 3)
						text = "Compiling cached shaders...";
					else
						text = "Loading cached shaders...";
				}

				ImGui::SetCursorPosX(width - ImGui::CalcTextSize(text.c_str()).x / 2);
				ImGui::Text("%s", text.c_str());

				float percentLoaded;
				if(isPipelines)
					percentLoaded = (float)g_shaderCacheLoaderState.loadedPipelines / (float)g_shaderCacheLoaderState.pipelineFileCount;
				else
					percentLoaded = (float)g_shaderCacheLoaderState.loadedShaderFiles / (float)g_shaderCacheLoaderState.shaderFileCount;
				ImGui::ProgressBar(percentLoaded, { -1, 0 }, "");

				if (isPipelines)
					text = fmt::format("{}/{} ({}%)", g_shaderCacheLoaderState.loadedPipelines, g_shaderCacheLoaderState.pipelineFileCount, (int)(percentLoaded * 100));
				else
					text = fmt::format("{}/{} ({}%)", g_shaderCacheLoaderState.loadedShaderFiles, g_shaderCacheLoaderState.shaderFileCount, (int)(percentLoaded * 100));
				ImGui::SetCursorPosX(width - ImGui::CalcTextSize(text.c_str()).x / 2);
				ImGui::Text("%s", text.c_str());
				ImGui::End();
			}
			ImGui::PopFont();
			ImGui::PopStyleColor(2);

			if (!isPipelines)
			{
				position = { 10, window_size.y - 10 };
				pivot = { 0, 1 };
				ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
				ImGui::SetNextWindowBgAlpha(0.8f);
				ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
				ImGui::PushFont(shader_count_font);
				if (ImGui::Begin("Shader count", nullptr, kPopupFlags))
				{
					const float offset = shader_count_font->FallbackAdvanceX * 25.f;
					ImGui::Text("Vertex shaders");
					ImGui::SameLine(offset);
					ImGui::Text("%d", shaderCacheScreenStats.vertexShaderCount);

					ImGui::Text("Pixel shaders");
					ImGui::SameLine(offset);
					ImGui::Text("%d", shaderCacheScreenStats.pixelShaderCount);

					ImGui::Text("Geometry shaders");
					ImGui::SameLine(offset);
					ImGui::Text("%d", shaderCacheScreenStats.geometryShaderCount);

					ImGui::End();
				}
				ImGui::PopStyleColor();
				ImGui::PopFont();
			}

			g_renderer->ImguiEnd();
			lastFrameUpdate = tick_cached();
		}

		// finish frame
		g_renderer->SwapBuffers(true, true);
	}
}

void LatteShaderCache_loadVulkanPipelineCache(uint64 cacheTitleId)
{
	auto& pipelineCache = VulkanPipelineStableCache::GetInstance();
	g_shaderCacheLoaderState.pipelineFileCount = pipelineCache.BeginLoading(cacheTitleId);
	g_shaderCacheLoaderState.loadedPipelines = 0;
	LatteShaderCache_ShowProgress(LatteShaderCache_updatePipelineLoadingProgress, true);
	pipelineCache.EndLoading();
}

bool LatteShaderCache_updatePipelineLoadingProgress()
{
	uint32 pipelinesMissingShaders = 0;
	return VulkanPipelineStableCache::GetInstance().UpdateLoading(g_shaderCacheLoaderState.loadedPipelines, pipelinesMissingShaders);
}

uint64 LatteShaderCache_getShaderNameInTransferableCache(uint64 baseHash, uint32 shaderType)
{
	baseHash &= ~(7ULL << 61ULL);
	baseHash |= ((uint64)shaderType << 61ULL);
	return baseHash;
}

void LatteShaderCache_writeSeparableVertexShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* fetchShader, uint32 fetchShaderSize, uint8* vertexShader, uint32 vertexShaderSize, uint32* contextRegisters, bool usesGeometryShader)
{
	if (!fc_shaderCacheGeneric)
		return;
	MemStreamWriter streamWriter(128 * 1024);
	// header
	streamWriter.writeBE<uint8>(1 | (SHADER_CACHE_TYPE_VERTEX << 4)); // version and type (shared field)
	streamWriter.writeBE<uint64>(shaderBaseHash);
	streamWriter.writeBE<uint64>(shaderAuxHash);
	streamWriter.writeBE<uint8>(usesGeometryShader ? 1 : 0);
	// register state
	Latte::GPUCompactedRegisterState compactRegState;
	Latte::StoreGPURegisterState(*(LatteContextRegister*)contextRegisters, compactRegState);
	Latte::SerializeRegisterState(compactRegState, streamWriter);
	// fetch shader
	Latte::SerializeShaderProgram(fetchShader, fetchShaderSize, streamWriter);
	// vertex shader
	Latte::SerializeShaderProgram(vertexShader, vertexShaderSize, streamWriter);
	// write to cache
	uint64 shaderCacheName = LatteShaderCache_getShaderNameInTransferableCache(shaderBaseHash, SHADER_CACHE_TYPE_VERTEX);
	std::span<uint8> dataBlob = streamWriter.getResult();
	fc_shaderCacheGeneric->AddFileAsync({ shaderCacheName, shaderAuxHash }, dataBlob.data(), dataBlob.size());
}

void LatteShaderCache_writeSeparableGeometryShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* geometryShader, uint32 geometryShaderSize, uint8* gsCopyShader, uint32 gsCopyShaderSize, uint32* contextRegisters, uint32* hleSpecialState, uint32 vsRingParameterCount)
{
	if (!fc_shaderCacheGeneric)
		return;
	MemStreamWriter streamWriter(128 * 1024);
	// header
	streamWriter.writeBE<uint8>(1 | (SHADER_CACHE_TYPE_GEOMETRY << 4)); // version and type (shared field)
	streamWriter.writeBE<uint64>(shaderBaseHash);
	streamWriter.writeBE<uint64>(shaderAuxHash);
	cemu_assert_debug(vsRingParameterCount < 0x10000);
	streamWriter.writeBE<uint16>(vsRingParameterCount);
	// register state
	Latte::GPUCompactedRegisterState compactRegState;
	Latte::StoreGPURegisterState(*(LatteContextRegister*)contextRegisters, compactRegState);
	Latte::SerializeRegisterState(compactRegState, streamWriter);
	// geometry copy shader
	Latte::SerializeShaderProgram(gsCopyShader, gsCopyShaderSize, streamWriter);
	// geometry shader
	Latte::SerializeShaderProgram(geometryShader, geometryShaderSize, streamWriter);
	// write to cache
	uint64 shaderCacheName = LatteShaderCache_getShaderNameInTransferableCache(shaderBaseHash, SHADER_CACHE_TYPE_GEOMETRY);
	std::span<uint8> dataBlob = streamWriter.getResult();
	fc_shaderCacheGeneric->AddFileAsync({ shaderCacheName, shaderAuxHash }, dataBlob.data(), dataBlob.size());
}

void LatteShaderCache_writeSeparablePixelShader(uint64 shaderBaseHash, uint64 shaderAuxHash, uint8* pixelShader, uint32 pixelShaderSize, uint32* contextRegisters, bool usesGeometryShader)
{
	if (!fc_shaderCacheGeneric)
		return;
	MemStreamWriter streamWriter(128 * 1024);
	streamWriter.writeBE<uint8>(1 | (SHADER_CACHE_TYPE_PIXEL << 4)); // version and type (shared field)
	streamWriter.writeBE<uint64>(shaderBaseHash);
	streamWriter.writeBE<uint64>(shaderAuxHash);
	streamWriter.writeBE<uint8>(usesGeometryShader ? 1 : 0);
	// register state
	Latte::GPUCompactedRegisterState compactRegState;
	Latte::StoreGPURegisterState(*(LatteContextRegister*)contextRegisters, compactRegState);
	Latte::SerializeRegisterState(compactRegState, streamWriter);
	// pixel shader
	Latte::SerializeShaderProgram(pixelShader, pixelShaderSize, streamWriter);
	// write to cache
	uint64 shaderCacheName = LatteShaderCache_getShaderNameInTransferableCache(shaderBaseHash, SHADER_CACHE_TYPE_PIXEL);
	std::span<uint8> dataBlob = streamWriter.getResult();
	fc_shaderCacheGeneric->AddFileAsync({ shaderCacheName, shaderAuxHash }, dataBlob.data(), dataBlob.size());
}

void LatteShaderCache_loadOrCompileSeparableShader(LatteDecompilerShader* shader, uint64 shaderBaseHash, uint64 shaderAuxHash)
{
	RendererShader::ShaderType shaderType;
	if (shader->shaderType == LatteConst::ShaderType::Vertex)
	{
		shaderType = RendererShader::ShaderType::kVertex;
		shaderCacheScreenStats.vertexShaderCount++;
	}
	else if (shader->shaderType == LatteConst::ShaderType::Geometry)
	{
		shaderType = RendererShader::ShaderType::kGeometry;
		shaderCacheScreenStats.geometryShaderCount++;
	}
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		shaderType = RendererShader::ShaderType::kFragment;
		shaderCacheScreenStats.pixelShaderCount++;
	}
	// compile shader
	shaderCacheScreenStats.compiledShaderCount++;
	LatteShader_CreateRendererShader(shader, true);
	if (shader->shader == nullptr)
		return;
	LatteShaderCache_addToCompileQueue(shader);
}

bool LatteShaderCache_readSeparableVertexShader(MemStreamReader& streamReader, uint8 version)
{
	std::unique_ptr<LatteContextRegister> lcr(new LatteContextRegister());
	if (version != 1)
		return false;
	uint64 shaderBaseHash = streamReader.readBE<uint64>();
	uint64 shaderAuxHash = streamReader.readBE<uint64>();
	bool usesGeometryShader = streamReader.readBE<uint8>() != 0;
	// context registers
	Latte::GPUCompactedRegisterState regState;
	if (!Latte::DeserializeRegisterState(regState, streamReader))
		return false;
	Latte::LoadGPURegisterState(*lcr, regState);
	if (streamReader.hasError())
		return false;
	// fetch shader
	std::vector<uint8> fetchShaderData;
	if (!Latte::DeserializeShaderProgram(fetchShaderData, streamReader))
		return false;
	if (streamReader.hasError())
		return false;
	// vertex shader
	std::vector<uint8> vertexShaderData;
	if (!Latte::DeserializeShaderProgram(vertexShaderData, streamReader))
		return false;
	if (streamReader.hasError() || !streamReader.isEndOfStream())
		return false;
	// update PS inputs (influence VS shader outputs)
	LatteShader_UpdatePSInputs(lcr->GetRawView());
	// get fetch shader
	LatteFetchShader::CacheHash fsHash = LatteFetchShader::CalculateCacheHash((uint32*)fetchShaderData.data(), fetchShaderData.size());
	LatteFetchShader* fetchShader = LatteShaderRecompiler_createFetchShader(fsHash, lcr->GetRawView(), (uint32*)fetchShaderData.data(), fetchShaderData.size());
	// decompile vertex shader
	LatteDecompilerOutput_t decompilerOutput{};
	LatteFetchShader* fetchShaderList[1];
	fetchShaderList[0] = fetchShader;
	LatteDecompiler_DecompileVertexShader(shaderBaseHash, lcr->GetRawView(), vertexShaderData.data(), vertexShaderData.size(), fetchShaderList, 1, lcr->GetSpecialStateValues(), usesGeometryShader, &decompilerOutput);
	LatteDecompilerShader* vertexShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, shaderBaseHash, false, shaderAuxHash, lcr->GetRawView());
	// compile
	LatteShader_DumpShader(shaderBaseHash, shaderAuxHash, vertexShader);
	LatteShader_DumpRawShader(shaderBaseHash, shaderAuxHash, SHADER_DUMP_TYPE_VERTEX, vertexShaderData.data(), vertexShaderData.size());
	LatteShaderCache_loadOrCompileSeparableShader(vertexShader, shaderBaseHash, shaderAuxHash);
	catchOpenGLError();
	LatteSHRC_RegisterShader(vertexShader, shaderBaseHash, shaderAuxHash);
	return true;
}

bool LatteShaderCache_readSeparableGeometryShader(MemStreamReader& streamReader, uint8 version)
{
	if (version != 1)
		return false;
	std::unique_ptr<LatteContextRegister> lcr(new LatteContextRegister());
	uint64 shaderBaseHash = streamReader.readBE<uint64>();
	uint64 shaderAuxHash = streamReader.readBE<uint64>();
	uint32 vsRingParameterCount = streamReader.readBE<uint16>();
	// context registers
	Latte::GPUCompactedRegisterState regState;
	if (!Latte::DeserializeRegisterState(regState, streamReader))
		return false;
	Latte::LoadGPURegisterState(*lcr, regState);
	if (streamReader.hasError())
		return false;
	// geometry copy shader
	std::vector<uint8> geometryCopyShaderData;
	if (!Latte::DeserializeShaderProgram(geometryCopyShaderData, streamReader))
		return false;
	// geometry shader
	std::vector<uint8> geometryShaderData;
	if (!Latte::DeserializeShaderProgram(geometryShaderData, streamReader))
		return false;
	if (streamReader.hasError() || !streamReader.isEndOfStream())
		return false;
	// update PS inputs
	LatteShader_UpdatePSInputs(lcr->GetRawView());
	// decompile geometry shader
	LatteDecompilerOutput_t decompilerOutput{};
	LatteDecompiler_DecompileGeometryShader(shaderBaseHash, lcr->GetRawView(), geometryShaderData.data(), geometryShaderData.size(), geometryCopyShaderData.data(), geometryCopyShaderData.size(), lcr->GetSpecialStateValues(), vsRingParameterCount, &decompilerOutput);
	LatteDecompilerShader* geometryShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, shaderBaseHash, false, shaderAuxHash, lcr->GetRawView());
	// compile
	LatteShader_DumpShader(shaderBaseHash, shaderAuxHash, geometryShader);
	LatteShader_DumpRawShader(shaderBaseHash, shaderAuxHash, SHADER_DUMP_TYPE_GEOMETRY, geometryShaderData.data(), geometryShaderData.size());
	LatteShaderCache_loadOrCompileSeparableShader(geometryShader, shaderBaseHash, shaderAuxHash);
	catchOpenGLError();
	LatteSHRC_RegisterShader(geometryShader, shaderBaseHash, shaderAuxHash);
	return true;
}

bool LatteShaderCache_readSeparablePixelShader(MemStreamReader& streamReader, uint8 version)
{
	if (version != 1)
		return false;
	std::unique_ptr<LatteContextRegister> lcr(new LatteContextRegister());
	uint64 shaderBaseHash = streamReader.readBE<uint64>();
	uint64 shaderAuxHash = streamReader.readBE<uint64>();
	bool usesGeometryShader = streamReader.readBE<uint8>() != 0;
	// context registers
	Latte::GPUCompactedRegisterState regState;
	if (!Latte::DeserializeRegisterState(regState, streamReader))
		return false;
	Latte::LoadGPURegisterState(*lcr, regState);
	if (streamReader.hasError())
		return false;
	// pixel shader
	std::vector<uint8> pixelShaderData;
	if (!Latte::DeserializeShaderProgram(pixelShaderData, streamReader))
		return false;
	if (streamReader.hasError() || !streamReader.isEndOfStream())
		return false;
	// update PS inputs
	LatteShader_UpdatePSInputs(lcr->GetRawView());
	// decompile pixel shader
	LatteDecompilerOutput_t decompilerOutput{};
	LatteDecompiler_DecompilePixelShader(shaderBaseHash, lcr->GetRawView(), pixelShaderData.data(), pixelShaderData.size(), lcr->GetSpecialStateValues(), usesGeometryShader, &decompilerOutput);
	LatteDecompilerShader* pixelShader = LatteShader_CreateShaderFromDecompilerOutput(decompilerOutput, shaderBaseHash, false, shaderAuxHash, lcr->GetRawView());
	// compile
	LatteShader_DumpShader(shaderBaseHash, shaderAuxHash, pixelShader);
	LatteShader_DumpRawShader(shaderBaseHash, shaderAuxHash, SHADER_DUMP_TYPE_PIXEL, pixelShaderData.data(), pixelShaderData.size());
	LatteShaderCache_loadOrCompileSeparableShader(pixelShader, shaderBaseHash, shaderAuxHash);
	catchOpenGLError();
	LatteSHRC_RegisterShader(pixelShader, shaderBaseHash, shaderAuxHash);
	return true;
}

// read shader info from shader cache
bool LatteShaderCache_readSeparableShader(uint8* shaderInfoData, sint32 shaderInfoSize)
{
	if (shaderInfoSize < 8)
		return false;
	MemStreamReader streamReader(shaderInfoData, shaderInfoSize);
	uint8 versionAndType = streamReader.readBE<uint8>();
	uint8 version = versionAndType & 0xF;
	uint8 type = (versionAndType >> 4) & 0xF;
	if (type == SHADER_CACHE_TYPE_VERTEX)
		return LatteShaderCache_readSeparableVertexShader(streamReader, version);
	else if (type == SHADER_CACHE_TYPE_GEOMETRY)
		return LatteShaderCache_readSeparableGeometryShader(streamReader, version);
	else if (type == SHADER_CACHE_TYPE_PIXEL)
		return LatteShaderCache_readSeparablePixelShader(streamReader, version);
	return false;
}

#include <wx/msgdlg.h>

void LatteShaderCache_handleDeprecatedCacheFiles(fs::path pathGeneric, fs::path pathGenericPre1_25_0, fs::path pathGenericPre1_16_0)
{
	std::error_code ec;

	bool hasOldCacheFiles = fs::exists(pathGenericPre1_25_0, ec) || fs::exists(pathGenericPre1_16_0, ec);
	bool hasNewCacheFiles = fs::exists(pathGeneric, ec);

	if (hasOldCacheFiles && !hasNewCacheFiles)
	{
		// ask user if they want to delete or keep the old cache file
		const auto infoMsg = L"Outdated shader cache\n\nCemu detected that the shader cache for this game is outdated\nOnly shader caches generated with Cemu 1.25.0 or above are supported\n\n"
			"We recommend deleting the outdated cache file as it will no longer be used by Cemu";
			
		wxMessageDialog dialog(nullptr, _(infoMsg), _("Outdated shader cache"),
			wxYES_NO | wxCENTRE | wxICON_EXCLAMATION);

		dialog.SetYesNoLabels(_("Delete outdated cache file [recommended]"), _("Keep outdated cache file"));
		const auto result = dialog.ShowModal();
		if (result == wxID_YES)
		{
			fs::remove(pathGenericPre1_16_0, ec);
			fs::remove(pathGenericPre1_25_0, ec);
		}
	}
}
