#pragma once

#include <string>
#include <memory>
#include <deque>

#include "IAudioAPI.h"
#include "OpenALLoader.h"

class OpenALAPI : public IAudioAPI
{
  public:
	class OpenALDeviceDescription : public DeviceDescription
	{
	  public:
		OpenALDeviceDescription(const std::wstring& device_name)
			: DeviceDescription(device_name) {}

		std::wstring GetIdentifier() const override
		{
			return GetName();
		}
	};

	using OpenALDeviceDescriptionPtr = std::shared_ptr<OpenALDeviceDescription>;

	OpenALAPI(std::wstring device_name, uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample);
	~OpenALAPI();

	AudioAPI GetType() const override
	{
		return OpenAL;
	}
	bool NeedAdditionalBlocks() const override;
	bool FeedBlock(sint16* data) override;
	bool Play() override;
	bool Stop() override;
	void SetVolume(sint32 volume) override;

	static std::vector<DeviceDescriptionPtr> GetDevices();

	static bool InitializeStatic();
	static void Destroy();

private:
	bool StopUnsafe();

	uint32_t m_source;
	std::deque<uint32_t> m_buffers;
	uint32_t m_format;

	ALCdevice* m_device;
	ALCcontext* m_context;
	OpenALLib* m_openal;
};
