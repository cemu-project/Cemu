#pragma once

#include "Cemu/ncrypto/ncrypto.h"
#include "config/ActiveSettings.h"

#include "Cafe/OS/libs/nn_olv/nn_olv_Common.h"

namespace nn
{
	namespace olv
	{
		class UploadedCommunityData
		{
		public:
			static const inline uint32 FLAG_HAS_TITLE_TEXT = (1 << 0);
			static const inline uint32 FLAG_HAS_DESC_TEXT = (1 << 1);
			static const inline uint32 FLAG_HAS_APP_DATA = (1 << 2);
			static const inline uint32 FLAG_HAS_ICON_DATA = (1 << 3);
		
			UploadedCommunityData()
			{
				this->titleTextMaxLen = 0;
				this->appDataLen = 0;
				this->descriptionMaxLen = 0;
				this->pid = 0;
				this->communityId = 0;
				this->flags = 0;
				this->iconDataSize = 0;
			}
			static UploadedCommunityData* __ctor(UploadedCommunityData* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				}
				else
					return new (_this) UploadedCommunityData();
			}

			static UploadedCommunityData* Clean(UploadedCommunityData* data)
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
			static bool __TestFlags(UploadedCommunityData* _this, uint32 flags)
			{
				return _this->TestFlags(flags);
			}

			uint32 GetCommunityId() const
			{
				return this->communityId;
			}
			static uint32 __GetCommunityId(UploadedCommunityData* _this)
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
			static sint32 __GetCommunityCode(UploadedCommunityData* _this, char* pBuffer, uint32 bufferSize)
			{
				return _this->GetCommunityCode(pBuffer, bufferSize);
			}

			uint32 GetOwnerPid() const
			{
				return this->pid;
			}
			static uint32 __GetOwnerPid(UploadedCommunityData* _this)
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
					return  OLV_RESULT_SUCCESS;
				}

