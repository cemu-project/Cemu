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
#include "Cafe/IOSU/iosu_types_common.h"
#include "Cafe/IOSU/nn/iosu_nn_service.h"

#include "Common/CafeString.h"

std::mutex g_friend_notification_mutex;
std::vector< std::pair<std::string, int> > g_friend_notifications;

namespace iosu
{
	namespace fpd
	{
		using NotificationRunningId = uint64;

		struct NotificationEntry
		{
			NotificationEntry(uint64 index, NexFriends::NOTIFICATION_TYPE type, uint32 pid) : timestamp(std::chrono::steady_clock::now()), runningId(index), type(type), pid(pid) {}
			std::chrono::steady_clock::time_point timestamp;
			NotificationRunningId runningId;
			NexFriends::NOTIFICATION_TYPE type;
			uint32 pid;
		};

		class
		{
		  public:
			void TrackNotification(NexFriends::NOTIFICATION_TYPE type, uint32 pid)
			{
				std::unique_lock _l(m_mtxNotificationQueue);
				m_notificationQueue.emplace_back(m_notificationQueueIndex++, type, pid);
			}

			void RemoveExpired()
			{
				// remove entries older than 10 seconds
				std::chrono::steady_clock::time_point expireTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
				std::erase_if(m_notificationQueue, [expireTime](const auto& notification) {
					return notification.timestamp < expireTime;
				});
			}

			std::optional<NotificationEntry> GetNextNotification(NotificationRunningId& previousRunningId)
			{
				std::unique_lock _l(m_mtxNotificationQueue);
				auto it = std::lower_bound(m_notificationQueue.begin(), m_notificationQueue.end(), previousRunningId, [](const auto& notification, const auto& runningId) {
					return notification.runningId <= runningId;
				});
				size_t itIndex = it - m_notificationQueue.begin();
				if(it == m_notificationQueue.end())
					return std::nullopt;
				previousRunningId = it->runningId;
				return *it;
			}

		  private:
			std::vector<NotificationEntry> m_notificationQueue;
			std::mutex m_mtxNotificationQueue;
			std::atomic_uint64_t m_notificationQueueIndex{1};
		}g_NotificationQueue;

		struct  
		{
			bool isThreadStarted;
			bool isInitialized2;
			NexFriends* nexFriendSession;
			std::mutex mtxFriendSession;
			// session state
			std::atomic_bool sessionStarted{false};
			// current state
			nexPresenceV2 myPresence;
		}g_fpd = {};

