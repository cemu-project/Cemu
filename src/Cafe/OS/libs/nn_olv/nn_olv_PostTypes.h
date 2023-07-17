#pragma once
#include <zlib.h>

namespace nn
{
	namespace olv
	{
		struct DownloadedDataBase
		{
			enum class FLAGS : uint32
			{
				HAS_BODY_TEXT = 0x01,
				HAS_BODY_MEMO = 0x02,
				HAS_EXTERNAL_IMAGE = 0x04,
				HAS_EXTERNAL_BINARY_DATA = 0x08,
				HAS_MII_DATA = 0x10,
				HAS_EXTERNAL_URL = 0x20,
				HAS_APP_DATA = 0x40,
				HAS_EMPATHY_ADDED = 0x80,
				IS_AUTOPOST = 0x100,
				IS_SPOILER = 0x200,
				IS_NOT_AUTOPOST = 0x400, // autopost flag was explicitly set to false
			};

			void Reset()
			{
				memset(this, 0, sizeof(DownloadedDataBase));
			}

			void SetFlag(FLAGS flag)
			{
				flags =  (FLAGS)((uint32)flags.value() | (uint32)flag);
			}

			betype<FLAGS> flags;
			uint32be userPid;
			uint8be postId[32]; // string, up to 22 characters but the buffer is 32 bytes
			uint64be postDate;
			sint8be feeling;
			uint8be _padding0031[3];
			uint32be regionId;
			uint8be platformId;
			uint8be languageId;
			uint8be countryId;
			uint8be _padding003B;
			uint16be bodyText[256];
			uint32be bodyTextLength;
			uint8be compressedMemoBody[40960];
			uint32be compressedMemoBodySize;
			uint16be topicTag[152];
			uint8be appData[1024];
			uint32be appDataLength;
			uint8be externalBinaryUrl[256];
			uint32be externalBinaryDataSize;
			uint8be externalImageDataUrl[256];
			uint32be externalImageDataSize;
			uint8be externalURL[256];
			uint8be miiData[96];
			uint16be miiNickname[16];
			uint32be _paddingAB00[1344];
			uint32be uknC000_someVTableMaybe;
			uint32be uknC004;

			// getters
			// TestFlags__Q3_2nn3olv18DownloadedDataBaseCFUi
			static bool TestFlags(DownloadedDataBase* _this, DownloadedDataBase::FLAGS flag)
			{
				return HAS_FLAG((uint32)_this->flags.value(), (uint32)flag);
			}

			// GetUserPid__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetUserPid(DownloadedDataBase* _this)
			{
				return _this->userPid;
			}

			// GetPostDate__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint64 GetPostDate(DownloadedDataBase* _this)
			{
				return _this->postDate;
			}

			// GetFeeling__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetFeeling(DownloadedDataBase* _this)
			{
				if(_this->feeling >= 6)
					return 0;
				return _this->feeling;
			}

			// GetRegionId__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetRegionId(DownloadedDataBase* _this)
			{
				return _this->regionId;
			}

			// GetPlatformId__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetPlatformId(DownloadedDataBase* _this)
			{
				return _this->platformId;
			}

			// GetLanguageId__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetLanguageId(DownloadedDataBase* _this)
			{
				return _this->languageId;
			}

			// GetCountryId__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetCountryId(DownloadedDataBase* _this)
			{
				return _this->countryId;
			}

			// GetExternalUrl__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint8be* GetExternalUrl(DownloadedDataBase* _this)
			{
				if (!TestFlags(_this, FLAGS::HAS_EXTERNAL_URL))
					return nullptr;
				return _this->externalURL;
			}

			// GetMiiData__Q3_2nn3olv18DownloadedDataBaseCFP12FFLStoreData
			static nnResult GetMiiData1(DownloadedDataBase* _this, void* miiDataOut)
			{
				if (!TestFlags(_this, FLAGS::HAS_MII_DATA))
					return OLV_RESULT_MISSING_DATA;
				if (!miiDataOut)
					return OLV_RESULT_INVALID_PTR;
				memcpy(miiDataOut, _this->miiData, 96);
				return OLV_RESULT_SUCCESS;
			}

