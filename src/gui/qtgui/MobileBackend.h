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
}
