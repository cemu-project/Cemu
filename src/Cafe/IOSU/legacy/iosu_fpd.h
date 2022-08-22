#pragma once

namespace iosu
{
	namespace fpd
	{
		typedef struct
		{
			/* +0x0 */ uint16be year;
			/* +0x2 */ uint8 month;
			/* +0x3 */ uint8 day;
			/* +0x4 */ uint8 hour;
			/* +0x5 */ uint8 minute;
			/* +0x6 */ uint8 second;
			/* +0x7 */ uint8 padding;
		}fpdDate_t;

		static_assert(sizeof(fpdDate_t) == 8);

		typedef struct
		{
			/* +0x000 */ uint8 type; // type(Non-Zero -> Friend, 0 -> Friend request ? )
			/* +0x001 */ uint8 _padding1[7];
			/* +0x008 */ uint32be pid;
			/* +0x00C */ char nnid[0x10 + 1];
			/* +0x01D */ uint8 ukn01D;
			/* +0x01E */ uint8 _padding1E[2];
			/* +0x020 */ uint16be screenname[10 + 1];
			/* +0x036 */ uint8 ukn036;
			/* +0x037 */ uint8 _padding037;
			/* +0x038 */ uint8 mii[0x60];
			/* +0x098 */ fpdDate_t uknDate;
			// sub struct (the part above seems to be shared with friend requests)
			union
			{
				struct 
				{
					/* +0x0A0 */ uint8 ukn0A0; // country code?
					/* +0x0A1 */ uint8 ukn0A1; // country subcode?
					/* +0x0A2 */ uint8 _paddingA2[2];
					/* +0x0A4 */ uint32be ukn0A4;
					/* +0x0A8 */ uint64 gameKeyTitleId;
					/* +0x0B0 */ uint16be gameKeyUkn;
					/* +0x0B2 */ uint8 _paddingB2[6];
					/* +0x0B8 */ uint32 ukn0B8;
					/* +0x0BC */ uint32 ukn0BC;
					/* +0x0C0 */ uint32 ukn0C0;
					/* +0x0C4 */ uint32 ukn0C4;
					/* +0x0C8 */ uint32 ukn0C8;
					/* +0x0CC */ uint32 ukn0CC;
					/* +0x0D0 */ uint8  appSpecificData[0x14];
					/* +0x0E4 */ uint8  ukn0E4;
					/* +0x0E5 */ uint8  _paddingE5;
					/* +0x0E6 */ uint16 uknStr[0x80]; // game mode description (could be larger)
					/* +0x1E6 */ uint8 _padding1E6[0x1EC - 0x1E6];
					/* +0x1EC */ uint8 isOnline;
					/* +0x1ED */ uint8 _padding1ED[3];
					// some other sub struct?
					/* +0x1F0 */ uint8 ukn1F0;
					/* +0x1F1 */ uint8 _padding1F1;
					/* +0x1F2 */ uint16be statusMessage[16 + 1]; // pops up every few seconds in friend list (ingame character name?)
					/* +0x214 */ uint8 _padding214[4];
					/* +0x218 */ fpdDate_t uknDate218;
					/* +0x220 */ fpdDate_t lastOnline;
				}friendExtraData;
				struct 
				{
					/* +0x0A0 */ uint64 messageId; // guessed
					/* +0x0A8 */ uint8 ukn0A8;
					/* +0x0A9 */ uint8 ukn0A9; // comment language? (guessed)
					/* +0x0AA */ uint16be comment[0x40];
					/* +0x12A */ uint8 ukn12A; // ingame name language? (guessed)
					/* +0x12B */ uint8 _padding12B;
					/* +0x12C */ uint16be uknMessage[18]; // exact length unknown, could be shorter (ingame screen name?)
					/* +0x150 */ uint64 gameKeyTitleId;
					/* +0x158 */ uint16be gameKeyUkn;
					/* +0x15A */ uint8 _padding[6];
					/* +0x160 */ fpdDate_t uknData0;
					/* +0x168 */ fpdDate_t uknData1;
				}requestExtraData;
			};
		}friendData_t;

		static_assert(sizeof(friendData_t) == 0x228, "");
		static_assert(offsetof(friendData_t, nnid) == 0x00C, "");
		static_assert(offsetof(friendData_t, friendExtraData.gameKeyTitleId) == 0x0A8, "");
		static_assert(offsetof(friendData_t, friendExtraData.appSpecificData) == 0x0D0, "");
		static_assert(offsetof(friendData_t, friendExtraData.uknStr) == 0x0E6, "");
		static_assert(offsetof(friendData_t, friendExtraData.ukn1F0) == 0x1F0, "");
		
