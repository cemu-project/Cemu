#pragma once

#include "nn_olv_InitializeTypes.h"
#include "CafeSystem.h"
#include "Cafe/OS/libs/nn_act/nn_act.h"
#include <time.h>

namespace nn
{
	namespace olv
	{
		uint32_t g_ReportTypes = 0;
		bool g_IsOnlineMode = false;
		bool g_IsInitialized = false;
		bool g_IsOfflineDBMode = false;
		ParamPackStorage g_ParamPack;
		DiscoveryResultStorage g_DiscoveryResults;

		sint32 GetOlvAccessKey(uint32_t* pOutKey)
		{
			*pOutKey = 0;
			uint32_t accessKey = CafeSystem::GetForegroundTitleOlvAccesskey();
			if (accessKey == -1)
				return OLV_RESULT_STATUS(1102);

			*pOutKey = accessKey;
			return OLV_RESULT_SUCCESS;
		}

		sint32 CreateParamPack(uint64_t titleId, uint32_t accessKey)
		{
			g_ParamPack.languageId = uint8(GetConfig().console_language.GetValue());

			uint32be simpleAddress = 0;
			nn::act::GetSimpleAddressIdEx(&simpleAddress, nn::act::ACT_SLOT_CURRENT);
			uint32 countryCode = nn::act::getCountryCodeFromSimpleAddress(simpleAddress);

			g_ParamPack.countryId = countryCode;
			g_ParamPack.titleId = titleId;
			g_ParamPack.platformId = 1;

			g_ParamPack.areaId = (simpleAddress >> 8) & 0xff;

			MCPHANDLE handle = MCP_Open();
			SysProdSettings sysProdSettings;
			MCP_GetSysProdSettings(handle, &sysProdSettings);
			MCP_Close(handle);

			g_ParamPack.regionId = sysProdSettings.platformRegion;
			g_ParamPack.accessKey = accessKey;

			g_ParamPack.networkRestriction = 0;
			g_ParamPack.friendRestriction = 0;
			g_ParamPack.ratingRestriction = 18;
			g_ParamPack.ratingOrganization = 4; // PEGI ?

			uint64 transferrableId;
			nn::act::GetTransferableIdEx(&transferrableId, (titleId >> 8) & 0xFFFFF, nn::act::ACT_SLOT_CURRENT);
			g_ParamPack.transferableId = transferrableId;

			strcpy(g_ParamPack.tzName, "CEMU/Olive"); // Should be nn::act::GetTimeZoneId
			g_ParamPack.utcOffset = (uint64_t)nn::act::GetUtcOffset() / 1'000'000;

			char paramPackStr[1024];
			snprintf(
				paramPackStr,
				sizeof(paramPackStr),
				"\\%s\\%llu\\%s\\%u\\%s\\%u\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%d\\%s\\%u\\%s\\%d\\%s\\%llu\\"
				"%s\\%s\\%s\\%lld\\",
				"title_id",
				g_ParamPack.titleId,
				"access_key",
				g_ParamPack.accessKey,
				"platform_id",
				g_ParamPack.platformId,
				"region_id",
				g_ParamPack.regionId,
				"language_id",
				g_ParamPack.languageId,
				"country_id",
				g_ParamPack.countryId,
				"area_id",
				g_ParamPack.areaId,
				"network_restriction",
				g_ParamPack.networkRestriction,
				"friend_restriction",
				g_ParamPack.friendRestriction,
				"rating_restriction",
				g_ParamPack.ratingRestriction,
				"rating_organization",
				g_ParamPack.ratingOrganization,
				"transferable_id",
				g_ParamPack.transferableId,
				"tz_name",
				g_ParamPack.tzName,
				"utc_offset",
				g_ParamPack.utcOffset);
			std::string encodedParamPack = NCrypto::base64Encode(paramPackStr, strnlen(paramPackStr, 1024));
			memset(&g_ParamPack.encodedParamPack, 0, sizeof(g_ParamPack.encodedParamPack));
			memcpy(&g_ParamPack.encodedParamPack, encodedParamPack.data(), encodedParamPack.size());

			return OLV_RESULT_SUCCESS;
		}

		sint32 MakeDiscoveryRequest_AsyncRequestImpl(CurlRequestHelper& req, const char* reqUrl)
		{
			bool reqResult = req.submitRequest();
			long httpCode = 0;
			curl_easy_getinfo(req.getCURL(), CURLINFO_RESPONSE_CODE, &httpCode);
			if (!reqResult)
			{
				cemuLog_log(LogType::Force, "Failed request: {} ({})", reqUrl, httpCode);
				if (!(httpCode >= 400))
					return OLV_RESULT_FAILED_REQUEST;
			}

			pugi::xml_document doc;
			if (!doc.load_buffer(req.getReceivedData().data(), req.getReceivedData().size()))
			{
				cemuLog_log(LogType::Force, fmt::format("Invalid XML in discovery service response"));
				return OLV_RESULT_INVALID_XML;
			}

			sint32 responseError = CheckOliveResponse(doc);
			if (responseError < 0)
				return responseError;

			if (httpCode != 200)
				return OLV_RESULT_STATUS(httpCode + 4000);


			/*
				<result>
					<has_error>0</has_error>
					<version>1</version>
					<endpoint>
						<host>api.olv.pretendo.cc</host>
						<api_host>api.olv.pretendo.cc</api_host>
						<portal_host>portal.olv.pretendo.cc</portal_host>
						<n3ds_host>ctr.olv.pretendo.cc</n3ds_host>
					</endpoint>
				</result>
			*/

			pugi::xml_node resultNode = doc.child("result");
			if (!resultNode)
			{
				cemuLog_log(LogType::Force, "Discovery response doesn't contain <result>");
				return OLV_RESULT_INVALID_XML;
			}

			pugi::xml_node endpointNode = resultNode.child("endpoint");
			if (!endpointNode)
			{
				cemuLog_log(LogType::Force, "Discovery response doesn't contain <result><endpoint>");
				return OLV_RESULT_INVALID_XML;
			}

			// Yes it only uses <host> and <portal_host>
			std::string_view host = endpointNode.child_value("host");
			std::string_view portal_host = endpointNode.child_value("portal_host");

			snprintf(g_DiscoveryResults.apiEndpoint, sizeof(g_DiscoveryResults.apiEndpoint), "https://%s", host.data());
			snprintf(g_DiscoveryResults.portalEndpoint, sizeof(g_DiscoveryResults.portalEndpoint), "https://%s", portal_host.data());

			return OLV_RESULT_SUCCESS;
		}

