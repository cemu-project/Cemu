#pragma once

#define OLV_RESULT_SUCCESS 0x1100080

#include "Cafe/OS/common/OSCommon.h"
#include "Cemu/napi/napi_helper.h"
#include "util/helpers/StringHelpers.h"
#include "pugixml.hpp"

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

		static void InitializeOliveRequest(CurlRequestHelper& req) {
			req.addHeaderField("X-Nintendo-ServiceToken", g_DiscoveryResults.serviceToken);
			req.addHeaderField("X-Nintendo-ParamPack", g_ParamPack.encodedParamPack);
			curl_easy_setopt(req.getCURL(), CURLOPT_USERAGENT, g_DiscoveryResults.userAgent);
		}

		static void appendQueryToURL(char* url, const char* query) {
			size_t urlLength = strlen(url);
			size_t queryLength = strlen(query);

			char* delimiter = strchr(url, '?');
			if (delimiter)
				snprintf(url + urlLength, queryLength + 2, "&%s", query);
			else
				snprintf(url + urlLength, queryLength + 2, "?%s", query);
		}

		static sint32 CheckOliveResponse(pugi::xml_document& doc) {

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
			if (!resultNode) {
				cemuLog_log(LogType::Force, "Discovery response doesn't contain <result>...</result>");
				return 0xA113EA00;
			}

			std::string_view has_error = resultNode.child_value("has_error");
			std::string_view version = resultNode.child_value("version");
			std::string_view code = resultNode.child_value("code");
			std::string_view error_code = resultNode.child_value("error_code");

			if (has_error.compare("1") == 0) {
				int codeVal = StringHelpers::ToInt(code, -1);
				if (codeVal < 0) {
					codeVal = StringHelpers::ToInt(error_code, -1);
					return ((codeVal << 7) - 0x5EE63C00) & 0xFFFFF | 0xA1100000;
				}
				return ((codeVal << 7) - 0x5EE83000) & 0xFFFFF | 0xA1100000;
			}

			if (version.compare("1") != 0) {
				return 0xA113E880;
			}

			return OLV_RESULT_SUCCESS;
		}

		sint32 olv_copy_wstr(char16_t* dest, const char16_t* src, uint32_t maxSize, uint32_t destSize);
		size_t olv_wstrnlen(const char16_t* str, size_t max_len);
		char16_t* olv_wstrncpy(char16_t* dest, const char16_t* src, size_t n);

		sint32 DecodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, sint32 checkEnum);
		sint32 EncodeTGA(uint8* pInBuffer, uint32 inSize, uint8* pOutBuffer, uint32 outSize, sint32 checkEnum);
		
		bool CompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize);
		bool DecompressTGA(uint8* pOutBuffer, uint32* pOutSize, uint8* pInBuffer, uint32 inSize);

		bool CheckTGA(const uint8* pTgaFile, uint32 pTgaFileLen, sint32 unk);

		bool GetCommunityIdFromCode(uint32* pOutId, const char* pCode);
		bool FormatCommunityCode(char* pOutCode, uint32* outLen, uint32 communityId);

		sint32 olv_curlformcode_to_error(CURLFORMcode code);

	}
}
