#include "Cafe/GraphicPack/GraphicPack2.h"
#include "config/CemuConfig.h"
#include "config/ActiveSettings.h"
#include "openssl/sha.h"
#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"
#include "Cafe/Filesystem/fsc.h"
#include "boost/algorithm/string.hpp"
#include "util/helpers/MapAdaptor.h"
#include "util/helpers/StringParser.h"
#include "Cafe/HW/Latte/Core/LatteTiming.h"
#include "util/IniParser/IniParser.h"
#include "util/helpers/StringHelpers.h"
#include "Cafe/CafeSystem.h"
#include <cinttypes>

std::vector<GraphicPackPtr> GraphicPack2::s_graphic_packs;
std::vector<GraphicPackPtr> GraphicPack2::s_active_graphic_packs;
std::atomic_bool GraphicPack2::s_isReady;

#define GP_LEGACY_VERSION		(2)

void GraphicPack2::LoadGraphicPack(fs::path graphicPackPath)
{
	fs::path rulesPath = graphicPackPath;
	rulesPath.append("rules.txt");
	std::unique_ptr<FileStream> fs_rules(FileStream::openFile2(rulesPath));
	if (!fs_rules)
		return;
	std::vector<uint8> rulesData;
	fs_rules->extract(rulesData);
	IniParser iniParser(rulesData, _pathToUtf8(rulesPath));

	if (!iniParser.NextSection())
	{
		cemuLog_log(LogType::Force, "{}: Does not contain any sections", _pathToUtf8(rulesPath));
		return;
	}
	if (!boost::iequals(iniParser.GetCurrentSectionName(), "Definition"))
	{
		cemuLog_log(LogType::Force, "{}: [Definition] must be the first section", _pathToUtf8(rulesPath));
		return;
	}

	auto option_version = iniParser.FindOption("version");
	if (option_version)
	{
		sint32 versionNum = -1;
		auto [ptr, ec] = std::from_chars(option_version->data(), option_version->data() + option_version->size(), versionNum);
		if (ec != std::errc{})
		{
			cemuLog_log(LogType::Force, "{}: Unable to parse version", _pathToUtf8(rulesPath));
			return;
		}
		if (versionNum > GP_LEGACY_VERSION)
		{
			GraphicPack2::LoadGraphicPack(rulesPath, iniParser);
			return;
		}
	}
	cemuLog_log(LogType::Force, "{}: Outdated graphic pack", _pathToUtf8(rulesPath));
}

void GraphicPack2::LoadAll()
{
	std::error_code ec;
	fs::path basePath = ActiveSettings::GetUserDataPath("graphicPacks");
	for (fs::recursive_directory_iterator it(basePath, ec); it != end(it); ++it)
	{
		if (!it->is_directory(ec))
			continue;
		fs::path gfxPackPath = it->path();
		if (fs::exists(gfxPackPath / "rules.txt", ec))
		{
			LoadGraphicPack(gfxPackPath);
			it.disable_recursion_pending(); // dont recurse deeper in a gfx pack directory
			continue;
		}
	}
}

