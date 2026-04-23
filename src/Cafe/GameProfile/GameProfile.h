#pragma once
#include "config/CemuConfig.h"

struct gameProfileIntegerOption_t
{
	bool isPresent = false;
	sint32 value;
};

class GameProfile
{
public:
	static const uint32 kThreadQuantumDefault = 45000;

	bool Load(uint64_t title_id);
	void Save(uint64_t title_id);
	void ResetOptional();
	void Reset();

	[[nodiscard]] uint64 GetTitleId() const { return m_title_id; }
	[[nodiscard]] bool IsLoaded() const { return m_is_loaded; }
	[[nodiscard]] bool IsDefaultProfile() const { return m_is_default; }
	[[nodiscard]] const std::optional<std::string>& GetGameName() const { return m_gameName; }

	[[nodiscard]] const std::optional<bool>& ShouldLoadSharedLibraries() const { return m_loadSharedLibraries; }
	void SetShouldLoadSharedLibraries(bool shouldLoadSharedLibraries) { m_loadSharedLibraries = shouldLoadSharedLibraries; }
	[[nodiscard]] bool StartWithGamepadView() const { return m_startWithPadView; }
	void SetStartWithGamepadView(bool startWithPadView) { m_startWithPadView = startWithPadView; }

	[[nodiscard]] const std::optional<GraphicAPI>& GetGraphicsAPI() const { return m_graphics_api; }
	void SetGraphicsAPI(GraphicAPI graphicApi) { m_graphics_api = graphicApi; }
	void ClearGraphicsAPI() { m_graphics_api.reset(); }
	[[nodiscard]] const AccurateShaderMulOption& GetAccurateShaderMul() const { return m_accurateShaderMul; }
	void SetAccurateShaderMul(AccurateShaderMulOption accurateShaderMulOption) { m_accurateShaderMul = accurateShaderMulOption; }
#ifdef ENABLE_METAL
	[[nodiscard]] bool GetShaderFastMath() const { return m_shaderFastMath; }
	void SetShaderFastMath(bool shaderFastMath) { m_shaderFastMath = shaderFastMath; }
	[[nodiscard]] MetalBufferCacheMode GetBufferCacheMode() const { return m_metalBufferCacheMode; }
	void SetBufferCacheMode(MetalBufferCacheMode metalBufferCacheMode) { m_metalBufferCacheMode = metalBufferCacheMode; }
	[[nodiscard]] PositionInvariance GetPositionInvariance() const { return m_positionInvariance; }
	void SetPositionInvariance(PositionInvariance positionInvariance) { m_positionInvariance = positionInvariance; }
#endif
	[[nodiscard]] const std::optional<PrecompiledShaderOption>& GetPrecompiledShadersState() const { return m_precompiledShaders; }

	[[nodiscard]] uint32 GetThreadQuantum() const { return m_threadQuantum; }
	void SetThreadQuantum(uint32 threadQuantum){ m_threadQuantum = threadQuantum; }
	[[nodiscard]] const std::optional<CPUMode>& GetCPUMode() const { return m_cpuMode; }
	void SetCPUMode(CPUMode cpuMode) { m_cpuMode = cpuMode; }

	[[nodiscard]] bool IsAudioDisabled() const { return m_disableAudio; }

	[[nodiscard]] const std::array< std::optional<std::string>, 8>& GetControllerProfiles() const { return m_controllerProfile; }
	[[nodiscard]] const std::optional<std::string>& GetControllerProfile(size_t controllerIndex) const { return m_controllerProfile[controllerIndex]; }
	void SetControllerProfile(size_t controllerIndex, const std::string& profileName) { m_controllerProfile[controllerIndex] = profileName; }
	void ResetControllerProfile(size_t controllerIndex) { m_controllerProfile[controllerIndex].reset(); }

#if BOOST_PLAT_ANDROID
	struct DriverSetting
	{
		DriverSettingMode mode = DriverSettingMode::Global;
		std::optional<std::string> customPath;
	};

	[[nodiscard]] DriverSetting GetDriverSetting() const
	{
		return m_driverSetting;
	}

	void SetDriverSetting(DriverSetting driverSetting)
	{
		m_driverSetting = driverSetting;
	}

  private:
	DriverSetting m_driverSetting;

#endif

private:
	uint64_t m_title_id = 0;
	bool m_is_loaded = false;
	bool m_is_default = true;

	std::optional<std::string> m_gameName{};

	// general settings
	std::optional<bool> m_loadSharedLibraries{}; // = true;
	bool m_startWithPadView = false;

	// graphic settings
	std::optional<GraphicAPI> m_graphics_api{};
	AccurateShaderMulOption m_accurateShaderMul = AccurateShaderMulOption::True;
#ifdef ENABLE_METAL
	bool m_shaderFastMath = true;
	MetalBufferCacheMode m_metalBufferCacheMode = MetalBufferCacheMode::Auto;
	PositionInvariance m_positionInvariance = PositionInvariance::Auto;
#endif
	std::optional<PrecompiledShaderOption> m_precompiledShaders{};
	// cpu settings
	uint32 m_threadQuantum = kThreadQuantumDefault; // values: 20000 45000 60000 80000 100000
	std::optional<CPUMode> m_cpuMode{}; // = CPUModeOption::kSingleCoreRecompiler;
	// audio
	bool m_disableAudio = false;
	// controller settings
	std::array< std::optional<std::string>, 8> m_controllerProfile{};
};
extern std::unique_ptr<GameProfile> g_current_game_profile;

void gameProfile_load();
