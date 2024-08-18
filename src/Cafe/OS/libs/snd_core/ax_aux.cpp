#include "Cafe/OS/libs/snd_core/ax.h"
#include "Cafe/OS/libs/snd_core/ax_internal.h"
#include "Cafe/HW/Espresso/PPCState.h"

namespace snd_core
{
	const int AX_AUX_FRAME_COUNT = 2;

	// old (deprecated) style AUX callbacks
	MPTR __AXOldAuxDRCCallbackFunc[AX_AUX_BUS_COUNT * 2];
	MPTR __AXOldAuxDRCCallbackUserParam[AX_AUX_BUS_COUNT * 2];

	MPTR __AXOldAuxTVCallbackFunc[AX_AUX_BUS_COUNT];
	MPTR __AXOldAuxTVCallbackUserParam[AX_AUX_BUS_COUNT];

	// new style AUX callbacks
	MPTR __AXAuxDRCCallbackFunc[AX_AUX_BUS_COUNT * 2]; // 2 DRCs
	MPTR __AXAuxDRCCallbackUserParam[AX_AUX_BUS_COUNT * 2];

	MPTR __AXAuxTVCallbackFunc[AX_AUX_BUS_COUNT];
	MPTR __AXAuxTVCallbackUserParam[AX_AUX_BUS_COUNT];

	struct AUXTVBuffer
	{
		sint32be _buf[AX_AUX_FRAME_COUNT * AX_TV_CHANNEL_COUNT * AX_AUX_BUS_COUNT * AX_SAMPLES_MAX];

		sint32be* GetBuffer(uint32 auxBus, uint32 auxFrame, uint32 channel = 0)
		{
			const size_t samplesPerChannel = AXGetInputSamplesPerFrame();
			const size_t samplesPerBus = AX_SAMPLES_PER_3MS_48KHZ * AX_TV_CHANNEL_COUNT;
			const size_t samplesPerFrame = samplesPerBus * AX_AUX_BUS_COUNT;
			return _buf + auxFrame * samplesPerFrame + auxBus * samplesPerBus + channel * samplesPerChannel;
		}

		void ClearBuffer()
		{
			memset(_buf, 0, sizeof(_buf));
		}
	};

	struct AUXDRCBuffer
	{
		sint32be _buf[AX_AUX_FRAME_COUNT * AX_DRC_CHANNEL_COUNT * AX_AUX_BUS_COUNT * AX_SAMPLES_MAX];

		sint32be* GetBuffer(uint32 auxBus, uint32 auxFrame, uint32 channel = 0)
		{
			const size_t samplesPerChannel = AXGetInputSamplesPerFrame();
			const size_t samplesPerBus = AX_SAMPLES_PER_3MS_48KHZ * AX_DRC_CHANNEL_COUNT;
			const size_t samplesPerFrame = samplesPerBus * AX_AUX_BUS_COUNT;
			return _buf + auxFrame * samplesPerFrame + auxBus * samplesPerBus + channel * samplesPerChannel;
		}

		void ClearBuffer()
		{
			memset(_buf, 0, sizeof(_buf));
		}
	};

	SysAllocator<AUXTVBuffer>			__AXAuxTVBuffer;
	SysAllocator<AUXDRCBuffer, 2>		__AXAuxDRCBuffer;

	uint32 __AXCurrentAuxInputFrameIndex = 0;

	uint32 AXAux_GetOutputFrameIndex()
	{
		return 1 - __AXCurrentAuxInputFrameIndex;
	}

	sint32be* AXAux_GetInputBuffer(sint32 device, sint32 deviceIndex, sint32 auxBus)
	{
		if (auxBus < 0 || auxBus >= AX_AUX_BUS_COUNT)
			return nullptr;
		if (device == AX_DEV_TV)
		{
			cemu_assert_debug(deviceIndex == 0);
			if (__AXOldAuxTVCallbackFunc[auxBus] == MPTR_NULL && __AXAuxTVCallbackFunc[auxBus] == MPTR_NULL)
				return nullptr;
			return __AXAuxTVBuffer->GetBuffer(auxBus, __AXCurrentAuxInputFrameIndex);
		}
		else if (device == AX_DEV_DRC)
		{
			cemu_assert_debug(deviceIndex >= 0 && deviceIndex <= 1);
			if (__AXOldAuxDRCCallbackFunc[deviceIndex * AX_AUX_BUS_COUNT + auxBus] == MPTR_NULL && __AXAuxDRCCallbackFunc[deviceIndex * AX_AUX_BUS_COUNT + auxBus] == MPTR_NULL)
				return nullptr;
			return __AXAuxDRCBuffer[deviceIndex].GetBuffer(auxBus, __AXCurrentAuxInputFrameIndex);
		}
		else
		{
			cemu_assert_debug(false);
		}
		return nullptr;
	}

