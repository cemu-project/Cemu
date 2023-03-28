#include "input/api/GameCube/GameCubeControllerProvider.h"
#include "input/api/GameCube/GameCubeController.h"
#include "util/libusbWrapper/libusbWrapper.h"

#if HAS_GAMECUBE

constexpr uint16_t kVendorId = 0x57e, kProductId = 0x337;

GameCubeControllerProvider::GameCubeControllerProvider()
{
	m_libusb = libusbWrapper::getInstance();
	m_libusb->init();
	if(!m_libusb->isAvailable())
		throw std::runtime_error("libusbWrapper not available");
    m_libusb->p_libusb_init(&m_context);

	for(auto i = 0; i < kMaxAdapters; ++i)
	{
		auto device = fetch_usb_device(i);
		if(std::get<0>(device))
		{
			m_adapters[i].m_device_handle = std::get<0>(device);
			m_adapters[i].m_endpoint_reader = std::get<1>(device);
			m_adapters[i].m_endpoint_writer = std::get<2>(device);
		}
	}

	if (m_libusb->p_libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG))
	{
		m_libusb->p_libusb_hotplug_register_callback(m_context, static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
			LIBUSB_HOTPLUG_NO_FLAGS, kVendorId, kProductId, LIBUSB_HOTPLUG_MATCH_ANY, &GameCubeControllerProvider::hotplug_event, this, &m_callback_handle);
	}

	m_running = true;
	m_reader_thread = std::thread(&GameCubeControllerProvider::reader_thread, this);
	m_writer_thread = std::thread(&GameCubeControllerProvider::writer_thread, this);
}

GameCubeControllerProvider::~GameCubeControllerProvider()
{
	if (m_running)
	{
		m_running = false;
		m_writer_thread.join();
		m_reader_thread.join();
	}

	if (m_callback_handle)
	{
		m_libusb->p_libusb_hotplug_deregister_callback(m_context, m_callback_handle);
		m_callback_handle = 0;
	}

	for (auto& a : m_adapters)
	{
		m_libusb->p_libusb_close(a.m_device_handle);
	}

	if (m_context)
	{
		m_libusb->p_libusb_exit(m_context);
		m_context = nullptr;
	}
}

std::vector<ControllerPtr> GameCubeControllerProvider::get_controllers()
{
	std::vector<ControllerPtr> result;

	const auto adapter_count = get_adapter_count();
	for (uint32 adapter_index = 0; adapter_index < adapter_count && adapter_index < kMaxAdapters; ++adapter_index)
	{
		// adapter doesn't tell us which one is actually connected, so we return all of them
		for (int index = 0; index < 4; ++index)
			result.emplace_back(std::make_shared<GameCubeController>(adapter_index, index));
	}

	return result;
}

uint32 GameCubeControllerProvider::get_adapter_count() const
{
	uint32 adapter_count = 0;

	libusb_device** devices;
	const auto count = m_libusb->p_libusb_get_device_list(nullptr, &devices);
	if (count < 0)
	{
		cemuLog_log(LogType::Force, "libusb error {} at libusb_get_device_list: {}", static_cast<int>(count), m_libusb->p_libusb_error_name(static_cast<int>(count)));
		return adapter_count;
	}

	for (ssize_t i = 0; i < count; ++i)
	{
		if (!devices[i])
			continue;

		libusb_device_descriptor desc;
		int ret = m_libusb->p_libusb_get_device_descriptor(devices[i], &desc);
		if (ret != 0)
		{
			cemuLog_log(LogType::Force, "libusb error {} at libusb_get_device_descriptor: {}", ret, m_libusb->p_libusb_error_name(ret));
			continue;
		}

		if (desc.idVendor != kVendorId || desc.idProduct != kProductId)
			continue;

		++adapter_count;
	}

	m_libusb->p_libusb_free_device_list(devices, 1);
	return adapter_count;
}

bool GameCubeControllerProvider::has_rumble_connected(uint32 adapter_index) const
{
	if (adapter_index >= m_adapters.size())
		return false;

	std::scoped_lock lock(m_adapters[adapter_index].m_state_mutex);
	return m_adapters[adapter_index].m_rumble_connected;
}

bool GameCubeControllerProvider::is_connected(uint32 adapter_index) const
{
	if (adapter_index >= m_adapters.size())
		return false;

	return m_adapters[adapter_index].m_device_handle != nullptr;
}

