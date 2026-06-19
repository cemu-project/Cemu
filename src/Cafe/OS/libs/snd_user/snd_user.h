#pragma once

#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/RPL/COSModule.h"

namespace snd_user
{
	struct MixControl;

	enum class MixSoundMode : uint32
	{
		Mono = 0,
		Stereo = 1,
		Surround = 2,
		SurroundDPL2 = 3,
		Surround6CH = 4
	};

	void MIXInit();
	void MIXInitInputControl(snd_core::AXVPB* voice, uint16 input, uint32 mode);
	void MIXInitDeviceControl(snd_core::AXVPB* voice, uint32 device, uint32 deviceSubIndex, MixControl* control, uint32 mode);
	void MIXSetSoundMode(MixSoundMode soundMode);
	void MIXSetDeviceSoundMode(uint32 device, MixSoundMode mode);
	void MIXUpdateSettings();
	void MIXSetInput(snd_core::AXVPB* voice, uint16 input);
	void MIXDRCInitChannel(snd_core::AXVPB* voice, uint32 flags, uint16 aux, uint16 pan, uint16 fader);
	void MIXAssignChannel(snd_core::AXVPB* voice);
	void MIXInitChannel(snd_core::AXVPB* voice, uint32 flags, sint16 input, sint16 aux1, sint16 aux2, sint16 aux3, sint16 pan, sint16 span, sint16 fader);
	void MIXReleaseChannel(snd_core::AXVPB* voice);

	MixSoundMode MIXGetSoundMode();

	COSModule* GetModuleSndUser1();
	COSModule* GetModuleSndUser2();
}
