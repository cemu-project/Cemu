#include "Cafe/OS/libs/nn_olv/nn_olv_Common.h"
#include "nn_olv_PostTypes.h"
#include "nn_olv_OfflineDB.h"
#include "Cemu/ncrypto/ncrypto.h" // for base64 decoder
#include "util/helpers/helpers.h"
#include <pugixml.hpp>
#include <zlib.h>

namespace nn
{
	namespace olv
	{
		bool ParseXml_DownloadedDataBase(DownloadedDataBase& obj, pugi::xml_node& xmlNode)
		{
			// todo:
			// painting with url?

			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("body"); tokenNode)
			{
				obj.bodyTextLength = SetStringUC2(obj.bodyText, tokenNode.child_value(), true);
				if(obj.bodyTextLength > 0)
					obj.SetFlag(DownloadedDataBase::FLAGS::HAS_BODY_TEXT);
			}
			if(tokenNode = xmlNode.child("topic_tag"); tokenNode)
			{
				SetStringUC2(obj.topicTag, tokenNode.child_value(), true);
			}
			if(tokenNode = xmlNode.child("feeling_id"); tokenNode)
			{
				obj.feeling = ConvertString<sint8>(tokenNode.child_value());
				if(obj.feeling < 0 || obj.feeling >= 5)
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedDataBase::ParseXml: feeling_id out of range");
					return false;
				}
			}
			if(tokenNode = xmlNode.child("id"); tokenNode)
			{
				std::string_view id_sv = tokenNode.child_value();
				if(id_sv.size() > 22)
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedDataBase::ParseXml: id too long");
					return false;
				}
				memcpy(obj.postId, id_sv.data(), id_sv.size());
				obj.postId[id_sv.size()] = '\0';
			}
			if(tokenNode = xmlNode.child("is_autopost"); tokenNode)
			{
				uint8 isAutopost = ConvertString<sint8>(tokenNode.child_value());
				if(isAutopost == 1)
					obj.SetFlag(DownloadedDataBase::FLAGS::IS_AUTOPOST);
				else if(isAutopost == 0)
					obj.SetFlag(DownloadedDataBase::FLAGS::IS_NOT_AUTOPOST);
				else
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedDataBase::ParseXml: is_autopost has invalid value");
					return false;
				}
			}
			if(tokenNode = xmlNode.child("empathy_added"); tokenNode)
			{
				if(ConvertString<sint32>(tokenNode.child_value()) > 0)
					obj.SetFlag(DownloadedDataBase::FLAGS::HAS_EMPATHY_ADDED);
			}
			if(tokenNode = xmlNode.child("is_spoiler"); tokenNode)
			{
				if(ConvertString<sint32>(tokenNode.child_value()) > 0)
					obj.SetFlag(DownloadedDataBase::FLAGS::IS_SPOILER);
			}
			if(tokenNode = xmlNode.child("mii"); tokenNode)
			{
				std::vector<uint8> miiData = NCrypto::base64Decode(tokenNode.child_value());
				if(miiData.size() != 96)
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedSystemTopicData mii data is not valid (incorrect size)");
					return false;
				}
				memcpy(obj.miiData, miiData.data(), miiData.size());
				obj.SetFlag(DownloadedDataBase::FLAGS::HAS_MII_DATA);
			}
			if(tokenNode = xmlNode.child("pid"); tokenNode)
			{
				obj.userPid = ConvertString<uint32>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("screen_name"); tokenNode)
			{
				SetStringUC2(obj.miiNickname, tokenNode.child_value(), true);
			}
			if(tokenNode = xmlNode.child("region_id"); tokenNode)
			{
				obj.regionId = ConvertString<uint32>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("platform_id"); tokenNode)
			{
				obj.platformId = ConvertString<uint8>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("language_id"); tokenNode)
			{
				obj.languageId = ConvertString<uint8>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("country_id"); tokenNode)
			{
				obj.countryId = ConvertString<uint8>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("painting"); tokenNode)
			{
				if(pugi::xml_node subNode = tokenNode.child("content"); subNode)
				{
					std::vector<uint8> paintingData = NCrypto::base64Decode(subNode.child_value());
					if (paintingData.size() > 0xA000)
					{
						cemuLog_log(LogType::Force, "[Olive-XML] DownloadedDataBase painting content is too large");
						return false;
					}
					memcpy(obj.compressedMemoBody, paintingData.data(), paintingData.size());
					obj.SetFlag(DownloadedDataBase::FLAGS::HAS_BODY_MEMO);
				}
				if(pugi::xml_node subNode = tokenNode.child("size"); subNode)
				{
					obj.compressedMemoBodySize = ConvertString<uint32>(subNode.child_value());
				}
			}
			if(tokenNode = xmlNode.child("app_data"); tokenNode)
			{
				std::vector<uint8> appData = NCrypto::base64Decode(tokenNode.child_value());
				if (appData.size() > 0x400)
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedDataBase AppData is too large");
					return false;
				}
				memcpy(obj.appData, appData.data(), appData.size());
				obj.appDataLength = appData.size();
				obj.SetFlag(DownloadedDataBase::FLAGS::HAS_APP_DATA);
			}
			return true;
		}

		bool ParseXML_DownloadedPostData(DownloadedPostData& obj, pugi::xml_node& xmlNode)
		{
			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("community_id"); tokenNode)
				obj.communityId = ConvertString<uint32>(tokenNode.child_value());
			if(tokenNode = xmlNode.child("empathy_count"); tokenNode)
				obj.empathyCount = ConvertString<uint32>(tokenNode.child_value());
			if(tokenNode = xmlNode.child("reply_count"); tokenNode)
				obj.commentCount = ConvertString<uint32>(tokenNode.child_value());
			return ParseXml_DownloadedDataBase(obj.downloadedDataBase, xmlNode);
		}

		bool ParseXML_DownloadedSystemPostData(hidden::DownloadedSystemPostData& obj, pugi::xml_node& xmlNode)
		{
			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("title_id"); tokenNode)
				obj.titleId = ConvertString<uint64>(tokenNode.child_value());
			return ParseXML_DownloadedPostData(obj.downloadedPostData, xmlNode);
		}

		bool ParseXML_DownloadedTopicData(DownloadedTopicData& obj, pugi::xml_node& xmlNode)
		{
			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("community_id"); tokenNode)
				obj.communityId = ConvertString<uint32>(tokenNode.child_value());
			return true;
		}

		bool Parse_DownloadedSystemTopicData(hidden::DownloadedSystemTopicData& obj, pugi::xml_node& xmlNode)
		{
			if(!ParseXML_DownloadedTopicData(obj.downloadedTopicData, xmlNode))
				return false;
			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("name"); tokenNode)
			{
				SetStringUC2(obj.titleText, tokenNode.child_value(), true);
				obj.downloadedTopicData.SetFlag(DownloadedTopicData::FLAGS::HAS_TITLE);
			}
			if(tokenNode = xmlNode.child("is_recommended"); tokenNode)
			{
				uint32 isRecommended = ConvertString<uint32>(tokenNode.child_value());
				if(isRecommended != 0)
					obj.downloadedTopicData.SetFlag(DownloadedTopicData::FLAGS::IS_RECOMMENDED);
			}
			if(tokenNode = xmlNode.child("title_id"); tokenNode)
			{
				obj.titleId = ConvertString<uint64>(tokenNode.child_value());
			}
			if(tokenNode = xmlNode.child("title_ids"); tokenNode)
			{
				cemu_assert_unimplemented();
			}
			if(tokenNode = xmlNode.child("icon"); tokenNode)
			{
				std::vector<uint8> iconData = NCrypto::base64Decode(tokenNode.child_value());
				if(iconData.size() > sizeof(obj.iconData))
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedSystemTopicData icon data is not valid");
					return false;
				}
				obj.iconDataSize = iconData.size();
				memcpy(obj.iconData, iconData.data(), iconData.size());
				obj.downloadedTopicData.SetFlag(DownloadedTopicData::FLAGS::HAS_ICON_DATA);
			}
			return true;
		}

		uint32 GetSystemTopicDataListFromRawData(hidden::DownloadedSystemTopicDataList* downloadedSystemTopicDataList, hidden::DownloadedSystemPostData* downloadedSystemPostData, uint32be* postCountOut, uint32 postCountMax, void* xmlData, uint32 xmlDataSize)
		{
			// copy xmlData into a temporary buffer since load_buffer_inplace will modify it
			std::vector<uint8> buffer;
			buffer.resize(xmlDataSize);
			memcpy(buffer.data(), xmlData, xmlDataSize);
			pugi::xml_document doc;
			if (!doc.load_buffer_inplace(buffer.data(), xmlDataSize, pugi::parse_default, pugi::xml_encoding::encoding_utf8))
				return -1;

			memset(downloadedSystemTopicDataList, 0, sizeof(hidden::DownloadedSystemTopicDataList));
			downloadedSystemTopicDataList->topicDataNum = 0;
		
			cemu_assert_debug(doc.child("result").child("topics"));

			size_t postCount = 0;

			// parse topics
			for (pugi::xml_node topicsChildNode : doc.child("result").child("topics").children())
			{
				const char* name = topicsChildNode.name();
				cemuLog_logDebug(LogType::Force, "topicsChildNode.name() = {}", name);
				if (strcmp(topicsChildNode.name(), "topic"))
					continue;
				// parse topic
				if(downloadedSystemTopicDataList->topicDataNum > 10)
				{
					cemuLog_log(LogType::Force, "[Olive-XML] DownloadedSystemTopicDataList exceeded maximum topic count (10)");
					return false;
				}
				auto& topicEntry = downloadedSystemTopicDataList->topicData[downloadedSystemTopicDataList->topicDataNum];
				memset(&topicEntry, 0, sizeof(hidden::DownloadedSystemTopicDataList::DownloadedSystemTopicWrapped));
				Parse_DownloadedSystemTopicData(topicEntry.downloadedSystemTopicData, topicsChildNode);
				downloadedSystemTopicDataList->topicDataNum = downloadedSystemTopicDataList->topicDataNum + 1;

				topicEntry.postDataNum = 0;
				// parse all posts within the current topic
				for (pugi::xml_node personNode : topicsChildNode.child("people").children("person"))
				{
					for (pugi::xml_node postNode : personNode.child("posts").children("post"))
					{
						if(postCount >= postCountMax)
						{
							cemuLog_log(LogType::Force, "[Olive-XML] GetSystemTopicDataListFromRawData exceeded maximum post count");
							return false;
						}
						auto& postEntry = downloadedSystemPostData[postCount];
						memset(&postEntry, 0, sizeof(hidden::DownloadedSystemPostData));
						bool r = ParseXML_DownloadedSystemPostData(postEntry, postNode);
						if(!r)
						{
							cemuLog_log(LogType::Force, "[Olive-XML] DownloadedSystemPostData parsing failed");
							return false;
						}
						postCount++;
						// add post to topic
						if(topicEntry.postDataNum >= hidden::DownloadedSystemTopicDataList::MAX_POSTS_PER_TOPIC)
						{
							cemuLog_log(LogType::Force, "[Olive-XML] DownloadedSystemTopicDataList has too many posts for a single topic (up to {})", hidden::DownloadedSystemTopicDataList::MAX_POSTS_PER_TOPIC);
							return false;
						}
						topicEntry.postDataList[topicEntry.postDataNum] = &postEntry;
						topicEntry.postDataNum = topicEntry.postDataNum + 1;
					}
				}
			}
			*postCountOut = postCount;
			return 0;
		}

		nnResult DownloadedDataBase::DownloadExternalImageData(DownloadedDataBase* _this, void* imageDataOut, uint32be* imageSizeOut, uint32 maxSize)
		{
			if(g_IsOfflineDBMode)
				return OfflineDB_DownloadPostDataListParam_DownloadExternalImageData(_this, imageDataOut, imageSizeOut, maxSize);

			if(!g_IsOnlineMode)
				return OLV_RESULT_OFFLINE_MODE_REQUEST;
			if (!TestFlags(_this, FLAGS::HAS_EXTERNAL_IMAGE))
				return OLV_RESULT_MISSING_DATA;

			cemuLog_logDebug(LogType::Force, "DownloadedDataBase::DownloadExternalImageData not implemented");
			return OLV_RESULT_FAILED_REQUEST; // placeholder error
		}

		nnResult DownloadPostDataListParam::GetRawDataUrl(DownloadPostDataListParam* _this, char* urlOut, uint32 urlMaxSize)
		{
			if(!g_IsOnlineMode)
				return OLV_RESULT_OFFLINE_MODE_REQUEST;
			//if(_this->communityId == 0)
			//	cemuLog_log(LogType::Force, "DownloadPostDataListParam::GetRawDataUrl called with invalid communityId");

			// get base url
			std::string baseUrl;
			baseUrl.append(g_DiscoveryResults.apiEndpoint);
			//baseUrl.append(fmt::format("/v1/communities/{}/posts", (uint32)_this->communityId));
			cemu_assert_debug(_this->communityId == 0);
			baseUrl.append(fmt::format("/v1/posts.search", (uint32)_this->communityId));

			// "v1/posts.search"

			// build parameter string
			std::string params;

			// this function behaves differently for the Wii U menu? Where it can lookup posts by titleId?
			if(_this->titleId != 0)
			{
				cemu_assert_unimplemented(); // Wii U menu mode
			}

			// todo: Generic parameters. Which includes: language_id, limit, type=text/memo

			// handle postIds
			for(size_t i=0; i<_this->MAX_NUM_POST_ID; i++)
			{
				if(_this->searchPostId[i].str[0] == '\0')
					continue;
				cemu_assert_unimplemented(); // todo
				// todo - postId parameter
				// handle filters
				if(_this->_HasFlag(DownloadPostDataListParam::FLAGS::WITH_MII))
					params.append("&with_mii=1");
				if(_this->_HasFlag(DownloadPostDataListParam::FLAGS::WITH_EMPATHY))
					params.append("&with_empathy_added=1");
				if(_this->bodyTextMaxLength != 0)
					params.append(fmt::format("&max_body_length={}", _this->bodyTextMaxLength));
			}

			if(_this->titleId != 0)
				params.append(fmt::format("&title_id={}", (uint64)_this->titleId));

			if (_this->_HasFlag(DownloadPostDataListParam::FLAGS::FRIENDS_ONLY))
				params.append("&by=friend");
			if (_this->_HasFlag(DownloadPostDataListParam::FLAGS::FOLLOWERS_ONLY))
				params.append("&by=followings");
			if (_this->_HasFlag(DownloadPostDataListParam::FLAGS::SELF_ONLY))
				params.append("&by=self");

			if(!params.empty())
				params[0] = '?'; // replace the leading ampersand

			baseUrl.append(params);
			if(baseUrl.size()+1 > urlMaxSize)
				return OLV_RESULT_NOT_ENOUGH_SIZE;
			strncpy(urlOut, baseUrl.c_str(), urlMaxSize);
			return OLV_RESULT_SUCCESS;
		}

		nnResult DownloadPostDataList(DownloadedTopicData* downloadedTopicData, DownloadedPostData* downloadedPostData, uint32be* postCountOut, uint32 maxCount, DownloadPostDataListParam* param)
		{
			if(g_IsOfflineDBMode)
				return OfflineDB_DownloadPostDataListParam_DownloadPostDataList(downloadedTopicData, downloadedPostData, postCountOut, maxCount, param);
			memset(downloadedTopicData, 0, sizeof(DownloadedTopicData));
			downloadedTopicData->communityId = param->communityId;
			*postCountOut = 0;

			char urlBuffer[2048];
			if (NN_RESULT_IS_FAILURE(DownloadPostDataListParam::GetRawDataUrl(param, urlBuffer, sizeof(urlBuffer))))
				return OLV_RESULT_INVALID_PARAMETER;

			/*
			CurlRequestHelper req;
			req.initate(urlBuffer, CurlRequestHelper::SERVER_SSL_CONTEXT::OLIVE);
			InitializeOliveRequest(req);
			bool reqResult = req.submitRequest();
			if (!reqResult)
			{
				long httpCode = 0;
				curl_easy_getinfo(req.getCURL(), CURLINFO_RESPONSE_CODE, &httpCode);
				cemuLog_log(LogType::Force, "Failed request: {} ({})", urlBuffer, httpCode);
				if (!(httpCode >= 400))
					return OLV_RESULT_FAILED_REQUEST;
			}
			pugi::xml_document doc;
			if (!doc.load_buffer(req.getReceivedData().data(), req.getReceivedData().size()))
			{
				cemuLog_log(LogType::Force, fmt::format("Invalid XML in community download response"));
				return OLV_RESULT_INVALID_XML;
			}
			*/

			*postCountOut = 0;

			return OLV_RESULT_SUCCESS;
		}

		void loadOlivePostAndTopicTypes()
		{
			cafeExportRegisterFunc(GetSystemTopicDataListFromRawData, "nn_olv", "GetSystemTopicDataListFromRawData__Q3_2nn3olv6hiddenFPQ4_2nn3olv6hidden29DownloadedSystemTopicDataListPQ4_2nn3olv6hidden24DownloadedSystemPostDataPUiUiPCUcT4", LogType::None);

			// DownloadedDataBase getters
			cafeExportRegisterFunc(DownloadedDataBase::TestFlags, "nn_olv", "TestFlags__Q3_2nn3olv18DownloadedDataBaseCFUi", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetUserPid, "nn_olv", "GetUserPid__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetPostDate, "nn_olv", "GetPostDate__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetFeeling, "nn_olv", "GetFeeling__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetRegionId, "nn_olv", "GetRegionId__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetPlatformId, "nn_olv", "GetPlatformId__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetLanguageId, "nn_olv", "GetLanguageId__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetCountryId, "nn_olv", "GetCountryId__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetExternalUrl, "nn_olv", "GetExternalUrl__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetMiiData1, "nn_olv", "GetMiiData__Q3_2nn3olv18DownloadedDataBaseCFP12FFLStoreData", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetMiiNickname, "nn_olv", "GetMiiNickname__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetBodyText, "nn_olv", "GetBodyText__Q3_2nn3olv18DownloadedDataBaseCFPwUi", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetBodyMemo, "nn_olv", "GetBodyMemo__Q3_2nn3olv18DownloadedDataBaseCFPUcPUiUi", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetTopicTag, "nn_olv", "GetTopicTag__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetAppData, "nn_olv", "GetAppData__Q3_2nn3olv18DownloadedDataBaseCFPUcPUiUi", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetAppDataSize, "nn_olv", "GetAppDataSize__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetPostId, "nn_olv", "GetPostId__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetMiiData2, "nn_olv", "GetMiiData__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::DownloadExternalImageData, "nn_olv", "DownloadExternalImageData__Q3_2nn3olv18DownloadedDataBaseCFPvPUiUi", LogType::None);
			cafeExportRegisterFunc(DownloadedDataBase::GetExternalImageDataSize, "nn_olv", "GetExternalImageDataSize__Q3_2nn3olv18DownloadedDataBaseCFv", LogType::None);

			// DownloadedPostData getters
			cafeExportRegisterFunc(DownloadedPostData::GetCommunityId, "nn_olv", "GetCommunityId__Q3_2nn3olv18DownloadedPostDataCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedPostData::GetEmpathyCount, "nn_olv", "GetEmpathyCount__Q3_2nn3olv18DownloadedPostDataCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedPostData::GetCommentCount, "nn_olv", "GetCommentCount__Q3_2nn3olv18DownloadedPostDataCFv", LogType::None);
			cafeExportRegisterFunc(DownloadedPostData::GetPostId, "nn_olv", "GetPostId__Q3_2nn3olv18DownloadedPostDataCFv", LogType::None);

			// DownloadedSystemPostData getters
			cafeExportRegisterFunc(hidden::DownloadedSystemPostData::GetTitleId, "nn_olv", "GetTitleId__Q4_2nn3olv6hidden24DownloadedSystemPostDataCFv", LogType::None);

			// DownloadedTopicData getters
			cafeExportRegisterFunc(DownloadedTopicData::GetCommunityId, "nn_olv", "GetCommunityId__Q3_2nn3olv19DownloadedTopicDataCFv", LogType::None);

			// DownloadedSystemTopicData getters
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicData::TestFlags, "nn_olv", "TestFlags__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFUi", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicData::GetTitleId, "nn_olv", "GetTitleId__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFv", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicData::GetTitleIdNum, "nn_olv", "GetTitleIdNum__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFv", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicData::GetTitleText, "nn_olv", "GetTitleText__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFPwUi", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicData::GetTitleIconData, "nn_olv", "GetTitleIconData__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFPUcPUiUi", LogType::None);

			// DownloadedSystemTopicDataList getters
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicDataList::GetDownloadedSystemTopicDataNum, "nn_olv", "GetDownloadedSystemTopicDataNum__Q4_2nn3olv6hidden29DownloadedSystemTopicDataListCFv", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicDataList::GetDownloadedSystemPostDataNum, "nn_olv", "GetDownloadedSystemPostDataNum__Q4_2nn3olv6hidden29DownloadedSystemTopicDataListCFi", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicDataList::GetDownloadedSystemTopicData, "nn_olv", "GetDownloadedSystemTopicData__Q4_2nn3olv6hidden29DownloadedSystemTopicDataListCFi", LogType::None);
			cafeExportRegisterFunc(hidden::DownloadedSystemTopicDataList::GetDownloadedSystemPostData, "nn_olv", "GetDownloadedSystemPostData__Q4_2nn3olv6hidden29DownloadedSystemTopicDataListCFiT1", LogType::None);

			// DownloadPostDataListParam constructor and getters
			cafeExportRegisterFunc(DownloadPostDataListParam::Construct, "nn_olv", "__ct__Q3_2nn3olv25DownloadPostDataListParamFv", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetFlags, "nn_olv", "SetFlags__Q3_2nn3olv25DownloadPostDataListParamFUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetLanguageId, "nn_olv", "SetLanguageId__Q3_2nn3olv25DownloadPostDataListParamFUc", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetCommunityId, "nn_olv", "SetCommunityId__Q3_2nn3olv25DownloadPostDataListParamFUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetSearchKey, "nn_olv", "SetSearchKey__Q3_2nn3olv25DownloadPostDataListParamFPCwUc", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetSearchKeySingle, "nn_olv", "SetSearchKey__Q3_2nn3olv25DownloadPostDataListParamFPCw", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetSearchPid, "nn_olv", "SetSearchPid__Q3_2nn3olv25DownloadPostDataListParamFUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetPostId, "nn_olv", "SetPostId__Q3_2nn3olv25DownloadPostDataListParamFPCcUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetPostDate, "nn_olv", "SetPostDate__Q3_2nn3olv25DownloadPostDataListParamFL", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetPostDataMaxNum, "nn_olv", "SetPostDataMaxNum__Q3_2nn3olv25DownloadPostDataListParamFUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataListParam::SetBodyTextMaxLength, "nn_olv", "SetBodyTextMaxLength__Q3_2nn3olv25DownloadPostDataListParamFUi", LogType::None);

			// URL and downloading functions
			cafeExportRegisterFunc(DownloadPostDataListParam::GetRawDataUrl, "nn_olv", "GetRawDataUrl__Q3_2nn3olv25DownloadPostDataListParamCFPcUi", LogType::None);
			cafeExportRegisterFunc(DownloadPostDataList, "nn_olv", "DownloadPostDataList__Q2_2nn3olvFPQ3_2nn3olv19DownloadedTopicDataPQ3_2nn3olv18DownloadedPostDataPUiUiPCQ3_2nn3olv25DownloadPostDataListParam", LogType::None);

		}

	}
}