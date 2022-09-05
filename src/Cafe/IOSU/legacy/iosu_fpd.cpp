#include "iosu_ioctl.h"
#include "iosu_act.h"
#include "iosu_fpd.h"
#include "Cemu/nex/nex.h"
#include "Cemu/nex/nexFriends.h"
#include "util/helpers/helpers.h"
#include "config/CemuConfig.h"
#include "Cafe/CafeSystem.h"
#include "config/ActiveSettings.h"
#include "Cemu/napi/napi.h"
#include "util/helpers/StringHelpers.h"
#include "Cafe/OS/libs/coreinit/coreinit.h"

uint32 memory_getVirtualOffsetFromPointer(void* ptr); // remove once we updated everything to MEMPTR

SysAllocator<uint32be> _fpdAsyncLoginRetCode;
SysAllocator<uint32be> _fpdAsyncAddFriendRetCode;

std::mutex g_friend_notification_mutex;
std::vector< std::pair<std::string, int> > g_friend_notifications;

namespace iosu
{
	namespace fpd
	{

		struct  
		{
			bool isThreadStarted;
			bool isInitialized2;
			NexFriends* nexFriendSession;
			// notification handler
			MPTR notificationFunc;
			MPTR notificationCustomParam;
			uint32 notificationMask;
			// login callback
			struct  
			{
				MPTR func;
				MPTR customParam;
			}asyncLoginCallback;
			// current state
			nexPresenceV2 myPresence;
		}g_fpd = {};

		void notificationHandler(NexFriends::NOTIFICATION_TYPE type, uint32 pid)
		{
			forceLogDebug_printf("Friends::Notification %02x pid %08x", type, pid);
			if(GetConfig().notification.friends)
			{
				std::unique_lock lock(g_friend_notification_mutex);
				std::string message;
				if(type == NexFriends::NOTIFICATION_TYPE::NOTIFICATION_TYPE_ONLINE)
				{
					g_friend_notifications.emplace_back("Connected to friend service", 5000);
					if(g_fpd.nexFriendSession && g_fpd.nexFriendSession->getPendingFriendRequestCount() > 0)
						g_friend_notifications.emplace_back(fmt::format("You have {} pending friend request(s)", g_fpd.nexFriendSession->getPendingFriendRequestCount()), 5000);
				}
				else
				{
					std::string msg_format;
					switch(type)
					{
					case NexFriends::NOTIFICATION_TYPE_ONLINE: break;
					case NexFriends::NOTIFICATION_TYPE_FRIEND_LOGIN: msg_format = "{} is now online"; break;
					case NexFriends::NOTIFICATION_TYPE_FRIEND_LOGOFF: msg_format = "{} is now offline"; break;
					case NexFriends::NOTIFICATION_TYPE_FRIEND_PRESENCE_CHANGE: break;
					case NexFriends::NOTIFICATION_TYPE_ADDED_FRIEND: msg_format = "{} has been added to your friend list"; break;
					case NexFriends::NOTIFICATION_TYPE_REMOVED_FRIEND: msg_format = "{} has been removed from your friend list"; break;
					case NexFriends::NOTIFICATION_TYPE_ADDED_OUTGOING_REQUEST: break;
					case NexFriends::NOTIFICATION_TYPE_REMOVED_OUTGOING_REQUEST: break;
					case NexFriends::NOTIFICATION_TYPE_ADDED_INCOMING_REQUEST: msg_format = "{} wants to add you to his friend list"; break;
					case NexFriends::NOTIFICATION_TYPE_REMOVED_INCOMING_REQUEST: break;
					default: ;
					}
					
					if (!msg_format.empty())
					{
						std::string name = fmt::format("{:#x}", pid);
						if (g_fpd.nexFriendSession)
						{
							const std::string tmp = g_fpd.nexFriendSession->getAccountNameByPid(pid);
							if (!tmp.empty())
								name = tmp;
						}

						g_friend_notifications.emplace_back(fmt::format(fmt::runtime(msg_format), name), 5000);
					}
				}
			}

			if (g_fpd.notificationFunc == MPTR_NULL)
				return;
			uint32 notificationFlag = 1 << (type - 1);
			if ( (notificationFlag&g_fpd.notificationMask) == 0 )
				return;
			coreinitAsyncCallback_add(g_fpd.notificationFunc, 3, type, pid, g_fpd.notificationCustomParam);
		}

		void convertMultiByteStringToBigEndianWidechar(const char* input, uint16be* output, sint32 maxOutputLength)
		{
			std::basic_string<uint16be> beStr = StringHelpers::FromUtf8(input);
			if (beStr.size() >= maxOutputLength - 1)
				beStr.resize(maxOutputLength-1);
			for (size_t i = 0; i < beStr.size(); i++)
				output[i] = beStr[i];
			output[beStr.size()] = '\0';
		}

		void convertFPDTimestampToDate(uint64 timestamp, fpdDate_t* fpdDate)
		{
			// if the timestamp is zero then still return a valid date
			if (timestamp == 0)
			{
				fpdDate->second = 0;
				fpdDate->minute = 0;
				fpdDate->hour = 0;
				fpdDate->day = 1;
				fpdDate->month = 1;
				fpdDate->year = 1970;
				return;
			}
			fpdDate->second = (uint8)((timestamp) & 0x3F);
			fpdDate->minute = (uint8)((timestamp >> 6) & 0x3F);
			fpdDate->hour = (uint8)((timestamp >> 12) & 0x1F);
			fpdDate->day = (uint8)((timestamp >> 17) & 0x1F);
			fpdDate->month = (uint8)((timestamp >> 22) & 0xF);
			fpdDate->year = (uint16)((timestamp >> 26));
		}

