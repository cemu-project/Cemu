#include "Cafe/GameProfile/GameProfile.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/CafeSystem.h"
#include "Cemu/Logging/CemuLogging.h"
#include "config/ActiveSettings.h"
#include "config/LaunchSettings.h"
#include "util/helpers/helpers.h"

void ActiveSettings::SetPaths(bool isPortableMode,
		const fs::path& executablePath,
		const fs::path& userDataPath,
		const fs::path& configPath,
		const fs::path& cachePath,
		const fs::path& dataPath,
		std::set<fs::path>& failedWriteAccess)
{
	cemu_assert_debug(!s_setPathsCalled); // can only change paths before loading
	s_isPortableMode = isPortableMode;
	s_executable_path = executablePath;
	s_user_data_path = userDataPath;
	s_config_path = configPath;
	s_cache_path = cachePath;
	s_data_path = dataPath;
	failedWriteAccess.clear();
	for (auto&& path : {userDataPath, configPath, cachePath})
	{
		std::error_code ec;
		if (!fs::exists(path, ec))
			fs::create_directories(path, ec);
		if (!TestWriteAccess(path))
		{
			cemuLog_log(LogType::Force, "Failed to write to {}", _pathToUtf8(path));
			failedWriteAccess.insert(path);
		}
	}
	s_executable_filename = s_executable_path.filename();
	s_setPathsCalled = true;
}

[[nodiscard]] bool ActiveSettings::IsPortableMode()
{
	return s_isPortableMode;
}

void ActiveSettings::Init()
{
	cemu_assert_debug(s_setPathsCalled);
	std::string additionalErrorInfo;
	s_has_required_online_files = iosuCrypt_checkRequirementsForOnlineMode(additionalErrorInfo) == IOS_CRYPTO_ONLINE_REQ_OK;
}

bool ActiveSettings::LoadSharedLibrariesEnabled()
{
	return g_current_game_profile->ShouldLoadSharedLibraries().value_or(true);
}

bool ActiveSettings::DisplayDRCEnabled()
{
	return g_current_game_profile->StartWithGamepadView();
}

bool ActiveSettings::FullscreenEnabled()
{
	if (LaunchSettings::FullscreenEnabled().has_value())
		return LaunchSettings::FullscreenEnabled().value();

	return GetConfig().fullscreen;
}

CPUMode ActiveSettings::GetCPUMode()
{
	auto mode = g_current_game_profile->GetCPUMode().value_or(CPUMode::Auto);

	if (mode == CPUMode::Auto)
	{
		if (GetPhysicalCoreCount() >= 4)
			mode = CPUMode::MulticoreRecompiler;
		else
			mode = CPUMode::SinglecoreRecompiler;
	}
	else if (mode == CPUMode::DualcoreRecompiler) // dualcore is disabled now
		mode = CPUMode::MulticoreRecompiler;

	return mode;
}

uint8 ActiveSettings::GetTimerShiftFactor()
{
	return s_timer_shift;
}

void ActiveSettings::SetTimerShiftFactor(uint8 shiftFactor)
{
	s_timer_shift = shiftFactor;
}

PrecompiledShaderOption ActiveSettings::GetPrecompiledShadersOption()
{
	return PrecompiledShaderOption::Auto; // g_current_game_profile->GetPrecompiledShadersState().value_or(GetConfig().precompiled_shaders);
}

bool ActiveSettings::RenderUpsideDownEnabled()
{
	return LaunchSettings::RenderUpsideDownEnabled().value_or(GetConfig().render_upside_down);
}

bool ActiveSettings::WaitForGX2DrawDoneEnabled()
{
	return GetConfig().gx2drawdone_sync;
}

GraphicAPI ActiveSettings::GetGraphicsAPI()
{
	GraphicAPI api = g_current_game_profile->GetGraphicsAPI().value_or(GetConfig().graphic_api);
	// check if vulkan even available
	if (api == kVulkan && !g_vulkan_available)
		api = kOpenGL;
	
	return api;
}

bool ActiveSettings::AudioOutputOnlyAux()
{
	return s_audio_aux_only;
}

void ActiveSettings::EnableAudioOnlyAux(bool state)
{
	s_audio_aux_only = state;
}

uint32 ActiveSettings::GetPersistentId()
{
	return LaunchSettings::GetPersistentId().value_or(GetConfig().account.m_persistent_id);
}

bool ActiveSettings::IsOnlineEnabled()
{
	if(!Account::GetAccount(GetPersistentId()).IsValidOnlineAccount())
		return false;
	if(!HasRequiredOnlineFiles())
		return false;
	NetworkService networkService = static_cast<NetworkService>(GetConfig().GetAccountNetworkService(GetPersistentId()));
	return networkService == NetworkService::Nintendo || networkService == NetworkService::Pretendo || networkService == NetworkService::Custom;
}

bool ActiveSettings::HasRequiredOnlineFiles()
{
	return s_has_required_online_files;
}

NetworkService ActiveSettings::GetNetworkService()
{
	return GetConfig().GetAccountNetworkService(GetPersistentId());
}

