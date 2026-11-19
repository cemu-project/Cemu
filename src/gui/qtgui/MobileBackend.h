#pragma once

// Bridge between the Qt/QML mobile interface and the Cemu core.
//
// Only standard C++ types are used here on purpose: the Qt side never includes Cemu headers and the
// Cemu side never includes Qt headers. Both pull in a lot of macros/types (X11, GTK, uint32, signals/slots, ..)
// which clash when mixed in a single translation unit.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MobileBackend
{
	struct GameEntry
	{
		uint64_t titleId = 0;
		std::string name;
		std::string region;
		int version = 0;
		bool hasUpdate = false;
		bool hasDLC = false;
	};

	struct NativeWindow
	{
		bool isWayland = false;
		void* display = nullptr; // wl_display* or Display*
		void* surface = nullptr; // wl_surface* or X11 window id
		int width = 0, height = 0;		   // logical size
		int physWidth = 0, physHeight = 0; // size in physical pixels
		double scale = 1.0;
	};

	// must be called once from the UI thread before anything else
	void Initialize();

	// ---- game list ----
	// the callback can be invoked from any thread
	void SetGameListChangedCallback(std::function<void()> cb);
	std::vector<GameEntry> GetGames();
	void RefreshGameList();
	bool IsScanning();
	// decodes the icon of a title to 32bit RGBA. Slow (mounts the title), call from a worker thread
	bool LoadGameIcon(uint64_t titleId, std::vector<uint8_t>& rgbaOut, int& widthOut, int& heightOut);

	// ---- game folders ----
	std::vector<std::string> GetGamePaths();
	void AddGamePath(const std::string& path);
	void RemoveGamePath(const std::string& path);

	// ---- emulation ----
	// returns false and sets errorOut on failure. The window must already be mapped/exposed
	bool LaunchGame(uint64_t titleId, const NativeWindow& window, std::string& errorOut);
	bool IsGameRunning();
	std::string GetRunningGameName();
	// invoked (from an emulator thread) when the emulated title exits on its own
	void SetGameExitedCallback(std::function<void()> cb);
	void Shutdown();
	// replaces the current process with a fresh instance of the mobile UI. Does not return on success
	void RestartProcess();

	// ---- window state & input forwarded from the render window ----
	void SetWindowSize(int width, int height, int physWidth, int physHeight, double scale);
	void SetAppActive(bool active);
	void SetKeyState(uint32_t platformKeyCode, bool down);
	void ReleaseAllKeys();
	void SetMouse(int physX, int physY, bool leftDown, bool rightDown);
	void SetTouch(int physX, int physY, bool down);

	std::string GetVersionString();

	// ---- settings (shared with the desktop interface, saved to settings.xml) ----
	// integer/bool settings addressed by key, see the table in MobileBackend.cpp. Unknown keys return 0
	int GetSetting(const std::string& key);
	void SetSetting(const std::string& key, int value); // saves the config
	struct AudioDevice
	{
		std::string id; // empty = disabled
		std::string name;
	};
	std::vector<AudioDevice> GetAudioDevices(); // output devices of the active audio API
	std::string GetTVAudioDevice();
	void SetTVAudioDevice(const std::string& id);

	// ---- per game actions ----
	int CountShaderCaches(uint64_t titleId);
	int RemoveShaderCaches(uint64_t titleId); // returns the number of deleted files

	struct GraphicPackPresetGroup
	{
		std::string category; // empty = default category
		std::vector<std::string> presets;
		std::string active;
	};
	struct GraphicPackInfo
	{
		int index = 0; // stable handle for the lifetime of the process
		std::string name;
		std::string path; // position in the graphic pack tree, e.g. "Resolution/BotW"
		std::string description;
		bool enabled = false;
		std::vector<GraphicPackPresetGroup> presetGroups; // only categories with visible presets
	};
	std::vector<GraphicPackInfo> GetGraphicPacks(uint64_t titleId);
	void SetGraphicPackEnabled(int index, bool enabled);
	void SetGraphicPackPreset(int index, const std::string& category, const std::string& preset);

	// ---- controllers (profiles are shared with the desktop interface: controllerProfiles/controllerN.xml) ----
	struct InputDevice
	{
		std::string uuid;
		std::string name;
		std::string api;
		bool connected = false;
	};
	struct InputPlayer
	{
		bool enabled = false;	  // false = no emulated controller on this player slot
		std::string type;		  // "Wii U GamePad", "Wii U Pro Controller", ...
		std::vector<InputDevice> devices; // physical devices assigned to the slot
	};
	struct InputPressedButton
	{
		int id = 0;
		std::string emulatedName; // name of the Wii U button
		std::string deviceName;	  // name of the physical button/axis mapped to it
	};
	std::vector<std::string> GetInputControllerTypes();
	InputPlayer GetInputPlayer(int player);
	// physical devices seen by SDL right now
	std::vector<InputDevice> GetAvailableInputDevices();
	// type "" removes the emulated controller of the slot. Both save the profile
	bool SetInputPlayerType(int player, const std::string& type, std::string& errorOut);
	// replaces the devices of the slot by the given one and applies the default mapping. Saves the profile
	bool AssignInputDevice(int player, const std::string& uuid, std::string& errorOut);
	void RemoveInputDevice(int player, const std::string& uuid);
	bool ResetInputMapping(int player, std::string& errorOut);
	// Wii U buttons currently held on this slot, for the test screen
	std::vector<InputPressedButton> GetPressedInputButtons(int player);

	// ---- navigation of the interface with a gamepad ----
	enum NavAction
	{
		kNavUp = 1,
		kNavDown,
		kNavLeft,
		kNavRight,
		kNavAccept, // south button (A / cross)
		kNavBack,	// east button (B / circle)
		kNavMenu	// north button (Y / triangle)
	};
	// polls every connected gamepad (D-pad, left stick, A/B/Y) and returns the actions triggered since the last call,
	// with key repeat for the directions. Call regularly from the UI thread while no game runs
	std::vector<int> PollUiNavigation();
}
