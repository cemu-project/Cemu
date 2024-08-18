#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/snd_core/ax_internal.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/coreinit/coreinit_MessageQueue.h"

namespace snd_core
{
	sndGeneric_t sndGeneric;

	void AXResetToDefaultState()
	{
		memset(&sndGeneric, 0x00, sizeof(sndGeneric));
		resetNumProcessedFrames();
        AXVBP_Reset();
    }

	bool AXIsInit()
	{
		return sndGeneric.isInitialized;
	}

	void __AXInit(bool isSoundCore2, uint32 frameLength, uint32 rendererFreq, uint32 pipelineMode)
	{
		cemu_assert(frameLength == AX_FRAMELENGTH_3MS);
		cemu_assert(rendererFreq == AX_RENDERER_FREQ_32KHZ || rendererFreq == AX_RENDERER_FREQ_48KHZ);
		sndGeneric.isSoundCore2 = isSoundCore2;
		sndGeneric.initParam.frameLength = frameLength;
		sndGeneric.initParam.rendererFreq = rendererFreq;
		sndGeneric.initParam.pipelineMode = pipelineMode;
		// init submodules
		AXIst_Init();
		AXOut_Init();
		AXVPB_Init();
		AXAux_Init();
		AXMix_Init();
		AXMultiVoice_Init();
		AXIst_InitThread();
		sndGeneric.isInitialized = true;
	}

	void sndcore2_AXInitWithParams(AXINITPARAM* initParam)
	{
		if (sndGeneric.isInitialized)
			return;
		__AXInit(true, initParam->frameLength, initParam->freq, initParam->pipelineMode);
	}

	void sndcore2_AXInit()
	{
		if (sndGeneric.isInitialized)
			return;
		__AXInit(true, AX_FRAMELENGTH_3MS, AX_RENDERER_FREQ_32KHZ, AX_PIPELINE_SINGLE);
	}

	void sndcore1_AXInit()
	{
		if (sndGeneric.isInitialized)
			return;
		__AXInit(false, AX_FRAMELENGTH_3MS, AX_RENDERER_FREQ_32KHZ, AX_PIPELINE_SINGLE);
	}

	void sndcore2_AXInitEx(uint32 uknParam)
	{
		cemu_assert_debug(uknParam == 0);
		if (sndGeneric.isInitialized)
			return;
		__AXInit(true, AX_FRAMELENGTH_3MS, AX_RENDERER_FREQ_32KHZ, AX_PIPELINE_SINGLE);
	}

	void sndcore1_AXInitEx(uint32 uknParam)
	{
		cemu_assert_debug(uknParam == 0);
		if (sndGeneric.isInitialized)
			return;
		__AXInit(false, AX_FRAMELENGTH_3MS, AX_RENDERER_FREQ_32KHZ, AX_PIPELINE_SINGLE);
	}

	void AXQuit()
	{
        AXResetCallbacks();
        // todo - should we wait to make sure any active callbacks are finished with execution before we exit AXQuit?
        // request worker thread stop and wait until complete
        AXIst_StopThread();
        // clean up subsystems
        AXVBP_Reset();
		sndGeneric.isInitialized = false;
	}

	sint32 AXGetMaxVoices()
	{
		return sndGeneric.isInitialized ? AX_MAX_VOICES : 0;
	}

	void export_AXGetDeviceFinalMixCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(device, 0);
		ppcDefineParamU32BEPtr(funcAddrPtr, 1);
		sint32 r = AXGetDeviceFinalMixCallback(device, funcAddrPtr);
		cemuLog_log(LogType::SoundAPI, "AXGetDeviceFinalMixCallback({},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4]);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXRegisterDeviceFinalMixCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(device, 0);
		ppcDefineParamMPTR(funcAddr, 1);
		cemuLog_log(LogType::SoundAPI, "AXRegisterDeviceFinalMixCallback({},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4]);
		sint32 r = AXRegisterDeviceFinalMixCallback(device, funcAddr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXRegisterAppFrameCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXRegisterAppFrameCallback(0x{:08x})", hCPU->gpr[3]);
		ppcDefineParamMPTR(funcAddr, 0);
		sint32 r = AXRegisterAppFrameCallback(funcAddr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXDeregisterAppFrameCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXDeregisterAppFrameCallback(0x{:08x})", hCPU->gpr[3]);
		ppcDefineParamMPTR(funcAddr, 0);
		sint32 r = AXDeregisterAppFrameCallback(funcAddr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXRegisterFrameCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXRegisterFrameCallback(0x{:08x})", hCPU->gpr[3]);
		ppcDefineParamMPTR(funcAddr, 0);
		sint32 r = AXRegisterFrameCallback(funcAddr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXRegisterCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXRegisterCallback(0x{:08x})", hCPU->gpr[3]);
		ppcDefineParamMPTR(funcAddr, 0);
		sint32 r = AXRegisterFrameCallback(funcAddr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXRegisterAuxCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXRegisterAuxCallback(0x{:08x},0x{:08x},0x{:08x},0x{:08x},0x{:08x}) LR {:08x}", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7], hCPU->spr.LR);
		ppcDefineParamU32(device, 0);
		ppcDefineParamU32(deviceIndex, 1);
		ppcDefineParamU32(auxBusIndex, 2);
		ppcDefineParamMPTR(funcAddr, 3);
		ppcDefineParamMPTR(userParam, 4);
		sint32 r = AXRegisterAuxCallback(device, deviceIndex, auxBusIndex, funcAddr, userParam);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXGetAuxCallback(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXGetAuxCallback(0x{:08x},0x{:08x},0x{:08x},0x{:08x},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);
		ppcDefineParamU32(device, 0);
		ppcDefineParamU32(deviceIndex, 1);
		ppcDefineParamU32(auxBusIndex, 2);
		ppcDefineParamMEMPTR(funcAddrOut, uint32be, 3);
		ppcDefineParamMEMPTR(userParamOut, uint32be, 4);
		sint32 r = AXGetAuxCallback(device, deviceIndex, auxBusIndex, funcAddrOut, userParamOut);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXSetAuxReturnVolume(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXSetAuxReturnVolume(0x{:08x},0x{:08x},0x{:08x},0x{:04x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
		ppcDefineParamU32(device, 0);
		ppcDefineParamU32(deviceIndex, 1);
		ppcDefineParamU32(auxBusIndex, 2);
		ppcDefineParamU16(volume, 3);
		sint32 r = AXSetAuxReturnVolume(device, deviceIndex, auxBusIndex, volume);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXGetDeviceMode(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXGetDeviceMode({})", hCPU->gpr[3]);
		ppcDefineParamS32(device, 0);
		ppcDefineParamU32BEPtr(mode, 1);
		*mode = AXGetDeviceMode(device);
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_AXSetDeviceUpsampleStage(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXSetDeviceUpsampleStage({},{})", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamS32(device, 0);
		ppcDefineParamS32(upsampleStage, 1);
		sint32 r = AXSetDeviceUpsampleStage(device, upsampleStage);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXGetDeviceUpsampleStage(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXGetDeviceUpsampleStage({},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4]);
		ppcDefineParamS32(device, 0);
		ppcDefineParamU32BEPtr(upsampleStagePtr, 1);
		sint32 r = AXGetDeviceUpsampleStage(device, upsampleStagePtr);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXAcquireVoiceEx(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(priority, 0);
		ppcDefineParamMPTR(callbackEx, 1);
		ppcDefineParamMPTR(userParam, 2);
		cemuLog_log(LogType::SoundAPI, "AXAcquireVoiceEx({},0x{:08x},0x{:08x})", priority, callbackEx, userParam);
		MEMPTR<AXVPB> r = AXAcquireVoiceEx(priority, callbackEx, userParam);
		osLib_returnFromFunction(hCPU, r.GetMPTR());
	}

	void export_AXAcquireVoice(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(priority, 0);
		ppcDefineParamMPTR(callback, 1);
		ppcDefineParamMPTR(userParam, 2);
		cemuLog_log(LogType::SoundAPI, "AXAcquireVoice({},0x{:08x},0x{:08x})", priority, callback, userParam);
		MEMPTR<AXVPB> r = AXAcquireVoiceEx(priority, MPTR_NULL, MPTR_NULL);
		if (r.IsNull() == false)
		{
			r->callback = (uint32be)callback;
			r->userParam = (uint32be)userParam;
		}
		osLib_returnFromFunction(hCPU, r.GetMPTR());
	}

	void export_AXFreeVoice(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(vpb, AXVPB, 0);
		cemuLog_log(LogType::SoundAPI, "AXFreeVoice(0x{:08x})", hCPU->gpr[3]);
		AXFreeVoice(vpb);
		osLib_returnFromFunction(hCPU, 0);
	}

	void export_AXUserIsProtected(PPCInterpreter_t* hCPU)
	{
		sint32 r = AXUserIsProtected();
		cemuLog_log(LogType::SoundAPI, "AXUserIsProtected() -> {}", r!=0?"true":"false");
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXUserBegin(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXUserBegin()");
		sint32 r = AXUserBegin();
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXUserEnd(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXUserEnd()");
		sint32 r = AXUserEnd();
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXVoiceBegin(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(vpb, AXVPB, 0);
		cemuLog_log(LogType::SoundAPI, "AXVoiceBegin(0x{:08x})", hCPU->gpr[3]);
		sint32 r = AXVoiceBegin(vpb);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXVoiceEnd(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(vpb, AXVPB, 0);
		cemuLog_log(LogType::SoundAPI, "AXVoiceEnd(0x{:08x})", hCPU->gpr[3]);
		sint32 r = AXVoiceEnd(vpb);
		osLib_returnFromFunction(hCPU, r);
	}

	void export_AXVoiceIsProtected(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamStructPtr(vpb, AXVPB, 0);
		cemuLog_log(LogType::SoundAPI, "AXVoiceIsProtected(0x{:08x})", hCPU->gpr[3]);
		sint32 r = AXVoiceProtection_IsProtectedByCurrentThread(vpb)?1:0;
		osLib_returnFromFunction(hCPU, r);
	}

	uint32 __AXCalculatePointerHighExtension(uint16 format, MPTR sampleBase, uint32 offset)
	{
		sampleBase = memory_virtualToPhysical(sampleBase);
		uint32 ptrHighExtension;
		if (format == AX_FORMAT_PCM8)
		{
			ptrHighExtension = ((sampleBase + offset) >> 29);
		}
		else if (format == AX_FORMAT_PCM16)
		{
			ptrHighExtension = ((sampleBase + offset * 2) >> 29);
		}
		else if (format == AX_FORMAT_ADPCM)
		{
			ptrHighExtension = ((sampleBase + offset / 2) >> 29);
		}
		return ptrHighExtension;
	}

	void export_AXCheckVoiceOffsets(PPCInterpreter_t* hCPU)
	{
		cemuLog_log(LogType::SoundAPI, "AXCheckVoiceOffsets(0x{:08x})", hCPU->gpr[3]);
		ppcDefineParamStructPtr(pbOffset, AXPBOFFSET_t, 0);

		uint16 format = _swapEndianU16(pbOffset->format);
		MPTR sampleBase = _swapEndianU32(pbOffset->samples);

		uint32 highExtLoop = __AXCalculatePointerHighExtension(format, sampleBase, _swapEndianU32(pbOffset->loopOffset));
		uint32 highExtEnd = __AXCalculatePointerHighExtension(format, sampleBase, _swapEndianU32(pbOffset->endOffset));
		uint32 highExtCurrent = __AXCalculatePointerHighExtension(format, sampleBase, _swapEndianU32(pbOffset->currentOffset));

		bool isSameRange;
		if (highExtLoop == highExtEnd && highExtEnd == highExtCurrent)
			isSameRange = true;
		else
			isSameRange = false;

		osLib_returnFromFunction(hCPU, isSameRange ? 1 : 0);
	}
		
	void export_AXSetDeviceRemixMatrix(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(device, 0);
		ppcDefineParamU32(chanIn, 1);
		ppcDefineParamU32(chanOut, 2);
		ppcDefineParamMEMPTR(matrix, float32be, 3);
		cemuLog_log(LogType::SoundAPI, "AXSetDeviceRemixMatrix({},{},{},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
		const auto result = AXSetDeviceRemixMatrix(device, chanIn, chanOut, matrix);
		osLib_returnFromFunction(hCPU, result);
	}
	
	void export_AXGetDeviceRemixMatrix(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamS32(device, 0);
		ppcDefineParamU32(chanIn, 1);
		ppcDefineParamU32(chanOut, 2);
		ppcDefineParamMEMPTR(matrix, MEMPTR<float32be>, 3);
		cemuLog_log(LogType::SoundAPI, "AXGetDeviceRemixMatrix({},{},{},0x{:08x})", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
		const auto result = AXGetDeviceRemixMatrix(device, chanIn, chanOut, matrix);
		osLib_returnFromFunction(hCPU, result);
	}

	struct AXGetDeviceFinalOutput_t 
	{
		/* +0x00 */ uint32be channelCount;
		/* +0x04 */ uint32be uknValue;
		/* +0x08 */ uint32be ukn08;
		/* +0x0C */ uint32be ukn0C;
		/* +0x10 */ uint32be size;
		// struct might be bigger?
	};

	sint32 AXGetDeviceFinalOutput(uint32 device, sint16be* sampleBufferOutput, uint32 bufferSize, AXGetDeviceFinalOutput_t* output)
	{
		if (device != AX_DEV_TV && device != AX_DEV_DRC)
			return -1;
		sint32 channelCount = AIGetChannelCount(device);
		sint32 samplesPerChannel = AIGetSamplesPerChannel(device);
		sint32 samplesToCopy = samplesPerChannel * channelCount;

		if (bufferSize < (samplesToCopy * sizeof(sint16be)))
			return -11; // buffer not large enough

		// copy samples to buffer
		sint16* samplesBuffer = AIGetCurrentDMABuffer(device);
		for (sint32 i = 0; i < samplesToCopy; i++)
			sampleBufferOutput[i] = samplesBuffer[i];

		// set output struct
		output->size = samplesToCopy * sizeof(sint16be);
		output->channelCount = channelCount;
		output->uknValue = 1; // always set to 1/true?

		return 0;
	}

	void loadExportsSndCore1()
	{
		cafeExportRegisterFunc(sndcore1_AXInit, "snd_core", "AXInit", LogType::SoundAPI);
		cafeExportRegisterFunc(sndcore1_AXInitEx, "snd_core", "AXInitEx", LogType::SoundAPI);
		cafeExportRegister("snd_core", AXIsInit, LogType::SoundAPI);
		cafeExportRegister("snd_core", AXQuit, LogType::SoundAPI);

		cafeExportRegister("snd_core", AXGetMaxVoices, LogType::SoundAPI);
		cafeExportRegister("snd_core", AXGetInputSamplesPerFrame, LogType::SoundAPI);
		cafeExportRegister("snd_core", AXGetInputSamplesPerSec, LogType::SoundAPI);
		cafeExportRegister("snd_core", AXSetDefaultMixerSelect, LogType::SoundAPI);
		cafeExportRegister("snd_core", AXGetDefaultMixerSelect, LogType::SoundAPI);

		osLib_addFunction("snd_core", "AXGetDeviceFinalMixCallback", export_AXGetDeviceFinalMixCallback);
		osLib_addFunction("snd_core", "AXRegisterDeviceFinalMixCallback", export_AXRegisterDeviceFinalMixCallback);

		osLib_addFunction("snd_core", "AXRegisterAppFrameCallback", export_AXRegisterAppFrameCallback);
		osLib_addFunction("snd_core", "AXDeregisterAppFrameCallback", export_AXDeregisterAppFrameCallback);

		osLib_addFunction("snd_core", "AXRegisterFrameCallback", export_AXRegisterFrameCallback);
		osLib_addFunction("snd_core", "AXRegisterCallback", export_AXRegisterCallback);

		osLib_addFunction("snd_core", "AXRegisterAuxCallback", export_AXRegisterAuxCallback);
		osLib_addFunction("snd_core", "AXGetAuxCallback", export_AXGetAuxCallback);

		osLib_addFunction("snd_core", "AXSetAuxReturnVolume", export_AXSetAuxReturnVolume);

		osLib_addFunction("snd_core", "AXGetDeviceMode", export_AXGetDeviceMode);

		osLib_addFunction("snd_core", "AXSetDeviceUpsampleStage", export_AXSetDeviceUpsampleStage);
		osLib_addFunction("snd_core", "AXGetDeviceUpsampleStage", export_AXGetDeviceUpsampleStage);

		osLib_addFunction("snd_core", "AXAcquireVoiceEx", export_AXAcquireVoiceEx);
		osLib_addFunction("snd_core", "AXAcquireVoice", export_AXAcquireVoice);
		osLib_addFunction("snd_core", "AXFreeVoice", export_AXFreeVoice);

		osLib_addFunction("snd_core", "AXUserIsProtected", export_AXUserIsProtected);
		osLib_addFunction("snd_core", "AXUserBegin", export_AXUserBegin);
		osLib_addFunction("snd_core", "AXUserEnd", export_AXUserEnd);
		osLib_addFunction("snd_core", "AXVoiceBegin", export_AXVoiceBegin);
		osLib_addFunction("snd_core", "AXVoiceEnd", export_AXVoiceEnd);
		osLib_addFunction("snd_core", "AXVoiceIsProtected", export_AXVoiceIsProtected);

		osLib_addFunction("snd_core", "AXCheckVoiceOffsets", export_AXCheckVoiceOffsets);
		
		osLib_addFunction("snd_core", "AXSetDeviceRemixMatrix", export_AXSetDeviceRemixMatrix);
		osLib_addFunction("snd_core", "AXGetDeviceRemixMatrix", export_AXGetDeviceRemixMatrix);

		cafeExportRegister("snd_core", AXGetDeviceFinalOutput, LogType::SoundAPI);
	}

	void loadExportsSndCore2()
	{
		cafeExportRegisterFunc(sndcore2_AXInitWithParams, "sndcore2", "AXInitWithParams", LogType::SoundAPI);
		cafeExportRegisterFunc(sndcore2_AXInit, "sndcore2", "AXInit", LogType::SoundAPI);
		cafeExportRegisterFunc(sndcore2_AXInitEx, "sndcore2", "AXInitEx", LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXIsInit, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXQuit, LogType::SoundAPI);

		cafeExportRegister("sndcore2", AXGetMaxVoices, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXGetInputSamplesPerFrame, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXGetInputSamplesPerSec, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetDefaultMixerSelect, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXGetDefaultMixerSelect, LogType::SoundAPI);

		osLib_addFunction("sndcore2", "AXGetDeviceFinalMixCallback", export_AXGetDeviceFinalMixCallback);
		osLib_addFunction("sndcore2", "AXRegisterDeviceFinalMixCallback", export_AXRegisterDeviceFinalMixCallback);

		osLib_addFunction("sndcore2", "AXRegisterAppFrameCallback", export_AXRegisterAppFrameCallback);
		osLib_addFunction("sndcore2", "AXDeregisterAppFrameCallback", export_AXDeregisterAppFrameCallback);

		osLib_addFunction("sndcore2", "AXRegisterFrameCallback", export_AXRegisterFrameCallback);
		osLib_addFunction("sndcore2", "AXRegisterCallback", export_AXRegisterCallback);

		osLib_addFunction("sndcore2", "AXRegisterAuxCallback", export_AXRegisterAuxCallback);
		osLib_addFunction("sndcore2", "AXGetAuxCallback", export_AXGetAuxCallback);

		osLib_addFunction("sndcore2", "AXSetAuxReturnVolume", export_AXSetAuxReturnVolume);

		osLib_addFunction("sndcore2", "AXGetDeviceMode", export_AXGetDeviceMode);

		osLib_addFunction("sndcore2", "AXSetDeviceUpsampleStage", export_AXSetDeviceUpsampleStage);
		osLib_addFunction("sndcore2", "AXGetDeviceUpsampleStage", export_AXGetDeviceUpsampleStage);

		osLib_addFunction("sndcore2", "AXAcquireVoiceEx", export_AXAcquireVoiceEx);
		osLib_addFunction("sndcore2", "AXAcquireVoice", export_AXAcquireVoice);
		osLib_addFunction("sndcore2", "AXFreeVoice", export_AXFreeVoice);

		osLib_addFunction("sndcore2", "AXUserIsProtected", export_AXUserIsProtected);
		osLib_addFunction("sndcore2", "AXUserBegin", export_AXUserBegin);
		osLib_addFunction("sndcore2", "AXUserEnd", export_AXUserEnd);
		osLib_addFunction("sndcore2", "AXVoiceBegin", export_AXVoiceBegin);
		osLib_addFunction("sndcore2", "AXVoiceEnd", export_AXVoiceEnd);

		osLib_addFunction("sndcore2", "AXVoiceIsProtected", export_AXVoiceIsProtected);

		osLib_addFunction("sndcore2", "AXCheckVoiceOffsets", export_AXCheckVoiceOffsets);
		
		osLib_addFunction("sndcore2", "AXSetDeviceRemixMatrix", export_AXSetDeviceRemixMatrix);
		osLib_addFunction("sndcore2", "AXGetDeviceRemixMatrix", export_AXGetDeviceRemixMatrix);

		cafeExportRegister("sndcore2", AXGetDeviceFinalOutput, LogType::SoundAPI);

		// multi voice
		cafeExportRegister("sndcore2", AXAcquireMultiVoice, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXFreeMultiVoice, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXGetMultiVoiceReformatBufferSize, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceType, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceAdpcm, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceSrcType, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceOffsets, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceVe, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceSrcRatio, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceSrc, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceLoop, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceState, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXSetMultiVoiceAdpcmLoop, LogType::SoundAPI);
		cafeExportRegister("sndcore2", AXIsMultiVoiceRunning, LogType::SoundAPI);
	}

	void loadExports()
	{
        AXResetToDefaultState();

		loadExportsSndCore1();
		loadExportsSndCore2();
	}

	bool isInitialized()
	{
		return sndGeneric.isInitialized;
	}

	void reset()
	{
        AXOut_reset();
        AXResetToDefaultState();
        sndGeneric.isInitialized = false;
	}

}
