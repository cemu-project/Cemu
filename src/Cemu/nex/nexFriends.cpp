#include "prudp.h"
#include "nex.h"
#include "nexFriends.h"
#include "Cafe/CafeSystem.h"

static const int NOTIFICATION_SRV_FRIEND_OFFLINE = 0x0A; // the opposite event (friend online) is notified via _PRESENCE_CHANGE
static const int NOTIFICATION_SRV_FRIEND_PRESENCE_CHANGE = 0x18;

static const int NOTIFICATION_SRV_FRIEND_REMOVED = 0x1A; // also used to indicate that an incoming friend request was canceled
static const int NOTIFICATION_SRV_FRIEND_REQUEST_INCOMING = 0x1B; // someone sent a friend request to us
static const int NOTIFICATION_SRV_FRIEND_ADDED = 0x1E; // not sent when friend request is accepted locally

void NexFriends::processServerNotification_friendOffline(uint32 pid)
{
	std::unique_lock listLock(mtx_lists);
	for (auto& it : list_friends)
	{
		if (it.nnaInfo.principalInfo.principalId == pid)
		{
			it.presence.isOnline = 0;
			generateNotification(NOTIFICATION_TYPE_FRIEND_LOGOFF, pid);
			return;
		}
	}
}

void NexFriends::processServerNotification_presenceChange(uint32 pid, nexPresenceV2& presence)
{
	std::unique_lock listLock(mtx_lists);
	for (auto& it : list_friends)
	{
		if (it.nnaInfo.principalInfo.principalId == pid)
		{
			bool isOnlineChange = (presence.isOnline != it.presence.isOnline);
			it.presence = presence;
			if (isOnlineChange)
				generateNotification((presence.isOnline!=0)?NOTIFICATION_TYPE_FRIEND_LOGIN:NOTIFICATION_TYPE_FRIEND_LOGOFF, pid);
			generateNotification(NOTIFICATION_TYPE_FRIEND_PRESENCE_CHANGE, pid);
			return;
		}
	}
}

void NexFriends::processServerNotification_removeFriendOrFriendRequest(uint32 pid)
{
	std::unique_lock listLock(mtx_lists);
	for (auto it = list_friends.begin(); it != list_friends.end();)
	{
		if (it->nnaInfo.principalInfo.principalId == pid)
		{
			list_friends.erase(it);
			generateNotification(NOTIFICATION_TYPE_REMOVED_FRIEND, pid);
			return;
		}
		it++;
	}
	for (auto it = list_friendReqIncoming.begin(); it != list_friendReqIncoming.end();)
	{
		if (it->principalInfo.principalId == pid)
		{
			list_friendReqIncoming.erase(it);
			generateNotification(NOTIFICATION_TYPE_REMOVED_INCOMING_REQUEST, pid);
			return;
		}
		it++;
	}
}

void NexFriends::processServerNotification_incomingFriendRequest(uint32 pid, nexFriendRequest& friendRequest)
{
	std::unique_lock listLock(mtx_lists);
	// check for duplicate
	for (auto& it : list_friendReqIncoming)
	{
		if (it.principalInfo.principalId == pid)
			return;
	}
	// add friend request and send notification
	list_friendReqIncoming.push_back(friendRequest);
	generateNotification(NOTIFICATION_TYPE_ADDED_INCOMING_REQUEST, pid);
}

void NexFriends::processServerNotification_addedFriend(uint32 pid, nexFriend& frd)
{
	std::unique_lock listLock(mtx_lists);
	// remove any related incoming friend request
	for (auto it = list_friendReqIncoming.begin(); it != list_friendReqIncoming.end();)
	{
		if (it->principalInfo.principalId == pid)
		{
			list_friendReqIncoming.erase(it);
			generateNotification(NOTIFICATION_TYPE_REMOVED_INCOMING_REQUEST, pid);
			break;
		}
		it++;
	}
	// remove any related outgoing friend request
	for (auto it = list_friendReqOutgoing.begin(); it != list_friendReqOutgoing.end();)
	{
		if (it->principalInfo.principalId == pid)
		{
			list_friendReqOutgoing.erase(it);
			generateNotification(NOTIFICATION_TYPE_REMOVED_OUTGOING_REQUEST, pid);
			break;
		}
		it++;
	}
	// check for duplicate
	for (auto& it : list_friendReqIncoming)
	{
		if (it.principalInfo.principalId == pid)
			return;
	}
	// add friend
	list_friends.push_back(frd);
	generateNotification(NOTIFICATION_TYPE_ADDED_FRIEND, pid);
}

