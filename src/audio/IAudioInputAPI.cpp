#include "IAudioInputAPI.h"
#if HAS_CUBEB
#include "CubebInputAPI.h"
#endif

std::shared_mutex g_audioInputMutex;
AudioInputAPIPtr g_inputAudio;

std::array<bool, IAudioInputAPI::AudioInputAPIEnd> IAudioInputAPI::s_availableApis{};

IAudioInputAPI::IAudioInputAPI(uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample)
	: m_samplerate(samplerate), m_channels(channels), m_samplesPerBlock(samples_per_block), m_bitsPerSample(bits_per_sample) 
{
	m_bytesPerBlock = samples_per_block * channels * (bits_per_sample / 8);
}

void IAudioInputAPI::PrintLogging()
{
	cemuLog_log(LogType::Force, "------- Init Audio input backend -------");
	cemuLog_log(LogType::Force, "Cubeb: {}", s_availableApis[Cubeb] ? "available" : "not supported");
}

void IAudioInputAPI::InitializeStatic()
{
#if HAS_CUBEB
	s_availableApis[Cubeb] = CubebInputAPI::InitializeStatic();
#endif
}

bool IAudioInputAPI::IsAudioInputAPIAvailable(AudioInputAPI api)
{
	if ((size_t)api < s_availableApis.size())
		return s_availableApis[api];

	cemu_assert_debug(false);
	return false;
}

AudioInputAPIPtr IAudioInputAPI::CreateDevice(AudioInputAPI api, const DeviceDescriptionPtr& device, sint32 samplerate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample)
{
	if (!IsAudioInputAPIAvailable(api))
		return {};

	switch(api)
	{
#if HAS_CUBEB
	case Cubeb:
	{
		const auto tmp = std::dynamic_pointer_cast<CubebInputAPI::CubebDeviceDescription>(device);
		return std::make_unique<CubebInputAPI>(tmp->GetDeviceId(), samplerate, channels, samples_per_block, bits_per_sample);
	}
#endif
	default:
		throw std::runtime_error(fmt::format("invalid audio api: {}", api));
	}
}

std::vector<IAudioInputAPI::DeviceDescriptionPtr> IAudioInputAPI::GetDevices(AudioInputAPI api)
{
	if (!IsAudioInputAPIAvailable(api))
		return {};
	
	switch(api)
	{
#if HAS_CUBEB
	case Cubeb:
	{
		return CubebInputAPI::GetDevices();
	}
#endif
	default:
		throw std::runtime_error(fmt::format("invalid audio api: {}", api));
	}
}
