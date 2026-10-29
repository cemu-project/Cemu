#pragma once
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/TitleList/TitleId.h"

enum class CosCapabilityBits : uint64;
enum class CosCapabilityGroup : uint32;
enum class CafeConsoleRegion;

namespace CafeSystem
{
	class SystemImplementation
	{
	public:
		virtual void CafeRecreateCanvas() = 0;
		virtual void CafePPCProcessExit() = 0; // emulated process exited
	};

	enum class PREPARE_STATUS_CODE
	{
		SUCCESS,
		INVALID_RPX,
		UNABLE_TO_MOUNT, // failed to mount through TitleInfo (most likely caused by an invalid or outdated path)
	};

	void Initialize();
	void SetImplementation(SystemImplementation* impl);
    void Shutdown();

	PREPARE_STATUS_CODE PrepareForegroundTitle(TitleId titleId);
	PREPARE_STATUS_CODE PrepareForegroundTitleFromStandaloneRPX(const fs::path& path);
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
	CosCapabilityBits GetForegroundTitleCosCapabilities(CosCapabilityGroup group);
	std::optional<sint32> GetForegroundTitleReturnStatus(); // valid once the foreground title exited gracefully via coreinit exit

	void ShutdownTitle();

	std::string GetMlcStoragePath(TitleId titleId);
	void MlcStorageMountAllTitles();

	std::string GetInternalVirtualCodeFolder();

	uint32 GetRPXHashBase();
	uint32 GetRPXHashUpdated();
	void RequestRecreateCanvas();
	void NotifyPPCProcessExit(sint32 status);

	// input of the game windows
	enum class MouseButton : uint8
	{
		Left,
		Right,
	};
	void NotifyMouseMove(bool padWindow, sint32 x, sint32 y); // physical pixels
	void NotifyMouseButton(bool padWindow, MouseButton button, bool pressed, sint32 x, sint32 y);
	void NotifyMouseWheel(sint32 rotation); // 120 per notch
	void NotifyTextInput(uint32 codepoint);
	bool IsTextInputActive();
	void NotifyKeyEvent(uint8 hidUsage, bool pressed);
	void ReleaseInput();
	bool IsUSBKeyboardInUse();
	void SetUSBKeyboardEnabled(bool enabled);

	void DumpCurrentRPX();
};

extern RPLModule* applicationRPX;

extern std::atomic_bool g_isGPUInitFinished;