void NexFriends::processServerNotification(uint32 notificationType, uint32 pid, nexPacketBuffer* notificationData)
{
	nexNotificationEventGeneral notificationGeneral;
	if (notificationType == NOTIFICATION_SRV_FRIEND_PRESENCE_CHANGE)
	{
		nexPresenceV2 presence;
		if (notificationData->readPlaceholderType(presence))
		{
			this->processServerNotification_presenceChange(pid, presence);
		}
	}
	else if (notificationType == 0x21)
	{
		// change comment text
		if (notificationData->readPlaceholderType(notificationGeneral))
		{
			// todo
		}
	}
	else if (notificationType == NOTIFICATION_SRV_FRIEND_OFFLINE)
	{
		// friend now offline
		if (notificationData->readPlaceholderType(notificationGeneral))
		{
			this->processServerNotification_friendOffline(pid);
		}
	}
	else if (notificationType == NOTIFICATION_SRV_FRIEND_REMOVED)
	{
		// friend removed or friend-request removed
		if (notificationData->readPlaceholderType(notificationGeneral))
		{
			this->processServerNotification_removeFriendOrFriendRequest(pid);
		}
	}
	else if (notificationType == NOTIFICATION_SRV_FRIEND_REQUEST_INCOMING)
	{
		nexFriendRequest friendRequest;
		if (notificationData->readPlaceholderType(friendRequest))
		{
			this->processServerNotification_incomingFriendRequest(pid, friendRequest);
		}
	}
	else if (notificationType == NOTIFICATION_SRV_FRIEND_ADDED)
	{
		nexFriend frd;
		if (notificationData->readPlaceholderType(frd))
		{
			this->processServerNotification_addedFriend(pid, frd);
		}
	}
	else if (true)
	{
		cemuLog_logDebug(LogType::Force, "Unsupported friend server notification type 0x{:02x}", notificationType);
	}
}

void nexFriends_protocolNotification_processRequest(nexServiceRequest_t* request)
{
	NexFriends* nexFriends = (NexFriends*)request->custom;
	if (request->methodId == 0x1 || request->methodId == 0x2)
	{
		// notification
		uint32 notificationType = request->data.readU32();
		uint32 pid = request->data.readU32();
		if (request->data.hasReadOutOfBounds())
		{
			request->nex->sendRequestResponse(request, 0, nullptr, 0);
			return;
		}
		cemuLog_logDebug(LogType::Force, "NN_NOTIFICATION methodId {} type {:02x} pid {:08x}", request->methodId, notificationType, pid);
		nexFriends->processServerNotification(notificationType, pid, &request->data);
		request->nex->sendRequestResponse(request, 0, nullptr, 0);
		return;
	}
	// unknown method
	request->nex->sendRequestResponse(request, 0x80000000, nullptr, 0);
}

NexFriends::NexFriends(uint32 authServerIp, uint16 authServerPort, const char* accessKey, uint32 pid, const char* nexPassword, const char* nexToken, const char* nnid, uint8* miiData, const wchar_t* miiNickname, uint8 countryCode, nexPresenceV2& myPresence)
	: nexCon(nullptr)
{
	memcpy(this->miiData, miiData, FFL_SIZE);
	strcpy(this->nnid, nnid);
	this->pid = pid;
	this->countryCode = countryCode;
	this->myPresence = myPresence;
	this->miiNickname = boost::nowide::narrow(miiNickname);
	cemu_assert_debug(this->miiNickname.size() <= 96-1);
	auth.serverIp = authServerIp;
	auth.port = authServerPort;
	auth.accessKey = std::string(accessKey);
	auth.nexPassword = std::string(nexPassword);
	auth.nexToken = std::string(nexToken);
	// login related variables
	this->lastLoginAttemptTime = prudpGetMSTimestamp() - 1000*60*60;
	this->isCurrentlyConnected = false;
	this->hasData = false;
	this->loginInProcess = false;
	this->numFailedLogins = 0;
	this->numSuccessfulLogins = 0;
}