			// GetMiiData__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint8be* GetMiiData2(DownloadedDataBase* _this)
			{
				if (!TestFlags(_this, FLAGS::HAS_MII_DATA))
					return nullptr;
				return _this->miiData;
			}

			// GetMiiNickname__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint16be* GetMiiNickname(DownloadedDataBase* _this)
			{
				if (_this->miiNickname[0] == 0)
					return nullptr;
				return _this->miiNickname;
			}

			// GetBodyText__Q3_2nn3olv18DownloadedDataBaseCFPwUi
			static nnResult GetBodyText(DownloadedDataBase* _this, uint16be* bodyTextOut, uint32 maxLength)
			{
				if (!bodyTextOut)
					return OLV_RESULT_INVALID_PTR;
				if (maxLength == 0)
					return OLV_RESULT_NOT_ENOUGH_SIZE;
				uint32 outputLength = std::min<uint32>(_this->bodyTextLength, maxLength);
				olv_wstrncpy((char16_t*)bodyTextOut, (char16_t*)_this->bodyText, _this->bodyTextLength);
				return OLV_RESULT_SUCCESS;
			}

			// GetBodyMemo__Q3_2nn3olv18DownloadedDataBaseCFPUcPUiUi
			static nnResult GetBodyMemo(DownloadedDataBase* _this, uint8be* bodyMemoOut, uint32* bodyMemoSizeOut, uint32 maxSize)
			{
				if (!bodyMemoOut)
					return OLV_RESULT_INVALID_PTR;
				if (maxSize < 0x2582C)
					return OLV_RESULT_NOT_ENOUGH_SIZE;
				if (!TestFlags(_this, FLAGS::HAS_BODY_MEMO))
					return OLV_RESULT_MISSING_DATA;
				// uncompress TGA
				uLongf decompressedSize = maxSize;
				if (uncompress((uint8*)bodyMemoOut, &decompressedSize, (uint8*)_this->compressedMemoBody, _this->compressedMemoBodySize) != Z_OK)
				{
					cemuLog_log(LogType::Force, "DownloadedSystemTopicData::GetTitleIconData: uncompress failed");
					return OLV_RESULT_INVALID_TEXT_FIELD; // status
				}
				if(bodyMemoSizeOut)
					*bodyMemoSizeOut = decompressedSize;
				// todo - verify TGA header
				return OLV_RESULT_SUCCESS;
			}

			// GetTopicTag__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint16be* GetTopicTag(DownloadedDataBase* _this)
			{
				return _this->topicTag;
			}

			// GetAppData__Q3_2nn3olv18DownloadedDataBaseCFPUcPUiUi
			static nnResult GetAppData(DownloadedDataBase* _this, uint8be* appDataOut, uint32* appDataSizeOut, uint32 maxSize)
			{
				if (!appDataOut)
					return OLV_RESULT_INVALID_PTR;
				if (!TestFlags(_this, FLAGS::HAS_APP_DATA))
					return OLV_RESULT_MISSING_DATA;
				uint32 outputSize = std::min<uint32>(maxSize, _this->appDataLength);
				memcpy(appDataOut, _this->appData, outputSize);
				if(appDataSizeOut)
					*appDataSizeOut = outputSize;
				return OLV_RESULT_SUCCESS;
			}

			// GetAppDataSize__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint32 GetAppDataSize(DownloadedDataBase* _this)
			{
				return _this->appDataLength;
			}

			// GetPostId__Q3_2nn3olv18DownloadedDataBaseCFv
			static uint8be* GetPostId(DownloadedDataBase* _this)
			{
				return _this->postId;
			}

			// todo:
			// DownloadExternalImageData__Q3_2nn3olv18DownloadedDataBaseCFPvPUiUi
			// GetExternalImageDataSize__Q3_2nn3olv18DownloadedDataBaseCFv
			// DownloadExternalBinaryData__Q3_2nn3olv18DownloadedDataBaseCFPvPUiUi
			// GetExternalBinaryDataSize__Q3_2nn3olv18DownloadedDataBaseCFv
		};
		ENABLE_BITMASK_OPERATORS(DownloadedDataBase::FLAGS);

