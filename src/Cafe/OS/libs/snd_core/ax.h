#pragma once

#include "Cafe/OS/libs/coreinit/coreinit.h" // for OSThread*
#include "util/helpers/fspinlock.h"

struct PPCInterpreter_t;

namespace snd_core
{
	// sndcore2 - AX init param config
	const int AX_RENDERER_FREQ_32KHZ = 0;
	const int AX_RENDERER_FREQ_48KHZ = 1;
	const int AX_FRAMELENGTH_3MS = 0;

	const int AX_SAMPLES_PER_3MS_48KHZ = (144); // 48000*3/1000
	const int AX_SAMPLES_PER_3MS_32KHZ = (96);  // 32000*3/1000
	const int AX_SAMPLES_MAX = AX_SAMPLES_PER_3MS_48KHZ; // the maximum amount of samples in a single frame

	const int AX_DEV_TV = 0;
	const int AX_DEV_DRC = 1;
	const int AX_DEV_RMT = 2;
	const int AX_DEV_COUNT = 3;

	const int AX_UPSAMPLE_STAGE_BEFORE_FINALMIX = 0;
	const int AX_UPSAMPLE_STAGE_AFTER_FINALMIX = 1;

	const int AX_PIPELINE_SINGLE = 0;

	struct AXINITPARAM
	{
		uint32be freq; // AX_RENDERER_FREQ_*
		uint32be frameLength; // AX_FRAMELENGTH_*
		uint32be pipelineMode; // AX_PIPELINE_*
	};

	// maximum number of supported channels per device
	const int AX_TV_CHANNEL_COUNT = 6;
	const int AX_DRC_CHANNEL_COUNT = 4;
	const int AX_RMT_CHANNEL_COUNT = 1;
	
	const int AX_APP_FRAME_CALLBACK_MAX = 64;

	const int AX_MODE_STEREO = 0;
	const int AX_MODE_SURROUND = 1;
	const int AX_MODE_DPL2 = 2;
	const int AX_MODE_6CH = 3;
	const int AX_MODE_MONO = 5;

	const int AX_PRIORITY_MAX = 32;
	const int AX_PRIORITY_FREE = 0;
	const int AX_PRIORITY_NODROP = 31;
	const int AX_PRIORITY_LOWEST = 1;
	const int AX_MAX_VOICES = 96;

	const int AX_AUX_BUS_COUNT = 3;
	const int AX_MAX_NUM_BUS = 4;

	const int AX_FORMAT_ADPCM = 0x0;
	const int AX_FORMAT_PCM16 = 0xA;
	const int AX_FORMAT_PCM8 = 0x19;

	const int AX_LPF_OFF = 0x0;

	const int AX_BIQUAD_OFF = 0x0;

	const int AX_SRC_TYPE_NONE = 0x0;
	const int AX_SRC_TYPE_LINEAR = 0x1;
	const int AX_SRC_TYPE_LOWPASS1 = 0x2;
	const int AX_SRC_TYPE_LOWPASS2 = 0x3;
	const int AX_SRC_TYPE_LOWPASS3 = 0x4;

	const int AX_FILTER_MODE_TAP = 0x0;
	const int AX_FILTER_MODE_LINEAR = 0x1;
	const int AX_FILTER_MODE_NONE = 0x2;

	const int AX_FILTER_LOWPASS_8K = 0x0;
	const int AX_FILTER_LOWPASS_12K = 0x1;
	const int AX_FILTER_LOWPASS_16K = 0x2;

	void loadExports();
	bool isInitialized();
	void reset();

	// AX VPB

	struct AXAUXCBCHANNELINFO
	{
		/* +0x00 */ uint32be numChannels;
		/* +0x04 */ uint32be numSamples;
	};

	struct AXPBOFFSET_t
	{
		/* +0x00 | +0x34 */ uint16		format;
		/* +0x02 | +0x36 */ uint16		loopFlag;
		/* +0x04 | +0x38 */ uint32		loopOffset;
		/* +0x08 | +0x3C */ uint32		endOffset;
		/* +0x0C | +0x40 */ uint32		currentOffset;
		/* +0x10 | +0x44 */ MPTR		samples;
	};

	struct AXPBVE
	{
		uint16be currentVolume;
		sint16be currentDelta; 
	};

	struct AXVPBItd
	{
		uint8 ukn[64];
	};

