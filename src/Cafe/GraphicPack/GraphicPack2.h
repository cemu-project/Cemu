#pragma once

#include "util/helpers/helpers.h"
#include "Cemu/ExpressionParser/ExpressionParser.h"
#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"
#include "util/helpers/Serializer.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include <variant>
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "GraphicPack2Patches.h"
#include "util/IniParser/IniParser.h"

class GraphicPack2
{
public:
	enum class GP_SHADER_TYPE : uint8
	{
		PIXEL = 0,
		VERTEX = 1,
		GEOMETRY = 2,
	};

	enum
	{
		GFXPACK_VERSION_5 = 5,
		GFXPACK_VERSION_6 = 6, // added memory extensions
		GFXPACK_VERSION_7 = 7, // added fine-grained origin control in patch format (no more forced 4 byte alignment), .string directive (an alias to .byte) and support for more than one constant per data directive
	};

	struct TextureRule
	{
		// filter (texture must match these settings)
		struct FILTER_SETTINGS
		{
			enum class MEM1_FILTER {
				BOTH,
				INSIDE,
				OUTSIDE,
			};
			sint32 width = -1;
			sint32 height = -1;
			sint32 depth = -1;
			MEM1_FILTER inMEM1 = MEM1_FILTER::BOTH;
			std::vector<sint32> format_whitelist{};
			std::vector<sint32> format_blacklist{};
			std::vector<sint32> tilemode_whitelist{};
			std::vector<sint32> tilemode_blacklist{};
		} filter_settings;
		// overwrite (if match found, these settings are overwritten)
		struct OVERWRITE_SETTINGS
		{
			sint32 width = -1;
			sint32 height = -1;
			sint32 depth = -1;
			sint32 format = -1;
			sint32 lod_bias = -1; // in 1/64th steps
			sint32 relative_lod_bias = -1; // in 1/64th steps
			sint32 anistropic_value = -1; // 1<<n
		} overwrite_settings;		
	};

	struct CustomShader
	{
		std::string source;
		uint64 shader_base_hash;
		uint64 shader_aux_hash;
		GP_SHADER_TYPE type;
		bool isPreVulkanShader{}; // set to true for V3 packs since the shaders are not compatible with the Vulkan renderer
	};

	enum VarType
	{
		kDouble = 0,
		kInt = 1,
	};
	using PresetVar = std::pair<VarType, double>;

	struct Preset
	{
		std::string category; // preset category (empty for default)
		std::string name; // displayed name
		std::string condition;
		std::unordered_map<std::string, PresetVar> variables;
		bool active = false; // selected/active preset
		bool visible = true; // set by condition or true
		bool is_default = false; // selected by default
		
		Preset(std::string_view name, std::unordered_map<std::string, PresetVar> vars)
			: name(name), variables(std::move(vars)) {}

		Preset(std::string_view category, std::string_view name, std::unordered_map<std::string, PresetVar> vars)
			: category(category), name(name), variables(std::move(vars)) {}
		
		Preset(std::string_view category, std::string_view name, std::string_view condition, std::unordered_map<std::string, PresetVar> vars)
			: category(category), name(name), condition(condition), variables(std::move(vars)) {}
	};
	using PresetPtr = std::shared_ptr<Preset>;

	GraphicPack2(std::string filename, IniParser& rules);

	bool IsEnabled() const { return m_enabled; }
	bool IsActivated() const { return m_activated; }
	sint32 GetVersion() const { return m_version; }
	const std::string& GetFilename() const { return m_filename; }
	const fs::path GetFilename2() const { return fs::path(m_filename); }
	bool RequiresRestart(bool changeEnableState, bool changePreset);
	bool Reload();

	bool HasName() const { return !m_name.empty();  }

	const std::string& GetName() const { return m_name.empty() ? m_path : m_name; }
	const std::string& GetPath() const { return m_path; }
	const std::string& GetDescription() const { return m_description; }
	bool IsDefaultEnabled() const {	return m_default_enabled; }

	void SetEnabled(bool state) { m_enabled = state; }

	bool ContainsTitleId(uint64_t title_id) const;
	const std::vector<uint64_t>& GetTitleIds() const { return m_title_ids; }
	bool HasCustomVSyncFrequency() const { return m_vsync_frequency >= 1; }
	sint32 GetCustomVSyncFrequency() const { return m_vsync_frequency; }

