#pragma once

#include "Cemu/ncrypto/ncrypto.h"
#include "config/ActiveSettings.h"

#include "Cafe/OS/libs/nn_olv/nn_olv_Common.h"

namespace nn
{
	namespace olv
	{
		class UploadedFavoriteToCommunityData
		{
		public:
			static const inline uint32 FLAG_HAS_TITLE_TEXT = (1 << 0);
			static const inline uint32 FLAG_HAS_DESC_TEXT = (1 << 1);
			static const inline uint32 FLAG_HAS_APP_DATA = (1 << 2);
			static const inline uint32 FLAG_HAS_ICON_DATA = (1 << 3);
		
			UploadedFavoriteToCommunityData()
			{
				this->titleTextMaxLen = 0;
				this->appDataLen = 0;
				this->descriptionMaxLen = 0;
				this->pid = 0;
				this->communityId = 0;
				this->flags = 0;
				this->iconDataSize = 0;
			}
			static UploadedFavoriteToCommunityData* __ctor(UploadedFavoriteToCommunityData* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				} 
				else
					return new (_this) UploadedFavoriteToCommunityData();
			}

			static UploadedFavoriteToCommunityData* Clean(UploadedFavoriteToCommunityData* data)
			{
				data->appDataLen = 0;
				data->pid = 0;
				data->titleText[0] = 0;
				data->description[0] = 0;
				data->appData[0] = 0;
				data->titleTextMaxLen = 0;
				data->iconData[0] = 0;
				data->descriptionMaxLen = 0;
				data->communityId = 0;
				data->flags = 0;
				data->iconDataSize = 0;
				return data;
			}

			bool TestFlags(uint32 flags) const
			{
				return (this->flags & flags) != 0;
			}
			static bool __TestFlags(UploadedFavoriteToCommunityData* _this, uint32 flags)
			{
				return _this->TestFlags(flags);
			}

			uint32 GetCommunityId() const
			{
				return this->communityId;
			}
			static uint32 __GetCommunityId(UploadedFavoriteToCommunityData* _this)
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
			static sint32 __GetCommunityCode(UploadedFavoriteToCommunityData* _this, char* pBuffer, uint32 bufferSize)
			{
				return _this->GetCommunityCode(pBuffer, bufferSize);
			}

			uint32 GetOwnerPid() const
			{
				return this->pid;
			}
			static uint32 __GetOwnerPid(UploadedFavoriteToCommunityData* _this)
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
			static sint32 __GetTitleText(UploadedFavoriteToCommunityData* _this, char16_t* pBuffer, uint32 numChars)
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
			static sint32 __GetDescriptionText(UploadedFavoriteToCommunityData* _this, char16_t* pBuffer, uint32 numChars)
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
			static sint32 __GetAppData(UploadedFavoriteToCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				return _this->GetAppData(pBuffer, pOutSize, bufferSize);
			}

			uint32 GetAppDataSize() const
			{
				if (this->TestFlags(FLAG_HAS_APP_DATA))
					return this->appDataLen;

				return 0;
			}
			static uint32 __GetAppDataSize(UploadedFavoriteToCommunityData* _this)
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
			static sint32 __GetIconData(UploadedFavoriteToCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				return _this->GetIconData(pBuffer, pOutSize, bufferSize);
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
			uint8 unk[6328];
		};
		static_assert(sizeof(nn::olv::UploadedFavoriteToCommunityData) == 0x12000, "sizeof(nn::olv::UploadedFavoriteToCommunityData) != 0x12000");

		class UploadFavoriteToCommunityDataParam
		{

		public:
			static const inline uint32 FLAG_DELETION = (1 << 0);

			UploadFavoriteToCommunityDataParam()
			{
				this->communityId = 0;
				this->flags = 0;
			}
			static UploadFavoriteToCommunityDataParam* __ctor(UploadFavoriteToCommunityDataParam* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				}
				else
					return new (_this) UploadFavoriteToCommunityDataParam();
			}