void GameCubeControllerProvider::set_rumble_state(uint32 adapter_index, uint32 index, bool state)
{
	if (adapter_index >= m_adapters.size())
		return;

	if (index >= kMaxIndex)
		return;

	std::scoped_lock lock(m_writer_mutex);
	m_adapters[adapter_index].rumble_states[index] = state;
	m_rumble_changed = true;
	m_writer_cond.notify_all();
}

GameCubeControllerProvider::GCState GameCubeControllerProvider::get_state(uint32 adapter_index, uint32 index)
{
	if (adapter_index >= m_adapters.size())
		return {};

	if (index >= kMaxIndex)
		return {};

	std::scoped_lock lock(m_adapters[adapter_index].m_state_mutex);
	return m_adapters[adapter_index].m_states[index];
}

#ifdef interface
#undef interface
#endif

std::tuple<libusb_device_handle*, uint8, uint8> GameCubeControllerProvider::fetch_usb_device(uint32 adapter) const
{
	std::tuple<libusb_device_handle*, uint8, uint8> result{nullptr, 0xFF, 0xFF};

	libusb_device** devices;
	const auto count = m_libusb->p_libusb_get_device_list(nullptr, &devices);
	if (count < 0)
	{
		cemuLog_log(LogType::Force, "libusb error {} at libusb_get_device_list: {}", static_cast<int>(count), m_libusb->p_libusb_error_name(static_cast<int>(count)));
		return result;
	}

	int adapter_index = 0;
	for (ssize_t i = 0; i < count; ++i)
	{
		if (!devices[i])
			continue;

		libusb_device_descriptor desc;
		int ret = m_libusb->p_libusb_get_device_descriptor(devices[i], &desc);
		if (ret != 0)
		{
			cemuLog_log(LogType::Force, "libusb error {} at libusb_get_device_descriptor: {}", ret, m_libusb->p_libusb_error_name(ret));
			continue;
		}

		if (desc.idVendor != kVendorId || desc.idProduct != kProductId)
			continue;

		if (adapter != adapter_index++)
			continue;

		libusb_device_handle* device_handle;
		ret = m_libusb->p_libusb_open(devices[i], &device_handle);
		if (ret != 0)
		{
			cemuLog_log(LogType::Force, "libusb error {} at libusb_open: {}", ret, m_libusb->p_libusb_error_name(ret));
			continue;
		}

		if (m_libusb->p_libusb_kernel_driver_active(device_handle, 0) == 1)
		{
			ret = m_libusb->p_libusb_detach_kernel_driver(device_handle, 0);
			if (ret != 0)
			{
				cemuLog_log(LogType::Force, "libusb error {} at libusb_detach_kernel_driver: {}", ret, m_libusb->p_libusb_error_name(ret));
				m_libusb->p_libusb_close(device_handle);
				continue;
			}
		}

		ret = m_libusb->p_libusb_claim_interface(device_handle, 0);
		if (ret != 0)
		{
			cemuLog_log(LogType::Force, "libusb error {} at libusb_claim_interface: {}", ret, m_libusb->p_libusb_error_name(ret));
			m_libusb->p_libusb_close(device_handle);
			continue;
		}

		libusb_config_descriptor* config = nullptr;
		m_libusb->p_libusb_get_config_descriptor(devices[i], 0, &config);
		for (auto ic = 0; ic < config->bNumInterfaces; ic++)
		{
			const auto& interface = config->interface[ic];
			for (auto j = 0; j < interface.num_altsetting; j++)
			{
				const auto& interface_desc = interface.altsetting[j];
				for (auto k = 0; k < interface_desc.bNumEndpoints; k++)
				{
					const auto& endpoint = interface_desc.endpoint[k];
					if (endpoint.bEndpointAddress & LIBUSB_ENDPOINT_IN)
						std::get<1>(result) = endpoint.bEndpointAddress;
					else
						std::get<2>(result) = endpoint.bEndpointAddress;
				}
			}
		}

		m_libusb->p_libusb_free_config_descriptor(config);

		// start polling
		int size = 0;
		uint8_t payload = 0x13;
		m_libusb->p_libusb_interrupt_transfer(device_handle, std::get<2>(result), &payload, sizeof(payload), &size, 25);

		std::get<0>(result) = device_handle;

		break;
	}

	m_libusb->p_libusb_free_device_list(devices, 1);
	return result;
}