bool ActiveSettings::DumpShadersEnabled()
{
	return s_dump_shaders;
}

bool ActiveSettings::DumpTexturesEnabled()
{
	return s_dump_textures;
}

bool ActiveSettings::DumpLibcurlRequestsEnabled()
{
	return s_dump_libcurl_requests;
}

void ActiveSettings::EnableDumpShaders(bool state)
{
	s_dump_shaders = state;
}

void ActiveSettings::EnableDumpTextures(bool state)
{
	s_dump_textures = state;
}

void ActiveSettings::EnableDumpLibcurlRequests(bool state)
{
	s_dump_libcurl_requests = state;
}

bool ActiveSettings::VPADDelayEnabled()
{
	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	// workaround for Art Academy spamming VPADRead
	return /* Art Academy: Home Studio (US) */ titleId == 0x000500001017BF00 ||
		/* Art Academy: Home Studio (JP) */ titleId == 0x000500001017BE00 ||
		/* Art Academy: Atelier (EU) */ titleId == 0x000500001017B500;
}

bool ActiveSettings::ShaderPreventInfiniteLoopsEnabled()
{
	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	// workaround for NSMBU (and variants) having a bug where shaders can get stuck in infinite loops
	// Fatal Frame has an actual infinite loop in shader 0xb6a67c19f6472e00 encountered during a cutscene for the second drop (eShop version only?)
	// update: As of Cemu 1.20.0 this should no longer be required for NSMBU/NSLU due to fixes with uniform handling. But we leave it here for good measure
	// todo - Once we add support for loop config registers this workaround should become unnecessary
	return /* NSMBU JP */ titleId == 0x0005000010101C00 ||
		/* NSMBU US */ titleId == 0x0005000010101D00 ||
		/* NSMBU EU */ titleId == 0x0005000010101E00 ||
		/* NSMBU+L US */ titleId == 0x000500001014B700 ||
		/* NSMBU+L EU */ titleId == 0x000500001014B800 ||
		/* NSLU US */ titleId == 0x0005000010142300 ||
		/* NSLU EU */ titleId == 0x0005000010142400 ||
	   /* Project Zero: Maiden of Black Water (EU) */ titleId == 0x00050000101D0300 ||
	   /* Fatal Frame: Maiden of Black Water (US) */ titleId == 0x00050000101D0600 ||
	   /* Project Zero: Maiden of Black Water (JP) */ titleId == 0x000500001014D200 ||
	   /* Project Zero: Maiden of Black Water (Trial, EU) */ titleId == 0x00050000101D3F00;
}

bool ActiveSettings::FlushGPUCacheOnSwap()
{
	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	// games that require flushing the cache between frames
	return
		/* PAC-MAN and the Ghostly Adventures (EU) */ titleId == 0x0005000010147900 ||
		/* PAC-MAN and the Ghostly Adventures (US) */ titleId == 0x0005000010146300 ||
		/* PAC-MAN and the Ghostly Adventures 2 (EU) */ titleId == 0x000500001017E500 ||
		/* PAC-MAN and the Ghostly Adventures 2 (US) */ titleId == 0x000500001017E600;
}

bool ActiveSettings::ForceSamplerRoundToPrecision()
{
	// some Wayforward games (Duck Tales, Adventure Time Explore The Dungeon) sample textures exactly on the texel edge. On Wii U this is fine because the rounding mode can be controlled
	// on OpenGL/Vulkan when uv coordinates are converted from (un)normalized to integer the implementation always truncates which causes an off-by-one error in edge cases
	// In the future we should look into optionally correctly emulating sampler behavior (its quite complex, Latte supports flexible precision levels)
	const uint64 titleId = CafeSystem::GetForegroundTitleId();
	return
		/* Adventure Time ETDBIDK (EU) */ titleId == 0x000500001014E100 ||
		/* Adventure Time ETDBIDK (US) */ titleId == 0x0005000010144000 ||
		/* DuckTales Remastered (EU) */ titleId == 0x0005000010129200 ||
		/* DuckTales Remastered (US) */ titleId == 0x0005000010129000;
	return false;
}

fs::path ActiveSettings::GetMlcPath()
{
	cemu_assert_debug(s_setPathsCalled);
	if(const auto launch_mlc = LaunchSettings::GetMLCPath(); launch_mlc.has_value())
		return launch_mlc.value();

	if(const auto config_mlc = GetConfig().mlc_path.GetValue(); !config_mlc.empty())
		return _utf8ToPath(config_mlc);

	return GetDefaultMLCPath();
}

bool ActiveSettings::IsCustomMlcPath()
{
	cemu_assert_debug(s_setPathsCalled);
	return !GetConfig().mlc_path.GetValue().empty();
}

bool ActiveSettings::IsCommandLineMlcPath()
{
	return LaunchSettings::GetMLCPath().has_value();
}

fs::path ActiveSettings::GetDefaultMLCPath()
{
	return GetUserDataPath("mlc01");
}

