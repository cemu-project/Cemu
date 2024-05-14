#include "nn_olv_UploadFavoriteTypes.h"
#include <algorithm>

namespace nn
{
	namespace olv
	{
	
		sint32 UploadFavoriteToCommunityData_AsyncRequestImpl(CurlRequestHelper& req, const char* reqUrl,
			UploadedFavoriteToCommunityData* pOutData, const UploadFavoriteToCommunityDataParam* pParam
		);

		sint32 UploadFavoriteToCommunityData_AsyncRequest(CurlRequestHelper& req, const char* reqUrl, coreinit::OSEvent* requestDoneEvent,
			UploadedFavoriteToCommunityData* pOutData, const UploadFavoriteToCommunityDataParam* pParam
		)
		{
			sint32 res = UploadFavoriteToCommunityData_AsyncRequestImpl(req, reqUrl, pOutData, pParam);
			coreinit::OSSignalEvent(requestDoneEvent);
			return res;
		}

		sint32 UploadFavoriteToCommunityData(UploadedFavoriteToCommunityData* pOutData, const UploadFavoriteToCommunityDataParam* pParam)
		{
			if (!nn::olv::g_IsInitialized)
				return OLV_RESULT_NOT_INITIALIZED;

			if (!nn::olv::g_IsOnlineMode)
				return OLV_RESULT_OFFLINE_MODE_REQUEST;

			if (!pParam)
				return OLV_RESULT_INVALID_PTR;

			if (pOutData)
				UploadedFavoriteToCommunityData::Clean(pOutData);

			char requestUrl[512];
			if (pParam->flags & UploadFavoriteToCommunityDataParam::FLAG_DELETION)
				snprintf(requestUrl, sizeof(requestUrl), "%s/v1/communities/%lu.unfavorite", g_DiscoveryResults.apiEndpoint, pParam->communityId.value());
			else 
				snprintf(requestUrl, sizeof(requestUrl), "%s/v1/communities/%lu.favorite", g_DiscoveryResults.apiEndpoint, pParam->communityId.value());
			
			CurlRequestHelper req;
			req.initate(ActiveSettings::GetNetworkService(), requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::OLIVE);
			InitializeOliveRequest(req);

			StackAllocator<coreinit::OSEvent> requestDoneEvent;
			coreinit::OSInitEvent(&requestDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			std::future<sint32> requestRes = std::async(std::launch::async, UploadFavoriteToCommunityData_AsyncRequest, std::ref(req), requestUrl, requestDoneEvent.GetPointer(), pOutData, pParam);
			coreinit::OSWaitEvent(&requestDoneEvent);

			return requestRes.get();
		}

		sint32 UploadFavoriteToCommunityData(const UploadFavoriteToCommunityDataParam* pParam)
		{
			return UploadFavoriteToCommunityData(nullptr, pParam);
		}

		sint32 UploadFavoriteToCommunityData_AsyncRequestImpl(CurlRequestHelper& req, const char* reqUrl,
			UploadedFavoriteToCommunityData* pOutData, const UploadFavoriteToCommunityDataParam* pParam
		)
		{
			bool reqResult = req.submitRequest(true);
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
				cemuLog_log(LogType::Force, fmt::format("Invalid XML in community favorite upload response"));
				return OLV_RESULT_INVALID_XML;
			}

			sint32 responseError = CheckOliveResponse(doc);
			if (responseError < 0)
				return responseError;

			if (httpCode != 200)
				return OLV_RESULT_STATUS(httpCode + 4000);

			if (pOutData)
			{
				std::string_view app_data = doc.select_single_node("//app_data").node().child_value();
				std::string_view community_id = doc.select_single_node("//community_id").node().child_value();
				std::string_view name = doc.select_single_node("//name").node().child_value();
				std::string_view description = doc.select_single_node("//description").node().child_value();
				std::string_view pid = doc.select_single_node("//pid").node().child_value();
				std::string_view icon = doc.select_single_node("//icon").node().child_value();

				if (app_data.size() != 0)
				{
					auto app_data_bin = NCrypto::base64Decode(app_data);
					if (app_data_bin.size() != 0)
					{
						memcpy(pOutData->appData, app_data_bin.data(), std::min(size_t(0x400), app_data_bin.size()));
						pOutData->flags |= UploadedFavoriteToCommunityData::FLAG_HAS_APP_DATA;
						pOutData->appDataLen = app_data_bin.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				sint64 community_id_val = StringHelpers::ToInt64(community_id, -1);
				if (community_id_val == -1)
					return OLV_RESULT_INVALID_INTEGER_FIELD;

				pOutData->communityId = community_id_val;

				if (name.size() != 0)
				{
					auto name_utf16 = StringHelpers::FromUtf8(name).substr(0, 128);
					if (name_utf16.size() != 0)
					{
						for (int i = 0; i < name_utf16.size(); i++)
							pOutData->titleText[i] = name_utf16.at(i);

						pOutData->flags |= UploadedFavoriteToCommunityData::FLAG_HAS_TITLE_TEXT;
						pOutData->titleTextMaxLen = name_utf16.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				if (description.size() != 0)
				{
					auto description_utf16 = StringHelpers::FromUtf8(description).substr(0, 256);
					if (description_utf16.size() != 0)
					{
						for (int i = 0; i < description_utf16.size(); i++)
							pOutData->description[i] = description_utf16.at(i);

						pOutData->flags |= UploadedFavoriteToCommunityData::FLAG_HAS_DESC_TEXT;
						pOutData->descriptionMaxLen = description_utf16.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				sint64 pid_val = StringHelpers::ToInt64(pid, -1);
				if (pid_val == -1)
					return OLV_RESULT_INVALID_INTEGER_FIELD;

				pOutData->pid = pid_val;

				if (icon.size() != 0)
				{
					auto icon_bin = NCrypto::base64Decode(icon);
					if (icon_bin.size() != 0)
					{
						memcpy(pOutData->iconData, icon_bin.data(), std::min(size_t(0x1002c), icon_bin.size()));
						pOutData->flags |= UploadedFavoriteToCommunityData::FLAG_HAS_ICON_DATA;
						pOutData->iconDataSize = icon_bin.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}
			}

			return OLV_RESULT_SUCCESS;
		}
	}
}