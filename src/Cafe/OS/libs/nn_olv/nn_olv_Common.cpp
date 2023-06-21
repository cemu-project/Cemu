#include "nn_olv_Common.h"
#include <zlib.h>

namespace nn
{
	namespace olv
	{

		sint32 olv_copy_wstr(char16_t* dest, const char16_t* src, uint32_t maxSize, uint32_t destSize) {
			size_t len = maxSize + 1;
			if (olv_wstrnlen(src, len) > maxSize)
				return 0xC1106580;

			memset(dest, 0, 2 * destSize);
			olv_wstrncpy(dest, src, len);
			return OLV_RESULT_SUCCESS;
		}

		size_t olv_wstrnlen(const char16_t* str, size_t max_len) {
			size_t len = 0;
			while (len < max_len && str[len] != u'\0')
			{
				len++;
			}
			return len;
		}

		char16_t* olv_wstrncpy(char16_t* dest, const char16_t* src, size_t n) {
			char16_t* ret = dest;
			while (n > 0 && *src != u'\0') {
				*dest++ = *src++;
				n--;
			}
			while (n > 0) {
				*dest++ = u'\0';
				n--;
			}
			return ret;
		}

		sint32 DecodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, sint32 checkEnum) {
			sint32 result; // r3
			sint32 v8; // r31
			uint32 v9; // [sp+8h] [-10h] BYREF

			v9 = outSize;
			if (DecompressTGA(pOutBuffer, &v9, pInBuffer, inSize))
			{
				v8 = -2;
				if (CheckTGA(pOutBuffer, v9, checkEnum))
					v8 = v9;
				result = v8;
			}
			else
			{
				cemuLog_log(LogType::Force, "OLIVE uncompress error.\n");
				result = -1;
			}
			return result;
		}

