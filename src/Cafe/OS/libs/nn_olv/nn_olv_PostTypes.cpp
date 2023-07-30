#include "Cafe/OS/libs/nn_olv/nn_olv_Common.h"
#include "nn_olv_PostTypes.h"
#include "Cemu/ncrypto/ncrypto.h" // for base64 decoder
#include "util/helpers/helpers.h"
#include <pugixml.hpp>
#include <zlib.h>

namespace nn
{
	namespace olv
	{

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

		bool ParseXml_DownloadedDataBase(DownloadedDataBase& obj, pugi::xml_node& xmlNode)
		{
			// todo:
			// app_data, body, painting, name

			pugi::xml_node tokenNode;
			if(tokenNode = xmlNode.child("body"); tokenNode)
			{
				//cemu_assert_unimplemented();
				obj.bodyTextLength = SetStringUC2(obj.bodyText, tokenNode.child_value(), true);
				if(obj.bodyTextLength > 0)
					obj.SetFlag(DownloadedDataBase::FLAGS::HAS_BODY_TEXT);
			}
			if(tokenNode = xmlNode.child("feeling_id"); tokenNode)
			{
				obj.feeling = ConvertString<sint8>(tokenNode.child_value());
				if(obj.feeling < 0 || obj.feeling >= 5)
				{
					cemuLog_log(LogType::Force, "DownloadedDataBase::ParseXml: feeling_id out of range");
					return false;
				}
			}
			if(tokenNode = xmlNode.child("id"); tokenNode)
			{
				std::string_view id_sv = tokenNode.child_value();
				if(id_sv.size() > 22)
				{
					cemuLog_log(LogType::Force, "DownloadedDataBase::ParseXml: id too long");
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
					cemuLog_log(LogType::Force, "DownloadedDataBase::ParseXml: is_autopost has invalid value");
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

		}

	}
}