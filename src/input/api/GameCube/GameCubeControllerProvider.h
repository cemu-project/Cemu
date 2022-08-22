#pragma once

#include "util/libusbWrapper/libusbWrapper.h"
#include "input/api/ControllerProvider.h"

#ifdef HAS_GAMECUBE

class GameCubeControllerProvider : public ControllerProviderBase
{
	friend class DSUController;
public:
	constexpr static size_t kMaxAdapters = 4;
	constexpr static size_t kMaxIndex = 4;

	GameCubeControllerProvider();
	~GameCubeControllerProvider();

	inline static InputAPI::Type kAPIType = InputAPI::GameCube;
	InputAPI::Type api() const override { return kAPIType; }

	std::vector<std::shared_ptr<ControllerBase>> get_controllers() override;

	uint32 get_adapter_count() const;
	bool has_rumble_connected(uint32 adapter_index) const;
	bool is_connected(uint32 adapter_index) const;

	void set_rumble_state(uint32 adapter_index, uint32 index, bool state);
	
	struct GCState
	{
		bool valid = false;
		uint16 button = 0;

		uint8 lstick_x = 0;
		uint8 lstick_y = 0;

		uint8 rstick_x = 0;
		uint8 rstick_y = 0;

		uint8 lstick = 0;
		uint8 rstick = 0;
	};
	GCState get_state(uint32 adapter_index, uint32 index);

private:
	std::shared_ptr<libusbWrapper> m_libusb;
	libusb_context* m_context = nullptr;

	std::atomic_bool m_running = false;
	std::thread m_reader_thread, m_writer_thread;

	void reader_thread();
	void writer_thread();

	// handle, endpoint_reader, endpoint_writer
	std::tuple<libusb_device_handle*, uint8, uint8> fetch_usb_device(uint32 adapter) const;

	std::mutex m_writer_mutex;
	std::condition_variable m_writer_cond;
	bool m_rumble_changed = false;
	struct Adapter
	{
		std::atomic<libusb_device_handle*> m_device_handle{};

		uint8 m_endpoint_reader = 0xFF, m_endpoint_writer = 0xFF;

		mutable std::mutex m_state_mutex;
		std::array<GCState, kMaxIndex> m_states{};
		bool m_rumble_connected = false;

		std::array<bool, kMaxIndex> rumble_states{};
	};
	std::array<Adapter, kMaxAdapters> m_adapters;
	
	libusb_hotplug_callback_handle m_callback_handle = 0;
	static int hotplug_event(struct libusb_context* ctx, struct libusb_device* dev, libusb_hotplug_event event, void* user_data);
};

#endif