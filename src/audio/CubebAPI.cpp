#include "CubebAPI.h"

#if BOOST_OS_WINDOWS
#include <combaseapi.h>
#include <mmreg.h>
#include <mmsystem.h>
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "ksuser.lib")
#endif


static void state_cb(cubeb_stream* stream, void* user, cubeb_state state)
{
	if (!stream)
		return;

	/*switch (state)
	{
	case CUBEB_STATE_STARTED:
		fprintf(stderr, "stream started\n");
		break;
	case CUBEB_STATE_STOPPED:
		fprintf(stderr, "stream stopped\n");
		break;
	case CUBEB_STATE_DRAINED:
		fprintf(stderr, "stream drained\n");
		break;
	default:
		fprintf(stderr, "unknown stream state %d\n", state);
	}*/
}

long CubebAPI::data_cb(cubeb_stream* stream, void* user, const void* inputbuffer, void* outputbuffer, long nframes)
{
	auto* thisptr = (CubebAPI*)user;
	//const auto size = (size_t)thisptr->m_bytesPerBlock; // (size_t)nframes* thisptr->m_channels;

	// m_bytesPerBlock = samples_per_block * channels * (bits_per_sample / 8);
	const auto size = (size_t)nframes * thisptr->m_channels * (thisptr->m_bitsPerSample/8);

	std::unique_lock lock(thisptr->m_mutex);
	if (thisptr->m_buffer.empty())
	{
		// we got no data, just write silence
		memset(outputbuffer, 0x00, size);
	}
	else
	{
		const auto copied = std::min(thisptr->m_buffer.size(), size);
		memcpy(outputbuffer, thisptr->m_buffer.data(), copied);
		thisptr->m_buffer.erase(thisptr->m_buffer.begin(), std::next(thisptr->m_buffer.begin(), copied));
		lock.unlock();
		// fill rest with silence
		if (copied != size)
			memset((uint8*)outputbuffer + copied, 0x00, size - copied);
	}

	return nframes;
}

CubebAPI::CubebAPI(cubeb_devid devid, uint32 samplerate, uint32 channels, uint32 samples_per_block,
                   uint32 bits_per_sample)
	: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample)
{
	cubeb_stream_params output_params;

	output_params.format = CUBEB_SAMPLE_S16LE;
	output_params.rate = samplerate;
	output_params.channels = channels;
	output_params.prefs = CUBEB_STREAM_PREF_NONE;

	switch (channels)
	{
	case 8:
		output_params.layout = CUBEB_LAYOUT_3F4_LFE;
		break;
	case 6:
		output_params.layout = CUBEB_LAYOUT_3F2_LFE_BACK;
		break;
	case 4:
		output_params.layout = CUBEB_LAYOUT_QUAD;
		break;
	case 2:
		output_params.layout = CUBEB_LAYOUT_STEREO;
		break;
	default:
		output_params.layout = CUBEB_LAYOUT_MONO;
		break;
	}

	uint32 latency = 1;
	cubeb_get_min_latency(s_context, &output_params, &latency);

	m_buffer.reserve((size_t)m_bytesPerBlock * kBlockCount);

	if (cubeb_stream_init(s_context, &m_stream, "Cemu Cubeb output",
	                      nullptr, nullptr,
	                      devid, &output_params,
	                      latency, data_cb, state_cb, this) != CUBEB_OK)
	{
		throw std::runtime_error("can't initialize cubeb device");
	}
}

CubebAPI::~CubebAPI()
{
	if (m_stream)
	{
		Stop();
		cubeb_stream_destroy(m_stream);
	}
}

bool CubebAPI::NeedAdditionalBlocks() const
{
	std::shared_lock lock(m_mutex);
	return m_buffer.size() < s_audioDelay * m_bytesPerBlock;
}

bool CubebAPI::FeedBlock(sint16* data)
{
	std::unique_lock lock(m_mutex);
	if (m_buffer.capacity() <= m_buffer.size() + m_bytesPerBlock)
	{
		cemuLog_logDebug(LogType::Force, "dropped direct sound block since too many buffers are queued");
		return false;
	}

	m_buffer.insert(m_buffer.end(), (uint8*)data, (uint8*)data + m_bytesPerBlock);
	return true;
}

bool CubebAPI::Play()
{
	if (m_is_playing)
		return true;

	if (cubeb_stream_start(m_stream) == CUBEB_OK)
	{
		m_is_playing = true;
		return true;
	}

	return false;
}

bool CubebAPI::Stop()
{
	if (!m_is_playing)
		return true;

	if (cubeb_stream_stop(m_stream) == CUBEB_OK)
	{
		m_is_playing = false;
		return true;
	}

	return false;
}

void CubebAPI::SetVolume(sint32 volume)
{
	IAudioAPI::SetVolume(volume);
	cubeb_stream_set_volume(m_stream, (float)volume / 100.0f);
}


bool CubebAPI::InitializeStatic()
{
	if (cubeb_init(&s_context, "Cemu Cubeb", nullptr))
	{
		cemuLog_log(LogType::Force, "can't create cubeb audio api");
		return false;
	}
	return true;
}

void CubebAPI::Destroy()
{
	if (s_context)
		cubeb_destroy(s_context);
}

std::vector<IAudioAPI::DeviceDescriptionPtr> CubebAPI::GetDevices()
{
	cubeb_device_collection devices;
	if (cubeb_enumerate_devices(s_context, CUBEB_DEVICE_TYPE_OUTPUT, &devices) != CUBEB_OK)
		return {};

	std::vector<DeviceDescriptionPtr> result;
	result.reserve(devices.count + 1); // Reserve space for the default device

	// Add the default device to the list
	auto defaultDevice = std::make_shared<CubebDeviceDescription>(nullptr, "default", L"Default Device");
	result.emplace_back(defaultDevice);

	for (size_t i = 0; i < devices.count; ++i)
	{
		// const auto& device = devices.device[i];
		if (devices.device[i].state == CUBEB_DEVICE_STATE_ENABLED)
		{
			auto device = std::make_shared<CubebDeviceDescription>(devices.device[i].devid, devices.device[i].device_id,
																   boost::nowide::widen(
																	   devices.device[i].friendly_name));
			result.emplace_back(device);
		}
	}

	cubeb_device_collection_destroy(s_context, &devices);

	return result;
}
