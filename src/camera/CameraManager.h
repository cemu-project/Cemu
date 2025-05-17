#pragma once
#include <string>

namespace CameraManager
{
	struct DeviceInfo
	{
		std::string uniqueId;
		std::string name;
	};
	constexpr static uint32 DEVICE_NONE = std::numeric_limits<uint32>::max();

	void Init();
	void Deinit();
	void Open();
	void Close();

	void FillNV12Buffer(uint8* nv12Buffer);
	void FillRGBBuffer(uint8* rgbBuffer);

	void SetDevice(uint32 deviceNo);
	std::vector<DeviceInfo> EnumerateDevices();
	void SaveDevice();
} // namespace CameraManager
