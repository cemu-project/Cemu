#include "XAudio27API.h"

#include "../dependencies/DirectX_2010/XAudio2.h"

static_assert(IAudioAPI::kBlockCount < XAUDIO2_MAX_QUEUED_BUFFERS, "too many xaudio2 buffers");

HMODULE XAudio27API::s_xaudio_dll = nullptr;
std::unique_ptr<IXAudio2, XAudio27API::XAudioDeleter> XAudio27API::s_xaudio;

XAudio27API::XAudio27API(uint32 device_id, uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample)
	: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample)
{
	if (!s_xaudio)
		throw std::runtime_error("xaudio 2.7 not initialized!");

	// we use -1 for always selecting the primary device, which is the first one
	if (device_id == -1)
		device_id = 0;

	HRESULT hres;
	IXAudio2* xaudio;
	if (FAILED((hres = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR))))
		throw std::runtime_error(fmt::format("can't create xaudio device (hres: {:#x})", hres));
	m_xaudio = decltype(m_xaudio)(xaudio);

	IXAudio2MasteringVoice* mastering_voice;
	if (FAILED((hres = m_xaudio->CreateMasteringVoice(&mastering_voice, channels, samplerate, 0, device_id))))
		throw std::runtime_error(fmt::format("can't create xaudio mastering voice (hres: {:#x})", hres));

	m_mastering_voice = decltype(m_mastering_voice)(mastering_voice);

	m_wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	m_wfx.Format.nChannels = channels;
	m_wfx.Format.nSamplesPerSec = samplerate;
	m_wfx.Format.wBitsPerSample = bits_per_sample;
	m_wfx.Format.nBlockAlign = (m_wfx.Format.nChannels * m_wfx.Format.wBitsPerSample) / 8; // must equal (nChannels × wBitsPerSample) / 8
	m_wfx.Format.nAvgBytesPerSec = m_wfx.Format.nSamplesPerSec * m_wfx.Format.nBlockAlign; // must equal nSamplesPerSec × nBlockAlign.
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

	IXAudio2SourceVoice* source_voice;
	if (FAILED((hres = m_xaudio->CreateSourceVoice(&source_voice, &m_wfx.Format, 0, 1.0f))))
		throw std::runtime_error(fmt::format("can't create xaudio source voice (hres: {:#x})", hres));
	m_source_voice = decltype(m_source_voice)(source_voice);

	m_sound_buffer_size = kBlockCount * (samples_per_block * channels * (bits_per_sample / 8));

	for (uint32 i = 0; i < kBlockCount; ++i)
		m_audio_buffer[i] = std::make_unique<uint8[]>(m_bytesPerBlock);

	m_xaudio->StartEngine();
}

XAudio27API::~XAudio27API()
{
	if(m_xaudio)
		m_xaudio->StopEngine();

	XAudio27API::Stop();

	m_source_voice.reset();
	m_mastering_voice.reset();
	m_xaudio.reset();
}

void XAudio27API::SetVolume(sint32 volume)
{
	IAudioAPI::SetVolume(volume);
	m_mastering_voice->SetVolume((float)volume / 100.0f);
}

bool XAudio27API::Play()
{
	if (m_playing)
		return true;

	m_playing = SUCCEEDED(m_source_voice->Start());
	return m_playing;
}

bool XAudio27API::Stop()
{
	if (!m_playing)
		return true;

	m_playing = FAILED(m_source_voice->Stop());
	m_source_voice->FlushSourceBuffers();

	return m_playing;
}

bool XAudio27API::InitializeStatic()
{
	if (s_xaudio)
		return true;

#ifdef _DEBUG
	s_xaudio_dll = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if(!s_xaudio_dll)
		s_xaudio_dll = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
	s_xaudio_dll = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif

	try
	{
		if (!s_xaudio_dll)
			throw std::exception();

		IXAudio2* xaudio;
		if (FAILED(XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR)))
			throw std::exception();

		s_xaudio = decltype(s_xaudio)(xaudio);
		return true;
	}
	catch (const std::exception&)
	{
		if (s_xaudio_dll)
			FreeLibrary(s_xaudio_dll);

		return false;
	}
}

void XAudio27API::Destroy()
{
	s_xaudio.reset();

	if (s_xaudio_dll)
		FreeLibrary(s_xaudio_dll);
}

std::vector<XAudio27API::DeviceDescriptionPtr> XAudio27API::GetDevices()
{
	std::vector<DeviceDescriptionPtr> result;

	// always add the default device
	auto default_device = std::make_shared<XAudio27DeviceDescription>(L"Primary Sound Driver", L"", -1);
	result.emplace_back(default_device);

	uint32 count = 0;
	if (FAILED(s_xaudio->GetDeviceCount(&count)))
		return result;

	if (!count)
		return result;

	result.reserve(count + 1);

	// first device is always the primary device
	for (uint32 id = 0; id < count; ++id)
	{
		XAUDIO2_DEVICE_DETAILS details;
		if (SUCCEEDED(s_xaudio->GetDeviceDetails(id, &details)))
		{
			auto device = std::make_shared<XAudio27DeviceDescription>(details.DisplayName, details.DeviceID, id);
			result.emplace_back(device);
		}
	}

	return result;
}

void XAudio27API::XAudioDeleter::operator()(IXAudio2* ptr) const
{
	if (ptr) 
		ptr->Release();
}

void XAudio27API::VoiceDeleter::operator()(IXAudio2Voice* ptr) const
{
	if (ptr) 
		ptr->DestroyVoice();
}

bool XAudio27API::FeedBlock(sint16* data)
{
	// check if we queued too many blocks
	if(m_blocks_queued >= kBlockCount)
	{
		XAUDIO2_VOICE_STATE state{};
		m_source_voice->GetState(&state);
		m_blocks_queued = state.BuffersQueued;

		if (m_blocks_queued >= kBlockCount)
		{
			cemuLog_logDebug(LogType::Force, "dropped xaudio2 block since too many buffers are queued");
			return false;
		}
	}

	memcpy(m_audio_buffer[m_offset].get(), data, m_bytesPerBlock);

	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = m_bytesPerBlock;
	buffer.pAudioData = m_audio_buffer[m_offset].get();
	m_source_voice->SubmitSourceBuffer(&buffer);

	m_offset = (m_offset + 1) % kBlockCount;
	m_blocks_queued++;
	return true;
}

bool XAudio27API::NeedAdditionalBlocks() const
{
	return m_blocks_queued < s_audioDelay;
}
