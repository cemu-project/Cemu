#include "nn_olv_Common.h"
#include <zlib.h>

namespace nn
{
	namespace olv
	{

		sint32 olv_copy_wstr(char16_t* dest, const char16_t* src, uint32_t maxSize, uint32_t destSize)
		{
			size_t len = maxSize + 1;
			if (olv_wstrnlen(src, len) > maxSize)
				return OLV_RESULT_NOT_ENOUGH_SIZE;

			memset(dest, 0, 2 * destSize);
			olv_wstrncpy(dest, src, len);
			return OLV_RESULT_SUCCESS;
		}

		size_t olv_wstrnlen(const char16_t* str, size_t max_len)
		{
			size_t len = 0;
			while (len < max_len && str[len] != u'\0')
				len++;

			return len;
		}

		char16_t* olv_wstrncpy(char16_t* dest, const char16_t* src, size_t n)
		{
			char16_t* ret = dest;
			while (n > 0 && *src != u'\0')
			{
				*dest++ = *src++;
				n--;
			}
			while (n > 0)
			{
				*dest++ = u'\0';
				n--;
			}
			return ret;
		}

		bool CheckTGA(const uint8* pTgaFile, uint32 pTgaFileLen, TGACheckType checkType)
		{
			const TGAHeader* header = (const TGAHeader*)pTgaFile;
			try
			{
				if (checkType == TGACheckType::CHECK_PAINTING)
				{
					if (
						header->idLength ||
						header->colorMapType ||
						header->imageType != 2 || // Uncompressed true color
						header->first_entry_idx ||
						header->colormap_length ||
						header->bpp ||
						header->x_origin ||
						header->y_origin ||
						header->width != 320 ||
						header->height != 120 ||
						header->pixel_depth_bpp != 32 ||
						header->image_desc_bits != 8
						)
					{
						throw std::runtime_error("TGACheckType::CHECK_PAINTING - Invalid TGA file!");
					}
				}
				else if (checkType == TGACheckType::CHECK_COMMUNITY_ICON)
				{
					if (header->width != 128 || header->height != 128 || header->pixel_depth_bpp != 32)
						throw std::runtime_error("TGACheckType::CHECK_COMMUNITY_ICON - Invalid TGA file -> width, height or bpp is wrong");
				}
				else if (checkType == TGACheckType::CHECK_100x100_200x200)
				{
					if (header->pixel_depth_bpp != 32)
						throw std::runtime_error("TGACheckType::CHECK_100x100_200x200 - Invalid TGA file -> bpp is wrong");

					if (header->width == 100)
					{
						if (header->height != 100)
							throw std::runtime_error("TGACheckType::CHECK_100x100_200x200 - Invalid TGA file -> Not 100x100");
					}
					else if (header->width != 200 || header->height != 200)
						throw std::runtime_error("TGACheckType::CHECK_100x100_200x200 - Invalid TGA file -> Not 100x100 or 200x200");
				}
			}
			catch (const std::runtime_error& error)
			{
				// TGA Check Error! illegal format
				cemuLog_log(LogType::Force, error.what());
				return false;
			}
			return true;
		}

		sint32 DecodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, TGACheckType checkType)
		{
			uint32 decompressedSize = outSize;
			if (DecompressTGA(pOutBuffer, &decompressedSize, pInBuffer, inSize))
			{
				if (CheckTGA(pOutBuffer, decompressedSize, checkType))
					return decompressedSize;
				
				return -2;
			}
			else
			{
				cemuLog_log(LogType::Force, "OLIVE uncompress error.\n");
				return -1;
			}
		}

		sint32 EncodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, TGACheckType checkType)
		{
			if (inSize == outSize)
			{
				if (!CheckTGA(pInBuffer, inSize, checkType))
					return -1;
				
					uint32 compressedSize = outSize;
					if (CompressTGA(pOutBuffer, &compressedSize, pInBuffer, inSize))
						return compressedSize;
					else
					{
						cemuLog_log(LogType::Force, "OLIVE compress error.\n");
						return -1;
					}
			}
			else
			{
				cemuLog_log(LogType::Force, "compress buffer size check error. uSrcBufSize({}) != uDstBufSize({})\n", inSize, outSize);
				return -1;
			}
		}

		bool DecompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize)
		{
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

		bool CompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize)
		{
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
			uint64 topByte = GetCommunityCodeTopByte(communityId);
			if ((0xe7 < topByte) && ((0xe8 < topByte || (0xd4a50fff < communityId))))
				return ((topByte << 32) | communityId) & 0x7fffffffff;

			return ((topByte << 32) | communityId);
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

		bool FormatCommunityCode(char* pOutCode, uint32* outLen, uint32 communityId)
		{
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

		uint32 ExtractCommunityIdFromCode(char* pCode)
		{
			uint32 id = 0;
			uint64 code;
			if (sscanf(pCode, "%012llu", &code) > 0)
			{
				uint32 lower_code = code;
				id = CreateCommunityIdByCode(lower_code);
			}
			return id;
		}

		bool GetCommunityIdFromCode(uint32* pOutId, const char* pCode)
		{
			if (!EnsureCommunityCode((char*)pCode))
				return false;

			*pOutId = ExtractCommunityIdFromCode((char*)pCode);
			return true;
		}

		sint32 olv_curlformcode_to_error(CURLFORMcode code)
		{
			switch (code)
			{
				case CURL_FORMADD_OK:
					return OLV_RESULT_SUCCESS;

				case CURL_FORMADD_MEMORY:
					return OLV_RESULT_FATAL(25);

				case CURL_FORMADD_OPTION_TWICE:
				default:
					return OLV_RESULT_LVL6(50);
			}
		}
	}
}