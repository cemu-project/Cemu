#include "CameraManager.h"

#include "config/CemuConfig.h"
#include "util/helpers/helpers.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <thread>

#include <SDL3/SDL_camera.h>
#include <SDL3/SDL_init.h>

namespace CameraManager
{
    namespace
    {
        struct InternalDeviceInfo
        {
            SDL_CameraID id;
            std::string name;
        };
        std::mutex s_deviceMutex;
        std::mutex s_bufferMutex;
        std::optional<SDL_CameraID> s_deviceId;
        SDL_Camera* s_camera;
        std::unique_ptr<uint8[]> s_rgbBufferOut;
        std::unique_ptr<uint8[]> s_nv12BufferOut;
        int s_refCount = 0;
        std::thread s_captureThread;
        std::atomic_bool s_capturing = false;
        std::atomic_bool s_running = false;
    }

    static void CaptureWorkerFunc()
    {
        SetThreadName("CameraManager");
        auto nv12Buffer =  std::unique_ptr<uint8[]>(new uint8[CAMERA_NV12_BUFFER_SIZE]);
        while (s_running)
        {
            while (s_capturing)
            {
                if (auto deviceLock = std::unique_lock(s_deviceMutex, std::try_to_lock); deviceLock && s_camera)
                {
                    if (auto frame = SDL_AcquireCameraFrame(s_camera, nullptr))
                    {
                        if (frame->format != SDL_PIXELFORMAT_NV12)
                            return;
                        if (SDL_MUSTLOCK(frame))
                            SDL_LockSurface(frame);
                        const auto byteRowCount = (frame->h * 3) >> 1;
                        for (auto row = 0; row < byteRowCount; ++row)
                        {
                            const auto lineIn = static_cast<const uint8*>(frame->pixels) + frame->pitch * row;
                            const auto lineOut = nv12Buffer.get() + CAMERA_PITCH * row;
                            std::memcpy(lineOut, lineIn, frame->w);
                        }
                        if (SDL_MUSTLOCK(frame))
                            SDL_UnlockSurface(frame);
                        std::scoped_lock lock(s_bufferMutex);
                        std::swap(s_nv12BufferOut, nv12Buffer);
                        SDL_ReleaseCameraFrame(s_camera, frame);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::this_thread::yield();
        }
    }

    static void OpenStream()
    {
        SDL_CameraSpec cameraSpec;
        cameraSpec.format = SDL_PIXELFORMAT_NV12;
        cameraSpec.colorspace = SDL_COLORSPACE_BT601_LIMITED;
        cameraSpec.framerate_numerator = 30;
        cameraSpec.framerate_denominator = 1;
        cameraSpec.width = CAMERA_WIDTH;
        cameraSpec.height = CAMERA_HEIGHT;
        const auto camera = SDL_OpenCamera(*s_deviceId, &cameraSpec);
        if (camera == nullptr)
            return;

        if (SDL_GetCameraFormat(camera, &cameraSpec) && cameraSpec.format != SDL_PIXELFORMAT_NV12)
        {
            cemuLog_log(LogType::Force, "Camera output format is NV12");
            SDL_CloseCamera(camera);
            return;
        }
        s_capturing = true;
        s_camera = camera;
    }

    static void CloseStream()
    {
        s_capturing = false;
        if (s_camera)
        {
            SDL_CloseCamera(s_camera);
            s_camera = nullptr;
        }
    }

    static void ResetBuffers()
    {
        std::scoped_lock lock(s_bufferMutex);
        std::fill_n(s_rgbBufferOut.get(), CAMERA_RGB_BUFFER_SIZE, 0);
        constexpr static auto PIXEL_COUNT = CAMERA_HEIGHT * CAMERA_PITCH;
        std::ranges::fill_n(s_nv12BufferOut.get(), PIXEL_COUNT, 16);
        std::ranges::fill_n(s_nv12BufferOut.get() + PIXEL_COUNT, (PIXEL_COUNT / 2), 128);
    }

    static std::vector<InternalDeviceInfo> InternalEnumerateDevices()
    {
        std::vector<InternalDeviceInfo> infos;
        int deviceCount = 0;
        auto devices = SDL_GetCameras(&deviceCount);
        if (devices == nullptr)
        {
            cemuLog_log(LogType::Force, "{}", SDL_GetError());
            return {};
        }
        cemuLog_log(LogType::InputAPI, "Available video capture devices:");
        for (auto cameraId : std::span(devices, deviceCount))
        {
            const auto name = SDL_GetCameraName(cameraId);
            infos.emplace_back(cameraId, name ? name : "");
        }
        SDL_free(devices);
        return infos;
    }

    static void Init()
    {
        SDL_InitSubSystem(SDL_INIT_CAMERA);
        s_running = true;
        s_rgbBufferOut.reset(new uint8[CAMERA_RGB_BUFFER_SIZE]);
        s_nv12BufferOut.reset(new uint8[CAMERA_NV12_BUFFER_SIZE]);

        s_captureThread = std::thread(&CaptureWorkerFunc);

        const auto deviceName = GetConfig().camera_id.GetValue();
        if (!deviceName.empty())
        {
            for (auto device : InternalEnumerateDevices())
            {
                if (device.name == deviceName)
                {
                    s_deviceId = device.id;
                }
            }
        }
        ResetBuffers();
    }

    static void Deinit()
    {
        CloseStream();
        s_running = false;
        s_captureThread.join();
        s_nv12BufferOut.reset();
        s_rgbBufferOut.reset();
        SDL_QuitSubSystem(SDL_INIT_CAMERA);
    }

    void FillNV12Buffer(std::span<uint8, CAMERA_NV12_BUFFER_SIZE> nv12Buffer)
    {
        std::scoped_lock lock(s_bufferMutex);
        std::ranges::copy_n(s_nv12BufferOut.get(), CAMERA_NV12_BUFFER_SIZE, nv12Buffer.data());
    }

    static uint8 ClampU8(int v)
    {
        v =  (v > 255) ? 255 : v;
        v =  (v < 0) ? 0 : v;
        return v;
    }

    void FillRGBBuffer(std::span<uint8, CAMERA_RGB_BUFFER_SIZE> rgbBuffer)
    {
        std::scoped_lock lock(s_bufferMutex);
        const auto* yPtr = s_nv12BufferOut.get();
        const auto* uvPtr = s_nv12BufferOut.get() + CAMERA_PITCH * CAMERA_HEIGHT;
        auto* rgbPtr = rgbBuffer.data();
        for (auto row = 0; row < CAMERA_HEIGHT; ++row)
        {
            const auto yRow = yPtr + row * CAMERA_PITCH;
            const auto uvRow = uvPtr + (row >> 1) * CAMERA_PITCH;
            const auto rgbRow = rgbPtr + row * CAMERA_WIDTH * 3;
            for (auto col = 0; col < CAMERA_WIDTH; ++col)
            {
                const auto _y = 19 * (yRow[col] - 16);
                const auto uvCol = col >> 1;
                const auto u = uvRow[(uvCol << 1) + 0];
                const auto v = uvRow[(uvCol << 1) + 1];
                rgbRow[col * 3 + 0] = ClampU8((_y + 26 * (v - 128)) >> 4);
                rgbRow[col * 3 + 1] = ClampU8((_y - 13 * (v - 128) - 6 * (u - 128)) >> 4);
                rgbRow[col * 3 + 2] = ClampU8((_y + 32 * (u - 128)) >> 4);;
            }
        }
    }

    void SetDevice(std::string_view deviceId)
    {
        std::scoped_lock lock(s_deviceMutex);
        CloseStream();
        SDL_CameraID id;
        auto [it, ec] = std::from_chars(deviceId.begin(), deviceId.end(), id);
        if (ec != std::errc{})
            return;
        s_deviceId = id;
        if (s_refCount != 0)
            OpenStream();
    }

    void ResetDevice()
    {
        std::scoped_lock lock(s_deviceMutex);
        CloseStream();
        s_deviceId = std::nullopt;
        ResetBuffers();
    }


    void Open()
    {
        std::scoped_lock lock(s_deviceMutex);
        if (!s_running)
        {
            Init();
        }
        if (s_deviceId && s_refCount == 0)
        {
            OpenStream();
        }
        s_refCount += 1;
    }

    void Close()
    {
        std::scoped_lock lock(s_deviceMutex);
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
        std::scoped_lock lock(s_deviceMutex);
        std::vector<DeviceInfo> deviceInfos;
        for (const auto& [id, name] : InternalEnumerateDevices())
        {
            deviceInfos.emplace_back(fmt::to_string(id), name);
        }
        return deviceInfos;
    }

    void SaveDevice()
    {
        std::scoped_lock lock(s_deviceMutex);
        const std::string cameraId = s_deviceId ? SDL_GetCameraName(*s_deviceId) : "";
        GetConfig().camera_id.SetValue(cameraId);
        GetConfigHandle().Save();
    }

    std::optional<std::string> GetCurrentDevice()
    {
        if (s_deviceId)
            return std::to_string(*s_deviceId);
        return std::nullopt;
    }
} // namespace CameraManager