		static_assert(offsetof(friendData_t, requestExtraData.messageId) == 0x0A0, "");
		static_assert(offsetof(friendData_t, requestExtraData.comment) == 0x0AA, "");
		static_assert(offsetof(friendData_t, requestExtraData.uknMessage) == 0x12C, "");
		static_assert(offsetof(friendData_t, requestExtraData.gameKeyTitleId) == 0x150, "");
		static_assert(offsetof(friendData_t, requestExtraData.uknData1) == 0x168, "");

		typedef struct
		{
			/* +0x00 */ uint32be pid;
			/* +0x04 */ char nnid[0x11];
			/* +0x15 */ uint8 ukn15;
			/* +0x16 */ uint8 uknOrPadding[2];
			/* +0x18 */ uint16be screenname[11];
			/* +0x2E */ uint8 ukn2E; // bool option
			/* +0x2F */ uint8 ukn2F;
			/* +0x30 */ uint8 miiData[0x60];
			/* +0x90 */ fpdDate_t uknDate90;
		}friendBasicInfo_t; // size is 0x98

		static_assert(sizeof(friendBasicInfo_t) == 0x98, "");
		static_assert(offsetof(friendBasicInfo_t, nnid) == 0x04, "");
		static_assert(offsetof(friendBasicInfo_t, ukn15) == 0x15, "");
		static_assert(offsetof(friendBasicInfo_t, screenname) == 0x18, "");
		static_assert(offsetof(friendBasicInfo_t, ukn2E) == 0x2E, "");
		static_assert(offsetof(friendBasicInfo_t, miiData) == 0x30, "");

		typedef struct  
		{
			/* +0x000 */ uint32be pid;
			/* +0x004 */ uint8 nnid[17]; // guessed type
			/* +0x015 */ uint8 _uknOrPadding15;
			/* +0x016 */ uint8 _uknOrPadding16;
			/* +0x017 */ uint8 _uknOrPadding17;
			/* +0x018 */ uint16be screenname[11];
			/* +0x02E */ uint8 ukn2E; // bool option
			/* +0x02F */ uint8 ukn2F;  // ukn
			/* +0x030 */ uint8 miiData[0x60];
			/* +0x090 */ fpdDate_t uknDate;
			/* +0x098 */ uint64 ukn98;
			/* +0x0A0 */ uint8 isMarkedAsReceived;
			/* +0x0A1 */ uint8 uknA1;
			/* +0x0A2 */ uint16be message[0x40];
			/* +0x122 */ uint8 ukn122;
			/* +0x123 */ uint8 _padding123;
			/* +0x124 */ uint16be uknString2[18];
			/* +0x148 */ uint64 gameKeyTitleId;
			/* +0x150 */ uint16be gameKeyUkn;
			/* +0x152 */ uint8 _padding152[6];
			/* +0x158 */ fpdDate_t uknDate2;
			/* +0x160 */ fpdDate_t expireDate;
		}friendRequest_t;

		static_assert(sizeof(friendRequest_t) == 0x168, "");
		static_assert(offsetof(friendRequest_t, uknDate) == 0x090, "");
		static_assert(offsetof(friendRequest_t, message) == 0x0A2, "");
		static_assert(offsetof(friendRequest_t, uknString2) == 0x124, "");
		static_assert(offsetof(friendRequest_t, gameKeyTitleId) == 0x148, "");

		typedef struct
		{
			/* +0x00 */ uint32be joinFlagMask;
			/* +0x04 */ uint32be matchmakeType;
			/* +0x08 */ uint32be joinGameId;
			/* +0x0C */ uint32be joinGameMode;
			/* +0x10 */ uint32be hostPid;
			/* +0x14 */ uint32be groupId;
			/* +0x18 */ uint8    appSpecificData[0x14];
		}gameMode_t;

		static_assert(sizeof(gameMode_t) == 0x2C, "");

		typedef struct  
		{
			gameMode_t gameMode;
			/* +0x2C */ uint8    region;
			/* +0x2D */ uint8    regionSubcode;
			/* +0x2E */ uint8    platform;
			/* +0x2F */ uint8    _padding2F;
			/* +0x30 */ uint8	 isOnline;
			/* +0x31 */ uint8    isValid;
			/* +0x32 */ uint8	 padding[2]; // guessed
		}friendPresence_t;

		static_assert(sizeof(friendPresence_t) == 0x34, "");
		static_assert(offsetof(friendPresence_t, region) == 0x2C, "");
		static_assert(offsetof(friendPresence_t, isOnline) == 0x30, "");

		static const int RELATIONSHIP_INVALID = 0;
		static const int RELATIONSHIP_FRIENDREQUEST_OUT = 1;
		static const int RELATIONSHIP_FRIENDREQUEST_IN = 2;
		static const int RELATIONSHIP_FRIEND = 3;

