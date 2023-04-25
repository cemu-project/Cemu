

//#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)

#include <xaudio2.h>

#ifndef XAUDIO2_DLL
#error wrong <xaudio2.h> included!
#endif

#include "XAudio2API.h"
#include "util/helpers/helpers.h"

#include <WbemCli.h>
#include <OleAuto.h>
#include <system_error>

// guid from mmdeviceapi.h
static const GUID DEVINTERFACE_AUDIO_RENDER_GUID = { 0xe6327cad, 0xdcec, 0x4949, 0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2 };

#pragma comment(lib, "wbemuuid.lib")

static_assert(IAudioAPI::kBlockCount < XAUDIO2_MAX_QUEUED_BUFFERS, "too many xaudio2 buffers");

HMODULE XAudio2API::s_xaudio_dll = nullptr;
std::vector<XAudio2API::DeviceDescriptionPtr> XAudio2API::s_devices;

XAudio2API::XAudio2API(std::wstring device_id, uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample)
	: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample), m_device_id(std::move(device_id))
{
	const auto _XAudio2Create = (decltype(&XAudio2Create))GetProcAddress(s_xaudio_dll, "XAudio2Create");
	if (!_XAudio2Create)
		throw std::runtime_error("can't find XAudio2Create import");

	HRESULT hres;
	IXAudio2* xaudio;
	if (FAILED((hres = _XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR))))
		throw std::runtime_error(fmt::format("can't create xaudio device (hres: {:#x})", hres));

	m_xaudio = decltype(m_xaudio)(xaudio);

	IXAudio2MasteringVoice* mastering_voice;
	if (FAILED((hres = m_xaudio->CreateMasteringVoice(&mastering_voice, channels, samplerate, 0, m_device_id.empty() ? nullptr : m_device_id.c_str()))))
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
	switch(channels)
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

void XAudio2API::XAudioDeleter::operator()(IXAudio2* ptr) const
{
	if (ptr)
		ptr->Release();
}

void XAudio2API::VoiceDeleter::operator()(IXAudio2Voice* ptr) const
{
	if (ptr)
		ptr->DestroyVoice();
}

XAudio2API::~XAudio2API()
{
	if(m_xaudio)
		m_xaudio->StopEngine();

	XAudio2API::Stop();

	m_source_voice.reset();
	m_mastering_voice.reset();
	m_xaudio.reset();
}

void XAudio2API::SetVolume(sint32 volume)
{
	IAudioAPI::SetVolume(volume);
	m_mastering_voice->SetVolume((float)volume / 100.0f);
}

bool XAudio2API::Play()
{
	if (m_playing)
		return true;

	m_playing = SUCCEEDED(m_source_voice->Start());
	return m_playing;
}

bool XAudio2API::Stop()
{
	if (!m_playing)
		return true;

	m_playing = FAILED(m_source_voice->Stop());
	m_source_voice->FlushSourceBuffers();

	return m_playing;
}