		sint32 EncodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, sint32 checkEnum) {
			sint32 result; // r3
			uint32 v10; // [sp+8h] [-18h] BYREF

			if (inSize == outSize)
			{
				if (CheckTGA(pInBuffer, inSize, checkEnum))
				{
					v10 = outSize;
					if (CompressTGA(pOutBuffer, &v10, pInBuffer, inSize))
					{
						result = v10;
					}
					else
					{
						cemuLog_log(LogType::Force, "OLIVE compress error.\n");
						result = -1;
					}
				}
				else
				{
					result = -1;
				}
			}
			else
			{
				cemuLog_log(LogType::Force, "compress buffer size check error. uSrcBufSize({}) != uDstBufSize({})\n", inSize, outSize);
				result = -1;
			}
			return result;
		}

		bool DecompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize) {
			if (pOutBuffer == nullptr || pOutSize == nullptr || pInBuffer == nullptr || inSize == 0)
				return false;

			uLongf bufferSize = *pOutSize;
			int result = uncompress(pOutBuffer, &bufferSize, pInBuffer, inSize);

			if (result == Z_OK)
			{
				*pOutSize = static_cast<unsigned int>(bufferSize);
				return true;
			}
			else
			{
				const char* error_msg = (result == Z_MEM_ERROR) ? "Insufficient memory" : "Unknown decompression error";
				cemuLog_log(LogType::Force, "OLIVE ZLIB - ERROR: {}\n", error_msg);
				return false;
			}
		}

		bool CompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize) {
			if (pOutBuffer == nullptr || pOutSize == nullptr || pInBuffer == nullptr || inSize == 0)
				return false;

			uLongf bufferSize = *pOutSize;
			int result = compress(pOutBuffer, &bufferSize, pInBuffer, inSize);

			if (result == Z_OK)
			{
				*pOutSize = static_cast<unsigned int>(bufferSize);
				return true;
			}
			else
			{
				const char* error_msg = (result == Z_MEM_ERROR) ? "Insufficient memory" : "Unknown compression error";
				cemuLog_log(LogType::Force, "OLIVE ZLIB - ERROR: {}\n", error_msg);
				return false;
			}
		}

		bool CheckTGA(const uint8* pTgaFile, uint32 pTgaFileLen, sint32 unk) {
			int v4; // r0
			if (unk)
			{
				if (unk == 1)
				{
					if (pTgaFile[12] != 0x80 || pTgaFile[13] || pTgaFile[14] != 128 || pTgaFile[15] || pTgaFile[16] != 32)
						goto LABEL_39;
				}
				else
				{
					if (unk != 2)
						return 1;
					if (pTgaFile[16] != 32)
						goto LABEL_39;
					v4 = pTgaFile[12];
					if (v4 == 100)
					{
						if (pTgaFile[13] || pTgaFile[14] != 100)
							goto LABEL_39;
					}
					else if (v4 != 200 || pTgaFile[13] || pTgaFile[14] != 200)
					{
						goto LABEL_39;
					}
					if (pTgaFile[15])
					{
					LABEL_39:
						cemuLog_log(LogType::Force, "OLIVE TGA Check Error! illegal format.\n");
						return 0;
					}
				}
			}
			else if (*pTgaFile || pTgaFile[1] || pTgaFile[2] != 2 || pTgaFile[3] || pTgaFile[4] || pTgaFile[5] || pTgaFile[6] || pTgaFile[7] || pTgaFile[8] || pTgaFile[9] || pTgaFile[10] || pTgaFile[11] || pTgaFile[12] != 64 || pTgaFile[13] != 1 || pTgaFile[14] != 120 || pTgaFile[15] || pTgaFile[16] != 32 || pTgaFile[17] != 8)
			{
				goto LABEL_39;
			}
			return 1;
		}

		constexpr uint32 CreateCommunityCodeById(uint32 communityId)
		{
			uint32 res = communityId ^ (communityId << 18) ^ (communityId << 24) ^ (communityId << 30);
			return res ^ (16 * (res & 0xF0F0F0F)) ^ ((res ^ (16 * (res & 0xF0F0F0F))) >> 17) ^ ((res ^ (16 * (res & 0xF0F0F0F))) >> 23) ^ ((res ^ (16 * (res & 0xF0F0F0F))) >> 29) ^ 0x20121002;
		}

		constexpr uint32 CreateCommunityIdByCode(uint32 code)
		{
			uint32 res = code ^ 0x20121002 ^ ((code ^ 0x20121002u) >> 17) ^ ((code ^ 0x20121002u) >> 23) ^ ((code ^ 0x20121002u) >> 29);
			return res ^ (16 * (res & 0xF0F0F0F)) ^ ((res ^ (16 * (res & 0xF0F0F0F))) << 18) ^ ((res ^ (16 * (res & 0xF0F0F0F))) << 24) ^ ((res ^ (16 * (res & 0xF0F0F0F))) << 30);
		}


		constexpr uint32 GetCommunityCodeTopByte(uint32 communityId)
		{
			uint8 code_byte3 = (uint8_t)(communityId >> 0x18);
			uint8 code_byte2 = (uint8_t)(communityId >> 0x10);
			uint8 code_byte1 = (uint8_t)(communityId >> 8);
			uint8 code_byte0 = (uint8_t)(communityId >> 0);
			return code_byte3 ^ code_byte2 ^ code_byte1 ^ code_byte0 ^ 0xff;
		}

		constexpr uint64 GetRealCommunityCode(uint32_t communityId)
		{
			uint64 uVar1 = GetCommunityCodeTopByte(communityId);
			if ((0xe7 < uVar1) && ((0xe8 < uVar1 || (0xd4a50fff < communityId))))
			{
				return ((uVar1 << 32) | communityId) & 0x7fffffffff;
			}
			return ((uVar1 << 32) | communityId);
		}

		void WriteCommunityCode(char* pOutCode, uint32 communityId)
		{
			uint32 code = CreateCommunityCodeById(communityId);
			uint64 communityCode = GetRealCommunityCode(code);
			sprintf(pOutCode, "%012llu", communityCode);
		}

		bool EnsureCommunityCode(char* pCode)
		{
			uint64 code;
			if (sscanf(pCode, "%012llu", &code) > 0)
			{
				uint32 lowerCode = code;
				uint64 newCode = GetRealCommunityCode(code);
				return code == newCode;
			}
			return false;
		}

		bool FormatCommunityCode(char* pOutCode, uint32* outLen, uint32 communityId) {
			bool result = false;
			if (communityId != -1)
			{
				if (communityId)
				{
					WriteCommunityCode(pOutCode, communityId);
					*outLen = strnlen(pOutCode, 12);
					if (EnsureCommunityCode(pOutCode))
						result = 1;
				}
			}
			return result;
		}

		static_assert(GetRealCommunityCode(CreateCommunityCodeById(140500)) == 717651734336, "Wrong community code generation code, result must match.");

		uint32 ExtractCommunityIdFromCode(char* pCode) {
			uint32 id = 0;
			uint64 code;
			if (sscanf(pCode, "%012llu", &code) > 0) {
				uint32 lower_code = code;
				id = CreateCommunityIdByCode(lower_code);
			}
			return id;
		}

		bool GetCommunityIdFromCode(uint32* pOutId, const char* pCode) {
			if (!EnsureCommunityCode((char*)pCode))
				return false;

			*pOutId = ExtractCommunityIdFromCode((char*)pCode);
			return true;
		}

		sint32 olv_curlformcode_to_error(CURLFORMcode code) {
			switch (code) {
			case CURL_FORMADD_OK:
				return OLV_RESULT_SUCCESS;

			case CURL_FORMADD_MEMORY:
				return 0xE1103280;

			case CURL_FORMADD_OPTION_TWICE:
			default:
				return 0xC1106480;
			}
		}
	}
}