NexFriends::~NexFriends()
{
	if(nexCon)
		nexCon->destroy();
}

void NexFriends::doAsyncLogin()
{
	nexCon = nex_establishSecureConnection(auth.serverIp, auth.port, auth.accessKey.c_str(), pid, auth.nexPassword.c_str(), auth.nexToken.c_str());
	if (nexCon == nullptr)
	{
		this->numFailedLogins++;
		this->loginInProcess = false;
		return;
	}

	nexCon->registerProtocolRequestHandler(0x64, nexFriends_protocolNotification_processRequest, this);
	nexCon->registerForAsyncProcessing();
	this->isCurrentlyConnected = true;
	this->firstInformationRequest = true;
	this->loginInProcess = false;
	this->numSuccessfulLogins++;
	requestGetAllInformation();
}

void NexFriends::initiateLogin()
{
	if (this->isCurrentlyConnected)
		return;
	if (this->loginInProcess)
		return;
	std::unique_lock loginLock(mtx_login);
	this->loginInProcess = true;
	// reset all data
	std::unique_lock listLock(mtx_lists);
	list_friends.resize(0);
	list_friendReqIncoming.resize(0);
	list_friendReqOutgoing.resize(0);
	previousState.list_friends.resize(0);
	previousState.list_friendReqIncoming.resize(0);
	previousState.list_friendReqOutgoing.resize(0);
	this->hasData = false;
	// start login attempt
	cemuLog_logDebug(LogType::Force, "Attempt to log into friend server...");
	std::thread t(&NexFriends::doAsyncLogin, this);
	t.detach();
}

void NexFriends::handleResponse_getAllInformation(nexServiceResponse_t* response, NexFriends* nexFriends, std::function<void(uint32)> cb)
{
	if (response->isSuccessful == false)
	{
		if (cb)
			cb(ERR_RPC_FAILED);
		return;
	}
	NexFriends* session = (NexFriends*)nexFriends;
	session->myPreference = nexPrincipalPreference(&response->data);
	nexComment comment(&response->data);
	if (response->data.hasReadOutOfBounds())
		return;
	// acquire lock on lists
	std::unique_lock listLock(session->mtx_lists);
	// remember current state of lists for change tracking
	session->previousState.list_friends = session->list_friends;
	session->previousState.list_friendReqIncoming = session->list_friendReqIncoming;
	session->previousState.list_friendReqOutgoing = session->list_friendReqOutgoing;
	// parse response and update lists
	// friends
	uint32 friendCount = response->data.readU32();
	session->list_friends.resize(friendCount);
	for (uint32 i = 0; i < friendCount; i++)
		session->list_friends[i].readData(&response->data);
	// friend requests (outgoing)
	uint32 friendRequestsOutCount = response->data.readU32();
	if (response->data.hasReadOutOfBounds())
		return;
	session->list_friendReqOutgoing.resize(friendRequestsOutCount);
	for (uint32 i = 0; i < friendRequestsOutCount; i++)
		session->list_friendReqOutgoing[i].readData(&response->data);
	// friend requests (incoming)
	uint32 friendRequestsInCount = response->data.readU32();
	if (response->data.hasReadOutOfBounds())
		return;
	session->list_friendReqIncoming.resize(friendRequestsInCount);
	for (uint32 i = 0; i < friendRequestsInCount; i++)
		session->list_friendReqIncoming[i].readData(&response->data);
	if (response->data.hasReadOutOfBounds())
		return;
	// blacklist
	uint32 blacklistCount = response->data.readU32();
	for (uint32 i = 0; i < blacklistCount; i++)
	{
		nexBlacklisted blacklisted(&response->data);
	}
	// ukn
	uint8 uknSetting = response->data.readU8(); // ? (usually zero)
	// persistent notifications
	uint32 perstNotificationCount = response->data.readU32();
	for (uint32 i = 0; i < perstNotificationCount; i++)
	{
		nexPersistentNotification notification(&response->data);
		//printf("--- Notification ---\n");
		//printf("ID %016llx pid1 %08x pid2 %08x type %08x\n", notification.messageId, notification.pid1, notification.pid2, notification.type);
		//printf("Msg %s\n", notification.msg.c_str());
	}
	uint8 isPreferenceInvalid = response->data.readU8(); // if not zero, preferences must be setup
	if (isPreferenceInvalid)
	{
		cemuLog_log(LogType::Force, "NEX: First time login into friend account, setting up default preferences");
		session->updatePreferencesAsync(nexPrincipalPreference(1, 1, 0), [](RpcErrorCode err){});
	}

	if (session->firstInformationRequest == false)
		session->trackNotifications();
	else
	{
		// first request after login -> send online notification
		session->generateNotification(NOTIFICATION_TYPE_ONLINE, session->pid);
	}

	session->firstInformationRequest = false;

	session->hasData = true;

	if (cb)
		cb(ERR_NONE);
}

