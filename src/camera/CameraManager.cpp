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
	CapContext s_ctx;
	std::optional<CapDeviceID> s_device;
	std::optional<CapStream> s_stream;
	std::array<uint8, CAMERA_WIDTH * CAMERA_HEIGHT * 3> s_rgbBuffer;
	std::array<uint8, CAMERA_PITCH * CAMERA_HEIGHT * 3 / 2> s_nv12Buffer;
	int s_refCount = 0;
	std::thread s_captureThread;
	std::atomic_bool s_capturing = false;
	std::atomic_bool s_running = false;

	std::string FourCC(uint32le value)
	{
		return {
			static_cast<char>((value >> 0) & 0xFF),
			static_cast<char>((value >> 8) & 0xFF),
			static_cast<char>((value >> 16) & 0xFF),
			static_cast<char>((value >> 24) & 0xFF)};
	}

	void CaptureLogFunction(uint32_t level, const char* string)
	{
		cemuLog_log(LogType::CameraAPI, "OpenPNPCapture: {}: {}", level, string);
	}

	std::optional<CapFormatID> FindCorrectFormat()
	{
		const auto device = *s_device;
		cemuLog_log(LogType::CameraAPI, "Video capture device '{}' available formats:", Cap_getDeviceName(s_ctx, device));
		const auto formatCount = Cap_getNumFormats(s_ctx, device);
		for (int32_t formatId = 0; formatId < formatCount; ++formatId)
		{
			CapFormatInfo formatInfo;
			if (Cap_getFormatInfo(s_ctx, device, formatId, &formatInfo) != CAPRESULT_OK)
				continue;
			cemuLog_log(LogType::CameraAPI, "{}: {} {}x{} @ {} fps, {} bpp", formatId, FourCC(formatInfo.fourcc), formatInfo.width, formatInfo.height, formatInfo.fps, formatInfo.bpp);
			if (formatInfo.width == CAMERA_WIDTH && formatInfo.height == CAMERA_HEIGHT)
			{
				cemuLog_log(LogType::CameraAPI, "Selected video format {}", formatId);
				return formatId;
			}
		}
		cemuLog_log(LogType::CameraAPI, "Failed to find suitable video format");
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
		constexpr auto pixCount = CAMERA_HEIGHT * CAMERA_PITCH;
		std::ranges::fill_n(s_nv12Buffer.begin(), pixCount, 16);
		std::ranges::fill_n(s_nv12Buffer.begin() + pixCount, (pixCount / 2), 128);
	}

	void Init()
	{
		{
			std::scoped_lock lock(s_mutex);
			if (s_running)
				return;
			s_running = true;
			s_ctx = Cap_createContext();
			Cap_setLogLevel(4);
			Cap_installCustomLogFunction(CaptureLogFunction);
		}

		s_captureThread = std::thread(&CaptureWorker);

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
		s_running = false;
		s_captureThread.join();
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
		cemuLog_log(LogType::CameraAPI, "Available video capture devices:");
		for (CapDeviceID deviceNo = 0; deviceNo < deviceCount; ++deviceNo)
		{
			const auto uniqueId = Cap_getDeviceUniqueID(s_ctx, deviceNo);
			const auto name = Cap_getDeviceName(s_ctx, deviceNo);
			DeviceInfo info;
			info.uniqueId = uniqueId;

			if (name)
				info.name = fmt::format("{}: {}", deviceNo, name);
			else
				info.name = fmt::format("{}: Unknown", deviceNo);
			infos.push_back(info);
			cemuLog_log(LogType::CameraAPI, "{}", info.name);
		}
		if (infos.empty())
			cemuLog_log(LogType::CameraAPI, "No available video capture devices");
		return infos;
	}
	void SaveDevice()
	{
		std::scoped_lock lock(s_mutex);
		auto& config = GetConfig();
		const auto cameraId = s_device ? Cap_getDeviceUniqueID(s_ctx, *s_device) : "";
		config.camera_id = cameraId;
	}

	std::optional<uint32> GetCurrentDevice()
	{
		return s_device;
	}
} // namespace CameraManager