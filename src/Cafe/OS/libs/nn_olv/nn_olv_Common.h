#pragma once

#include "Cafe/OS/libs/nn_common.h"
#include "Cafe/OS/common/OSCommon.h"
#include "Cemu/napi/napi_helper.h"
#include "util/helpers/StringHelpers.h"
#include "pugixml.hpp"

// https://github.com/kinnay/NintendoClients/wiki/Wii-U-Error-Codes#act-error-codes
#define OLV_ACT_RESULT_STATUS(code) (BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_OLV, ((code) << 7)))

#define OLV_RESULT_STATUS(code) (BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_OLV, ((code) << 7)))
#define OLV_RESULT_LVL6(code) (BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_OLV, ((code) << 7)))
#define OLV_RESULT_FATAL(code) (BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_OLV, ((code) << 7)))
#define OLV_RESULT_SUCCESS (BUILD_NN_RESULT(0, NN_RESULT_MODULE_NN_OLV, 1 << 7))

#define OLV_RESULT_INVALID_PARAMETER (OLV_RESULT_LVL6(201))
#define OLV_RESULT_INVALID_DATA (OLV_RESULT_LVL6(202))
#define OLV_RESULT_NOT_ENOUGH_SIZE (OLV_RESULT_LVL6(203))
#define OLV_RESULT_INVALID_PTR (OLV_RESULT_LVL6(204))
#define OLV_RESULT_NOT_INITIALIZED (OLV_RESULT_LVL6(205))
#define OLV_RESULT_ALREADY_INITIALIZED (OLV_RESULT_LVL6(206))
#define OLV_RESULT_OFFLINE_MODE_REQUEST (OLV_RESULT_LVL6(207))
#define OLV_RESULT_MISSING_DATA (OLV_RESULT_LVL6(208))
#define OLV_RESULT_INVALID_SIZE (OLV_RESULT_LVL6(209))

#define OLV_RESULT_BAD_VERSION (OLV_RESULT_STATUS(2001))
#define OLV_RESULT_FAILED_REQUEST (OLV_RESULT_STATUS(2003))
#define OLV_RESULT_INVALID_XML (OLV_RESULT_STATUS(2004))
#define OLV_RESULT_INVALID_TEXT_FIELD (OLV_RESULT_STATUS(2006))
#define OLV_RESULT_INVALID_INTEGER_FIELD (OLV_RESULT_STATUS(2007))

#define OLV_CLIENT_ID "87cd32617f1985439ea608c2746e4610"

#define OLV_VERSION_MAJOR 5
#define OLV_VERSION_MINOR 0
#define OLV_VERSION_PATCH 3

namespace nn
{
	namespace olv
	{
		struct ParamPackStorage
		{
			uint64_t titleId;
			uint32_t accessKey;
			uint32_t platformId;
			uint8_t regionId, languageId, countryId, areaId;
			uint8_t networkRestriction, friendRestriction;
			uint32_t ratingRestriction;
			uint8_t ratingOrganization;
			uint64_t transferableId;
			char tzName[72];
			uint64_t utcOffset;
			char encodedParamPack[512];
		};

		struct DiscoveryResultStorage
		{
			sint32 has_error;
			char serviceToken[512];
			char userAgent[64];
			char apiEndpoint[256];
			char portalEndpoint[256];
		};

		extern ParamPackStorage g_ParamPack;
		extern DiscoveryResultStorage g_DiscoveryResults;
		extern uint32_t g_ReportTypes;
		extern bool g_IsInitialized;
		extern bool g_IsOnlineMode;
		extern bool g_IsOfflineDBMode; // use offline cache for posts

		static void InitializeOliveRequest(CurlRequestHelper& req)
		{
			req.addHeaderField("X-Nintendo-ServiceToken", g_DiscoveryResults.serviceToken);
			req.addHeaderField("X-Nintendo-ParamPack", g_ParamPack.encodedParamPack);
			curl_easy_setopt(req.getCURL(), CURLOPT_USERAGENT, g_DiscoveryResults.userAgent);
		}

