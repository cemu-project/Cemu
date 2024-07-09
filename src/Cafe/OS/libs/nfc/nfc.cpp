#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/nfc/nfc.h"
#include "Cafe/OS/libs/nn_nfp/nn_nfp.h"
#include "Common/FileStream.h"

#include "TagV0.h"
#include "ndef.h"

#define NFC_MODE_INVALID	-1
#define NFC_MODE_IDLE		0
#define NFC_MODE_ACTIVE		1

#define NFC_STATE_UNINITIALIZED		0x0
#define NFC_STATE_INITIALIZED		0x1
#define NFC_STATE_IDLE			0x2
#define NFC_STATE_READ			0x3
#define NFC_STATE_WRITE			0x4
#define NFC_STATE_ABORT			0x5
#define NFC_STATE_FORMAT		0x6
#define NFC_STATE_SET_READ_ONLY		0x7
#define NFC_STATE_TAG_PRESENT		0x8
#define NFC_STATE_DETECT		0x9
#define NFC_STATE_SEND_RAW_DATA		0xA

#define NFC_STATUS_COMMAND_COMPLETE	0x1
#define NFC_STATUS_READY		0x2
#define NFC_STATUS_HAS_TAG		0x4

namespace nfc
{
	struct NFCContext
	{
		bool isInitialized;
		uint32 state;
		sint32 mode;
		bool hasTag;

		uint32 nfcStatus;
		std::chrono::time_point<std::chrono::system_clock> touchTime;
		std::chrono::time_point<std::chrono::system_clock> discoveryTimeout;
		struct {
			NFCUid uid;
			NFCUid mask;
		} filter;

		MPTR tagDetectCallback;
		void* tagDetectContext;
		MPTR abortCallback;
		void* abortContext;
		MPTR rawCallback;
		void* rawContext;
		MPTR readCallback;
		void* readContext;
		MPTR writeCallback;
		void* writeContext;
		MPTR getTagInfoCallback;

		SysAllocator<NFCTagInfo> tagInfo;

		fs::path tagPath;
		std::shared_ptr<TagV0> tag;

		ndef::Message writeMessage;
	};
	NFCContext gNFCContexts[2];

	sint32 NFCInit(uint32 chan)
	{
		return NFCInitEx(chan, 0);
	}

	void __NFCClearContext(NFCContext* context)
	{
		context->isInitialized = false;
		context->state = NFC_STATE_UNINITIALIZED;
		context->mode = NFC_MODE_IDLE;
		context->hasTag = false;

		context->nfcStatus = NFC_STATUS_READY;
		context->discoveryTimeout = {};

		context->tagDetectCallback = MPTR_NULL;
		context->tagDetectContext = nullptr;
		context->abortCallback = MPTR_NULL;
		context->abortContext = nullptr;
		context->rawCallback = MPTR_NULL;
		context->rawContext = nullptr;
		context->readCallback = MPTR_NULL;
		context->readContext = nullptr;
		context->writeCallback = MPTR_NULL;
		context->writeContext = nullptr;

		context->tagPath = "";
		context->tag = {};
	}

	sint32 NFCInitEx(uint32 chan, uint32 powerMode)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		__NFCClearContext(ctx);
		ctx->isInitialized = true;
		ctx->state = NFC_STATE_INITIALIZED;

