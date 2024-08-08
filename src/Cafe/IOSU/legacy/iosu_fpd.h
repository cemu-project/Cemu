#pragma once
#include "Cafe/IOSU/iosu_types_common.h"
#include "Common/CafeString.h"

namespace iosu
{
	namespace fpd
	{
		struct FPDDate
		{
			/* +0x0 */ uint16be year;
			/* +0x2 */ uint8 month;
			/* +0x3 */ uint8 day;
			/* +0x4 */ uint8 hour;
			/* +0x5 */ uint8 minute;
			/* +0x6 */ uint8 second;
			/* +0x7 */ uint8 padding;
		};

		static_assert(sizeof(FPDDate) == 8);

		struct RecentPlayRecordEx
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
			/* +0x60 */ iosu::fpd::FPDDate date;
		};

		static_assert(sizeof(RecentPlayRecordEx) == 0x68, "");
		static_assert(offsetof(RecentPlayRecordEx, ukn06) == 0x06, "");
		static_assert(offsetof(RecentPlayRecordEx, ukn50) == 0x50, "");

		struct GameKey
		{
			/* +0x00 */ uint64be titleId;
			/* +0x08 */ uint16be ukn08;
			/* +0x0A */ uint8 _padding0A[6];
		};
		static_assert(sizeof(GameKey) == 0x10);

		struct Profile
		{
			uint8be region;
			uint8be regionSubcode;
			uint8be platform;
			uint8be ukn3;
		};
		static_assert(sizeof(Profile) == 0x4);

		struct GameMode
		{
			/* +0x00 */ uint32be joinFlagMask;
			/* +0x04 */ uint32be matchmakeType;
			/* +0x08 */ uint32be joinGameId;
			/* +0x0C */ uint32be joinGameMode;
			/* +0x10 */ uint32be hostPid;
			/* +0x14 */ uint32be groupId;
			/* +0x18 */ uint8    appSpecificData[0x14];
		};
		static_assert(sizeof(GameMode) == 0x2C);

		struct FriendData
		{
			/* +0x000 */ uint8 type; // type (Non-Zero -> Friend, 0 -> Friend request ? )
			/* +0x001 */ uint8 _padding1[7];
			/* +0x008 */ uint32be pid;
			/* +0x00C */ char nnid[0x10 + 1];
			/* +0x01D */ uint8 ukn01D;
			/* +0x01E */ uint8 _padding1E[2];
			/* +0x020 */ uint16be screenname[10 + 1];
			/* +0x036 */ uint8 ukn036;
			/* +0x037 */ uint8 _padding037;
			/* +0x038 */ uint8 mii[0x60];
			/* +0x098 */ FPDDate uknDate;
			// sub struct (the part above seems to be shared with friend requests)
			union
			{
				struct 
				{
					/* +0x0A0 */ Profile profile; // this is returned for nn_fp.GetFriendProfile
					/* +0x0A4 */ uint32be ukn0A4;
					/* +0x0A8 */ GameKey gameKey;
					/* +0x0B8 */ GameMode gameMode;
					/* +0x0E4 */ CafeWideString<0x82> gameModeDescription;
					/* +0x1E8 */ Profile profile1E8; // how does it differ from the one at 0xA0? Returned by GetFriendPresence
					/* +0x1EC */ uint8 isOnline;
					/* +0x1ED */ uint8 _padding1ED[3];
					// some other sub struct?
					/* +0x1F0 */ CafeWideString<0x12> comment; // pops up every few seconds in friend list
					/* +0x214 */ uint32be _padding214;
					/* +0x218 */ FPDDate approvalTime;
					/* +0x220 */ FPDDate lastOnline;
				}friendExtraData;
				struct 
				{
					/* +0x0A0 */ uint64be messageId; // guessed. If 0, then relationship is FRIENDSHIP_REQUEST_OUT, otherwise FRIENDSHIP_REQUEST_IN
					/* +0x0A8 */ uint8 ukn0A8;
					/* +0x0A9 */ uint8 ukn0A9; // comment language? (guessed)
					/* +0x0AA */ uint16be comment[0x40];
					/* +0x12A */ uint8 ukn12A; // ingame name language? (guessed)
					/* +0x12B */ uint8 _padding12B;
					/* +0x12C */ uint16be uknMessage[18]; // exact length unknown, could be shorter (ingame screen name?)
					/* +0x150 */ uint64 gameKeyTitleId;
					/* +0x158 */ uint16be gameKeyUkn;
					/* +0x15A */ uint8 _padding[6];
					/* +0x160 */ FPDDate uknData0;
					/* +0x168 */ FPDDate uknData1;
				}requestExtraData;
			};
		};
		static_assert(sizeof(FriendData) == 0x228);
		static_assert(offsetof(FriendData, friendExtraData.gameKey) == 0x0A8);
		static_assert(offsetof(FriendData, friendExtraData.gameModeDescription) == 0x0E4);
		static_assert(offsetof(FriendData, friendExtraData.comment) == 0x1F0);
		
		static_assert(offsetof(FriendData, requestExtraData.messageId) == 0x0A0);
		static_assert(offsetof(FriendData, requestExtraData.comment) == 0x0AA);
		static_assert(offsetof(FriendData, requestExtraData.uknMessage) == 0x12C);
		static_assert(offsetof(FriendData, requestExtraData.gameKeyTitleId) == 0x150);
		static_assert(offsetof(FriendData, requestExtraData.uknData1) == 0x168);

