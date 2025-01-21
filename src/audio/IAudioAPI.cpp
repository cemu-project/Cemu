#include "IAudioAPI.h"

#if BOOST_OS_WINDOWS
#include "XAudio2API.h"
#include "XAudio27API.h"
#include "DirectSoundAPI.h"
#endif
#include "config/CemuConfig.h"
#if HAS_CUBEB
#include "CubebAPI.h"
#endif

std::shared_mutex g_audioMutex;
AudioAPIPtr g_tvAudio;
AudioAPIPtr g_padAudio;
AudioAPIPtr g_portalAudio;
std::atomic_int32_t g_padVolume = 0;

uint32 IAudioAPI::s_audioDelay = 2;
std::array<bool, IAudioAPI::AudioAPIEnd> IAudioAPI::s_availableApis{};

IAudioAPI::IAudioAPI(uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample)
	: m_samplerate(samplerate), m_channels(channels), m_samplesPerBlock(samples_per_block), m_bitsPerSample(bits_per_sample)
{
	m_bytesPerBlock = samples_per_block * channels * (bits_per_sample / 8);
	InitWFX(m_samplerate, m_channels, m_bitsPerSample);
}

void IAudioAPI::PrintLogging()
{
	cemuLog_log(LogType::Force, "------- Init Audio backend -------");
	cemuLog_log(LogType::Force, "DirectSound: {}", s_availableApis[DirectSound] ? "available" : "not supported");
	cemuLog_log(LogType::Force, "XAudio 2.8: {}", s_availableApis[XAudio2] ? "available" : "not supported");
	if (!s_availableApis[XAudio2])
	{
		cemuLog_log(LogType::Force, "XAudio 2.7: {}", s_availableApis[XAudio27] ? "available" : "not supported");
	}

	cemuLog_log(LogType::Force, "Cubeb: {}", s_availableApis[Cubeb] ? "available" : "not supported");
}

