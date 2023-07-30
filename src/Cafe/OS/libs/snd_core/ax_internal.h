#pragma once
#include "Cafe/OS/libs/snd_core/ax.h"

namespace snd_core
{
	typedef struct
	{
		bool isInitialized;
		bool isSoundCore2;
		// init params
		struct
		{
			uint32 rendererFreq; // 32Khz or 48Khz
			uint32 frameLength; // 3MS
			uint32 pipelineMode;
		}initParam;
	}sndGeneric_t;

	extern sndGeneric_t sndGeneric;

	const uint32 AX_SYNCFLAG_SRCFILTER = 0x1;				// Voice src type (AXSetVoiceSrcType)
	const uint32 AX_SYNCFLAG_DEVICEMIXMASK = 0x2;			// Voice mix related (AXSetVoiceDeviceMix)
	const uint32 AX_SYNCFLAG_PLAYBACKSTATE = 0x4;			// Voice play state (AXSetVoiceState)
	const uint32 AX_SYNCFLAG_VOICETYPE = 0x8;				// Voice type (AXSetVoiceType)
	const uint32 AX_SYNCFLAG_DEVICEMIX = 0x10;				// Voice mix related (AXSetVoiceDeviceMix)
	const uint32 AX_SYNCFLAG_ITD20 = 0x20;					// Voice initial time delay (AXSetVoiceItdOn)
	const uint32 AX_SYNCFLAG_ITD40 = 0x40;					// Voice initial time delay (AXSetVoiceItdOn, AXSetVoiceItdTarget)
	const uint32 AX_SYNCFLAG_VE = 0x100;					// Voice ve (AXSetVoiceVe)
	const uint32 AX_SYNCFLAG_VEDELTA = 0x200;				// Voice ve delta (AXSetVoiceVeDelta)
	const uint32 AX_SYNCFLAG_OFFSETS = 0x400;				// Voice offset data (AXSetVoiceOffsets)
	const uint32 AX_SYNCFLAG_LOOPFLAG = 0x800;				// Voice loop flag (AXSetVoiceLoop)
	const uint32 AX_SYNCFLAG_LOOPOFFSET = 0x1000;			// Voice loop offset (AXSetVoiceLoopOffset)
	const uint32 AX_SYNCFLAG_ENDOFFSET = 0x2000;			// Voice end offset (AXSetVoiceEndOffset)
	const uint32 AX_SYNCFLAG_CURRENTOFFSET = 0x4000;		// Voice current offset (AXSetVoiceCurrentOffset)
	const uint32 AX_SYNCFLAG_ADPCMDATA = 0x8000;			// Voice adpcm data (AXSetVoiceAdpcm)
	const uint32 AX_SYNCFLAG_SRCDATA = 0x10000;				// Voice src + src ratio (AXSetVoiceSrc)
	const uint32 AX_SYNCFLAG_SRCRATIO = 0x20000;			// Voice src ratio (AXSetVoiceSrcRatio)
	const uint32 AX_SYNCFLAG_ADPCMLOOP = 0x40000;			// Voice adpcm loop (AXSetVoiceAdpcmLoop)
	const uint32 AX_SYNCFLAG_LPFDATA = 0x80000;				// Voice lpf (AXSetVoiceLpf)
	const uint32 AX_SYNCFLAG_LPFCOEF = 0x100000;			// Voice lpf coef (AXSetVoiceLpfCoefs)
	const uint32 AX_SYNCFLAG_BIQUADDATA = 0x200000;			// Voice biquad (AXSetVoiceBiquad)
	const uint32 AX_SYNCFLAG_BIQUADCOEF = 0x400000;			// Voice biquad coef (AXSetVoiceBiquadCoefs)
	const uint32 AX_SYNCFLAG_VOICEREMOTEON = 0x800000;		// ??? (AXSetVoiceRmtOn?)
	const uint32 AX_SYNCFLAG_4000000 = 0x4000000;			// ???
	const uint32 AX_SYNCFLAG_8000000 = 0x8000000;			// ???

