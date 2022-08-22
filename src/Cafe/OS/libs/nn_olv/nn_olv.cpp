#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/OS/libs/nn_common.h"
#include "nn_olv.h"

namespace nn
{
	namespace olv
	{
		struct DownloadedPostData_t
		{
			/* +0x0000 */ uint32be flags;
			/* +0x0004 */ uint32be userPrincipalId;
			/* +0x0008 */ char postId[0x20]; // size guessed
			/* +0x0028 */ uint64 postDate;
			/* +0x0030 */ uint8 feeling;
			/* +0x0031 */ uint8 padding0031[3];
			/* +0x0034 */ uint32be regionId;
			/* +0x0038 */ uint8 platformId;
			/* +0x0039 */ uint8 languageId;
			/* +0x003A */ uint8 countryId;
			/* +0x003B */ uint8 padding003B[1];
			/* +0x003C */ uint16be bodyText[0x100]; // actual size is unknown
			/* +0x023C */ uint32be bodyTextLength; 
			/* +0x0240 */ uint8 compressedMemoBody[0xA000]; // 40KB
			/* +0xA240 */ uint32be compressedMemoBodyRelated; // size of compressed data?
			/* +0xA244 */ uint16be topicTag[0x98];
			// app data
			/* +0xA374 */ uint8 appData[0x400];
			/* +0xA774 */ uint32be appDataLength;
			// external binary
			/* +0xA778 */ uint8 externalBinaryUrl[0x100];
			/* +0xA878 */ uint32be externalBinaryDataSize;
			// external image
			/* +0xA87C */ uint8 externalImageDataUrl[0x100];
			/* +0xA97C */ uint32be externalImageDataSize;
			// external url ?
			/* +0xA980 */ char externalUrl[0x100];
			// mii
			/* +0xAA80 */ uint8 miiData[0x60];
			/* +0xAAE0 */ uint16be miiNickname[0x20];
			/* +0xAB20 */ uint8 unusedAB20[0x14E0];

			// everything above is part of DownloadedDataBase
			// everything below is part of DownloadedPostData
			/* +0xC000 */ uint8 uknDataC000[8]; // ??
			/* +0xC008 */ uint32be communityId;
			/* +0xC00C */ uint32be empathyCount;
			/* +0xC010 */ uint32be commentCount;
			/* +0xC014 */ uint8 unused[0x1F4];
		}; // size: 0xC208

		static_assert(sizeof(DownloadedPostData_t) == 0xC208, "");
		static_assert(offsetof(DownloadedPostData_t, postDate) == 0x0028, "");
		static_assert(offsetof(DownloadedPostData_t, platformId) == 0x0038, "");
		static_assert(offsetof(DownloadedPostData_t, bodyText) == 0x003C, "");
		static_assert(offsetof(DownloadedPostData_t, compressedMemoBody) == 0x0240, "");
		static_assert(offsetof(DownloadedPostData_t, topicTag) == 0xA244, "");
		static_assert(offsetof(DownloadedPostData_t, appData) == 0xA374, "");
		static_assert(offsetof(DownloadedPostData_t, externalBinaryUrl) == 0xA778, "");
		static_assert(offsetof(DownloadedPostData_t, externalImageDataUrl) == 0xA87C, "");
		static_assert(offsetof(DownloadedPostData_t, externalUrl) == 0xA980, "");
		static_assert(offsetof(DownloadedPostData_t, miiData) == 0xAA80, "");
		static_assert(offsetof(DownloadedPostData_t, miiNickname) == 0xAAE0, "");
		static_assert(offsetof(DownloadedPostData_t, unusedAB20) == 0xAB20, "");
		static_assert(offsetof(DownloadedPostData_t, communityId) == 0xC008, "");
		static_assert(offsetof(DownloadedPostData_t, empathyCount) == 0xC00C, "");
		static_assert(offsetof(DownloadedPostData_t, commentCount) == 0xC010, "");

		const int POST_DATA_FLAG_HAS_BODY_TEXT = (0x0001);
		const int POST_DATA_FLAG_HAS_BODY_MEMO = (0x0002);


		void export_DownloadPostDataList(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedTopicData, void, 0); // DownloadedTopicData
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 1); // DownloadedPostData
			ppcDefineParamTypePtr(downloadedPostDataSize, uint32be, 2);
			ppcDefineParamS32(maxCount, 3);
			ppcDefineParamTypePtr(listParam, void, 4); // DownloadPostDataListParam

			maxCount = 0; // DISABLED

