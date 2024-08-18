#pragma once

#ifndef FFL_SIZE
#define FFL_SIZE	0x60
#endif

class nexGameKey : public nexType
{
public:
	nexGameKey() = default;

	nexGameKey(uint64 titleId, uint16 ukn)
	{
		this->titleId = titleId;
		this->ukn = ukn;
	}

	nexGameKey(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeU64(titleId);
		pb->writeU16(ukn);
	}
	void readData(nexPacketBuffer* pb) override
	{
		titleId = pb->readU64();
		ukn = pb->readU16();
	}
public:
	uint64 titleId;
	uint16 ukn;
};

class nexMiiV2 : public nexType
{
public:
	nexMiiV2()
	{
		miiNickname[0] = '\0';
	}

	nexMiiV2(const char* miiNickname, uint8* miiData)
	{
		strncpy(this->miiNickname, miiNickname, 127);
		this->miiNickname[127] = '\0';
		memcpy(this->miiData, miiData, FFL_SIZE);
	}

	nexMiiV2(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeString(this->miiNickname);
		pb->writeU8(0); // ukn
		pb->writeU8(0); // ukn
		pb->writeBuffer(this->miiData, FFL_SIZE);
		pb->writeU64(0); // ukn
	}
	void readData(nexPacketBuffer* pb) override
	{
		pb->readString(this->miiNickname, sizeof(this->miiNickname));
		this->miiNickname[127] = '\0';
		pb->readU8(); // ukn
		pb->readU8(); // ukn
		memset(miiData, 0, sizeof(miiData));
		pb->readBuffer(this->miiData, FFL_SIZE);
		pb->readU64(); // ukn
	}
public:
	uint8 miiData[FFL_SIZE];
	char miiNickname[128];
};

class nexPresenceV2 : public nexType
{
public:
	nexPresenceV2()
	{

	}

	nexPresenceV2(int ukn)
	{
		// todo
	}

	nexPresenceV2(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	const char* getMetaName() override
	{
		return "NintendoPresenceV2";
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeU32(1); // isValid?
		pb->writeU8(isOnline ? 1 : 0);
		// gameKey:
		pb->writeU64(gameKey.titleId);
		pb->writeU16(gameKey.ukn);

		pb->writeU8(1); //	uint8			ukn3(example value : 1)
		pb->writeString("");//	String			msg(current game ? )

		pb->writeU32(joinFlagMask);
		pb->writeU8(joinAvailability);
		pb->writeU32(gameId);
		pb->writeU32(gameMode);
		pb->writeU32(hostPid);
		pb->writeU32(groupId);

		pb->writeBuffer(appSpecificData, sizeof(appSpecificData));
		pb->writeU8(3); // ukn
		pb->writeU8(1); // ukn
		pb->writeU8(3); // ukn
	}
	void readData(nexPacketBuffer* pb) override
	{
		ukn0 = pb->readU32();
		isOnline = pb->readU8();
		gameKey.readData(pb);
		ukn3 = pb->readU8();
		char msgBuffer[1024];
		pb->readString(msgBuffer, sizeof(msgBuffer));
		msgBuffer[1023] = '\0';
		this->msg = std::string(msgBuffer);
		joinFlagMask = pb->readU32();
		joinAvailability = pb->readU8();
		gameId = pb->readU32();
		gameMode = pb->readU32();
		hostPid = pb->readU32();
		groupId = pb->readU32();
		pb->readBuffer(appSpecificData, sizeof(appSpecificData));
		ukn11 = pb->readU8();
		ukn12 = pb->readU8();
		ukn13 = pb->readU8();
	}
public:
	uint32 ukn0; // seen values: 0 -> offline, 1 -> online (menu, but not always), 8 and 0x1E6 -> in Splatoon
	uint8 isOnline;
	nexGameKey gameKey;
	uint8 ukn3; // message language?
	std::string msg;
	uint32 joinFlagMask;
	uint8 joinAvailability;
	uint32 gameId;
	uint32 gameMode;
	uint32 hostPid;
	uint32 groupId;
	uint8 appSpecificData[0x14];
	uint8 ukn11;
	uint8 ukn12;
	uint8 ukn13;
};

class nexPrincipalBasicInfo : public nexType
{
public:
	nexPrincipalBasicInfo()
	{
		this->nnid[0] = '\0';
	}

