#include <sstream>

#include <boost/thread.hpp>

#include "AL/alc.h"
#include "AL/al.h"
#include "AL/alext.h"

#include "OpenALAPI.h"
#include "OpenALLoader.h"

// While OpenAL should support multi-threading, some implementations have shown random issues when multiple threads
// are involved, even when calls are properly guarded with a mutex. So, let's run everything in a single thread
// through a thread pool instead.
static boost::asio::thread_pool s_openal_pool(1);
static constexpr uint32_t TARGET_NUM_BLOCKS = 2;

static std::string oalErrorToString(ALenum error)
{
	switch (error)
	{
		case AL_INVALID_NAME:
			return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:
			return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:
			return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:
			return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:
			return "AL_OUT_OF_MEMORY";
		default:
			std::stringstream ss;
			ss << "Unknown error: " << error;
			return ss.str();
	}
}

static void oalLogAndThrow(std::string msg) {
	cemuLog_log(LogType::SoundAPI, msg);
	throw std::runtime_error(msg);
}

OpenALAPI::OpenALAPI(std::wstring device_name, uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample)
	: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample),
	m_format(AL_NONE),
	m_openal(LoadOpenAL()),
	m_context(nullptr),
	m_device(nullptr),
	m_source(0)
{
	boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		ALenum error;
		m_device = m_openal->alcOpenDevice(boost::nowide::narrow(device_name).c_str());
		if ((error = m_openal->alcGetError(nullptr)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't open device: " + oalErrorToString(error));
		}

		m_context = m_openal->alcCreateContext(m_device, nullptr);
		if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't create context: " + oalErrorToString(error));
		}

		m_openal->alcMakeContextCurrent(m_context);
		if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't make context current: " + oalErrorToString(error));
		}

		m_openal->alGenSources(1, &m_source);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't create OpenAL source: " + oalErrorToString(error));
		}

		switch (channels)
		{
		case 8:
			m_format = AL_FORMAT_71CHN16;
			break;
		case 6:
			m_format = AL_FORMAT_51CHN16;
			break;
		case 4:
			m_format = AL_FORMAT_QUAD16;
			break;
		case 2:
			m_format = AL_FORMAT_STEREO16;
			break;
		default:
			m_format = AL_FORMAT_MONO16;
			break;
		}
	})).get();
}

OpenALAPI::~OpenALAPI()
{
	if (m_device)
	{
		boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
			ALenum error;

			m_openal->alcMakeContextCurrent(m_context);
			if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
			{
				cemuLog_log(LogType::Force, "can't make context current in destructor: " + oalErrorToString(error));
				return;
			}

			StopUnsafe();
			m_openal->alDeleteSources(1, &m_source);
			m_openal->alGetError();
			m_openal->alcMakeContextCurrent(nullptr);
			m_openal->alcDestroyContext(m_context);
			m_openal->alcCloseDevice(m_device);
		})).get();
	}
}

bool OpenALAPI::NeedAdditionalBlocks() const
{
	return boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		ALenum error = AL_NO_ERROR;
		m_openal->alcMakeContextCurrent(m_context);
		if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't make context current: " + oalErrorToString(error));
		}

		ALint num_processed;
		m_openal->alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_processed);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't get number of processed buffers: " + oalErrorToString(error));
		}

		ALint num_queued;
		m_openal->alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't check number of queued buffers: " + oalErrorToString(error));
		}

		// This method is const, we cannot modify the driver state
		return (num_queued - num_processed) < GetAudioDelay();
	})).get();
}

bool OpenALAPI::FeedBlock(sint16* data)
{
	return boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		ALenum error = AL_NO_ERROR;
		m_openal->alcMakeContextCurrent(m_context);
		if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't make context current: " + oalErrorToString(error));
		}

		ALint processed;
		m_openal->alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't get number of processed buffers: " + oalErrorToString(error));
		}

		if (processed > 0)
		{
			std::vector<uint32_t> buffers(processed);
			m_openal->alSourceUnqueueBuffers(m_source, buffers.size(), buffers.data());
			if ((error = m_openal->alGetError()) != AL_NO_ERROR)
			{
				oalLogAndThrow("unable to unqueue buffers: " + oalErrorToString(error));
			}

			m_buffers.insert(m_buffers.end(), buffers.begin(), buffers.end());
		}

		ALint num_queued;
		m_openal->alGetSourcei(m_source, AL_BUFFERS_QUEUED, &num_queued);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't check number of queued buffers: " + oalErrorToString(error));
		}

		// Slight pitch increase (up to 5%) to consume blocks faster and restore target latency
		float_t pitch = 1.0f;
		float_t max_skew = 0.05f;
		if (num_queued > kBlockCount)
		{
			cemuLog_logDebug(LogType::Force, "resetting OpenAL due to too many queued buffers");
			StopUnsafe();
			Play();
			return false;
		}
		else if (num_queued > TARGET_NUM_BLOCKS)
		{
			pitch = 1.0f + (max_skew * static_cast<float>(num_queued) / kBlockCount);
		}
		m_openal->alSourcef(m_source, AL_PITCH, pitch);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't change pitch: " + oalErrorToString(error));
		}

		ALuint buffer = 0;
		if (m_buffers.size() == 0)
		{
			m_openal->alGenBuffers(1, &buffer);
			if ((error = m_openal->alGetError()) != AL_NO_ERROR)
			{
				oalLogAndThrow("can't create buffer: " + oalErrorToString(error));
			}
		}
		else
		{
			buffer = m_buffers.front();
			m_buffers.pop_front();
		}

		m_openal->alBufferData(buffer, m_format, data, m_bytesPerBlock, m_samplerate);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't buffer data: " + oalErrorToString(error));
		}

		m_openal->alSourceQueueBuffers(m_source, 1, &buffer);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't queue buffer: " + oalErrorToString(error));
		}

		if (m_playing)
		{
			ALint state;
			m_openal->alGetSourcei(m_source, AL_SOURCE_STATE, &state);
			if ((error = m_openal->alGetError()) != AL_NO_ERROR)
			{
				oalLogAndThrow("can't get source state buffer: " + oalErrorToString(error));
			}

			if (state != AL_PLAYING)
			{
				m_openal->alSourcePlay(m_source);
				if ((error = m_openal->alGetError()) != AL_NO_ERROR)
				{
					oalLogAndThrow("can't play source: " + oalErrorToString(error));
				}
			}
		}

		return true;
	})).get();
}