		void OverlayNotificationHandler(NexFriends::NOTIFICATION_TYPE type, uint32 pid)
		{
			cemuLog_logDebug(LogType::Force, "Friends::Notification {:02x} pid {:08x}", type, pid);
			if(!GetConfig().notification.friends)
				return;
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

		void NotificationHandler(NexFriends::NOTIFICATION_TYPE type, uint32 pid)
		{
			OverlayNotificationHandler(type, pid);
			g_NotificationQueue.TrackNotification(type, pid);
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

		void convertFPDTimestampToDate(uint64 timestamp, FPDDate* fpdDate)
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

		uint64 convertDateToFPDTimestamp(FPDDate* fpdDate)
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

		void NexPresenceToGameMode(const nexPresenceV2* presence, GameMode* gameMode)
		{
			memset(gameMode, 0, sizeof(GameMode));
			gameMode->joinFlagMask = presence->joinFlagMask;
			gameMode->matchmakeType = presence->joinAvailability;
			gameMode->joinGameId = presence->gameId;
			gameMode->joinGameMode = presence->gameMode;
			gameMode->hostPid = presence->hostPid;
			gameMode->groupId = presence->groupId;
			memcpy(gameMode->appSpecificData, presence->appSpecificData, 0x14);
		}

		void GameModeToNexPresence(const GameMode* gameMode, nexPresenceV2* presence)
		{
			*presence = {};
			presence->joinFlagMask = gameMode->joinFlagMask;
			presence->joinAvailability = (uint8)(uint32)gameMode->matchmakeType;
			presence->gameId = gameMode->joinGameId;
			presence->gameMode = gameMode->joinGameMode;
			presence->hostPid = gameMode->hostPid;
			presence->groupId = gameMode->groupId;
			memcpy(presence->appSpecificData, gameMode->appSpecificData, 0x14);
		}

		void NexFriendToFPDFriendData(const nexFriend* frd, FriendData* friendData)
		{
			memset(friendData, 0, sizeof(FriendData));
			// setup friend data
			friendData->type = 1; // friend
			friendData->pid = frd->nnaInfo.principalInfo.principalId;
			memcpy(friendData->mii, frd->nnaInfo.principalInfo.mii.miiData, FFL_SIZE);
			strcpy((char*)friendData->nnid, frd->nnaInfo.principalInfo.nnid);
			// screenname
			convertMultiByteStringToBigEndianWidechar(frd->nnaInfo.principalInfo.mii.miiNickname, friendData->screenname, sizeof(friendData->screenname) / sizeof(uint16be));

			friendData->friendExtraData.isOnline = frd->presence.isOnline != 0 ? 1 : 0;

			friendData->friendExtraData.gameKey.titleId = frd->presence.gameKey.titleId;
			friendData->friendExtraData.gameKey.ukn08 = frd->presence.gameKey.ukn;
			NexPresenceToGameMode(&frd->presence, &friendData->friendExtraData.gameMode);

			auto fixed_presence_msg = '\0' + frd->presence.msg; // avoid first character of comment from being cut off
			friendData->friendExtraData.gameModeDescription.assignFromUTF8(fixed_presence_msg);
			
			auto fixed_comment = '\0' + frd->comment.commentString; // avoid first character of comment from being cut off
			friendData->friendExtraData.comment.assignFromUTF8(fixed_comment);
			
			// set valid dates
			friendData->uknDate.year = 2018;
			friendData->uknDate.day = 1;
			friendData->uknDate.month = 1;
			friendData->uknDate.hour = 1;
			friendData->uknDate.minute = 1;
			friendData->uknDate.second = 1;

			friendData->friendExtraData.approvalTime.year = 2018;
			friendData->friendExtraData.approvalTime.day = 1;
			friendData->friendExtraData.approvalTime.month = 1;
			friendData->friendExtraData.approvalTime.hour = 1;
			friendData->friendExtraData.approvalTime.minute = 1;
			friendData->friendExtraData.approvalTime.second = 1;

			convertFPDTimestampToDate(frd->lastOnlineTimestamp, &friendData->friendExtraData.lastOnline);
		}

		void NexFriendRequestToFPDFriendData(const nexFriendRequest* frdReq, bool isIncoming, FriendData* friendData)
		{
			memset(friendData, 0, sizeof(FriendData));
			// setup friend data
			friendData->type = 0; // friend request
			friendData->pid = frdReq->principalInfo.principalId;
			memcpy(friendData->mii, frdReq->principalInfo.mii.miiData, FFL_SIZE);
			strcpy((char*)friendData->nnid, frdReq->principalInfo.nnid);
			// screenname
			convertMultiByteStringToBigEndianWidechar(frdReq->principalInfo.mii.miiNickname, friendData->screenname, sizeof(friendData->screenname) / sizeof(uint16be));

			convertMultiByteStringToBigEndianWidechar(frdReq->message.commentStr.c_str(), friendData->requestExtraData.comment, sizeof(friendData->requestExtraData.comment) / sizeof(uint16be));

			FPDDate expireDate;
			convertFPDTimestampToDate(frdReq->message.expireTimestamp, &expireDate);

			bool isProvisional = frdReq->message.expireTimestamp == 0;

			//friendData->requestExtraData.ukn0A8 = 0; // no change?
			//friendData->requestExtraData.ukn0A0 = 0; // if not set -> provisional friend request
			//friendData->requestExtraData.ukn0A4 = isProvisional ? 0 : 123; // no change?

			friendData->requestExtraData.messageId = frdReq->message.messageId;

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

		void NexFriendRequestToFPDFriendRequest(const nexFriendRequest* frdReq, bool isIncoming, FriendRequest* friendRequest)
		{
			memset(friendRequest, 0, sizeof(FriendRequest));

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

		struct FPProfile
		{
			uint8be country;
			uint8be area;
			uint16be unused;
		};
		static_assert(sizeof(FPProfile) == 4);

		struct SelfPresence
		{
			uint8be ukn[0x130]; // todo
		};
		static_assert(sizeof(SelfPresence) == 0x130);

		struct SelfPlayingGame
		{
			uint8be ukn0[0x10];
		};
		static_assert(sizeof(SelfPlayingGame) == 0x10);

		static const auto FPResult_Ok = 0;
		static const auto FPResult_InvalidIPCParam = BUILD_NN_RESULT(NN_RESULT_LEVEL_LVL6, NN_RESULT_MODULE_NN_FP, 0x680);
		static const auto FPResult_RequestFailed = BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_FP, 0); // figure out proper error code
		static const auto FPResult_Aborted = BUILD_NN_RESULT(NN_RESULT_LEVEL_STATUS, NN_RESULT_MODULE_NN_FP, 0x3480);

		class FPDService : public iosu::nn::IPCSimpleService
		{

			struct NotificationAsyncRequest
			{
				NotificationAsyncRequest(IPCCommandBody* cmd, uint32 maxNumEntries, FPDNotification* notificationsOut, uint32be* countOut)
					: cmd(cmd), maxNumEntries(maxNumEntries), notificationsOut(notificationsOut), countOut(countOut)
				{
				}

				IPCCommandBody* cmd;
				uint32 maxNumEntries;
				FPDNotification* notificationsOut;
				uint32be* countOut;
			};

			struct FPDClient
			{
				bool hasLoggedIn{false};
				uint32 notificationMask{0};
				NotificationRunningId prevRunningId{0};
				std::vector<NotificationAsyncRequest> notificationRequests;
			};

			// storage for async IPC requests
			std::vector<IPCCommandBody*> m_asyncLoginRequests;
			std::vector<FPDClient*> m_clients;

		  public:
			FPDService() : iosu::nn::IPCSimpleService("/dev/fpd") {}

			std::string GetThreadName() override
			{
				return "IOSUModule::FPD";
			}

			void StartService() override
			{
				cemu_assert_debug(m_asyncLoginRequests.empty());
			}

			void StopService() override
			{
				m_asyncLoginRequests.clear();
				for(auto& it : m_clients)
					delete it;
				m_clients.clear();
			}

			void* CreateClientObject() override
			{
				FPDClient* client = new FPDClient();
				m_clients.push_back(client);
				return client;
			}

			void DestroyClientObject(void* clientObject) override
			{
				FPDClient* client = (FPDClient*)clientObject;
				std::erase(m_clients, client);
				delete client;
			}

			void SendQueuedNotifications(FPDClient* client)
			{
				if (client->notificationRequests.empty())
					return;
				if (client->notificationRequests.size() > 1)
					cemuLog_log(LogType::Force, "FPD: More than one simultanous notification query not supported");
				NotificationAsyncRequest& request = client->notificationRequests[0];
				uint32 numNotifications = 0;
				while(numNotifications < request.maxNumEntries)
				{
					auto notification = g_NotificationQueue.GetNextNotification(client->prevRunningId);
					if (!notification)
						break;
					uint32 flag = 1 << static_cast<uint32>(notification->type);
					if((client->notificationMask & flag) == 0)
						continue;
					request.notificationsOut[numNotifications].type = static_cast<uint32>(notification->type);
					request.notificationsOut[numNotifications].pid = notification->pid;
					numNotifications++;
				}
				if (numNotifications == 0)
					return;
				*request.countOut = numNotifications;
				ServiceCallAsyncRespond(request.cmd, FPResult_Ok);
				client->notificationRequests.erase(client->notificationRequests.begin());
			}

			void TimerUpdate() override
			{
				// called once a second while service is running
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (!g_fpd.nexFriendSession)
					return;
				g_fpd.nexFriendSession->update();
				while(!m_asyncLoginRequests.empty())
				{
					if(g_fpd.nexFriendSession->isOnline())
					{
						ServiceCallAsyncRespond(m_asyncLoginRequests.front(), FPResult_Ok);
						m_asyncLoginRequests.erase(m_asyncLoginRequests.begin());
					}
					else
						break;
				}
				// handle notification responses
				g_NotificationQueue.RemoveExpired();
				for(auto& client : m_clients)
					SendQueuedNotifications(client);
			}

			uint32 ServiceCall(void* clientObject, uint32 requestId, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut) override
			{
				// for /dev/fpd input and output vectors are swapped
				std::swap(vecIn, vecOut);
				std::swap(numVecIn, numVecOut);

				FPDClient* fpdClient = (FPDClient*)clientObject;
				switch(static_cast<FPD_REQUEST_ID>(requestId))
				{
				case FPD_REQUEST_ID::SetNotificationMask:
					return CallHandler_SetNotificationMask(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetNotificationAsync:
					return CallHandler_GetNotificationAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::SetLedEventMask:
					cemuLog_logDebug(LogType::Force, "[/dev/fpd] SetLedEventMask is todo");
					return FPResult_Ok;
				case FPD_REQUEST_ID::LoginAsync:
					return CallHandler_LoginAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::HasLoggedIn:
					return CallHandler_HasLoggedIn(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::IsOnline:
					return CallHandler_IsOnline(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyPrincipalId:
					return CallHandler_GetMyPrincipalId(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyAccountId:
					return CallHandler_GetMyAccountId(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyScreenName:
					return CallHandler_GetMyScreenName(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyMii:
					return CallHandler_GetMyMii(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyProfile:
					return CallHandler_GetMyProfile(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyPresence:
					return CallHandler_GetMyPresence(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyComment:
					return CallHandler_GetMyComment(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyPreference:
					return CallHandler_GetMyPreference(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetMyPlayingGame:
					return CallHandler_GetMyPlayingGame(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendAccountId:
					return CallHandler_GetFriendAccountId(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendScreenName:
					return CallHandler_GetFriendScreenName(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendMii:
					return CallHandler_GetFriendMii(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendPresence:
					return CallHandler_GetFriendPresence(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendRelationship:
					return CallHandler_GetFriendRelationship(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendList:
					return CallHandler_GetFriendList_GetFriendListAll(fpdClient, vecIn, numVecIn, vecOut, numVecOut, false);
				case FPD_REQUEST_ID::GetFriendListAll:
					return CallHandler_GetFriendList_GetFriendListAll(fpdClient, vecIn, numVecIn, vecOut, numVecOut, true);
				case FPD_REQUEST_ID::GetFriendRequestList:
					return CallHandler_GetFriendRequestList(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendRequestListEx:
					return CallHandler_GetFriendRequestListEx(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetBlackList:
					return CallHandler_GetBlackList(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetFriendListEx:
					return CallHandler_GetFriendListEx(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::UpdatePreferenceAsync:
					return CallHandler_UpdatePreferenceAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::AddFriendRequestByPlayRecordAsync:
					return CallHandler_AddFriendRequestAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::AcceptFriendRequestAsync:
					return CallHandler_AcceptFriendRequestAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::DeleteFriendRequestAsync:
					return CallHandler_DeleteFriendRequestAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::CancelFriendRequestAsync:
					return CallHandler_CancelFriendRequestAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::MarkFriendRequestsAsReceivedAsync:
					return CallHandler_MarkFriendRequestsAsReceivedAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::RemoveFriendAsync:
					return CallHandler_RemoveFriendAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::DeleteFriendFlagsAsync:
					return CallHandler_DeleteFriendFlagsAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetBasicInfoAsync:
					return CallHandler_GetBasicInfoAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::CheckSettingStatusAsync:
					return CallHandler_CheckSettingStatusAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::IsPreferenceValid:
					return CallHandler_IsPreferenceValid(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::GetRequestBlockSettingAsync:
					return CallHandler_GetRequestBlockSettingAsync(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::AddFriendAsyncByPid:
					return CallHandler_AddFriendAsyncByPid(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				case FPD_REQUEST_ID::UpdateGameModeVariation1:
				case FPD_REQUEST_ID::UpdateGameModeVariation2:
					return CallHandler_UpdateGameMode(fpdClient, vecIn, numVecIn, vecOut, numVecOut);
				default:
					cemuLog_log(LogType::Force, "Unsupported service call {} to /dev/fpd", requestId);
					return BUILD_NN_RESULT(NN_RESULT_LEVEL_FATAL, NN_RESULT_MODULE_NN_FP, 0);
				}
			}

			#define DeclareInputPtr(__Name, __T, __count, __vecIndex) if(sizeof(__T)*(__count) != vecIn[__vecIndex].size) { cemuLog_log(LogType::Force, "FPD: IPC buffer has incorrect size"); return FPResult_InvalidIPCParam;};  __T* __Name = ((__T*)vecIn[__vecIndex].basePhys.GetPtr())
			#define DeclareInput(__Name, __T, __vecIndex) if(sizeof(__T) != vecIn[__vecIndex].size) { cemuLog_log(LogType::Force, "FPD: IPC buffer has incorrect size"); return FPResult_InvalidIPCParam;}; __T __Name = *((__T*)vecIn[__vecIndex].basePhys.GetPtr())
			#define DeclareOutputPtr(__Name, __T, __count, __vecIndex) if(sizeof(__T)*(__count) != vecOut[__vecIndex].size) { cemuLog_log(LogType::Force, "FPD: IPC buffer has incorrect size"); return FPResult_InvalidIPCParam;}; __T* __Name = ((__T*)vecOut[__vecIndex].basePhys.GetPtr())

			template<typename T>
			static nnResult WriteValueOutput(IPCIoctlVector* vec, const T& value)
			{
				if(vec->size != sizeof(T))
					return FPResult_InvalidIPCParam;
				*(T*)vec->basePhys.GetPtr() = value;
				return FPResult_Ok;
			}

			nnResult CallHandler_SetNotificationMask(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				DeclareInput(notificationMask, uint32be, 0);
				fpdClient->notificationMask = notificationMask;
				return FPResult_Ok;
			}

			nnResult CallHandler_GetNotificationAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 2)
					return FPResult_InvalidIPCParam;
				if((vecOut[0].size % sizeof(FPDNotification)) != 0 || vecOut[0].size < sizeof(FPDNotification))
				{
					cemuLog_log(LogType::Force, "FPD GetNotificationAsync: Unexpected output size");
					return FPResult_InvalidIPCParam;
				}
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				DeclareOutputPtr(countOut, uint32be, 1, 1);
				uint32 maxCount = vecOut[0].size / sizeof(FPDNotification);
				DeclareOutputPtr(notificationList, FPDNotification, maxCount, 0);
				fpdClient->notificationRequests.emplace_back(cmd, maxCount, notificationList, countOut);
				SendQueuedNotifications(fpdClient); // if any notifications are queued, send them immediately
				return FPResult_Ok;
			}

			nnResult CallHandler_LoginAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!ActiveSettings::IsOnlineEnabled())
				{
					// not online, fail immediately
					return FPResult_Ok; // Splatoon expects this to always return success otherwise it will softlock. This should be FPResult_Aborted?
				}
				StartFriendSession();
				fpdClient->hasLoggedIn = true;
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				m_asyncLoginRequests.emplace_back(cmd);
				return FPResult_Ok;
			}

			nnResult CallHandler_HasLoggedIn(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				return WriteValueOutput<uint32be>(vecOut, fpdClient->hasLoggedIn ? 1 : 0);
			}

			nnResult CallHandler_IsOnline(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				bool isOnline = g_fpd.nexFriendSession ? g_fpd.nexFriendSession->isOnline() : false;
				return WriteValueOutput<uint32be>(vecOut, isOnline?1:0);
			}

			nnResult CallHandler_GetMyPrincipalId(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				uint32 pid = 0;
				iosu::act::getPrincipalId(slot, &pid);
				return WriteValueOutput<uint32be>(vecOut, pid);
			}

			nnResult CallHandler_GetMyAccountId(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				std::string accountId = iosu::act::getAccountId2(slot);
				if(vecOut->size != ACT_ACCOUNTID_LENGTH)
				{
					cemuLog_log(LogType::Force, "GetMyAccountId: Unexpected output size");
					return FPResult_InvalidIPCParam;
				}
				if(accountId.length() > ACT_ACCOUNTID_LENGTH-1)
				{
					cemuLog_log(LogType::Force, "GetMyAccountId: AccountID is too long");
					return FPResult_InvalidIPCParam;
				}
				if(accountId.empty())
				{
					cemuLog_log(LogType::Force, "GetMyAccountId: AccountID is empty");
					return FPResult_InvalidIPCParam; // should return 0xC0C00800 ?
				}
				char* outputStr = (char*)vecOut->basePhys.GetPtr();
				memset(outputStr, 0, ACT_ACCOUNTID_LENGTH);
				memcpy(outputStr, accountId.data(), accountId.length());
				return FPResult_Ok;
			}

			nnResult CallHandler_GetMyScreenName(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				if(vecOut->size != ACT_NICKNAME_SIZE*sizeof(uint16be))
				{
					cemuLog_log(LogType::Force, "GetMyScreenName: Unexpected output size");
					return FPResult_InvalidIPCParam;
				}
				uint16 screenname[ACT_NICKNAME_SIZE]{0};
				bool r = iosu::act::getScreenname(slot, screenname);
				if (!r)
				{
					cemuLog_log(LogType::Force, "GetMyScreenName: Screenname is empty");
					return FPResult_InvalidIPCParam; // should return 0xC0C00800 ?
				}
				uint16be* outputStr = (uint16be*)vecOut->basePhys.GetPtr();
				for(sint32 i = 0; i < ACT_NICKNAME_SIZE; i++)
					outputStr[i] = screenname[i];
				return FPResult_Ok;
			}

			nnResult CallHandler_GetMyMii(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				if(vecOut->size != FFL_SIZE)
				{
					cemuLog_log(LogType::Force, "GetMyMii: Unexpected output size");
					return FPResult_InvalidIPCParam;
				}
				bool r = iosu::act::getMii(slot, (FFLData_t*)vecOut->basePhys.GetPtr());
				if (!r)
				{
					cemuLog_log(LogType::Force, "GetMyMii: Mii is empty");
					return FPResult_InvalidIPCParam; // should return 0xC0C00800 ?
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetMyProfile(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				FPProfile profile{0};
				// todo
				cemuLog_log(LogType::Force, "GetMyProfile is todo");
				return WriteValueOutput<FPProfile>(vecOut, profile);
			}

			nnResult CallHandler_GetMyPresence(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				uint8 slot = iosu::act::getCurrentAccountSlot();
				SelfPresence selfPresence{0};
				cemuLog_log(LogType::Force, "GetMyPresence is todo");
				return WriteValueOutput<SelfPresence>(vecOut, selfPresence);
			}

			nnResult CallHandler_GetMyComment(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				static constexpr uint32 MY_COMMENT_LENGTH = 0x12; // are comments utf16? Buffer length is 0x24
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				if(vecOut->size != MY_COMMENT_LENGTH*sizeof(uint16be))
				{
					cemuLog_log(LogType::Force, "GetMyComment: Unexpected output size");
					return FPResult_InvalidIPCParam;
				}
				std::basic_string<uint16be> myComment;
				myComment.resize(MY_COMMENT_LENGTH);
				memcpy(vecOut->basePhys.GetPtr(), myComment.data(), MY_COMMENT_LENGTH*sizeof(uint16be));
				return 0;
			}

			nnResult CallHandler_GetMyPreference(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				FPDPreference selfPreference{0};
				if(g_fpd.nexFriendSession)
				{
					nexPrincipalPreference nexPreference;
					g_fpd.nexFriendSession->getMyPreference(nexPreference);
					selfPreference.showOnline = nexPreference.showOnline;
					selfPreference.showGame = nexPreference.showGame;
					selfPreference.blockFriendRequests = nexPreference.blockFriendRequests;
					selfPreference.ukn = 0;
				}
				else
					memset(&selfPreference, 0, sizeof(FPDPreference));
				return WriteValueOutput<FPDPreference>(vecOut, selfPreference);
			}

			nnResult CallHandler_GetMyPlayingGame(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				GameKey selfPlayingGame
				{
					CafeSystem::GetForegroundTitleId(),
					CafeSystem::GetForegroundTitleVersion(),
					{0,0,0,0,0,0}
				};
				if (GetTitleIdHigh(CafeSystem::GetForegroundTitleId()) != 0x00050000)
				{
					selfPlayingGame.titleId = 0;
					selfPlayingGame.ukn08 = 0;
				}
				return WriteValueOutput<GameKey>(vecOut, selfPlayingGame);
			}

			nnResult CallHandler_GetFriendAccountId(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				// todo - online check
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareOutputPtr(accountId, CafeString<ACT_ACCOUNTID_LENGTH>, count, 0);
				memset(accountId, 0, ACT_ACCOUNTID_LENGTH * count);
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						const uint32 pid = pidList[i];
						auto& nnidOutput = accountId[i];
						nexFriend frd;
						nexFriendRequest frdReq;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							nnidOutput.assign(frd.nnaInfo.principalInfo.nnid);
							continue;
						}
						bool incoming = false;
						if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
						{
							nnidOutput.assign(frdReq.principalInfo.nnid);
							continue;
						}
						cemuLog_log(LogType::Force, "GetFriendAccountId: PID {} not found", pid);
					}
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetFriendScreenName(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				static_assert(sizeof(CafeWideString<ACT_NICKNAME_SIZE>) == 11*2);
				if(numVecIn != 3 || numVecOut != 2)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareInput(replaceNonAscii, uint8be, 2);
				DeclareOutputPtr(nameList, CafeWideString<ACT_NICKNAME_SIZE>, count, 0);
				uint8be* languageList = nullptr;
				if(vecOut[1].size > 0) // languageList is optional
				{
					DeclareOutputPtr(_languageList, uint8be, count, 1);
					languageList = _languageList;
				}
				memset(nameList, 0, ACT_NICKNAME_SIZE * sizeof(CafeWideString<ACT_NICKNAME_SIZE>));
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						const uint32 pid = pidList[i];
						CafeWideString<ACT_NICKNAME_SIZE>& screennameOutput = nameList[i];
						if (languageList)
							languageList[i] = 0; // unknown
						nexFriend frd;
						nexFriendRequest frdReq;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							screennameOutput.assignFromUTF8(frd.nnaInfo.principalInfo.mii.miiNickname);
							if (languageList)
								languageList[i] = frd.nnaInfo.principalInfo.regionGuessed;
							continue;
						}
						bool incoming = false;
						if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
						{
							screennameOutput.assignFromUTF8(frdReq.principalInfo.mii.miiNickname);
							if (languageList)
								languageList[i] = frdReq.principalInfo.regionGuessed;
							continue;
						}
						cemuLog_log(LogType::Force, "GetFriendScreenName: PID {} not found", pid);
					}
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetFriendMii(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareOutputPtr(miiList, FFLData_t, count, 0);
				memset(miiList, 0, sizeof(FFLData_t) * count);
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						const uint32 pid = pidList[i];
						FFLData_t& miiOutput = miiList[i];
						nexFriend frd;
						nexFriendRequest frdReq;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							memcpy(&miiOutput, frd.nnaInfo.principalInfo.mii.miiData, FFL_SIZE);
							continue;
						}
						bool incoming = false;
						if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
						{
							memcpy(&miiOutput, frdReq.principalInfo.mii.miiData, FFL_SIZE);
							continue;
						}
					}
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetFriendPresence(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareOutputPtr(presenceList, FriendPresence, count, 0);
				memset(presenceList, 0, sizeof(FriendPresence) * count);
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						FriendPresence& presenceOutput = presenceList[i];
						const uint32 pid = pidList[i];
						nexFriend frd;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							presenceOutput.isOnline = frd.presence.isOnline ? 1 : 0;
							presenceOutput.isValid = 1;
							// todo - region and subregion
							presenceOutput.gameMode.joinFlagMask = frd.presence.joinFlagMask;
							presenceOutput.gameMode.matchmakeType = frd.presence.joinAvailability;
							presenceOutput.gameMode.joinGameId = frd.presence.gameId;
							presenceOutput.gameMode.joinGameMode = frd.presence.gameMode;
							presenceOutput.gameMode.hostPid = frd.presence.hostPid;
							presenceOutput.gameMode.groupId = frd.presence.groupId;

							memcpy(presenceOutput.gameMode.appSpecificData, frd.presence.appSpecificData, 0x14);
						}
						else
						{
							cemuLog_log(LogType::Force, "GetFriendPresence: PID {} not found", pid);
						}
					}
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetFriendRelationship(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				// todo - check for valid session (same for all GetFriend* functions)
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareOutputPtr(relationshipList, uint8be, count, 0); // correct?
				for(uint32 i=0; i<count; i++)
					relationshipList[i] = RELATIONSHIP_INVALID;
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						const uint32 pid = pidList[i];
						uint8be& relationshipOutput = relationshipList[i];
						nexFriend frd;
						nexFriendRequest frdReq;
						bool incoming;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							relationshipOutput = RELATIONSHIP_FRIEND;
							continue;
						}
						else if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
						{
							if (incoming)
								relationshipOutput = RELATIONSHIP_FRIENDREQUEST_IN;
							else
								relationshipOutput = RELATIONSHIP_FRIENDREQUEST_OUT;
						}
					}
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetFriendList_GetFriendListAll(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut, bool isAll)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if(numVecIn != 2 || numVecOut != 2)
					return FPResult_InvalidIPCParam;
				DeclareInput(startIndex, uint32be, 0);
				DeclareInput(maxCount, uint32be, 1);
				if (maxCount * sizeof(FriendPID) != vecOut[0].size || vecOut[0].basePhys.IsNull())
				{
					cemuLog_log(LogType::Force, "GetFriendListAll: pid list buffer size is incorrect");
					return FPResult_InvalidIPCParam;
				}
				if (!g_fpd.nexFriendSession)
					return WriteValueOutput<uint32be>(vecOut+1, 0);
				betype<FriendPID>* pidList = (betype<FriendPID>*)vecOut[0].basePhys.GetPtr();
				std::vector<FriendPID> temporaryPidList;
				temporaryPidList.resize(std::min<size_t>(maxCount, 500));
				uint32 pidCount = 0;
				g_fpd.nexFriendSession->getFriendPIDs(temporaryPidList.data(), &pidCount, startIndex, temporaryPidList.size(), isAll);
				std::copy(temporaryPidList.begin(), temporaryPidList.begin() + pidCount, pidList);
				return WriteValueOutput<uint32be>(vecOut+1, pidCount);
			}

			nnResult CallHandler_GetFriendRequestList(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if(numVecIn != 2 || numVecOut != 2)
					return FPResult_InvalidIPCParam;
				DeclareInput(startIndex, uint32be, 0);
				DeclareInput(maxCount, uint32be, 1);
				if(maxCount * sizeof(FriendPID) != vecOut[0].size || vecOut[0].basePhys.IsNull())
				{
					cemuLog_log(LogType::Force, "GetFriendRequestList: pid list buffer size is incorrect");
					return FPResult_InvalidIPCParam;
				}
				if (!g_fpd.nexFriendSession)
					return WriteValueOutput<uint32be>(vecOut+1, 0);
				betype<FriendPID>* pidList = (betype<FriendPID>*)vecOut[0].basePhys.GetPtr();
				std::vector<FriendPID> temporaryPidList;
				temporaryPidList.resize(std::min<size_t>(maxCount, 500));
				uint32 pidCount = 0;
				g_fpd.nexFriendSession->getFriendRequestPIDs(temporaryPidList.data(), &pidCount, startIndex, temporaryPidList.size(), true, false);
				std::copy(temporaryPidList.begin(), temporaryPidList.begin() + pidCount, pidList);
				return WriteValueOutput<uint32be>(vecOut+1, pidCount);
			}

			nnResult CallHandler_GetFriendRequestListEx(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, uint32be, count, 0);
				DeclareOutputPtr(friendRequests, FriendRequest, count, 0);
				memset(friendRequests, 0, sizeof(FriendRequest) * count);
				if (!g_fpd.nexFriendSession)
					return FPResult_Ok;
				for(uint32 i=0; i<count; i++)
				{
					nexFriendRequest frdReq;
					bool incoming = false;
					if (!g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pidList[i]))
					{
						cemuLog_log(LogType::Force, "GetFriendRequestListEx: Failed to get friend request");
						return FPResult_RequestFailed;
					}
					NexFriendRequestToFPDFriendRequest(&frdReq, incoming, friendRequests + i);
				}
				return FPResult_Ok;
			}

			nnResult CallHandler_GetBlackList(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if(numVecIn != 2 || numVecOut != 2)
					return FPResult_InvalidIPCParam;
				DeclareInput(startIndex, uint32be, 0);
				DeclareInput(maxCount, uint32be, 1);
				if(maxCount * sizeof(FriendPID) != vecOut[0].size)
				{
					cemuLog_log(LogType::Force, "GetBlackList: pid list buffer size is incorrect");
					return FPResult_InvalidIPCParam;
				}
				if (!g_fpd.nexFriendSession)
					return WriteValueOutput<uint32be>(vecOut+1, 0);
				betype<FriendPID>* pidList = (betype<FriendPID>*)vecOut[0].basePhys.GetPtr();
				// todo!
				cemuLog_logDebug(LogType::Force, "GetBlackList is todo");
				uint32 countOut = 0;

				return WriteValueOutput<uint32be>(vecOut+1, countOut);
			}

			nnResult CallHandler_GetFriendListEx(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if(numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, betype<FriendPID>, count, 0);
				if(count * sizeof(FriendPID) != vecIn[0].size)
				{
					cemuLog_log(LogType::Force, "GetFriendListEx: pid input list buffer size is incorrect");
					return FPResult_InvalidIPCParam;
				}
				if(count * sizeof(FriendData) != vecOut[0].size)
				{
					cemuLog_log(LogType::Force, "GetFriendListEx: Friend output list buffer size is incorrect");
					return FPResult_InvalidIPCParam;
				}
				FriendData* friendOutput = (FriendData*)vecOut[0].basePhys.GetPtr();
				memset(friendOutput, 0, sizeof(FriendData) * count);
				if (g_fpd.nexFriendSession)
				{
					for (uint32 i = 0; i < count; i++)
					{
						uint32 pid = pidList[i];
						FriendData* friendData = friendOutput + i;
						nexFriend frd;
						nexFriendRequest frdReq;
						if (g_fpd.nexFriendSession->getFriendByPID(frd, pid))
						{
							NexFriendToFPDFriendData(&frd, friendData);
							continue;
						}
						bool incoming = false;
						if (g_fpd.nexFriendSession->getFriendRequestByPID(frdReq, &incoming, pid))
						{
							NexFriendRequestToFPDFriendData(&frdReq, incoming, friendData);
							continue;
						}
						cemuLog_logDebug(LogType::Force, "GetFriendListEx: Failed to find friend or request with pid {}", pid);
						memset(friendData, 0, sizeof(FriendData));
					}
				}
				return FPResult_Ok;
			}

			static void NexBasicInfoToBasicInfo(const nexPrincipalBasicInfo& nexBasicInfo, FriendBasicInfo& basicInfo)
			{
				memset(&basicInfo, 0, sizeof(FriendBasicInfo));
				basicInfo.pid = nexBasicInfo.principalId;
				strcpy(basicInfo.nnid, nexBasicInfo.nnid);

				convertMultiByteStringToBigEndianWidechar(nexBasicInfo.mii.miiNickname, basicInfo.screenname, sizeof(basicInfo.screenname) / sizeof(uint16be));
				memcpy(basicInfo.miiData, nexBasicInfo.mii.miiData, FFL_SIZE);

				basicInfo.uknDate90.day = 1;
				basicInfo.uknDate90.month = 1;
				basicInfo.uknDate90.hour = 1;
				basicInfo.uknDate90.minute = 1;
				basicInfo.uknDate90.second = 1;

				// unknown values not set:
				// ukn15
				// ukn2E
				// ukn2F
			}

			nnResult CallHandler_GetBasicInfoAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidListBE, betype<FriendPID>, count, 0);
				DeclareOutputPtr(basicInfoList, FriendBasicInfo, count, 0);
				if (!g_fpd.nexFriendSession)
				{
					memset(basicInfoList, 0, sizeof(FriendBasicInfo) * sizeof(count));
					return FPResult_Ok;
				}
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				std::vector<uint32> pidList;
				std::copy(pidListBE, pidListBE + count, std::back_inserter(pidList));
				g_fpd.nexFriendSession->requestPrincipleBaseInfoByPID(pidList.data(), count, [cmd, basicInfoList, count](NexFriends::RpcErrorCode result, std::span<nexPrincipalBasicInfo> basicInfo) -> void {
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					cemu_assert_debug(basicInfo.size() == count);
					for(uint32 i = 0; i < count; i++)
						NexBasicInfoToBasicInfo(basicInfo[i], basicInfoList[i]);
					ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_UpdatePreferenceAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInputPtr(newPreference, FPDPreference, 1, 0);
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->updatePreferencesAsync(nexPrincipalPreference(newPreference->showOnline != 0 ? 1 : 0, newPreference->showGame != 0 ? 1 : 0, newPreference->blockFriendRequests != 0 ? 1 : 0), [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_AddFriendRequestAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 2 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInputPtr(playRecord, RecentPlayRecordEx, 1, 0);
				uint32 msgLength = vecIn[1].size/sizeof(uint16be);
				DeclareInputPtr(msgBE, uint16be, msgLength, 1);
				if(msgLength == 0 || msgBE[msgLength-1] != 0)
				{
					cemuLog_log(LogType::Force, "AddFriendRequestAsync: Message must contain at least a null-termination character and end with one");
					return FPResult_InvalidIPCParam;
				}
				std::string msg = StringHelpers::ToUtf8({ msgBE, msgLength-1 });
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->addFriendRequest(playRecord->pid, msg.data(), [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_AcceptFriendRequestAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(requestId, uint64be, 0);
				nexFriendRequest frq;
				bool isIncoming;
				if (!g_fpd.nexFriendSession->getFriendRequestByMessageId(frq, &isIncoming, requestId))
					return FPResult_RequestFailed;
				if(!isIncoming)
				{
					cemuLog_log(LogType::Force, "AcceptFriendRequestAsync: Trying to accept outgoing friend request");
					return FPResult_RequestFailed;
				}
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->acceptFriendRequest(requestId, [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					return ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_DeleteFriendRequestAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				// reject incoming friend request
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(requestId, uint64be, 0);
				nexFriendRequest frq;
				bool isIncoming;
				if (!g_fpd.nexFriendSession->getFriendRequestByMessageId(frq, &isIncoming, requestId))
					return FPResult_RequestFailed;
				if(!isIncoming)
				{
					cemuLog_log(LogType::Force, "CancelFriendRequestAsync: Trying to block outgoing friend request");
					return FPResult_RequestFailed;
				}
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->deleteFriendRequest(requestId, [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					return ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_CancelFriendRequestAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				// retract outgoing friend request
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(requestId, uint64be, 0);
				nexFriendRequest frq;
				bool isIncoming;
				if (!g_fpd.nexFriendSession->getFriendRequestByMessageId(frq, &isIncoming, requestId))
					return FPResult_RequestFailed;
				if(isIncoming)
				{
					cemuLog_log(LogType::Force, "CancelFriendRequestAsync: Trying to cancel incoming friend request");
					return FPResult_RequestFailed;
				}
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->removeFriend(frq.principalInfo.principalId, [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					return ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_MarkFriendRequestsAsReceivedAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 2 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(requestIdsBE, uint64be, count, 0);
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				// endian convert
				std::vector<uint64> requestIds;
				std::copy(requestIdsBE, requestIdsBE + count, std::back_inserter(requestIds));
				g_fpd.nexFriendSession->markFriendRequestsAsReceived(requestIds.data(), requestIds.size(), [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					return ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_RemoveFriendAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(pid, uint32be, 0);
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();
				g_fpd.nexFriendSession->removeFriend(pid, [cmd](NexFriends::RpcErrorCode result){
					if (result != NexFriends::ERR_NONE)
						return ServiceCallAsyncRespond(cmd, FPResult_RequestFailed);
					return ServiceCallAsyncRespond(cmd, FPResult_Ok);
				});
				return FPResult_Ok;
			}

			nnResult CallHandler_DeleteFriendFlagsAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				if (numVecIn != 3 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(pid, uint32be, 0);
				cemuLog_logDebug(LogType::Force, "DeleteFriendFlagsAsync is todo");
				return FPResult_Ok;
			}

			nnResult CallHandler_CheckSettingStatusAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if (numVecIn != 0 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				if (vecOut[0].size != sizeof(uint8be))
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				IPCCommandBody* cmd = ServiceCallDelayCurrentResponse();

				// for now we respond immediately
				uint8 settingsStatus = 1; // todo - figure out what this status means
				auto r = WriteValueOutput<uint8be>(vecOut, settingsStatus);
				ServiceCallAsyncRespond(cmd, r);
				cemuLog_log(LogType::Force, "CheckSettingStatusAsync is todo");

				return FPResult_Ok;
			}

			nnResult CallHandler_IsPreferenceValid(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if (numVecIn != 0 || numVecOut != 1)
					return 0;
				if (!g_fpd.nexFriendSession)
					return 0;
				// we currently automatically put the preferences into a valid state on session creation if they are not set yet
				return WriteValueOutput<uint32be>(vecOut, 1); // if we return 0, the friend app will show the first time setup screen
			}

			nnResult CallHandler_GetRequestBlockSettingAsync(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut) // todo
			{
				if (numVecIn != 2 || numVecOut != 1)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(count, uint32be, 1);
				DeclareInputPtr(pidList, betype<FriendPID>, count, 0);
				DeclareOutputPtr(settingList, uint8be, count, 0);
				cemuLog_log(LogType::Force, "GetRequestBlockSettingAsync is todo");

				for (uint32 i = 0; i < count; i++)
					settingList[i] = 0;
				// implementation is todo. Used by friend list app when adding a friend
				// 0 means not blocked. Friend app will continue with GetBasicInformation()
				// 1 means blocked. Friend app will continue with AddFriendAsync to add the user as a provisional friend

				return FPResult_Ok;
			}

			nnResult CallHandler_AddFriendAsyncByPid(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if (numVecIn != 1 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInput(pid, uint32be, 0);
				cemuLog_log(LogType::Force, "AddFriendAsyncByPid is todo");
				return FPResult_Ok;
			}

			nnResult CallHandler_UpdateGameMode(FPDClient* fpdClient, IPCIoctlVector* vecIn, uint32 numVecIn, IPCIoctlVector* vecOut, uint32 numVecOut)
			{
				if (numVecIn != 2 || numVecOut != 0)
					return FPResult_InvalidIPCParam;
				if (!g_fpd.nexFriendSession)
					return FPResult_RequestFailed;
				DeclareInputPtr(gameMode, iosu::fpd::GameMode, 1, 0);
				uint32 messageLength = vecIn[1].size / sizeof(uint16be);
				if(messageLength == 0 || (vecIn[1].size%sizeof(uint16be)) != 0)
				{
					cemuLog_log(LogType::Force, "UpdateGameMode: Message must contain at least a null-termination character");
					return FPResult_InvalidIPCParam;
				}
				DeclareInputPtr(gameModeMessage, uint16be, messageLength, 1);
				messageLength--;
				GameModeToNexPresence(gameMode, &g_fpd.myPresence);
				g_fpd.nexFriendSession->updateMyPresence(g_fpd.myPresence);
				// todo - message
				return FPResult_Ok;
			}

			void StartFriendSession()
			{
				bool expected = false;
				if (!g_fpd.sessionStarted.compare_exchange_strong(expected, true))
					return;
				cemu_assert(!g_fpd.nexFriendSession);
				NAPI::AuthInfo authInfo;
				NAPI::NAPI_MakeAuthInfoFromCurrentAccount(authInfo);
				NAPI::ACTGetNexTokenResult nexTokenResult = NAPI::ACT_GetNexToken_WithCache(authInfo, 0x0005001010001C00, 0x0000, 0x00003200);
				if (!nexTokenResult.isValid())
				{
					cemuLog_logDebug(LogType::Force, "IOSU_FPD: Failed to acquire nex token for friend server");
					g_fpd.myPresence.isOnline = 0;
					return;
				}
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
				if (GetTitleIdHigh(CafeSystem::GetForegroundTitleId()) == 0x00050000)
				{
					g_fpd.myPresence.gameKey.titleId = CafeSystem::GetForegroundTitleId();
					g_fpd.myPresence.gameKey.ukn = CafeSystem::GetForegroundTitleVersion();
				}
				else
				{
					g_fpd.myPresence.gameKey.titleId = 0; // icon will not be ??? or invalid to others
					g_fpd.myPresence.gameKey.ukn = 0;
				}
				// resolve potential domain to IP address
				struct addrinfo hints = {0}, *addrs;
				hints.ai_family = AF_INET;
				const int status = getaddrinfo(nexTokenResult.nexToken.host, NULL, &hints, &addrs);
				if (status != 0)
				{
					cemuLog_log(LogType::Force, "IOSU_FPD: Failed to resolve hostname {}", nexTokenResult.nexToken.host);
					return;
				}
				char addrstr[NI_MAXHOST];
				getnameinfo(addrs->ai_addr, addrs->ai_addrlen, addrstr, sizeof addrstr, NULL, 0, NI_NUMERICHOST);
				cemuLog_log(LogType::Force, "IOSU_FPD: Resolved IP for hostname {}, {}", nexTokenResult.nexToken.host, addrstr);
				// start session
				const uint32_t hostIp = ((struct sockaddr_in*)addrs->ai_addr)->sin_addr.s_addr;
				freeaddrinfo(addrs);
				g_fpd.mtxFriendSession.lock();
				g_fpd.nexFriendSession = new NexFriends(hostIp, nexTokenResult.nexToken.port, "ridfebb9", myPid, nexTokenResult.nexToken.nexPassword, nexTokenResult.nexToken.token, accountId, (uint8*)&miiData, (wchar_t*)screenName, (uint8)countryCode, g_fpd.myPresence);
				g_fpd.nexFriendSession->setNotificationHandler(NotificationHandler);
				g_fpd.mtxFriendSession.unlock();
				cemuLog_log(LogType::Force, "IOSU_FPD: Created friend server session");
			}

			void StopFriendSession()
			{
				std::unique_lock _l(g_fpd.mtxFriendSession);
				bool expected = true;
				if (!g_fpd.sessionStarted.compare_exchange_strong(expected, false) )
					return;
				delete g_fpd.nexFriendSession;
				g_fpd.nexFriendSession = nullptr;
			}

		  private:


		};

		FPDService gFPDService;

		class : public ::IOSUModule
		{
			void TitleStart() override
			{
				gFPDService.Start();
				gFPDService.SetTimerUpdate(1000); // call TimerUpdate() once a second
			}
			void TitleStop() override
			{
				gFPDService.StopFriendSession();
				gFPDService.Stop();
			}
		}sIOSUModuleNNFPD;

		IOSUModule* GetModule()
		{
			return static_cast<IOSUModule*>(&sIOSUModuleNNFPD);
		}

	}
}