	nexPrincipalBasicInfo(uint32 principalId, char* nnid, const nexMiiV2& mii)
		: principalId(principalId), mii(mii)
	{
		strcpy(this->nnid, nnid);
	}

	nexPrincipalBasicInfo(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeU32(principalId);
		pb->writeString(this->nnid);
		mii.writeData(pb);
		pb->writeU8(2); // todo
	}

	void readData(nexPacketBuffer* pb) override
	{
		principalId = pb->readU32();
		pb->readString(nnid, sizeof(nnid));
		mii.readData(pb);
		regionGuessed = pb->readU8();
	}

public:
	uint32 principalId;
	char nnid[32];
	nexMiiV2 mii;
	uint8 regionGuessed;
};

class nexNNAInfo : public nexType
{
public:
	nexNNAInfo()
	{

	}

	nexNNAInfo(uint8 countryCode, uint8 countrySubCode, const nexPrincipalBasicInfo& principalBasicInfo)
		: countryCode(countryCode), countrySubCode(countrySubCode), principalInfo(principalBasicInfo)
	{
	}

	nexNNAInfo(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		principalInfo.writeData(pb);
		pb->writeU8(countryCode);
		pb->writeU8(countrySubCode);
	}
	void readData(nexPacketBuffer* pb) override
	{
		principalInfo.readData(pb);
		countryCode = pb->readU8();
		countrySubCode = pb->readU8();
	}
public:
	uint8 countryCode;
	uint8 countrySubCode;
	nexPrincipalBasicInfo principalInfo;
};

class nexPrincipalPreference : public nexType
{
public:
	nexPrincipalPreference() = default;

	nexPrincipalPreference(uint8 ukn0, uint8 ukn1, uint8 ukn2)
	{
		this->showOnline = ukn0;
		this->showGame = ukn1;
		this->blockFriendRequests = ukn2;
	}

	nexPrincipalPreference(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeU8(showOnline);
		pb->writeU8(showGame);
		pb->writeU8(blockFriendRequests);
	}
	
	void readData(nexPacketBuffer* pb) override
	{
		showOnline = pb->readU8();
		showGame = pb->readU8();
		blockFriendRequests = pb->readU8();
	}
public:
	uint8 showOnline;
	uint8 showGame;
	uint8 blockFriendRequests;
};

class nexComment : public nexType
{
public:
	nexComment()
	{

	}

	nexComment(uint8 todo)
	{
		cemu_assert_unimplemented();
	}

	nexComment(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}

	void readData(nexPacketBuffer* pb) override
	{
		ukn0 = pb->readU8();
		char stringBuffer[1024];
		pb->readString(stringBuffer, 1024);
		stringBuffer[1023] = '\0';
		commentString = std::string(stringBuffer);
		ukn1 = pb->readU64();
	}
public:
	uint8 ukn0;
	std::string commentString;
	uint64 ukn1;
};

class nexFriendRequestMessage : public nexType
{
public:
	nexFriendRequestMessage()
	{

	}

	nexFriendRequestMessage(uint8 todo)
	{
		cemu_assert_unimplemented();
	}

	nexFriendRequestMessage(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}
	void readData(nexPacketBuffer* pb) override
	{
		messageId = pb->readU64();
		isMarkedAsReceived = pb->readU8();
		ukn2 = pb->readU8();
		char stringBuffer[1024];
		pb->readString(stringBuffer, sizeof(stringBuffer));
		stringBuffer[1023] = '\0';
		commentStr = std::string(stringBuffer);
		ukn4 = pb->readU8();
		pb->readString(stringBuffer, sizeof(stringBuffer));
		stringBuffer[1023] = '\0';
		ukn5Str = std::string(stringBuffer);
		gameKey.readData(pb);
		ukn7 = pb->readU64();
		expireTimestamp = pb->readU64();
	}
public:
	uint64 messageId;
	uint8 isMarkedAsReceived;
	uint8 ukn2;
	std::string commentStr;
	uint8 ukn4;
	std::string ukn5Str;
	nexGameKey gameKey;
	uint64 ukn7;
	uint64 expireTimestamp;
};

class nexFriendRequest : public nexType
{
public:
	nexFriendRequest()
	{
	}

