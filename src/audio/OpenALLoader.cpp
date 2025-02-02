#include <sstream>

#include <boost/dll.hpp>

#include "OpenALLoader.h"

static boost::dll::shared_library s_openal_lib;
static OpenALLib* s_loaded_lib;

#if BOOST_OS_WINDOWS
static std::string s_openal_libname = "OpenAL32";
#else
static std::string s_openal_libname = "openal";
#endif

OpenALLib* LoadOpenAL()
{
	// TODO: mutex?
	if (s_loaded_lib)
	{
		return s_loaded_lib;
	}

	if (!s_openal_lib.is_loaded())
	{
		boost::system::error_code ec;
		s_openal_lib.load(s_openal_libname, boost::dll::load_mode::search_system_folders | boost::dll::load_mode::append_decorations, ec);
		if (ec)
		{
			std::stringstream ss;
			ss << "unable to load OpenAL (\"" << s_openal_libname << "\") library due to: \"" << ec.what() << "\"";
			cemuLog_log(LogType::Force, ss.str());
			return nullptr;
		}
	}

	OpenALLib* lib = new OpenALLib;
	lib->alBufferData = s_openal_lib.get<decltype(alBufferData)>("alBufferData");
	lib->alcCloseDevice = s_openal_lib.get<decltype(alcCloseDevice)>("alcCloseDevice");
	lib->alcCreateContext = s_openal_lib.get<decltype(alcCreateContext)>("alcCreateContext");
	lib->alcDestroyContext = s_openal_lib.get<decltype(alcDestroyContext)>("alcDestroyContext");
	lib->alcGetContextsDevice = s_openal_lib.get<decltype(alcGetContextsDevice)>("alcGetContextsDevice");
	lib->alcGetCurrentContext = s_openal_lib.get<decltype(alcGetCurrentContext)>("alcGetCurrentContext");
	lib->alcGetString = s_openal_lib.get<decltype(alcGetString)>("alcGetString");
	lib->alcIsExtensionPresent = s_openal_lib.get<decltype(alcIsExtensionPresent)>("alcIsExtensionPresent");
	lib->alcMakeContextCurrent = s_openal_lib.get<decltype(alcMakeContextCurrent)>("alcMakeContextCurrent");
	lib->alcOpenDevice = s_openal_lib.get<decltype(alcOpenDevice)>("alcOpenDevice");
	lib->alcGetError = s_openal_lib.get<decltype(alcGetError)>("alcGetError");
	lib->alDeleteBuffers = s_openal_lib.get<decltype(alDeleteBuffers)>("alDeleteBuffers");
	lib->alDeleteSources = s_openal_lib.get<decltype(alDeleteSources)>("alDeleteSources");
	lib->alGenBuffers = s_openal_lib.get<decltype(alGenBuffers)>("alGenBuffers");
	lib->alGenSources = s_openal_lib.get<decltype(alGenSources)>("alGenSources");
	lib->alGetError = s_openal_lib.get<decltype(alGetError)>("alGetError");
	lib->alGetSourcei = s_openal_lib.get<decltype(alGetSourcei)>("alGetSourcei");
	lib->alGetString = s_openal_lib.get<decltype(alGetString)>("alGetString");
	lib->alIsExtensionPresent = s_openal_lib.get<decltype(alIsExtensionPresent)>("alIsExtensionPresent");
	lib->alSourcef = s_openal_lib.get<decltype(alSourcef)>("alSourcef");
	lib->alSourcei = s_openal_lib.get<decltype(alSourcei)>("alSourcei");
	lib->alSourcePlay = s_openal_lib.get<decltype(alSourcePlay)>("alSourcePlay");
	lib->alSourceQueueBuffers = s_openal_lib.get<decltype(alSourceQueueBuffers)>("alSourceQueueBuffers");
	lib->alSourceStop = s_openal_lib.get<decltype(alSourceStop)>("alSourceStop");
	lib->alSourceUnqueueBuffers = s_openal_lib.get<decltype(alSourceUnqueueBuffers)>("alSourceUnqueueBuffers");

	s_loaded_lib = lib;
	return s_loaded_lib;
}

void UnloadOpenAL()
{
	delete s_loaded_lib;
	s_loaded_lib = nullptr;
	s_openal_lib.unload();
}