		uint64 convertDateToFPDTimestamp(fpdDate_t* fpdDate)
		{
			uint64 t = 0;
			t |= (uint64)fpdDate->second;
			t |= ((uint64)fpdDate->minute<<6);
			t |= ((uint64)fpdDate->hour<<12);
			t |= ((uint64)fpdDate->day<<17);
			t |= ((uint64)fpdDate->month<<22);
			t |= ((uint64)(uint16)fpdDate->year<<26);
			return t;
		}

		void startFriendSession()
		{
			cemu_assert(!g_fpd.nexFriendSession);

			NAPI::AuthInfo authInfo;
			NAPI::NAPI_MakeAuthInfoFromCurrentAccount(authInfo);
			NAPI::ACTGetNexTokenResult nexTokenResult = NAPI::ACT_GetNexToken_WithCache(authInfo, 0x0005001010001C00, 0x0000, 0x00003200);
			if (nexTokenResult.isValid())
			{
				// get values needed for friend session
				uint32 myPid;
				uint8 currentSlot = iosu::act::getCurrentAccountSlot();
				iosu::act::getPrincipalId(currentSlot, &myPid);
				char accountId[256] = { 0 };
				iosu::act::getAccountId(currentSlot, accountId);
				FFLData_t miiData;
				act::getMii(currentSlot, &miiData);
				uint16 screenName[ACT_NICKNAME_LENGTH + 1] = { 0 };
				act::getScreenname(currentSlot, screenName);
				uint32 countryCode = 0;
				act::getCountryIndex(currentSlot, &countryCode);
				// init presence
				g_fpd.myPresence.isOnline = 1;
				g_fpd.myPresence.gameKey.titleId = CafeSystem::GetForegroundTitleId();
				g_fpd.myPresence.gameKey.ukn = CafeSystem::GetForegroundTitleVersion();
				// start session
				uint32 hostIp;
				inet_pton(AF_INET, nexTokenResult.nexToken.host, &hostIp);
				g_fpd.nexFriendSession = new NexFriends(hostIp, nexTokenResult.nexToken.port, "ridfebb9", myPid, nexTokenResult.nexToken.nexPassword, nexTokenResult.nexToken.token, accountId, (uint8*)&miiData, (wchar_t*)screenName, (uint8)countryCode, g_fpd.myPresence);
				g_fpd.nexFriendSession->setNotificationHandler(notificationHandler);
				forceLog_printf("IOSU_FPD: Created friend server session");
			}
			else
			{
				forceLogDebug_printf("IOSU_FPD: Failed to acquire nex token for friend server");
			}
		}

		void handleRequest_GetFriendList(iosuFpdCemuRequest_t* fpdCemuRequest, bool getAll)
		{
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0;
				fpdCemuRequest->resultU32.u32 = 0; // zero entries returned
				return;
			}

			uint32 temporaryPidList[800];
			uint32 pidCount = 0;
			g_fpd.nexFriendSession->getFriendPIDs(temporaryPidList, &pidCount, fpdCemuRequest->getFriendList.startIndex, std::min<uint32>(sizeof(temporaryPidList) / sizeof(temporaryPidList[0]), fpdCemuRequest->getFriendList.maxCount), getAll);
			uint32be* pidListOutput = fpdCemuRequest->getFriendList.pidList.GetPtr();
			if (pidListOutput)
			{
				for (uint32 i = 0; i < pidCount; i++)
					pidListOutput[i] = temporaryPidList[i];
			}
			fpdCemuRequest->returnCode = 0;
			fpdCemuRequest->resultU32.u32 = pidCount;
		}

		void handleRequest_GetFriendRequestList(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0;
				fpdCemuRequest->resultU32.u32 = 0; // zero entries returned
				return;
			}

