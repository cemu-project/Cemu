#pragma once

#include "Cemu/ncrypto/ncrypto.h"
#include "config/ActiveSettings.h"

#include "Cafe/OS/libs/nn_olv/nn_olv_Common.h"

namespace nn
{
	namespace olv
	{

		class DownloadedCommunityData
		{
		public:
			static const inline uint32 FLAG_HAS_TITLE_TEXT = (1 << 0);
			static const inline uint32 FLAG_HAS_DESC_TEXT = (1 << 1);
			static const inline uint32 FLAG_HAS_APP_DATA = (1 << 2);
			static const inline uint32 FLAG_HAS_ICON_DATA = (1 << 3);
			static const inline uint32 FLAG_HAS_MII_DATA = (1 << 4);

			DownloadedCommunityData()
			{
				this->titleTextMaxLen = 0;
				this->appDataLen = 0;
				this->descriptionMaxLen = 0;
				this->pid = 0;
				this->communityId = 0;
				this->flags = 0;
				this->iconDataSize = 0;
				this->miiDisplayName[0] = 0;
			}
			static DownloadedCommunityData* __ctor(DownloadedCommunityData* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				}
				else
					return new (_this) DownloadedCommunityData();
			}

			static DownloadedCommunityData* Clean(DownloadedCommunityData* data)
			{
				data->flags = 0;
				data->communityId = 0;
				data->pid = 0;
				data->iconData[0] = 0;
				data->titleTextMaxLen = 0;
				data->appData[0] = 0;
				data->appDataLen = 0;
				data->description[0] = 0;
				data->descriptionMaxLen = 0;
				data->iconDataSize = 0;
				data->titleText[0] = 0;
				data->miiDisplayName[0] = 0;
				return data;
			}

			bool TestFlags(uint32 flags) const
			{
				return (this->flags & flags) != 0;
			}
			static bool __TestFlags(DownloadedCommunityData* _this, uint32 flags)
			{
				return _this->TestFlags(flags);
			}

			uint32 GetCommunityId() const
			{
				return this->communityId;
			}
			static uint32 __GetCommunityId(DownloadedCommunityData* _this)
			{
				return _this->GetCommunityId();
			}

			sint32 GetCommunityCode(char* pBuffer, uint32 bufferSize) const
			{
				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (bufferSize <= 12)
					return OLV_RESULT_NOT_ENOUGH_SIZE;

				uint32 len = 0;
				if (FormatCommunityCode(pBuffer, &len, this->communityId))
					return OLV_RESULT_SUCCESS;

				return OLV_RESULT_INVALID_PARAMETER;
			}
			static sint32 __GetCommunityCode(DownloadedCommunityData* _this, char* pBuffer, uint32 bufferSize)
			{
				return _this->GetCommunityCode(pBuffer, bufferSize);
			}

			uint32 GetOwnerPid() const
			{
				return this->pid;
			}
			static uint32 __GetOwnerPid(DownloadedCommunityData* _this)
			{
				return _this->GetOwnerPid();
			}

			sint32 GetTitleText(char16_t* pBuffer, uint32 numChars)
			{
				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (numChars)
				{
					if (!this->TestFlags(FLAG_HAS_TITLE_TEXT))
						return OLV_RESULT_MISSING_DATA;

					memset(pBuffer, 0, 2 * numChars);
					uint32 readSize = this->titleTextMaxLen;
					if (numChars < readSize)
						readSize = numChars;

					olv_wstrncpy(pBuffer, this->titleText, readSize);
					return OLV_RESULT_SUCCESS;
				}

				return OLV_RESULT_NOT_ENOUGH_SIZE;
			}
			static sint32 __GetTitleText(DownloadedCommunityData* _this, char16_t* pBuffer, uint32 numChars)
			{
				return _this->GetTitleText(pBuffer, numChars);
			}

			sint32 GetDescriptionText(char16_t* pBuffer, uint32 numChars)
			{
				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (numChars)
				{
					if (!this->TestFlags(FLAG_HAS_DESC_TEXT))
						return OLV_RESULT_MISSING_DATA;

					memset(pBuffer, 0, 2 * numChars);
					olv_wstrncpy(pBuffer, this->description, numChars);
					return OLV_RESULT_SUCCESS;
				}

				return OLV_RESULT_NOT_ENOUGH_SIZE;
			}
			static sint32 __GetDescriptionText(DownloadedCommunityData* _this, char16_t* pBuffer, uint32 numChars)
			{
				return _this->GetDescriptionText(pBuffer, numChars);
			}

