#pragma once

#include "AL/al.h"
#include "AL/alc.h"

struct OpenALLib
{
	decltype(&alcCloseDevice) alcCloseDevice;
	decltype(&alcCreateContext) alcCreateContext;
	decltype(&alcDestroyContext) alcDestroyContext;
	decltype(&alcGetContextsDevice) alcGetContextsDevice;
	decltype(&alcGetCurrentContext) alcGetCurrentContext;
	decltype(&alcGetString) alcGetString;
	decltype(&alcIsExtensionPresent) alcIsExtensionPresent;
	decltype(&alcMakeContextCurrent) alcMakeContextCurrent;
	decltype(&alcOpenDevice) alcOpenDevice;
	decltype(&alcGetError) alcGetError;

	decltype(&alGenBuffers) alGenBuffers;
	decltype(&alDeleteBuffers) alDeleteBuffers;
	decltype(&alBufferData) alBufferData;

	decltype(&alGenSources) alGenSources;
	decltype(&alDeleteSources) alDeleteSources;
	decltype(&alGetSourcei) alGetSourcei;
	decltype(&alSourcef) alSourcef;
	decltype(&alSourcei) alSourcei;
	decltype(&alSourcePlay) alSourcePlay;
	decltype(&alSourceQueueBuffers) alSourceQueueBuffers;
	decltype(&alSourceStop) alSourceStop;
	decltype(&alSourceUnqueueBuffers) alSourceUnqueueBuffers;

	decltype(&alGetError) alGetError;
	decltype(&alGetString) alGetString;
	decltype(&alIsExtensionPresent) alIsExtensionPresent;
};

OpenALLib* LoadOpenAL();
void UnloadOpenAL();