	sint32be* AXAux_GetOutputBuffer(sint32 device, sint32 deviceIndex, sint32 auxBus)
	{
		uint32 outputFrameIndex = AXAux_GetOutputFrameIndex();

		if (device == AX_DEV_TV)
		{
			cemu_assert_debug(deviceIndex == 0);
			if (__AXOldAuxTVCallbackFunc[auxBus] == MPTR_NULL && __AXAuxTVCallbackFunc[auxBus] == MPTR_NULL)
				return nullptr;
			return __AXAuxTVBuffer->GetBuffer(auxBus, outputFrameIndex);
		}
		else if (device == AX_DEV_DRC)
		{
			cemu_assert_debug(deviceIndex >= 0 && deviceIndex <= 1);
			if (__AXOldAuxDRCCallbackFunc[deviceIndex * AX_AUX_BUS_COUNT + auxBus] == MPTR_NULL && __AXAuxDRCCallbackFunc[deviceIndex * AX_AUX_BUS_COUNT + auxBus] == MPTR_NULL)
				return nullptr;
			return __AXAuxDRCBuffer[deviceIndex].GetBuffer(auxBus, outputFrameIndex);
		}
		else
		{
			cemu_assert_debug(false);
		}
		return nullptr;
	}

	void AXAux_Init()
	{
		__AXCurrentAuxInputFrameIndex = 0;
		__AXAuxTVBuffer->ClearBuffer();
		__AXAuxDRCBuffer[0].ClearBuffer();
		__AXAuxDRCBuffer[1].ClearBuffer();

		memset(__AXAuxTVCallbackFunc, 0, sizeof(__AXAuxTVCallbackFunc));
		memset(__AXAuxTVCallbackUserParam, 0, sizeof(__AXAuxTVCallbackUserParam));
		memset(__AXOldAuxTVCallbackFunc, 0, sizeof(__AXOldAuxTVCallbackFunc));
		memset(__AXOldAuxTVCallbackUserParam, 0, sizeof(__AXOldAuxTVCallbackUserParam));

		memset(__AXAuxDRCCallbackFunc, 0, sizeof(__AXAuxDRCCallbackFunc));
		memset(__AXAuxDRCCallbackUserParam, 0, sizeof(__AXAuxDRCCallbackUserParam));
		memset(__AXOldAuxDRCCallbackFunc, 0, sizeof(__AXOldAuxDRCCallbackFunc));
		memset(__AXOldAuxDRCCallbackUserParam, 0, sizeof(__AXOldAuxDRCCallbackUserParam));
		// init aux return volume
		__AXTVAuxReturnVolume[0] = 0x8000;
		__AXTVAuxReturnVolume[1] = 0x8000;
		__AXTVAuxReturnVolume[2] = 0x8000;
	}

	sint32 AXRegisterAuxCallback(sint32 device, sint32 deviceIndex, uint32 auxBusIndex, MPTR funcMPTR, MPTR userParam)
	{
		sint32 r = AXIsValidDevice(device, deviceIndex);
		if (r != 0)
			return r;
		if (auxBusIndex >= AX_AUX_BUS_COUNT)
			return -5;
		if (device == AX_DEV_TV)
		{
			__AXAuxTVCallbackFunc[auxBusIndex] = funcMPTR;
			__AXAuxTVCallbackUserParam[auxBusIndex] = userParam;
		}
		else if (device == AX_DEV_DRC)
		{
			__AXAuxDRCCallbackFunc[auxBusIndex + deviceIndex * 3] = funcMPTR;
			__AXAuxDRCCallbackUserParam[auxBusIndex + deviceIndex * 3] = userParam;
		}
		else if (device == AX_DEV_RMT)
		{
			cemu_assert_debug(false);
		}
		return 0;
	}

	sint32 AXGetAuxCallback(sint32 device, sint32 deviceIndex, uint32 auxBusIndex, MEMPTR<uint32be> funcPtrOut, MEMPTR<uint32be> contextPtrOut)
	{
		sint32 r = AXIsValidDevice(device, deviceIndex);
		if (r != 0)
			return r;
		if (auxBusIndex >= AX_AUX_BUS_COUNT)
			return -5;
		if (device == AX_DEV_TV)
		{
			*funcPtrOut = __AXAuxTVCallbackFunc[auxBusIndex];
			*contextPtrOut = __AXAuxTVCallbackUserParam[auxBusIndex];
		}
		else if (device == AX_DEV_DRC)
		{
			*funcPtrOut = __AXAuxDRCCallbackFunc[auxBusIndex + deviceIndex * 3];
			*contextPtrOut = __AXAuxDRCCallbackUserParam[auxBusIndex + deviceIndex * 3];
		}
		else if (device == AX_DEV_RMT)
		{
			cemu_assert_debug(false);
			*funcPtrOut = MPTR_NULL;
			*contextPtrOut = MPTR_NULL;
		}
		return 0;
	}

	SysAllocator<MEMPTR<sint32be>, 6> __AXAuxCB_dataPtrs;
	SysAllocator<AXAUXCBCHANNELINFO> __AXAuxCB_auxCBStruct;

