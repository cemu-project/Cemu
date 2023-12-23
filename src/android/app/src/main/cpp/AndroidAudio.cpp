#include "AndroidAudio.h"

#include "Cafe/OS/libs/snd_core/ax.h"
#include "audio/IAudioAPI.h"

#if HAS_CUBEB
#include "audio/CubebAPI.h"
#endif  // HAS_CUBEB

namespace AndroidAudio
{

void createAudioDevice(IAudioAPI::AudioAPI audioApi, sint32 channels, sint32 volume, bool isTV)
{
    static constexpr int AX_FRAMES_PER_GROUP = 4;
    std::unique_lock lock(g_audioMutex);
    auto& audioDevice = isTV ? g_tvAudio : g_padAudio;
    switch (channels)
    {
        case 0:
            channels = 1;
            break;
        case 2:
            channels = 6;
            break;
        default:  // stereo
            channels = 2;
            break;
    }
    switch (audioApi)
    {
#if HAS_CUBEB
        case IAudioAPI::AudioAPI::Cubeb:
        {
            audioDevice.reset();
            std::shared_ptr<CubebAPI::CubebDeviceDescription> deviceDescriptionPtr = std::make_shared<CubebAPI::CubebDeviceDescription>(nullptr, std::string(), std::wstring());
            audioDevice = IAudioAPI::CreateDevice(IAudioAPI::AudioAPI::Cubeb, deviceDescriptionPtr, 48000, channels, snd_core::AX_SAMPLES_PER_3MS_48KHZ * AX_FRAMES_PER_GROUP, 16);
            audioDevice->SetVolume(volume);
            break;
        }
#endif  // HAS_CUBEB
        default:
            cemuLog_log(LogType::Force, fmt::format("Invalid audio api: {}", audioApi));
            break;
    }
}

void setAudioVolume(sint32 volume, bool isTV)
{
    std::shared_lock lock(g_audioMutex);
    auto& audioDevice = isTV ? g_tvAudio : g_padAudio;
    if (audioDevice)
        audioDevice->SetVolume(volume);
}

};  // namespace AndroidAudio