		static void appendQueryToURL(char* url, const char* query)
		{
			size_t urlLength = strlen(url);
			size_t queryLength = strlen(query);

			char* delimiter = strchr(url, '?');
			if (delimiter)
				snprintf(url + urlLength, queryLength + 2, "&%s", query);
			else
				snprintf(url + urlLength, queryLength + 2, "?%s", query);
		}

		static sint32 CheckOliveResponse(pugi::xml_document& doc)
		{

			/*
				<result>
					<has_error>1</has_error>
					<version>1</version>
					<code>400</code>
					<error_code>4</error_code>
					<message>SERVICE_CLOSED</message>
				</result>
			*/

			pugi::xml_node resultNode = doc.child("result");
			if (!resultNode)
			{
				cemuLog_log(LogType::Force, "Discovery response doesn't contain <result>...</result>");
				return OLV_RESULT_INVALID_XML;
			}

			std::string_view has_error = resultNode.child_value("has_error");
			std::string_view version = resultNode.child_value("version");
			std::string_view code = resultNode.child_value("code");
			std::string_view error_code = resultNode.child_value("error_code");

			if (has_error.compare("1") == 0)
			{
				int codeVal = StringHelpers::ToInt(error_code, -1);
				if (codeVal < 0)
				{
					codeVal = StringHelpers::ToInt(code, -1);
					return OLV_RESULT_STATUS(codeVal + 4000);
				}
				return OLV_RESULT_STATUS(codeVal + 5000);

			}

			if (version.compare("1") != 0)
				return OLV_RESULT_BAD_VERSION; // Version mismatch

			return OLV_RESULT_SUCCESS;
		}

		sint32 olv_copy_wstr(char16_t* dest, const char16_t* src, uint32_t maxSize, uint32_t destSize);
		size_t olv_wstrnlen(const char16_t* str, size_t max_len);
		char16_t* olv_wstrncpy(char16_t* dest, const char16_t* src, size_t n);

#pragma pack(push, 1)
		struct TGAHeader
		{
			uint8 idLength;
			uint8 colorMapType;
			uint8 imageType;
			uint16 first_entry_idx;
			uint16 colormap_length;
			uint8 bpp;
			uint16 x_origin;
			uint16 y_origin;
			uint16 width;
			uint16 height;
			uint8 pixel_depth_bpp;
			uint8 image_desc_bits;
		};
#pragma pack(pop)
		static_assert(sizeof(nn::olv::TGAHeader) == 0x12, "sizeof(nn::olv::TGAHeader != 0x12");

		enum TGACheckType : uint32
		{
			CHECK_PAINTING = 0,
			CHECK_COMMUNITY_ICON = 1,
			CHECK_100x100_200x200 = 2
		};


		bool CheckTGA(const uint8* pTgaFile, uint32 pTgaFileLen, TGACheckType checkType);
		sint32 DecodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, TGACheckType checkType);
		sint32 EncodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, TGACheckType checkTyp);
		
		bool CompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize);
		bool DecompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize);


		bool GetCommunityIdFromCode(uint32* pOutId, const char* pCode);
		bool FormatCommunityCode(char* pOutCode, uint32* outLen, uint32 communityId);

		sint32 olv_curlformcode_to_error(CURLFORMcode code);

		// convert and copy utf8 string into UC2 big-endian array
		template<size_t TLength>
		uint32 SetStringUC2(uint16be(&str)[TLength], std::string_view sv, bool unescape = false)
		{
			if(unescape)
			{
				// todo
			}
			std::wstring ws = boost::nowide::widen(sv);
			size_t copyLen = std::min<size_t>(TLength-1, ws.size());
			for(size_t i=0; i<copyLen; i++)
				str[i] = ws[i];
			str[copyLen] = '\0';
			return copyLen;
		}

		// safely copy null-terminated UC2 big-endian string into UC2 big-endian array
		template<size_t TLength>
		uint32 SetStringUC2(uint16be(&str)[TLength], const uint16be* strIn)
		{
			size_t copyLen = TLength-1;
			for(size_t i=0; i<copyLen; i++)
			{
				if(strIn[i] == 0)
				{
					str[i] = 0;
					return i;
				}
				str[i] = strIn[i];
			}
			str[copyLen] = '\0';
			return copyLen;
		}
	}
}