bool GraphicPack2::LoadGraphicPack(const fs::path& rulesPath, IniParser& rules)
{
	try
	{
		auto gp = std::make_shared<GraphicPack2>(rulesPath, rules);

		// check if enabled and preset set
		const auto& config_entries = g_config.data().graphic_pack_entries;

		// legacy absolute path checking for not breaking compatibility
		auto file = gp->GetRulesPath();
		auto it = config_entries.find(file.lexically_normal());
		if (it == config_entries.cend())
		{
			// check for relative path
			it = config_entries.find(_utf8ToPath(gp->GetNormalizedPathString()));
		}

		if (it != config_entries.cend())
		{
			bool enabled = true;
			for (auto& kv : it->second)
			{
				if (boost::iequals(kv.first, "_disabled"))
				{
					enabled = false;
					continue;
				}

				gp->SetActivePreset(kv.first, kv.second, false);
			}
			
			gp->SetEnabled(enabled);
		}

		gp->UpdatePresetVisibility();
		gp->ValidatePresetSelections();

		s_graphic_packs.emplace_back(gp);
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

bool GraphicPack2::ActivateGraphicPack(const std::shared_ptr<GraphicPack2>& graphic_pack)
{
	if (graphic_pack->Activate())
	{
		s_active_graphic_packs.push_back(graphic_pack);
		return true;
	}

	return false;
}

bool GraphicPack2::DeactivateGraphicPack(const std::shared_ptr<GraphicPack2>& graphic_pack)
{
	if (!graphic_pack->IsActivated())
		return false;

	const auto it = std::find_if(s_active_graphic_packs.begin(), s_active_graphic_packs.end(), 
		[graphic_pack](const GraphicPackPtr& gp)
	{
		return gp->GetNormalizedPathString() == graphic_pack->GetNormalizedPathString();
	}
	);

	if (it == s_active_graphic_packs.end())
		return false;

	graphic_pack->Deactivate();
	s_active_graphic_packs.erase(it);
	return true;
}

void GraphicPack2::ActivateForCurrentTitle()
{
	uint64 titleId = CafeSystem::GetForegroundTitleId();
	// activate graphic packs
	for (const auto& gp : GraphicPack2::GetGraphicPacks())
	{
		if (!gp->IsEnabled())
			continue;

		if (!gp->ContainsTitleId(titleId))
			continue;

		if (GraphicPack2::ActivateGraphicPack(gp))
		{
			if (gp->GetPresets().empty())
			{
				cemuLog_log(LogType::Force, "Activate graphic pack: {}", gp->GetVirtualPath());
			}
			else
			{
				std::string logLine;
				logLine.assign(fmt::format("Activate graphic pack: {} [Presets: ", gp->GetVirtualPath()));
				bool isFirst = true;
				for (auto& itr : gp->GetPresets())
				{
					if (!itr->active)
						continue;
					if (isFirst)
						isFirst = false;
					else
						logLine.append(",");
					logLine.append(itr->name);
				}
				logLine.append("]");
				cemuLog_log(LogType::Force, logLine);
			}
		}
	}
	s_isReady = true;
}

void GraphicPack2::Reset()
{
	s_active_graphic_packs.clear();
	s_isReady = false;
}

void GraphicPack2::ClearGraphicPacks()
{
	s_graphic_packs.clear();
	s_active_graphic_packs.clear();
}

void GraphicPack2::WaitUntilReady()
{
	while (!s_isReady)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

std::unordered_map<std::string, GraphicPack2::PresetVar> GraphicPack2::ParsePresetVars(IniParser& rules) const
{
	ExpressionParser parser;
	std::unordered_map<std::string, PresetVar> vars;
	for(auto& itr : rules.GetAllOptions())
	{
		auto option_name = itr.first;
		auto option_value = itr.second;
		if (option_name.empty() || option_name[0] != '$')
			continue;
		VarType type = kDouble;
		std::string name(option_name);
		const auto index = name.find(':');
		if(index != std::string::npos)
		{
			auto type_name = name.substr(index + 1);
			name = name.substr(0, index);

			trim(name);
			trim(type_name);

			if (type_name == "double")
				type = kDouble;
			else if (type_name == "int")
				type = kInt;
		}
		const double value = parser.Evaluate(option_value);
		vars.try_emplace(name, std::make_pair(type, value));
		parser.AddConstant(name, value);
	}
	return vars;
}

GraphicPack2::GraphicPack2(fs::path rulesPath, IniParser& rules)
	: m_rulesPath(std::move(rulesPath))
{
	// we're already in [Definition]
	auto option_version = rules.FindOption("version");
	if (!option_version)
		throw std::exception();
	m_version = StringHelpers::ToInt(*option_version, -1);
	if (m_version < 0)
	{
		cemuLog_log(LogType::Force, "{}: Invalid version", _pathToUtf8(m_rulesPath));
		throw std::exception();
	}

	auto option_rendererFilter = rules.FindOption("rendererFilter");
	if (option_rendererFilter)
	{
		if (boost::iequals(*option_rendererFilter, "vulkan"))
			m_renderer_api = RendererAPI::Vulkan;
		else if (boost::iequals(*option_rendererFilter, "opengl"))
			m_renderer_api = RendererAPI::OpenGL;
		else
			cemuLog_log(LogType::Force, "Unknown value '{}' for rendererFilter option", *option_rendererFilter);
	}

	auto option_defaultEnabled = rules.FindOption("default");
	if(option_defaultEnabled)
	{
		m_default_enabled = boost::iequals(*option_defaultEnabled, "true") || boost::iequals(*option_defaultEnabled, "1");
		m_enabled = m_default_enabled;
	}

	auto option_allowRendertargetSizeOptimization = rules.FindOption("colorbufferOptimizationAware");
	if (option_allowRendertargetSizeOptimization)
		m_allowRendertargetSizeOptimization = boost::iequals(*option_allowRendertargetSizeOptimization, "true") || boost::iequals(*option_allowRendertargetSizeOptimization, "1");

	auto option_vendorFilter = rules.FindOption("vendorFilter");
	if (option_vendorFilter)
	{
		if (boost::iequals(*option_vendorFilter, "amd"))
			m_gfx_vendor = GfxVendor::AMD;
		else if (boost::iequals(*option_vendorFilter, "intel"))
			m_gfx_vendor = GfxVendor::Intel;
		else if (boost::iequals(*option_vendorFilter, "mesa"))
			m_gfx_vendor = GfxVendor::Mesa;
		else if (boost::iequals(*option_vendorFilter, "nvidia"))
			m_gfx_vendor = GfxVendor::Nvidia;
		else if (boost::iequals(*option_vendorFilter, "apple"))
			m_gfx_vendor = GfxVendor::Apple;
		else
			cemuLog_log(LogType::Force, "Unknown value '{}' for vendorFilter", *option_vendorFilter);
	}

	auto option_path = rules.FindOption("path");
	if (!option_path)
	{
		auto gp_name_log = rules.FindOption("name");
		cemuLog_log(LogType::Force, "[Definition] section from '{}' graphic pack must contain option: path", gp_name_log.has_value() ? *gp_name_log : "Unknown");
		throw std::exception();
	}
	m_virtualPath = *option_path;

	auto option_gp_name = rules.FindOption("name");
	if (option_gp_name)
		m_name = *option_gp_name;

	auto option_description = rules.FindOption("description");
	if (option_description)
	{
		m_description = *option_description;
		std::replace(m_description.begin(), m_description.end(), '|', '\n');
	}

	m_title_ids = ParseTitleIds(rules, "titleIds");
	if(m_title_ids.empty())
		throw std::exception();

	auto option_fsPriority = rules.FindOption("fsPriority");
	if (option_fsPriority)
	{
		std::string tmp(*option_fsPriority);
		m_fs_priority = std::stoi(tmp);
	}

	// load presets
	while (rules.NextSection())
	{
		auto currentSectionName = rules.GetCurrentSectionName();
		if (boost::iequals(currentSectionName, "Default"))
		{
			m_preset_vars = ParsePresetVars(rules);
		}
		else if (boost::iequals(currentSectionName, "Preset"))
		{
			const auto preset_name = rules.FindOption("name");
			if (!preset_name)
			{
				cemuLog_log(LogType::Force, "Graphic pack \"{}\": Preset in line {} skipped because it has no name option defined", GetNormalizedPathString(), rules.GetCurrentSectionLineNumber());
				continue;
			}
			
			const auto category = rules.FindOption("category");
			const auto condition = rules.FindOption("condition");
			const auto default_selected = rules.FindOption("default");

			try
			{
				const auto vars = ParsePresetVars(rules);
				PresetPtr preset;
				if (category && condition)
					preset = std::make_shared<Preset>(*category, *preset_name, *condition, vars);
				else if (category)
					preset = std::make_shared<Preset>(*category, *preset_name, vars);
				else
					preset = std::make_shared<Preset>(*preset_name, vars);
				if (default_selected)
					preset->is_default = StringHelpers::ToInt(*default_selected) != 0;
				m_presets.emplace_back(preset);
			}
			catch (const std::exception & ex)
			{
				cemuLog_log(LogType::Force, "Graphic pack \"{}\": Can't parse preset \"{}\": {}", GetNormalizedPathString(), *preset_name, ex.what());
			}
		}
		else if (boost::iequals(currentSectionName, "RAM"))
		{
			for (uint32 i = 0; i < 32; i++)
			{
				char optionNameBuf[64];
				*fmt::format_to(optionNameBuf, "mapping{}", i) = '\0';
				const auto mappingOption = rules.FindOption(optionNameBuf);
				if (mappingOption)
				{
					if (m_version <= 5)
					{
						cemuLog_log(LogType::Force, "Graphic pack \"{}\": [RAM] options are only available for graphic pack version 6 or higher", GetNormalizedPathString(), optionNameBuf);
						throw std::exception();
					}

					StringTokenParser parser(*mappingOption);
					uint32 addrStart = 0, addrEnd = 0;
					if (parser.parseU32(addrStart) && parser.matchWordI("-") && parser.parseU32(addrEnd) && parser.isEndOfString())
					{
						if (addrEnd <= addrStart)
						{
							cemuLog_log(LogType::Force, "Graphic pack \"{}\": start address (0x{:08x}) must be greater than end address (0x{:08x}) for {}", GetNormalizedPathString(), addrStart, addrEnd, optionNameBuf);
							throw std::exception();
						}
						else if ((addrStart & 0xFFF) != 0 || (addrEnd & 0xFFF) != 0)
						{
							cemuLog_log(LogType::Force, "Graphic pack \"{}\": addresses for %s are not aligned to 0x1000", GetNormalizedPathString(), optionNameBuf);
							throw std::exception();
						}
						else
						{
							m_ramMappings.emplace_back(addrStart, addrEnd);
						}
					}
					else
					{
						cemuLog_log(LogType::Force, "Graphic pack \"{}\": has invalid syntax for option {}", GetNormalizedPathString(), optionNameBuf);
						throw std::exception();
					}
				}
			}
		}
	}

	if (m_version >= 5)
	{
		// store by category
		std::unordered_map<std::string, std::vector<PresetPtr>> tmp_map;
		
		// all vars must be defined in the default preset vars before
		std::vector<std::pair<std::string, std::string>> mismatchingPresetVars;
		for (const auto& presetEntry : m_presets)
		{
			tmp_map[presetEntry->category].emplace_back(presetEntry);
			
			for (auto& presetVar : presetEntry->variables)
			{
				const auto it = m_preset_vars.find(presetVar.first);
				if (it == m_preset_vars.cend())
				{
					mismatchingPresetVars.emplace_back(presetEntry->name, presetVar.first);
					continue;
				}
				// overwrite var type with default var type
				presetVar.second.first = it->second.first;
			}
		}

		if(!mismatchingPresetVars.empty())
		{
			cemuLog_log(LogType::Force, "Graphic pack \"{}\" contains preset variables which are not defined in the [Default] section:", GetNormalizedPathString());
			for (const auto& [presetName, varName] : mismatchingPresetVars)
				cemuLog_log(LogType::Force, "Preset: {} Variable: {}", presetName, varName);
			throw std::exception();
		}

		// have first entry be default active for every category if no default= is set
		for(auto entry : get_values(tmp_map))
		{
			if (!entry.empty())
			{
				const auto it = std::find_if(entry.cbegin(), entry.cend(), [](const PresetPtr& preset) { return preset->is_default; });
				if (it != entry.cend())
					(*it)->active = true;
				else
					(*entry.begin())->active = true;
			}
		}
	}
	else
	{
		// verify preset data to contain the same keys
		std::unordered_map<std::string, std::vector<PresetPtr>> tmp_map;
		for (const auto& entry : m_presets)
			tmp_map[entry->category].emplace_back(entry);

		for (const auto& kv : tmp_map)
		{
			bool has_default = false;
			for (size_t i = 0; i + 1 < kv.second.size(); ++i)
			{
				auto& p1 = kv.second[i];
				auto& p2 = kv.second[i + 1];
				if (p1->variables.size() != p2->variables.size())
				{
					cemuLog_log(LogType::Force, "Graphic pack: \"{}\" contains inconsistent preset variables", GetNormalizedPathString());
					throw std::exception();
				}

				std::set<std::string> keys1(get_keys(p1->variables).begin(), get_keys(p1->variables).end());
				std::set<std::string> keys2(get_keys(p2->variables).begin(), get_keys(p2->variables).end());
				if (keys1 != keys2)
				{
					cemuLog_log(LogType::Force, "Graphic pack: \"{}\" contains inconsistent preset variables", GetNormalizedPathString());
					throw std::exception();
				}

				if(p1->is_default)
				{
					if(has_default)
						cemuLog_log(LogType::Force, "Graphic pack: \"{}\" has more than one preset with the default key set for the same category \"{}\"", GetNormalizedPathString(), p1->name);
					p1->active = true;
					has_default = true;
				}
			}

			// have first entry by default active if no default is set
			if (!has_default)
				kv.second[0]->active = true;
		}
	}
}

// returns true if enabling, disabling (changeEnableState) or changing presets (changePreset) for the graphic pack requires restarting if the game is already running
bool GraphicPack2::RequiresRestart(bool changeEnableState, bool changePreset)
{
	if (!GetTextureRules().empty())
		return true;
	return false;
}

bool GraphicPack2::Reload()
{
	Deactivate();
	return Activate();
}

std::string GraphicPack2::GetNormalizedPathString() const
{
	return _pathToUtf8(MakeRelativePath(ActiveSettings::GetUserDataPath(), GetRulesPath()).lexically_normal());
}

bool GraphicPack2::ContainsTitleId(uint64_t title_id) const
{
	const auto it = std::find_if(m_title_ids.begin(), m_title_ids.end(), [title_id](uint64 id) { return id == title_id; });
	return it != m_title_ids.end();
}

bool GraphicPack2::HasActivePreset() const
{
	return std::any_of(m_presets.cbegin(), m_presets.cend(), [](const PresetPtr& preset)
		{
			return preset->active;
		});
}

std::string GraphicPack2::GetActivePreset(std::string_view category) const
{
	const auto it = std::find_if(m_presets.cbegin(), m_presets.cend(), [category](const PresetPtr& preset)
		{
			return preset->active && preset->category == category;
		});
	return it != m_presets.cend() ? (*it)->name : std::string{ "" };
}

void GraphicPack2::UpdatePresetVisibility()
{
	// update visiblity of each preset
	std::for_each(m_presets.begin(), m_presets.end(), [this](PresetPtr& p)
	{
		p->visible = m_version >= 5 ? IsPresetVisible(p) : true;
	});
}

void GraphicPack2::ValidatePresetSelections()
{
	if (m_version < 5)
		return; // only applies to new categorized presets

	// if any preset is changed then other categories might be affected indirectly
	//
	// example: selecting the aspect ratio in a resolution graphic pack would change the available presets in the resolution category
	// how to handle: select the first available resolution (or the one marked as default)
	//
	// example: a preset category might be hidden entirely (e.g. due to a separate advanced options dropdown)
	// how to handle: leave the previously selected preset
	// 
	// the logic is therefore as follows:
	// if there is a preset category with at least 1 visible preset entry then make sure one of those is actually selected
	// for completely hidden preset categories we leave the selection as-is

	std::vector<std::string> order;
	std::unordered_map<std::string, std::vector<GraphicPack2::PresetPtr>> categorizedPresets = GraphicPack2::GetCategorizedPresets(order);

	bool changedPresets = false;
	for (auto& categoryItr : categorizedPresets)
	{
		// get selection of this category
		size_t numVisiblePresets = 0;
		GraphicPack2::PresetPtr defaultSelection = nullptr;
		GraphicPack2::PresetPtr selectedPreset = nullptr;
		for (auto& presetItr : categoryItr.second)
		{
			if (presetItr->visible)
			{
				numVisiblePresets++;
				if (!defaultSelection || presetItr->is_default) // the preset marked as default becomes the default selection, otherwise pick first visible one
					defaultSelection = presetItr;
			}
			if (presetItr->active)
			{
				if (selectedPreset)
				{
					// multiple selections inside the same group are invalid
					presetItr->active = false;
					changedPresets = true;
				}
				else
					selectedPreset = presetItr;
			}
		}
		if (numVisiblePresets == 0)
			continue; // do not touch selection
		if (!selectedPreset)
		{
			// no selection at all
			if (defaultSelection)
			{
				selectedPreset = defaultSelection;
				selectedPreset->active = true;
			}
			continue;
		}
		// if the currently selected preset is invisible, update it to the preferred visible selection
		if (!selectedPreset->visible)
		{
			selectedPreset->active = false;
			defaultSelection->active = true;
			changedPresets = true;
		}
	}
	if (changedPresets)
		UpdatePresetVisibility();
}

bool GraphicPack2::SetActivePreset(std::string_view category, std::string_view name, bool update_visibility)
{
	// disable currently active preset
	std::for_each(m_presets.begin(), m_presets.end(), [category](PresetPtr& p)
	{
		if(p->category == category) 
			p->active = false;
	});
	
	if (name.empty())
		return true;
	
	// enable new preset
	const auto it = std::find_if(m_presets.cbegin(), m_presets.cend(), [category, name](const PresetPtr& preset)
		{
			return preset->category == category && preset->name == name; 
		});

	bool result;
	if (it != m_presets.cend())
	{
		(*it)->active = true;
		cemu_assert_debug(std::count_if(m_presets.cbegin(), m_presets.cend(), [category](const PresetPtr& p) { return p->category == category && p->active; }) == 1);
		result = true;
	}
	else
		result = false;

	if (update_visibility)
	{
		UpdatePresetVisibility();
		ValidatePresetSelections();
	}

	return result;
}

void GraphicPack2::LoadShaders()
{
	fs::path path = GetRulesPath();
	for (auto& it : fs::directory_iterator(path.remove_filename()))
	{
		if (!is_regular_file(it))
			continue;

		try
		{
			const auto& p = it.path();
			auto filename = p.filename().wstring();
			uint64 shader_base_hash = 0;
			uint64 shader_aux_hash = 0;
			wchar_t shader_type[256]{};
			if (filename.size() < 256 && swscanf(filename.c_str(), L"%" SCNx64 "_%" SCNx64 "_%ls", &shader_base_hash, &shader_aux_hash, shader_type) == 3)
			{
				if (shader_type[0] == 'p' && shader_type[1] == 's')
					m_custom_shaders.emplace_back(LoadShader(p, shader_base_hash, shader_aux_hash, GP_SHADER_TYPE::PIXEL));
				else if (shader_type[0] == 'v' && shader_type[1] == 's')
					m_custom_shaders.emplace_back(LoadShader(p, shader_base_hash, shader_aux_hash, GP_SHADER_TYPE::VERTEX));
				else if (shader_type[0] == 'g' && shader_type[1] == 's')
					m_custom_shaders.emplace_back(LoadShader(p, shader_base_hash, shader_aux_hash, GP_SHADER_TYPE::GEOMETRY));
			}
			else if (filename == L"output.glsl")
			{
				std::ifstream file(p);
				if (!file.is_open())
					throw std::runtime_error(fmt::format("can't open graphic pack file: {}", _pathToUtf8(p.filename())));

				file.seekg(0, std::ios::end);
				m_output_shader_source.reserve(file.tellg());
				file.seekg(0, std::ios::beg);

				m_output_shader_source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
				ApplyShaderPresets(m_output_shader_source);
			}
			else if (filename == L"upscaling.glsl")
			{
				std::ifstream file(p);
				if (!file.is_open())
					throw std::runtime_error(fmt::format("can't open graphic pack file: {}", _pathToUtf8(p.filename())));

				file.seekg(0, std::ios::end);
				m_upscaling_shader_source.reserve(file.tellg());
				file.seekg(0, std::ios::beg);

				m_upscaling_shader_source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
				ApplyShaderPresets(m_upscaling_shader_source);
			}
			else if (filename == L"downscaling.glsl")
			{
				std::ifstream file(p);
				if (!file.is_open())
					throw std::runtime_error(fmt::format("can't open graphic pack file: {}", _pathToUtf8(p.filename())));

				file.seekg(0, std::ios::end);
				m_downscaling_shader_source.reserve(file.tellg());
				file.seekg(0, std::ios::beg);

				m_downscaling_shader_source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
				ApplyShaderPresets(m_downscaling_shader_source);
			}
		}
		catch (const std::exception& ex)
		{
			cemuLog_log(LogType::Force, "graphicPack: error while loading custom shader: {}", ex.what());
		}
	}
}

bool GraphicPack2::SetActivePreset(std::string_view name)
{
	return SetActivePreset("", name);
}

bool GraphicPack2::IsPresetVisible(const PresetPtr& preset) const
{
	if (preset->condition.empty())
		return true;

	try
	{
		TExpressionParser<int> p;
		FillPresetConstants(p);
		return p.Evaluate(preset->condition) != 0;
	}
	catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "error when trying to check visiblity of preset: {}", ex.what());
		return false;
	}
}

std::optional<GraphicPack2::PresetVar> GraphicPack2::GetPresetVariable(const std::vector<PresetPtr>& presets, std::string_view var_name) const
{
	// no priority and visibility filter
	if(m_version < 5)
	{
		for (const auto& preset : presets)
		{
			const auto it = std::find_if(preset->variables.cbegin(), preset->variables.cend(), [&var_name](auto p) { return p.first == var_name; });
			if (it != preset->variables.cend())
				return it->second;
		}

		return {};
	}

	// visible > none visible > default
	for (const auto& preset : presets)
	{
		if (preset->visible)
		{
			const auto it = std::find_if(preset->variables.cbegin(), preset->variables.cend(), [&var_name](auto p) { return p.first == var_name; });
			if (it != preset->variables.cend())
				return it->second;
		}
	}
	
	for (const auto& preset : presets)
	{
		if (!preset->visible)
		{
			const auto it = std::find_if(preset->variables.cbegin(), preset->variables.cend(), [&var_name](auto p) { return p.first == var_name; });
			if (it != preset->variables.cend())
				return it->second;
		}
	}
	
	const auto it = std::find_if(m_preset_vars.cbegin(), m_preset_vars.cend(), [&var_name](auto p) { return p.first == var_name; });
	if (it != m_preset_vars.cend())
	{
		return it->second;
	}

	return {};
}

void GraphicPack2::AddConstantsForCurrentPreset(ExpressionParser& ep)
{
	if (m_version < 5)
	{
		for (const auto& preset : GetActivePresets())
		{
			for (auto& v : preset->variables)
			{
				ep.AddConstant(v.first, v.second.second);
			}
		}
	}
	else
	{
		FillPresetConstants(ep);
	}
}

void GraphicPack2::_iterateReplacedFiles(const fs::path& currentPath, bool isAOC)
{
	uint64 currentTitleId = CafeSystem::GetForegroundTitleId();
	uint64 aocTitleId = (currentTitleId & 0xFFFFFFFFull) | 0x0005000c00000000ull;
	for (auto& it : fs::recursive_directory_iterator(currentPath))
	{
		if (fs::is_regular_file(it))
		{
			fs::path virtualMountPath = fs::relative(it.path(), currentPath);
			if (isAOC)
			{
				virtualMountPath = fs::path(fmt::format("/vol/aoc{:016x}/", aocTitleId)) / virtualMountPath;
			}
			else
			{
				virtualMountPath = fs::path("vol/content/") / virtualMountPath;
			}
			fscDeviceRedirect_add(virtualMountPath.generic_string(), it.file_size(), it.path().generic_string(), m_fs_priority);
		}		
	}
}

void GraphicPack2::LoadReplacedFiles()
{
	if (m_patchedFilesLoaded)
		return;
	m_patchedFilesLoaded = true;

	fs::path gfxPackPath = GetRulesPath();
	gfxPackPath = gfxPackPath.remove_filename();

	// /content/
	fs::path contentPath(gfxPackPath);
	contentPath.append("content");

	std::error_code ec;
	if (fs::exists(contentPath, ec))
	{
		// setup redirections	
		fscDeviceRedirect_map();
		_iterateReplacedFiles(contentPath, false);
	}
	// /aoc/
	fs::path aocPath(gfxPackPath);
	aocPath.append("aoc");

	if (fs::exists(aocPath, ec))
	{
		uint64 aocTitleId = CafeSystem::GetForegroundTitleId();
		aocTitleId = aocTitleId & 0xFFFFFFFFULL;
		aocTitleId |= 0x0005000c00000000ULL;
		// setup redirections	
		fscDeviceRedirect_map();
		_iterateReplacedFiles(aocPath, true);
	}
}

bool GraphicPack2::Activate()
{
	if (m_activated)
		return true;

	// check if gp should be loaded
	if (m_renderer_api.has_value() && m_renderer_api.value() != g_renderer->GetType())
		return false;

	if (m_gfx_vendor.has_value())
	{
		auto vendor = g_renderer->GetVendor();
		if (m_gfx_vendor.value() != vendor)
			return false;
	}

	FileStream* fs_rules = FileStream::openFile2(m_rulesPath);
	if (!fs_rules)
		return false;
	std::vector<uint8> rulesData;
	fs_rules->extract(rulesData);
	delete fs_rules;

	IniParser rules({ (char*)rulesData.data(), rulesData.size()}, GetNormalizedPathString());

	// load rules
	try
	{
		ExpressionParser parser;
		AddConstantsForCurrentPreset(parser);

		while (rules.NextSection())
		{
			//const char* category_name = sPref_currentCategoryName(rules);
			std::string_view category_name = rules.GetCurrentSectionName();
			if (boost::iequals(category_name, "TextureRedefine"))
			{
				TextureRule rule{};
				ParseRule(parser, rules, "width", &rule.filter_settings.width);
				ParseRule(parser, rules, "height", &rule.filter_settings.height);
				ParseRule(parser, rules, "depth", &rule.filter_settings.depth);

				bool inMem1 = false;
				if (ParseRule(parser, rules, "inMEM1", &inMem1))
					rule.filter_settings.inMEM1 = inMem1 ? TextureRule::FILTER_SETTINGS::MEM1_FILTER::INSIDE : TextureRule::FILTER_SETTINGS::MEM1_FILTER::OUTSIDE;

				rule.filter_settings.format_whitelist = ParseList<sint32>(parser, rules, "formats");
				rule.filter_settings.format_blacklist = ParseList<sint32>(parser, rules, "formatsExcluded");
				rule.filter_settings.tilemode_whitelist = ParseList<sint32>(parser, rules, "tilemodes");
				rule.filter_settings.tilemode_blacklist = ParseList<sint32>(parser, rules, "tilemodesExcluded");

				ParseRule(parser, rules, "overwriteWidth", &rule.overwrite_settings.width);
				ParseRule(parser, rules, "overwriteHeight", &rule.overwrite_settings.height);
				ParseRule(parser, rules, "overwriteDepth", &rule.overwrite_settings.depth);
				ParseRule(parser, rules, "overwriteFormat", &rule.overwrite_settings.format);

				float lod_bias;
				if(ParseRule(parser, rules, "overwriteLodBias", &lod_bias))
					rule.overwrite_settings.lod_bias = (sint32)(lod_bias * 64.0f);

				if(ParseRule(parser, rules, "overwriteRelativeLodBias", &lod_bias))
					rule.overwrite_settings.relative_lod_bias = (sint32)(lod_bias * 64.0f);

				sint32 anisotropyValue;
				if (ParseRule(parser, rules, "overwriteAnisotropy", &anisotropyValue))
				{
					if (anisotropyValue == 1)
						rule.overwrite_settings.anistropic_value = 0;
					else if (anisotropyValue == 2)
						rule.overwrite_settings.anistropic_value = 1;
					else if (anisotropyValue == 4)
						rule.overwrite_settings.anistropic_value = 2;
					else if (anisotropyValue == 8)
						rule.overwrite_settings.anistropic_value = 3;
					else if (anisotropyValue == 16)
						rule.overwrite_settings.anistropic_value = 4;
					else
						cemuLog_log(LogType::Force, "Invalid value {} for overwriteAnisotropy in graphic pack {}. Only the values 1, 2, 4, 8 or 16 are allowed.", anisotropyValue, GetNormalizedPathString());
				}
				m_texture_rules.emplace_back(rule);
			}
			else if (boost::iequals(category_name, "Control"))
			{
				ParseRule(parser, rules, "vsyncFrequency", &m_vsync_frequency);
			}
			else if (boost::iequals(category_name, "OutputShader"))
			{
				auto option_upscale = rules.FindOption("upscaleMagFilter");
				if(option_upscale && boost::iequals(*option_upscale, "NearestNeighbor"))
					m_output_settings.upscale_filter = LatteTextureView::MagFilter::kNearestNeighbor;
				auto option_downscale = rules.FindOption("downscaleMinFilter");
				if (option_downscale && boost::iequals(*option_downscale, "NearestNeighbor"))
					m_output_settings.downscale_filter = LatteTextureView::MagFilter::kNearestNeighbor;
			}
		}
	}
	catch(const std::exception& ex)
	{
		cemuLog_log(LogType::Force, ex.what());
		return false;
	}

	// load shaders
	LoadShaders();

	// load patches
	LoadPatchFiles();

	// enable patch groups
	EnablePatches();
	
	// load replaced files
	LoadReplacedFiles();

	// set custom vsync
	if (HasCustomVSyncFrequency())
	{
		sint32 customVsyncFreq = GetCustomVSyncFrequency();
		sint32 globalCustomVsyncFreq = 0;
		if (LatteTiming_getCustomVsyncFrequency(globalCustomVsyncFreq))
		{
			if (customVsyncFreq != globalCustomVsyncFreq)
				cemuLog_log(LogType::Force, "rules.txt error: Mismatching vsync frequency {} in graphic pack \'{}\'", customVsyncFreq, GetVirtualPath());
		}
		else
		{
			cemuLog_log(LogType::Force, "Set vsync frequency to {} (graphic pack {})", customVsyncFreq, GetVirtualPath());
			LatteTiming_setCustomVsyncFrequency(customVsyncFreq);
		}
	}
	m_activated = true;
	return true;
}

bool GraphicPack2::Deactivate()
{
	if (!m_activated)
		return false;

	UnloadPatches();

	m_activated = false;
	m_custom_shaders.clear();
	m_texture_rules.clear();

	m_output_shader.reset();
	m_upscaling_shader.reset();
	m_downscaling_shader.reset();

	m_output_shader_ud.reset();
	m_upscaling_shader_ud.reset();
	m_downscaling_shader_ud.reset();

	m_output_shader_source.clear();
	m_upscaling_shader_source.clear();
	m_downscaling_shader_source.clear();
	
	if (HasCustomVSyncFrequency())
	{
		m_vsync_frequency = -1;

		LatteTiming_disableCustomVsyncFrequency();
	}

	return true;
}

const std::string* GraphicPack2::FindCustomShaderSource(uint64 shaderBaseHash, uint64 shaderAuxHash, GP_SHADER_TYPE type, bool isVulkanRenderer)
{
	for (const auto& gp : GraphicPack2::GetActiveGraphicPacks())
	{
		const auto it = std::find_if(gp->m_custom_shaders.begin(), gp->m_custom_shaders.end(),
			[shaderBaseHash, shaderAuxHash, type](const auto& s) { return s.shader_base_hash == shaderBaseHash && s.shader_aux_hash == shaderAuxHash && s.type == type; });

		if (it == gp->m_custom_shaders.end())
			continue;

		if(isVulkanRenderer && (*it).isPreVulkanShader)
			continue;

		return &it->source;
	}
	return nullptr;
}

std::unordered_map<std::string, std::vector<GraphicPack2::PresetPtr>> GraphicPack2::GetCategorizedPresets(std::vector<std::string>& order) const
{
	order.clear();
	
	std::unordered_map<std::string, std::vector<PresetPtr>> result;
	for(const auto& entry : m_presets)
	{
		result[entry->category].emplace_back(entry);
		const auto it = std::find(order.cbegin(), order.cend(), entry->category);
		if (it == order.cend())
			order.emplace_back(entry->category);
	}
	
	return result;
}

bool GraphicPack2::HasShaders() const
{
	return !GetCustomShaders().empty() 
	|| !m_output_shader_source.empty() || !m_upscaling_shader_source.empty() || !m_downscaling_shader_source.empty();
}

RendererOutputShader* GraphicPack2::GetOuputShader(bool render_upside_down)
{
	if(render_upside_down)
	{
		if (m_output_shader_ud)
			return m_output_shader_ud.get();

		if (!m_output_shader_source.empty())
			m_output_shader_ud = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_output_shader_source);

		return m_output_shader_ud.get();
	}
	else
	{
		if (m_output_shader)
			return m_output_shader.get();

		if (!m_output_shader_source.empty())
			m_output_shader = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_output_shader_source);

		return m_output_shader.get();
	}
}