	struct axADPCMInternal_t
	{
		/* +0x00 | +0x190 */ uint16 coef[16];
		/* +0x20 | +0x1B0 */ uint16 gain;
		/* +0x22 | +0x1B2 */ uint16 scale;
		/* +0x24 | +0x1B4 */ uint16 yn1;
		/* +0x26 | +0x1B6 */ uint16 yn2;
		// size: 0x28
	};

	struct axADPCMLoopInternal_t
	{
		/* +0x00 | 0x1C6 */ uint16 loopScale;
		/* +0x02 | 0x1C8 */ uint16 loopYn1;
		/* +0x04 | 0x1CA */ uint16 loopYn2;
		// size: 0x6
	};

	struct axOffsetsInternal_t
	{
		/* +0x00 / 0x17E */ uint16 loopFlag;
		/* +0x02 / 0x180 */ uint16 format;
		/* +0x04 / 0x182 */ uint16 ptrHighExtension; // defines 512mb range (highest 3 bit of current offset ptr counted in bytes)
		// offsets (relative to NULL, counted in sample words)
		// note: All offset ptr variables can only store values up to 512MB (PCM8 mask -> 0x1FFFFFFF, PCM16 mask -> 0x0FFFFFFF, ADPCM mask -> 0x3FFFFFFF)
		/* +0x06 / 0x184 */ uint16 loopOffsetPtrHigh;
		/* +0x08 / 0x186 */ uint16 loopOffsetPtrLow;
		/* +0x0A / 0x188 */ uint16 endOffsetPtrHigh;
		/* +0x0C / 0x18A */ uint16 endOffsetPtrLow;
		/* +0x0E / 0x18C */ uint16 currentOffsetPtrHigh;
		/* +0x10 / 0x18E */ uint16 currentOffsetPtrLow;

		uint32 GetLoopOffset32() const
		{
			uint32 offset = (uint32)_swapEndianU16(loopOffsetPtrHigh) << 16;
			return offset | (uint32)_swapEndianU16(loopOffsetPtrLow);
		}

		uint32 GetEndOffset32() const
		{
			uint32 offset = (uint32)_swapEndianU16(endOffsetPtrHigh) << 16;
			return offset | (uint32)_swapEndianU16(endOffsetPtrLow);
		}

		uint32 GetCurrentOffset32() const
		{
			uint32 offset = (uint32)_swapEndianU16(currentOffsetPtrHigh) << 16;
			return offset | (uint32)_swapEndianU16(currentOffsetPtrLow);
		}
	};

	const int AX_BUS_COUNT = 4;

