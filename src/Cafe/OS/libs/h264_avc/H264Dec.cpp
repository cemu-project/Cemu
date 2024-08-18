#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/HW/Espresso/PPCCallback.h"
#include "Cafe/OS/libs/h264_avc/parser/H264Parser.h"
#include "Cafe/OS/libs/h264_avc/H264DecInternal.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "Cafe/CafeSystem.h"

#include "h264dec.h"

enum class H264DEC_STATUS : uint32
{
	SUCCESS = 0x0,
	BAD_STREAM = 0x1000000,
	INVALID_PARAM = 0x1010000,
};

namespace H264
{
	bool H264_IsBotW()
	{
		// Cemuhook has a hack where it always returns a small size for H264DECMemoryRequirement (256 bytes)
		// it also outputs images pre-cropped instead of giving the game raw uncropped images
		// both of these are required to allow Breath of the Wild to playback the higher res (1080p) videos from the Switch version
		// we mirror these hacks for user convenience and because there are no downsides
		uint64 currentTitleId = CafeSystem::GetForegroundTitleId();
		if (currentTitleId == 0x00050000101c9500 || currentTitleId == 0x00050000101c9400 || currentTitleId == 0x00050000101c9300)
			return true;
		return false;
	}

	struct H264Context
	{
		struct
		{
			MEMPTR<void> ptr{ nullptr };
			uint32be length{ 0 };
			float64be timestamp;
		}BitStream;
		struct
		{
			MEMPTR<void> outputFunc{ nullptr };
			uint8be outputPerFrame{ 0 }; // whats the default?
			MEMPTR<void> userMemoryParam{ nullptr };
		}Param;
		// misc
		uint32be sessionHandle;

		// decoder state
		struct
		{
			uint32 numFramesInFlight{0};
		}decoderState;
	};

	uint32 H264DECMemoryRequirement(uint32 codecProfile, uint32 codecLevel, uint32 width, uint32 height, uint32be* sizeRequirementOut)
	{
		if (H264_IsBotW())
		{
			static_assert(sizeof(H264Context) < 256);
			*sizeRequirementOut = 256;
			return 0;
		}

		// note: On console this seems to check if maxWidth or maxHeight < 64 but Pikmin 3 passes 32x32 and crashes if this function fails ?
		if (width < 0x20 || height < 0x20 || width > 2800 || height > 1408 || sizeRequirementOut == MPTR_NULL || codecLevel >= 52 || (codecProfile != 0x42 && codecProfile != 0x4D && codecProfile != 0x64))
			return 0x1010000;

		uint32 workbufferSize = 0;
		if (codecLevel < 0xB)
		{
			workbufferSize = 0x18C << 10;
		}
		else if (codecLevel == 0xB)
		{
			workbufferSize = 0x384 << 10;
		}
		else if (codecLevel >= 0xC && codecLevel <= 0x14)
		{
			workbufferSize = 0x948 << 10;
		}
		else if (codecLevel == 0x15)
		{
			workbufferSize = 0x1290 << 10;
		}
		else if (codecLevel >= 0x16 && codecLevel <= 0x1E)
		{
			workbufferSize = 0x1FA4 << 10;
		}
		else if (codecLevel == 0x1F)
		{
			workbufferSize = 0x4650 << 10;
		}
		else if (codecLevel == 0x20)
		{
			workbufferSize = 0x1400000;
		}
		else if (codecLevel >= 0x21 && codecLevel <= 0x29)
		{
			workbufferSize = 0x8000 << 10;
		}
		else if (codecLevel == 0x2A)
		{
			workbufferSize = 0x2200000;
		}
		else if (codecLevel >= 0x2B && codecLevel <= 0x32)
		{
			workbufferSize = 0x1AF40 << 10;
		}
		else if (codecLevel >= 0x33)
		{
			workbufferSize = 0x2D000 << 10;
		}
		workbufferSize += 0x447;
		*sizeRequirementOut = workbufferSize;
		return 0;
	}

	uint32 H264DECCheckMemSegmentation(MPTR memory, uint32 size)
	{
		// return 0 for valid, 1 for invalid
		// currently we allow any range
		return 0;
	}

