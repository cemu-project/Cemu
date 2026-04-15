#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CameraManager
{
    constexpr uint32_t CAMERA_WIDTH = 640;
    constexpr uint32_t CAMERA_HEIGHT = 480;
    constexpr uint32_t CAMERA_PITCH = 768;

    constexpr uint32_t CAMERA_NV12_BUFFER_SIZE = (CAMERA_HEIGHT * CAMERA_PITCH * 3) >> 1;
    constexpr uint32_t CAMERA_RGB_BUFFER_SIZE = CAMERA_HEIGHT * CAMERA_WIDTH * 3;

    struct DeviceInfo
    {
        std::string uniqueId;
        std::string name;
    };

    void Open();
    void Close();

    void FillNV12Buffer(std::span<uint8_t, CAMERA_NV12_BUFFER_SIZE> nv12Buffer);
    void FillRGBBuffer(std::span<uint8_t, CAMERA_RGB_BUFFER_SIZE> rgbBuffer);

    void SetDevice(uint32_t deviceNo);
    void ResetDevice();
    std::vector<DeviceInfo> EnumerateDevices();
    void SaveDevice();
    std::optional<uint32_t> GetCurrentDevice();
} // namespace CameraManager