bool OpenALAPI::Play()
{
	m_playing = true;
	return m_playing;
}

bool OpenALAPI::Stop()
{
	return boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		return StopUnsafe();
	})).get();
}

bool OpenALAPI::StopUnsafe()
{
	ALenum error = AL_NO_ERROR;
	m_openal->alcMakeContextCurrent(m_context);
	if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
	{
		oalLogAndThrow("can't make context current: " + oalErrorToString(error));
	}

	m_openal->alSourceStop(m_source);
	if ((error = m_openal->alGetError()) != AL_NO_ERROR)
	{
		oalLogAndThrow("can't stop source: " + oalErrorToString(error));
	}

	ALint processed;
	m_openal->alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
	if ((error = m_openal->alGetError()) != AL_NO_ERROR)
	{
		oalLogAndThrow("can't get number of processed buffers: " + oalErrorToString(error));
	}

	if (processed > 0)
	{
		std::vector<ALuint> buffers(processed);
		m_openal->alSourceUnqueueBuffers(m_source, buffers.size(), buffers.data());
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("unable to unqueue buffers: " + oalErrorToString(error));
		}

		buffers.insert(buffers.end(), m_buffers.begin(), m_buffers.end());
		m_buffers.clear();

		m_openal->alDeleteBuffers(buffers.size(), buffers.data());
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("unable to delete buffers: " + oalErrorToString(error));
		}
	}

	ALint queued;
	m_openal->alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
	if ((error = m_openal->alGetError()) != AL_NO_ERROR)
	{
		oalLogAndThrow("can't get number of queued buffers: " + oalErrorToString(error));
	}

	if (queued > 0)
	{
		oalLogAndThrow("queued should be 0 but it is: " + std::to_string(queued));
	}

	m_playing = false;
	return true;
}

void OpenALAPI::SetVolume(sint32 volume)
{
	boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		ALenum error = AL_NO_ERROR;
		m_openal->alcMakeContextCurrent(m_context);
		if ((error = m_openal->alcGetError(m_device)) != AL_NO_ERROR)
		{
			oalLogAndThrow("can't make context current: " + oalErrorToString(error));
		}

		auto gain = static_cast<float>(volume) / 100;
		m_openal->alSourcef(m_source, AL_GAIN, gain);
		if ((error = m_openal->alGetError()) != AL_NO_ERROR)
		{
			oalLogAndThrow("unable to change source gain: " + oalErrorToString(error));
		}
	})).get();
}

std::vector<IAudioAPI::DeviceDescriptionPtr> OpenALAPI::GetDevices()
{
	return boost::asio::post(s_openal_pool, boost::asio::use_future([&]() {
		auto openal = LoadOpenAL();
		std::vector<DeviceDescriptionPtr> result;
		if (openal->alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
		{
			auto device_names = openal->alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
			while (device_names && *device_names)
			{
				result.emplace_back(std::make_shared<OpenALDeviceDescription>(boost::nowide::widen(device_names)));
				device_names += strlen(device_names) + 1;
			}
		}
		else if (openal->alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
		{
			auto device_names = openal->alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
			while (device_names && *device_names)
			{
				result.emplace_back(std::make_shared<OpenALDeviceDescription>(boost::nowide::widen(device_names)));
				device_names += strlen(device_names) + 1;
			}
		}

		return result;
	})).get();
}

bool OpenALAPI::InitializeStatic()
{
	auto openal = LoadOpenAL();
	return openal != nullptr;
}

void OpenALAPI::Destroy()
{
	UnloadOpenAL();
}