bool NexFriends::requestGetAllInformation()
{
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	nexNNAInfo(this->countryCode, 0, nexPrincipalBasicInfo(pid, nnid, nexMiiV2(miiNickname.c_str(), miiData))).writeData(&packetBuffer);
	myPresence.writeData(&packetBuffer);
	packetBuffer.writeU64(0x1f18420000); // birthday (set to 1990-1-1)
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 1, &packetBuffer, std::bind(handleResponse_getAllInformation, std::placeholders::_1, this, nullptr), true);
	return true;
}

bool NexFriends::requestGetAllInformation(std::function<void(uint32)> cb)
{
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	nexNNAInfo(this->countryCode, 0, nexPrincipalBasicInfo(pid, nnid, nexMiiV2(miiNickname.c_str(), miiData))).writeData(&packetBuffer);
	nexPresenceV2(0).writeData(&packetBuffer);
	packetBuffer.writeU64(0x1f18420000); // birthday (set to 1990-1-1)
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 1, &packetBuffer, std::bind(handleResponse_getAllInformation, std::placeholders::_1, this, cb), true);
	return true;
}

bool NexFriends::updatePreferencesAsync(nexPrincipalPreference newPreferences, std::function<void(RpcErrorCode)> cb)
{
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	newPreferences.writeData(&packetBuffer);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 16, &packetBuffer, [this, cb, newPreferences](nexServiceResponse_t* response) -> void
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED);
			this->myPreference = newPreferences;
			return cb(NexFriends::ERR_NONE);
		}, true);
	// TEST
	return true;
}

void NexFriends::getMyPreference(nexPrincipalPreference& preference)
{
	preference = myPreference;
}

bool NexFriends::addProvisionalFriendByPidGuessed(uint32 principalId)
{
	uint8 tempNexBufferArray[512];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(principalId);
	cemu_assert_unimplemented();
	//nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 2, &packetBuffer, handleResponse_test, this);
	return true;
}

// returns true once connection is established and friend list data is available
bool NexFriends::isOnline()
{
	return isCurrentlyConnected && hasData;
}

void NexFriends::handleResponse_acceptFriendRequest(nexService* nex, nexServiceResponse_t* response)
{
	NexFriends* session = (NexFriends*)response->custom;
	// returns new state of FriendRequest + FriendInfo (if friend request holds no valid data, it means the friend request was successfully accepted?)
}

void NexFriends::setNotificationHandler(void(*notificationHandler)(NOTIFICATION_TYPE notificationType, uint32 pid))
{
	this->notificationHandler = notificationHandler;
}

void NexFriends::generateNotification(NOTIFICATION_TYPE notificationType, uint32 pid)
{
	if (this->notificationHandler == nullptr)
		return;
	this->notificationHandler(notificationType, pid);
}

