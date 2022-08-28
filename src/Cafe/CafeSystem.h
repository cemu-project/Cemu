#pragma once
#include "Cafe/OS/RPL/rpl.h"
#include "util/helpers/Semaphore.h"
#include "Cafe/TitleList/TitleId.h"
#include "config/CemuConfig.h"

namespace CafeSystem
{
	enum class STATUS_CODE
	{
		SUCCESS,
		INVALID_RPX,
		UNABLE_TO_MOUNT, // failed to mount through TitleInfo (most likely caused by an invalid or outdated path)
		//BAD_META_DATA, - the title list only stores titles with valid meta, so this error code is impossible
	};

	void Initialize();
	STATUS_CODE PrepareForegroundTitle(TitleId titleId);
	STATUS_CODE PrepareForegroundTitleFromStandaloneRPX(const fs::path& path);
	void LaunchForegroundTitle();
	bool IsTitleRunning();

	TitleId GetForegroundTitleId();
	uint16 GetForegroundTitleVersion();
	CafeConsoleRegion GetForegroundTitleRegion();
	CafeConsoleRegion GetPlatformRegion();
	std::string GetForegroundTitleName();
	std::string GetForegroundTitleArgStr();

	void ShutdownTitle();

	std::string GetMlcStoragePath(TitleId titleId);
	void MlcStorageMountAllTitles();

	std::string GetInternalVirtualCodeFolder();

	uint32 GetRPXHashBase();
	uint32 GetRPXHashUpdated();
};

extern RPLModule* applicationRPX;

extern std::atomic_bool g_isGPUInitFinished;