		sint32 MakeDiscoveryRequest_AsyncRequest(CurlRequestHelper& req, const char* reqUrl, coreinit::OSEvent* requestDoneEvent)
		{
			sint32 res = MakeDiscoveryRequest_AsyncRequestImpl(req, reqUrl);
			coreinit::OSSignalEvent(requestDoneEvent);
			return res;
		}

		sint32 MakeDiscoveryRequest()
		{
			// =============================================================================
			// Discovery request | https://discovery.olv.nintendo.net/v1/endpoint
			// =============================================================================

			CurlRequestHelper req;
			std::string requestUrl;
			switch (ActiveSettings::GetNetworkService())
			{
				case NetworkService::Pretendo:
					requestUrl = PretendoURLs::OLVURL;
					break;
				case NetworkService::Custom:
					requestUrl = GetNetworkConfig().urls.OLV.GetValue();
					break;
				case NetworkService::Nintendo:
				default:
					requestUrl = NintendoURLs::OLVURL;
					break;
			}

			req.initate(requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::OLIVE);
			InitializeOliveRequest(req);

			StackAllocator<coreinit::OSEvent> requestDoneEvent;
			coreinit::OSInitEvent(requestDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			std::future<sint32> requestRes = std::async(std::launch::async, MakeDiscoveryRequest_AsyncRequest, std::ref(req), requestUrl.c_str(), requestDoneEvent.GetPointer());
			coreinit::OSWaitEvent(requestDoneEvent);

			return requestRes.get();
		}

		sint32 Initialize(nn::olv::InitializeParam* pParam)
		{
			if (g_IsInitialized)
				return OLV_RESULT_ALREADY_INITIALIZED;

			if (!pParam->m_Work)
			{
				g_IsInitialized = false;
				return OLV_RESULT_INVALID_PTR;
			}

			if (pParam->m_WorkSize < 0x10000)
			{
				g_IsInitialized = false;
				return OLV_RESULT_INVALID_SIZE;
			}

			uint32_t accessKey;
			int32_t olvAccessKeyStatus = GetOlvAccessKey(&accessKey);
			if (olvAccessKeyStatus < 0)
			{
				g_IsInitialized = false;
				return olvAccessKeyStatus;
			}

			uint64_t tid = CafeSystem::GetForegroundTitleId();
			int32_t createParamPackResult = CreateParamPack(tid, accessKey);
			if (createParamPackResult < 0)
			{
				g_IsInitialized = false;
				return createParamPackResult;
			}

			g_IsInitialized = true;

			if(ActiveSettings::GetNetworkService() == NetworkService::Nintendo)
			{
				// since the official Miiverse was shut down, use local post archive instead
				g_IsOnlineMode = true;
				g_IsOfflineDBMode = true;
				return OLV_RESULT_SUCCESS;
			}
			if ((pParam->m_Flags & InitializeParam::FLAG_OFFLINE_MODE) == 0)
			{
				g_IsOnlineMode = true;

				independentServiceToken_t token;
				sint32 res = (sint32)nn::act::AcquireIndependentServiceToken(&token, OLV_CLIENT_ID, 0);
				if (res < 0)
				{
					g_IsInitialized = false;
					return res;
				}

				// Assuming we're always a production WiiU (non-dev)
				uint32 uniqueId = (CafeSystem::GetForegroundTitleId() >> 8) & 0xFFFFF;

				char versionBuffer[32];
				snprintf(versionBuffer, sizeof(versionBuffer), "%d.%d.%d", OLV_VERSION_MAJOR, OLV_VERSION_MINOR, OLV_VERSION_PATCH);
				snprintf(g_DiscoveryResults.userAgent, sizeof(g_DiscoveryResults.userAgent), "%s/%s-%s/%d", "WiiU", "POLV", versionBuffer, uniqueId);

				memcpy(g_DiscoveryResults.serviceToken, token.token, sizeof(g_DiscoveryResults.serviceToken));

				sint32 discoveryRes = MakeDiscoveryRequest();
				if (discoveryRes < 0)
					g_IsInitialized = false;

				return discoveryRes;
			}

			return OLV_RESULT_SUCCESS;
		}

		namespace Report
		{
			uint32 GetReportTypes()
			{
				return g_ReportTypes;
			}

			void SetReportTypes(uint32 reportTypes)
			{
				g_ReportTypes = reportTypes | 0x1000;
			}
		}

		bool IsInitialized()
		{
			return g_IsInitialized;
		}
	}
}