// compare previous and current state of friend(-request) lists and generate change-notifications
void NexFriends::trackNotifications()
{
	// search for changed and added friend entries
	for (auto& frNew : list_friends)
	{
		bool entryFound = false;
		for (auto& frPrevious : previousState.list_friends)
		{
			if (frNew.nnaInfo.principalInfo.principalId == frPrevious.nnaInfo.principalInfo.principalId)
			{
				// todo - scan for changes
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// friend added
			generateNotification(NOTIFICATION_TYPE_ADDED_FRIEND, frNew.nnaInfo.principalInfo.principalId);
		}
	}
	// search for removed friends
	for (auto& frPrevious : previousState.list_friends)
	{
		bool entryFound = false;
		for (auto& frNew : list_friends)
		{
			if (frNew.nnaInfo.principalInfo.principalId == frPrevious.nnaInfo.principalInfo.principalId)
			{
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// friend removed
			generateNotification(NOTIFICATION_TYPE_REMOVED_FRIEND, frPrevious.nnaInfo.principalInfo.principalId);
		}
	}
	// search for added friend requests (incoming)
	for (auto& frqNew : list_friendReqIncoming)
	{
		bool entryFound = false;
		for (auto& frqPrevious : previousState.list_friendReqIncoming)
		{
			if (frqNew.principalInfo.principalId == frqPrevious.principalInfo.principalId)
			{
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// incoming friend request added
			generateNotification(NOTIFICATION_TYPE_ADDED_INCOMING_REQUEST, frqNew.principalInfo.principalId);
		}
	}
	// search for removed friend requests (incoming)
	for (auto& frqPrevious : previousState.list_friendReqIncoming)
	{
		bool entryFound = false;
		for (auto& frqNew : list_friendReqIncoming)
		{	
			if (frqNew.principalInfo.principalId == frqPrevious.principalInfo.principalId)
			{
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// incoming friend request removed
			generateNotification(NOTIFICATION_TYPE_REMOVED_INCOMING_REQUEST, frqPrevious.principalInfo.principalId);
		}
	}
	// search for added friend requests (outgoing)
	for (auto& frqNew : list_friendReqOutgoing)
	{
		bool entryFound = false;
		for (auto& frqPrevious : previousState.list_friendReqOutgoing)
		{
			if (frqNew.principalInfo.principalId == frqPrevious.principalInfo.principalId)
			{
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// outgoing friend request added
			generateNotification(NOTIFICATION_TYPE_ADDED_OUTGOING_REQUEST, frqNew.principalInfo.principalId);
		}
	}
	// search for removed friend requests (outgoing)
	for (auto& frqPrevious : previousState.list_friendReqOutgoing)
	{
		bool entryFound = false;
		for (auto& frqNew : list_friendReqOutgoing)
		{
			if (frqNew.principalInfo.principalId == frqPrevious.principalInfo.principalId)
			{
				entryFound = true;
				break;
			}
		}
		if (entryFound == false)
		{
			// outgoing friend request removed
			generateNotification(NOTIFICATION_TYPE_REMOVED_OUTGOING_REQUEST, frqPrevious.principalInfo.principalId);
		}
	}
}

void addUniquePidToList(std::vector<uint32>& pidList, uint32 pid)
{
	bool alreadyAdded = false;
	for (auto& it2 : pidList)
	{
		if (it2 == pid)
		{
			alreadyAdded = true;
			break;
		}
	}
	if (alreadyAdded)
		return;
	pidList.push_back(pid);
}

void NexFriends::getFriendPIDs(uint32* pidList, uint32* pidCount, sint32 offset, sint32 count, bool includeFriendRequests)
{
	if (count < 0)
	{
		*pidCount = 0;
		return;
	}
	// parse response and update lists
	std::vector<uint32> allPids;
	std::unique_lock listLock(this->mtx_lists);
	for (auto& it : this->list_friends)
	{
		uint32 pid = it.nnaInfo.principalInfo.principalId;
		addUniquePidToList(allPids, pid);
	}
	if (includeFriendRequests)
	{
		// incoming friend requests are not included
		for (auto& it : this->list_friendReqOutgoing)
		{
			uint32 pid = it.principalInfo.principalId;
			addUniquePidToList(allPids, pid);
		}
	}
	sint32 copyCount = std::max((sint32)allPids.size() - offset, 0);
	copyCount = std::min(copyCount, count);
	if (pidList)
	{
		for (sint32 i = 0; i < copyCount; i++)
			pidList[i] = allPids[offset + i];
	}
	*pidCount = copyCount;
}

void NexFriends::getFriendRequestPIDs(uint32* pidList, uint32* pidCount, sint32 offset, sint32 count, bool includeIncoming, bool includeOutgoing)
{
	if (count < 0)
	{
		*pidCount = 0;
		return;
	}
	// parse response and update lists
	std::vector<uint32> allPids;
	std::unique_lock listLock(this->mtx_lists);
	if (includeIncoming)
	{
		for (auto& it : this->list_friendReqIncoming)
		{
			uint32 pid = it.principalInfo.principalId;
			addUniquePidToList(allPids, pid);
		}
	}
	if (includeOutgoing)
	{
		for (auto& it : this->list_friendReqOutgoing)
		{
			uint32 pid = it.principalInfo.principalId;
			addUniquePidToList(allPids, pid);
		}
	}
	sint32 copyCount = std::max((sint32)allPids.size() - offset, 0);
	copyCount = std::min(copyCount, count);
	if (pidList)
	{
		for (sint32 i = 0; i < copyCount; i++)
			pidList[i] = allPids[offset + i];
	}
	*pidCount = copyCount;
}

std::string NexFriends::getAccountNameByPid(uint32 principalId)
{
	if (this->pid == principalId)
		return this->nnid;

	nexFriend friendData{};
	if(getFriendByPID(friendData, principalId))
		return friendData.nnaInfo.principalInfo.nnid;

	nexFriendRequest requestData{};
	if (getFriendRequestByPID(requestData, nullptr, principalId))
		return requestData.principalInfo.nnid;

	return {};
}

int NexFriends::getPendingFriendRequestCount()
{
	std::unique_lock listLock(this->mtx_lists);
	return (int)this->list_friendReqIncoming.size();
}


bool NexFriends::getFriendByPID(nexFriend& friendData, uint32 searchedPid)
{
	std::unique_lock listLock(this->mtx_lists);
	for (auto& it : this->list_friends)
	{
		uint32 pid = it.nnaInfo.principalInfo.principalId;
		if (pid == searchedPid)
		{
			friendData = it;
			return true;
		}
	}
	return false;
}

bool NexFriends::getFriendRequestByPID(nexFriendRequest& friendRequestData, bool* isIncoming, uint32 searchedPid)
{
	std::unique_lock listLock(this->mtx_lists);
	for (auto& it : this->list_friendReqIncoming)
	{
		uint32 pid = it.principalInfo.principalId;
		if (pid == searchedPid)
		{
			friendRequestData = it;
			if(isIncoming)
				*isIncoming = true;
			return true;
		}
	}
	for (auto& it : this->list_friendReqOutgoing)
	{
		uint32 pid = it.principalInfo.principalId;
		if (pid == searchedPid)
		{
			friendRequestData = it;
			if (isIncoming)
				*isIncoming = false;
			return true;
		}
	}
	return false;
}

bool NexFriends::getFriendRequestByMessageId(nexFriendRequest& friendRequestData, bool* isIncoming, uint64 messageId)
{
	std::unique_lock listLock(this->mtx_lists);
	for (auto& it : this->list_friendReqIncoming)
	{
		uint64 mid = it.message.messageId;
		if(mid == 0)
			continue;
		if (mid == messageId)
		{
			friendRequestData = it;
			*isIncoming = true;
			return true;
		}
	}
	for (auto& it : this->list_friendReqOutgoing)
	{
		uint64 mid = it.message.messageId;
		if (mid == 0)
			continue;
		if (mid == messageId)
		{
			friendRequestData = it;
			*isIncoming = false;
			return true;
		}
	}
	return false;
}

void addProvisionalFriendHandler(nexServiceResponse_t* nexResponse, std::function<void(uint32)> cb)
{
	if (nexResponse->isSuccessful)
		cb(0);
	else
	{
		// todo: Properly handle returned error code and data
		cb(NexFriends::ERR_RPC_FAILED);
	}
}

bool NexFriends::addProvisionalFriend(char* name, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
	{
		// not connected
		cb(ERR_NOT_CONNECTED);
		return false;
	}
	uint8 tempNexBufferArray[128];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeString(name);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 3, &packetBuffer, std::bind(addProvisionalFriendHandler, std::placeholders::_1, cb), true);
	return true;
}

void addFriendRequestHandler(nexServiceResponse_t* nexResponse, NexFriends* nexFriends, std::function<void(uint32)> cb)
{
	if (nexResponse->isSuccessful)
		cb(0);
	else
	{
		// todo: Properly handle returned error code
		cb(NexFriends::ERR_RPC_FAILED);
		nexFriends->requestGetAllInformation(); // refresh friend list and send add/remove notifications
	}
}

void NexFriends::addFriendRequest(uint32 pid, const char* comment, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
	{
		// not connected
		cb(ERR_NOT_CONNECTED);
		return;
	}
	uint8 tempNexBufferArray[2048];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(pid);
	packetBuffer.writeU8(0); // ukn (language of comment?)
	packetBuffer.writeString(comment);
	packetBuffer.writeU8(0); // ukn (language of next string?)
	packetBuffer.writeString(""); // ukn
	nexGameKey(0, 0).writeData(&packetBuffer);
	packetBuffer.writeU64(0); // ukn
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 5, &packetBuffer, std::bind(addFriendRequestHandler, std::placeholders::_1, this, cb), true);
}

void NexFriends::requestPrincipleBaseInfoByPID(uint32* pidList, sint32 count, const std::function<void(RpcErrorCode result, std::span<nexPrincipalBasicInfo> basicInfo)>& cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
		return cb(ERR_NOT_CONNECTED, {});
	uint8 tempNexBufferArray[512];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(count);
	for(sint32 i=0; i<count; i++)
		packetBuffer.writeU32(pidList[i]);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 17, &packetBuffer, [cb, count](nexServiceResponse_t* response)
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED, {});
			// process result
			uint32 resultCount = response->data.readU32();
			if (resultCount != count)
				return cb(NexFriends::ERR_UNEXPECTED_RESULT, {});
			std::vector<nexPrincipalBasicInfo> nexBasicInfo;
			nexBasicInfo.resize(count);
			for (uint32 i = 0; i < resultCount; i++)
				nexBasicInfo[i].readData(&response->data);
			if (response->data.hasReadOutOfBounds())
				return cb(NexFriends::ERR_UNEXPECTED_RESULT, {});
			return cb(NexFriends::ERR_NONE, nexBasicInfo);
		}, true);
}

