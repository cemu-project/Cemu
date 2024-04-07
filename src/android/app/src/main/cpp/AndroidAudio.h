#pragma once

#include "audio/IAudioAPI.h"

namespace AndroidAudio
{
	void createAudioDevice(IAudioAPI::AudioAPI audioApi, sint32 channels, sint32 volume, bool isTV = true);
	void setAudioVolume(sint32 volume, bool isTV = true);
}; // namespace AndroidAudio
