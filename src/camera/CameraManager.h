#pragma once

#pragma once
#include <string>

namespace CameraManager
{
    constexpr uint32 CAMERA_WIDTH = 640;
    constexpr uint32 CAMERA_HEIGHT = 480;
    constexpr uint32 CAMERA_PITCH = 768;

    constexpr uint32 CAMERA_NV12_BUFFER_SIZE = (CAMERA_HEIGHT * CAMERA_PITCH * 3) >> 1;
    constexpr uint32 CAMERA_RGB_BUFFER_SIZE = CAMERA_HEIGHT * CAMERA_WIDTH * 3;

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

    void FillNV12Buffer(std::span<uint8, CAMERA_NV12_BUFFER_SIZE> nv12Buffer);
    void FillRGBBuffer(std::span<uint8, CAMERA_RGB_BUFFER_SIZE> rgbBuffer);

    void SetDevice(uint32 deviceNo);
    std::vector<DeviceInfo> EnumerateDevices();
    void SaveDevice();
    std::optional<uint32> GetCurrentDevice();
} // namespace CameraManager