RendererOutputShader* GraphicPack2::GetUpscalingShader(bool render_upside_down)
{
	if (render_upside_down)
	{
		if (m_upscaling_shader_ud)
			return m_upscaling_shader_ud.get();

		if (!m_upscaling_shader_source.empty())
			m_upscaling_shader_ud = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_upscaling_shader_source);

		return m_upscaling_shader_ud.get();
	}
	else
	{
		if (m_upscaling_shader)
			return m_upscaling_shader.get();

		if (!m_upscaling_shader_source.empty())
			m_upscaling_shader = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_upscaling_shader_source);

		return m_upscaling_shader.get();
	}
}

RendererOutputShader* GraphicPack2::GetDownscalingShader(bool render_upside_down)
{
	if (render_upside_down)
	{
		if (m_downscaling_shader_ud)
			return m_downscaling_shader_ud.get();

		if (!m_downscaling_shader_source.empty())
			m_downscaling_shader_ud = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_downscaling_shader_source);

		return m_downscaling_shader_ud.get();
	}
	else
	{
		if (m_downscaling_shader)
			return m_downscaling_shader.get();

		if (!m_downscaling_shader_source.empty())
			m_downscaling_shader = std::make_unique<RendererOutputShader>(RendererOutputShader::GetOpenGlVertexSource(render_upside_down), m_downscaling_shader_source);

		return m_downscaling_shader.get();
	}
}