void IAudioAPI::InitWFX(sint32 samplerate, sint32 channels, sint32 bits_per_sample)
{
#if BOOST_OS_WINDOWS
	// move this to Windows-specific audio API implementations and use a cross-platform format here
	m_wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	m_wfx.Format.nChannels = channels;
	m_wfx.Format.nSamplesPerSec = samplerate;
	m_wfx.Format.wBitsPerSample = bits_per_sample;
	m_wfx.Format.nBlockAlign = (m_wfx.Format.nChannels * m_wfx.Format.wBitsPerSample) / 8; // must equal (nChannels � wBitsPerSample) / 8
	m_wfx.Format.nAvgBytesPerSec = m_wfx.Format.nSamplesPerSec * m_wfx.Format.nBlockAlign; // must equal nSamplesPerSec � nBlockAlign.
	m_wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

	m_wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	m_wfx.Samples.wValidBitsPerSample = bits_per_sample;
	switch (channels)
	{
	case 8:
		m_wfx.dwChannelMask |= (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_FRONT_LEFT_OF_CENTER | SPEAKER_FRONT_RIGHT_OF_CENTER);
		break;
	case 6:
		m_wfx.dwChannelMask |= (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT);
		break;
	case 4:
		m_wfx.dwChannelMask |= (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT);
		break;
	case 2:
		m_wfx.dwChannelMask |= (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
		break;
	default:
		m_wfx.dwChannelMask = 0;
		break;
	}
#endif
}

void IAudioAPI::InitializeStatic()
{
	s_audioDelay = GetConfig().audio_delay;

#if BOOST_OS_WINDOWS
	s_availableApis[DirectSound] = true;
	s_availableApis[XAudio2] = XAudio2API::InitializeStatic();
	if (!s_availableApis[XAudio2]) // don't try to initialize the older lib if the newer version is available
		s_availableApis[XAudio27] = XAudio27API::InitializeStatic();
#endif
#if HAS_CUBEB
	s_availableApis[Cubeb] = CubebAPI::InitializeStatic();
#endif
}

bool IAudioAPI::IsAudioAPIAvailable(AudioAPI api)
{
	if ((size_t)api < s_availableApis.size())
		return s_availableApis[api];

	cemu_assert_debug(false);
	return false;
}

AudioAPIPtr IAudioAPI::CreateDeviceFromConfig(AudioType type, sint32 rate, sint32 samples_per_block, sint32 bits_per_sample)
{
	sint32 channels = CemuConfig::AudioChannelsToNChannels(AudioTypeToChannels(type));
	return CreateDeviceFromConfig(TV, rate, channels, samples_per_block, bits_per_sample);
}

AudioAPIPtr IAudioAPI::CreateDeviceFromConfig(AudioType type, sint32 rate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample)
{
	AudioAPIPtr audioAPIDev;

	auto& config = GetConfig();

	const auto audio_api = (IAudioAPI::AudioAPI)config.audio_api;
	auto selectedDevice = GetDeviceFromType(type);

	if (selectedDevice.empty())
		return {};

	IAudioAPI::DeviceDescriptionPtr device_description;
	if (IAudioAPI::IsAudioAPIAvailable(audio_api))
	{
		auto devices = IAudioAPI::GetDevices(audio_api);
		const auto it = std::find_if(devices.begin(), devices.end(), [&selectedDevice](const auto& d) { return d->GetIdentifier() == selectedDevice; });
		if (it != devices.end())
			device_description = *it;
	}
	if (!device_description)
		throw std::runtime_error("failed to find selected device while trying to create audio device");

	audioAPIDev = CreateDevice(audio_api, device_description, rate, channels, samples_per_block, bits_per_sample);
	audioAPIDev->SetVolume(TV ? config.tv_volume : config.pad_volume);
	return audioAPIDev;
}

AudioAPIPtr IAudioAPI::CreateDevice(AudioAPI api, const DeviceDescriptionPtr& device, sint32 samplerate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample)
{
	if (!IsAudioAPIAvailable(api))
		return {};

	switch (api)
	{
#if BOOST_OS_WINDOWS
	case DirectSound:
	{
		const auto tmp = std::dynamic_pointer_cast<DirectSoundAPI::DirectSoundDeviceDescription>(device);
		return std::make_unique<DirectSoundAPI>(tmp->GetGUID(), samplerate, channels, samples_per_block, bits_per_sample);
	}
	case XAudio27:
	{
		const auto tmp = std::dynamic_pointer_cast<XAudio27API::XAudio27DeviceDescription>(device);
		return std::make_unique<XAudio27API>(tmp->GetDeviceId(), samplerate, channels, samples_per_block, bits_per_sample);
	}
	case XAudio2:
	{
		const auto tmp = std::dynamic_pointer_cast<XAudio2API::XAudio2DeviceDescription>(device);
		return std::make_unique<XAudio2API>(tmp->GetDeviceId(), samplerate, channels, samples_per_block, bits_per_sample);
	}
#endif
#if HAS_CUBEB
	case Cubeb:
	{
		const auto tmp = std::dynamic_pointer_cast<CubebAPI::CubebDeviceDescription>(device);
		return std::make_unique<CubebAPI>(tmp->GetDeviceId(), samplerate, channels, samples_per_block, bits_per_sample);
	}
#endif
	default:
		throw std::runtime_error(fmt::format("invalid audio api: {}", api));
	}
}

std::vector<IAudioAPI::DeviceDescriptionPtr> IAudioAPI::GetDevices(AudioAPI api)
{
	if (!IsAudioAPIAvailable(api))
		return {};

	switch (api)
	{
#if BOOST_OS_WINDOWS
	case DirectSound:
	{
		return DirectSoundAPI::GetDevices();
	}
	case XAudio27:
	{
		return XAudio27API::GetDevices();
	}
	case XAudio2:
	{
		return XAudio2API::GetDevices();
	}
#endif
#if HAS_CUBEB
	case Cubeb:
	{
		return CubebAPI::GetDevices();
	}
#endif
	default:
		throw std::runtime_error(fmt::format("invalid audio api: {}", api));
	}
}

void IAudioAPI::SetAudioDelayOverride(uint32 delay)
{
	m_audioDelayOverride = delay;
}

uint32 IAudioAPI::GetAudioDelay() const
{
	return m_audioDelayOverride > 0 ? m_audioDelayOverride : s_audioDelay;
}

AudioChannels IAudioAPI::AudioTypeToChannels(AudioType type)
{
	auto& config = GetConfig();
	switch (type)
	{
	case TV:
		return config.tv_channels;
	case Gamepad:
		return config.pad_channels;
	case Portal:
		return config.portal_channels;
	default:
		return kMono;
	}
}

std::wstring IAudioAPI::GetDeviceFromType(AudioType type)
{
	auto& config = GetConfig();
	switch (type)
	{
	case TV:
		return config.tv_device;
	case Gamepad:
		return config.pad_device;
	case Portal:
		return config.portal_device;
	default:
		return L"";
	}
}