		struct FriendBasicInfo
		{
			/* +0x00 */ uint32be pid;
			/* +0x04 */ char nnid[0x11];
			/* +0x15 */ uint8 ukn15;
			/* +0x16 */ uint8 uknOrPadding[2];
			/* +0x18 */ uint16be screenname[11];
			/* +0x2E */ uint8 ukn2E; // bool option
			/* +0x2F */ uint8 ukn2F;
			/* +0x30 */ uint8 miiData[0x60];
			/* +0x90 */ FPDDate uknDate90;
		};

		static_assert(sizeof(FriendBasicInfo) == 0x98);
		static_assert(offsetof(FriendBasicInfo, nnid) == 0x04);
		static_assert(offsetof(FriendBasicInfo, ukn15) == 0x15);
		static_assert(offsetof(FriendBasicInfo, screenname) == 0x18);
		static_assert(offsetof(FriendBasicInfo, ukn2E) == 0x2E);
		static_assert(offsetof(FriendBasicInfo, miiData) == 0x30);

		struct FriendRequest
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
			/* +0x090 */ FPDDate uknDate;
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
			/* +0x158 */ FPDDate uknDate2;
			/* +0x160 */ FPDDate expireDate;
		};

		static_assert(sizeof(FriendRequest) == 0x168);
		static_assert(offsetof(FriendRequest, uknDate) == 0x090);
		static_assert(offsetof(FriendRequest, message) == 0x0A2);
		static_assert(offsetof(FriendRequest, uknString2) == 0x124);
		static_assert(offsetof(FriendRequest, gameKeyTitleId) == 0x148);

		struct FriendPresence
		{
			GameMode gameMode;
			/* +0x2C */ Profile  profile;
			/* +0x30 */ uint8	 isOnline;
			/* +0x31 */ uint8    isValid;
			/* +0x32 */ uint8	 padding[2]; // guessed
		};
		static_assert(sizeof(FriendPresence) == 0x34);
		static_assert(offsetof(FriendPresence, isOnline) == 0x30);

		struct FPDNotification
		{
			betype<uint32> type;
			betype<uint32> pid;
		};
		static_assert(sizeof(FPDNotification) == 8);

		struct FPDPreference
		{
			uint8be showOnline; // show online status to others
			uint8be showGame; // show played game to others
			uint8be blockFriendRequests; // block friend requests
			uint8be ukn; // probably padding?
		};
		static_assert(sizeof(FPDPreference) == 4);

		static const int RELATIONSHIP_INVALID = 0;
		static const int RELATIONSHIP_FRIENDREQUEST_OUT = 1;
		static const int RELATIONSHIP_FRIENDREQUEST_IN = 2;
		static const int RELATIONSHIP_FRIEND = 3;

		static const int GAMEMODE_MAX_MESSAGE_LENGTH = 0x80; // limit includes null-terminator character, so only 0x7F actual characters can be used
		static const int MY_COMMENT_LENGTH = 0x12;

		enum class FPD_REQUEST_ID
		{
			LoginAsync = 0x2775,
			HasLoggedIn = 0x2777,
			IsOnline = 0x2778,
			GetMyPrincipalId = 0x27D9,
			GetMyAccountId = 0x27DA,
			GetMyScreenName = 0x27DB,
			GetMyMii = 0x27DC,
			GetMyProfile = 0x27DD,
			GetMyPreference = 0x27DE,
			GetMyPresence = 0x27DF,
			IsPreferenceValid = 0x27E0,
			GetFriendList = 0x283D,
			GetFriendListAll = 0x283E,
			GetFriendAccountId = 0x283F,
			GetFriendScreenName = 0x2840,
			GetFriendPresence = 0x2845,
			GetFriendRelationship = 0x2846,
			GetFriendMii = 0x2841,
			GetBlackList = 0x28A1,
			GetFriendRequestList = 0x2905,
			UpdateGameModeVariation1 = 0x2969, // there seem to be two different requestIds for the 2-param and 3-param version of UpdateGameMode,
			UpdateGameModeVariation2 = 0x296A, // but the third parameter is never used and the same handler is used for both
			AddFriendAsyncByPid = 0x29CD,
			AddFriendAsyncByXXX = 0x29CE, // probably by name?
			GetRequestBlockSettingAsync = 0x2B5D,
			GetMyComment = 0x4EE9,
			GetMyPlayingGame = 0x4EEA,
			CheckSettingStatusAsync = 0x7596,
			GetFriendListEx = 0x75F9,
			GetFriendRequestListEx = 0x76C1,
			UpdateCommentAsync = 0x7726,
			UpdatePreferenceAsync = 0x7727,
			RemoveFriendAsync = 0x7789,
			DeleteFriendFlagsAsync = 0x778A,
			AddFriendRequestByPlayRecordAsync = 0x778B,
			CancelFriendRequestAsync = 0x778C,
			AcceptFriendRequestAsync = 0x7851,
			DeleteFriendRequestAsync = 0x7852,
			MarkFriendRequestsAsReceivedAsync = 0x7854,
			GetBasicInfoAsync = 0x7919,
			SetLedEventMask = 0x9D0B,
			SetNotificationMask = 0x15ff5,
			GetNotificationAsync = 0x15FF6,
		};

		using FriendPID = uint32;

		IOSUModule* GetModule();
	}
}
