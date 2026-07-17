#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Cemu/Logging/CemuLogging.h"
#include "Common/FileStream.h"
#include "WindowSystem.h"
#include "util/helpers/StringParser.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "boost/algorithm/string.hpp"

// error handler
void PatchErrorHandler::printError(class PatchGroup* patchGroup, sint32 lineNumber, std::string_view errorMsg)
{
	if (m_anyErrorTriggered == false)
	{
		// stage error msg
		cemu_assert(m_gp);
		if (m_stage == STAGE::PARSER)
			cemuLog_writeLineToLog(fmt::format("An error occurred while trying to parse the patches for graphic pack \'{}\'", m_gp->GetName()), true, true);
		else if (m_stage == STAGE::APPLY)
			cemuLog_writeLineToLog(fmt::format("An error occurred while trying to apply the patches for graphic pack \'{}\'", m_gp->GetName()), true, true);
	}

	std::string msg;
	if (patchGroup == nullptr && lineNumber >= 0)
		msg.append(fmt::format("[Line {}] ", lineNumber));
	else if (patchGroup && lineNumber >= 0)
		msg.append(fmt::format("[{}, Line {}] ", patchGroup->getName(), lineNumber));
	else if (patchGroup && lineNumber < 0)
		msg.append(fmt::format("[{}] ", patchGroup->getName()));

	msg.append(errorMsg);

	cemuLog_writeLineToLog(msg, true, true);
	m_anyErrorTriggered = true;

	if (cemuLog_isLoggingEnabled(LogType::Patches))
		errorMessages.emplace_back(msg);
}

void PatchErrorHandler::showStageErrorMessageBox()
{
	std::string errorMsg;
	if (m_gp)
	{
		if (m_stage == STAGE::PARSER)
			errorMsg.assign(_tr("Failed to load patches for graphic pack \'{}\'", m_gp->GetName()));
		else
			errorMsg.assign(_tr("Failed to apply patches for graphic pack \'{}\'", m_gp->GetName()));
	}
	else
	{
		cemu_assert_debug(false); // graphic pack should always be set
	}
	if (cemuLog_isLoggingEnabled(LogType::Patches))
	{
		errorMsg.append("\n \n")
			.append(_tr("Details:"))
			.append("\n");
		for (auto& itr : errorMessages)
		{
			errorMsg.append(itr);
			errorMsg.append("\n");
		}
	}

	WindowSystem::ShowErrorDialog(errorMsg, _tr("Graphic pack error"), WindowSystem::ErrorCategory::GRAPHIC_PACKS);
}

// loads Cemu-style patches (patch_<anything>.cpb or patch_<anything>.asm)
// returns true if at least one file was found even if it could not be successfully parsed
bool GraphicPack2::LoadCemuPatches()
{
	static constexpr size_t kMaximumPatchFileSize = 64ULL * 1024ULL * 1024ULL;
	fs::path path(m_rulesPath);
	path.remove_filename();

	auto loadPatchFilesByExtension = [&](const char* extension, bool isBinaryPatch) -> bool
	{
		bool foundPatches = false;
		for (auto& p : fs::directory_iterator(path))
		{
			auto& patchPath = p.path();
			if (fs::is_regular_file(p.status()) && patchPath.has_filename())
			{
				// check if filename matches
				std::string filename = _pathToUtf8(patchPath.filename());
				if (boost::istarts_with(filename, "patch_") && boost::iends_with(filename, extension))
				{
					FileStream* patchFile = FileStream::openFile2(patchPath);
					if (patchFile)
					{
						// read file
						std::vector<uint8> fileData;
						patchFile->extract(fileData);
						delete patchFile;
						if (fileData.empty() || fileData.size() > kMaximumPatchFileSize)
						{
							cemuLog_log(LogType::Force,
								"Patch file \"{}\" is empty or exceeds the 64 MiB limit. No patches for this graphic pack will be applied.",
								_pathToUtf8(patchPath));
							list_patchGroups.clear();
							return true;
						}
						MemStreamReader patchesStream(fileData.data(), (sint32)fileData.size());
						if (isBinaryPatch && fileData.size() >= sizeof(uint32))
						{
							uint32 encodedMagic{};
							std::memcpy(&encodedMagic, fileData.data(), sizeof(encodedMagic));
							const auto magic = _swapEndianU32(encodedMagic);
							const auto expected = m_patchFormat == PatchFormat::Cpb2 ? 0x43504232U : 0x43504231U;
							if (magic != expected)
							{
								cemuLog_log(LogType::Force, "Patch file '{}' does not match rules.txt patchFormat",
									_pathToUtf8(patchPath));
								list_patchGroups.clear();
								return true;
							}
						}
						// load Cemu style patch file
						const bool parseResult = isBinaryPatch ? ParseCemuBinaryPatchesInternal(patchesStream) : ParseCemuPatchesTxtInternal(patchesStream);
						if (!parseResult)
						{
							cemuLog_log(LogType::Force, "Error while processing \"{}\". No patches for this graphic pack will be applied.", _pathToUtf8(patchPath));
							cemu_assert_debug(list_patchGroups.empty());
							return true; // return true since a patch was found even if we could not parse it
						}
					}
					else
					{
						cemuLog_log(LogType::Force, "Unable to load patch file \"{}\"", _pathToUtf8(patchPath));
					}
					foundPatches = true;
				}
			}
		}
		return foundPatches;
	};

	if (m_patchFormat == PatchFormat::Cpb1 || m_patchFormat == PatchFormat::Cpb2)
		return loadPatchFilesByExtension(".cpb", true);
	if (m_patchFormat == PatchFormat::Asm)
		return loadPatchFilesByExtension(".asm", false);
	return false;
}

