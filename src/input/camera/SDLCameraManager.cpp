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
        std::atomic<SDL_Camera*> s_camera;
        std::unique_ptr<uint8[]> s_rgbBufferOut;
        std::unique_ptr<uint8[]> s_nv12BufferOut;
        int s_refCount = 0;
        std::thread s_captureThread;
        std::atomic_bool s_running = false;
        std::atomic_bool s_permissionWasDenied = false;
        std::atomic_bool s_unsupportedFormat = false;
    }

    static void OpenStream()
    {
        s_unsupportedFormat = false;
        s_permissionWasDenied = false;
        SDL_CameraSpec cameraSpec;
        cameraSpec.format = SDL_PIXELFORMAT_NV12;
        cameraSpec.colorspace = SDL_COLORSPACE_BT601_LIMITED;
        cameraSpec.framerate_numerator = 30;
        cameraSpec.framerate_denominator = 1;
        cameraSpec.width = CAMERA_WIDTH;
        cameraSpec.height = CAMERA_HEIGHT;
        const auto camera = SDL_OpenCamera(*s_deviceId, &cameraSpec);
        if (camera == nullptr)
        {
            cemuLog_log(LogType::Force, "Failed to open camera: {}", SDL_GetError());
            return;
        }
        const auto permission = SDL_GetCameraPermissionState(camera);
        if (permission == SDL_CAMERA_PERMISSION_STATE_PENDING)
            cemuLog_log(LogType::Force, "Cemu is waiting for permission to access camera");
        s_camera = camera;
    }

    static void CloseStream()
    {
        if (s_camera)
        {
            SDL_CloseCamera(s_camera);
            s_camera = nullptr;
        }
    }

    static void CaptureWorkerFunc()
    {
        SetThreadName("CameraManager");
        auto nv12Buffer = std::unique_ptr<uint8[]>(new uint8[CAMERA_NV12_BUFFER_SIZE]);
        while (s_running)
        {
            if (!s_camera)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::this_thread::yield();
                continue;
            }
            if (auto deviceLock = std::unique_lock(s_deviceMutex, std::try_to_lock))
            {
                const auto camera = s_camera.load();
                if (!camera)
                    continue;
                if (auto frame = SDL_AcquireCameraFrame(camera, nullptr))
                {
                    if (frame->format != SDL_PIXELFORMAT_NV12 || frame->w != CAMERA_WIDTH || frame->h != CAMERA_HEIGHT)
                    {
                        cemuLog_log(LogType::Force, "Camera format is not NV12 {}x{}", CAMERA_WIDTH, CAMERA_HEIGHT);
                        s_unsupportedFormat = true;
                        CloseStream();
                        continue;
                    }
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
                    SDL_ReleaseCameraFrame(s_camera, frame);
                    std::scoped_lock lock(s_bufferMutex);
                    std::swap(s_nv12BufferOut, nv12Buffer);
                }
                else if (auto permission = SDL_GetCameraPermissionState(s_camera); permission == SDL_CAMERA_PERMISSION_STATE_DENIED)
                {
                    s_permissionWasDenied = true;
                    CloseStream();
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
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
        int deviceCount = 0;
        const auto devices = SDL_GetCameras(&deviceCount);
        if (devices == nullptr)
        {
            cemuLog_log(LogType::Force, "Failed to list cameras: {}", SDL_GetError());
            return {};
        }
        if (deviceCount == 0)
            return {};
        std::vector<InternalDeviceInfo> infos;
        cemuLog_log(LogType::Force, "Available video capture devices:");
        for (auto cameraId : std::span(devices, deviceCount))
        {
            const auto name = SDL_GetCameraName(cameraId);
            std::string strName = name ? name : "";
            cemuLog_log(LogType::Force, "\t{}", strName);
            infos.push_back(InternalDeviceInfo{.id = cameraId, .name = std::move(strName)});
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
        v = (v > 255) ? 255 : v;
        v = (v < 0) ? 0 : v;
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
        auto [it, ec] = std::from_chars(deviceId.data(), deviceId.data() + deviceId.size(), id);
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
            deviceInfos.push_back(DeviceInfo{.uniqueId = fmt::to_string(id), .name = name});
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

    State GetState()
    {
        if (!s_deviceId)
            return State::NoDevice;
        if (s_permissionWasDenied)
            return State::NoPermission;
        if (s_unsupportedFormat)
            return State::UnsupportedFormat;
        if (!s_camera)
            return State::NotOpen;
        const auto permission = SDL_GetCameraPermissionState(s_camera);
        if (permission == SDL_CAMERA_PERMISSION_STATE_PENDING)
            return State::NeedsPermission;
        return State::Capturing;
    }
} // namespace CameraManager