		static_assert(sizeof(DownloadedDataBase) == 0xC008);

		struct DownloadedPostData
		{
			DownloadedDataBase downloadedDataBase;
			uint32be communityId;
			uint32be empathyCount;
			uint32be commentCount;
			uint32be paddingC014[125]; // probably unused?

			// getters
			// GetCommunityId__Q3_2nn3olv18DownloadedPostDataCFv
			static uint32 GetCommunityId(DownloadedPostData* _this)
			{
				return _this->communityId;
			}

			// GetEmpathyCount__Q3_2nn3olv18DownloadedPostDataCFv
			static uint32 GetEmpathyCount(DownloadedPostData* _this)
			{
				return _this->empathyCount;
			}

			// GetCommentCount__Q3_2nn3olv18DownloadedPostDataCFv
			static uint32 GetCommentCount(DownloadedPostData* _this)
			{
				return _this->commentCount;
			}

			// GetPostId__Q3_2nn3olv18DownloadedPostDataCFv
			static uint8be* GetPostId(DownloadedPostData* _this)
			{
				return _this->downloadedDataBase.postId;
			}

		};

		static_assert(sizeof(DownloadedPostData) == 0xC208);

		struct DownloadedTopicData
		{
			enum class FLAGS
			{
				IS_RECOMMENDED = 0x01,
				HAS_TITLE = 0x02,
				HAS_ICON_DATA = 0x04,
			};
			betype<FLAGS> flags;
			uint32be communityId;
			int ukn[1022];

			void SetFlag(FLAGS flag)
			{
				flags = (FLAGS)((uint32)flags.value() | (uint32)flag);
			}

			// GetCommunityId__Q3_2nn3olv19DownloadedTopicDataCFv
			static uint32 GetCommunityId(DownloadedTopicData* _this)
			{
				return _this->communityId;
			}
		};
		ENABLE_BITMASK_OPERATORS(DownloadedTopicData::FLAGS);

		static_assert(sizeof(DownloadedTopicData) == 0x1000);

		namespace hidden
		{
			struct DownloadedSystemPostData
			{
				DownloadedPostData downloadedPostData;
				uint64be titleId;
				uint32be uknC210[124];
				uint32be uknC400;
				uint32be uknC404;

				// getters
				// GetTitleId__Q4_2nn3olv6hidden24DownloadedSystemPostDataCFv
				static uint64 GetTitleId(DownloadedSystemPostData* _this)
				{
					return _this->titleId;
				}
			};

			static_assert(sizeof(DownloadedSystemPostData) == 0xC408);

			struct DownloadedSystemTopicData
			{
				DownloadedTopicData downloadedTopicData;
				uint64be titleId;
				uint16be titleText[128];
				uint8be ukn1108[256];
				uint8be iconData[0x1002C];
				uint32be iconDataSize;
				uint64be titleIds[32];
				uint32be titleIdsCount;
				uint32be ukn1133C[1841];

				// implement getters as static methods for compatibility with CafeExportRegisterFunc()
				// TestFlags__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFUi
				static bool TestFlags(DownloadedSystemTopicData* _this, DownloadedTopicData::FLAGS flag)
				{
					return HAS_FLAG((uint32)_this->downloadedTopicData.flags.value(), (uint32)flag);
				}

				// GetTitleId__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFv
				static uint64 GetTitleId(DownloadedSystemTopicData* _this)
				{
					return _this->titleId;
				}

				// GetTitleIdNum__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFv
				static uint32 GetTitleIdNum(DownloadedSystemTopicData* _this)
				{
					return _this->titleIdsCount;
				}