		return NFC_RESULT_SUCCESS;
	}

	sint32 NFCShutdown(uint32 chan)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		__NFCClearContext(ctx);

		return NFC_RESULT_SUCCESS;
	}

	bool NFCIsInit(uint32 chan)
	{
		cemu_assert(chan < 2);

		return gNFCContexts[chan].isInitialized;
	}

	bool __NFCCompareUid(NFCUid* uid, NFCUid* filterUid, NFCUid* filterMask)
	{
		for (int i = 0; i < sizeof(uid->uid); i++)
		{
			if ((uid->uid[i] & filterMask->uid[i]) != filterUid->uid[i])
			{
				return false;
			}
		}

		return true;
	}

	void __NFCHandleRead(uint32 chan)
	{
		NFCContext* ctx = &gNFCContexts[chan];

		ctx->state = NFC_STATE_IDLE;

		sint32 result;
		StackAllocator<NFCUid> uid;
		bool readOnly = false;
		uint32 dataSize = 0;
		StackAllocator<uint8, 0x200> data;
		uint32 lockedDataSize = 0;
		StackAllocator<uint8, 0x200> lockedData;

		if (ctx->tag)
		{
			// Compare UID
			memcpy(uid.GetPointer(), ctx->tag->GetUIDBlock().data(), sizeof(NFCUid));
			if (__NFCCompareUid(uid.GetPointer(), &ctx->filter.uid, &ctx->filter.mask))
			{
				// Try to parse ndef message
				auto ndefMsg = ndef::Message::FromBytes(ctx->tag->GetNDEFData());
				if (ndefMsg)
				{
					// Look for the unknown TNF which contains the data we care about
					for (const auto& rec : *ndefMsg)
					{
						if (rec.GetTNF() == ndef::Record::NDEF_TNF_UNKNOWN)
						{
							dataSize = rec.GetPayload().size();
							cemu_assert(dataSize < 0x200);
							memcpy(data.GetPointer(), rec.GetPayload().data(), dataSize);
							break;
						}
					}

					if (dataSize)
					{
						// Get locked data
						lockedDataSize = ctx->tag->GetLockedArea().size();
						memcpy(lockedData.GetPointer(), ctx->tag->GetLockedArea().data(), lockedDataSize);

						result = NFC_RESULT_SUCCESS;
					}
					else
					{
						result = NFC_MAKE_RESULT(NFC_RESULT_BASE_TAG_PARSE, NFC_RESULT_INVALID_TAG);
					}
				}
				else
				{
					result = NFC_MAKE_RESULT(NFC_RESULT_BASE_TAG_PARSE, NFC_RESULT_INVALID_TAG);
				}
			}
			else
			{
				result = NFC_MAKE_RESULT(NFC_RESULT_BASE_READ, NFC_RESULT_UID_MISMATCH);
			}
		}
		else
		{
			result = NFC_MAKE_RESULT(NFC_RESULT_BASE_READ, NFC_RESULT_NO_TAG);
		}

		PPCCoreCallback(ctx->readCallback, chan, result, uid.GetPointer(), readOnly, dataSize, data.GetPointer(), lockedDataSize, lockedData.GetPointer(), ctx->readContext);
	}

	void __NFCHandleWrite(uint32 chan)
	{
		NFCContext* ctx = &gNFCContexts[chan];

		ctx->state = NFC_STATE_IDLE;

		sint32 result;

		if (ctx->tag)
		{
			NFCUid uid;
			memcpy(&uid, ctx->tag->GetUIDBlock().data(), sizeof(NFCUid));
			if (__NFCCompareUid(&uid, &ctx->filter.uid, &ctx->filter.mask))
			{
				// Update tag NDEF data
				ctx->tag->SetNDEFData(ctx->writeMessage.ToBytes());

				// open file for writing
				FileStream* fs = FileStream::openFile2(ctx->tagPath, true);
				if (!fs)
				{
					result = NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, 0x22);
				}
				else
				{
					auto tagBytes = ctx->tag->ToBytes();
					fs->writeData(tagBytes.data(), tagBytes.size());
					delete fs;

					result = NFC_RESULT_SUCCESS;
				}
			}
			else
			{
				result = NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, NFC_RESULT_UID_MISMATCH);
			}
		}
		else
		{
			result = NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, NFC_RESULT_NO_TAG);
		}

		PPCCoreCallback(ctx->writeCallback, chan, result, ctx->writeContext);
	}

	void __NFCHandleAbort(uint32 chan)
	{
		NFCContext* ctx = &gNFCContexts[chan];

		ctx->state = NFC_STATE_IDLE;

		PPCCoreCallback(ctx->abortCallback, chan, 0, ctx->abortContext);
	}

	void __NFCHandleRaw(uint32 chan)
	{
		NFCContext* ctx = &gNFCContexts[chan];

		ctx->state = NFC_STATE_IDLE;

		sint32 result;
		if (ctx->nfcStatus & NFC_STATUS_HAS_TAG)
		{
			result = NFC_RESULT_SUCCESS;
		}
		else
		{
			result = NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_NO_TAG);
		}

		// We don't actually send any commands/responses
		uint32 responseSize = 0;
		void* responseData = nullptr;

		PPCCoreCallback(ctx->rawCallback, chan, result, responseSize, responseData, ctx->rawContext);
	}

	bool __NFCShouldHandleState(NFCContext* ctx)
	{
		// Always handle abort
		if (ctx->state == NFC_STATE_ABORT)
		{
			return true;
		}

		// Do we have a tag?
		if (ctx->nfcStatus & NFC_STATUS_HAS_TAG)
		{
			return true;
		}

		// Did the timeout expire?
		if (ctx->discoveryTimeout < std::chrono::system_clock::now())
		{
			return true;
		}

		return false;
	}

	void NFCProc(uint32 chan)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!ctx->isInitialized)
		{
			return;
		}

		if (ctx->state == NFC_STATE_INITIALIZED)
		{
			ctx->state = NFC_STATE_IDLE;
		}

		// Check if the detect callback should be called
		if (ctx->nfcStatus & NFC_STATUS_HAS_TAG)
		{
			if (!ctx->hasTag && ctx->state > NFC_STATE_IDLE && ctx->state != NFC_STATE_ABORT)
			{
				if (ctx->tagDetectCallback)
				{
					PPCCoreCallback(ctx->tagDetectCallback, chan, true, ctx->tagDetectContext);
				}

				ctx->hasTag = true;
			}

			// Check if the tag should be removed again
			if (ctx->touchTime + std::chrono::seconds(2) < std::chrono::system_clock::now())
			{
				ctx->nfcStatus &= ~NFC_STATUS_HAS_TAG;
				ctx->tag = {};
				ctx->tagPath = "";
			}
		}
		else
		{
			if (ctx->hasTag && ctx->state == NFC_STATE_IDLE)
			{
				if (ctx->tagDetectCallback)
				{
					PPCCoreCallback(ctx->tagDetectCallback, chan, false, ctx->tagDetectContext);
				}

				ctx->hasTag = false;
			}
		}

		if (__NFCShouldHandleState(ctx))
		{
			switch (ctx->state)
			{
			case NFC_STATE_READ:
				__NFCHandleRead(chan);
				break;
			case NFC_STATE_WRITE:
				__NFCHandleWrite(chan);
				break;
			case NFC_STATE_ABORT:
				__NFCHandleAbort(chan);
				break;
			case NFC_STATE_SEND_RAW_DATA:
				__NFCHandleRaw(chan);
				break;
			default:
				break;
			}

			// Return back to idle mode
			ctx->mode = NFC_MODE_IDLE;
		}
	}

	sint32 NFCGetMode(uint32 chan)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan) || ctx->state == NFC_STATE_UNINITIALIZED)
		{
			return NFC_MODE_INVALID;
		}

		return ctx->mode;
	}

	sint32 NFCSetMode(uint32 chan, sint32 mode)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SET_MODE, NFC_RESULT_UNINITIALIZED);
		}

		if (ctx->state == NFC_STATE_UNINITIALIZED)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SET_MODE, NFC_RESULT_INVALID_STATE);
		}

		ctx->mode = mode;

		return NFC_RESULT_SUCCESS;
	}

	void NFCSetTagDetectCallback(uint32 chan, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];
		ctx->tagDetectCallback = callback;
		ctx->tagDetectContext = context;
	}

	sint32 NFCAbort(uint32 chan, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_ABORT, NFC_RESULT_UNINITIALIZED);
		}

		if (ctx->state <= NFC_STATE_IDLE)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_ABORT, NFC_RESULT_INVALID_STATE);
		}

		ctx->state = NFC_STATE_ABORT;
		ctx->abortCallback = callback;
		ctx->abortContext = context;

		return NFC_RESULT_SUCCESS;
	}

	sint32 __NFCConvertGetTagInfoResult(sint32 result)
	{
		if (result == NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_NO_TAG))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_GET_TAG_INFO, NFC_RESULT_TAG_INFO_TIMEOUT);
		}

		// TODO convert the rest of the results
		return result;
	}

	void __NFCGetTagInfoCallback(PPCInterpreter_t* hCPU)
	{
		ppcDefineParamU32(chan, 0);
		ppcDefineParamS32(error, 1);
		ppcDefineParamU32(responseSize, 2);
		ppcDefineParamPtr(responseData, void, 3);
		ppcDefineParamPtr(context, void, 4);

		NFCContext* ctx = &gNFCContexts[chan];

		error = __NFCConvertGetTagInfoResult(error);
		if (error == 0 && ctx->tag)
		{
			// this is usually parsed from response data
			ctx->tagInfo->uidSize = sizeof(NFCUid);
			memcpy(ctx->tagInfo->uid, ctx->tag->GetUIDBlock().data(), ctx->tagInfo->uidSize);
			ctx->tagInfo->technology = NFC_TECHNOLOGY_A;
			ctx->tagInfo->protocol = NFC_PROTOCOL_T1T;            
		}

		PPCCoreCallback(ctx->getTagInfoCallback, chan, error, ctx->tagInfo.GetPtr(), context);
		osLib_returnFromFunction(hCPU, 0);
	}

	sint32 NFCGetTagInfo(uint32 chan, uint32 discoveryTimeout, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		// Forward this request to nn_nfp, if the title initialized it
		// TODO integrate nn_nfp/ntag/nfc
		if (nnNfp_isInitialized())
		{
			return nn::nfp::NFCGetTagInfo(chan, discoveryTimeout, callback, context);
		}

		NFCContext* ctx = &gNFCContexts[chan];

		ctx->getTagInfoCallback = callback;

		sint32 result = NFCSendRawData(chan, true, discoveryTimeout, 1000U, 0, 0, nullptr, RPLLoader_MakePPCCallable(__NFCGetTagInfoCallback), context);
		return __NFCConvertGetTagInfoResult(result);
	}

	sint32 NFCSendRawData(uint32 chan, bool startDiscovery, uint32 discoveryTimeout, uint32 commandTimeout, uint32 commandSize, uint32 responseSize, void* commandData, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_UNINITIALIZED);
		}

		// Only allow discovery
		if (!startDiscovery)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_INVALID_MODE);
		}

		if (NFCGetMode(chan) == NFC_MODE_ACTIVE && NFCSetMode(chan, NFC_MODE_IDLE) < 0)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_INVALID_MODE);
		}

		if (ctx->state != NFC_STATE_IDLE)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_SEND_RAW_DATA, NFC_RESULT_INVALID_STATE);
		}

		ctx->state = NFC_STATE_SEND_RAW_DATA;
		ctx->rawCallback = callback;
		ctx->rawContext = context;

		// If the discoveryTimeout is 0, no timeout
		if (discoveryTimeout == 0)
		{
			ctx->discoveryTimeout = std::chrono::time_point<std::chrono::system_clock>::max();
		}
		else
		{
			ctx->discoveryTimeout = std::chrono::system_clock::now() + std::chrono::milliseconds(discoveryTimeout);
		}

		return NFC_RESULT_SUCCESS;
	}

	sint32 NFCRead(uint32 chan, uint32 discoveryTimeout, NFCUid* uid, NFCUid* uidMask, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_READ, NFC_RESULT_UNINITIALIZED);
		}

		if (NFCGetMode(chan) == NFC_MODE_ACTIVE && NFCSetMode(chan, NFC_MODE_IDLE) < 0)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_READ, NFC_RESULT_INVALID_MODE);
		}

		if (ctx->state != NFC_STATE_IDLE)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_READ, NFC_RESULT_INVALID_STATE);
		}

		ctx->state = NFC_STATE_READ;
		ctx->readCallback = callback;
		ctx->readContext = context;

		// If the discoveryTimeout is 0, no timeout
		if (discoveryTimeout == 0)
		{
			ctx->discoveryTimeout = std::chrono::time_point<std::chrono::system_clock>::max();
		}
		else
		{
			ctx->discoveryTimeout = std::chrono::system_clock::now() + std::chrono::milliseconds(discoveryTimeout);
		}

		memcpy(&ctx->filter.uid, uid, sizeof(*uid));
		memcpy(&ctx->filter.mask, uidMask, sizeof(*uidMask));

		return NFC_RESULT_SUCCESS;
	}

	sint32 NFCWrite(uint32 chan, uint32 discoveryTimeout, NFCUid* uid, NFCUid* uidMask, uint32 size, void* data, MPTR callback, void* context)
	{
		cemu_assert(chan < 2);

		NFCContext* ctx = &gNFCContexts[chan];

		if (!NFCIsInit(chan))
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, NFC_RESULT_UNINITIALIZED);
		}

		if (NFCGetMode(chan) == NFC_MODE_ACTIVE && NFCSetMode(chan, NFC_MODE_IDLE) < 0)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, NFC_RESULT_INVALID_MODE);
		}

		if (ctx->state != NFC_STATE_IDLE)
		{
			return NFC_MAKE_RESULT(NFC_RESULT_BASE_WRITE, NFC_RESULT_INVALID_STATE);
		}

		// Create unknown record which contains the rw area
		ndef::Record rec;
		rec.SetTNF(ndef::Record::NDEF_TNF_UNKNOWN);
		rec.SetPayload(std::span(reinterpret_cast<std::byte*>(data), size));

		// Create ndef message which contains the record
		ndef::Message msg;
		msg.append(rec);
		ctx->writeMessage = msg; 

		ctx->state = NFC_STATE_WRITE;
		ctx->writeCallback = callback;
		ctx->writeContext = context;

		// If the discoveryTimeout is 0, no timeout
		if (discoveryTimeout == 0)
		{
			ctx->discoveryTimeout = std::chrono::time_point<std::chrono::system_clock>::max();
		}
		else
		{
			ctx->discoveryTimeout = std::chrono::system_clock::now() + std::chrono::milliseconds(discoveryTimeout);
		}

		memcpy(&ctx->filter.uid, uid, sizeof(*uid));
		memcpy(&ctx->filter.mask, uidMask, sizeof(*uidMask));

		return NFC_RESULT_SUCCESS;
	}

	void Initialize()
	{
		cafeExportRegister("nfc", NFCInit, LogType::NFC);
		cafeExportRegister("nfc", NFCInitEx, LogType::NFC);
		cafeExportRegister("nfc", NFCShutdown, LogType::NFC);
		cafeExportRegister("nfc", NFCIsInit, LogType::NFC);
		cafeExportRegister("nfc", NFCProc, LogType::NFC);
		cafeExportRegister("nfc", NFCGetMode, LogType::NFC);
		cafeExportRegister("nfc", NFCSetMode, LogType::NFC);
		cafeExportRegister("nfc", NFCSetTagDetectCallback, LogType::NFC);
		cafeExportRegister("nfc", NFCGetTagInfo, LogType::NFC);
		cafeExportRegister("nfc", NFCSendRawData, LogType::NFC);
		cafeExportRegister("nfc", NFCAbort, LogType::NFC);
		cafeExportRegister("nfc", NFCRead, LogType::NFC);
		cafeExportRegister("nfc", NFCWrite, LogType::NFC);
	}

	bool TouchTagFromFile(const fs::path& filePath, uint32* nfcError)
	{
		// Forward this request to nn_nfp, if the title initialized it
		// TODO integrate nn_nfp/ntag/nfc
		if (nnNfp_isInitialized())
		{
			return nnNfp_touchNfcTagFromFile(filePath, nfcError);
		}

		NFCContext* ctx = &gNFCContexts[0];

		auto nfcData = FileStream::LoadIntoMemory(filePath);
		if (!nfcData)
		{
			*nfcError = NFC_TOUCH_TAG_ERROR_NO_ACCESS;
			return false;
		}

		ctx->tag = TagV0::FromBytes(std::as_bytes(std::span(nfcData->data(), nfcData->size())));
		if (!ctx->tag)
		{
			*nfcError = NFC_TOUCH_TAG_ERROR_INVALID_FILE_FORMAT;
			return false;
		}

		ctx->nfcStatus |= NFC_STATUS_HAS_TAG;
		ctx->tagPath = filePath;
		ctx->touchTime = std::chrono::system_clock::now();

		*nfcError = NFC_TOUCH_TAG_ERROR_NONE;
		return true;
	}
}
