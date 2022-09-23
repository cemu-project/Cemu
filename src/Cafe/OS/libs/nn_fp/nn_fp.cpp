#include "Cafe/OS/common/OSCommon.h"
#include "Cafe/IOSU/legacy/iosu_ioctl.h"
#include "Cafe/IOSU/legacy/iosu_act.h"
#include "Cafe/IOSU/legacy/iosu_fpd.h"
#include "Cafe/OS/libs/coreinit/coreinit_IOS.h"

#define fpdPrepareRequest() \
StackAllocator<iosu::fpd::iosuFpdCemuRequest_t> _buf_fpdRequest; \
StackAllocator<ioBufferVector_t> _buf_bufferVector; \
iosu::fpd::iosuFpdCemuRequest_t* fpdRequest = _buf_fpdRequest.GetPointer(); \
ioBufferVector_t* fpdBufferVector = _buf_bufferVector.GetPointer(); \
memset(fpdRequest, 0, sizeof(iosu::fpd::iosuFpdCemuRequest_t)); \
memset(fpdBufferVector, 0, sizeof(ioBufferVector_t)); \
fpdBufferVector->buffer = (uint8*)fpdRequest;

namespace nn
{
	namespace fp
	{

		struct  
		{
			bool isInitialized;
			bool isAdminMode;
		}g_fp = { };

		void Initialize()
		{
			if (g_fp.isInitialized == false)
			{
				g_fp.isInitialized = true;
				fpdPrepareRequest();
				fpdRequest->requestCode = iosu::fpd::IOSU_FPD_INITIALIZE;
				__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);
			}
		}




		void export_IsInitialized(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("Called nn_fp.IsInitialized LR %08x", hCPU->spr.LR);
			osLib_returnFromFunction(hCPU, g_fp.isInitialized ? 1 : 0);
		}