				// GetTitleText__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFPwUi
				static nnResult GetTitleText(DownloadedSystemTopicData* _this, uint16be* titleTextOut, uint32 maxLength)
				{
					if (!TestFlags(_this, DownloadedTopicData::FLAGS::HAS_TITLE))
						return OLV_RESULT_MISSING_DATA;
					if (!titleTextOut)
						return OLV_RESULT_INVALID_PTR;
					memset(titleTextOut, 0, maxLength * sizeof(uint16be));
					if (maxLength > 128)
						maxLength = 128;
					olv_wstrncpy((char16_t*)titleTextOut, (char16_t*)_this->titleText, maxLength);
					return OLV_RESULT_SUCCESS;
				}

				// GetTitleIconData__Q4_2nn3olv6hidden25DownloadedSystemTopicDataCFPUcPUiUi
				static nnResult GetTitleIconData(DownloadedSystemTopicData* _this, void* iconDataOut, uint32be* iconSizeOut, uint32 iconDataMaxSize)
				{
					if (!TestFlags(_this, DownloadedTopicData::FLAGS::HAS_ICON_DATA))
						return OLV_RESULT_MISSING_DATA;
					if (!iconDataOut)
						return OLV_RESULT_INVALID_PTR;
					if (iconDataMaxSize < 0x1002C)
						return OLV_RESULT_NOT_ENOUGH_SIZE;
					uLongf decompressedSize = iconDataMaxSize;
					if (uncompress((uint8*)iconDataOut, &decompressedSize, (uint8*)_this->iconData, _this->iconDataSize) != Z_OK)
					{
						cemuLog_log(LogType::Force, "DownloadedSystemTopicData::GetTitleIconData: uncompress failed");
						return OLV_RESULT_INVALID_TEXT_FIELD; // status
					}
					*iconSizeOut = decompressedSize;
					// todo - check for TGA
					return OLV_RESULT_SUCCESS;
				}
			};

			static_assert(sizeof(DownloadedSystemTopicData) == 0x13000);

			struct DownloadedSystemTopicDataList
			{
				static constexpr size_t MAX_TOPIC_COUNT = 10;
				static constexpr size_t MAX_POSTS_PER_TOPIC = 300;

				// 0x134B8 sized wrapper of DownloadedSystemTopicData
				struct DownloadedSystemTopicWrapped
				{
					DownloadedSystemTopicData downloadedSystemTopicData;
					uint32be postDataNum;
					MEMPTR<DownloadedSystemPostData> postDataList[MAX_POSTS_PER_TOPIC];
					uint32 uknPadding;
				};
				static_assert(offsetof(DownloadedSystemTopicWrapped, postDataNum) == 0x13000);
				static_assert(sizeof(DownloadedSystemTopicWrapped) == 0x134B8);

				static uint32 GetDownloadedSystemTopicDataNum(DownloadedSystemTopicDataList* _this)
				{
					return _this->topicDataNum;
				};

				static uint32 GetDownloadedSystemPostDataNum(DownloadedSystemTopicDataList* _this, uint32 topicIndex)
				{
					if(topicIndex >= MAX_TOPIC_COUNT)
						return 0;
					return _this->topicData[topicIndex].postDataNum;
				};

				static DownloadedSystemTopicData* GetDownloadedSystemTopicData(DownloadedSystemTopicDataList* _this, uint32 topicIndex)
				{
					if(topicIndex >= MAX_TOPIC_COUNT)
						return nullptr;
					return &_this->topicData[topicIndex].downloadedSystemTopicData;
				};

				static DownloadedSystemPostData* GetDownloadedSystemPostData(DownloadedSystemTopicDataList* _this, sint32 topicIndex, sint32 postIndex)
				{
					if (topicIndex >= MAX_TOPIC_COUNT || postIndex >= MAX_POSTS_PER_TOPIC)
						return nullptr;
					return _this->topicData[topicIndex].postDataList[postIndex];
				}

				// member variables
				uint32be topicDataNum;
				uint32be ukn4;
				DownloadedSystemTopicWrapped topicData[MAX_TOPIC_COUNT];
				uint32be uknC0F38[50];
			};

			static_assert(sizeof(DownloadedSystemTopicDataList) == 0xC1000);
		}

		void loadOlivePostAndTopicTypes();
	}
}