#include "nn_olv_UploadCommunityTypes.h"
#include <algorithm>

namespace nn
{
	namespace olv
	{

		sint32 UploadCommunityData_AsyncRequestImpl(CurlRequestHelper& req, const char* reqUrl,
			UploadedCommunityData* pOutData, UploadCommunityDataParam const* pParam);

		sint32 UploadCommunityData_AsyncRequest(CurlRequestHelper& req, const char* reqUrl, coreinit::OSEvent* requestDoneEvent,
			UploadedCommunityData* pOutData, UploadCommunityDataParam const* pParam
		)
		{
			sint32 res = UploadCommunityData_AsyncRequestImpl(req, reqUrl, pOutData, pParam);
			coreinit::OSSignalEvent(requestDoneEvent);
			return res;
		}

		sint32 UploadCommunityData(UploadedCommunityData* pOutData, UploadCommunityDataParam const* pParam)
		{
			if (!nn::olv::g_IsInitialized)
				return OLV_RESULT_NOT_INITIALIZED;
			
			if (!nn::olv::g_IsOnlineMode)
				return OLV_RESULT_OFFLINE_MODE_REQUEST;

			if (!pParam)
				return OLV_RESULT_INVALID_PTR;

			if (pOutData)
				UploadedCommunityData::Clean(pOutData);

			char requestUrl[512];
			if (pParam->flags & UploadCommunityDataParam::FLAG_DELETION)
			{
				if (!pParam->communityId)
					return OLV_RESULT_INVALID_PARAMETER;

				snprintf(requestUrl, sizeof(requestUrl), "%s/v1/communities/%lu.delete", g_DiscoveryResults.apiEndpoint, pParam->communityId.value());
			}
			else
			{
				if (pParam->communityId)
					snprintf(requestUrl, sizeof(requestUrl), "%s/v1/communities/%lu", g_DiscoveryResults.apiEndpoint, pParam->communityId.value());
				else
					snprintf(requestUrl, sizeof(requestUrl), "%s/v1/communities", g_DiscoveryResults.apiEndpoint);
			}

			
			CurlRequestHelper req;
			req.initate(ActiveSettings::GetNetworkService(), requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::OLIVE);
			InitializeOliveRequest(req);

			StackAllocator<coreinit::OSEvent> requestDoneEvent;
			coreinit::OSInitEvent(&requestDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			std::future<sint32> requestRes = std::async(std::launch::async, UploadCommunityData_AsyncRequest, std::ref(req), requestUrl, requestDoneEvent.GetPointer(), pOutData, pParam);
			coreinit::OSWaitEvent(&requestDoneEvent);

			return requestRes.get();
		}

		sint32 UploadCommunityData(UploadCommunityDataParam const* pParam)
		{
			return UploadCommunityData(nullptr, pParam);
		}

		sint32 UploadCommunityData_AsyncRequestImpl(CurlRequestHelper& req, const char* reqUrl,
			UploadedCommunityData* pOutData, UploadCommunityDataParam const* pParam)
		{
			sint32 res = OLV_RESULT_SUCCESS;

			std::string base64icon;
			std::string form_name;
			std::string form_desc;
			std::string form_searchKey[5];
			std::string encodedAppData;
			uint8* encodedIcon = nullptr;

			curl_mime *mime = curl_mime_init(req.getCURL());
			curl_mimepart *part;

			try
			{
				if (!pParam->iconData.IsNull())
				{
					encodedIcon = new uint8[pParam->iconDataLen];
					if (encodedIcon)
					{
						sint32 iconEncodeRes = EncodeTGA(pParam->iconData.GetPtr(), pParam->iconDataLen, encodedIcon, pParam->iconDataLen, TGACheckType::CHECK_COMMUNITY_ICON);
						if (iconEncodeRes <= 0)
						{
							delete[] encodedIcon;
							return OLV_RESULT_NOT_ENOUGH_SIZE; // ?
						}

						base64icon = NCrypto::base64Encode(encodedIcon, iconEncodeRes);
						part = curl_mime_addpart(mime);
						curl_mime_name(part, "icon");
						res = olv_curlformcode_to_error(curl_mime_data(part, base64icon.data(), base64icon.size()));

						if (res < 0)
							throw std::runtime_error("curl_formadd() error! - icon");
					}
				}

				if (pParam->titleText[0])
				{
					form_name = StringHelpers::ToUtf8((const uint16be*)pParam->titleText, 127);
					part = curl_mime_addpart(mime);
					curl_mime_name(part, "name");
					res = olv_curlformcode_to_error(curl_mime_data(part, form_name.data(), form_name.size()));

					if (res < 0)
						throw std::runtime_error("curl_formadd() error! - name");
				}

				if (pParam->description[0])
				{
					form_desc = StringHelpers::ToUtf8((const uint16be*)pParam->description, 255);
					part = curl_mime_addpart(mime);
					curl_mime_name(part, "description");
					res = olv_curlformcode_to_error(curl_mime_data(part, form_desc.data(), form_desc.size()));


					if (res < 0)
						throw std::runtime_error("curl_formadd() error! - description");
				}

				for (int i = 0; i < 5; i++)
				{
					if (pParam->searchKeys[i][0])
					{
						form_searchKey[i] = StringHelpers::ToUtf8((const uint16be*)pParam->searchKeys[i], 151);
						part = curl_mime_addpart(mime);
						curl_mime_name(part, "search_key");
						res = olv_curlformcode_to_error(curl_mime_data(part, form_searchKey[i].data(), form_searchKey[i].size()));

						if (res < 0)
							throw std::runtime_error("curl_formadd() error! - search_key");
					}
				}

				if (!pParam->appData.IsNull())
				{
					encodedAppData = NCrypto::base64Encode(pParam->appData.GetPtr(), pParam->appDataLen);
					if (encodedAppData.size() < pParam->appDataLen)
						res = OLV_RESULT_FATAL(101);
					else
					{
						part = curl_mime_addpart(mime);
						curl_mime_name(part, "app_data");
						res = olv_curlformcode_to_error(curl_mime_data(part, encodedAppData.data(), encodedAppData.size()));

						if (res < 0)
							throw std::runtime_error("curl_formadd() error! - app_data");
					}
				}
			}
			catch (const std::runtime_error& error)
			{
				cemuLog_log(LogType::Force, "Error in multipart curl -> {}", error.what());
				curl_mime_free(mime);

				if (encodedIcon)
					delete[] encodedIcon;

				return res;
			}

			curl_easy_setopt(req.getCURL(), CURLOPT_MIMEPOST, mime);
			req.setUseMultipartFormData(true);

			bool reqResult = req.submitRequest(true);
			long httpCode = 0;
			curl_easy_getinfo(req.getCURL(), CURLINFO_RESPONSE_CODE, &httpCode);

			if (encodedIcon)
				delete[] encodedIcon;

			if (!reqResult)
			{
				cemuLog_log(LogType::Force, "Failed request: {} ({})", reqUrl, httpCode);
				if (!(httpCode >= 400))
					return OLV_RESULT_FAILED_REQUEST;
			}

			pugi::xml_document doc;
			if (!doc.load_buffer(req.getReceivedData().data(), req.getReceivedData().size()))
			{
				cemuLog_log(LogType::Force, fmt::format("Invalid XML in community upload response"));
				return OLV_RESULT_INVALID_XML;
			}

			sint32 responseError = CheckOliveResponse(doc);
			if (responseError < 0)
				return responseError;

			if (httpCode != 200)
				return OLV_RESULT_STATUS(httpCode + 4000);
			
			if (pOutData)
			{

				std::string_view app_data = doc.select_node("//app_data").node().child_value();
				std::string_view community_id = doc.select_node("//community_id").node().child_value();
				std::string_view name = doc.select_node("//name").node().child_value();
				std::string_view description = doc.select_node("//description").node().child_value();
				std::string_view pid = doc.select_node("//pid").node().child_value();
				std::string_view icon = doc.select_node("//icon").node().child_value();

				if (app_data.size() != 0)
				{
					auto app_data_bin = NCrypto::base64Decode(app_data);
					if (app_data_bin.size() != 0) {
						memcpy(pOutData->appData, app_data_bin.data(), std::min(size_t(0x400), app_data_bin.size()));
						pOutData->flags |= UploadedCommunityData::FLAG_HAS_APP_DATA;
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
					auto name_utf16 = StringHelpers::FromUtf8(name);
					name_utf16.resize(std::min<size_t>(name_utf16.size(), 128));
					if (name_utf16.size() != 0)
					{
						for (int i = 0; i < name_utf16.size(); i++)
							pOutData->titleText[i] = name_utf16.at(i);

						pOutData->flags |= UploadedCommunityData::FLAG_HAS_TITLE_TEXT;
						pOutData->titleTextMaxLen = name_utf16.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				if (description.size() != 0)
				{
					auto description_utf16 = StringHelpers::FromUtf8(description);
					description_utf16.resize(std::min<size_t>(description_utf16.size(), 256));
					if (description_utf16.size() != 0)
					{
						for (int i = 0; i < description_utf16.size(); i++)
							pOutData->description[i] = description_utf16.at(i);

						pOutData->flags |= UploadedCommunityData::FLAG_HAS_DESC_TEXT;
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
						pOutData->flags |= UploadedCommunityData::FLAG_HAS_ICON_DATA;
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