#pragma once

#include "Cafe/OS/libs/snd_core/ax.h"

namespace snd
{
	namespace user
	{
		struct MixControl;

		void MIXInit();
		void MIXInitInputControl(snd_core::AXVPB* voice, uint16 input, uint32 mode);
		void MIXInitDeviceControl(snd_core::AXVPB* voice, uint32 device_type, uint32 index, MixControl* control, uint32 mode);
		void MIXSetDeviceSoundMode(uint32 device, uint32 mode);
		void MIXUpdateSettings();
		void MIXSetInput(snd_core::AXVPB* voice, uint16 input); 
		void MIXDRCInitChannel(snd_core::AXVPB* voice, uint16 mode, uint16 vol1, uint16 vol2, uint16 vol3);
		void MIXAssignChannel(snd_core::AXVPB* voice);
		void MIXInitChannel(snd_core::AXVPB* voice, uint16 mode, uint16 input, uint16 aux1, uint16 aux2, uint16 aux3, uint16 pan, uint16 span, uint16 fader);
		uint32 MIXGetSoundMode();
		void MIXSetSoundMode(uint32 sound_mode);

		void Initialize();
	}
}
