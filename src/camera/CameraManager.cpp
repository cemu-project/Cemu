#include "CameraManager.h"

#include "config/CemuConfig.h"
#include "util/helpers/helpers.h"
#include "Rgb2Nv12.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <thread>

#include <openpnp-capture.h>

constexpr unsigned CAMERA_WIDTH = 640;
constexpr unsigned CAMERA_HEIGHT = 480;
constexpr unsigned CAMERA_PITCH = 768;

namespace CameraManager
{
	std::mutex s_mutex;
	bool s_initialized = false;
	CapContext s_ctx;
	std::optional<CapDeviceID> s_device;
	std::optional<CapStream> s_stream;
	std::array<uint8, CAMERA_WIDTH * CAMERA_HEIGHT * 3> s_rgbBuffer;
	std::array<uint8, CAMERA_PITCH * CAMERA_HEIGHT * 3 / 2> s_nv12Buffer;
	int s_refCount = 0;
	std::thread s_captureThread;
	std::atomic_bool s_capturing = false;
	std::atomic_bool s_running = false;

	std::optional<CapFormatID> FindCorrectFormat()
	{
		const auto formatCount = Cap_getNumFormats(s_ctx, *s_device);
		for (int32_t formatId = 0; formatId < formatCount; ++formatId)
		{
			CapFormatInfo formatInfo;
			if (Cap_getFormatInfo(s_ctx, *s_device, formatId, &formatInfo) != CAPRESULT_OK)
				continue;
			if (formatInfo.width == CAMERA_WIDTH && formatInfo.height == CAMERA_HEIGHT)
				return formatId;
		}
		return std::nullopt;
	}

	void CaptureWorker()
	{
		SetThreadName("CameraManager");
		while (s_running)
		{
			while (s_capturing)
			{
				s_mutex.lock();
				if (s_stream && Cap_hasNewFrame(s_ctx, *s_stream) &&
					Cap_captureFrame(s_ctx, *s_stream, s_rgbBuffer.data(), s_rgbBuffer.size()) == CAPRESULT_OK)
					Rgb2Nv12(s_rgbBuffer.data(), CAMERA_WIDTH, CAMERA_HEIGHT, s_nv12Buffer.data(), CAMERA_PITCH);
				s_mutex.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
			std::this_thread::yield();
		}
	}
	void OpenStream()
	{
		const auto formatId = FindCorrectFormat();
		if (!formatId)
			return;
		const auto stream = Cap_openStream(s_ctx, *s_device, *formatId);
		if (stream == -1)
			return;
		s_capturing = true;
		s_stream = stream;
	}
	void CloseStream()
	{
		s_capturing = false;
		if (s_stream)
		{
			Cap_closeStream(s_ctx, *s_stream);
			s_stream = std::nullopt;
		}
	}
	void ResetBuffers()
	{
		std::ranges::fill(s_rgbBuffer, 0);
		std::ranges::fill_n(s_nv12Buffer.begin(), CAMERA_WIDTH * CAMERA_PITCH, 16);
		std::ranges::fill_n(s_nv12Buffer.begin() + CAMERA_WIDTH * CAMERA_PITCH, (CAMERA_WIDTH / 2), 128);
	}

	void Init()
	{
		std::scoped_lock lock(s_mutex);
		if (s_initialized)
			return;
		s_initialized = true;
		s_mutex.unlock();
		s_running = true;
		s_captureThread = std::thread(&CaptureWorker);

		s_ctx = Cap_createContext();

		const auto uniqueId = GetConfig().camera_id.GetValue();
		if (!uniqueId.empty())
		{
			const auto devices = EnumerateDevices();
			for (CapDeviceID deviceId = 0; deviceId < devices.size(); ++deviceId)
			{
				if (devices[deviceId].uniqueId == uniqueId)
				{
					s_device = deviceId;
					return;
				}
			}
		}
		ResetBuffers();
	}
	void Deinit()
	{
		CloseStream();
		Cap_releaseContext(s_ctx);
		s_captureThread.join();
		s_initialized = false;
	}
	void FillNV12Buffer(uint8* nv12Buffer)
	{
		std::scoped_lock lock(s_mutex);
		std::ranges::copy(s_nv12Buffer, nv12Buffer);
	}

	void FillRGBBuffer(uint8* rgbBuffer)
	{
		std::scoped_lock lock(s_mutex);
		std::ranges::copy(s_rgbBuffer, rgbBuffer);
	}
	void SetDevice(uint32 deviceNo)
	{
		std::scoped_lock lock(s_mutex);
		CloseStream();
		if (deviceNo == DEVICE_NONE)
		{
			s_device = std::nullopt;
			ResetBuffers();
			return;
		}
		s_device = deviceNo;
		if (s_refCount != 0)
			OpenStream();
	}
	void Open()
	{
		std::scoped_lock lock(s_mutex);
		if (s_device && s_refCount == 0)
		{
			OpenStream();
		}
		s_refCount += 1;
	}
	void Close()
	{
		std::scoped_lock lock(s_mutex);
		if (s_refCount == 0)
			return;
		s_refCount -= 1;
		if (s_refCount != 0)
			return;
		CloseStream();
	}
	std::vector<DeviceInfo> EnumerateDevices()
	{
		std::scoped_lock lock(s_mutex);
		std::vector<DeviceInfo> infos;
		const auto deviceCount = Cap_getDeviceCount(s_ctx);
		for (CapDeviceID deviceNo = 0; deviceNo < deviceCount; ++deviceNo)
		{
			const auto uniqueId = Cap_getDeviceUniqueID(s_ctx, deviceNo);
			const auto name = Cap_getDeviceName(s_ctx, deviceNo);
			DeviceInfo info;
			info.uniqueId = uniqueId;

			if (name)
				info.name = fmt::format("{}: {}", deviceNo + 1, name);
			else
				info.name = fmt::format("{}: Unknown", deviceNo + 1);
			infos.push_back(info);
		}
		return infos;
	}
	void SaveDevice()
	{
		std::scoped_lock lock(s_mutex);
		if (s_device)
			GetConfig().camera_id = Cap_getDeviceUniqueID(s_ctx, *s_device);
		else
			GetConfig().camera_id = "";
	}
} // namespace CameraManager