	struct AXVPB
	{
		/* +0x00 */ uint32be			index;
		/* +0x04 */ uint32be			playbackState;
		/* +0x08 */ uint32be			ukn08;
		/* +0x0C */ uint32be			mixerSelect;
		/* +0x10 */ MEMPTR<AXVPB>		next;
		/* +0x14 */ MEMPTR<AXVPB>		prev;
		/* +0x18 */ uint32be			ukn18;
		/* +0x1C */ uint32be			priority;
		/* +0x20 */ uint32be			callback;
		/* +0x24 */ uint32be			userParam;
		/* +0x28 */ uint32be			sync;
		/* +0x2C */ uint32be			depop;
		/* +0x30 */ MEMPTR<AXVPBItd>	itd;
		/* +0x34 */ AXPBOFFSET_t		offsets;
		/* +0x48 */ uint32be			callbackEx; // AXAcquireVoiceEx
		/* +0x4C */ uint32be			ukn4C_dropReason;
		/* +0x50 */ float32be			dspLoad;
		/* +0x54 */ float32be			ppcLoad;
	};

	struct AXPBLPF_t
	{
		/* +0x00 */ uint16 on;
		/* +0x02 */ sint16 yn1;
		/* +0x04 */ sint16 a0;
		/* +0x06 */ sint16 b0;
	};

	struct AXPBBIQUAD_t
	{
		/* +0x00 */ uint16 on;
		/* +0x02 */ sint16 xn1;
		/* +0x04 */ sint16 xn2;
		/* +0x06 */ sint16 yn1;
		/* +0x08 */ sint16 yn2;
		/* +0x0A */ uint16 b0;
		/* +0x0C */ uint16 b1;
		/* +0x0E */ uint16 b2;
		/* +0x10 */ uint16 a1;
		/* +0x12 */ uint16 a2;
	};

	struct AXRemixMatrix_t
	{
		/* +0x00 */ uint32be channelIn;
		/* +0x04 */ uint32be channelOut;
		/* +0x08 */ MEMPTR<float32be> matrix;
	};
	
	struct AXRemixMatrices_t
	{
		/* +0x00 */ AXRemixMatrix_t deviceEntry[3]; // tv, drc, remote
	};

	struct AXPBADPCM_t
	{
		uint16 a[16];
		uint16 gain;
		uint16 scale;
		uint16 yn1;
		uint16 yn2;
	};

	struct AXPBADPCMLOOP_t
	{
		uint16 loopScale;
		uint16 loopYn1;
		uint16 loopYn2;
	};

	struct AXPBSRC_t
	{
		/* +0x1B8 */ uint16 ratioHigh;
		/* +0x1BA */ uint16 ratioLow;
		/* +0x1BC */ uint16 currentFrac;
		/* +0x1BE */ uint16 historySamples[4];

		uint32 GetSrcRatio32() const
		{
			uint32 offset = (uint32)_swapEndianU16(ratioHigh) << 16;
			return offset | (uint32)_swapEndianU16(ratioLow);
		}
	};

	struct AXCHMIX_DEPR
	{
		uint16 vol;
		sint16 delta;
	};

	struct AXCHMIX2
	{
		uint16be vol;
		sint16be delta;
	};

	void AXVPB_Init();
    void AXResetToDefaultState();

	sint32 AXIsValidDevice(sint32 device, sint32 deviceIndex);

	AXVPB* AXAcquireVoiceEx(uint32 priority, MPTR callbackEx, MPTR userParam);
	void AXFreeVoice(AXVPB* vpb);

	bool AXUserIsProtected();
	sint32 AXUserBegin();
	sint32 AXUserEnd();

	bool AXVoiceProtection_IsProtectedByAnyThread(AXVPB* vpb);
	bool AXVoiceProtection_IsProtectedByCurrentThread(AXVPB* vpb);