			sint32 GetAppData(uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				uint32 appDataSize = bufferSize;
				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (bufferSize)
				{
					if (!this->TestFlags(FLAG_HAS_APP_DATA))
						return OLV_RESULT_MISSING_DATA;

					if (this->appDataLen < appDataSize)
						appDataSize = this->appDataLen;

					memcpy(pBuffer, this->appData, appDataSize);
					if (pOutSize)
						*pOutSize = appDataSize;

					return OLV_RESULT_SUCCESS;
				}

				return OLV_RESULT_NOT_ENOUGH_SIZE;
			}
			static sint32 __GetAppData(DownloadedCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				return _this->GetAppData(pBuffer, pOutSize, bufferSize);
			}

			uint32 GetAppDataSize() const
			{
				if (this->TestFlags(FLAG_HAS_APP_DATA))
					return this->appDataLen;

				return 0;
			}
			static uint32 __GetAppDataSize(DownloadedCommunityData* _this)
			{
				return _this->GetAppDataSize();
			}

			sint32 GetIconData(uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (bufferSize < sizeof(this->iconData))
					return OLV_RESULT_NOT_ENOUGH_SIZE;

				if (!this->TestFlags(FLAG_HAS_ICON_DATA))
					return OLV_RESULT_MISSING_DATA;

				sint32 decodeRes = DecodeTGA(this->iconData, this->iconDataSize, pBuffer, bufferSize, TGACheckType::CHECK_COMMUNITY_ICON);
				if (decodeRes >= 0)
				{
					if (pOutSize)
						*pOutSize = (uint32)decodeRes;

					return OLV_RESULT_SUCCESS;
				}

				if (pOutSize)
					*pOutSize = 0;

				if (decodeRes == -1)
					cemuLog_log(LogType::Force, "OLIVE - icon uncompress failed.\n");
				else if (decodeRes == -2)
					cemuLog_log(LogType::Force, "OLIVE - icon decode error. NOT TGA.\n");

				return OLV_RESULT_INVALID_TEXT_FIELD;
			}
			static sint32 __GetIconData(DownloadedCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				return _this->GetIconData(pBuffer, pOutSize, bufferSize);
			}

			sint32 GetOwnerMiiData(/* FFLStoreData* */void* pBuffer) const
			{
				if (!this->TestFlags(FLAG_HAS_MII_DATA))
					return OLV_RESULT_MISSING_DATA;

				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				memcpy(pBuffer, this->miiFFLStoreData, sizeof(this->miiFFLStoreData));
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __GetOwnerMiiData(DownloadedCommunityData* _this, /* FFLStoreData* */void* pBuffer)
			{
				return _this->GetOwnerMiiData(pBuffer);
			}

			const char16_t* GetOwnerMiiNickname() const
			{
				if (this->miiDisplayName[0])
					return this->miiDisplayName;

				return nullptr;
			}
			static const char16_t* __GetOwnerMiiNickname(DownloadedCommunityData* _this)
			{
				return _this->GetOwnerMiiNickname();
			}

		public:
			uint32be flags;
			uint32be communityId;
			uint32be pid;
			char16_t titleText[128];
			uint32be titleTextMaxLen;
			char16_t description[256];
			uint32be descriptionMaxLen;
			uint8 appData[1024];
			uint32be appDataLen;
			uint8 iconData[65580];
			uint32be iconDataSize;
			uint8 miiFFLStoreData[96];
			char16_t miiDisplayName[32];
			uint8 unk[6168];
		};
		static_assert(sizeof(nn::olv::DownloadedCommunityData) == 0x12000, "sizeof(nn::olv::DownloadedCommunityData) != 0x12000");

		class DownloadCommunityDataListParam
		{
		public:
			static const inline uint32 FLAG_FILTER_FAVORITES = (1 << 0);
			static const inline uint32 FLAG_FILTER_OFFICIALS = (1 << 1);
			static const inline uint32 FLAG_FILTER_OWNED = (1 << 2);
			static const inline uint32 FLAG_QUERY_MII_DATA = (1 << 3);
			static const inline uint32 FLAG_QUERY_ICON_DATA = (1 << 4);

			DownloadCommunityDataListParam()
			{
				this->flags = 0;
				this->communityDownloadLimit = 0;
				this->communityId = 0;

				for (int i = 0; i < 20; i++)
					this->additionalCommunityIdList[i] = -2;
			}
			static DownloadCommunityDataListParam* __ctor(DownloadCommunityDataListParam* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				}
				else
					return new (_this) DownloadCommunityDataListParam();
			}