std::vector<GraphicPack2::PresetPtr> GraphicPack2::GetActivePresets() const
{
	std::vector<PresetPtr> result;
	result.reserve(m_presets.size());
	std::copy_if(m_presets.cbegin(), m_presets.cend(), std::back_inserter(result), [](const PresetPtr& p) { return p->active; });
	return result;
}

std::vector<uint64> GraphicPack2::ParseTitleIds(IniParser& rules, const char* option_name) const
{
	std::vector<uint64> result;

	auto option_text = rules.FindOption(option_name);
	if (!option_text)
		return result;

	for (auto& token : TokenizeView(*option_text, ','))
	{
		try
		{
			result.emplace_back(ConvertString<uint64>(token, 16));
		}
		catch (const std::invalid_argument&) {}
	}

	return result;
}

void GraphicPack2::ApplyShaderPresets(std::string& shader_source) const
{
	const auto active_presets = GetActivePresets();
	const std::regex regex(R"(\$[a-zA-Z_0-9]+)");

	std::smatch match;
	size_t offset = 0;
	while (std::regex_search(shader_source.cbegin() + offset, shader_source.cend(), match, regex))
	{
		if (active_presets.empty())
			throw std::runtime_error("found variable in shader but no preset is active");

		const auto str = match.str();

		std::optional<PresetVar> var = GetPresetVariable(active_presets, str);
		if(!var)
			throw std::runtime_error("using an unknown preset variable in shader");

		std::string new_value;
		if (var->first == kInt)
			new_value = fmt::format("{}", (int)var->second);
		else
			new_value = fmt::format("{:f}", var->second);

		shader_source.replace(match.position() + offset, match.length(), new_value);
		offset += match.position() + new_value.length();
	}
}