			uint32 temporaryPidList[800];
			uint32 pidCount = 0;
			// get only incoming friend requests
			g_fpd.nexFriendSession->getFriendRequestPIDs(temporaryPidList, &pidCount, fpdCemuRequest->getFriendList.startIndex, std::min<uint32>(sizeof(temporaryPidList) / sizeof(temporaryPidList[0]), fpdCemuRequest->getFriendList.maxCount), true, false);
			uint32be* pidListOutput = fpdCemuRequest->getFriendList.pidList.GetPtr();
			if (pidListOutput)
			{
				for (uint32 i = 0; i < pidCount; i++)
					pidListOutput[i] = temporaryPidList[i];
			}
			fpdCemuRequest->returnCode = 0;
			fpdCemuRequest->resultU32.u32 = pidCount;
		}

		void setFriendDataFromNexFriend(friendData_t* friendData, nexFriend* frd)
		{
			memset(friendData, 0, sizeof(friendData_t));
			// setup friend data
			friendData->type = 1; // friend
			friendData->pid = frd->nnaInfo.principalInfo.principalId;
			memcpy(friendData->mii, frd->nnaInfo.principalInfo.mii.miiData, FFL_SIZE);
			strcpy((char*)friendData->nnid, frd->nnaInfo.principalInfo.nnid);
			// screenname
			convertMultiByteStringToBigEndianWidechar(frd->nnaInfo.principalInfo.mii.miiNickname, friendData->screenname, sizeof(friendData->screenname) / sizeof(uint16be));

			//friendData->friendExtraData.ukn0E4 = 0;
			friendData->friendExtraData.isOnline = frd->presence.isOnline != 0 ? 1 : 0;

			friendData->friendExtraData.gameKeyTitleId = _swapEndianU64(frd->presence.gameKey.titleId);
			friendData->friendExtraData.gameKeyUkn = frd->presence.gameKey.ukn;

			friendData->friendExtraData.statusMessage[0] = '\0';

			// set valid dates
			friendData->uknDate.year = 2018;
			friendData->uknDate.day = 1;
			friendData->uknDate.month = 1;
			friendData->uknDate.hour = 1;
			friendData->uknDate.minute = 1;
			friendData->uknDate.second = 1;

			friendData->friendExtraData.uknDate218.year = 2018;
			friendData->friendExtraData.uknDate218.day = 1;
			friendData->friendExtraData.uknDate218.month = 1;
			friendData->friendExtraData.uknDate218.hour = 1;
			friendData->friendExtraData.uknDate218.minute = 1;
			friendData->friendExtraData.uknDate218.second = 1;

			convertFPDTimestampToDate(frd->lastOnlineTimestamp, &friendData->friendExtraData.lastOnline);
		}

		void setFriendDataFromNexFriendRequest(friendData_t* friendData, nexFriendRequest* frdReq, bool isIncoming)
		{
			memset(friendData, 0, sizeof(friendData_t));
			// setup friend data
			friendData->type = 0; // friend request
			friendData->pid = frdReq->principalInfo.principalId;
			memcpy(friendData->mii, frdReq->principalInfo.mii.miiData, FFL_SIZE);
			strcpy((char*)friendData->nnid, frdReq->principalInfo.nnid);
			// screenname
			convertMultiByteStringToBigEndianWidechar(frdReq->principalInfo.mii.miiNickname, friendData->screenname, sizeof(friendData->screenname) / sizeof(uint16be));

			convertMultiByteStringToBigEndianWidechar(frdReq->message.commentStr.c_str(), friendData->requestExtraData.comment, sizeof(friendData->requestExtraData.comment) / sizeof(uint16be));

			fpdDate_t expireDate;
			convertFPDTimestampToDate(frdReq->message.expireTimestamp, &expireDate);

			bool isProvisional = frdReq->message.expireTimestamp == 0;

			//friendData->requestExtraData.ukn0A8 = 0; // no change?
			//friendData->requestExtraData.ukn0A0 = 0; // if not set -> provisional friend request
			//friendData->requestExtraData.ukn0A4 = isProvisional ? 0 : 123; // no change?

			friendData->requestExtraData.messageId = _swapEndianU64(frdReq->message.messageId);
			

			//find the value for 'markedAsReceived'

			///* +0x0A8 */ uint8 ukn0A8;
			///* +0x0A9 */ uint8 ukn0A9; // comment language? (guessed)
			///* +0x0AA */ uint16be comment[0x40];
			///* +0x12A */ uint8 ukn12A; // ingame name language? (guessed)
			///* +0x12B */ uint8 _padding12B;

			// set valid dates

			friendData->uknDate.year = 2018;
			friendData->uknDate.day = 20;
			friendData->uknDate.month = 4;
			friendData->uknDate.hour = 12;
			friendData->uknDate.minute = 1;
			friendData->uknDate.second = 1;

			friendData->requestExtraData.uknData0.year = 2018;
			friendData->requestExtraData.uknData0.day = 24;
			friendData->requestExtraData.uknData0.month = 4;
			friendData->requestExtraData.uknData0.hour = 1;
			friendData->requestExtraData.uknData0.minute = 1;
			friendData->requestExtraData.uknData0.second = 1;

			// this is the date used for 'Expires in'
			convertFPDTimestampToDate(frdReq->message.expireTimestamp, &friendData->requestExtraData.uknData1);
		}

		void setFriendRequestFromNexFriendRequest(friendRequest_t* friendRequest, nexFriendRequest* frdReq, bool isIncoming)
		{
			memset(friendRequest, 0, sizeof(friendRequest_t));

			friendRequest->pid = frdReq->principalInfo.principalId;
			
			strncpy((char*)friendRequest->nnid, frdReq->principalInfo.nnid, sizeof(friendRequest->nnid));
			friendRequest->nnid[sizeof(friendRequest->nnid) - 1] = '\0';

			memcpy(friendRequest->miiData, frdReq->principalInfo.mii.miiData, sizeof(friendRequest->miiData));

			convertMultiByteStringToBigEndianWidechar(frdReq->message.commentStr.c_str(), friendRequest->message, sizeof(friendRequest->message) / sizeof(friendRequest->message[0]));
			convertMultiByteStringToBigEndianWidechar(frdReq->principalInfo.mii.miiNickname, friendRequest->screenname, sizeof(friendRequest->screenname) / sizeof(friendRequest->screenname[0]));

			friendRequest->isMarkedAsReceived = 1;

			friendRequest->ukn98 = _swapEndianU64(frdReq->message.messageId);

			convertFPDTimestampToDate(0, &friendRequest->uknDate);
			convertFPDTimestampToDate(0, &friendRequest->uknDate2);
			convertFPDTimestampToDate(frdReq->message.expireTimestamp, &friendRequest->expireDate);
		}

		void handleRequest_GetFriendListEx(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			for (uint32 i = 0; i < fpdCemuRequest->getFriendListEx.count; i++)
			{
				uint32 pid = fpdCemuRequest->getFriendListEx.pidList[i];
				friendData_t* friendData = fpdCemuRequest->getFriendListEx.friendData.GetPtr() + i;
				nexFriend frd;
				nexFriendRequest frdReq;
				if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
				{
					setFriendDataFromNexFriend(friendData, &frd);
					continue;
				}
				bool incoming = false;
				if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
				{
					setFriendDataFromNexFriendRequest(friendData, &frdReq, incoming);
					continue;
				}
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
		}

		void handleRequest_GetFriendRequestListEx(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			for (uint32 i = 0; i < fpdCemuRequest->getFriendRequestListEx.count; i++)
			{
				uint32 pid = fpdCemuRequest->getFriendListEx.pidList[i];
				friendRequest_t* friendRequest = fpdCemuRequest->getFriendRequestListEx.friendRequest.GetPtr() + i;
				nexFriendRequest frdReq;
				bool incoming = false;
				if (!g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
				{
					cemuLog_log(LogType::Force, "Failed to get friend request");
					fpdCemuRequest->returnCode = 0x80000000;
					return;
				}
				setFriendRequestFromNexFriendRequest(friendRequest, &frdReq, incoming);
			}
		}

		typedef struct  
		{
			MPTR funcMPTR;
			MPTR customParam;
		}fpAsyncCallback_t;

		typedef struct 
		{
			nexPrincipalBasicInfo* principalBasicInfo;
			uint32* pidList;
			sint32 count;
			friendBasicInfo_t* friendBasicInfo;
			fpAsyncCallback_t fpCallback;
		}getBasicInfoAsyncParams_t;

		SysAllocator<uint32, 32> _fpCallbackResultArray; // use a ring buffer of results to avoid overwriting the result when multiple callbacks are queued at the same time
		sint32 fpCallbackResultIndex = 0;

		void handleFPCallback(fpAsyncCallback_t* fpCallback, uint32 resultCode)
		{
			fpCallbackResultIndex = (fpCallbackResultIndex + 1) % 32;
			uint32* resultPtr = _fpCallbackResultArray.GetPtr() + fpCallbackResultIndex;

			*resultPtr = resultCode;

			coreinitAsyncCallback_add(fpCallback->funcMPTR, 2, memory_getVirtualOffsetFromPointer(resultPtr), fpCallback->customParam);
		}

		void handleFPCallback2(MPTR funcMPTR, MPTR custom, uint32 resultCode)
		{
			fpCallbackResultIndex = (fpCallbackResultIndex + 1) % 32;
			uint32* resultPtr = _fpCallbackResultArray.GetPtr() + fpCallbackResultIndex;

			*resultPtr = resultCode;

			coreinitAsyncCallback_add(funcMPTR, 2, memory_getVirtualOffsetFromPointer(resultPtr), custom);
		}

		void handleResultCB_GetBasicInfoAsync(NexFriends* nexFriends, uint32 result, void* custom)
		{
			getBasicInfoAsyncParams_t* cbInfo = (getBasicInfoAsyncParams_t*)custom;
			if (result != 0)
			{
				handleFPCallback(&cbInfo->fpCallback, 0x80000000); // todo - properly translate internal error to nn::fp error code
				free(cbInfo->principalBasicInfo);
				free(cbInfo->pidList);
				free(cbInfo);
				return;
			}

			// convert PrincipalBasicInfo into friendBasicInfo
			for (sint32 i = 0; i < cbInfo->count; i++)
			{
				friendBasicInfo_t* basicInfo = cbInfo->friendBasicInfo + i;
				nexPrincipalBasicInfo* principalBasicInfo = cbInfo->principalBasicInfo + i;

				memset(basicInfo, 0, sizeof(friendBasicInfo_t));
				basicInfo->pid = principalBasicInfo->principalId;
				strcpy(basicInfo->nnid, principalBasicInfo->nnid);

				convertMultiByteStringToBigEndianWidechar(principalBasicInfo->mii.miiNickname, basicInfo->screenname, sizeof(basicInfo->screenname) / sizeof(uint16be));
				memcpy(basicInfo->miiData, principalBasicInfo->mii.miiData, FFL_SIZE);

				basicInfo->uknDate90.day = 1;
				basicInfo->uknDate90.month = 1;
				basicInfo->uknDate90.hour = 1;
				basicInfo->uknDate90.minute = 1;
				basicInfo->uknDate90.second = 1;

				// unknown values not set:
				// ukn15
				// ukn2E
				// ukn2F
			}
			// success
			handleFPCallback(&cbInfo->fpCallback, 0x00000000);

			free(cbInfo->principalBasicInfo);
			free(cbInfo->pidList);
			free(cbInfo);
		}

		void handleRequest_GetBasicInfoAsync(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			sint32 count = fpdCemuRequest->getBasicInfo.count;

			nexPrincipalBasicInfo* principalBasicInfo = new nexPrincipalBasicInfo[count];
			uint32* pidList = (uint32*)malloc(sizeof(uint32)*count);
			for (sint32 i = 0; i < count; i++)
				pidList[i] = fpdCemuRequest->getBasicInfo.pidList[i];
			getBasicInfoAsyncParams_t* cbInfo = (getBasicInfoAsyncParams_t*)malloc(sizeof(getBasicInfoAsyncParams_t));
			cbInfo->principalBasicInfo = principalBasicInfo;
			cbInfo->pidList = pidList;
			cbInfo->count = count;
			cbInfo->friendBasicInfo = fpdCemuRequest->getBasicInfo.basicInfo.GetPtr();
			cbInfo->fpCallback.funcMPTR = fpdCemuRequest->getBasicInfo.funcPtr;
			cbInfo->fpCallback.customParam = fpdCemuRequest->getBasicInfo.custom;
			g_fpd.nexFriendSession->requestPrincipleBaseInfoByPID(principalBasicInfo, pidList, count, handleResultCB_GetBasicInfoAsync, cbInfo);
		}

		void handleResponse_addOrRemoveFriend(uint32 errorCode, MPTR funcMPTR, MPTR custom)
		{
			if (errorCode == 0)
			{
				handleFPCallback2(funcMPTR, custom, 0);
				g_fpd.nexFriendSession->requestGetAllInformation(); // refresh list
			}
			else
				handleFPCallback2(funcMPTR, custom, 0x80000000);
		}

		void handleRequest_RemoveFriendAsync(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			g_fpd.nexFriendSession->removeFriend(fpdCemuRequest->addOrRemoveFriend.pid, std::bind(handleResponse_addOrRemoveFriend, std::placeholders::_1, fpdCemuRequest->addOrRemoveFriend.funcPtr, fpdCemuRequest->addOrRemoveFriend.custom));
		}

		void handleResponse_MarkFriendRequestAsReceivedAsync(uint32 errorCode, MPTR funcMPTR, MPTR custom)
		{
			if (errorCode == 0)
				handleFPCallback2(funcMPTR, custom, 0);
			else
				handleFPCallback2(funcMPTR, custom, 0x80000000);
		}

		void handleRequest_MarkFriendRequestAsReceivedAsync(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			// convert messageId list to little endian
			uint64 messageIds[100];
			sint32 count = 0;
			for (uint32 i = 0; i < fpdCemuRequest->markFriendRequest.count; i++)
			{
				uint64 mid = _swapEndianU64(fpdCemuRequest->markFriendRequest.messageIdList.GetPtr()[i]);
				if (mid == 0)
				{
					forceLogDebug_printf("MarkFriendRequestAsReceivedAsync - Invalid messageId");
					continue;
				}
				messageIds[count] = mid;
				count++;
				if (count >= sizeof(messageIds)/sizeof(messageIds[0]))
					break;
			}
			// skipped for now
			g_fpd.nexFriendSession->markFriendRequestsAsReceived(messageIds, count, std::bind(handleResponse_MarkFriendRequestAsReceivedAsync, std::placeholders::_1, fpdCemuRequest->markFriendRequest.funcPtr, fpdCemuRequest->markFriendRequest.custom));
		}

		void handleResponse_cancelMyFriendRequest(uint32 errorCode, uint32 pid, MPTR funcMPTR, MPTR custom)
		{
			if (errorCode == 0)
			{
				handleFPCallback2(funcMPTR, custom, 0);
			}
			else
				handleFPCallback2(funcMPTR, custom, 0x80000000);
		}

		void handleRequest_CancelFriendRequestAsync(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			// find friend request with matching pid
			nexFriendRequest frq;
			bool isIncoming;
			if (g_fpd.nexFriendSession->getFriendRequestByMessageId(frq, &isIncoming, fpdCemuRequest->cancelOrAcceptFriendRequest.messageId))
			{
				g_fpd.nexFriendSession->removeFriend(frq.principalInfo.principalId, std::bind(handleResponse_cancelMyFriendRequest, std::placeholders::_1, frq.principalInfo.principalId, fpdCemuRequest->cancelOrAcceptFriendRequest.funcPtr, fpdCemuRequest->cancelOrAcceptFriendRequest.custom));
			}
			else
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
		}

		void handleResponse_acceptFriendRequest(uint32 errorCode, uint32 pid, MPTR funcMPTR, MPTR custom)
		{
			if (errorCode == 0)
			{
				handleFPCallback2(funcMPTR, custom, 0);
				g_fpd.nexFriendSession->requestGetAllInformation(); // refresh list
			}
			else
				handleFPCallback2(funcMPTR, custom, 0x80000000);
		}

		void handleRequest_AcceptFriendRequestAsync(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
			// find friend request with matching pid
			nexFriendRequest frq;
			bool isIncoming;
			if (g_fpd.nexFriendSession->getFriendRequestByMessageId(frq, &isIncoming, fpdCemuRequest->cancelOrAcceptFriendRequest.messageId))
			{
				g_fpd.nexFriendSession->acceptFriendRequest(fpdCemuRequest->cancelOrAcceptFriendRequest.messageId, std::bind(handleResponse_acceptFriendRequest, std::placeholders::_1, frq.principalInfo.principalId, fpdCemuRequest->cancelOrAcceptFriendRequest.funcPtr, fpdCemuRequest->cancelOrAcceptFriendRequest.custom));
			}
			else
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}
		}

		void handleResponse_addFriendRequest(uint32 errorCode, MPTR funcMPTR, MPTR custom)
		{
			if (errorCode == 0)
			{
				handleFPCallback2(funcMPTR, custom, 0);
				g_fpd.nexFriendSession->requestGetAllInformation(); // refresh list
			}
			else
				handleFPCallback2(funcMPTR, custom, 0x80000000);
		}

		void handleRequest_AddFriendRequest(iosuFpdCemuRequest_t* fpdCemuRequest)
		{
			fpdCemuRequest->returnCode = 0;
			if (g_fpd.nexFriendSession == nullptr || g_fpd.nexFriendSession->isOnline() == false)
			{
				fpdCemuRequest->returnCode = 0x80000000;
				return;
			}

			uint16be* input = fpdCemuRequest->addFriendRequest.message.GetPtr();
			size_t inputLen = 0;
			while (input[inputLen] != 0)
				inputLen++;
			std::string msg = StringHelpers::ToUtf8({ input, inputLen });

			g_fpd.nexFriendSession->addFriendRequest(fpdCemuRequest->addFriendRequest.pid, msg.data(), std::bind(handleResponse_addFriendRequest, std::placeholders::_1, fpdCemuRequest->addFriendRequest.funcPtr, fpdCemuRequest->addFriendRequest.custom));
		}

		// called once a second to handle state checking and updates of the friends service
		void iosuFpd_updateFriendsService()
		{
			if (g_fpd.nexFriendSession)
			{
				g_fpd.nexFriendSession->update();

				if (g_fpd.asyncLoginCallback.func != MPTR_NULL)
				{
					if (g_fpd.nexFriendSession->isOnline())
					{
						*_fpdAsyncLoginRetCode.GetPtr() = 0x00000000;
						coreinitAsyncCallback_add(g_fpd.asyncLoginCallback.func, 2, _fpdAsyncLoginRetCode.GetMPTR(), g_fpd.asyncLoginCallback.customParam);
						g_fpd.asyncLoginCallback.func = MPTR_NULL;
						g_fpd.asyncLoginCallback.customParam = MPTR_NULL;
					}
				}
			}
		}

		void iosuFpd_thread()
		{
			SetThreadName("iosuFpd_thread");
			while (true)
			{
				uint32 returnValue = 0; // Ioctl return value
				ioQueueEntry_t* ioQueueEntry = iosuIoctl_getNextWithTimeout(IOS_DEVICE_FPD, 1000);
				if (ioQueueEntry == nullptr)
				{
					iosuFpd_updateFriendsService();
					continue;
				}
				if (ioQueueEntry->request == IOSU_FPD_REQUEST_CEMU)
				{
					iosuFpdCemuRequest_t* fpdCemuRequest = (iosuFpdCemuRequest_t*)ioQueueEntry->bufferVectors[0].buffer.GetPtr();
					if (fpdCemuRequest->requestCode == IOSU_FPD_INITIALIZE)
					{
						if (g_fpd.isInitialized2 == false)
						{
							if(ActiveSettings::IsOnlineEnabled())
								startFriendSession();
							
							g_fpd.isInitialized2 = true;
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_SET_NOTIFICATION_HANDLER)
					{
						g_fpd.notificationFunc = fpdCemuRequest->setNotificationHandler.funcPtr;
						g_fpd.notificationCustomParam = fpdCemuRequest->setNotificationHandler.custom;
						g_fpd.notificationMask = fpdCemuRequest->setNotificationHandler.notificationMask;
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_LOGIN_ASYNC)
					{
						if (g_fpd.nexFriendSession)
						{
							g_fpd.asyncLoginCallback.func = fpdCemuRequest->loginAsync.funcPtr;
							g_fpd.asyncLoginCallback.customParam = fpdCemuRequest->loginAsync.custom;
						}
						else
						{
							// offline mode
							*_fpdAsyncLoginRetCode.GetPtr() = 0; // if we return 0x80000000 here then Splatoon softlocks during boot
							coreinitAsyncCallback_add(fpdCemuRequest->loginAsync.funcPtr, 2, _fpdAsyncLoginRetCode.GetMPTR(), fpdCemuRequest->loginAsync.custom);
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_IS_ONLINE)
					{
						fpdCemuRequest->resultU32.u32 = g_fpd.nexFriendSession ? g_fpd.nexFriendSession->isOnline() : 0;
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_IS_PREFERENCE_VALID)
					{
						fpdCemuRequest->resultU32.u32 = 1; // todo (if this returns 0, the friend app will show the first-time-setup screen and ask the user to configure preferences)
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_MY_PRINCIPAL_ID)
					{
						uint8 slot = iosu::act::getCurrentAccountSlot();
						iosu::act::getPrincipalId(slot, &fpdCemuRequest->resultU32.u32);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_MY_ACCOUNT_ID)
					{
						char* accountId = (char*)fpdCemuRequest->common.ptr.GetPtr();
						uint8 slot = iosu::act::getCurrentAccountSlot();
						if (iosu::act::getAccountId(slot, accountId) == false)
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
						else
							fpdCemuRequest->returnCode = 0;
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_MY_MII)
					{
						FFLData_t* fflData = (FFLData_t*)fpdCemuRequest->common.ptr.GetPtr();
						uint8 slot = iosu::act::getCurrentAccountSlot();
						if (iosu::act::getMii(slot, fflData) == false)
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
						else
							fpdCemuRequest->returnCode = 0;
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_MY_SCREENNAME)
					{
						uint16be* screennameOutput = (uint16be*)fpdCemuRequest->common.ptr.GetPtr();
						uint8 slot = iosu::act::getCurrentAccountSlot();
						uint16 screennameTemp[ACT_NICKNAME_LENGTH];
						if (iosu::act::getScreenname(slot, screennameTemp))
						{
							for (sint32 i = 0; i < ACT_NICKNAME_LENGTH; i++)
							{
								screennameOutput[i] = screennameTemp[i];
							}
							screennameOutput[ACT_NICKNAME_LENGTH] = '\0'; // length is ACT_NICKNAME_LENGTH+1
							fpdCemuRequest->returnCode = 0;
						}
						else
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							screennameOutput[0] = '\0';
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_ACCOUNT_ID)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						fpdCemuRequest->returnCode = 0;
						for (sint32 i = 0; i < fpdCemuRequest->getFriendAccountId.count; i++)
						{
							char* nnidOutput = fpdCemuRequest->getFriendAccountId.accountIds.GetPtr()+i*17;
							uint32 pid = fpdCemuRequest->getFriendAccountId.pidList[i];
							if (g_fpd.nexFriendSession == nullptr)
							{
								nnidOutput[0] = '\0';
								continue;
							}
							nexFriend frd;
							nexFriendRequest frdReq;
							if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
							{
								strcpy(nnidOutput, frd.nnaInfo.principalInfo.nnid);
								continue;
							}
							bool incoming = false;
							if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
							{
								strcpy(nnidOutput, frdReq.principalInfo.nnid);
								continue;
							}
							nnidOutput[0] = '\0';
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_SCREENNAME)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						fpdCemuRequest->returnCode = 0;
						for (sint32 i = 0; i < fpdCemuRequest->getFriendScreenname.count; i++)
						{
							uint16be* screennameOutput = fpdCemuRequest->getFriendScreenname.nameList.GetPtr()+i*11;
							uint32 pid = fpdCemuRequest->getFriendScreenname.pidList[i];
							if(fpdCemuRequest->getFriendScreenname.languageList.IsNull() == false)
								fpdCemuRequest->getFriendScreenname.languageList.GetPtr()[i] = 0;
							if (g_fpd.nexFriendSession == nullptr)
							{
								screennameOutput[0] = '\0';
								continue;
							}
							nexFriend frd;
							nexFriendRequest frdReq;
							if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
							{
								convertMultiByteStringToBigEndianWidechar(frd.nnaInfo.principalInfo.mii.miiNickname, screennameOutput, 11);
								if (fpdCemuRequest->getFriendScreenname.languageList.IsNull() == false)
									fpdCemuRequest->getFriendScreenname.languageList.GetPtr()[i] = frd.nnaInfo.principalInfo.regionGuessed;
								continue;
							}
							bool incoming = false;
							if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
							{
								convertMultiByteStringToBigEndianWidechar(frdReq.principalInfo.mii.miiNickname, screennameOutput, 11);
								if (fpdCemuRequest->getFriendScreenname.languageList.IsNull() == false)
									fpdCemuRequest->getFriendScreenname.languageList.GetPtr()[i] = frdReq.principalInfo.regionGuessed;
								continue;
							}
							screennameOutput[0] = '\0';
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_MII)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						fpdCemuRequest->returnCode = 0;
						for (sint32 i = 0; i < fpdCemuRequest->getFriendMii.count; i++)
						{
							uint8* miiOutput = fpdCemuRequest->getFriendMii.miiList + i * FFL_SIZE;
							uint32 pid = fpdCemuRequest->getFriendMii.pidList[i];
							if (g_fpd.nexFriendSession == nullptr)
							{
								memset(miiOutput, 0, FFL_SIZE);
								continue;
							}
							nexFriend frd;
							nexFriendRequest frdReq;
							if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
							{
								memcpy(miiOutput, frd.nnaInfo.principalInfo.mii.miiData, FFL_SIZE);
								continue;
							}
							bool incoming = false;
							if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
							{
								memcpy(miiOutput, frdReq.principalInfo.mii.miiData, FFL_SIZE);
								continue;
							}
							memset(miiOutput, 0, FFL_SIZE);
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_PRESENCE)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						fpdCemuRequest->returnCode = 0;
						for (sint32 i = 0; i < fpdCemuRequest->getFriendPresence.count; i++)
						{
							friendPresence_t* presenceOutput = (friendPresence_t*)(fpdCemuRequest->getFriendPresence.presenceList + i * sizeof(friendPresence_t));
							memset(presenceOutput, 0, sizeof(friendPresence_t));
							uint32 pid = fpdCemuRequest->getFriendPresence.pidList[i];
							if (g_fpd.nexFriendSession == nullptr)
							{
								continue;
							}
							nexFriend frd;
							nexFriendRequest frdReq;
							if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
							{
								presenceOutput->isOnline = frd.presence.isOnline ? 1 : 0;
								presenceOutput->isValid = 1;

								presenceOutput->gameMode.joinFlagMask = frd.presence.joinFlagMask;
								presenceOutput->gameMode.matchmakeType = frd.presence.joinAvailability;
								presenceOutput->gameMode.joinGameId = frd.presence.gameId;
								presenceOutput->gameMode.joinGameMode = frd.presence.gameMode;
								presenceOutput->gameMode.hostPid = frd.presence.hostPid;
								presenceOutput->gameMode.groupId = frd.presence.groupId;

								memcpy(presenceOutput->gameMode.appSpecificData, frd.presence.appSpecificData, 0x14);

								continue;
							}
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_RELATIONSHIP)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						fpdCemuRequest->returnCode = 0;
						for (sint32 i = 0; i < fpdCemuRequest->getFriendRelationship.count; i++)
						{
							uint8* relationshipOutput = (fpdCemuRequest->getFriendRelationship.relationshipList + i);
							uint32 pid = fpdCemuRequest->getFriendRelationship.pidList[i];
							*relationshipOutput = RELATIONSHIP_INVALID;
							if (g_fpd.nexFriendSession == nullptr)
							{
								continue;
							}
							nexFriend frd;
							nexFriendRequest frdReq;
							bool incoming;
							if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
							{
								*relationshipOutput = RELATIONSHIP_FRIEND;
								continue;
							}
							else if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
							{
								if(incoming)
									*relationshipOutput = RELATIONSHIP_FRIENDREQUEST_IN;
								else
									*relationshipOutput = RELATIONSHIP_FRIENDREQUEST_OUT;
							}
						}
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_LIST)
					{
						handleRequest_GetFriendList(fpdCemuRequest, false);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIENDREQUEST_LIST)
					{
						handleRequest_GetFriendRequestList(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_LIST_ALL)
					{
						handleRequest_GetFriendList(fpdCemuRequest, true);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIEND_LIST_EX)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						handleRequest_GetFriendListEx(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_FRIENDREQUEST_LIST_EX)
					{
						if (g_fpd.nexFriendSession == nullptr)
						{
							fpdCemuRequest->returnCode = 0x80000000; // todo - proper error code
							iosuIoctl_completeRequest(ioQueueEntry, returnValue);
							return;
						}
						handleRequest_GetFriendRequestListEx(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_ADD_FRIEND)
					{
						// todo - figure out how this works
						*_fpdAsyncAddFriendRetCode.GetPtr() = 0;
						coreinitAsyncCallback_add(fpdCemuRequest->addOrRemoveFriend.funcPtr, 2, _fpdAsyncAddFriendRetCode.GetMPTR(), fpdCemuRequest->addOrRemoveFriend.custom);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_ADD_FRIEND_REQUEST)
					{
						handleRequest_AddFriendRequest(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_REMOVE_FRIEND_ASYNC)
					{
						handleRequest_RemoveFriendAsync(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_MARK_FRIEND_REQUEST_AS_RECEIVED_ASYNC)
					{
						handleRequest_MarkFriendRequestAsReceivedAsync(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_CANCEL_FRIEND_REQUEST_ASYNC)
					{
						handleRequest_CancelFriendRequestAsync(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_ACCEPT_FRIEND_REQUEST_ASYNC)
					{
						handleRequest_AcceptFriendRequestAsync(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_GET_BASIC_INFO_ASYNC)
					{
						handleRequest_GetBasicInfoAsync(fpdCemuRequest);
					}
					else if (fpdCemuRequest->requestCode == IOSU_FPD_UPDATE_GAMEMODE)
					{
						gameMode_t* gameMode = fpdCemuRequest->updateGameMode.gameMode.GetPtr();
						uint16be* gameModeMessage = fpdCemuRequest->updateGameMode.gameModeMessage.GetPtr();

						g_fpd.myPresence.joinFlagMask = gameMode->joinFlagMask;

						g_fpd.myPresence.joinAvailability = (uint8)(uint32)gameMode->matchmakeType;
						g_fpd.myPresence.gameId = gameMode->joinGameId;
						g_fpd.myPresence.gameMode = gameMode->joinGameMode;
						g_fpd.myPresence.hostPid = gameMode->hostPid;
						g_fpd.myPresence.groupId = gameMode->groupId;
						memcpy(g_fpd.myPresence.appSpecificData, gameMode->appSpecificData, 0x14);

						if (g_fpd.nexFriendSession)
						{
							g_fpd.nexFriendSession->updateMyPresence(g_fpd.myPresence);
						}
						
						fpdCemuRequest->returnCode = 0;
					}
					else
						cemu_assert_unimplemented();
				}
				else
					assert_dbg();
				iosuIoctl_completeRequest(ioQueueEntry, returnValue);
			}
			return;
		}

		void Initialize()
		{
			if (g_fpd.isThreadStarted)
				return;
			std::thread t1(iosuFpd_thread);
			t1.detach();
			g_fpd.isThreadStarted = true;
		}
	}
}