	sint32 AXVoiceBegin(AXVPB* voice);
	sint32 AXVoiceEnd(AXVPB* voice);
	sint32 AXSetVoiceDeviceMix(AXVPB* vpb, sint32 device, sint32 deviceIndex, AXCHMIX_DEPR* mix);
	void AXSetVoiceState(AXVPB* vpb, sint32 voiceState);
	sint32 AXIsVoiceRunning(AXVPB* vpb);
	void AXSetVoiceType(AXVPB* vpb, uint16 voiceType);
	void AXSetVoiceAdpcm(AXVPB* vpb, AXPBADPCM_t* adpcm);
	void AXSetVoiceAdpcmLoop(AXVPB* vpb, AXPBADPCMLOOP_t* adpcmLoop);
	void AXSetVoiceSrc(AXVPB* vpb, AXPBSRC_t* src);
	void AXSetVoiceSrcType(AXVPB* vpb, uint32 srcType);
	sint32 AXSetVoiceSrcRatio(AXVPB* vpb, float ratio);
	void AXSetVoiceVe(AXVPB* vpb, AXPBVE* ve);
	void AXComputeLpfCoefs(uint32 freq, uint16be* a0, uint16be* b0);
	void AXSetVoiceLpf(AXVPB* vpb, AXPBLPF_t* lpf);
	void AXSetVoiceLpfCoefs(AXVPB* vpb, uint16 a0, uint16 b0);
	void AXSetVoiceBiquad(AXVPB* vpb, AXPBBIQUAD_t* biquad);
	void AXSetVoiceBiquadCoefs(AXVPB* vpb, uint16 b0, uint16 b1, uint16 b2, uint16 a1, uint16 a2);
	void AXSetVoiceOffsets(AXVPB* vpb, AXPBOFFSET_t* pbOffset);
	void AXGetVoiceOffsets(AXVPB* vpb, AXPBOFFSET_t* pbOffset);
	void AXGetVoiceOffsetsEx(AXVPB* vpb, AXPBOFFSET_t* pbOffset, MPTR sampleBase);
	void AXSetVoiceCurrentOffset(AXVPB* vpb, uint32 currentOffset);
	void AXSetVoiceLoopOffset(AXVPB* vpb, uint32 loopOffset);
	void AXSetVoiceEndOffset(AXVPB* vpb, uint32 endOffset);
	void AXSetVoiceCurrentOffsetEx(AXVPB* vpb, uint32 currentOffset, MPTR sampleBase);
	void AXSetVoiceLoopOffsetEx(AXVPB* vpb, uint32 loopOffset, MPTR sampleBase);
	void AXSetVoiceEndOffsetEx(AXVPB* vpb, uint32 endOffset, MPTR sampleBase);
	uint32 AXGetVoiceCurrentOffsetEx(AXVPB* vpb, MPTR sampleBase);
	void AXSetVoiceLoop(AXVPB* vpb, uint16 loopState);

	// AXIst

	void AXIst_Init();
	void AXIst_ThreadEntry(PPCInterpreter_t* hCPU);
	void AXIst_QueueFrame();

	void AXResetCallbacks();

	bool AXIst_IsFrameBeingProcessed();

	sint32 AXSetDeviceUpsampleStage(sint32 device, int upsampleStage);
	sint32 AXGetDeviceUpsampleStage(sint32 device, uint32be* upsampleStage);

	sint32 AXGetDeviceFinalMixCallback(sint32 device, uint32be* funcAddrPtr);
	sint32 AXRegisterDeviceFinalMixCallback(sint32 device, MPTR funcAddr);

	sint32 AXSetDeviceRemixMatrix(sint32 deviceId, uint32 inputChannelCount, uint32 outputChannelCount, const MEMPTR<float32be>& matrix);
	sint32 AXGetDeviceRemixMatrix(uint32 deviceId, uint32 inputChannelCount, uint32 outputChannelCount, MEMPTR<MEMPTR<float32be>>& matrix);

	sint32 AXRegisterAppFrameCallback(MPTR funcAddr);
	sint32 AXDeregisterAppFrameCallback(MPTR funcAddr);
	MPTR AXRegisterFrameCallback(MPTR funcAddr);

	sint32 AXGetInputSamplesPerFrame();
	sint32 AXGetInputSamplesPerSec();

	// AXOut

	void resetNumProcessedFrames();
	uint32 getNumProcessedFrames();

	void AXOut_Init();

	sint32 AIGetSamplesPerChannel(uint32 device);
	sint32 AIGetChannelCount(uint32 device);
	sint16* AIGetCurrentDMABuffer(uint32 device);

	void AXOut_SubmitTVFrame(sint32 frameIndex);
	void AXOut_SubmitDRCFrame(sint32 frameIndex);

	sint32 AXGetDeviceMode(sint32 device);

	extern uint32 numProcessedFrames;

	// AUX

	void AXAux_Init();

	void AXAux_Process();
	
	sint32be* AXAux_GetInputBuffer(sint32 device, sint32 deviceIndex, sint32 auxBus);
	sint32be* AXAux_GetOutputBuffer(sint32 device, sint32 deviceIndex, sint32 auxBus);