	nexFriendRequest(uint64 titleId, uint16 ukn)
	{
		cemu_assert_unimplemented();
	}

	nexFriendRequest(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	const char* getMetaName() override
	{
		return "FriendRequest";
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}

	void readData(nexPacketBuffer* pb) override
	{
		principalInfo.readData(pb);
		message.readData(pb);
		ukn = pb->readU64();
	}
public:
	nexPrincipalBasicInfo	principalInfo;
	nexFriendRequestMessage	message;
	uint64					ukn;
};

class nexFriend : public nexType
{
public:
	nexFriend()
	{
	}

	nexFriend(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	const char* getMetaName() override
	{
		return "FriendInfo";
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}

	void readData(nexPacketBuffer* pb) override
	{
		nnaInfo.readData(pb);
		presence.readData(pb);
		comment.readData(pb);
		friendsSinceTimestamp = pb->readU64();
		lastOnlineTimestamp = pb->readU64();
		ukn6 = pb->readU64();
	}
public:
	nexNNAInfo		nnaInfo;
	nexPresenceV2	presence;
	nexComment		comment;
	uint64			friendsSinceTimestamp;
	uint64			lastOnlineTimestamp;
	uint64			ukn6;
};

class nexBlacklisted : public nexType
{
public:
	nexBlacklisted()
	{
	}

	nexBlacklisted(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}
	void readData(nexPacketBuffer* pb) override
	{
		basicInfo.readData(pb);
		gameKey.readData(pb);
		ukn = pb->readU64();
	}
public:
	nexPrincipalBasicInfo basicInfo;
	nexGameKey gameKey;
	uint64 ukn;
};


class nexPersistentNotification : public nexType
{
public:
	nexPersistentNotification()
	{
	}

	nexPersistentNotification(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}
	void readData(nexPacketBuffer* pb) override
	{
		messageId = pb->readU64();
		pid1 = pb->readU32();
		pid2 = pb->readU32();
		type = pb->readU32();
		pb->readStdString(msg);
	}
public:
	uint64		messageId;
	uint32		pid1; // principal id 1 (might differ depending on type)
	uint32		pid2; // principal id 2 (might differ depending on type)
	uint32		type;
	std::string	msg;
};

class NexFriends
{
public:
  	using RpcErrorCode = int; // replace with enum class later

	static const int ERR_NONE = 0;
	static const int ERR_RPC_FAILED = 1;
	static const int ERR_UNEXPECTED_RESULT = 2;
	static const int ERR_NOT_CONNECTED = 3;

	enum NOTIFICATION_TYPE
	{
		NOTIFICATION_TYPE_ONLINE = 0,

		NOTIFICATION_TYPE_FRIEND_LOGIN = 4,
		NOTIFICATION_TYPE_FRIEND_LOGOFF = 5,
		NOTIFICATION_TYPE_FRIEND_PRESENCE_CHANGE = 6,

		NOTIFICATION_TYPE_ADDED_FRIEND = 9,
		NOTIFICATION_TYPE_REMOVED_FRIEND = 10,
		NOTIFICATION_TYPE_ADDED_OUTGOING_REQUEST = 11,
		NOTIFICATION_TYPE_REMOVED_OUTGOING_REQUEST = 12,
		NOTIFICATION_TYPE_ADDED_INCOMING_REQUEST = 17,
		NOTIFICATION_TYPE_REMOVED_INCOMING_REQUEST = 18


	};

public:
	NexFriends(uint32 authServerIp, uint16 authServerPort, const char* accessKey, uint32 pid, const char* nexPassword, const char* nexToken, const char* nnid, uint8* miiData, const wchar_t* miiNickname, uint8 countryCode, nexPresenceV2& myPresence);

	~NexFriends();

	std::string getAccountNameByPid(uint32 principalId);
	int getPendingFriendRequestCount();

	bool requestGetAllInformation();
	bool addProvisionalFriendByPidGuessed(uint32 principalId);