void GraphicPack2::LoadPatchFiles()
{
	fs::path directory(m_rulesPath); directory.remove_filename();
	if (m_patchFormat == PatchFormat::Unspecified)
	{
		bool hasPatch = fs::exists(directory / "patches.txt");
		for (std::error_code error; !hasPatch && !error;)
		{
			for (fs::directory_iterator iterator(directory, error), end; !error && iterator != end; ++iterator)
			{
				const auto name = _pathToUtf8(iterator->path().filename());
				if (boost::istarts_with(name, "patch_") &&
					(boost::iends_with(name, ".cpb") || boost::iends_with(name, ".asm"))) { hasPatch = true; break; }
			}
			break;
		}
		if (hasPatch)
		{
			const auto message = fmt::format("Graphic pack '{}' contains native patches but rules.txt has no patchFormat. The pack was disabled fail-closed.", GetName());
			cemuLog_log(LogType::Force, message);
			WindowSystem::ShowErrorDialog(message, _tr("Graphic pack error"), WindowSystem::ErrorCategory::GRAPHIC_PACKS);
			list_patchGroups.clear();
		}
		return;
	}
	if (m_patchFormat == PatchFormat::Cpb1 || m_patchFormat == PatchFormat::Cpb2 ||
		m_patchFormat == PatchFormat::Asm)
	{
		static std::unordered_set<std::string> warned;
		if (warned.emplace(GetNormalizedPathString()).second)
		{
			const auto warning = fmt::format("Graphic pack '{}' contains trusted native PPC patches. It is not isolated and cannot open a CemuExtend ABI 2 session.", GetName());
			cemuLog_log(LogType::Force, warning);
			WindowSystem::ShowErrorDialog(warning, _tr("Trusted native graphic pack"), WindowSystem::ErrorCategory::GRAPHIC_PACKS);
		}
	}
	// order of loading patches:
	// 1) Load Cemu binary patches (patch_<name>.cpb), stop here if at least one patch file exists
	// 2) Load Cemu-style patches (patch_<name>.asm), stop here if at least one patch file exists
	// 3) Load Cemuhook patches.txt
	if (m_patchFormat != PatchFormat::Cemuhook && LoadCemuPatches())
		return; // exit if at least one Cemu patch file was found
	// fall back to Cemuhook patches.txt to guarantee backward compatibility
	if (m_patchFormat != PatchFormat::Cemuhook)
		return;
	fs::path path(m_rulesPath);
	path.remove_filename();
	path.append("patches.txt");
	FileStream* patchFile = FileStream::openFile2(path);
	if (patchFile == nullptr)
		return;
	// read file
	std::vector<uint8> fileData;
	patchFile->extract(fileData);
	delete patchFile;
	cemu_assert_debug(list_patchGroups.empty());
	// parse
	MemStreamReader patchesStream(fileData.data(), (sint32)fileData.size());
	ParseCemuhookPatchesTxtInternal(patchesStream);
}

void GraphicPack2::EnablePatches()
{
	std::lock_guard<std::recursive_mutex> lock(mtx_patches);
	for (auto& itr : list_modules)
		ApplyPatchesForModule(itr);
}

void GraphicPack2::UnloadPatches()
{
	if (list_patchGroups.empty())
		return;
	std::lock_guard<std::recursive_mutex> lock(mtx_patches);
	// if any patch groups were applied then revert here
	// do this by calling RevertPatchesForModule for every module?
	for (auto& itr : list_modules)
		RevertPatchesForModule(itr);
	// delete all patches
	for (auto itr : list_patchGroups)
		delete itr;
	list_patchGroups.clear();
}

bool GraphicPack2::HasPatches()
{
	return !list_patchGroups.empty();
}

const std::vector<PatchGroup*>& GraphicPack2::GetPatchGroups() {
	return list_patchGroups;
}

void GraphicPack2::ApplyPatchesForModule(const RPLModule* rpl)
{
	if (list_patchGroups.empty())
		return;
	// gather list of all patch groups that apply to this module
	std::vector<PatchGroup*> list_groups;
	for (auto itr : list_patchGroups)
	{
		if (itr->matchesCRC(rpl->patchCRC) || (itr->m_isRpxOnlyTarget && rpl->IsRPX()))
			list_groups.emplace_back(itr);
	}
	// apply all groups at once
	if (!list_groups.empty())
		ApplyPatchGroups(list_groups, rpl);
}

void GraphicPack2::RevertPatchesForModule(const RPLModule* rpl)
{
	if (list_patchGroups.empty())
		return;
	// gather list of all patch groups that apply to this module
	std::vector<PatchGroup*> list_groups;
	for (auto itr : list_patchGroups)
	{
		if (itr->matchesCRC(rpl->patchCRC) || (itr->m_isRpxOnlyTarget && rpl->IsRPX()))
			list_groups.emplace_back(itr);
	}
	// undo all groups at once
	if (!list_groups.empty())
		UndoPatchGroups(list_groups, rpl);
}

std::recursive_mutex GraphicPack2::mtx_patches;
std::vector<const RPLModule*> GraphicPack2::list_modules;
