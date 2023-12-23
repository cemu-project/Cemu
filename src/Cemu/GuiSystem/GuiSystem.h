#pragma once

namespace GuiSystem
{

struct WindowHandleInfo
{
	enum class Backend
	{
		X11,
		Wayland,
		Android,
		Cocoa,
		Windows
	} backend;
	void* display = nullptr;
	void* surface = nullptr;
};

enum struct PlatformKeyCodes : uint32
{
	LCONTROL,
	RCONTROL,
	TAB,
	MAX
};

struct WindowInfo
{
	std::atomic_bool app_active;  // our app is active/has focus

	std::atomic_int32_t width, height;            // client size of main window
	std::atomic_int32_t phys_width, phys_height;  // client size of main window in physical pixels
	std::atomic<double> dpi_scale;

	std::atomic_bool pad_open;                            // if separate pad view is open
	std::atomic_int32_t pad_width, pad_height;            // client size of pad window
	std::atomic_int32_t phys_pad_width, phys_pad_height;  // client size of pad window in physical pixels
	std::atomic<double> pad_dpi_scale;

	std::atomic_bool pad_maximized = false;
	std::atomic_int32_t restored_pad_x = -1, restored_pad_y = -1;
	std::atomic_int32_t restored_pad_width = -1, restored_pad_height = -1;

	std::atomic_bool has_screenshot_request;
	std::atomic_bool is_fullscreen;

	inline void set_keystate(uint32 keycode, bool state)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		m_keydown[keycode] = state;
	}

	inline void set_keystate(PlatformKeyCodes keycode, bool state)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		m_platformkeydown.at(static_cast<std::size_t>(keycode)) = state;
	}

	inline bool get_keystate(uint32 keycode)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		auto result = m_keydown.find(keycode);
		if (result == m_keydown.end())
			return false;
		return result->second;
	}

	inline bool get_keystate(PlatformKeyCodes keycode)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		return m_platformkeydown.at(static_cast<size_t>(keycode));
	}

	inline void set_keystates_up()
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		std::for_each(m_keydown.begin(), m_keydown.end(), [](std::pair<const uint32, bool>& el) { el.second = false; });
		m_platformkeydown.fill(false);
	}

	template <typename fn>
	void iter_keystates(fn f)
	{
		const std::lock_guard<std::mutex> lock(keycode_mutex);
		std::for_each(m_keydown.cbegin(), m_keydown.cend(), f);
	}

	WindowHandleInfo window_main;
	WindowHandleInfo window_pad;

	WindowHandleInfo canvas_main;
	WindowHandleInfo canvas_pad;

   private:
	std::array<bool, static_cast<uint32>(PlatformKeyCodes::MAX)> m_platformkeydown;
	std::mutex keycode_mutex;
	std::unordered_map<uint32, bool> m_keydown;
};

void registerKeyCodeToStringCallback(const std::function<std::string(uint32)>& keyCodeToString);
void unregisterKeyCodeToStringCallback();
std::string keyCodeToString(uint32 key);
void getWindowSize(int& w, int& h);
void getPadWindowSize(int& w, int& h);
void getWindowPhysSize(int& w, int& h);
void getPadWindowPhysSize(int& w, int& h);
double getWindowDPIScale();
double getPadDPIScale();
bool isPadWindowOpen();
bool isKeyDown(uint32 key);
bool isKeyDown(PlatformKeyCodes key);
bool isFullScreen();
WindowInfo& getWindowInfo();
}  // namespace GuiSystem