	void AXAux_Process()
	{
		uint32 processedAuxFrameIndex = AXAux_GetOutputFrameIndex();
		uint32 sampleCount = AXGetInputSamplesPerFrame();
		// TV aux callbacks
		uint32 tvChannelCount = AX_TV_CHANNEL_COUNT;
		for (sint32 auxBusIndex = 0; auxBusIndex < AX_AUX_BUS_COUNT; auxBusIndex++)
		{
			MPTR auxCBFuncMPTR = MPTR_NULL;
			auxCBFuncMPTR = __AXAuxTVCallbackFunc[auxBusIndex];
			if (auxCBFuncMPTR == MPTR_NULL)
				auxCBFuncMPTR = __AXOldAuxTVCallbackFunc[auxBusIndex];
			if (auxCBFuncMPTR == MPTR_NULL)
			{
				void* auxOutput = __AXAuxTVBuffer->GetBuffer(auxBusIndex, processedAuxFrameIndex);
				memset(auxOutput, 0, sampleCount * AX_TV_CHANNEL_COUNT * sizeof(sint32));
				continue;
			}
			for (sint32 channelIndex = 0; channelIndex < AX_TV_CHANNEL_COUNT; channelIndex++)
				__AXAuxCB_dataPtrs[channelIndex] = __AXAuxTVBuffer->GetBuffer(auxBusIndex, processedAuxFrameIndex, channelIndex);
			// do callback
			if (__AXAuxTVCallbackFunc[auxBusIndex] != MPTR_NULL)
			{
				// new style callback
				AXAUXCBCHANNELINFO* cbStruct = __AXAuxCB_auxCBStruct.GetPtr();
				cbStruct->numChannels = tvChannelCount;
				cbStruct->numSamples = sampleCount;
				PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
				hCPU->gpr[3] = __AXAuxCB_dataPtrs.GetMPTR();
				hCPU->gpr[4] = __AXAuxTVCallbackUserParam[auxBusIndex];
				hCPU->gpr[5] = __AXAuxCB_auxCBStruct.GetMPTR();
				PPCCore_executeCallbackInternal(auxCBFuncMPTR);
			}
			else
			{
				// old style callback
				cemu_assert_debug(false); // todo
			}
		}
		// DRC aux callbacks
		for (sint32 drcIndex = 0; drcIndex < 2; drcIndex++)
		{
			uint32 drcChannelCount = AX_DRC_CHANNEL_COUNT;
			for (sint32 auxBusIndex = 0; auxBusIndex < AX_AUX_BUS_COUNT; auxBusIndex++)
			{
				MPTR auxCBFuncMPTR = MPTR_NULL;
				auxCBFuncMPTR = __AXAuxDRCCallbackFunc[auxBusIndex + drcIndex * 3];
				if (auxCBFuncMPTR == MPTR_NULL)
				{
					auxCBFuncMPTR = __AXOldAuxDRCCallbackFunc[auxBusIndex + drcIndex * 3];
				}
				if (auxCBFuncMPTR == MPTR_NULL)
				{
					void* auxOutput = __AXAuxDRCBuffer[drcIndex].GetBuffer(auxBusIndex, processedAuxFrameIndex);
					memset(auxOutput, 0, 96 * 4 * sizeof(sint32));
					continue;
				}
				if (__AXAuxDRCCallbackFunc[auxBusIndex + drcIndex * 3] != MPTR_NULL)
				{
					// new style callback
					for (sint32 channelIndex = 0; channelIndex < AX_DRC_CHANNEL_COUNT; channelIndex++)
						__AXAuxCB_dataPtrs[channelIndex] = __AXAuxDRCBuffer[drcIndex].GetBuffer(auxBusIndex, processedAuxFrameIndex, channelIndex);
					AXAUXCBCHANNELINFO* cbStruct = __AXAuxCB_auxCBStruct.GetPtr();
					cbStruct->numChannels = drcChannelCount;
					cbStruct->numSamples = sampleCount;
					PPCInterpreter_t* hCPU = PPCInterpreter_getCurrentInstance();
					hCPU->gpr[3] = __AXAuxCB_dataPtrs.GetMPTR();
					hCPU->gpr[4] = __AXAuxDRCCallbackUserParam[auxBusIndex + drcIndex * 3];
					hCPU->gpr[5] = __AXAuxCB_auxCBStruct.GetMPTR();
					PPCCore_executeCallbackInternal(auxCBFuncMPTR);
				}
				else
				{
					// old style callback
					cemu_assert_debug(false);
				}
			}
		}
	}

	void AXAux_incrementBufferIndex()
	{
		__AXCurrentAuxInputFrameIndex = 1 - __AXCurrentAuxInputFrameIndex;
	}

	sint32 AXSetAuxReturnVolume(uint32 device, uint32 deviceIndex, uint32 auxBus, uint16 volume)
	{
		sint32 r = AXIsValidDevice(device, deviceIndex);
		if (r)
			return r;
		if (auxBus >= AX_AUX_BUS_COUNT)
			return -5;
		if( device == AX_DEV_TV )
		{ 
			__AXTVAuxReturnVolume[auxBus] = volume;
		}
		else
		{
			cemuLog_logDebug(LogType::Force, "sndcore2.AXSetAuxReturnVolume() - unsupported device {}", device);
		}
		return 0;
	}

}
