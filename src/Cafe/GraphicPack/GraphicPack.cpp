#include "gui/wxgui.h"
#include "GraphicPack.h"

#include "config/ActiveSettings.h"
#include "Cafe/GraphicPack/GraphicPack2.h"

typedef struct
{
	int placeholder;
}graphicPack_t;

// scans the graphic pack directory for shaders
__declspec(dllexport) void graphicPack_loadGraphicPackShaders(graphicPack_t* gp, wchar_t* graphicPackPath)
{
	// this function is part of the deprecated/removed v1 graphic pack code
	// as of Cemuhook 0.5.7.3 this function must exist with a minimum length for detour
	// otherwise Cemuhook graphic pack stuff will error out, so we just create some pointless instructions which wont be optimized away
	forceLog_printf("STUB1");
	forceLog_printf("STUB2");
	forceLog_printf("STUB3");
}

// for cemuhook compatibility only
__declspec(dllexport) bool config_isGraphicPackEnabled(uint64 id)
{
	forceLog_printf("STUB4");
	forceLog_printf("STUB5");
	forceLog_printf("STUB6");
	return false;
}

/*
 * Loads the graphic pack if the titleId is referenced in rules.ini
 */
void graphicPack_loadGraphicPack(wchar_t* graphicPackPath)
{
	fs::path rulesPath = fs::path(graphicPackPath);
	rulesPath.append("rules.txt");
	std::unique_ptr<FileStream> fs_rules(FileStream::openFile2(rulesPath));
	if (!fs_rules)
		return;
	std::vector<uint8> rulesData;
	fs_rules->extract(rulesData);
	IniParser iniParser(rulesData, rulesPath.string());

	if (!iniParser.NextSection())
	{
		cemuLog_force(u8"{}: Does not contain any sections", rulesPath.generic_u8string());
		return;
	}
	if (!boost::iequals(iniParser.GetCurrentSectionName(), "Definition"))
	{
		cemuLog_force(u8"{}: [Definition] must be the first section", rulesPath.generic_u8string());
		return;
	}


	auto option_version = iniParser.FindOption("version");
	if (option_version)
	{
		sint32 versionNum = -1;
		auto [ptr, ec] = std::from_chars(option_version->data(), option_version->data() + option_version->size(), versionNum);
		if (ec != std::errc{})
		{
			cemuLog_force(u8"{}: Unable to parse version", rulesPath.generic_u8string());
			return;
		}

		if (versionNum > GP_LEGACY_VERSION)
		{
			GraphicPack2::LoadGraphicPack(rulesPath.generic_wstring(), iniParser);
			return;
		}
	}
	cemuLog_force(u8"{}: Outdated graphic pack", rulesPath.generic_u8string());
}

void graphicPack_scanForGFXPackFolders(const fs::path& currentPath, std::wstring& relativePath)
{
	// check if this directory has rules txt
	fs::path rulesPath = fs::path(currentPath);
	rulesPath.append("rules.txt");

	if (fs::exists(rulesPath) && relativePath.length() != 0)
	{
		graphicPack_loadGraphicPack((wchar_t*)currentPath.generic_wstring().c_str());
		return; // when a rules.txt file is found stop recursion
	}
	
	if (!fs::exists(currentPath))
		return;
	
	for (auto& p : fs::directory_iterator(currentPath))
	{
		auto& path = p.path();
		if (fs::is_directory(p.status()))
		{
			// dir
			sint32 origSize = relativePath.size();
			relativePath.append(L"/");
			relativePath.append(path.filename().generic_wstring());
			graphicPack_scanForGFXPackFolders(path, relativePath);
			relativePath.resize(origSize);
		}
	}
}

void graphicPack_loadAll()
{
	// recursively iterate all directories in graphicPacks/ folder
	std::wstring graphicPackRelativePath;
	graphicPack_scanForGFXPackFolders(ActiveSettings::GetPath("graphicPacks/"), graphicPackRelativePath);
}

void graphicPack_activateForCurrentTitle(uint64 titleId)
{
	// activate graphic packs
	for (const auto& gp : GraphicPack2::GetGraphicPacks())
	{
		if (!gp->IsEnabled())
			continue;

		if (!gp->ContainsTitleId(titleId))
			continue;

		if(GraphicPack2::ActivateGraphicPack(gp))
		{
			if (gp->GetPresets().empty())
			{
				forceLog_printf("Activate graphic pack: %s", gp->GetPath().c_str());
			}
			else
			{
				std::string logLine;
				logLine.assign(fmt::format("Activate graphic pack: {} [Presets: ", gp->GetPath()));
				bool isFirst = true;
				for (auto& itr : gp->GetPresets())
				{
					if(!itr->active)
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
}