void GameCubeControllerProvider::reader_thread()
{
	SetThreadName("GCControllerAdapter::reader_thread");
	while (m_running.load(std::memory_order_relaxed))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::yield();

		for(auto& adapter : m_adapters)
		{
			if (!adapter.m_device_handle)
				continue;

			std::array<uint8_t, 37> data{};
			int read;
			const int result = m_libusb->p_libusb_interrupt_transfer(adapter.m_device_handle, adapter.m_endpoint_reader, data.data(), static_cast<int>(data.size()), &read, 25);
			if (result == 0)
			{
				/*
				byte 1
					0x10 NORMAL STATE
					0x20 WAVEBIRD STATE
					0x04 RUMBLE POWER
				 */
				for (int i = 0; i < 4; ++i)
				{
					GCState state;
					state.valid = true;
					state.button = *(uint16*)&data[1 + (i * 9) + 1];
					state.lstick_x = data[1 + (i * 9) + 3];
					state.lstick_y = data[1 + (i * 9) + 4];
					state.rstick_x = data[1 + (i * 9) + 5];
					state.rstick_y = data[1 + (i * 9) + 6];
					state.lstick = data[1 + (i * 9) + 7];
					state.rstick = data[1 + (i * 9) + 8];

					std::scoped_lock lock(adapter.m_state_mutex);
					adapter.m_rumble_connected = HAS_FLAG(data[1], 4);
					adapter.m_states[i] = state;
				}
			}
			else if (result == LIBUSB_ERROR_NO_DEVICE || result == LIBUSB_ERROR_IO)
			{
				cemuLog_log(LogType::Force, "libusb error {} at libusb_interrupt_transfer: {}", result, m_libusb->p_libusb_error_name(result));
				if (const auto handle = adapter.m_device_handle.exchange(nullptr))
					m_libusb->p_libusb_close(handle);
			}
			else { cemuLog_log(LogType::Force, "libusb error {} at libusb_interrupt_transfer: {}", result, m_libusb->p_libusb_error_name(result)); }
		}
	}
}

void GameCubeControllerProvider::writer_thread()
{
	SetThreadName("GCControllerAdapter::writer_thread");

	std::array<std::array<bool, 4>, kMaxAdapters> rumble_states{};

	while (m_running.load(std::memory_order_relaxed))
	{
		std::unique_lock lock(m_writer_mutex);
		if (!m_rumble_changed && m_writer_cond.wait_for(lock, std::chrono::milliseconds(250)) == std::cv_status::timeout)
		{
			if (!m_running)
				return;

			continue;
		}

		bool cmd_sent = false;
		for (size_t i = 0; i < kMaxAdapters; ++i)
		{
			auto& adapter = m_adapters[i];
			if (!adapter.m_device_handle)
				continue;

			if (adapter.rumble_states == rumble_states[i])
				continue;

			rumble_states[i] = adapter.rumble_states;
			m_rumble_changed = false;
			lock.unlock();

			std::array<uint8, 5> rumble{ 0x11, rumble_states[i][0],rumble_states[i][1],rumble_states[i][2], rumble_states[i][3] };

			int written;
			const int result = m_libusb->p_libusb_interrupt_transfer(adapter.m_device_handle, adapter.m_endpoint_writer, rumble.data(), static_cast<int>(rumble.size()), &written, 25);
			if (result != 0) { cemuLog_log(LogType::Force, "libusb error {} at libusb_interrupt_transfer: {}", result, m_libusb->p_libusb_error_name(result)); }
			cmd_sent = true;

			lock.lock();
		}

		if(cmd_sent)
		{
			lock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
}

int GameCubeControllerProvider::hotplug_event(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data)
{
	auto* thisptr = static_cast<GameCubeControllerProvider*>(user_data);
	if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event)
	{
		for (auto i = 0; i < kMaxAdapters; ++i)
		{
			if (thisptr->m_adapters[i].m_device_handle)
				continue;

			auto device = thisptr->fetch_usb_device(i);
			if (std::get<0>(device))
			{
				thisptr->m_adapters[i].m_endpoint_reader = std::get<1>(device);
				thisptr->m_adapters[i].m_endpoint_writer = std::get<2>(device);

				thisptr->m_adapters[i].m_device_handle = std::get<0>(device);
			}
		}
	}
	/*else
	{
		const auto device_handle = thisptr->m_device_handle.exchange(nullptr);
		if (device_handle)
			thisptr->m_libusb->p_libusb_close(device_handle);
	}*/

	return 0;
}

#endif
