#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/snd_core/ax_internal.h"
#include "Cafe/HW/MMU/MMU.h"
#include "config/ActiveSettings.h"

void mic_updateOnAXFrame();

namespace snd_core
{
	uint32 __AXDefaultMixerSelect = AX_MIXER_SELECT_BOTH;

	uint16 __AXTVAuxReturnVolume[AX_AUX_BUS_COUNT];

	void AXSetDefaultMixerSelect(uint32 mixerSelect)
	{
		__AXDefaultMixerSelect = mixerSelect;
	}

	uint32 AXGetDefaultMixerSelect()
	{
		return __AXDefaultMixerSelect;
	}

	void AXMix_Init()
	{
		AXSetDefaultMixerSelect(AX_MIXER_SELECT_PPC);
	}

	void AXMix_DepopVoice(AXVPBInternal_t* internalShadowCopy)
	{
		// todo
	}

#define handleAdpcmDecodeLoop() \
	if (internalShadowCopy->internalOffsets.loopFlag != 0) \
	{	\
		scale = _swapEndianU16(internalShadowCopy->adpcmLoop.loopScale); \
		delta = 1 << (scale & 0xF); \
		coefIndex = (scale >> 4) & 7; \
		coefA = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 0]); \
		coefB = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 1]); \
		playbackNibbleOffset = vpbLoopOffset; \
	} \
	else \
	{ \
		/* no loop */ \
		internalShadowCopy->playbackState = 0; \
		memset(outputWriter, 0, sampleCount * sizeof(sint16)); \
		break; \
	}

	uint32 _AX_fixAdpcmEndOffset(uint32 adpcmOffset)
	{
		// How to Survive uses an end offset of 0x40000 which is not a valid adpcm sample offset and thus can never be reached (the ppc side decoder jumps from 0x3FFFF to 0x40002)
		// todo - investigate if the DSP decoder routines can handle invalid ADPCM end offsets

		// for now we use this workaround to force a valid address
		if ((adpcmOffset & 0xF) <= 1)
			return adpcmOffset + 2;
		return adpcmOffset;
	}

	void AX_readADPCMSamples(AXVPBInternal_t* internalShadowCopy, sint16* output, sint32 sampleCount)
	{
		if (internalShadowCopy->playbackState == 0)
		{
			memset(output, 0, sampleCount * sizeof(sint16));
			return;
		}

		AXVPB* vpb = __AXVPBArrayPtr + (sint32)internalShadowCopy->index;

		uint8* sampleBase = (uint8*)memory_getPointerFromVirtualOffset(_swapEndianU32(vpb->offsets.samples));

		uint32 vpbLoopOffset = _swapEndianU32(*(uint32*)&vpb->offsets.loopOffset);
		uint32 vpbEndOffset = _swapEndianU32(*(uint32*)&vpb->offsets.endOffset);
		
		vpbEndOffset = _AX_fixAdpcmEndOffset(vpbEndOffset);

		uint32 internalCurrentOffset = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh);
		uint32 internalLoopOffset = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.loopOffsetPtrHigh);

		sint32 scale = (sint32)_swapEndianU16(internalShadowCopy->adpcmData.scale);
		sint32 delta = 1 << (scale & 0xF);
		sint32 coefIndex = (scale >> 4) & 7;

		sint32 coefA = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 0]);
		sint32 coefB = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 1]);

		sint32 hist0 = (sint32)_swapEndianS16(internalShadowCopy->adpcmData.yn1);
		sint32 hist1 = (sint32)_swapEndianS16(internalShadowCopy->adpcmData.yn2);

		uint32 playbackNibbleOffset = vpbLoopOffset + (internalCurrentOffset - internalLoopOffset);
		uint32 playbackNibbleOffsetStart = playbackNibbleOffset;

		sint16* outputWriter = output;
		// decode samples from current partial ADPCM block
		while (sampleCount && (playbackNibbleOffset & 0xF))
		{
			sint8 nibble = (sint8)sampleBase[(sint32)playbackNibbleOffset >> 1];
			nibble <<= (4 * ((playbackNibbleOffset & 1)));
			nibble &= 0xF0;
			sint32 v = (sint32)nibble;
			v <<= (11 - 4);
			v *= delta;
			v += (hist0 * coefA + hist1 * coefB);
			v = (v + 0x400) >> 11;
			v = std::clamp(v, -32768, 32767);

			hist1 = hist0;
			hist0 = v;
			*outputWriter = v;
			outputWriter++;

			// check for loop / end offset
			if (playbackNibbleOffset == vpbEndOffset)
			{
				handleAdpcmDecodeLoop();
			}
			else
			{
				playbackNibbleOffset++;
			}
			sampleCount--;
		}
		// optimized code to decode whole blocks
		if (!(playbackNibbleOffset <= vpbEndOffset && ((uint64)playbackNibbleOffset + (uint64)(sampleCount * 16 / 14)) >= (uint64)vpbEndOffset))
		{
			while (sampleCount >= 14) // 14 samples per 16 byte block
			{
				// decode header
				sint8* sampleInputData = (sint8*)sampleBase + ((sint32)playbackNibbleOffset >> 1);
				scale = (uint8)sampleInputData[0];
				delta = 1 << (scale & 0xF);
				coefIndex = (scale >> 4) & 7;
				coefA = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 0]);
				coefB = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 1]);
				playbackNibbleOffset += 2;
				// decode samples
				sampleInputData++;
				for (sint32 i = 0; i < 7; i++)
				{
					// n0
					sint8 nibble = *sampleInputData;
					sampleInputData++;
					sint32 v;
					// upper nibble
					v = (sint32)(sint8)(nibble & 0xF0);
					v <<= (11 - 4);
					v *= delta;
					v += (hist0 * coefA + hist1 * coefB);
					v = (v + 0x400) >> 11;

					v = std::min(v, 32767);
					v = std::max(v, -32768);

					hist1 = hist0;
					hist0 = v;
					*outputWriter = v;
					outputWriter++;
					// lower nibble
					v = (sint32)(sint8)(nibble << 4);
					v <<= (11 - 4);
					v *= delta;
					v += (hist0 * coefA + hist1 * coefB);
					v = (v + 0x400) >> 11;

					v = std::min(v, 32767);
					v = std::max(v, -32768);

					hist1 = hist0;
					hist0 = v;

					*outputWriter = v;
					outputWriter++;
				}
				playbackNibbleOffset += 7 * 2;
				sampleCount -= 7 * 2;
			}
		}
		// decode remaining samples
		while (sampleCount)
		{
			if ((playbackNibbleOffset & 0xF) == 0)
			{
				scale = sampleBase[(sint32)playbackNibbleOffset >> 1];
				delta = 1 << (scale & 0xF);
				coefIndex = (scale >> 4) & 7;
				coefA = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 0]);
				coefB = (sint32)(sint16)_swapEndianU16(internalShadowCopy->adpcmData.coef[coefIndex * 2 + 1]);
				playbackNibbleOffset += 2;
			}
			sint8 nibble = (sint8)sampleBase[(sint32)playbackNibbleOffset >> 1];
			nibble <<= (4 * ((playbackNibbleOffset & 1)));
			nibble &= 0xF0;
			sint32 v = (sint32)nibble;
			v <<= (11 - 4);
			v *= delta;
			v += (hist0 * coefA + hist1 * coefB);
			v = (v + 0x400) >> 11;

			v = std::min(v, 32767);
			v = std::max(v, -32768);

			hist1 = hist0;
			hist0 = v;
			*outputWriter = v;
			outputWriter++;

			// check for loop / end offset
			if (playbackNibbleOffset == vpbEndOffset)
			{
				handleAdpcmDecodeLoop();
			}
			else
			{
				playbackNibbleOffset++;
			}
			sampleCount--;
		}

		// write updated values
		internalShadowCopy->adpcmData.scale = _swapEndianU16(scale);
		internalShadowCopy->adpcmData.yn1 = (uint16)_swapEndianS16(hist0);
		internalShadowCopy->adpcmData.yn2 = (uint16)_swapEndianS16(hist1);

		uint32 newInternalCurrentOffset = internalCurrentOffset + (playbackNibbleOffset - playbackNibbleOffsetStart);
		*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh = _swapEndianU32(newInternalCurrentOffset);

	}

	void AX_DecodeSamplesADPCM_NoSrc(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		sint16 sampleBuffer[1024];
		cemu_assert(sampleCount <= (sizeof(sampleBuffer) / sizeof(sampleBuffer[0])));
		AX_readADPCMSamples(internalShadowCopy, sampleBuffer, sampleCount);
		for (sint32 i = 0; i < sampleCount; i++)
		{
			// decode next sample
			sint32 s = sampleBuffer[i];
			s <<= 8;
			*output = (float)s;
			output++;
		}
	}

	void AX_DecodeSamplesADPCM_Linear(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		uint32 currentFracPos = (uint32)_swapEndianU16(internalShadowCopy->src.currentFrac);
		uint32 ratio = _swapEndianU32(*(uint32*)&internalShadowCopy->src.ratioHigh);

		sint16 historySamples[4];
		historySamples[0] = _swapEndianS16(internalShadowCopy->src.historySamples[0]);
		historySamples[1] = _swapEndianS16(internalShadowCopy->src.historySamples[1]);
		historySamples[2] = _swapEndianS16(internalShadowCopy->src.historySamples[2]);
		historySamples[3] = _swapEndianS16(internalShadowCopy->src.historySamples[3]);

		sint32 historyIndex = 0;

		sint32 numberOfDecodedAdpcmSamples = (sint32)((currentFracPos + ratio * sampleCount) >> 16);
		sint16 adpcmSampleBuffer[4096];
		if (numberOfDecodedAdpcmSamples >= 4096)
		{
			memset(output, 0, sizeof(float)*sampleCount);
			cemuLog_log(LogType::Force, "Too many ADPCM samples to decode. ratio = {:08x}", ratio);
			return;
		}
		AX_readADPCMSamples(internalShadowCopy, adpcmSampleBuffer, numberOfDecodedAdpcmSamples);

		sint32 readSampleCount = 0;

		if (ratio == 0x10000 && currentFracPos == 0)
		{
			// optimized path when accessing only fully aligned samples
			for (sint32 i = 0; i < sampleCount; i++)
			{
				cemu_assert_debug(readSampleCount < numberOfDecodedAdpcmSamples);
				// get next sample
				sint16 s = adpcmSampleBuffer[readSampleCount];
				readSampleCount++;
				historyIndex = (historyIndex + 1) & 3;
				historySamples[historyIndex] = s;
				// use previous sample
				sint32 previousSample = historySamples[(historyIndex + 3) & 3];
				sint32 p0 = previousSample << 8;
				*output = (float)p0;
				output++;
			}
		}
		else
		{
			for (sint32 i = 0; i < sampleCount; i++)
			{
				// move playback pos
				currentFracPos += ratio;
				// get more samples if needed
				while (currentFracPos >= 0x10000)
				{
					cemu_assert_debug(readSampleCount < numberOfDecodedAdpcmSamples);
					sint16 s = adpcmSampleBuffer[readSampleCount];
					readSampleCount++;
					currentFracPos -= 0x10000;
					historyIndex = (historyIndex + 1) & 3;
					historySamples[historyIndex] = s;
				}
				// linear interpolation of current sample
				sint32 previousSample = historySamples[(historyIndex + 3) & 3];
				sint32 nextSample = historySamples[historyIndex];

				sint32 p0 = (sint32)previousSample * (sint32)(0x10000 - currentFracPos);
				sint32 p1 = (sint32)nextSample * (sint32)(currentFracPos);

				p0 >>= 7;
				p1 >>= 7;

				sint32 interpolatedSample = p0 + p1;
				interpolatedSample >>= 1;

				*output = (float)interpolatedSample;
				output++;
			}
		}
		cemu_assert_debug(readSampleCount == numberOfDecodedAdpcmSamples);
		// set variables
		internalShadowCopy->src.currentFrac = _swapEndianU16((uint16)(currentFracPos));
		internalShadowCopy->src.historySamples[0] = _swapEndianS16(historySamples[(historyIndex + 0) & 3]);
		internalShadowCopy->src.historySamples[1] = _swapEndianS16(historySamples[(historyIndex + 1) & 3]);
		internalShadowCopy->src.historySamples[2] = _swapEndianS16(historySamples[(historyIndex + 2) & 3]);
		internalShadowCopy->src.historySamples[3] = _swapEndianS16(historySamples[(historyIndex + 3) & 3]);
	}

	void AX_DecodeSamplesADPCM_Tap(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// todo - implement this
		AX_DecodeSamplesADPCM_Linear(internalShadowCopy, output, sampleCount);
	}

	void AX_DecodeSamplesPCM8_Linear(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// get variables
		uint32 currentFracPos = (uint32)_swapEndianU16(internalShadowCopy->src.currentFrac);
		uint32 ratio = _swapEndianU32(*(uint32*)&internalShadowCopy->src.ratioHigh);

		uint32 endOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.endOffsetPtrHigh);
		uint32 currentOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh);
		uint32 loopOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.loopOffsetPtrHigh);
		uint32 ptrHighExtension = _swapEndianU16(internalShadowCopy->internalOffsets.ptrHighExtension);

		uint8* endOffsetAddr = memory_base + (endOffsetPtr | (ptrHighExtension << 29));
		uint8* currentOffsetAddr = memory_base + (currentOffsetPtr | (ptrHighExtension << 29));

		sint16 historySamples[4];
		historySamples[0] = _swapEndianS16(internalShadowCopy->src.historySamples[0]);
		historySamples[1] = _swapEndianS16(internalShadowCopy->src.historySamples[1]);
		historySamples[2] = _swapEndianS16(internalShadowCopy->src.historySamples[2]);
		historySamples[3] = _swapEndianS16(internalShadowCopy->src.historySamples[3]);

		sint32 historyIndex = 0;

		for (sint32 i = 0; i<sampleCount; i++)
		{
			currentFracPos += ratio;
			while (currentFracPos >= 0x10000)
			{
				// read next sample
				historyIndex = (historyIndex + 1) & 3;
				if (internalShadowCopy->playbackState)
				{
					sint32 s = (sint32)(sint8)*currentOffsetAddr;
					s <<= 8;
					historySamples[historyIndex] = s;
					if (currentOffsetAddr == endOffsetAddr)
					{
						if (internalShadowCopy->internalOffsets.loopFlag)
						{
							// loop
							currentOffsetAddr = memory_base + (loopOffsetPtr | (ptrHighExtension << 29));
						}
						else
						{
							// stop playing
							internalShadowCopy->playbackState = 0;
						}
					}
					else
					{
						currentOffsetAddr++;
					}
				}
				else
				{
					// voice not playing, read sample as 0
					historySamples[historyIndex] = 0;
				}
				currentFracPos -= 0x10000;
			}
			// interpolate sample
			sint32 previousSample = historySamples[(historyIndex + 3) & 3];
			sint32 nextSample = historySamples[historyIndex];
			sint32 p0 = (sint32)previousSample * (sint32)(0x10000 - currentFracPos);
			sint32 p1 = (sint32)nextSample * (sint32)(currentFracPos);
			p0 >>= 7;
			p1 >>= 7;
			sint32 interpolatedSample = p0 + p1;
			interpolatedSample >>= 1;
			*output = (float)interpolatedSample;
			output++;
		}
		// set variables
		internalShadowCopy->src.currentFrac = _swapEndianU16((uint16)(currentFracPos));
		internalShadowCopy->src.historySamples[0] = _swapEndianS16(historySamples[(historyIndex + 0) & 3]);
		internalShadowCopy->src.historySamples[1] = _swapEndianS16(historySamples[(historyIndex + 1) & 3]);
		internalShadowCopy->src.historySamples[2] = _swapEndianS16(historySamples[(historyIndex + 2) & 3]);
		internalShadowCopy->src.historySamples[3] = _swapEndianS16(historySamples[(historyIndex + 3) & 3]);
		// store current offset
		currentOffsetPtr = (uint32)((uint8*)currentOffsetAddr - memory_base);
		currentOffsetPtr &= 0x1FFFFFFF; // is this correct?
		*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh = _swapEndianU32(currentOffsetPtr);
	}

	void AX_DecodeSamplesPCM8_Tap(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// todo - implement this
		AX_DecodeSamplesPCM8_Linear(internalShadowCopy, output, sampleCount);
	}

	void AX_DecodeSamplesPCM8_NoSrc(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// get variables
		uint32 currentFracPos = (uint32)_swapEndianU16(internalShadowCopy->src.currentFrac);
		uint32 ratio = _swapEndianU32(*(uint32*)&internalShadowCopy->src.ratioHigh);

		uint32 endOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.endOffsetPtrHigh);
		uint32 currentOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh);
		uint32 ptrHighExtension = _swapEndianU16(internalShadowCopy->internalOffsets.ptrHighExtension);

		uint8* endOffsetAddr = memory_base + (endOffsetPtr | (ptrHighExtension << 29));
		uint8* currentOffsetAddr = memory_base + (currentOffsetPtr | (ptrHighExtension << 29));

		sint16 historySamples[4];
		historySamples[0] = _swapEndianS16(internalShadowCopy->src.historySamples[0]);
		historySamples[1] = _swapEndianS16(internalShadowCopy->src.historySamples[1]);
		historySamples[2] = _swapEndianS16(internalShadowCopy->src.historySamples[2]);
		historySamples[3] = _swapEndianS16(internalShadowCopy->src.historySamples[3]);

		cemu_assert_debug(false); // todo
	}

	void AX_DecodeSamplesPCM16_Linear(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		uint32 currentFracPos = (uint32)_swapEndianU16(internalShadowCopy->src.currentFrac);
		uint32 ratio = _swapEndianU32(*(uint32*)&internalShadowCopy->src.ratioHigh);

		uint32 endOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.endOffsetPtrHigh);
		uint32 currentOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh);
		uint32 loopOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.loopOffsetPtrHigh);
		uint32 ptrHighExtension = _swapEndianU16(internalShadowCopy->internalOffsets.ptrHighExtension);

		uint16* endOffsetAddr = (uint16*)(memory_base + ((endOffsetPtr * 2) | (ptrHighExtension << 29)));
		uint16* currentOffsetAddr = (uint16*)(memory_base + ((currentOffsetPtr * 2) | (ptrHighExtension << 29)));

		uint16* loopOffsetAddrDebug = (uint16*)(memory_base + ((loopOffsetPtr * 2) | (ptrHighExtension << 29)));

		sint16 historySamples[4];
		historySamples[0] = _swapEndianS16(internalShadowCopy->src.historySamples[0]);
		historySamples[1] = _swapEndianS16(internalShadowCopy->src.historySamples[1]);
		historySamples[2] = _swapEndianS16(internalShadowCopy->src.historySamples[2]);
		historySamples[3] = _swapEndianS16(internalShadowCopy->src.historySamples[3]);

		sint32 historyIndex = 0;

		for (sint32 i = 0; i < sampleCount; i++)
		{
			currentFracPos += ratio;
			while (currentFracPos >= 0x10000)
			{
				// read next sample
				historyIndex = (historyIndex + 1) & 3;
				if (internalShadowCopy->playbackState)
				{
					sint32 s = _swapEndianS16(*currentOffsetAddr);
					historySamples[historyIndex] = s;
					if (currentOffsetAddr == endOffsetAddr)
					{
						if (internalShadowCopy->internalOffsets.loopFlag)
						{
							// loop
							currentOffsetAddr = (uint16*)(memory_base + ((loopOffsetPtr * 2) | (ptrHighExtension << 29)));
						}
						else
						{
							// stop playing
							internalShadowCopy->playbackState = 0;
						}
					}
					else
					{
						currentOffsetAddr++; // increment pointer only if not at end offset
					}
				}
				else
				{
					// voice not playing -> sample is silent
					historySamples[historyIndex] = 0;
				}
				currentFracPos -= 0x10000;
			}
			// interpolate sample
			sint32 previousSample = historySamples[(historyIndex + 3) & 3];
			sint32 nextSample = historySamples[historyIndex];

			sint32 p0 = (sint32)previousSample * (sint32)(0x10000 - currentFracPos);
			sint32 p1 = (sint32)nextSample * (sint32)(currentFracPos);
			p0 >>= 7;
			p1 >>= 7;
			sint32 interpolatedSample = p0 + p1;
			interpolatedSample >>= 1;

			*output = (float)interpolatedSample;
			output++;
		}

		// set variables
		internalShadowCopy->src.currentFrac = _swapEndianU16((uint16)(currentFracPos));
		internalShadowCopy->src.historySamples[0] = _swapEndianS16(historySamples[(historyIndex + 0) & 3]);
		internalShadowCopy->src.historySamples[1] = _swapEndianS16(historySamples[(historyIndex + 1) & 3]);
		internalShadowCopy->src.historySamples[2] = _swapEndianS16(historySamples[(historyIndex + 2) & 3]);
		internalShadowCopy->src.historySamples[3] = _swapEndianS16(historySamples[(historyIndex + 3) & 3]);
		// store current offset
		currentOffsetPtr = (uint32)((uint8*)currentOffsetAddr - memory_base);
		currentOffsetPtr &= 0x1FFFFFFF;
		currentOffsetPtr >>= 1;
		*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh = _swapEndianU32(currentOffsetPtr);
	}

	void AX_DecodeSamplesPCM16_Tap(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// todo - implement this
		AX_DecodeSamplesPCM16_Linear(internalShadowCopy, output, sampleCount);
	}

	void AX_DecodeSamplesPCM16_NoSrc(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		// get variables
		uint32 currentFracPos = (uint32)_swapEndianU16(internalShadowCopy->src.currentFrac);
		uint32 ratio = _swapEndianU32(*(uint32*)&internalShadowCopy->src.ratioHigh);

		uint32 endOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.endOffsetPtrHigh);
		uint32 currentOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh);
		uint32 loopOffsetPtr = _swapEndianU32(*(uint32*)&internalShadowCopy->internalOffsets.loopOffsetPtrHigh);
		uint32 ptrHighExtension = _swapEndianU16(internalShadowCopy->internalOffsets.ptrHighExtension);

		uint16* endOffsetAddr = (uint16*)(memory_base + (endOffsetPtr * 2 | (ptrHighExtension << 29)));
		uint16* currentOffsetAddr = (uint16*)(memory_base + (currentOffsetPtr * 2 | (ptrHighExtension << 29)));

		if (internalShadowCopy->playbackState == 0)
		{
			memset(output, 0, sizeof(float)*sampleCount);
			return;
		}

		for (sint32 i = 0; i < sampleCount; i++)
		{
			sint32 s = _swapEndianS16(*currentOffsetAddr);
			s <<= 8;
			output[i] = (float)s;
			if (currentOffsetAddr == endOffsetAddr)
			{
				if (internalShadowCopy->internalOffsets.loopFlag)
				{
					currentOffsetAddr = (uint16*)(memory_base + (loopOffsetPtr * 2 | (ptrHighExtension << 29)));
				}
				else
				{
					internalShadowCopy->playbackState = 0;
					for (; i < sampleCount; i++)
					{
						output[i] = 0.0f;
					}
					break;
				}
			}
			else
				currentOffsetAddr++;
		}
		// store current offset
		currentOffsetPtr = (uint32)((uint8*)currentOffsetAddr - memory_base);
		currentOffsetPtr &= 0x1FFFFFFF;
		currentOffsetPtr >>= 1;

		*(uint32*)&internalShadowCopy->internalOffsets.currentOffsetPtrHigh = _swapEndianU32(currentOffsetPtr);
	}

	void AXVoiceMix_DecodeSamples(AXVPBInternal_t* internalShadowCopy, float* output, sint32 sampleCount)
	{
		uint32 srcFilterMode = _swapEndianU16(internalShadowCopy->srcFilterMode);
		uint16 format = _swapEndianU16(internalShadowCopy->internalOffsets.format);

		if (srcFilterMode == AX_FILTER_MODE_LINEAR || srcFilterMode == AX_FILTER_MODE_TAP)
		{
			if (format == AX_FORMAT_ADPCM)
				AX_DecodeSamplesADPCM_Tap(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM16)
				AX_DecodeSamplesPCM16_Tap(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM8)
				AX_DecodeSamplesPCM8_Tap(internalShadowCopy, output, sampleCount);
			else
				cemu_assert_debug(false);
		}
		else if (srcFilterMode == AX_FILTER_MODE_LINEAR)
		{
			if (format == AX_FORMAT_ADPCM)
				AX_DecodeSamplesADPCM_Linear(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM16)
				AX_DecodeSamplesPCM16_Linear(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM8)
				AX_DecodeSamplesPCM8_Linear(internalShadowCopy, output, sampleCount);
			else
				cemu_assert_debug(false);
		}
		else if (srcFilterMode == AX_FILTER_MODE_NONE)
		{
			if (format == AX_FORMAT_ADPCM)
				AX_DecodeSamplesADPCM_NoSrc(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM16)
				AX_DecodeSamplesPCM16_NoSrc(internalShadowCopy, output, sampleCount);
			else if (format == AX_FORMAT_PCM8)
				AX_DecodeSamplesPCM8_NoSrc(internalShadowCopy, output, sampleCount);
			else
				cemu_assert_debug(false);
		}
	}

	sint32 AXVoiceMix_MergeInto(float* inputSamples, float* outputSamples, sint32 sampleCount, AXCHMIX_DEPR* mix, sint16 deltaI)
	{
		float vol = (float)_swapEndianU16(mix->vol) / (float)0x8000;
		if (deltaI != 0)
		{
			float delta = (float)deltaI / (float)0x8000;
			for (sint32 i = 0; i < sampleCount; i++)
			{
				vol += delta;
				outputSamples[i] += inputSamples[i] * vol;
			}
		}
		else
		{
			// optimized version for delta == 0.0
			for (sint32 i = 0; i < sampleCount; i++)
			{
				outputSamples[i] += inputSamples[i] * vol;
			}
		}
		uint16 volI = (uint16)(vol * 32768.0f);
		mix->vol = _swapEndianU16(volI);
		return volI;
	}

	float __AXMixBufferTV[AX_SAMPLES_MAX * AX_TV_CHANNEL_COUNT * AX_BUS_COUNT];
	float __AXMixBufferDRC[2 * AX_SAMPLES_MAX * AX_DRC_CHANNEL_COUNT * AX_BUS_COUNT];

	void AXVoiceMix_ApplyADSR(AXVPBInternal_t* internalShadowCopy, float* sampleData, sint32 sampleCount)
	{
		uint16 volume = internalShadowCopy->veVolume;
		sint16 volumeDelta = (sint16)internalShadowCopy->veDelta;
		if (volume == 0x8000 && volumeDelta == 0)
			return;
		float volumeScaler = (float)volume / 32768.0f;
		if (volumeDelta == 0)
		{
			// without delta
			for (sint32 i = 0; i < sampleCount; i++)
				sampleData[i] *= volumeScaler;
			return;
		}
		// with delta
		double volumeScalerDelta = (double)volumeDelta / 32768.0;
		volumeScalerDelta = volumeScalerDelta + volumeScalerDelta;
		for (sint32 i = 0; i < sampleCount; i++)
		{
			volumeScaler += (float)volumeScalerDelta;
			sampleData[i] *= volumeScaler;
		}
		if (volumeDelta != 0)
		{
			volume = (uint16)(volumeScaler * 32768.0);
			internalShadowCopy->veVolume = volume;
		}
	}

	void AXVoiceMix_ApplyBiquad(AXVPBInternal_t* internalShadowCopy, float* sampleData, sint32 sampleCount)
	{
		if (internalShadowCopy->biquad.on == AX_BIQUAD_OFF)
			return;
#ifdef CEMU_DEBUG_ASSERT
		if (internalShadowCopy->biquad.on != 0x0200)
		{
			cemuLog_logDebug(LogType::Force, "AX_ApplyBiquad() with incorrect biquad.on value 0x{:04x}", _swapEndianU16(internalShadowCopy->biquad.on));
		}
#endif

		float a1 = (float)(sint16)_swapEndianS16(internalShadowCopy->biquad.a1) / 16384.0f;
		float a2 = (float)(sint16)_swapEndianS16(internalShadowCopy->biquad.a2) / 16384.0f;
		float b0 = (float)(sint16)_swapEndianS16(internalShadowCopy->biquad.b0) / 16384.0f;
		float b1 = (float)(sint16)_swapEndianS16(internalShadowCopy->biquad.b1) / 16384.0f;
		float b2 = (float)(sint16)_swapEndianS16(internalShadowCopy->biquad.b2) / 16384.0f;

		float yn1 = (float)_swapEndianS16(internalShadowCopy->biquad.yn1);
		float yn2 = (float)_swapEndianS16(internalShadowCopy->biquad.yn2);
		float xn1 = (float)_swapEndianS16(internalShadowCopy->biquad.xn1);
		float xn2 = (float)_swapEndianS16(internalShadowCopy->biquad.xn2);
		if (internalShadowCopy->biquad.b1 != 0)
		{
			for (sint32 i = 0; i < sampleCount; i++)
			{
				float inputSample = sampleData[i] / 256.0f;
				float temp = b0 * inputSample + b1 * xn1 + b2 * xn2 + a1 * yn1 + a2 * yn2;
				sampleData[i] = temp * 256.0f;
				temp = std::min(32767.0f, temp);
				temp = std::max(-32768.0f, temp);
				yn2 = yn1;
				xn2 = xn1;
				yn1 = temp;
				xn1 = inputSample;
			}
		}
		else
		{
			// optimized variant where voiceInternal->biquad.b1 is hardcoded as zero (used heavily in BotW and Splatoon)
			for (sint32 i = 0; i < sampleCount; i++)
			{
				float inputSample = sampleData[i] / 256.0f;
				float temp = b0 * inputSample + b2 * xn2 + a1 * yn1 + a2 * yn2;
				sampleData[i] = temp * 256.0f;
				temp = std::min(32767.0f, temp);
				temp = std::max(-32768.0f, temp);
				yn2 = yn1;
				xn2 = xn1;
				yn1 = temp;
				xn1 = inputSample;
			}
		}

		internalShadowCopy->biquad.yn1 = _swapEndianU16((sint16)(yn1));
		internalShadowCopy->biquad.yn2 = _swapEndianU16((sint16)(yn2));
		internalShadowCopy->biquad.xn1 = _swapEndianU16((sint16)(xn1));
		internalShadowCopy->biquad.xn2 = _swapEndianU16((sint16)(xn2));
	}

	void AXVoiceMix_ApplyLowPass(AXVPBInternal_t* internalShadowCopy, float* sampleData, sint32 sampleCount)
	{
		if (internalShadowCopy->lpf.on == _swapEndianU16(AX_LPF_OFF))
			return;
		float a0 = (float)_swapEndianS16(internalShadowCopy->lpf.a0) / 32767.0f;
		float b0 = (float)_swapEndianS16(internalShadowCopy->lpf.b0) / 32767.0f;
		float prevSample = (float)_swapEndianS16((sint16)internalShadowCopy->lpf.yn1) * 256.0f / 32767.0f;
		for (sint32 i = 0; i < sampleCount; i++)
		{
			sampleData[i] = a0 * sampleData[i] - b0 * prevSample;
			prevSample = sampleData[i];
		}
		internalShadowCopy->lpf.yn1 = (uint16)_swapEndianS16((sint16)(prevSample / 256.0f * 32767.0f));
	}

	// mix audio generated from voice into main bus and aux buses
	void AXVoiceMix_MixIntoBuses(AXVPBInternal_t* internalShadowCopy, float* sampleData, sint32 sampleCount, sint32 samplesPerFrame)
	{
		// TV mixing
		for (sint32 busIndex = 0; busIndex < AX_BUS_COUNT; busIndex++)
		{
			for (sint32 channel = 0; channel < 6; channel++)
			{
				uint32 channelMixMask = (_swapEndianU16(internalShadowCopy->deviceMixMaskTV[busIndex]) >> (channel * 2)) & 3;
				if (channelMixMask == 0)
				{
					internalShadowCopy->reserved1E8[busIndex*AX_TV_CHANNEL_COUNT + channel] = 0;
					continue;
				}
				AXCHMIX_DEPR* mix = internalShadowCopy->deviceMixTV + channel * 4 + busIndex;
				float* output = __AXMixBufferTV + (busIndex * 6 + channel) * samplesPerFrame;
				AXVoiceMix_MergeInto(sampleData, output, sampleCount, mix, _swapEndianS16(mix->delta));
				internalShadowCopy->reserved1E8[busIndex*AX_TV_CHANNEL_COUNT + channel] = mix->vol;
			}
		}
		// DRC0 mixing
		for (sint32 busIndex = 0; busIndex < AX_BUS_COUNT; busIndex++)
		{
			for (sint32 channel = 0; channel < AX_DRC_CHANNEL_COUNT; channel++)
			{
				uint32 channelMixMask = (_swapEndianU16(internalShadowCopy->deviceMixMaskDRC[busIndex]) >> (channel * 2)) & 3;

				if (channelMixMask == 0)
				{
					//internalShadowCopy->reserved1E8[busIndex*AX_DRC_CHANNEL_COUNT + channel] = 0;
					continue;
				}
				AXCHMIX_DEPR* mix = internalShadowCopy->deviceMixDRC + channel * 4 + busIndex;
				float* output = __AXMixBufferDRC + (busIndex * AX_DRC_CHANNEL_COUNT + channel) * samplesPerFrame;
				AXVoiceMix_MergeInto(sampleData, output, sampleCount, mix, _swapEndianS16(mix->delta));
			}
		}

		// DRC1 mixing + RMT mixing
		// todo
	}

	void AXMix_ProcessVoices(AXVPBInternal_t* firstVoice)
	{
		if (firstVoice == nullptr)
			return;
		size_t sampleCount = AXGetInputSamplesPerFrame();
		AXVPBInternal_t* internalVoice = firstVoice;
		cemu_assert_debug(sndGeneric.initParam.frameLength == 0);
		float tmpSampleBuffer[AX_SAMPLES_MAX];
		while (internalVoice)
		{
			AXVoiceMix_DecodeSamples(internalVoice, tmpSampleBuffer, sampleCount);
			AXVoiceMix_ApplyADSR(internalVoice, tmpSampleBuffer, sampleCount);
			AXVoiceMix_ApplyBiquad(internalVoice, tmpSampleBuffer, sampleCount);
			AXVoiceMix_ApplyLowPass(internalVoice, tmpSampleBuffer, sampleCount);
			AXVoiceMix_MixIntoBuses(internalVoice, tmpSampleBuffer, sampleCount, sampleCount);
			// next
			internalVoice = internalVoice->nextToProcess.GetPtr();
		}
	}

	void AXMix_MergeBusSamples(float* input, sint32* output, sint32 sampleCount, uint16& volume, sint16 delta)
	{
		float volumeF = (float)volume / 32768.0f;
		float deltaF = (float)delta / 32768.0f;

		if (delta)
		{
			for (sint32 i = 0; i < sampleCount; i++)
			{
				float s = *input;
				input++;
				s *= volumeF;
				volumeF += deltaF;
				*output = _swapEndianS32(_swapEndianS32(*output) + (sint32)s);
				output++;
			}
			volume = (uint16)(volumeF * 32768.0f);
		}
		else
		{
			// no delta
			for (sint32 i = 0; i < sampleCount; i++)
			{
				float s = *input;
				input++;
				s *= volumeF;
				*output = _swapEndianS32(_swapEndianS32(*output) + (sint32)s);
				output++;
			}
		}
	}

	void AXAuxMix_StoreAuxSamples(float* input, sint32be* output, sint32 sampleCount)
	{
		// Not 100% sure why but we need to temporarily right shift the aux samples by 8 to get the sample range the games expect for the AUX callback
		// without this, Color Splash will apply it's effects incorrectly
		// Its probably because AUX mixing always goes through the DSP which uses 16bit arithmetic?

		// no delta
		for (sint32 i = 0; i < sampleCount; i++)
		{
			float s = *input;
			input++;
			*output = ((sint32)s) >> 8;
			output++;
		}
	}

	void AXAuxMix_MixProcessedAuxSamplesIntoOutput(sint32be* input, float* output, sint32 sampleCount, uint16* volumePtr, sint16 delta)
	{
		uint16 volume = *volumePtr;
		float volumeF = (float)volume / 32768.0f;
		float deltaF = (float)delta / 32768.0f;

		cemu_assert_debug(delta == 0); // todo

		for (sint32 i = 0; i < sampleCount; i++)
		{
			float s = (float)(((sint32)*input)<<8);
			input++;
			s *= volumeF;
			*output += s;
			output++;
		}
		*volumePtr = volume;
	}

	uint16 __AXMasterVolume = 0x8000;
	uint16 __AXDRCMasterVolume = 0x8000;

	// mix into __AXTVOutputBuffer
	void AXMix_mergeTVBuses()
	{
		size_t sampleCount = AXGetInputSamplesPerFrame();

		// debug - Erase main bus and only output AUX
		if (ActiveSettings::AudioOutputOnlyAux())
		{
			memset(__AXMixBufferTV, 0, sizeof(float) * sampleCount * 6);
		}
		// Mix aux into TV main bus
		for (sint32 auxBus = 0; auxBus < AX_AUX_BUS_COUNT; auxBus++)
		{
			sint32be* auxOutput = AXAux_GetOutputBuffer(AX_DEV_TV, 0, auxBus);
			if (auxOutput == nullptr)
				continue;
			// AUX return from output buffer
			uint16 auxReturnVolume = __AXTVAuxReturnVolume[auxBus];
			sint16 auxReturnDelta = 0;
			AXAuxMix_MixProcessedAuxSamplesIntoOutput(auxOutput, __AXMixBufferTV, sampleCount * AX_TV_CHANNEL_COUNT, &auxReturnVolume, auxReturnDelta);
		}
		// mix TV main bus into output
		float* input = __AXMixBufferTV;
		uint16 masterVolume = __AXMasterVolume;
		sint32* output = __AXTVOutputBuffer.GetPtr();
		cemu_assert_debug(masterVolume == 0x8000); // todo -> Calculate delta between old master volume and new volume
		sint16 delta = 0;
		uint16 volVar;
		for (uint16 c = 0; c < AX_TV_CHANNEL_COUNT; c++)
		{
			volVar = _swapEndianU16(masterVolume);
			AXMix_MergeBusSamples(input, output, sampleCount, masterVolume, delta);
			output += sampleCount;
			input += sampleCount;
		}
	}

	// mix into __AXDRCOutputBuffer
	void AXMix_mergeDRC0Buses()
	{
		sint32* output = __AXDRCOutputBuffer.GetPtr();
		uint16 masterVolume = __AXDRCMasterVolume;		
		size_t sampleCount = AXGetInputSamplesPerFrame();

		// todo - drc0 AUX

		// mix DRC0 main bus into output
		float* input = __AXMixBufferDRC;
		cemu_assert_debug(masterVolume == 0x8000); // todo -> Calculate delta between old master volume and new volume
		sint16 delta = 0;
		for (uint16 c = 0; c < AX_DRC_CHANNEL_COUNT; c++)
		{
			AXMix_MergeBusSamples(input, output, sampleCount, masterVolume, delta);
			output += sampleCount;
			input += sampleCount;
		}
	}

	void AXMix_process(AXVPBInternal_t* internalShadowCopyHead)
	{
		memset(__AXMixBufferTV, 0, sizeof(__AXMixBufferTV));
		memset(__AXMixBufferDRC, 0, sizeof(__AXMixBufferDRC));

		AXMix_ProcessVoices(internalShadowCopyHead);
		AXAux_Process(); // apply AUX effects to previous frame
		AXIst_HandleFrameCallbacks();

		size_t sampleCount = AXGetInputSamplesPerFrame();
		// TV aux store
		for (sint32 auxBus = 0; auxBus < AX_AUX_BUS_COUNT; auxBus++)
		{
			sint32be* auxInput = AXAux_GetInputBuffer(AX_DEV_TV, 0, auxBus);
			if (auxInput == nullptr)
				continue;
			float* tvInput = __AXMixBufferTV + (1 + auxBus) * (sampleCount * AX_TV_CHANNEL_COUNT);
			AXAuxMix_StoreAuxSamples(tvInput, auxInput, sampleCount * AX_TV_CHANNEL_COUNT);
		}

		// DRC aux store
		// todo

		// merge main and aux buses
		AXMix_mergeTVBuses();
		AXMix_mergeDRC0Buses();

		AXAux_incrementBufferIndex();
		// update microphone
		mic_updateOnAXFrame();
	}

}
