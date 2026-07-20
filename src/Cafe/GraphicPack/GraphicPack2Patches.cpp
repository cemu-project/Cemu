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

// Loads upstream Cemu-style patch_<anything>.asm files and patches.txt.
// Returns true if at least one file was found even if parsing failed.
bool GraphicPack2::LoadCemuPatches()
{
	fs::path path(m_rulesPath);
	path.remove_filename();
	bool foundPatches = false;
	for (auto& entry : fs::directory_iterator(path))
	{
		const auto& patchPath = entry.path();
		if (!fs::is_regular_file(entry.status()) || !patchPath.has_filename())
			continue;
		const auto filename = _pathToUtf8(patchPath.filename());
		if (!boost::istarts_with(filename, "patch_") || !boost::iends_with(filename, ".asm"))
			continue;
		foundPatches = true;
		FileStream* patchFile = FileStream::openFile2(patchPath);
		if (!patchFile)
		{
			cemuLog_log(LogType::Force, "Unable to load patch file \"{}\"", _pathToUtf8(patchPath));
			continue;
		}
		std::vector<uint8> fileData;
		patchFile->extract(fileData);
		delete patchFile;
		if (fileData.empty() || fileData.size() > 64ULL * 1024ULL * 1024ULL)
		{
			cemuLog_log(LogType::Force,
				"Patch file \"{}\" is empty or exceeds 64 MiB. No patches for this graphic pack will be applied.",
				_pathToUtf8(patchPath));
			list_patchGroups.clear();
			return true;
		}
		MemStreamReader patchesStream(fileData.data(), static_cast<sint32>(fileData.size()));
		if (!ParseCemuPatchesTxtInternal(patchesStream))
		{
			cemuLog_log(LogType::Force,
				"Error while processing \"{}\". No patches for this graphic pack will be applied.",
				_pathToUtf8(patchPath));
			cemu_assert_debug(list_patchGroups.empty());
			return true;
		}
	}
	return foundPatches;
}

void GraphicPack2::LoadPatchFiles()
{
	// Preserve the upstream text formats: patch_*.asm takes precedence over
	// Cemuhook patches.txt. There is no binary-patch fallback.
	if (LoadCemuPatches())
		return;
	fs::path path(m_rulesPath);
	path.remove_filename();
	path.append("patches.txt");
	FileStream* patchFile = FileStream::openFile2(path);
	if (!patchFile)
		return;
	std::vector<uint8> fileData;
	patchFile->extract(fileData);
	delete patchFile;
	cemu_assert_debug(list_patchGroups.empty());
	MemStreamReader patchesStream(fileData.data(), static_cast<sint32>(fileData.size()));
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