			sint32 SetFlags(uint32 flags)
			{
				this->flags = flags;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetFlags(UploadFavoriteToCommunityDataParam* _this, uint32 flags)
			{
				return _this->SetFlags(flags);
			}

			sint32 SetCommunityCode(const char* pBuffer)
			{
				if (strnlen(pBuffer, 13) != 12)
					return OLV_RESULT_INVALID_TEXT_FIELD;

				uint32_t id;
				if (GetCommunityIdFromCode(&id, pBuffer))
				{
					this->communityId = id;
					return OLV_RESULT_SUCCESS;
				}

				return OLV_RESULT_STATUS(1901);
			}
			static sint32 __SetCommunityCode(UploadFavoriteToCommunityDataParam* _this, char* pBuffer)
			{
				return _this->SetCommunityCode(pBuffer);
			}

			sint32 SetCommunityId(uint32 communityId)
			{
				if (communityId == -1)
					return OLV_RESULT_INVALID_PARAMETER;

				this->communityId = communityId;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetCommunityId(UploadFavoriteToCommunityDataParam* _this, uint32 communityId)
			{
				return _this->SetCommunityId(communityId);
			}

		public:
			uint32be flags;
			uint32be communityId;
			uint8 unk[54]; // Unused
		};
		static_assert(sizeof(nn::olv::UploadFavoriteToCommunityDataParam) == 64, "sizeof(nn::olv::UploadFavoriteToCommunityDataParam) != 64");

		sint32 UploadFavoriteToCommunityData(const UploadFavoriteToCommunityDataParam* pParam);
		sint32 UploadFavoriteToCommunityData(UploadedFavoriteToCommunityData* pOutData, const UploadFavoriteToCommunityDataParam* pParam);

		static void loadOliveUploadFavoriteTypes()
		{
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__ctor, "nn_olv", "__ct__Q3_2nn3olv31UploadedFavoriteToCommunityDataFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__TestFlags, "nn_olv", "TestFlags__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetCommunityId, "nn_olv", "GetCommunityId__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetCommunityCode, "nn_olv", "GetCommunityCode__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFPcUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetOwnerPid, "nn_olv", "GetOwnerPid__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetTitleText, "nn_olv", "GetTitleText__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetDescriptionText, "nn_olv", "GetDescriptionText__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetAppData, "nn_olv", "GetAppData__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFPUcPUiUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetAppDataSize, "nn_olv", "GetAppDataSize__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedFavoriteToCommunityData::__GetIconData, "nn_olv", "GetIconData__Q3_2nn3olv31UploadedFavoriteToCommunityDataCFPUcPUiUi", LogType::NN_OLV);
			
			cafeExportRegisterFunc(UploadFavoriteToCommunityDataParam::__ctor, "nn_olv", "__ct__Q3_2nn3olv34UploadFavoriteToCommunityDataParamFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadFavoriteToCommunityDataParam::__SetFlags, "nn_olv", "SetFlags__Q3_2nn3olv34UploadFavoriteToCommunityDataParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadFavoriteToCommunityDataParam::__SetCommunityCode, "nn_olv", "SetCommunityCode__Q3_2nn3olv34UploadFavoriteToCommunityDataParamFPCc", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadFavoriteToCommunityDataParam::__SetCommunityId, "nn_olv", "SetCommunityId__Q3_2nn3olv34UploadFavoriteToCommunityDataParamFUi", LogType::NN_OLV);

			cafeExportRegisterFunc((sint32(*)(const UploadFavoriteToCommunityDataParam*))UploadFavoriteToCommunityData,
				"nn_olv", "UploadFavoriteToCommunityData__Q2_2nn3olvFPCQ3_2nn3olv34UploadFavoriteToCommunityDataParam", LogType::NN_OLV);

			cafeExportRegisterFunc((sint32(*)(UploadedFavoriteToCommunityData*, const UploadFavoriteToCommunityDataParam*))UploadFavoriteToCommunityData,
				"nn_olv", "UploadFavoriteToCommunityData__Q2_2nn3olvFPQ3_2nn3olv31UploadedFavoriteToCommunityDataPCQ3_2nn3olv34UploadFavoriteToCommunityDataParam", LogType::NN_OLV);
		}

	}
}