void genericFriendServiceNoResponseHandler(nexServiceResponse_t* nexResponse, std::function<void(uint32)> cb)
{
	if (nexResponse->isSuccessful)
		cb(0);
	else
	{
		// todo: Properly handle returned error code
		cb(NexFriends::ERR_RPC_FAILED);
	}
}

void NexFriends::removeFriend(uint32 pid, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
	{
		// not connected
		cb(ERR_NOT_CONNECTED);
		return;
	}
	uint8 tempNexBufferArray[512];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(pid);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 4, &packetBuffer, [this, cb](nexServiceResponse_t* response) -> void
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED);
			else
			{
				cb(NexFriends::ERR_NONE);
				this->requestGetAllInformation(); // refresh friend list and send add/remove notifications
				return;
			}
		}, true);
}

void NexFriends::cancelOutgoingProvisionalFriendRequest(uint32 pid, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
	{
		// not connected
		cb(ERR_NOT_CONNECTED);
		return;
	}
	uint8 tempNexBufferArray[512];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(pid);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 4, &packetBuffer, [cb](nexServiceResponse_t* response) -> void
	{
		if (!response->isSuccessful)
			return cb(NexFriends::ERR_RPC_FAILED);
		else
			return cb(NexFriends::ERR_NONE);
		}, true);
}

