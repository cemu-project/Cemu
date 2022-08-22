#pragma once

using nnResult = uint32;

inline bool NN_RESULT_IS_SUCCESS(nnResult r) 
{
	return (r & 0x80000000) == 0;
}

inline bool NN_RESULT_IS_FAILURE(nnResult r)
{
	return (r & 0x80000000) != 0;
}

// any level with MSB set is considered an error
#define NN_RESULT_LEVEL_FATAL		(0x7) // 111
#define NN_RESULT_LEVEL_LVL6		(0x6) // 110
#define NN_RESULT_LEVEL_STATUS		(0x5) // 101
#define NN_RESULT_LEVEL_SUCCESS		(0x0) // 000

#define BUILD_NN_RESULT(__level, __module, __detailedErrorCode) ((((uint32)(__level)&0x7)<<29) | ((uint32)(__module)<<20)  | ((uint32)(__detailedErrorCode)<<0) )
#define BUILD_NN_RESULT_STATUS(__module, __detailedErrorCode) ((((uint32)NN_RESULT_LEVEL_STATUS)<<29) | ((uint32)(__module)<<20) | ((uint32)(__detailedErrorCode)<<0) )

#define NN_RESULT_SUCCESS	((nnResult)0)
#define NN_RESULT_PLACEHOLDER_ERROR	((nnResult)0x80000000)

enum NN_RESULT_MODULE : uint32
{
	NN_RESULT_MODULE_COMMON			= 0,
	NN_RESULT_MODULE_NN_IPC			= 1,
	NN_RESULT_MODULE_NN_BOSS		= 2,
	NN_RESULT_MODULE_NN_ACP			= 3,
	NN_RESULT_MODULE_NN_IOS			= 4,
	NN_RESULT_MODULE_NN_NIM			= 5,
	NN_RESULT_MODULE_NN_PDM			= 6,
	NN_RESULT_MODULE_NN_ACT			= 7,
	NN_RESULT_MODULE_NN_NUP			= 10,
	NN_RESULT_MODULE_NN_NDM			= 11,
	NN_RESULT_MODULE_NN_FP			= 12,
	NN_RESULT_MODULE_NN_AC			= 13,
	NN_RESULT_MODULE_NN_DRMAPP		= 15,
	NN_RESULT_MODULE_NN_OLV			= 17,
	NN_RESULT_MODULE_NN_VCTL		= 18,
	NN_RESULT_MODULE_NN_SPM			= 20,
	NN_RESULT_MODULE_NN_EC			= 22,
	NN_RESULT_MODULE_NN_SL			= 24,
	NN_RESULT_MODULE_NN_ECO			= 25,
	NN_RESULT_MODULE_NN_NFP			= 27,

	NN_RESULT_MODULE_MCP			= 511,
};

// 0000-9999
// these are the user-facing error codes you see in ErrEula. Usually combined with a module-specific prefix. E.g. 102-1234
// in nnResult these are stored in the description field left-shifted by 7
enum class NN_ERROR_CODE : uint32
{
	ACT_UNKNOWN_SERVER_ERROR = 2932,
};

// takes simplified error code in range 0-9999
constexpr inline nnResult nnResultStatus(NN_RESULT_MODULE module, NN_ERROR_CODE errorCode)
{
	uint32 description = (uint32)errorCode;
	description <<= 7;
	return BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, (uint32)module, description);
}

// nn_act description codes - todo: define these as NN_ERROR_CODE
#define NN_ACT_RESULT_ACCOUNT_DOES_NOT_EXIST	128128

// nn_nfp description codes
#define NN_RESULT_NFP_CODE_APPAREAIDMISMATCH	0x11300
#define NN_RESULT_NFP_CODE_NOAPPAREA			0x10400

namespace nn
{
	inline NN_RESULT_MODULE nnResult_GetModule(const nnResult res) { return (NN_RESULT_MODULE)((res >> 20) & 0x1FF); };
	inline uint32 nnResult_GetDescription(const nnResult res) { return (uint32)((res >> 0) & 0xFFFFF); };

	#define	NN_ERRCODE_MODULE_PREFIX_ACT	1020000

	inline NN_ERROR_CODE NNResultToErrorCode(nnResult result, NN_RESULT_MODULE expectedModule)
	{
		if (((uint32)result & 0x18000000) == 0x18000000)
		{
			cemu_assert_unimplemented();
		}
		uint32 errorCodeBase = 0;
		if (expectedModule == NN_RESULT_MODULE_NN_ACT)
			errorCodeBase = NN_ERRCODE_MODULE_PREFIX_ACT;
		else
		{
			cemu_assert_unimplemented();
		}

		NN_RESULT_MODULE module = nnResult_GetModule(result);
		if (module != expectedModule)
			return (NN_ERROR_CODE)(errorCodeBase + 9999);
		uint32 desc = nnResult_GetDescription(result);
		uint32 errCode = (desc >> 7);
		if (errCode > 9999)
		{
			cemu_assert_suspicious();
			return (NN_ERROR_CODE)(errorCodeBase + 9999);
		}
		return (NN_ERROR_CODE)(errorCodeBase + errCode);
	}

}

// tests
static_assert(BUILD_NN_RESULT_STATUS(NN_RESULT_MODULE_NN_ACT, 317696) == 0xA074D900);
