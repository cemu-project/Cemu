#include "CameraManager.h"

#include "config/CemuConfig.h"
#include "util/helpers/helpers.h"
#include "Rgb2Nv12.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <thread>

#include <openpnp-capture.h>

namespace CameraManager
{
    static std::mutex s_mutex;
    static CapContext s_ctx;
    static std::optional<CapDeviceID> s_device;
    static std::optional<CapStream> s_stream;
    static uint8_t* s_rgbBuffer;
    static uint8_t* s_nv12Buffer;
    static int s_refCount = 0;
    static std::thread s_captureThread;
    static std::atomic_bool s_capturing = false;
    static std::atomic_bool s_running = false;

    static std::string FourCC(uint32le value)
    {
        return {
            static_cast<char>((value >> 0) & 0xFF),
            static_cast<char>((value >> 8) & 0xFF),
            static_cast<char>((value >> 16) & 0xFF),
            static_cast<char>((value >> 24) & 0xFF)
        };
    }

    static void CaptureLogFunction(uint32_t level, const char* string)
    {
        cemuLog_log(LogType::InputAPI, "openpnp-capture: {}: {}", level, string);
    }

    static std::optional<CapFormatID> FindCorrectFormat()
    {
        const auto device = *s_device;
        cemuLog_log(LogType::InputAPI, "Video capture device '{}' available formats:",
                    Cap_getDeviceName(s_ctx, device));
        const auto formatCount = Cap_getNumFormats(s_ctx, device);
        for (int32_t formatId = 0; formatId < formatCount; ++formatId)
        {
            CapFormatInfo formatInfo;
            if (Cap_getFormatInfo(s_ctx, device, formatId, &formatInfo) != CAPRESULT_OK)
                continue;
            cemuLog_log(LogType::Force, "{}: {} {}x{} @ {} fps, {} bpp", formatId, FourCC(formatInfo.fourcc),
                        formatInfo.width, formatInfo.height, formatInfo.fps, formatInfo.bpp);
            if (formatInfo.width == CAMERA_WIDTH && formatInfo.height == CAMERA_HEIGHT)
            {
                cemuLog_log(LogType::Force, "Selected video format {}", formatId);
                return formatId;
            }
        }
        cemuLog_log(LogType::Force, "Failed to find suitable video format");
        return std::nullopt;
    }

    static void CaptureWorkerFunc()
    {
        SetThreadName("CameraManager");
        while (s_running)
        {
            while (s_capturing)
            {
                if (s_mutex.try_lock() && s_stream && Cap_hasNewFrame(s_ctx, *s_stream) &&
                    Cap_captureFrame(s_ctx, *s_stream, s_rgbBuffer, CAMERA_RGB_BUFFER_SIZE) == CAPRESULT_OK)
                {
                    Rgb2Nv12(s_rgbBuffer, CAMERA_WIDTH, CAMERA_HEIGHT, s_nv12Buffer, CAMERA_PITCH);
                    s_mutex.unlock();
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::this_thread::yield();
        }
    }

    static void OpenStream()
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

    static void CloseStream()
    {
        s_capturing = false;
        if (s_stream)
        {
            Cap_closeStream(s_ctx, *s_stream);
            s_stream = std::nullopt;
        }
    }

    static void ResetBuffers()
    {
        std::fill_n(s_rgbBuffer, CAMERA_RGB_BUFFER_SIZE, 0);
        constexpr static auto PIXEL_COUNT = CAMERA_HEIGHT * CAMERA_PITCH;
        std::ranges::fill_n(s_nv12Buffer, PIXEL_COUNT, 16);
        std::ranges::fill_n(s_nv12Buffer + PIXEL_COUNT, (PIXEL_COUNT / 2), 128);
    }

    static std::vector<DeviceInfo> InternalEnumerateDevices()
    {
        std::vector<DeviceInfo> infos;
        const auto deviceCount = Cap_getDeviceCount(s_ctx);
        cemuLog_log(LogType::InputAPI, "Available video capture devices:");
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
            cemuLog_log(LogType::InputAPI, "{}", info.name);
        }
        return infos;
    }

    static void Init()
    {
        s_running = true;
        s_ctx = Cap_createContext();
        Cap_setLogLevel(4);
        Cap_installCustomLogFunction(CaptureLogFunction);
        s_rgbBuffer = new uint8[CAMERA_RGB_BUFFER_SIZE];
        s_nv12Buffer = new uint8[CAMERA_NV12_BUFFER_SIZE];

        s_captureThread = std::thread(&CaptureWorkerFunc);

        const auto uniqueId = GetConfig().camera_id.GetValue();
        if (!uniqueId.empty())
        {
            const auto devices = InternalEnumerateDevices();
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

    static void Deinit()
    {
        CloseStream();
        Cap_releaseContext(s_ctx);
        s_running = false;
        s_captureThread.join();
        delete[] s_rgbBuffer;
        delete[] s_nv12Buffer;
    }

    void FillNV12Buffer(std::span<uint8, CAMERA_NV12_BUFFER_SIZE> nv12Buffer)
    {
        std::scoped_lock lock(s_mutex);
        std::ranges::copy_n(s_nv12Buffer, CAMERA_NV12_BUFFER_SIZE, nv12Buffer.data());
    }

    void FillRGBBuffer(std::span<uint8, CAMERA_RGB_BUFFER_SIZE> rgbBuffer)
    {
        std::scoped_lock lock(s_mutex);
        std::ranges::copy_n(s_rgbBuffer, CAMERA_RGB_BUFFER_SIZE, rgbBuffer.data());
    }

    void SetDevice(uint32 deviceNo)
    {
        std::scoped_lock lock(s_mutex);
        CloseStream();
        s_device = deviceNo;
        if (s_refCount != 0)
            OpenStream();
    }

    void ResetDevice()
    {
        std::scoped_lock lock(s_mutex);
        CloseStream();
        s_device = std::nullopt;
        ResetBuffers();
    }

    void Open()
    {
        std::scoped_lock lock(s_mutex);
        if (!s_running)
        {
            Init();
        }
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
        Deinit();
    }

    std::vector<DeviceInfo> EnumerateDevices()
    {
        std::scoped_lock lock(s_mutex);
        return InternalEnumerateDevices();
    }

    void SaveDevice()
    {
        std::scoped_lock lock(s_mutex);
        const std::string cameraId = s_device ? Cap_getDeviceUniqueID(s_ctx, *s_device) : "";
        GetConfig().camera_id.SetValue(cameraId);
        GetConfigHandle().Save();
    }

    std::optional<uint32> GetCurrentDevice()
    {
        return s_device;
    }
} // namespace CameraManager