	H264DEC_STATUS H264DECFindDecstartpoint(uint8* ptr, uint32 length, uint32be* offsetOut)
	{
		if (!ptr || length < 4 || !offsetOut)
			return H264DEC_STATUS::INVALID_PARAM;
		for (uint32 i = 0; i < length - 4; ++i)
		{
			uint8 b = ptr[i];
			if (b != 0)
				continue;

			b = ptr[i + 1];
			if (b != 0)
				continue;

			b = ptr[i + 2];
			if (b != 1)
				continue;

			b = ptr[i + 3];
			b &= 0x9F;
			if (b != 7) // check for NAL type SPS
				continue;

			if (i > 0)
				*offsetOut = i - 1;
			else
				*offsetOut = 0;

			return H264DEC_STATUS::SUCCESS;
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	H264DEC_STATUS H264DECFindIdrpoint(uint8* ptr, uint32 length, uint32be* offsetOut)
	{
		if (!ptr || length < 4 || !offsetOut)
			return H264DEC_STATUS::INVALID_PARAM;

		for (uint32 i = 0; i < length - 4; ++i)
		{
			uint8 b = ptr[i];
			if (b != 0)
				continue;

			b = ptr[i + 1];
			if (b != 0)
				continue;

			b = ptr[i + 2];
			if (b != 1)
				continue;

			b = ptr[i + 3];
			b &= 0x9F;
			if (b != 5 && b != 7 && b != 8) // check for NAL type IDR slice, but also accept SPS or PPS slices
				continue;

			if (i > 0)
				*offsetOut = i - 1;
			else
				*offsetOut = 0;

			return H264DEC_STATUS::SUCCESS;
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	H264DEC_STATUS H264DECGetImageSize(uint8* stream, uint32 length, uint32 offset, uint32be* outputWidth, uint32be* outputHeight)
	{
		if(!stream || length < 4 || !outputWidth || !outputHeight)
			return H264DEC_STATUS::INVALID_PARAM;
		if( (offset+4) > length )
			return H264DEC_STATUS::INVALID_PARAM;
		uint8* cur = stream + offset;
		uint8* end = stream + length;
		cur += 2; // we access cur[-2] and cur[-1] so we need to start at offset 2
		while(cur < end-2)
		{
			// check for start code
			if(*cur != 1)
			{
				cur++;
				continue;
			}
			// check if this is a valid NAL header
			if(cur[-2] != 0 || cur[-1] != 0 || cur[0] != 1)
			{
				cur++;
				continue;
			}
			uint8 nalHeader = cur[1];
			if((nalHeader & 0x1F) != 7)
			{
				cur++;
				continue;
			}
			h264State_seq_parameter_set_t psp;
			bool r = h264Parser_ParseSPS(cur+2, end-cur-2, psp);
			if(!r)
			{
				cemu_assert_suspicious(); // should not happen
				return H264DEC_STATUS::BAD_STREAM;
			}
			*outputWidth = (psp.pic_width_in_mbs_minus1 + 1) * 16;
			*outputHeight = (psp.pic_height_in_map_units_minus1 + 1) * 16; // affected by frame_mbs_only_flag?
			return H264DEC_STATUS::SUCCESS;
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	uint32 H264DECInitParam(uint32 workMemorySize, void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		*ctx = {};
		return 0;
	}

	std::unordered_map<uint32, H264DecoderBackend*> sDecoderSessions;
	std::mutex sDecoderSessionsMutex;
	std::atomic_uint32_t sCurrentSessionHandle{ 1 };

	H264DecoderBackend* CreateAVCDecoder();

	static H264DecoderBackend* _CreateDecoderSession(uint32& handleOut)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		handleOut = sCurrentSessionHandle.fetch_add(1);
		H264DecoderBackend* session = CreateAVCDecoder();
		sDecoderSessions.try_emplace(handleOut, session);
		return session;
	}

	static H264DecoderBackend* _AcquireDecoderSession(uint32 handle)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		auto it = sDecoderSessions.find(handle);
		if (it == sDecoderSessions.end())
			return nullptr;
		H264DecoderBackend* session = it->second;
		if (sDecoderSessions.size() >= 5)
		{
			cemuLog_log(LogType::Force, "H264: Warning - more than 5 active sessions");
			cemu_assert_suspicious();
		}
		return session;
	}

	static void _ReleaseDecoderSession(H264DecoderBackend* session)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);

	}

	static void _DestroyDecoderSession(uint32 handle)
	{
		std::unique_lock _lock(sDecoderSessionsMutex);
		auto it = sDecoderSessions.find(handle);
		if (it == sDecoderSessions.end())
			return;
		H264DecoderBackend* session = it->second;
		session->Destroy();
		delete session;
		sDecoderSessions.erase(it);
	}

	uint32 H264DECOpen(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		uint32 sessionHandle;
		_CreateDecoderSession(sessionHandle);
		ctx->sessionHandle = sessionHandle;
		return 0;
	}

	uint32 H264DECClose(void* workMemory)
	{
		if (workMemory)
		{
			H264Context* ctx = (H264Context*)workMemory;
			_DestroyDecoderSession(ctx->sessionHandle);
		}
		return 0;
	}

	uint32 H264DECBegin(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		H264DecoderBackend* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECBegin(): Invalid session");
			return 0;
		}
		session->Init(ctx->Param.outputPerFrame == 0);
		ctx->decoderState.numFramesInFlight = 0;
		_ReleaseDecoderSession(session);
		return 0;
	}

	void H264DoFrameOutputCallback(H264Context* ctx, H264DecoderBackend::DecodeResult& decodeResult);

	H264DEC_STATUS H264DECEnd(void* workMemory)
	{
		H264Context* ctx = (H264Context*)workMemory;
		H264DecoderBackend* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECEnd(): Invalid session");
			return H264DEC_STATUS::SUCCESS;
		}
		coreinit::OSEvent* flushEvt = &session->GetFlushEvent();
		coreinit::OSResetEvent(flushEvt);
		session->QueueFlush();
		coreinit::OSWaitEvent(flushEvt);
		while(true)
		{
			H264DecoderBackend::DecodeResult decodeResult;
			if( !session->GetFrameOutputIfReady(decodeResult) )
				break;
			// todo - output all frames in a single callback?
			H264DoFrameOutputCallback(ctx, decodeResult);
			ctx->decoderState.numFramesInFlight--;
		}
		cemu_assert_debug(ctx->decoderState.numFramesInFlight == 0); // no frames should be in flight anymore. Exact behavior is not well understood but we may have to output dummy frames if necessary
		_ReleaseDecoderSession(session);
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_FPTR_OUTPUT(H264Context* ctx, void* outputFunc)
	{
		ctx->Param.outputFunc = outputFunc;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_OUTPUT_PER_FRAME(H264Context* ctx, uint32 outputPerFrame)
	{
		ctx->Param.outputPerFrame = outputPerFrame != 0 ? 1 : 0;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam_USER_MEMORY(H264Context* ctx, MEMPTR<void*>* userMemoryParamPtr)
	{
		ctx->Param.userMemoryParam = *userMemoryParamPtr;
		return H264DEC_STATUS::SUCCESS;
	}

	H264DEC_STATUS H264DECSetParam(H264Context* ctx, uint32 paramId, void* paramValue)
	{
		const uint32 PARAMID_FPTR_OUTPUT = 0x1;
		const uint32 PARAMID_OUTPUT_PER_FRAME = 0x20000002;
		const uint32 PARAMID_USER_MEMORY = 0x70000001;
		const uint32 PARAMID_UKN = 0x20000030;

		if (paramId == PARAMID_FPTR_OUTPUT)
		{
			ctx->Param.outputFunc = paramValue;
		}
		else if (paramId == PARAMID_USER_MEMORY)
		{
			ctx->Param.userMemoryParam = paramValue;
		}
		else if (paramId == PARAMID_OUTPUT_PER_FRAME)
		{
			ctx->Param.outputPerFrame = *(uint8be*)paramValue != 0;
		}
		else if (paramId == PARAMID_UKN)
		{
			// unknown purpose, seen in MK8. paramValue points to a bool
		}
		else
		{
			cemuLog_log(LogType::Force, "h264Export_H264DECSetParam(): Unsupported parameterId 0x{:08x}\n", paramId);
			cemu_assert_unimplemented();
		}
		return H264DEC_STATUS::SUCCESS;
	}

	uint32 H264DECSetBitstream(void* workMemory, void* ptr, uint32 length, double timestamp)
	{
		H264Context* ctx = (H264Context*)workMemory;
		ctx->BitStream.ptr = ptr;
		ctx->BitStream.length = length;
		ctx->BitStream.timestamp = timestamp;
		return 0;
	}

	struct H264DECFrameOutput
	{
		/* +0x00 */ uint32be result;
		/* +0x04 */ uint32be padding04;
		/* +0x08 */ betype<double> timestamp;
		/* +0x10 */ uint32be frameWidth;
		/* +0x14 */ uint32be frameHeight;
		/* +0x18 */ uint32be bytesPerRow;
		/* +0x1C */ uint32be cropEnable;
		/* +0x20 */ uint32be cropTop;
		/* +0x24 */ uint32be cropBottom;
		/* +0x28 */ uint32be cropLeft;
		/* +0x2C */ uint32be cropRight;

		/* +0x30 */ uint32be ukn30;
		/* +0x34 */ uint32be ukn34;
		/* +0x38 */ uint32be ukn38;
		/* +0x3C */ uint32be ukn3C;
		/* +0x40 */ uint32be ukn40;

		/* +0x44 */ MEMPTR<uint8> imagePtr;

		/* +0x48 */ uint32 vuiEnable;
		/* +0x4C */ MPTR   vuiPtr;
		/* +0x50 */ sint32 unused[10];
	};

	struct H264OutputCBStruct
	{
		uint32be frameCount;
		MEMPTR<MEMPTR<H264DECFrameOutput>> resultArray;
		uint32be userParam;
	};

	static_assert(sizeof(H264OutputCBStruct) == 12);

	void H264DoFrameOutputCallback(H264Context* ctx, H264DecoderBackend::DecodeResult& decodeResult)
	{
		sint32 outputFrameCount = 1;

		cemu_assert(outputFrameCount < 8);

		StackAllocator<MEMPTR<void>, 8> stack_resultPtrArray;
		StackAllocator<H264DECFrameOutput, 8> stack_decodedFrameResult;

		for (sint32 i = 0; i < outputFrameCount; i++)
			stack_resultPtrArray[i] = &stack_decodedFrameResult + i;

		H264DECFrameOutput* frameOutput = &stack_decodedFrameResult + 0;
		memset(frameOutput, 0x00, sizeof(H264DECFrameOutput));
		frameOutput->imagePtr = (uint8*)decodeResult.imageOutput;
		frameOutput->result = 100;
		frameOutput->timestamp = decodeResult.timestamp;
		frameOutput->frameWidth = decodeResult.frameWidth;
		frameOutput->frameHeight = decodeResult.frameHeight;
		frameOutput->bytesPerRow = decodeResult.bytesPerRow;
		frameOutput->cropEnable = decodeResult.cropEnable;
		frameOutput->cropTop = decodeResult.cropTop;
		frameOutput->cropBottom = decodeResult.cropBottom;
		frameOutput->cropLeft = decodeResult.cropLeft;
		frameOutput->cropRight = decodeResult.cropRight;

		StackAllocator<H264OutputCBStruct> stack_fptrOutputData;
		stack_fptrOutputData->frameCount = outputFrameCount;
		stack_fptrOutputData->resultArray = (MEMPTR<H264DECFrameOutput>*)stack_resultPtrArray.GetPointer();
		stack_fptrOutputData->userParam = ctx->Param.userMemoryParam.GetBEValue();

		// FPTR callback
		if (!ctx->Param.outputFunc.IsNull())
		{
			cemuLog_log(LogType::H264, "H264: Outputting frame via callback. Timestamp: {} Buffer 0x{:08x} UserParam 0x{:08x}", (double)decodeResult.timestamp, (uint32)frameOutput->imagePtr.GetMPTR(), ctx->Param.userMemoryParam.GetMPTR());
			PPCCoreCallback(ctx->Param.outputFunc.GetMPTR(), stack_fptrOutputData.GetMPTR());
		}
	}

	uint32 H264DECExecute(void* workMemory, void* imageOutput)
	{
		BenchmarkTimer bt;
		bt.Start();
		H264Context* ctx = (H264Context*)workMemory;
		H264DecoderBackend* session = _AcquireDecoderSession(ctx->sessionHandle);
		if (!session)
		{
			cemuLog_log(LogType::Force, "H264DECExecute(): Invalid session");
			return 0;
		}
		// feed data to backend
		session->QueueForDecode((uint8*)ctx->BitStream.ptr.GetPtr(), ctx->BitStream.length, ctx->BitStream.timestamp, imageOutput);
		ctx->decoderState.numFramesInFlight++;
		// H264DECExecute is synchronous and will return a frame after either every call (non-buffered) or after 6 calls (buffered)
		// normally frame decoding happens only during H264DECExecute, but in order to hide the latency of our CPU decoder we will decode asynchronously in buffered mode
		uint32 numFramesToBuffer = (ctx->Param.outputPerFrame == 0) ? 5 : 0;
		if(ctx->decoderState.numFramesInFlight > numFramesToBuffer)
		{
			ctx->decoderState.numFramesInFlight--;
			while(true)
			{
				coreinit::OSEvent& evt = session->GetFrameOutputEvent();
				coreinit::OSWaitEvent(&evt);
				H264DecoderBackend::DecodeResult decodeResult;
				if( !session->GetFrameOutputIfReady(decodeResult) )
					continue;
				H264DoFrameOutputCallback(ctx, decodeResult);
				break;
			}
		}
		_ReleaseDecoderSession(session);
		bt.Stop();
		double callTime = bt.GetElapsedMilliseconds();
		cemuLog_log(LogType::H264, "H264Bench | H264DECExecute took {}ms", callTime);
		return 0x80 | 100;
	}

	H264DEC_STATUS H264DECCheckDecunitLength(void* workMemory, uint8* data, uint32 maxLength, uint32 offset, uint32be* unitLengthOut)
	{
		// todo: our implementation for this currently doesn't parse slice headers and instead assumes that each frame is encoded into a single NAL slice. For all known cases this is sufficient but it doesn't match console behavior for cases where frames are split into multiple NALs
		if (offset >= maxLength || maxLength < 4)
		{
			return H264DEC_STATUS::INVALID_PARAM;
		}

		data += offset;
		maxLength -= offset;

		NALInputBitstream nalStream(data, maxLength);

		if (nalStream.hasError())
		{
			cemu_assert_debug(false);
			return H264DEC_STATUS::BAD_STREAM;
		}

		// search for start code
		sint32 startCodeOffset = 0;
		bool hasStartcode = false;
		while (startCodeOffset < (sint32)(maxLength - 3))
		{
			if (data[startCodeOffset + 0] == 0x00 && data[startCodeOffset + 1] == 0x00 && data[startCodeOffset + 2] == 0x01)
			{
				hasStartcode = true;
				break;
			}
			startCodeOffset++;
		}
		if (hasStartcode == false)
			return H264DEC_STATUS::BAD_STREAM;
		data += startCodeOffset;
		maxLength -= startCodeOffset;

		// parse NAL data
		while (true)
		{
			if (nalStream.isEndOfStream())
				break;
			RBSPInputBitstream rbspStream;
			if (nalStream.getNextRBSP(rbspStream, true) == false)
				break;

			sint32 streamSubOffset = (sint32)(rbspStream.getBasePtr() - data);
			sint32 streamSubLength = rbspStream.getBaseLength();

			// parse NAL header
			uint8 nalHeaderByte = rbspStream.readU8();
			if ((nalHeaderByte & 0x80) != 0)
			{
				// MSB must be zero
				cemu_assert_debug(false);
				continue;
			}
			uint8 nal_ref_idc = (nalHeaderByte >> 5) & 0x3;
			uint8 nal_unit_type = (nalHeaderByte >> 0) & 0x1f;
			if (nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21)
			{
				cemu_assert_debug(false);
				continue;
			}
			switch (nal_unit_type)
			{
			case 1:
			case 5:
			{
				*unitLengthOut = (sint32)((rbspStream.getBasePtr() + rbspStream.getBaseLength()) - data) + startCodeOffset;
				return H264DEC_STATUS::SUCCESS;
			}
			case 6:
				// SEI
				break;
			case 7:
				// SPS
				break;
			case 8:
				// PPS
				break;
			case 9:
				// access unit delimiter
				break;
			case 10:
				// end of sequence
				break;
			default:
				cemuLog_logDebug(LogType::Force, "Unsupported NAL unit type {}", nal_unit_type);
				cemu_assert_unimplemented();
				// todo
				break;
			}
		}
		return H264DEC_STATUS::BAD_STREAM;
	}

	void Initialize()
	{
		cafeExportRegister("h264", H264DECCheckMemSegmentation, LogType::H264);
		cafeExportRegister("h264", H264DECMemoryRequirement, LogType::H264);
		cafeExportRegister("h264", H264DECFindDecstartpoint, LogType::H264);
		cafeExportRegister("h264", H264DECFindIdrpoint, LogType::H264);
		cafeExportRegister("h264", H264DECGetImageSize, LogType::H264);

		cafeExportRegister("h264", H264DECInitParam, LogType::H264);
		cafeExportRegister("h264", H264DECOpen, LogType::H264);
		cafeExportRegister("h264", H264DECClose, LogType::H264);
		cafeExportRegister("h264", H264DECBegin, LogType::H264);
		cafeExportRegister("h264", H264DECEnd, LogType::H264);

		cafeExportRegister("h264", H264DECSetParam_FPTR_OUTPUT, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam_OUTPUT_PER_FRAME, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam_USER_MEMORY, LogType::H264);
		cafeExportRegister("h264", H264DECSetParam, LogType::H264);

		cafeExportRegister("h264", H264DECSetBitstream, LogType::H264);
		cafeExportRegister("h264", H264DECExecute, LogType::H264);

		cafeExportRegister("h264", H264DECCheckDecunitLength, LogType::H264);
	}
}
