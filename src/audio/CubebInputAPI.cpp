#include "CubebInputAPI.h"

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

long CubebInputAPI::data_cb(cubeb_stream* stream, void* user, const void* inputbuffer, void* outputbuffer, long nframes)
{
	auto* thisptr = (CubebInputAPI*)user;

	const auto size = (size_t)nframes * thisptr->m_channels * (thisptr->m_bitsPerSample / 8);

	std::unique_lock lock(thisptr->m_mutex);
	if (thisptr->m_buffer.capacity() <= thisptr->m_buffer.size() + size)
	{
		cemuLog_logDebug(LogType::Force, "dropped input sound block since too many buffers are queued");
		return nframes;
	}

	thisptr->m_buffer.insert(thisptr->m_buffer.end(), (uint8*)inputbuffer, (uint8*)inputbuffer + size);

	return nframes;
}

CubebInputAPI::CubebInputAPI(cubeb_devid devid, uint32 samplerate, uint32 channels, uint32 samples_per_block,
                   uint32 bits_per_sample)
	: IAudioInputAPI(samplerate, channels, samples_per_block, bits_per_sample)
{
	cubeb_stream_params input_params;

	input_params.format = CUBEB_SAMPLE_S16LE;
	input_params.rate = samplerate;
	input_params.channels = channels;
	input_params.prefs = CUBEB_STREAM_PREF_NONE;

	switch (channels)
	{
	case 8:
		input_params.layout = CUBEB_LAYOUT_3F4_LFE;
		break;
	case 6:
		input_params.layout = CUBEB_LAYOUT_QUAD_LFE | CHANNEL_FRONT_CENTER;
		break;
	case 4:
		input_params.layout = CUBEB_LAYOUT_QUAD;
		break;
	case 2:
		input_params.layout = CUBEB_LAYOUT_STEREO;
		break;
	default:
		input_params.layout = CUBEB_LAYOUT_MONO;
		break;
	}

	uint32 latency = 1;
	cubeb_get_min_latency(s_context, &input_params, &latency);

	m_buffer.reserve((size_t)m_bytesPerBlock * kBlockCount);

	if (cubeb_stream_init(s_context, &m_stream, "Cemu Cubeb input",
	                      devid, &input_params,
                          nullptr, nullptr,
	                      latency, data_cb, state_cb, this) != CUBEB_OK)
	{
		throw std::runtime_error("can't initialize cubeb device");
	}
}

CubebInputAPI::~CubebInputAPI()
{
	if (m_stream)
	{
		Stop();
		cubeb_stream_destroy(m_stream);
	}
}

bool CubebInputAPI::ConsumeBlock(sint16* data)
{
	std::unique_lock lock(m_mutex);
	if (m_buffer.empty())
	{
		// we got no data, just write silence
		memset(data, 0x00, m_bytesPerBlock);
	}
	else
	{
		const auto copied = std::min(m_buffer.size(), (size_t)m_bytesPerBlock);
		memcpy(data, m_buffer.data(), copied);
		m_buffer.erase(m_buffer.begin(), std::next(m_buffer.begin(), copied));
		lock.unlock();
		// fill rest with silence
		if (copied != m_bytesPerBlock)
			memset((uint8*)data + copied, 0x00, m_bytesPerBlock - copied);
	}

	return true;
}

bool CubebInputAPI::Play()
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

bool CubebInputAPI::Stop()
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

void CubebInputAPI::SetVolume(sint32 volume)
{
	IAudioInputAPI::SetVolume(volume);
	cubeb_stream_set_volume(m_stream, (float)volume / 100.0f);
}

bool CubebInputAPI::InitializeStatic()
{
	if (cubeb_init(&s_context, "Cemu Input Cubeb", nullptr))
	{
		cemuLog_log(LogType::Force, "can't create cubeb audio api");
		return false;
	}

	return true;
}

void CubebInputAPI::Destroy()
{
	if (s_context)
		cubeb_destroy(s_context);
}

std::vector<IAudioInputAPI::DeviceDescriptionPtr> CubebInputAPI::GetDevices()
{
	cubeb_device_collection devices;
	if (cubeb_enumerate_devices(s_context, CUBEB_DEVICE_TYPE_INPUT, &devices) != CUBEB_OK)
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
