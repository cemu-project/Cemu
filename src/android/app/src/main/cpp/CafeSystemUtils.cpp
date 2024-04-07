#include "CafeSystemUtils.h"

#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"

namespace CafeSystemUtils
{
	void startGame(TitleId titleId)
	{
		TitleInfo launchTitle;

		if (!CafeTitleList::GetFirstByTitleId(titleId, launchTitle))
			return;

		if (launchTitle.IsValid())
		{
			// the title might not be in the TitleList, so we add it as a temporary entry
			CafeTitleList::AddTitleFromPath(launchTitle.GetPath());
			// title is valid, launch from TitleId
			TitleId baseTitleId;
			if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(), baseTitleId))
			{
			}
			CafeSystem::STATUS_CODE statusCode = CafeSystem::PrepareForegroundTitle(baseTitleId);
			if (statusCode == CafeSystem::STATUS_CODE::INVALID_RPX)
			{
			}
			else if (statusCode == CafeSystem::STATUS_CODE::UNABLE_TO_MOUNT)
			{
			}
			else if (statusCode != CafeSystem::STATUS_CODE::SUCCESS)
			{
			}
		}
		else // if (launchTitle.GetFormat() == TitleInfo::TitleDataFormat::INVALID_STRUCTURE )
		{
			// title is invalid, if its an RPX/ELF we can launch it directly
			// otherwise its an error
			CafeTitleFileType fileType = DetermineCafeSystemFileType(launchTitle.GetPath());
			if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF)
			{
				CafeSystem::STATUS_CODE statusCode = CafeSystem::PrepareForegroundTitleFromStandaloneRPX(
					launchTitle.GetPath());
				if (statusCode != CafeSystem::STATUS_CODE::SUCCESS)
				{
					return;
				}
			}
		}
		CafeSystem::LaunchForegroundTitle();
	}
}; // namespace CafeSystemUtils