void NexFriends::acceptFriendRequest(uint64 messageId, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
		return cb(ERR_NOT_CONNECTED);
	uint8 tempNexBufferArray[128];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU64(messageId);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 7, &packetBuffer, [cb](nexServiceResponse_t* response) -> void
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED);
			else
				return cb(NexFriends::ERR_NONE);
		}, true);
}

void NexFriends::deleteFriendRequest(uint64 messageId, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
		return cb(ERR_NOT_CONNECTED);
	uint8 tempNexBufferArray[128];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU64(messageId);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 8, &packetBuffer, [this, cb](nexServiceResponse_t* response) -> void
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED);
			cb(NexFriends::ERR_NONE);
			this->requestGetAllInformation(); // refresh friend list and send add/remove notifications
		}, true);
}

void NexFriends::markFriendRequestsAsReceived(uint64* messageIdList, sint32 count, std::function<void(RpcErrorCode)> cb)
{
	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
		return cb(ERR_NOT_CONNECTED);
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	packetBuffer.writeU32(count);
	for(sint32 i=0; i<count; i++)
		packetBuffer.writeU64(messageIdList[i]);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 10, &packetBuffer, [cb](nexServiceResponse_t* response) -> void
		{
			if (!response->isSuccessful)
				return cb(NexFriends::ERR_RPC_FAILED);
			else
				return cb(NexFriends::ERR_NONE);
		}, true);
}