	// texture rules
	const std::vector<TextureRule>& GetTextureRules() const { return m_texture_rules; }

	// presets
	[[nodiscard]] bool HasActivePreset() const;
	[[nodiscard]] std::string GetActivePreset(std::string_view category = "") const;
	[[nodiscard]] std::vector<PresetPtr> GetActivePresets() const;
	[[nodiscard]] bool IsPresetVisible(const PresetPtr& preset) const;
	[[nodiscard]] std::optional<PresetVar> GetPresetVariable(const std::vector<PresetPtr>& presets, std::string_view var_name) const;

	void ValidatePresetSelections();
	bool SetActivePreset(std::string_view category, std::string_view name, bool update_visibility = true);
	bool SetActivePreset(std::string_view name);
	void UpdatePresetVisibility();
	
	void AddConstantsForCurrentPreset(ExpressionParser& ep);
	bool ResolvePresetConstant(const std::string& varname, double& value) const;

	[[nodiscard]] const std::vector<PresetPtr>& GetPresets() const { return m_presets; }
	[[nodiscard]] std::unordered_map<std::string, std::vector<PresetPtr>> GetCategorizedPresets(std::vector<std::string>& order) const;
	
	// shaders
	void LoadShaders();
	bool HasShaders() const;
	const std::vector<CustomShader>& GetCustomShaders() const { return m_custom_shaders; }

	static const std::string* FindCustomShaderSource(uint64 shaderBaseHash, uint64 shaderAuxHash, GP_SHADER_TYPE type, bool isVulkanRenderer);

	const std::string& GetOutputShaderSource() const { return m_output_shader_source; }
	const std::string& GetDownscalingShaderSource() const { return m_downscaling_shader_source; }
	const std::string& GetUpscalingShaderSource() const { return m_upscaling_shader_source; }
	RendererOutputShader* GetOuputShader(bool render_upside_down);
	RendererOutputShader* GetUpscalingShader(bool render_upside_down);
	RendererOutputShader* GetDownscalingShader(bool render_upside_down);
	LatteTextureView::MagFilter GetUpscalingMagFilter() const { return m_output_settings.upscale_filter; }
	LatteTextureView::MagFilter GetDownscalingMagFilter() const { return m_output_settings.downscale_filter; }

	// static methods
	static void LoadAll();

	static const std::vector<std::shared_ptr<GraphicPack2>>& GetGraphicPacks() { return s_graphic_packs; }
	static const std::vector<std::shared_ptr<GraphicPack2>>& GetActiveGraphicPacks() { return s_active_graphic_packs; }
	static void LoadGraphicPack(fs::path graphicPackPath);
	static bool LoadGraphicPack(const std::string& filename, class IniParser& rules);
	static bool ActivateGraphicPack(const std::shared_ptr<GraphicPack2>& graphic_pack);
	static bool DeactivateGraphicPack(const std::shared_ptr<GraphicPack2>& graphic_pack);
	static void ClearGraphicPacks();
	static void WaitUntilReady(); // wait until all graphic packs finished activation

	static void ActivateForCurrentTitle();
	static void Reset();

private:
	bool Activate();
	bool Deactivate();

	static std::vector<std::shared_ptr<GraphicPack2>> s_graphic_packs;
	static std::vector<std::shared_ptr<GraphicPack2>> s_active_graphic_packs;
	static std::atomic_bool s_isReady;

	template<typename TType>
	void FillPresetConstants(TExpressionParser<TType>& parser) const
	{
		// fils preset variables with priority
		// active && visible > active > default
		const auto active_presets = GetActivePresets();
		for(const auto& preset : active_presets)
		{
			if(preset->visible)
			{
				for (auto& var : preset->variables)
					parser.AddConstant(var.first, (TType)var.second.second);
			}	
		}
		for(const auto& preset : active_presets)
		{
			if(!preset->visible)
			{
				for (auto& var : preset->variables)
					parser.TryAddConstant(var.first, (TType)var.second.second);
			}	
		}

		for (auto& var : m_preset_vars)
			parser.TryAddConstant(var.first, (TType)var.second.second);
	}

	std::string m_filename;

	sint32 m_version;
	std::string m_name;
	std::string m_path;
	std::string m_description;

	bool m_default_enabled = false;

	// filter
	std::optional<RendererAPI> m_renderer_api;
	std::optional<GfxVendor> m_gfx_vendor;

