#pragma once

#if BOOST_OS_WINDOWS
#include <mmreg.h>
#endif

class IAudioAPI
{
	friend class GeneralSettings2;

public:
	class DeviceDescription
	{
	public:
		explicit DeviceDescription(std::wstring name)
			: m_name(std::move(name)) { }

		virtual ~DeviceDescription() = default;
		virtual std::wstring GetIdentifier() const = 0;
		const std::wstring& GetName() const { return m_name; }

		bool operator==(const DeviceDescription& o) const
		{
			return GetIdentifier() == o.GetIdentifier();
		}

	private:
		std::wstring m_name;
	};

	using DeviceDescriptionPtr = std::shared_ptr<DeviceDescription>;

	enum AudioAPI
	{
		DirectSound = 0,
		XAudio27,
		XAudio2,
		Cubeb,

		AudioAPIEnd,
	};
	static constexpr uint32 kBlockCount = 24;
	
	IAudioAPI(uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample);
	virtual ~IAudioAPI() = default;
	virtual AudioAPI GetType() const = 0;

	sint32 GetChannels() const { return m_channels; }

	virtual sint32 GetVolume() const { return m_volume; }
	virtual void SetVolume(sint32 volume) { m_volume = volume; }
	virtual void SetInputVolume(sint32 volume) { m_inputVolume = volume; }

	virtual bool NeedAdditionalBlocks() const = 0;
	virtual bool FeedBlock(sint16* data) = 0;
	virtual bool Play() = 0;
	virtual bool Stop() = 0;

	static void PrintLogging();
	static void InitializeStatic();
	static bool IsAudioAPIAvailable(AudioAPI api);
	
	static std::unique_ptr<IAudioAPI> CreateDevice(AudioAPI api, const DeviceDescriptionPtr& device, sint32 samplerate, sint32 channels, sint32 samples_per_block, sint32 bits_per_sample);
	static std::vector<DeviceDescriptionPtr> GetDevices(AudioAPI api);

protected:
#if BOOST_OS_WINDOWS
	WAVEFORMATEXTENSIBLE m_wfx{};
#endif
	
	uint32 m_samplerate, m_channels, m_samplesPerBlock, m_bitsPerSample;
	uint32 m_bytesPerBlock;

	sint32 m_volume = 0, m_inputVolume = 0;
	bool m_playing = false;

	static std::array<bool, AudioAPIEnd> s_availableApis;
	static uint32 s_audioDelay;

private:
	void InitWFX(sint32 samplerate, sint32 channels, sint32 bits_per_sample);
	
};

using AudioAPIPtr = std::unique_ptr<IAudioAPI>;
extern std::shared_mutex g_audioMutex;
extern AudioAPIPtr g_tvAudio;

extern AudioAPIPtr g_padAudio;
extern std::atomic_int32_t g_padVolume;