GraphicPack2::CustomShader GraphicPack2::LoadShader(const fs::path& path, uint64 shader_base_hash, uint64 shader_aux_hash, GP_SHADER_TYPE shader_type) const
{
	CustomShader shader;

	std::ifstream file(path);
	if (!file.is_open())
		throw std::runtime_error("can't open shader file");

	file.seekg(0, std::ios::end);
	shader.source.reserve(file.tellg());
	file.seekg(0, std::ios::beg);

	shader.source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	ApplyShaderPresets(shader.source);

	shader.shader_base_hash = shader_base_hash;
	shader.shader_aux_hash = shader_aux_hash;
	shader.type = shader_type;
	shader.isPreVulkanShader = this->m_version <= 3;

	return shader;
}

std::vector<std::pair<MPTR, MPTR>> GraphicPack2::GetActiveRAMMappings()
{
	uint64 currentTitleId = CafeSystem::GetForegroundTitleId();
	std::vector<std::pair<MPTR, MPTR>> v;
	for (const auto& gp : GraphicPack2::GetGraphicPacks())
	{
		if (!gp->IsEnabled())
			continue;
		if (!gp->ContainsTitleId(currentTitleId))
			continue;
		if (!gp->m_ramMappings.empty())
			v.insert(v.end(), gp->m_ramMappings.begin(), gp->m_ramMappings.end());
	}
	std::sort(v.begin(), v.end(),
		[](const std::pair<MPTR, MPTR>& a, const std::pair<MPTR, MPTR>& b) -> bool
		{
			return a.first < b.first;
		});
	return v;
}