bool XAudio2API::InitializeStatic()
{
	if (s_xaudio_dll)
		return true;

	// win 10
	s_xaudio_dll = LoadLibraryEx(XAUDIO2_DLL, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

	try
	{
		if (!s_xaudio_dll)
			throw std::exception();

		const auto _XAudio2Create = (decltype(&XAudio2Create))GetProcAddress(s_xaudio_dll, "XAudio2Create");
		if (!_XAudio2Create)
			throw std::exception();

		RefreshDevices();
		return true;
	}
	catch (const std::exception&)
	{
		if (s_xaudio_dll)
			FreeLibrary(s_xaudio_dll);

		return false;
	}
}

void XAudio2API::Destroy()
{
	if (s_xaudio_dll)
		FreeLibrary(s_xaudio_dll);
}

const std::vector<XAudio2API::DeviceDescriptionPtr>& XAudio2API::RefreshDevices()
{
	s_devices.clear();

	try
	{
		struct IWbemLocator *wbem_locator = nullptr;

		HRESULT hres = CoCreateInstance(__uuidof(WbemLocator), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID*)&wbem_locator);
		if (FAILED(hres) || !wbem_locator)
			throw std::system_error(hres, std::system_category());

		std::shared_ptr<OLECHAR> path(SysAllocString(LR"(\\.\root\cimv2)"), SysFreeString);
		std::shared_ptr<OLECHAR> language(SysAllocString(L"WQL"), SysFreeString);
		std::shared_ptr<OLECHAR> query(SysAllocString(LR"(SELECT Name,DeviceID FROM Win32_PNPEntity WHERE PNPClass = "AudioEndpoint")"), SysFreeString);
		std::shared_ptr<OLECHAR> name_row(SysAllocString(L"Name"), SysFreeString);
		std::shared_ptr<OLECHAR> device_id_row(SysAllocString(L"DeviceID"), SysFreeString);

		IWbemServices *wbem_services = nullptr;
		hres = wbem_locator->ConnectServer(path.get(), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &wbem_services);
		wbem_locator->Release(); // Free memory resources.

		if (FAILED(hres) || !wbem_services)
			throw std::system_error(hres, std::system_category());

		hres = CoSetProxyBlanket(wbem_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
		if (FAILED(hres))
			throw std::system_error(hres, std::system_category());

		IEnumWbemClassObject* wbem_enum = nullptr;
		hres = wbem_services->ExecQuery(language.get(), query.get(), WBEM_FLAG_RETURN_WBEM_COMPLETE | WBEM_FLAG_FORWARD_ONLY, nullptr, &wbem_enum);
		if (FAILED(hres) || !wbem_enum)
			throw std::system_error(hres, std::system_category());

		ULONG returned;
		IWbemClassObject* object[20];
		// WBEM_S_TIMEDOUT
		while (SUCCEEDED(hres = wbem_enum->Next(100, 20, object, &returned)) && returned > 0)
		{
			for (ULONG i = 0; i < returned; ++i)
			{
				std::wstring name, device_id;

				VARIANT var;
				if (SUCCEEDED(object[i]->Get(name_row.get(), 0L, &var, NULL, NULL)) && var.vt == VT_BSTR && var.bstrVal)
				{
					name = var.bstrVal;
					if (SUCCEEDED(object[i]->Get(device_id_row.get(), 0L, &var, NULL, NULL)) && var.vt == VT_BSTR && var.bstrVal)
					{
						std::wstring id = var.bstrVal;

						if(id.find(L"{0.0.0.00000000}") == std::wstring::npos)
						{
							object[i]->Release();
							continue;
						}

						std::replace(id.begin(), id.end(), L'\\', L'#'); // xaudio devices have "#" instead of backslashes
						
						std::wstringstream tmp;
						tmp << L"\\\\?\\" << id << L"#{" << WStringFromGUID(DEVINTERFACE_AUDIO_RENDER_GUID) << L"}";
						device_id = tmp.str();
					}
				}
			
				auto device = std::make_shared<XAudio2DeviceDescription>(name,device_id);
				s_devices.emplace_back(device);

				object[i]->Release();
			}
		}

		// Only add default device if audio devices exist
		if (s_devices.size() > 0) {
			auto default_device = std::make_shared<XAudio2DeviceDescription>(L"Primary Sound Driver", L"");
			s_devices.insert(s_devices.begin(), default_device);
		}
		
		wbem_enum->Release();

		// Clean up
		wbem_services->Release();
	}
	catch (const std::system_error& ex)
	{
		cemuLog_log(LogType::Force, "XAudio2API::RefreshDevices: error while refreshing device list ({} - code: 0x{:08x})", ex.what(), ex.code().value());
	}

	CoUninitialize();
	return s_devices;
}

bool XAudio2API::FeedBlock(sint16* data)
{
	// check if we queued too many blocks
	if (m_blocks_queued >= kBlockCount)
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

bool XAudio2API::NeedAdditionalBlocks() const
{
	return m_blocks_queued < s_audioDelay;
}
