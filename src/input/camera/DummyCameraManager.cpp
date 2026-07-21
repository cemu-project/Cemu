#include "CameraManager.h"

namespace CameraManager
{
    void FillNV12Buffer(std::span<uint8, CAMERA_NV12_BUFFER_SIZE> nv12Buffer)
    {
        constexpr static auto PIXEL_COUNT = CAMERA_HEIGHT * CAMERA_PITCH;
        std::ranges::fill_n(nv12Buffer.data(), PIXEL_COUNT, 16);
        std::ranges::fill_n(nv12Buffer.data() + PIXEL_COUNT, (PIXEL_COUNT / 2), 128);
    }

    void FillRGBBuffer(std::span<uint8, CAMERA_RGB_BUFFER_SIZE> rgbBuffer)
    {
        std::ranges::fill(rgbBuffer, 0);
    }

    void SetDevice(std::string_view deviceId)
    {
    }

    void ResetDevice()
    {
    }

    void Open()
    {
    }

    void Close()
    {
    }

    std::vector<DeviceInfo> EnumerateDevices()
    {
        return {};
    }

    void SaveDevice()
    {
    }

    std::optional<std::string> GetCurrentDevice()
    {
        return std::nullopt;
    }
} // namespace CameraManager
