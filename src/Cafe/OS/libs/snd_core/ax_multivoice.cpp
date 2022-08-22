#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/snd_core/ax_internal.h"
#include "Cafe/HW/MMU/MMU.h"

namespace snd_core
{
	static_assert(sizeof(AXVPBMULTI) == 0x20, "");

	SysAllocator<AXVPBMULTI, AX_MAX_VOICES> _buffer__AXVPBMultiVoiceArray;
	AXVPBMULTI* __AXVPBMultiVoiceArray;

	void AXMultiVoice_Init()
	{
		__AXVPBMultiVoiceArray = _buffer__AXVPBMultiVoiceArray.GetPtr();
		for (sint32 i = 0; i < AX_MAX_VOICES; i++)
		{
			__AXVPBMultiVoiceArray[i].isUsed = 0;
		}
	}

	sint32 AXAcquireMultiVoice(sint32 voicePriority, void* cbFunc, void* cbData, AXMULTIVOICEUKNSTRUCT* uknR6, MEMPTR<AXVPBMULTI>* multiVoiceOut)
	{
		for (sint32 i = 0; i < AX_MAX_VOICES; i++)
		{
			if (__AXVPBMultiVoiceArray[i].isUsed == (uint32)0)
			{
				sint16 channelCount = uknR6->channelCount;
				if (channelCount <= 0 || channelCount > 6)
				{
					return -0x15;
				}
				for (sint32 f = 0; f < channelCount; f++)
				{
					__AXVPBMultiVoiceArray[i].voice[f] = nullptr;
				}
				__AXVPBMultiVoiceArray[i].isUsed = 1;
				for (sint32 f = 0; f < channelCount; f++)
				{
					AXVPB* vpb = AXAcquireVoiceEx(voicePriority, memory_getVirtualOffsetFromPointer(cbFunc), memory_getVirtualOffsetFromPointer(cbData));
					if (vpb == nullptr)
					{
						AXFreeMultiVoice(__AXVPBMultiVoiceArray + i);
						return -0x16;
					}
					__AXVPBMultiVoiceArray[i].voice[f] = vpb;
				}
				__AXVPBMultiVoiceArray[i].channelCount = channelCount;
				*multiVoiceOut = (__AXVPBMultiVoiceArray+i);
				return 0;
			}
		}
		return -0x14;
	}

	void AXFreeMultiVoice(AXVPBMULTI* multiVoice)
	{
		cemu_assert_debug(multiVoice->isUsed != (uint32)0);
		uint16 numChannels = multiVoice->channelCount;
		for (uint16 i = 0; i < numChannels; i++)
		{
			if(multiVoice->voice[i] != nullptr)
				AXFreeVoice(multiVoice->voice[i].GetPtr());
			multiVoice->voice[i] = nullptr;
		}
		multiVoice->isUsed = 0;
	}

	sint32 AXGetMultiVoiceReformatBufferSize(sint32 voiceFormat, uint32 channelCount, uint32 sizeInBytes, uint32be* sizeOutput)
	{
		// used by Axiom Verge
		if (voiceFormat == AX_FORMAT_ADPCM)
		{
			sint32 alignedSize = (sizeInBytes + 7) & ~7;
			*sizeOutput = alignedSize * channelCount;
		}
		else if (voiceFormat == AX_FORMAT_PCM16)
		{
			*sizeOutput = sizeInBytes;
		}
		else if (voiceFormat == AX_FORMAT_PCM8)
		{
			*sizeOutput = sizeInBytes<<1;
		}
		else
			return -23;
		return 0;
	}

	void AXSetMultiVoiceType(AXVPBMULTI* mv, uint16 type)
	{
		for(uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceType(mv->voice[i].GetPtr(), type);
	}

	void AXSetMultiVoiceAdpcm(AXVPBMULTI* mv, AXDSPADPCM* adpcm)
	{
		static_assert(sizeof(AXDSPADPCM) == 0x60);
		for (uint32 i = 0; i < mv->channelCount; ++i)
		{
			AXPBADPCM_t tmp;
			tmp.gain = adpcm[i].gain.bevalue();
			tmp.yn1 = adpcm[i].yn1.bevalue();
			tmp.yn2 = adpcm[i].yn2.bevalue();
			tmp.scale = adpcm[i].scale.bevalue();

			static_assert(sizeof(tmp.a) == sizeof(adpcm->coef));
			memcpy(tmp.a, adpcm[i].coef, sizeof(tmp.a));

			AXSetVoiceAdpcm(mv->voice[i].GetPtr(), &tmp);
		}
	}

	void AXSetMultiVoiceSrcType(AXVPBMULTI* mv, uint32 type)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceSrcType(mv->voice[i].GetPtr(), type);
	}

	void AXSetMultiVoiceOffsets(AXVPBMULTI* mv, AXPBOFFSET_t* offsets)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceOffsets(mv->voice[i].GetPtr(), offsets + i);
	}

	void AXSetMultiVoiceVe(AXVPBMULTI* mv, AXPBVE* ve)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceVe(mv->voice[i].GetPtr(), ve);
	}
	
	void AXSetMultiVoiceSrcRatio(AXVPBMULTI* mv, float ratio)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceSrcRatio(mv->voice[i].GetPtr(), ratio);
	}
	
	void AXSetMultiVoiceSrc(AXVPBMULTI* mv, AXPBSRC_t* src)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceSrc(mv->voice[i].GetPtr(), src);
	}
	
	void AXSetMultiVoiceLoop(AXVPBMULTI* mv, uint16 loop)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceLoop(mv->voice[i].GetPtr(), loop);
	}
	
	void AXSetMultiVoiceState(AXVPBMULTI* mv, uint16 state)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceState(mv->voice[i].GetPtr(), state);
	}
	
	void AXSetMultiVoiceAdpcmLoop(AXVPBMULTI* mv, AXPBADPCMLOOP_t* loops)
	{
		for (uint32 i = 0; i < mv->channelCount; ++i)
			AXSetVoiceAdpcmLoop(mv->voice[i].GetPtr(), loops + i);
	}

	sint32 AXIsMultiVoiceRunning(AXVPBMULTI* mv)
	{
		const sint32 result = AXIsVoiceRunning(mv->voice[0].GetPtr());
		return result;
	}

}