		typedef struct
		{
			uint32 requestCode;
			union
			{
				struct  
				{
					MEMPTR<void> ptr;
				}common;
				struct  
				{
					MPTR funcPtr;
					MPTR custom;
				}loginAsync;
				struct  
				{
					MEMPTR<uint32be> pidList;
					uint32 startIndex;
					uint32 maxCount;
				}getFriendList;
				struct
				{
					MEMPTR<friendData_t> friendData;
					MEMPTR<uint32be> pidList;
					uint32 count;
				}getFriendListEx;
				struct
				{
					MEMPTR<friendRequest_t> friendRequest;
					MEMPTR<uint32be> pidList;
					uint32 count;
				}getFriendRequestListEx;
				struct
				{
					uint32 pid;
					MPTR funcPtr;
					MPTR custom;
				}addOrRemoveFriend;
				struct
				{
					uint64 messageId;
					MPTR funcPtr;
					MPTR custom;
				}cancelOrAcceptFriendRequest;
				struct
				{
					uint32 pid;
					MEMPTR<uint16be> message;
					MPTR funcPtr;
					MPTR custom;
				}addFriendRequest;
				struct
				{
					MEMPTR<uint64> messageIdList;
					uint32 count;
					MPTR funcPtr;
					MPTR custom;
				}markFriendRequest;
				struct
				{
					MEMPTR<friendBasicInfo_t> basicInfo;
					MEMPTR<uint32be> pidList;
					sint32 count;
					MPTR funcPtr;
					MPTR custom;
				}getBasicInfo;
				struct  
				{
					uint32 notificationMask;
					MPTR funcPtr;
					MPTR custom;
				}setNotificationHandler;
				struct  
				{
					MEMPTR<char> accountIds;
					MEMPTR<uint32be> pidList;
					sint32 count;
				}getFriendAccountId;
				struct  
				{
					MEMPTR<uint16be> nameList;
					MEMPTR<uint32be> pidList;
					sint32 count;
					bool replaceNonAscii;
					MEMPTR<uint8> languageList;
				}getFriendScreenname;
				struct
				{
					uint8* miiList;
					MEMPTR<uint32be> pidList;
					sint32 count;
				}getFriendMii;
				struct
				{
					uint8* presenceList;
					MEMPTR<uint32be> pidList;
					sint32 count;
				}getFriendPresence;
				struct
				{
					uint8* relationshipList;
					MEMPTR<uint32be> pidList;
					sint32 count;
				}getFriendRelationship;
				struct
				{
					MEMPTR<gameMode_t> gameMode;
					MEMPTR<uint16be> gameModeMessage;
				}updateGameMode;
			};

			// output
			uint32 returnCode; // return value
			union
			{
				struct  
				{
					uint32 numReturnedCount;
				}resultGetFriendList;
				struct
				{
					uint32 u32;
				}resultU32;
			};
		}iosuFpdCemuRequest_t;

		// custom dev/fpd protocol (Cemu only)
		#define IOSU_FPD_REQUEST_CEMU			(0xEE)

		// FPD request Cemu subcodes
		enum
		{
			_IOSU_FPD_NONE,
			IOSU_FPD_INITIALIZE,
			IOSU_FPD_SET_NOTIFICATION_HANDLER,
			IOSU_FPD_LOGIN_ASYNC,
			IOSU_FPD_IS_ONLINE,
			IOSU_FPD_IS_PREFERENCE_VALID,

			IOSU_FPD_UPDATE_GAMEMODE,

			IOSU_FPD_GET_MY_PRINCIPAL_ID,
			IOSU_FPD_GET_MY_ACCOUNT_ID,
			IOSU_FPD_GET_MY_MII,
			IOSU_FPD_GET_MY_SCREENNAME,
			
			IOSU_FPD_GET_FRIEND_ACCOUNT_ID,
			IOSU_FPD_GET_FRIEND_SCREENNAME,
			IOSU_FPD_GET_FRIEND_MII,
			IOSU_FPD_GET_FRIEND_PRESENCE,
			IOSU_FPD_GET_FRIEND_RELATIONSHIP,
			
			IOSU_FPD_GET_FRIEND_LIST,
			IOSU_FPD_GET_FRIENDREQUEST_LIST,
			IOSU_FPD_GET_FRIEND_LIST_ALL,
			IOSU_FPD_GET_FRIEND_LIST_EX,
			IOSU_FPD_GET_FRIENDREQUEST_LIST_EX,
			IOSU_FPD_ADD_FRIEND,
			IOSU_FPD_ADD_FRIEND_REQUEST,
			IOSU_FPD_REMOVE_FRIEND_ASYNC,
			IOSU_FPD_CANCEL_FRIEND_REQUEST_ASYNC,
			IOSU_FPD_ACCEPT_FRIEND_REQUEST_ASYNC,
			IOSU_FPD_MARK_FRIEND_REQUEST_AS_RECEIVED_ASYNC,
			IOSU_FPD_GET_BASIC_INFO_ASYNC,
		};

		void Initialize();
	}
}