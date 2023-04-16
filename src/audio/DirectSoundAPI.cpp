#include "DirectSoundAPI.h"

#include "gui/wxgui.h"

#include "util/helpers/helpers.h"
#include "gui/guiWrapper.h"

#pragma comment(lib, "Dsound.lib")

std::wstring DirectSoundAPI::DirectSoundDeviceDescription::GetIdentifier() const
{
	return m_guid ? WStringFromGUID(*m_guid) : L"default";
}

DirectSoundAPI::DirectSoundAPI(GUID* guid, sint32 samplerate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample)
	: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample)
{
	LPDIRECTSOUND8 direct_sound;
	if (DirectSoundCreate8(guid, &direct_sound, nullptr) != DS_OK)
		throw std::runtime_error("can't create directsound device");

	m_direct_sound = decltype(m_direct_sound)(direct_sound);

	if (FAILED(m_direct_sound->SetCooperativeLevel(gui_getWindowInfo().window_main.hwnd, DSSCL_PRIORITY)))
		throw std::runtime_error("can't set directsound priority");

	DSBUFFERDESC bd{};
	bd.dwSize = sizeof(DSBUFFERDESC);
	bd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY;
	bd.dwBufferBytes = kBufferCount * m_bytesPerBlock; // kBlockCount * (samples_per_block * channels * (bits_per_sample / 8));
	bd.lpwfxFormat = (LPWAVEFORMATEX)&m_wfx;

	LPDIRECTSOUNDBUFFER sound_buffer;
	if (FAILED(m_direct_sound->CreateSoundBuffer(&bd, &sound_buffer, nullptr)))
		throw std::runtime_error("can't create directsound soundbuffer");

	DSBCAPS caps{};
	caps.dwSize = sizeof(DSBCAPS);
	if (FAILED(sound_buffer->GetCaps(&caps)))
		throw std::runtime_error("can't get directsound soundbuffer size");

	m_sound_buffer_size = caps.dwBufferBytes;

	LPDIRECTSOUNDBUFFER8 sound_buffer8;
	LPDIRECTSOUNDNOTIFY8 notify8;
	sound_buffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&sound_buffer8);

	if (!sound_buffer8)
	{
		sound_buffer->Release();
		throw std::runtime_error("can't get directsound buffer interface");
	}

	m_sound_buffer = decltype(m_sound_buffer)(sound_buffer8);

	sound_buffer->QueryInterface(IID_IDirectSoundNotify8, (void**)&notify8);
	if (!notify8)
	{
		sound_buffer->Release();
		throw std::runtime_error("can't get directsound notify interface");
	}
	m_notify = decltype(m_notify)(notify8);

	sound_buffer->Release();

	{ // initialize sound buffer
		void *ptr1, *ptr2;
		DWORD bytes1, bytes2;
		m_sound_buffer->Lock(0, m_sound_buffer_size, &ptr1, &bytes1, &ptr2, &bytes2, 0);
		memset(ptr1, 0x00, bytes1);
		if (ptr2 && bytes2 > 0)
			memset(ptr2, 0x00, bytes2);
		m_sound_buffer->Unlock(ptr1, bytes1, ptr2, bytes2);
	}

	m_sound_buffer->SetCurrentPosition(0);

	DSBPOSITIONNOTIFY notify[kBufferCount]{};
	for (size_t i = 0; i < kBufferCount; ++i)
	{
		m_notify_event[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		notify[i].hEventNotify = m_notify_event[i];
		//notify[i].dwOffset = ((i*2) + 1) * (m_bytes_per_block / 2);
		notify[i].dwOffset = (i * m_bytesPerBlock);
	}

	if (FAILED(m_notify->SetNotificationPositions(kBufferCount, notify)))
		throw std::runtime_error("can't set directsound notify positions");

	m_running = true;
	m_thread = std::thread(&DirectSoundAPI::AudioThread, this);
#if BOOST_OS_WINDOWS
	SetThreadPriority(m_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
}

void DirectSoundAPI::AudioThread()
{
	while (m_running)
	{
		HRESULT hr = WaitForMultipleObjects(m_notify_event.size(), m_notify_event.data(), FALSE, 10);
		if (WAIT_OBJECT_0 <= hr && hr <= WAIT_OBJECT_0 + kBufferCount)
		{
			// write to the following buffer
			const sint32 position = (hr - WAIT_OBJECT_0 + 1) % kBufferCount;

			void *ptr1, *ptr2;
			DWORD bytes1, bytes2;
			hr = m_sound_buffer->Lock(position * m_bytesPerBlock, m_bytesPerBlock, &ptr1, &bytes1, &ptr2, &bytes2, 0);
			if (hr == DSERR_BUFFERLOST)
			{
				m_sound_buffer->Restore();
				hr = m_sound_buffer->Lock(position * m_bytesPerBlock, m_bytesPerBlock, &ptr1, &bytes1, &ptr2, &bytes2, 0);
			}

			if (FAILED(hr))
			{
				cemuLog_log(LogType::Force, "DirectSound: Dropped audio block due to locking failure");
				continue;
			}

			{
				std::unique_lock lock(m_mutex);
				if (m_buffer.empty())
				{
					//cemuLog_logDebug(LogType::Force, "DirectSound: writing silence");

					// we got no data, just write silence
					memset(ptr1, 0x00, bytes1);
					if (ptr2)
						memset(ptr2, 0x00, bytes2);
				}
				else
				{
					const auto& buffer = m_buffer.front();
					memcpy(ptr1, buffer.get(), bytes1);
					if (ptr2)
						memcpy(ptr2, buffer.get() + bytes1, bytes2);

					m_buffer.pop();
				}
			}

			m_sound_buffer->Unlock(ptr1, bytes1, ptr2, bytes2);
		}
	}
}

DirectSoundAPI::~DirectSoundAPI()
{
	m_running = false;
	DirectSoundAPI::Stop();
	
	if(m_thread.joinable())
		m_thread.join();

	m_notify.reset();
	m_sound_buffer.reset();
	m_direct_sound.reset();

	for(auto entry : m_notify_event)
	{
		if (entry)
			CloseHandle(entry);
	}
}

bool DirectSoundAPI::Play()
{
	if (m_playing)
		return true;

	m_playing = SUCCEEDED(m_sound_buffer->Play(0, 0, DSBPLAY_LOOPING));
	return m_playing;
}

bool DirectSoundAPI::Stop()
{
	if (!m_playing)
		return true;

	m_playing = FAILED(m_sound_buffer->Stop());
	return m_playing;
}

bool DirectSoundAPI::FeedBlock(sint16* data)
{
	std::unique_lock lock(m_mutex);
	if (m_buffer.size() > kBlockCount)
	{
		cemuLog_logDebug(LogType::Force, "dropped direct sound block since too many buffers are queued");
		return false;
	}

	auto tmp = std::make_unique<uint8[]>(m_bytesPerBlock);
	memcpy(tmp.get(), data, m_bytesPerBlock);
	m_buffer.emplace(std::move(tmp));
	return true;
}

void DirectSoundAPI::SetVolume(sint32 volume)
{
	IAudioAPI::SetVolume(volume);

	const LONG value = pow((float)volume / 100.0f, 0.20f) * (DSBVOLUME_MAX - DSBVOLUME_MIN) + DSBVOLUME_MIN;
	m_sound_buffer->SetVolume(value);
}

bool DirectSoundAPI::NeedAdditionalBlocks() const
{
	std::shared_lock lock(m_mutex);
	return m_buffer.size() < s_audioDelay;
}

std::vector<DirectSoundAPI::DeviceDescriptionPtr> DirectSoundAPI::GetDevices()
{
	std::vector<DeviceDescriptionPtr> result;

	DirectSoundEnumerateW(
		[](LPGUID lpGuid, LPCWSTR lpcstrDescription, LPCWSTR lpcstrModule, LPVOID lpContext) -> BOOL
		{
			auto results = (std::vector<DeviceDescriptionPtr>*)lpContext;
			auto obj = std::make_shared<DirectSoundDeviceDescription>(lpcstrDescription, lpGuid);
			results->emplace_back(obj);
			return TRUE;
		}, &result);

	//Exclude default primary sound device if no other sound devices are available
	if (result.size() == 1 && result.at(0).get()->GetIdentifier() == L"default") {
		result.clear();
	}
	
	return result;
}

std::vector<DirectSoundAPI::DeviceDescriptionPtr> DirectSoundAPI::GetInputDevices()
{
	std::vector<DeviceDescriptionPtr> result;

	DirectSoundCaptureEnumerateW(
		[](LPGUID lpGuid, LPCWSTR lpcstrDescription, LPCWSTR lpcstrModule, LPVOID lpContext) -> BOOL
	{
		auto results = (std::vector<DirectSoundDeviceDescriptionPtr>*)lpContext;
		auto obj = std::make_shared<DirectSoundDeviceDescription>(lpcstrDescription, lpGuid);
		results->emplace_back(obj);
		return TRUE;
	}, &result);

	return result;
}