	struct AXVPBInternal_t
	{
		/* matches what the DSP expects */
		/* +0x000 */ uint16be nextAddrHigh; // points to next shadow copy (physical pointer, NULL for last voice in list)
		/* +0x002 */ uint16be nextAddrLow;
		/* +0x004 */ uint16be selfAddrHigh; // points to shadow copy of self (physical pointer)
		/* +0x006 */ uint16be selfAddrLow;
		/* +0x008 */ uint16 srcFilterMode; // AX_FILTER_MODE_*
		/* +0x00A */ uint16 srcTapFilter; // AX_FILTER_TAP_*
		/* +0x00C */ uint16be mixerSelect;
		/* +0x00E */ uint16 voiceType;
		/* +0x010 */ uint16 deviceMixMaskTV[4];
		/* +0x018 */ uint16 deviceMixMaskDRC[4 * 2];
		/* +0x028 */ AXCHMIX_DEPR deviceMixTV[AX_BUS_COUNT * AX_TV_CHANNEL_COUNT]; // TV device mix
		/* +0x088 */ AXCHMIX_DEPR deviceMixDRC[AX_BUS_COUNT * AX_DRC_CHANNEL_COUNT * 2]; // DRC device mix
		/* +0x108 */ AXCHMIX_DEPR deviceMixRMT[0x40 / 4]; // RMT device mix (size unknown)
		/* +0x148 */ uint16 reserved148_voiceRmtOn;
		/* +0x14A */ uint16 deviceMixMaskRMT[0x10];
		/* +0x16A */ uint16 playbackState;
		// itd (0x16C - 0x1B4?)
		/* +0x16C */ uint16 reserved16C;
		/* +0x16E */ uint16be itdAddrHigh; // points to AXItd_t (physical pointer)
		/* +0x170 */ uint16be itdAddrLow;
		/* +0x172 */ uint16 reserved172;
		/* +0x174 */ uint16 reserved174;
		/* +0x176 */ uint16 reserved176_itdRelated;
		/* +0x178 */ uint16 reserved178_itdRelated;
		/* +0x17A */ uint16be veVolume;
		/* +0x17C */ uint16be veDelta;
		/* +0x17E */ axOffsetsInternal_t internalOffsets;
		/* +0x190 */ axADPCMInternal_t adpcmData;
		/* +0x1B8 */ AXPBSRC_t src;
		/* +0x1C6 */ axADPCMLoopInternal_t adpcmLoop;
		struct
		{
			/* +0x1CC */ uint16 on;
			/* +0x1CE */ uint16 yn1;
			/* +0x1D0 */ uint16 a0;
			/* +0x1D2 */ uint16 b0;
		}lpf;
		struct
		{
			/* +0x1D4 */ uint16 on;
			/* +0x1D6 */ sint16 xn1;
			/* +0x1D8 */ sint16 xn2;
			/* +0x1DA */ sint16 yn1;
			/* +0x1DC */ sint16 yn2;
			/* +0x1DE */ uint16 b0;
			/* +0x1E0 */ uint16 b1;
			/* +0x1E2 */ uint16 b2;
			/* +0x1E4 */ uint16 a1;
			/* +0x1E6 */ uint16 a2;
		}biquad;
		uint16 reserved1E8[0x18];
		uint16 reserved218[0x20]; // not related to device mix?
		uint16 reserved258[0x10]; // not related to device mix?
		// rmt src related
		uint16 reserved278;
		uint16 reserved27A;
		uint16 reserved27C;
		uint16 reserved27E;
		uint16 reserved280;
		uint16 reserved282_rmtIIRGuessed;
		uint32 reserved284;
		uint32 reserved288;
		uint32 reserved28C;
		uint32 reserved290;
		uint16 reserved294;
		uint16 reserved296;
		/* +0x298 */ uint16 reserved298;
		/* +0x29A */ uint16 reserved29A;
		/* +0x29C */ uint16 reserved29C;
		/* +0x29E */ uint16 reserved29E;
		/* +0x2A0 */ uint16be index;
		/* +0x2A2 */ uint16be ukn2A2; // voice active/valid and being processed?
		uint16 reserved2A4;
		uint16 reserved2A6;
		uint16 reserved2A8;
		uint16 reserved2AA;
		/* +0x2AC */ MEMPTR<AXVPBInternal_t> nextToProcess;
		uint32 reserved2B0;
		uint32 reserved2B4;
		uint32 reserved2B8;
		uint32 reserved2BC;
		// size: 0x2C0
	};

	static_assert(sizeof(AXVPBInternal_t) == 0x2C0);

	extern AXVPBInternal_t* __AXVPBInternalVoiceArray;
	extern AXVPBInternal_t* __AXVPBInternalVoiceShadowCopyArrayPtr;
	extern AXVPB* __AXVPBArrayPtr;

	void AXResetVoiceLoopCount(AXVPB* vpb);

	std::vector<AXVPB*>& AXVoiceList_GetListByPriority(uint32 priority);
	std::vector<AXVPB*>& AXVoiceList_GetFreeVoices();

	inline AXVPBInternal_t* GetInternalVoice(const AXVPB* vpb)
	{
		return __AXVPBInternalVoiceArray + (size_t)vpb->index;
	}

	inline uint32 GetVoiceIndex(const AXVPB* vpb)
	{
		return (uint32)vpb->index;
	}

    void AXVBP_Reset();

	// AXIst
	void AXIst_InitThread();
	OSThread_t* AXIst_GetThread();
	void AXIst_StopThread();

	void AXIst_HandleFrameCallbacks();

	// AXAux
	void AXAux_incrementBufferIndex();

	// internal mix buffers
	extern SysAllocator<sint32, AX_SAMPLES_MAX * AX_TV_CHANNEL_COUNT> __AXTVOutputBuffer;
	extern SysAllocator<sint32, AX_SAMPLES_MAX * AX_DRC_CHANNEL_COUNT * 2> __AXDRCOutputBuffer;

}