void NexFriends::updateMyPresence(nexPresenceV2& myPresence)
{
	this->myPresence = myPresence;

	if (GetTitleIdHigh(CafeSystem::GetForegroundTitleId()) == 0x00050000)
	{
		myPresence.gameKey.titleId = CafeSystem::GetForegroundTitleId();
		myPresence.gameKey.ukn = CafeSystem::GetForegroundTitleVersion();
	}
	else
	{
		myPresence.gameKey.titleId = 0; // icon will not be ??? or invalid to others
		myPresence.gameKey.ukn = 0;
	}

	if (nexCon == nullptr || nexCon->getState() != nexService::STATE_CONNECTED)
	{
		// not connected
		return;
	}
	uint8 tempNexBufferArray[1024];
	nexPacketBuffer packetBuffer(tempNexBufferArray, sizeof(tempNexBufferArray), true);
	myPresence.writeData(&packetBuffer);
	nexCon->callMethod(NEX_PROTOCOL_FRIENDS_WIIU, 13, &packetBuffer, +[](nexServiceResponse_t* nexResponse){}, false);
}

void NexFriends::update()
{
	std::unique_lock loginLock(mtx_login);
	if (!isCurrentlyConnected)
	{
		if (loginInProcess)
		{
			// wait for login attempt to finish
		}
		else
		{
			// should we try to reconnect?
			uint32 timeSinceLastLoginAttempt = prudpGetMSTimestamp() - this->lastLoginAttemptTime;
			uint32 delayTime = 30; // 30 seconds by default
			if (this->numFailedLogins < 3)
				delayTime = 30;
			else
			{
				if (this->numSuccessfulLogins == 0)
					return; // never try again
				if (this->numFailedLogins >= 10)
					return; // stop after 10 failed attempts
				delayTime = 60 + (this->numFailedLogins - 3) * 60; // add one minute each time it fails
			}
			if (timeSinceLastLoginAttempt < delayTime)
				return;
			cemuLog_log(LogType::Force, "NEX: Attempt async friend service login");
			initiateLogin();
		}
	}
	else
	{
		if (this->nexCon == nullptr || this->nexCon->getState() != nexService::STATE_CONNECTED)
		{
			cemuLog_log(LogType::Force, "NEX: Lost friend server session");
			if (this->nexCon)
			{
				this->nexCon->destroy();
				this->nexCon = nullptr;
			}

			isCurrentlyConnected = false;
		}
	}
}