				return OLV_RESULT_NOT_ENOUGH_SIZE;
			}
			static sint32 __GetTitleText(UploadedCommunityData* _this, char16_t* pBuffer, uint32 numChars)
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
			static sint32 __GetDescriptionText(UploadedCommunityData* _this, char16_t* pBuffer, uint32 numChars)
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
			static sint32 __GetAppData(UploadedCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
			{
				return _this->GetAppData(pBuffer, pOutSize, bufferSize);
			}

			uint32 GetAppDataSize() const
			{
				if (this->TestFlags(FLAG_HAS_APP_DATA))
					return this->appDataLen;

				return 0;
			}
			static uint32 __GetAppDataSize(UploadedCommunityData* _this)
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
			static sint32 __GetIconData(UploadedCommunityData* _this, uint8* pBuffer, uint32be* pOutSize, uint32 bufferSize)
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
		static_assert(sizeof(nn::olv::UploadedCommunityData) == 0x12000, "sizeof(nn::olv::UploadedCommunityData) != 0x12000");


		class UploadCommunityDataParam
		{
		public:
			static const inline uint32 FLAG_DELETION = (1 << 0);
		
			UploadCommunityDataParam()
			{
				this->appDataLen = 0;
				this->communityId = 0;
				this->titleId = 0;
				this->iconData = MEMPTR<uint8>(nullptr);
				this->appData = MEMPTR<uint8>(nullptr);
				this->iconDataLen = 0;
				this->flags = 0;
				memset(this->titleText, 0, sizeof(this->titleText));
				memset(this->description, 0, sizeof(this->description));
				for (int i = 0; i < 5; i++)
					memset(this->searchKeys[i], 0, sizeof(this->searchKeys[0]));
			}
			static UploadCommunityDataParam* __ctor(UploadCommunityDataParam* _this)
			{
				if (!_this)
				{
					assert_dbg(); // DO NOT CONTINUE, SHOULD NEVER HAPPEN
					return nullptr;
				}
				else
					return new (_this) UploadCommunityDataParam();
			}

			sint32 SetFlags(uint32 flags)
			{
				this->flags = flags;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetFlags(UploadCommunityDataParam* _this, uint32 flags)
			{
				return _this->SetFlags(flags);
			}

			sint32 SetCommunityId(uint32 communityId)
			{
				if (communityId == -1)
					return OLV_RESULT_INVALID_PARAMETER;

				this->communityId = communityId;
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetCommunityId(UploadCommunityDataParam* _this, uint32 communityId)
			{
				return _this->SetCommunityId(communityId);
			}

			sint32 SetAppData(MEMPTR<uint8> pBuffer, uint32 bufferSize)
			{
				if (!pBuffer.IsNull())
				{
					if (bufferSize - 1 >= 0x400)
						return OLV_RESULT_NOT_ENOUGH_SIZE;

					this->appData = pBuffer;
					this->appDataLen = bufferSize;
				}
				else
				{
					this->appData = MEMPTR<uint8>(nullptr);
					this->appDataLen = 0;
				}
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetAppData(UploadCommunityDataParam* _this, MEMPTR<uint8> pBuffer, uint32 bufferSize)
			{
				return _this->SetAppData(pBuffer, bufferSize);
			}

			sint32 SetTitleText(char16_t const* pText)
			{
				if (pText)
					return olv_copy_wstr(this->titleText, pText, 127, 128);
			
				memset(this->titleText, 0, sizeof(this->titleText));
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetTitleText(UploadCommunityDataParam* _this, char16_t const* pText)
			{
				return _this->SetTitleText(pText);
			}

			sint32 SetDescriptionText(char16_t const* pText)
			{
				if (pText)
					return olv_copy_wstr(this->description, pText, 255, 256);

				memset(this->description, 0, sizeof(this->description));
				return OLV_RESULT_SUCCESS;
			}
			static sint32 __SetDescriptionText(UploadCommunityDataParam* _this, char16_t const* pText)
			{
				return _this->SetDescriptionText(pText);
			}

			sint32 SetIconData(MEMPTR<uint8> pBuffer, uint32 bufferSize)
			{
				if (!pBuffer.IsNull())
				{
					if (bufferSize)
					{
						if (bufferSize - 0x10012 < 0x1B)
						{
							if (CheckTGA(pBuffer.GetPtr(), bufferSize, TGACheckType::CHECK_COMMUNITY_ICON))
							{
								this->iconData = pBuffer;
								this->iconDataLen = bufferSize;
								return OLV_RESULT_SUCCESS;
							}
							else
							{
								cemuLog_log(LogType::Force, "OLIVE - SetIconData: TGA Check Failed.\n");
								return OLV_RESULT_INVALID_DATA;
							}
						}
						else
							return OLV_RESULT_NOT_ENOUGH_SIZE;
					}
					else
						return OLV_RESULT_NOT_ENOUGH_SIZE;
				}
				else
				{
					this->iconData = MEMPTR<uint8>(nullptr);
					this->iconDataLen = 0;
					return OLV_RESULT_SUCCESS;
				}
			}
			static sint32 __SetIconData(UploadCommunityDataParam* _this, MEMPTR<uint8> pBuffer, uint32 bufferSize)
			{
				return _this->SetIconData(pBuffer, bufferSize);
			}

		public:
			uint32be flags;
			sint32be ___padding_0c;
			uint64be titleId;
			uint32be communityId;
			char16_t titleText[128];
			char16_t description[256];
			char16_t searchKeys[5][152];
			MEMPTR<uint8_t> appData;
			uint32be appDataLen;
			MEMPTR<uint8_t> iconData;
			uint32be iconDataLen;
			char unk3[1772];
		};
		static_assert(sizeof(nn::olv::UploadCommunityDataParam) == 0x1000, "sizeof(nn::olv::UploadCommunityDataParam) != 0x1000");

		sint32 UploadCommunityData(UploadCommunityDataParam const* pParam);
		sint32 UploadCommunityData(UploadedCommunityData* pOutData, UploadCommunityDataParam const* pParam);

		static void loadOliveUploadCommunityTypes()
		{
			cafeExportRegisterFunc(UploadedCommunityData::__ctor, "nn_olv", "__ct__Q3_2nn3olv21UploadedCommunityDataFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__TestFlags, "nn_olv", "TestFlags__Q3_2nn3olv21UploadedCommunityDataCFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetCommunityId, "nn_olv", "GetCommunityId__Q3_2nn3olv21UploadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetCommunityCode, "nn_olv", "GetCommunityCode__Q3_2nn3olv21UploadedCommunityDataCFPcUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetOwnerPid, "nn_olv", "GetOwnerPid__Q3_2nn3olv21UploadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetTitleText, "nn_olv", "GetTitleText__Q3_2nn3olv21UploadedCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetDescriptionText, "nn_olv", "GetDescriptionText__Q3_2nn3olv21UploadedCommunityDataCFPwUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetAppData, "nn_olv", "GetAppData__Q3_2nn3olv21UploadedCommunityDataCFPUcPUiUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetAppDataSize, "nn_olv", "GetAppDataSize__Q3_2nn3olv21UploadedCommunityDataCFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadedCommunityData::__GetIconData, "nn_olv", "GetIconData__Q3_2nn3olv21UploadedCommunityDataCFPUcPUiUi", LogType::NN_OLV);
		
			cafeExportRegisterFunc(UploadCommunityDataParam::__ctor, "nn_olv", "__ct__Q3_2nn3olv24UploadCommunityDataParamFv", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetFlags, "nn_olv", "SetFlags__Q3_2nn3olv24UploadCommunityDataParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetCommunityId, "nn_olv", "SetCommunityId__Q3_2nn3olv24UploadCommunityDataParamFUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetAppData, "nn_olv", "SetAppData__Q3_2nn3olv24UploadCommunityDataParamFPCUcUi", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetTitleText, "nn_olv", "SetTitleText__Q3_2nn3olv24UploadCommunityDataParamFPCw", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetDescriptionText, "nn_olv", "SetDescriptionText__Q3_2nn3olv24UploadCommunityDataParamFPCw", LogType::NN_OLV);
			cafeExportRegisterFunc(UploadCommunityDataParam::__SetIconData, "nn_olv", "SetIconData__Q3_2nn3olv24UploadCommunityDataParamFPCUcUi", LogType::NN_OLV);

			cafeExportRegisterFunc((sint32(*)(UploadCommunityDataParam const*))UploadCommunityData,
				"nn_olv", "UploadCommunityData__Q2_2nn3olvFPCQ3_2nn3olv24UploadCommunityDataParam", LogType::NN_OLV);
		
			cafeExportRegisterFunc((sint32(*)(UploadedCommunityData *, UploadCommunityDataParam const*))UploadCommunityData,
				"nn_olv", "UploadCommunityData__Q2_2nn3olvFPQ3_2nn3olv21UploadedCommunityDataPCQ3_2nn3olv24UploadCommunityDataParam", LogType::NN_OLV);
		}

	}
}