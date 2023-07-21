#pragma once
#include "Cafe/OS/RPL/rpl.h"
#include "util/helpers/Semaphore.h"
#include "Cafe/TitleList/TitleId.h"
#include "config/CemuConfig.h"

namespace CafeSystem
{
	class SystemImplementation
	{
	public:
		virtual void CafeRecreateCanvas() = 0;
	};

	enum class STATUS_CODE
	{
		SUCCESS,
		INVALID_RPX,
		UNABLE_TO_MOUNT, // failed to mount through TitleInfo (most likely caused by an invalid or outdated path)
		//BAD_META_DATA, - the title list only stores titles with valid meta, so this error code is impossible
	};

	void Initialize();
	void SetImplementation(SystemImplementation* impl);
    void Shutdown();

	STATUS_CODE PrepareForegroundTitle(TitleId titleId);
	STATUS_CODE PrepareForegroundTitleFromStandaloneRPX(const fs::path& path);
	void LaunchForegroundTitle();
	bool IsTitleRunning();

	bool GetOverrideArgStr(std::vector<std::string>& args);
	void SetOverrideArgs(std::span<std::string> args);
	void UnsetOverrideArgs();

	TitleId GetForegroundTitleId();
	uint16 GetForegroundTitleVersion();
	uint32 GetForegroundTitleSDKVersion();
	CafeConsoleRegion GetForegroundTitleRegion();
	CafeConsoleRegion GetPlatformRegion();
	std::string GetForegroundTitleName();
	std::string GetForegroundTitleArgStr();
	uint32 GetForegroundTitleOlvAccesskey();

	void ShutdownTitle();

	std::string GetMlcStoragePath(TitleId titleId);
	void MlcStorageMountAllTitles();

	std::string GetInternalVirtualCodeFolder();

	uint32 GetRPXHashBase();
	uint32 GetRPXHashUpdated();

	void RequestRecreateCanvas();
};

extern RPLModule* applicationRPX;

extern std::atomic_bool g_isGPUInitFinished;