		void export_Initialize(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("Called nn_fp.Initialize LR %08x", hCPU->spr.LR);

			Initialize();

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_InitializeAdmin(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("Called nn_fp.InitializeAdmin LR %08x", hCPU->spr.LR);
			Initialize();
			g_fp.isAdminMode = true;
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_IsInitializedAdmin(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.IsInitializedAdmin()");
			osLib_returnFromFunction(hCPU, g_fp.isInitialized ? 1 : 0);
		}

		void export_SetNotificationHandler(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamU32(notificationMask, 0);
			ppcDefineParamMPTR(funcMPTR, 1);
			ppcDefineParamMPTR(customParam, 2);

			forceLogDebug_printf("nn_fp.SetNotificationHandler(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_SET_NOTIFICATION_HANDLER;
			fpdRequest->setNotificationHandler.notificationMask = notificationMask;
			fpdRequest->setNotificationHandler.funcPtr = funcMPTR;
			fpdRequest->setNotificationHandler.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_LoginAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMPTR(funcPtr, 0);
			ppcDefineParamMPTR(custom, 1);
			forceLogDebug_printf("nn_fp.LoginAsync(0x%08x,0x%08x)", funcPtr, custom);
			if (g_fp.isInitialized == false)
			{
				osLib_returnFromFunction(hCPU, 0xC0C00580);
				return;
			}
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_LOGIN_ASYNC;
			fpdRequest->loginAsync.funcPtr = funcPtr;
			fpdRequest->loginAsync.custom = custom;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);
			osLib_returnFromFunction(hCPU, 0);
		}

		void export_HasLoggedIn(PPCInterpreter_t* hCPU)
		{
			// Sonic All Star Racing needs this
			forceLogDebug_printf("nn_fp.HasLoggedIn()");
			osLib_returnFromFunction(hCPU, 1);
		}

		void export_IsOnline(PPCInterpreter_t* hCPU)
		{
			//forceLogDebug_printf("nn_fp.IsOnline();\n");
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_IS_ONLINE;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);
			forceLogDebug_printf("nn_fp.IsOnline() -> %d", fpdRequest->resultU32.u32);

			osLib_returnFromFunction(hCPU, fpdRequest->resultU32.u32);
		}

		void export_GetFriendList(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(pidList, uint32be, 0);
			ppcDefineParamMEMPTR(returnedCount, uint32be, 1);
			ppcDefineParamU32(startIndex, 2);
			ppcDefineParamU32(maxCount, 3);

			forceLogDebug_printf("nn_fp.GetFriendList(...)");
			//debug_printf("nn_fp.GetFriendList(0x%08x, 0x%08x, %d, %d)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_LIST;

			fpdRequest->getFriendList.pidList = pidList;
			fpdRequest->getFriendList.startIndex = startIndex;
			fpdRequest->getFriendList.maxCount = maxCount;

			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			*returnedCount = fpdRequest->resultU32.u32;

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendRequestList(PPCInterpreter_t* hCPU)
		{
			// GetFriendRequestList__Q2_2nn2fpFPUiT1UiT3
			ppcDefineParamMEMPTR(pidList, uint32be, 0);
			ppcDefineParamMEMPTR(returnedCount, uint32be, 1);
			ppcDefineParamU32(startIndex, 2);
			ppcDefineParamU32(maxCount, 3);

			forceLogDebug_printf("nn_fp.GetFriendRequestList(...)");
			//debug_printf("nn_fp.GetFriendList(0x%08x, 0x%08x, %d, %d)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIENDREQUEST_LIST;

			fpdRequest->getFriendList.pidList = pidList;
			fpdRequest->getFriendList.startIndex = startIndex;
			fpdRequest->getFriendList.maxCount = maxCount;

			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			*returnedCount = fpdRequest->resultU32.u32;

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendListAll(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(pidList, uint32be, 0);
			ppcDefineParamMEMPTR(returnedCount, uint32be, 1);
			ppcDefineParamU32(startIndex, 2);
			ppcDefineParamU32(maxCount, 3);

			forceLogDebug_printf("nn_fp.GetFriendListAll(...)");

			//debug_printf("nn_fp.GetFriendListAll(0x%08x, 0x%08x, %d, %d)\n", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6]);
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_LIST_ALL;

			fpdRequest->getFriendList.pidList = pidList;
			fpdRequest->getFriendList.startIndex = startIndex;
			fpdRequest->getFriendList.maxCount = maxCount;

			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			*returnedCount = fpdRequest->resultU32.u32;

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendListEx(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(friendData, iosu::fpd::friendData_t, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			forceLogDebug_printf("nn_fp.GetFriendListEx(...)");

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_LIST_EX;

			fpdRequest->getFriendListEx.friendData = friendData;
			fpdRequest->getFriendListEx.pidList = pidList;
			fpdRequest->getFriendListEx.count = count;

			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendRequestListEx(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(friendRequest, iosu::fpd::friendRequest_t, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			forceLogDebug_printf("nn_fp.GetFriendRequestListEx(...)");

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIENDREQUEST_LIST_EX;

			fpdRequest->getFriendRequestListEx.friendRequest = friendRequest;
			fpdRequest->getFriendRequestListEx.pidList = pidList;
			fpdRequest->getFriendRequestListEx.count = count;

			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetBasicInfoAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamMEMPTR(basicInfo, iosu::fpd::friendBasicInfo_t, 0);
			ppcDefineParamTypePtr(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);
			ppcDefineParamMPTR(funcMPTR, 3);
			ppcDefineParamU32(customParam, 4);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_BASIC_INFO_ASYNC;
			fpdRequest->getBasicInfo.basicInfo = basicInfo;
			fpdRequest->getBasicInfo.pidList = pidList;
			fpdRequest->getBasicInfo.count = count;
			fpdRequest->getBasicInfo.funcPtr = funcMPTR;
			fpdRequest->getBasicInfo.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);;

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetMyPrincipalId(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetMyPrincipalId()");

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_MY_PRINCIPAL_ID;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			uint32 principalId = fpdRequest->resultU32.u32;

			osLib_returnFromFunction(hCPU, principalId);
		}

		void export_GetMyAccountId(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetMyAccountId(0x%08x)", hCPU->gpr[3]);
			ppcDefineParamTypePtr(accountId, uint8, 0);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_MY_ACCOUNT_ID;
			fpdRequest->common.ptr = (void*)accountId;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetMyScreenName(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetMyScreenName(0x%08x)", hCPU->gpr[3]);
			ppcDefineParamTypePtr(screenname, uint16be, 0);

			screenname[0] = '\0';
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_MY_SCREENNAME;
			fpdRequest->common.ptr = (void*)screenname;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, 0);
		}

		typedef struct  
		{
			uint8 showOnline; // show online status to others
			uint8 showGame; // show played game to others
			uint8 blockFriendRequests; // block friend requests
		}fpPerference_t;

		void export_GetMyPreference(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetMyPreference(0x%08x) - placeholder", hCPU->gpr[3]);
			ppcDefineParamTypePtr(pref, fpPerference_t, 0);

			pref->showOnline = 1;
			pref->showGame = 1;
			pref->blockFriendRequests = 0;

			osLib_returnFromFunction(hCPU, 0);
		}

		// GetMyPreference__Q2_2nn2fpFPQ3_2nn2fp10Preference

		void export_GetMyMii(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetMyMii(0x%08x)", hCPU->gpr[3]);
			ppcDefineParamTypePtr(fflData, FFLData_t, 0);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_MY_MII;
			fpdRequest->common.ptr = (void*)fflData;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendAccountId(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetFriendAccountId(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamMEMPTR(accountIds, char, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_ACCOUNT_ID;
			fpdRequest->getFriendAccountId.accountIds = accountIds;
			fpdRequest->getFriendAccountId.pidList = pidList;
			fpdRequest->getFriendAccountId.count = count;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendScreenName(PPCInterpreter_t* hCPU)
		{
			// GetFriendScreenName__Q2_2nn2fpFPA11_wPCUiUibPUc
			forceLogDebug_printf("nn_fp.GetFriendScreenName(0x%08x,0x%08x,0x%08x,%d,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5], hCPU->gpr[6], hCPU->gpr[7]);
			ppcDefineParamMEMPTR(nameList, uint16be, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);
			ppcDefineParamU32(replaceNonAscii, 3);
			ppcDefineParamMEMPTR(languageList, uint8, 4);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_SCREENNAME;
			fpdRequest->getFriendScreenname.nameList = nameList;
			fpdRequest->getFriendScreenname.pidList = pidList;
			fpdRequest->getFriendScreenname.count = count;
			fpdRequest->getFriendScreenname.replaceNonAscii = replaceNonAscii != 0;
			fpdRequest->getFriendScreenname.languageList = languageList;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendMii(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetFriendMii(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamMEMPTR(miiList, FFLData_t, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_MII;
			fpdRequest->getFriendMii.miiList = (uint8*)miiList.GetPtr();
			fpdRequest->getFriendMii.pidList = pidList;
			fpdRequest->getFriendMii.count = count;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendPresence(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetFriendPresence(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamMEMPTR(presenceList, uint8, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_PRESENCE;
			fpdRequest->getFriendPresence.presenceList = (uint8*)presenceList.GetPtr();
			fpdRequest->getFriendPresence.pidList = pidList;
			fpdRequest->getFriendPresence.count = count;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_GetFriendRelationship(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.GetFriendRelationship(0x%08x,0x%08x,0x%08x)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamMEMPTR(relationshipList, uint8, 0);
			ppcDefineParamMEMPTR(pidList, uint32be, 1);
			ppcDefineParamU32(count, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_RELATIONSHIP;
			fpdRequest->getFriendRelationship.relationshipList = (uint8*)relationshipList.GetPtr();
			fpdRequest->getFriendRelationship.pidList = pidList;
			fpdRequest->getFriendRelationship.count = count;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_IsJoinable(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(presence, iosu::fpd::friendPresence_t, 0);
			ppcDefineParamU64(joinMask, 2);

			if (presence->isValid == 0 ||
				presence->isOnline == 0 ||
				presence->gameMode.joinGameId == 0 ||
				presence->gameMode.matchmakeType == 0 ||
				presence->gameMode.groupId == 0 ||
				presence->gameMode.joinGameMode >= 64 )
			{
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			uint32 joinGameMode = presence->gameMode.joinGameMode;
			uint64 joinModeMask = (1ULL<<joinGameMode);
			if ((joinModeMask&joinMask) == 0)
			{
				osLib_returnFromFunction(hCPU, 0);
				return;
			}

			// check relation ship
			uint32 joinFlagMask = presence->gameMode.joinFlagMask;
			if (joinFlagMask == 0)
			{
				osLib_returnFromFunction(hCPU, 0);
				return;
			}
			if (joinFlagMask == 1)
			{
				osLib_returnFromFunction(hCPU, 1);
				return;
			}
			if (joinFlagMask == 2)
			{
				//  check relationship
				uint8 relationship[1] = { 0 };
				StackAllocator<uint32be, 1> pidList;
				pidList[0] = presence->gameMode.hostPid;
				fpdPrepareRequest();
				fpdRequest->requestCode = iosu::fpd::IOSU_FPD_GET_FRIEND_RELATIONSHIP;
				fpdRequest->getFriendRelationship.relationshipList = relationship;
				fpdRequest->getFriendRelationship.pidList = pidList.GetPointer();
				fpdRequest->getFriendRelationship.count = 1;
				__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

				if(relationship[0] == iosu::fpd::RELATIONSHIP_FRIEND)
					osLib_returnFromFunction(hCPU, 1);
				else
					osLib_returnFromFunction(hCPU, 0);
				return;
			}
			if (joinFlagMask == 0x65 || joinFlagMask == 0x66)
			{
				forceLog_printf("Unsupported friend invite");
			}

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_CheckSettingStatusAsync(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.CheckSettingStatusAsync(0x%08x,0x%08x,0x%08x) - placeholder", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamTypePtr(uknR3, uint8, 0);
			ppcDefineParamMPTR(funcMPTR, 1);
			ppcDefineParamU32(customParam, 2);

			if (g_fp.isAdminMode == false)
			{

				osLib_returnFromFunction(hCPU, 0xC0C00800);
				return;
			}

			*uknR3 = 1;

			StackAllocator<uint32be> callbackResultCode;

			*callbackResultCode.GetPointer() = 0;

			hCPU->gpr[3] = callbackResultCode.GetMPTR();
			hCPU->gpr[4] = customParam;
			PPCCore_executeCallbackInternal(funcMPTR);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_IsPreferenceValid(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.IsPreferenceValid()");
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_IS_PREFERENCE_VALID;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->resultU32.u32);
		}

		void export_UpdatePreferenceAsync(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.UpdatePreferenceAsync(0x%08x,0x%08x,0x%08x) - placeholder", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamTypePtr(uknR3, uint8, 0);
			ppcDefineParamMPTR(funcMPTR, 1);
			ppcDefineParamU32(customParam, 2);

			if (g_fp.isAdminMode == false)
			{

				osLib_returnFromFunction(hCPU, 0xC0C00800);
				return;
			}

			//*uknR3 = 0;	// seems to be 3 bytes (nn::fp::Preference const *)

			StackAllocator<uint32be> callbackResultCode;

			*callbackResultCode.GetPointer() = 0;

			hCPU->gpr[3] = callbackResultCode.GetMPTR();
			hCPU->gpr[4] = customParam;
			PPCCore_executeCallbackInternal(funcMPTR);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_UpdateGameMode(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.UpdateGameMode(0x%08x,0x%08x,%d)", hCPU->gpr[3], hCPU->gpr[4], hCPU->gpr[5]);
			ppcDefineParamMEMPTR(gameMode, iosu::fpd::gameMode_t, 0);
			ppcDefineParamMEMPTR(gameModeMessage, uint16be, 1);
			ppcDefineParamU32(uknR5, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_UPDATE_GAMEMODE;
			fpdRequest->updateGameMode.gameMode = gameMode;
			fpdRequest->updateGameMode.gameModeMessage = gameModeMessage;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_GetRequestBlockSettingAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(settingList, uint8, 0);
			ppcDefineParamTypePtr(pidList, uint32be, 1);
			ppcDefineParamU32(pidCount, 2);
			ppcDefineParamMPTR(funcMPTR, 3);
			ppcDefineParamMPTR(customParam, 4);

			forceLogDebug_printf("GetRequestBlockSettingAsync(...) - todo");

			for (uint32 i = 0; i < pidCount; i++)
				settingList[i] = 0;
			// 0 means not blocked. Friend app will continue with GetBasicInformation()
			// 1 means blocked. Friend app will continue with AddFriendAsync to add the user as a provisional friend
			
			StackAllocator<uint32be> callbackResultCode;

			*callbackResultCode.GetPointer() = 0;

			hCPU->gpr[3] = callbackResultCode.GetMPTR();
			hCPU->gpr[4] = customParam;
			PPCCore_executeCallbackInternal(funcMPTR);

			osLib_returnFromFunction(hCPU, 0);
		}

		void export_AddFriendAsync(PPCInterpreter_t* hCPU)
		{
			// AddFriendAsync__Q2_2nn2fpFPCcPFQ2_2nn6ResultPv_vPv
			ppcDefineParamU32(principalId, 0);
			ppcDefineParamMPTR(funcMPTR, 1);
			ppcDefineParamMPTR(customParam, 2);

#ifdef CEMU_DEBUG_ASSERT
			assert_dbg();
#endif
			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_ADD_FRIEND;
			fpdRequest->addOrRemoveFriend.pid = principalId;
			fpdRequest->addOrRemoveFriend.funcPtr = funcMPTR;
			fpdRequest->addOrRemoveFriend.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}


		void export_DeleteFriendFlagsAsync(PPCInterpreter_t* hCPU)
		{
			forceLogDebug_printf("nn_fp.DeleteFriendFlagsAsync(...) - todo");
			ppcDefineParamU32(uknR3, 0); // example value: pointer
			ppcDefineParamU32(uknR4, 1); // example value: 1
			ppcDefineParamU32(uknR5, 2); // example value: 1
			ppcDefineParamMPTR(funcMPTR, 3);
			ppcDefineParamU32(customParam, 4);

			if (g_fp.isAdminMode == false)
			{
				osLib_returnFromFunction(hCPU, 0xC0C00800);
				return;
			}

			StackAllocator<uint32be> callbackResultCode;

			*callbackResultCode.GetPointer() = 0;

			hCPU->gpr[3] = callbackResultCode.GetMPTR();
			hCPU->gpr[4] = customParam;
			PPCCore_executeCallbackInternal(funcMPTR);

			osLib_returnFromFunction(hCPU, 0);
		}


		typedef struct  
		{
			/* +0x00 */ uint32be pid;
			/* +0x04 */ uint8 ukn04;
			/* +0x05 */ uint8 ukn05;
			/* +0x06 */ uint8 ukn06[0x22];
			/* +0x28 */ uint8 ukn28[0x22];
			/* +0x4A */ uint8 _uknOrPadding4A[6];
			/* +0x50 */ uint32be ukn50;
			/* +0x54 */ uint32be ukn54;
			/* +0x58 */ uint16be ukn58;
			/* +0x5C */ uint8 _padding5C[4];
			/* +0x60 */ iosu::fpd::fpdDate_t date;
		}RecentPlayRecordEx_t;

		static_assert(sizeof(RecentPlayRecordEx_t) == 0x68, "");
		static_assert(offsetof(RecentPlayRecordEx_t, ukn06) == 0x06, "");
		static_assert(offsetof(RecentPlayRecordEx_t, ukn50) == 0x50, "");

		void export_AddFriendRequestAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(playRecord, RecentPlayRecordEx_t, 0);
			ppcDefineParamTypePtr(message, uint16be, 1);
			ppcDefineParamMPTR(funcMPTR, 2);
			ppcDefineParamMPTR(customParam, 3);

			fpdPrepareRequest();

			uint8* uknData = (uint8*)playRecord;

			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_ADD_FRIEND_REQUEST;
			fpdRequest->addFriendRequest.pid = playRecord->pid;
			fpdRequest->addFriendRequest.message = message;
			fpdRequest->addFriendRequest.funcPtr = funcMPTR;
			fpdRequest->addFriendRequest.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_RemoveFriendAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamU32(principalId, 0);
			ppcDefineParamMPTR(funcMPTR, 1);
			ppcDefineParamMPTR(customParam, 2);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_REMOVE_FRIEND_ASYNC;
			fpdRequest->addOrRemoveFriend.pid = principalId;
			fpdRequest->addOrRemoveFriend.funcPtr = funcMPTR;
			fpdRequest->addOrRemoveFriend.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_MarkFriendRequestsAsReceivedAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamTypePtr(messageIdList, uint64, 0);
			ppcDefineParamU32(count, 1);
			ppcDefineParamMPTR(funcMPTR, 2);
			ppcDefineParamMPTR(customParam, 3);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_MARK_FRIEND_REQUEST_AS_RECEIVED_ASYNC;
			fpdRequest->markFriendRequest.messageIdList = messageIdList;
			fpdRequest->markFriendRequest.count = count;
			fpdRequest->markFriendRequest.funcPtr = funcMPTR;
			fpdRequest->markFriendRequest.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_CancelFriendRequestAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamU64(frqMessageId, 0);
			ppcDefineParamMPTR(funcMPTR, 2);
			ppcDefineParamMPTR(customParam, 3);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_CANCEL_FRIEND_REQUEST_ASYNC;
			fpdRequest->cancelOrAcceptFriendRequest.messageId = frqMessageId;
			fpdRequest->cancelOrAcceptFriendRequest.funcPtr = funcMPTR;
			fpdRequest->cancelOrAcceptFriendRequest.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void export_AcceptFriendRequestAsync(PPCInterpreter_t* hCPU)
		{
			ppcDefineParamU64(frqMessageId, 0);
			ppcDefineParamMPTR(funcMPTR, 2);
			ppcDefineParamMPTR(customParam, 3);

			fpdPrepareRequest();
			fpdRequest->requestCode = iosu::fpd::IOSU_FPD_ACCEPT_FRIEND_REQUEST_ASYNC;
			fpdRequest->cancelOrAcceptFriendRequest.messageId = frqMessageId;
			fpdRequest->cancelOrAcceptFriendRequest.funcPtr = funcMPTR;
			fpdRequest->cancelOrAcceptFriendRequest.custom = customParam;
			__depr__IOS_Ioctlv(IOS_DEVICE_FPD, IOSU_FPD_REQUEST_CEMU, 1, 1, fpdBufferVector);

			osLib_returnFromFunction(hCPU, fpdRequest->returnCode);
		}

		void load()
		{
			osLib_addFunction("nn_fp", "Initialize__Q2_2nn2fpFv", export_Initialize);
			osLib_addFunction("nn_fp", "InitializeAdmin__Q2_2nn2fpFv", export_InitializeAdmin);
			osLib_addFunction("nn_fp", "IsInitialized__Q2_2nn2fpFv", export_IsInitialized);
			osLib_addFunction("nn_fp", "IsInitializedAdmin__Q2_2nn2fpFv", export_IsInitializedAdmin);

			osLib_addFunction("nn_fp", "SetNotificationHandler__Q2_2nn2fpFUiPFQ3_2nn2fp16NotificationTypeUiPv_vPv", export_SetNotificationHandler);

			osLib_addFunction("nn_fp", "LoginAsync__Q2_2nn2fpFPFQ2_2nn6ResultPv_vPv", export_LoginAsync);
			osLib_addFunction("nn_fp", "HasLoggedIn__Q2_2nn2fpFv", export_HasLoggedIn);

			osLib_addFunction("nn_fp", "IsOnline__Q2_2nn2fpFv", export_IsOnline);

			osLib_addFunction("nn_fp", "GetFriendList__Q2_2nn2fpFPUiT1UiT3", export_GetFriendList);
			osLib_addFunction("nn_fp", "GetFriendRequestList__Q2_2nn2fpFPUiT1UiT3", export_GetFriendRequestList);
			osLib_addFunction("nn_fp", "GetFriendListAll__Q2_2nn2fpFPUiT1UiT3", export_GetFriendListAll);
			osLib_addFunction("nn_fp", "GetFriendListEx__Q2_2nn2fpFPQ3_2nn2fp10FriendDataPCUiUi", export_GetFriendListEx);
			osLib_addFunction("nn_fp", "GetFriendRequestListEx__Q2_2nn2fpFPQ3_2nn2fp13FriendRequestPCUiUi", export_GetFriendRequestListEx);
			osLib_addFunction("nn_fp", "GetBasicInfoAsync__Q2_2nn2fpFPQ3_2nn2fp9BasicInfoPCUiUiPFQ2_2nn6ResultPv_vPv", export_GetBasicInfoAsync);

			osLib_addFunction("nn_fp", "GetMyPrincipalId__Q2_2nn2fpFv", export_GetMyPrincipalId);
			osLib_addFunction("nn_fp", "GetMyAccountId__Q2_2nn2fpFPc", export_GetMyAccountId);
			osLib_addFunction("nn_fp", "GetMyScreenName__Q2_2nn2fpFPw", export_GetMyScreenName);
			osLib_addFunction("nn_fp", "GetMyMii__Q2_2nn2fpFP12FFLStoreData", export_GetMyMii);
			osLib_addFunction("nn_fp", "GetMyPreference__Q2_2nn2fpFPQ3_2nn2fp10Preference", export_GetMyPreference);

			osLib_addFunction("nn_fp", "GetFriendAccountId__Q2_2nn2fpFPA17_cPCUiUi", export_GetFriendAccountId);
			osLib_addFunction("nn_fp", "GetFriendScreenName__Q2_2nn2fpFPA11_wPCUiUibPUc", export_GetFriendScreenName);
			osLib_addFunction("nn_fp", "GetFriendMii__Q2_2nn2fpFP12FFLStoreDataPCUiUi", export_GetFriendMii);
			osLib_addFunction("nn_fp", "GetFriendPresence__Q2_2nn2fpFPQ3_2nn2fp14FriendPresencePCUiUi", export_GetFriendPresence);
			osLib_addFunction("nn_fp", "GetFriendRelationship__Q2_2nn2fpFPUcPCUiUi", export_GetFriendRelationship);
			osLib_addFunction("nn_fp", "IsJoinable__Q2_2nn2fpFPCQ3_2nn2fp14FriendPresenceUL", export_IsJoinable);

			osLib_addFunction("nn_fp", "CheckSettingStatusAsync__Q2_2nn2fpFPUcPFQ2_2nn6ResultPv_vPv", export_CheckSettingStatusAsync);
			osLib_addFunction("nn_fp", "IsPreferenceValid__Q2_2nn2fpFv", export_IsPreferenceValid);
			osLib_addFunction("nn_fp", "UpdatePreferenceAsync__Q2_2nn2fpFPCQ3_2nn2fp10PreferencePFQ2_2nn6ResultPv_vPv", export_UpdatePreferenceAsync);

			osLib_addFunction("nn_fp", "UpdateGameMode__Q2_2nn2fpFPCQ3_2nn2fp8GameModePCwUi", export_UpdateGameMode);

			osLib_addFunction("nn_fp", "GetRequestBlockSettingAsync__Q2_2nn2fpFPUcPCUiUiPFQ2_2nn6ResultPv_vPv", export_GetRequestBlockSettingAsync);

			osLib_addFunction("nn_fp", "AddFriendAsync__Q2_2nn2fpFPCcPFQ2_2nn6ResultPv_vPv", export_AddFriendAsync);
			osLib_addFunction("nn_fp", "AddFriendRequestAsync__Q2_2nn2fpFPCQ3_2nn2fp18RecentPlayRecordExPCwPFQ2_2nn6ResultPv_vPv", export_AddFriendRequestAsync);
			osLib_addFunction("nn_fp", "DeleteFriendFlagsAsync__Q2_2nn2fpFPCUiUiT2PFQ2_2nn6ResultPv_vPv", export_DeleteFriendFlagsAsync);

			osLib_addFunction("nn_fp", "RemoveFriendAsync__Q2_2nn2fpFUiPFQ2_2nn6ResultPv_vPv", export_RemoveFriendAsync);
			osLib_addFunction("nn_fp", "MarkFriendRequestsAsReceivedAsync__Q2_2nn2fpFPCULUiPFQ2_2nn6ResultPv_vPv", export_MarkFriendRequestsAsReceivedAsync);
			osLib_addFunction("nn_fp", "CancelFriendRequestAsync__Q2_2nn2fpFULPFQ2_2nn6ResultPv_vPv", export_CancelFriendRequestAsync);
			osLib_addFunction("nn_fp", "AcceptFriendRequestAsync__Q2_2nn2fpFULPFQ2_2nn6ResultPv_vPv", export_AcceptFriendRequestAsync);

		}
	}
}