	bool m_enabled = false;
	bool m_activated = false; // set if the graphic pack is currently used by the running game
	std::vector<uint64_t> m_title_ids;
	bool m_patchedFilesLoaded = false; // set to true once patched files are loaded
	
	sint32 m_vsync_frequency = -1;
	sint32 m_fs_priority = 100;

	struct
	{
		LatteTextureView::MagFilter upscale_filter = LatteTextureView::MagFilter::kLinear;
		LatteTextureView::MagFilter downscale_filter = LatteTextureView::MagFilter::kLinear;
	} m_output_settings;

	std::vector<PresetPtr> m_presets;
	// default preset vars
	std::unordered_map<std::string, PresetVar> m_preset_vars;
	
	std::vector<CustomShader> m_custom_shaders;
	std::vector<TextureRule> m_texture_rules;
	std::string m_output_shader_source, m_upscaling_shader_source, m_downscaling_shader_source;
	std::unique_ptr<RendererOutputShader> m_output_shader, m_upscaling_shader, m_downscaling_shader, m_output_shader_ud, m_upscaling_shader_ud, m_downscaling_shader_ud;
	
	template<typename T>
	bool ParseRule(const ExpressionParser& parser, IniParser& iniParser, const char* option_name, T* value_out) const;

	template<typename T>
	std::vector<T> ParseList(const ExpressionParser& parser, IniParser& iniParser, const char* option_name) const;

	std::unordered_map<std::string, PresetVar> ParsePresetVars(IniParser& rules) const;

	std::vector<uint64> ParseTitleIds(IniParser& rules, const char* option_name) const;

	CustomShader LoadShader(const fs::path& path, uint64 shader_base_hash, uint64 shader_aux_hash, GP_SHADER_TYPE shader_type) const;
	void ApplyShaderPresets(std::string& shader_source) const;
	void LoadReplacedFiles();
	void _iterateReplacedFiles(const fs::path& currentPath, std::wstring& internalPath, bool isAOC);

	// ram mappings
	std::vector<std::pair<MPTR, MPTR>> m_ramMappings;

	// patches
	void LoadPatchFiles(); // loads Cemuhook or Cemu patches
	bool LoadCemuPatches();

	void ParseCemuhookPatchesTxtInternal(MemStreamReader& patchesStream);
	bool ParseCemuPatchesTxtInternal(MemStreamReader& patchesStream);
	void CancelParsingPatches();

	void ApplyPatchGroups(std::vector<PatchGroup*>& groups, const RPLModule* rpl);
	void UndoPatchGroups(std::vector<PatchGroup*>& groups, const RPLModule* rpl);

	void AddPatchGroup(PatchGroup* group);
	sint32 GetLengthWithoutComment(const char* str, size_t length);
	void LogPatchesSyntaxError(sint32 lineNumber, std::string_view errorMsg);

	std::vector<PatchGroup*> list_patchGroups;

	static std::recursive_mutex mtx_patches;
	static std::vector<const RPLModule*> list_modules;

public:
	static std::vector<std::pair<MPTR, MPTR>> GetActiveRAMMappings();
	void EnablePatches();
	void UnloadPatches();
	bool HasPatches();
	const std::vector<PatchGroup*>& GetPatchGroups();
	void ApplyPatchesForModule(const RPLModule* rpl);
	void RevertPatchesForModule(const RPLModule* rpl);

	static void NotifyModuleLoaded(const RPLModule* rpl);
	static void NotifyModuleUnloaded(const RPLModule* rpl);
};

using GraphicPackPtr = std::shared_ptr<GraphicPack2>;

template <typename T>
bool GraphicPack2::ParseRule(const ExpressionParser& parser, IniParser& iniParser, const char* option_name, T* value_out) const
{
	auto option_value = iniParser.FindOption(option_name);
	if (option_value)
	{
		*value_out = parser.Evaluate<T>(*option_value);
		return true;
	}

	return false;
}

template <typename T>
std::vector<T> GraphicPack2::ParseList(const ExpressionParser& parser, IniParser& iniParser, const char* option_name) const
{
	std::vector<T> result;

	auto option_text = iniParser.FindOption(option_name);
	if (!option_text)
		return result;

	for(auto& token : Tokenize(*option_text, ','))
	{
		try
		{
			result.emplace_back(parser.Evaluate<T>(token));
		}
		catch (const std::invalid_argument&) {}
	}
	
	return result;
}