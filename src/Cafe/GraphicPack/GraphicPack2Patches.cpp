#include "Cafe/GraphicPack/GraphicPack2.h"
#include "Common/FileStream.h"
#include "util/helpers/StringParser.h"
#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/OS/RPL/rpl_structs.h"
#include "boost/algorithm/string.hpp"

#include "gui/wxgui.h" // for wxMessageBox

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

	if (cafeLog_isLoggingFlagEnabled(LOG_TYPE_PATCHES))
		errorMessages.emplace_back(msg);
}

void PatchErrorHandler::showStageErrorMessageBox()
{
	std::string errorMsg;
	if (m_gp)
	{
		if (m_stage == STAGE::PARSER)
			errorMsg.assign(fmt::format("Failed to load patches for graphic pack \'{}\'", m_gp->GetName()));
		else
			errorMsg.assign(fmt::format("Failed to apply patches for graphic pack \'{}\'", m_gp->GetName()));
	}
	else
	{
		cemu_assert_debug(false); // graphic pack should always be set
	}
	if (cafeLog_isLoggingFlagEnabled(LOG_TYPE_PATCHES))
	{
		errorMsg.append("\n \nDetails:\n");
		for (auto& itr : errorMessages)
		{
			errorMsg.append(itr);
			errorMsg.append("\n");
		}
	}

	wxMessageBox(errorMsg, "Graphic pack error");
}

// loads Cemu-style patches (patch_<anything>.asm)
// returns true if at least one file was found even if it could not be successfully parsed
bool GraphicPack2::LoadCemuPatches()
{
	// todo - once we have updated to C++20 we can replace these with the new std::string functions
	auto startsWith = [](const std::wstring& str, const std::wstring& prefix)
	{
		return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
	};

	auto endsWith = [](const std::wstring& str, const std::wstring& suffix)
	{
		return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
	};

	bool foundPatches = false;
	fs::path path(m_filename);
	path.remove_filename();
	for (auto& p : fs::directory_iterator(path))
	{
		auto& path = p.path();
		if (fs::is_regular_file(p.status()) && path.has_filename())
		{
			// check if filename matches
			std::wstring filename = path.filename().generic_wstring();
			if (boost::istarts_with(filename, L"patch_") && boost::iends_with(filename, L".asm"))
			{
				FileStream* patchFile = FileStream::openFile(path.generic_wstring().c_str());
				if (patchFile)
				{
					// read file
					std::vector<uint8> fileData;
					patchFile->extract(fileData);
					delete patchFile;
					MemStreamReader patchesStream(fileData.data(), (sint32)fileData.size());
					// load Cemu style patch file
					if (!ParseCemuPatchesTxtInternal(patchesStream))
					{
						forceLog_printfW(L"Error while processing \"%s\". No patches for this graphic pack will be applied.", path.c_str());
						cemu_assert_debug(list_patchGroups.empty());
						return true; // return true since a .asm patch was found even if we could not parse it
					}
				}
				else
				{
					forceLog_printfW(L"Unable to load patch file \"%s\"", path.c_str());
				}
				foundPatches = true;
			}
		}
	}
	return foundPatches;
}

void GraphicPack2::LoadPatchFiles()
{
	// order of loading patches:
	// 1) Load Cemu-style patches (patch_<name>.asm), stop here if at least one patch file exists
	// 2) Load Cemuhook patches.txt

	// update: As of 1.20.2b Cemu always takes over patching since Cemuhook patching broke due to other internal changes (memory allocation changed and some reordering on when graphic packs get loaded)
	if (LoadCemuPatches())
		return; // exit if at least one Cemu style patch file was found
	// fall back to Cemuhook patches.txt to guarantee backward compatibility
	fs::path path(m_filename);
	path.remove_filename();
	path.append("patches.txt");

	FileStream* patchFile = FileStream::openFile(path.generic_wstring().c_str());

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
		if (itr->matchesCRC(rpl->patchCRC))
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
		if (itr->matchesCRC(rpl->patchCRC))
			list_groups.emplace_back(itr);
	}
	// undo all groups at once
	if (!list_groups.empty())
		UndoPatchGroups(list_groups, rpl);
}

std::recursive_mutex GraphicPack2::mtx_patches;
std::vector<const RPLModule*> GraphicPack2::list_modules;
