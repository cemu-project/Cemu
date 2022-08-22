#pragma once

#define DIRECTSOUND_VERSION 0x0800
#include <mmsystem.h>
//#include <mmreg.h>
#include <dsound.h>

#include "IAudioAPI.h"

class DirectSoundAPI : public IAudioAPI
{
public:
	class DirectSoundDeviceDescription : public DeviceDescription
	{
	public:
		DirectSoundDeviceDescription(const std::wstring& name, GUID* guid)
			: DeviceDescription(name), m_guid(guid) { }

		std::wstring GetIdentifier() const override;
		GUID* GetGUID() const { return m_guid; }

	private:
		GUID* m_guid;
	};

	using DirectSoundDeviceDescriptionPtr = std::shared_ptr<DirectSoundDeviceDescription>;

	// output
	DirectSoundAPI(GUID* guid, sint32 samplerate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample);
	~DirectSoundAPI();

	AudioAPI GetType() const override { return DirectSound; }

	bool Play() override;
	bool Stop() override;
	bool FeedBlock(sint16* data) override;
	void SetVolume(sint32 volume) override;
	bool NeedAdditionalBlocks() const override;

	static std::vector<DeviceDescriptionPtr> GetDevices();
	static std::vector<DeviceDescriptionPtr> GetInputDevices();

private:
	struct DirectSoundDeleter
	{
		void operator()(IUnknown* ptr) const { if (ptr) ptr->Release(); }
	};

	std::unique_ptr<IDirectSound8, DirectSoundDeleter> m_direct_sound;
	//std::unique_ptr<IDirectSoundCapture8, DirectSoundDeleter> m_direct_sound_capture;
	std::unique_ptr<IDirectSoundBuffer8, DirectSoundDeleter> m_sound_buffer;
	std::unique_ptr<IDirectSoundNotify8, DirectSoundDeleter> m_notify;

	DWORD m_sound_buffer_size = 0;
	uint32_t m_offset = 0;
	bool m_data_written = false;

	static const uint32 kBufferCount = 4;
	std::array<HANDLE, kBufferCount> m_notify_event{};
	mutable std::shared_mutex m_mutex;

	std::queue<std::unique_ptr<uint8[]>> m_buffer;
	std::thread m_thread;
	std::atomic_bool m_running = false;
	void AudioThread();
};