			sint32 SetFlags(uint32 flags)
			{
				this->flags = flags;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetFlags(DownloadCommunityDataListParam* _this, uint32 flags)
			{
				return _this->SetFlags(flags);
			}

			sint32 SetCommunityId(uint32 communityId)
			{
				if (communityId == -1)
					return OLV_RESULT_INVALID_PARAMETER;

				this->communityId = communityId;
				if (communityId)
				{
					if (!this->communityDownloadLimit)
						this->communityDownloadLimit = 1;
				}

				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetCommunityId(DownloadCommunityDataListParam* _this, uint32 communityId)
			{
				return _this->SetCommunityId(communityId);
			}

			sint32 SetCommunityId(uint32 communityId, uint8 idx)
			{
				if (communityId == -1)
					return OLV_RESULT_INVALID_PARAMETER;

				if (idx >= 20)
					return OLV_RESULT_INVALID_PARAMETER;

				this->additionalCommunityIdList[idx] = communityId;
				int validIdsCount = 0;
				for (int i = 0; i < 20; i++ )
				{
					if (this->additionalCommunityIdList[i] != -2)
						++validIdsCount;
				}

				if (validIdsCount > this->communityDownloadLimit)
					this->communityDownloadLimit = validIdsCount;

				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetCommunityId(DownloadCommunityDataListParam* _this, uint32 communityId, uint8 idx)
			{
				return _this->SetCommunityId(communityId, idx);
			}

			sint32 SetCommunityDataMaxNum(uint32 num)
			{
				if (!num)
					return OLV_RESULT_INVALID_PARAMETER;

				int validIdsCount = 0;
				for (int i = 0; i < 20; ++i)
				{
					if (this->additionalCommunityIdList[i] != -2)
						++validIdsCount;
				}

				if (validIdsCount > num)
					return OLV_RESULT_INVALID_PARAMETER;

				this->communityDownloadLimit = num;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetCommunityDataMaxNum(DownloadCommunityDataListParam* _this, uint32 num)
			{
				return _this->SetCommunityDataMaxNum(num);
			}

			sint32 GetRawDataUrl(char* pBuffer, uint32 bufferSize) const
			{
				if (!g_IsOnlineMode)
					return OLV_RESULT_OFFLINE_MODE_REQUEST;

				if (!pBuffer)
					return OLV_RESULT_INVALID_PTR;

				if (!bufferSize)
					return OLV_RESULT_NOT_ENOUGH_SIZE;

				char tmpFormatBuffer[64];
				char urlBuffer[1024];
				memset(urlBuffer, 0, sizeof(urlBuffer));

				uint32 communityId;
				int validIdsCount = 0;
				for (int i = 0; i < 20; ++i)
				{
					if (this->additionalCommunityIdList[i] != -2)
					{
						communityId = this->additionalCommunityIdList[i];
						++validIdsCount;
					}
				}

				if (validIdsCount)
				{
					if (this->communityId && this->communityId != -2)
						return OLV_RESULT_INVALID_PARAMETER;

					uint32 unkFlag = this->flags & 0xFFFFFFE7;
					if (unkFlag)
						return OLV_RESULT_INVALID_PARAMETER;

					// It's how it's done in the real nn_olv, what even the fuck is this, never seen used yet.
					snprintf(urlBuffer, sizeof(urlBuffer), "%s/v1/communities/%u.search", g_DiscoveryResults.apiEndpoint, communityId);

					for (int i = 0; i < 20; ++i)
					{
						if (this->additionalCommunityIdList[i] != -2)
						{
							snprintf(tmpFormatBuffer, 64, "community_id=%u", this->additionalCommunityIdList[i].value());
							appendQueryToURL(urlBuffer, tmpFormatBuffer);
							++unkFlag;
						}
					}
				}
				else
					snprintf(urlBuffer, sizeof(urlBuffer), "%s/v1/communities", g_DiscoveryResults.apiEndpoint);

				if (this->communityId)
				{
					snprintf(tmpFormatBuffer, 64, "community_id=%u", this->communityId.value());
					appendQueryToURL(urlBuffer, tmpFormatBuffer);
				}
				else
				{
					uint32 filterBy_favorite = (this->flags & FLAG_FILTER_FAVORITES) != 0;
					uint32 filterBy_official = (this->flags & FLAG_FILTER_OFFICIALS) != 0;
					uint32 filterBy_selfmade = (this->flags & FLAG_FILTER_OWNED) != 0;
				
					if ((filterBy_favorite + filterBy_official + filterBy_selfmade) != 1)
						return OLV_RESULT_INVALID_PARAMETER;

					snprintf(tmpFormatBuffer, 64, "limit=%u", this->communityDownloadLimit.value());
					appendQueryToURL(urlBuffer, tmpFormatBuffer);

					if (filterBy_favorite)
					{
						strncpy(tmpFormatBuffer, "type=favorite", 64);
						appendQueryToURL(urlBuffer, tmpFormatBuffer);
					}
					else if (filterBy_official)
					{
						strncpy(tmpFormatBuffer, "type=official", 64);
						appendQueryToURL(urlBuffer, tmpFormatBuffer);
					}
					else
					{
						strncpy(tmpFormatBuffer, "type=my", 64);
						appendQueryToURL(urlBuffer, tmpFormatBuffer);
					}
				}

				if (this->flags & FLAG_QUERY_MII_DATA)
				{
					strncpy(tmpFormatBuffer, "with_mii=1", 64);
					appendQueryToURL(urlBuffer, tmpFormatBuffer);
				}

				if (this->flags & FLAG_QUERY_ICON_DATA)
				{
					strncpy(tmpFormatBuffer, "with_icon=1", 64);
					appendQueryToURL(urlBuffer, tmpFormatBuffer);
				}

				int res = snprintf(pBuffer, bufferSize, "%s", urlBuffer);
				if (res < 0)
					return OLV_RESULT_NOT_ENOUGH_SIZE;

				return OLV_RESULT_SUCCESS;
			}
			static sint32 __GetRawDataUrl(DownloadCommunityDataListParam* _this, char* pBuffer, uint32 bufferSize)
			{
				return _this->GetRawDataUrl(pBuffer, bufferSize);
			}

		public:
			uint32be flags;
			uint32be communityId;
			uint32be communityDownloadLimit;
			uint32be additionalCommunityIdList[20]; // Additional community ID filter list
			uint8 unk[4004]; // Looks unused lol, probably reserved data
		};
		static_assert(sizeof(nn::olv::DownloadCommunityDataListParam) == 0x1000, "sizeof(nn::olv::DownloadCommunityDataListParam) != 0x1000");

		sint32 DownloadCommunityDataList(DownloadedCommunityData* pOutList, uint32* pOutNum, uint32 numMaxList, const DownloadCommunityDataListParam* pParam);

		static void loadOliveDownloadCommunityTypes()
		{
			cafeExportRegisterFunc(DownloadedCommunityData::__ctor, "nn_olv", "__ct__Q3_2nn3olv23DownloadedCommunityDataFv", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__TestFlags, "nn_olv", "TestFlags__Q3_2nn3olv23DownloadedCommunityDataCFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetCommunityId, "nn_olv", "GetCommunityId__Q3_2nn3olv23DownloadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetCommunityCode, "nn_olv", "GetCommunityCode__Q3_2nn3olv23DownloadedCommunityDataCFPcUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetOwnerPid, "nn_olv", "GetOwnerPid__Q3_2nn3olv23DownloadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetTitleText, "nn_olv", "GetTitleText__Q3_2nn3olv23DownloadedCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetDescriptionText, "nn_olv", "GetDescriptionText__Q3_2nn3olv23DownloadedCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetAppData, "nn_olv", "GetAppData__Q3_2nn3olv23DownloadedCommunityDataCFPUcPUiUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetAppDataSize, "nn_olv", "GetAppDataSize__Q3_2nn3olv23DownloadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetIconData, "nn_olv", "GetIconData__Q3_2nn3olv23DownloadedCommunityDataCFPUcPUiUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetOwnerMiiData, "nn_olv", "GetOwnerMiiData__Q3_2nn3olv23DownloadedCommunityDataCFP12FFLStoreData", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadedCommunityData::__GetOwnerMiiNickname, "nn_olv", "GetOwnerMiiNickname__Q3_2nn3olv23DownloadedCommunityDataCFv", LogType::NN_OLV);

			cafeExportRegisterFunc(DownloadCommunityDataListParam::__ctor, "nn_olv", "__ct__Q3_2nn3olv30DownloadCommunityDataListParamFv", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadCommunityDataListParam::__SetFlags, "nn_olv", "SetFlags__Q3_2nn3olv30DownloadCommunityDataListParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadCommunityDataListParam::__SetCommunityDataMaxNum, "nn_olv", "SetCommunityDataMaxNum__Q3_2nn3olv30DownloadCommunityDataListParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(DownloadCommunityDataListParam::__GetRawDataUrl, "nn_olv", "GetRawDataUrl__Q3_2nn3olv30DownloadCommunityDataListParamCFPcUi", LogType::NN_OLV);

			cafeExportRegisterFunc((sint32 (*)(DownloadCommunityDataListParam*, uint32))DownloadCommunityDataListParam::__SetCommunityId,
				"nn_olv", "SetCommunityId__Q3_2nn3olv30DownloadCommunityDataListParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc((sint32(*)(DownloadCommunityDataListParam*, uint32, uint8))DownloadCommunityDataListParam::__SetCommunityId,
				"nn_olv", "SetCommunityId__Q3_2nn3olv30DownloadCommunityDataListParamFUiUc", LogType::NN_OLV);
		
			cafeExportRegisterFunc(DownloadCommunityDataList, "nn_olv", "DownloadCommunityDataList__Q2_2nn3olvFPQ3_2nn3olv23DownloadedCommunityDataPUiUiPCQ3_2nn3olv30DownloadCommunityDataListParam", LogType::NN_OLV);
		}
	}
}