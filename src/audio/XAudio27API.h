#pragma once

#define DIRECTSOUND_VERSION 0x0800
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>

#include "IAudioAPI.h"

struct IXAudio2;
struct IXAudio2Voice;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

class XAudio27API : public IAudioAPI
{
public:
	class XAudio27DeviceDescription : public DeviceDescription
	{
	public:
		XAudio27DeviceDescription(const std::wstring& name, std::wstring device_id, uint32 id)
			: DeviceDescription(name), m_device_id(std::move(device_id)), m_id(id) { }

		std::wstring GetIdentifier() const override { return m_device_id.empty() ? L"default" : m_device_id; }
		uint32 GetDeviceId() const { return m_id; }

	private:
		std::wstring m_device_id;
		const uint32 m_id;
	};

	using XAudio2DeviceDescriptionPtr = std::shared_ptr<XAudio27DeviceDescription>;

	AudioAPI GetType() const override { return XAudio27; }

	XAudio27API(uint32 device_id, uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample);
	~XAudio27API();
	void SetVolume(sint32 volume) override;

	bool Play() override;
	bool Stop() override;
	bool FeedBlock(sint16* data) override;
	bool NeedAdditionalBlocks() const override;

	static bool InitializeStatic();
	static void Destroy();
	static std::vector<DeviceDescriptionPtr> GetDevices();

private:
	struct XAudioDeleter
	{
		void operator()(IXAudio2* ptr) const;
	};

	struct VoiceDeleter
	{
		void operator()(IXAudio2Voice* ptr) const;
	};

	static HMODULE s_xaudio_dll;
	static std::unique_ptr<IXAudio2, XAudioDeleter> s_xaudio;

	std::unique_ptr<IXAudio2, XAudioDeleter> m_xaudio;
	std::wstring m_device_id;
	std::unique_ptr<IXAudio2MasteringVoice, VoiceDeleter> m_mastering_voice;
	std::unique_ptr<IXAudio2SourceVoice, VoiceDeleter> m_source_voice;
	
	std::unique_ptr<uint8[]> m_audio_buffer[kBlockCount];
	DWORD m_sound_buffer_size = 0;
	uint32_t m_offset = 0;
	uint32_t m_blocks_queued = 0;
};