	// synchronous API (returns immediately)
	bool requestGetAllInformation(std::function<void(uint32)> cb);
	void getFriendPIDs(uint32* pidList, uint32* pidCount, sint32 offset, sint32 count, bool includeFriendRequests);
	void getFriendRequestPIDs(uint32* pidList, uint32* pidCount, sint32 offset, sint32 count, bool includeIncoming, bool includeOutgoing);
	bool getFriendByPID(nexFriend& friendData, uint32 pid);
	bool getFriendRequestByPID(nexFriendRequest& friendRequestData, bool* isIncoming, uint32 searchedPid);
	bool getFriendRequestByMessageId(nexFriendRequest& friendRequestData, bool* isIncoming, uint64 messageId);
	bool isOnline();
	void getMyPreference(nexPrincipalPreference& preference);

	// asynchronous API (data has to be requested)
	bool addProvisionalFriend(char* name, std::function<void(RpcErrorCode)> cb);
	void addFriendRequest(uint32 pid, const char* comment, std::function<void(RpcErrorCode)> cb);
	void requestPrincipleBaseInfoByPID(uint32* pidList, sint32 count, const std::function<void(RpcErrorCode result, std::span<nexPrincipalBasicInfo> basicInfo)>& cb);
	void removeFriend(uint32 pid, std::function<void(RpcErrorCode)> cb);
	void cancelOutgoingProvisionalFriendRequest(uint32 pid, std::function<void(RpcErrorCode)> cb);
	void markFriendRequestsAsReceived(uint64* messageIdList, sint32 count, std::function<void(RpcErrorCode)> cb);
	void acceptFriendRequest(uint64 messageId, std::function<void(RpcErrorCode)> cb);
	void deleteFriendRequest(uint64 messageId, std::function<void(RpcErrorCode)> cb); // rejecting incoming friend request (differs from blocking friend requests)
	bool updatePreferencesAsync(const nexPrincipalPreference newPreferences, std::function<void(RpcErrorCode)> cb);
	void updateMyPresence(nexPresenceV2& myPresence);

	void setNotificationHandler(void(*notificationHandler)(NOTIFICATION_TYPE notificationType, uint32 pid));

	void update();

	void processServerNotification(uint32 notificationType, uint32 pid, nexPacketBuffer* notificationData);

private:
	void doAsyncLogin();
	void initiateLogin();

	static void handleResponse_acceptFriendRequest(nexService* nex, nexServiceResponse_t* response);
	static void handleResponse_getAllInformation(nexServiceResponse_t* response, NexFriends* nexFriends, std::function<void(uint32)> cb);

	void generateNotification(NOTIFICATION_TYPE notificationType, uint32 pid);
	void trackNotifications();
	
	// notification-service handlers
	void processServerNotification_friendOffline(uint32 pid);
	void processServerNotification_presenceChange(uint32 pid, nexPresenceV2& presence);
	void processServerNotification_removeFriendOrFriendRequest(uint32 pid);
	void processServerNotification_incomingFriendRequest(uint32 pid, nexFriendRequest& friendRequest);
	void processServerNotification_addedFriend(uint32 pid, nexFriend& frd);

private:
	void(*notificationHandler)(NOTIFICATION_TYPE notificationType, uint32 pid);
	bool isCurrentlyConnected;
	bool hasData; // set after connect when information request response was received
	bool firstInformationRequest;
	nexService* nexCon;
	uint8 miiData[FFL_SIZE];
	std::string miiNickname;
	char nnid[96];
	uint32 pid;
	uint8 countryCode;
	// login tracking
	std::recursive_mutex mtx_login;
	bool loginInProcess;
	uint32 lastLoginAttemptTime;
	uint32 numFailedLogins;
	uint32 numSuccessfulLogins;
	// auth
	struct  
	{
		uint32 serverIp;
		uint16 port;
		std::string accessKey;
		std::string nexPassword;
		std::string nexToken;
	}auth;
	// local friend state
	nexPresenceV2 myPresence;
	nexPrincipalPreference myPreference;

	std::recursive_mutex mtx_lists;
	std::vector<nexFriend> list_friends;
	std::vector<nexFriendRequest> list_friendReqOutgoing;
	std::vector<nexFriendRequest> list_friendReqIncoming;
	struct  
	{
		std::vector<nexFriend> list_friends;
		std::vector<nexFriendRequest> list_friendReqOutgoing;
		std::vector<nexFriendRequest> list_friendReqIncoming;
	}previousState;
};
