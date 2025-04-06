#include "nn_olv_DownloadCommunityTypes.h"

namespace nn
{
	namespace olv
	{

		sint32 DownloadCommunityDataList_AsyncRequestImpl(
			CurlRequestHelper& req, const char* reqUrl,
			DownloadedCommunityData* pOutList, uint32* pOutNum, uint32 numMaxList, const DownloadCommunityDataListParam* pParam);


		sint32 DownloadCommunityDataList_AsyncRequest(
			CurlRequestHelper& req, const char* reqUrl, coreinit::OSEvent* requestDoneEvent,
			DownloadedCommunityData* pOutList, uint32* pOutNum, uint32 numMaxList, const DownloadCommunityDataListParam* pParam
		)
		{
			sint32 res = DownloadCommunityDataList_AsyncRequestImpl(req, reqUrl, pOutList, pOutNum, numMaxList, pParam);
			coreinit::OSSignalEvent(requestDoneEvent);
			return res;
		}

		sint32 DownloadCommunityDataList(DownloadedCommunityData* pOutList, uint32* pOutNum, uint32 numMaxList, const DownloadCommunityDataListParam* pParam)
		{
			if (!g_IsInitialized)
				return OLV_RESULT_NOT_INITIALIZED;

			if (!g_IsOnlineMode)
				return OLV_RESULT_OFFLINE_MODE_REQUEST;

			if (!pOutList || !pOutNum || !pParam)
				return OLV_RESULT_INVALID_PTR;

			if (!numMaxList)
				return OLV_RESULT_NOT_ENOUGH_SIZE;

			for (int i = 0; i < numMaxList; i++)
				DownloadedCommunityData::Clean(&pOutList[i]);

			char reqUrl[2048];
			sint32 res = pParam->GetRawDataUrl(reqUrl, sizeof(reqUrl));
			if (res < 0)
				return res;

			CurlRequestHelper req;
			req.initate(ActiveSettings::GetNetworkService(), reqUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::OLIVE);
			InitializeOliveRequest(req);

			StackAllocator<coreinit::OSEvent> requestDoneEvent;
			coreinit::OSInitEvent(&requestDoneEvent, coreinit::OSEvent::EVENT_STATE::STATE_NOT_SIGNALED, coreinit::OSEvent::EVENT_MODE::MODE_MANUAL);
			std::future<sint32> requestRes = std::async(std::launch::async, DownloadCommunityDataList_AsyncRequest,
				std::ref(req), reqUrl, requestDoneEvent.GetPointer(), pOutList, pOutNum, numMaxList, pParam);
			coreinit::OSWaitEvent(&requestDoneEvent);

			return requestRes.get();	
		}

		sint32 DownloadCommunityDataList_AsyncRequestImpl(
			CurlRequestHelper& req, const char* reqUrl,
			DownloadedCommunityData* pOutList, uint32* pOutNum, uint32 numMaxList, const DownloadCommunityDataListParam* pParam
		)
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
				cemuLog_log(LogType::Force, fmt::format("Invalid XML in community download response"));
				return OLV_RESULT_INVALID_XML;
			}

			sint32 responseError = CheckOliveResponse(doc);
			if (responseError < 0)
				return responseError;

			if (httpCode != 200)
				return OLV_RESULT_STATUS(httpCode + 4000);

			std::string request_name = doc.select_single_node("//request_name").node().child_value();
			if (request_name.size() == 0)
			{
				cemuLog_log(LogType::Force, "Community download response doesn't contain <request_name>");
				return OLV_RESULT_INVALID_XML;
			}

			if ((request_name.compare("communities") != 0) && (request_name.compare("specified_communities") != 0))
			{
				cemuLog_log(LogType::Force, "Community download response <request_name> isn't \"communities\" or \"specified_communities\"");
				return OLV_RESULT_INVALID_XML;
			}

			pugi::xml_node communities = doc.select_single_node("//communities").node();
			if (!communities)
			{
				cemuLog_log(LogType::Force, "Community download response doesn't contain <communities>");
				return OLV_RESULT_INVALID_XML;
			}

			int idx = 0;
			for (pugi::xml_node communityNode : communities.children("community"))
			{
				if (idx >= numMaxList)
					break;

				DownloadedCommunityData* pOutData = &pOutList[idx];

				std::string_view app_data = communityNode.child_value("app_data");
				std::string_view community_id = communityNode.child_value("community_id");
				std::string_view name = communityNode.child_value("name");
				std::string_view description = communityNode.child_value("description");
				std::string_view pid = communityNode.child_value("pid");
				std::string_view icon = communityNode.child_value("icon");
				std::string_view mii = communityNode.child_value("mii");
				std::string_view screen_name = communityNode.child_value("screen_name");

				if (app_data.size() != 0)
				{
					auto app_data_bin = NCrypto::base64Decode(app_data);
					if (app_data_bin.size() != 0)
					{
						memcpy(pOutData->appData, app_data_bin.data(), std::min(size_t(0x400), app_data_bin.size()));
						pOutData->flags |= DownloadedCommunityData::FLAG_HAS_APP_DATA;
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
							pOutData->titleText[i] = name_utf16.at(i).bevalue();

						pOutData->flags |= DownloadedCommunityData::FLAG_HAS_TITLE_TEXT;
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
							pOutData->description[i] = description_utf16.at(i).bevalue();

						pOutData->flags |= DownloadedCommunityData::FLAG_HAS_DESC_TEXT;
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
						pOutData->flags |= DownloadedCommunityData::FLAG_HAS_ICON_DATA;
						pOutData->iconDataSize = icon_bin.size();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				if (mii.size() != 0)
				{
					auto mii_bin = NCrypto::base64Decode(mii);
					if (mii_bin.size() != 0)
					{
						memcpy(pOutData->miiFFLStoreData, mii_bin.data(), std::min(size_t(96), mii_bin.size()));
						pOutData->flags |= DownloadedCommunityData::FLAG_HAS_MII_DATA;
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				if (screen_name.size() != 0)
				{
					auto screen_name_utf16 = StringHelpers::FromUtf8(screen_name).substr(0, 32);
					if (screen_name_utf16.size() != 0)
					{
						for (int i = 0; i < screen_name_utf16.size(); i++)
							pOutData->miiDisplayName[i] = screen_name_utf16.at(i).bevalue();
					}
					else
						return OLV_RESULT_INVALID_TEXT_FIELD;
				}

				idx++;

			}

			*pOutNum = _swapEndianU32(idx);
			sint32 res = OLV_RESULT_SUCCESS;
			if (idx > 0)
				res = 0; // nn_olv doesn't do it like that, but it's the same effect. I have no clue why it returns 0 when you have 1+ communities downloaded

			return res;
		}
	}
}