			// just some test
			for (sint32 i = 0; i < maxCount; i++)
			{
				DownloadedPostData_t* postData = downloadedPostData + i;
				memset(postData, 0, sizeof(DownloadedPostData_t));
				postData->userPrincipalId = 0x1000 + i;
				// post id
				sprintf(postData->postId, "postid-%04x", i+(GetTickCount()%10000));
				postData->bodyTextLength = 12;
				postData->bodyText[0] = 'H';
				postData->bodyText[1] = 'e';
				postData->bodyText[2] = 'l';
				postData->bodyText[3] = 'l';
				postData->bodyText[4] = 'o';
				postData->bodyText[5] = ' ';
				postData->bodyText[6] = 'w';
				postData->bodyText[7] = 'o';
				postData->bodyText[8] = 'r';
				postData->bodyText[9] = 'l';
				postData->bodyText[10] = 'd';
				postData->bodyText[11] = '!';

				postData->miiNickname[0] = 'C';
				postData->miiNickname[1] = 'e';
				postData->miiNickname[2] = 'm';
				postData->miiNickname[3] = 'u';
				postData->miiNickname[4] = '-';
				postData->miiNickname[5] = 'M';
				postData->miiNickname[6] = 'i';
				postData->miiNickname[7] = 'i';

				postData->topicTag[0] = 't';
				postData->topicTag[1] = 'o';
				postData->topicTag[2] = 'p';
				postData->topicTag[3] = 'i';
				postData->topicTag[4] = 'c';

				postData->flags = POST_DATA_FLAG_HAS_BODY_TEXT;
			}
			*downloadedPostDataSize = maxCount;

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_DownloadCommunityDataList(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(communityListSizeOut, uint32be, 1);

			*communityListSizeOut = 0;
			osLib_returnFromFunction(hCPU, 0);
		}

		void exportDownloadPostData_TestFlags(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 0);
			ppcDefineParamU32(testFlags, 1);

			if (((uint32)downloadedPostData->flags) & testFlags)
				osLib_returnFromFunction(hCPU, 1);
			else
				osLib_returnFromFunction(hCPU, 0);
		}

		void exportDownloadPostData_GetPostId(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 0);
			osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(downloadedPostData->postId));
		}

		void exportDownloadPostData_GetMiiNickname(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 0);
			if(downloadedPostData->miiNickname[0] == 0 )
				osLib_returnFromFunction(hCPU, MPTR_NULL);
			else
				osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(downloadedPostData->miiNickname));
		}

		void exportDownloadPostData_GetTopicTag(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 0);
			osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(downloadedPostData->topicTag));
		}

		void exportDownloadPostData_GetBodyText(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(downloadedPostData, DownloadedPostData_t, 0);
			ppcDefineParamWStrBE(strOut, 1);
			ppcDefineParamS32(maxLength, 2);

			if (((uint32)downloadedPostData->flags&POST_DATA_FLAG_HAS_BODY_TEXT) == 0)
			{
				osLib_returnFromFunction(hCPU, 0xC1106800);
				return;
			}

			memset(strOut, 0, sizeof(uint16be)*maxLength);
			sint32 copyLen = std::min(maxLength - 1, (sint32)downloadedPostData->bodyTextLength);
			for (sint32 i = 0; i < copyLen; i++)
			{
				strOut[i] = downloadedPostData->bodyText[i];
			}
			strOut[copyLen] = '\0';

			osLib_returnFromFunction(hCPU, 0);
		}

		struct PortalAppParam_t
		{
			/* +0x1A663B */ char serviceToken[32]; // size is unknown
		};

		void exportPortalAppParam_GetServiceToken(PPCInterpreter_t* hCPU)
		{
			// r3 = PortalAppParam
			ppcDefineParamTypePtr(portalAppParam, PortalAppParam_t, 0);

			strcpy(portalAppParam->serviceToken, "servicetoken");
			// this token is probably just the act IndependentServiceToken for the Miiverse title?

			osLib_returnFromFunction(hCPU, memory_getVirtualOffsetFromPointer(&portalAppParam->serviceToken));
		}

		uint32 UploadPostDataByPostApp(void *postParam)
		{
			forceLog_printf("UploadPostDataByPostApp() called. Returning error");
			return BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_OLV, 0); // undefined error
		}

		void load()
		{
			osLib_addFunction("nn_olv", "DownloadPostDataList__Q2_2nn3olvFPQ3_2nn3olv19DownloadedTopicDataPQ3_2nn3olv18DownloadedPostDataPUiUiPCQ3_2nn3olv25DownloadPostDataListParam", export_DownloadPostDataList);
			osLib_addFunction("nn_olv", "TestFlags__Q3_2nn3olv18DownloadedDataBaseCFUi", exportDownloadPostData_TestFlags);
			osLib_addFunction("nn_olv", "GetPostId__Q3_2nn3olv18DownloadedDataBaseCFv", exportDownloadPostData_GetPostId);
			osLib_addFunction("nn_olv", "GetMiiNickname__Q3_2nn3olv18DownloadedDataBaseCFv", exportDownloadPostData_GetMiiNickname);
			osLib_addFunction("nn_olv", "GetTopicTag__Q3_2nn3olv18DownloadedDataBaseCFv", exportDownloadPostData_GetTopicTag);
			osLib_addFunction("nn_olv", "GetBodyText__Q3_2nn3olv18DownloadedDataBaseCFPwUi", exportDownloadPostData_GetBodyText);

			osLib_addFunction("nn_olv", "DownloadCommunityDataList__Q2_2nn3olvFPQ3_2nn3olv23DownloadedCommunityDataPUiUiPCQ3_2nn3olv30DownloadCommunityDataListParam", export_DownloadCommunityDataList);

			osLib_addFunction("nn_olv", "GetServiceToken__Q4_2nn3olv6hidden14PortalAppParamCFv", exportPortalAppParam_GetServiceToken);

			cafeExportRegisterFunc(UploadPostDataByPostApp, "nn_olv", "UploadPostDataByPostApp__Q2_2nn3olvFPCQ3_2nn3olv28UploadPostDataByPostAppParam", LogType::Placeholder);
		}

	}
}