	sint32 AXGetAuxCallback(sint32 device, sint32 deviceIndex, uint32 auxBusIndex, MEMPTR<uint32be> funcPtrOut, MEMPTR<uint32be> contextPtrOut);
	sint32 AXRegisterAuxCallback(sint32 device, sint32 deviceIndex, uint32 auxBusIndex, MPTR funcMPTR, MPTR userParam);

	sint32 AXSetAuxReturnVolume(uint32 device, uint32 deviceIndex, uint32 auxBus, uint16 volume);

	extern uint16 __AXTVAuxReturnVolume[AX_AUX_BUS_COUNT];

	// AXMix
	// mixer select constants (for AXSetDefaultMixerSelect / AXGetDefaultMixerSelect)
	const int AX_MIXER_SELECT_DSP = (0);
	const int AX_MIXER_SELECT_PPC = (1);
	const int AX_MIXER_SELECT_BOTH = (2);

	void AXMix_Init();
	void AXSetDefaultMixerSelect(uint32 mixerSelect);
	uint32 AXGetDefaultMixerSelect();

	void AXMix_DepopVoice(struct AXVPBInternal_t* internalShadowCopy);

	void AXMix_process(struct AXVPBInternal_t* internalShadowCopyHead);

	extern FSpinlock __AXVoiceListSpinlock;

	// AX multi voice
	struct AXVPBMULTI
	{
		/* +0x00 */ betype<uint32> isUsed;
		/* +0x04 */ betype<uint32> channelCount;
		/* +0x08 */ MEMPTR<AXVPB> voice[6];
		// size: 0x20
	};
	static_assert(sizeof(AXVPBMULTI) == 0x20);

	struct AXMULTIVOICEUKNSTRUCT
	{
		uint8 ukn[0x4A];
		betype<sint16> channelCount;
	};

	struct AXDSPADPCM
	{
		/* +0x00 */ uint32be numSamples;
		/* +0x04 */ uint32be ukn04;
		/* +0x08 */ uint32be sampleRate;
		/* +0x0C */ uint16be isLooped;
		/* +0x0E */ uint16be format;
		/* +0x10 */ uint32be ukn10;
		/* +0x14 */ uint32be ukn14;
		/* +0x18 */ uint32be ukn18;
		/* +0x1C */ uint16be coef[16];
		/* +0x3C */ uint16be gain;
		/* +0x3E */ uint16be scale;
		/* +0x40 */ uint16be yn1;
		/* +0x42 */ uint16be yn2;
		/* +0x44 */ uint16be ukn44;
		/* +0x46 */ uint16be ukn46;
		/* +0x48 */ uint16be ukn48;
		/* +0x4A */ uint16be channelCount;
		/* +0x4C */ uint16be ukn4C;
		/* +0x4E */ uint8 _padding4E[0x12];
	};
	static_assert(sizeof(AXDSPADPCM) == 0x60);

	void AXMultiVoice_Init();

	sint32 AXAcquireMultiVoice(sint32 voicePriority, void* cbFunc, void* cbData, AXMULTIVOICEUKNSTRUCT* uknR6, MEMPTR<AXVPBMULTI>* multiVoiceOut);
	void AXFreeMultiVoice(AXVPBMULTI* multiVoice);
	sint32 AXGetMultiVoiceReformatBufferSize(sint32 voiceFormat, uint32 channelCount, uint32 sizeInBytes, uint32be* sizeOutput);
	void AXSetMultiVoiceType(AXVPBMULTI* mv, uint16 type);
	void AXSetMultiVoiceAdpcm(AXVPBMULTI* mv, AXDSPADPCM* data);
	void AXSetMultiVoiceSrcType(AXVPBMULTI* mv, uint32 type);
	void AXSetMultiVoiceOffsets(AXVPBMULTI* mv, AXPBOFFSET_t* offsets);
	void AXSetMultiVoiceVe(AXVPBMULTI* mv, AXPBVE* ve);
	void AXSetMultiVoiceSrcRatio(AXVPBMULTI* mv, float ratio);
	void AXSetMultiVoiceSrc(AXVPBMULTI* mv, AXPBSRC_t* src);
	void AXSetMultiVoiceLoop(AXVPBMULTI* mv, uint16 loop);
	void AXSetMultiVoiceState(AXVPBMULTI* mv, uint16 state);
	void AXSetMultiVoiceAdpcmLoop(AXVPBMULTI* mv, AXPBADPCMLOOP_t* loops);
	sint32 AXIsMultiVoiceRunning(AXVPBMULTI* mv);
	
	void AXOut_init();
	void AXOut_reset();
	void AXOut_update();

	void Initialize();
}