#pragma once
#include "config/CemuConfig.h" // for ConsoleLanguage
#include "config/NetworkSettings.h" // for NetworkService
#include "config/ActiveSettings.h" // for GetNetworkService()

enum class NAPI_RESULT
{
	SUCCESS = 0,
	FAILED = 1, // general failure
	XML_ERROR = 2, // XML response unexpected
	DATA_ERROR = 3, // incorrect values
	SERVICE_ERROR = 4, // server reply indicates error. Extended error code (serviceError) is set
};

namespace NAPI
{
	// common auth info structure shared by ACT, ECS and IAS service
	struct AuthInfo
	{
		// nnid
		std::string accountId;
		std::array<uint8, 32> passwordHash;
		// console
		uint32 deviceId;
		std::string serial;
		CafeConsoleRegion region;
		std::string country;
		std::string deviceCertBase64;

		uint64 getDeviceIdWithPlatform()
		{
			uint64 deviceType = 5;
			return (deviceType << 32) | ((uint64)deviceId);
		}

		// IAS token (for ECS and other SOAP service requests)
		struct 
		{
			std::string accountId; // console account id
			std::string deviceToken;
		}IASToken;

		// service selection, if not set fall back to global setting
		std::optional<NetworkService> serviceOverwrite;

		NetworkService GetService() const
		{
			return serviceOverwrite.value_or(ActiveSettings::GetNetworkService());
		}
	};

	bool NAPI_MakeAuthInfoFromCurrentAccount(AuthInfo& authInfo); // helper function. Returns false if online credentials/dumped files are not available

	/* Shared result */

	// ErrorCodes IAS:
	//	928		-> IAS - Device cert does not verify.
	//  954     -> IAS - Invalid Challenge

	enum class EC_ERROR_CODE
	{
		NONE = 0,
		ECS_INVALID_DEVICE_TOKEN = 903,
		IAS_DEVICE_CERT_ERROR = 928, // either invalid signature or mismatching incorrect device cert chain
		IAS_INVALID_CHALLENGE = 954,
		ECS_NO_PERMISSION = 701, // AccountGetETickets
		NUS_SOAP_INVALID_VERSION = 1301, // seen when accidentally passing wrong SOAP version to GetSystemCommonETicket
	};

	enum class ACT_ERROR_CODE // are these shared with EC_ERROR_CODE?
	{
		NONE = 0,
		ACT_GAME_SERVER_NOT_FOUND = 1021, // seen when passing wrong serverId to nex token request
	};

	struct _NAPI_CommonResultSOAP
	{
		NAPI_RESULT apiError{ NAPI_RESULT::FAILED };
		EC_ERROR_CODE serviceError{ EC_ERROR_CODE::NONE };

		bool isValid() const
		{
			return apiError == NAPI_RESULT::SUCCESS;
		}
	};

	struct _NAPI_CommonResultACT
	{
		NAPI_RESULT apiError{ NAPI_RESULT::FAILED };
		ACT_ERROR_CODE serviceError{ ACT_ERROR_CODE::NONE };

		bool isValid() const
		{
			return apiError == NAPI_RESULT::SUCCESS;
		}
	};

	/* account service (account.nintendo.net) */

	struct ACTGetProfileResult 
	{
		int todo;
	};

	bool ACT_GetProfile(AuthInfo& authInfo, ACTGetProfileResult& result);

	struct ACTNexToken
	{
		/* +0x000 */ char token[0x201];
		/* +0x201 */ uint8 _padding201[3];
		/* +0x204 */ char nexPassword[0x41];
		/* +0x245 */ uint8 _padding245[3];
		/* +0x248 */ char host[0x10];
		/* +0x258 */ uint16be port;
		/* +0x25A */ uint8 _padding25A[2];
	};

	static_assert(sizeof(ACTNexToken) == 0x25C);

	struct ACTGetNexTokenResult : public _NAPI_CommonResultACT
	{
		ACTNexToken nexToken;
	};

	ACTGetNexTokenResult ACT_GetNexToken_WithCache(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion, uint32 serverId);

	struct ACTGetIndependentTokenResult : public _NAPI_CommonResultACT
	{
		std::string token;
		sint64 expiresIn{};
	};

	ACTGetIndependentTokenResult ACT_GetIndependentToken_WithCache(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion, std::string_view clientId);

	struct ACTConvertNnidToPrincipalIdResult : public _NAPI_CommonResultACT
	{
		bool isFound{false};
		uint32 principalId{};
	};

	ACTConvertNnidToPrincipalIdResult ACT_ACTConvertNnidToPrincipalId(AuthInfo& authInfo, std::string_view nnid);

	/* NUS */

	struct NAPI_NUSGetSystemCommonETicket_Result : public _NAPI_CommonResultSOAP
	{
		std::vector<uint8> eTicket;
		std::vector<std::vector<uint8>> certs;
	};

	NAPI_NUSGetSystemCommonETicket_Result NUS_GetSystemCommonETicket(AuthInfo& authInfo, uint64 titleId);

	/* IAS/ECS */
	
	struct NAPI_IASGetChallenge_Result : public _NAPI_CommonResultSOAP
	{
		std::string challenge;
	};

	struct NAPI_IASGetRegistrationInfo_Result : public _NAPI_CommonResultSOAP
	{
		std::string accountId;
		std::string deviceToken;
	};

	struct NAPI_ECSGetAccountStatus_Result : public _NAPI_CommonResultSOAP
	{
		enum class AccountStatus
		{
			REGISTERED = 'R',
			TRANSFERRED = 'T',
			UNREGISTERED = 'U',
			UNKNOWN = '\0',
		};

		std::string accountId;
		AccountStatus accountStatus{ AccountStatus::UNKNOWN };

		struct 
		{
			std::string ContentPrefixURL;
			std::string UncachedContentPrefixURL;
			std::string SystemContentPrefixURL;
			std::string SystemUncachedContentPrefixURL;
			std::string EcsURL;
			std::string IasURL;
			std::string CasURL;
			std::string NusURL;
		}serviceURLs;
	};

	struct NAPI_ECSAccountListETicketIds_Result : public _NAPI_CommonResultSOAP
	{
		struct TIV
		{
			sint64 ticketId;
			uint32 ticketVersion;
		};

		std::vector<TIV> tivs;
	};

	struct NAPI_ECSAccountGetETickets_Result : public _NAPI_CommonResultSOAP
	{
		std::vector<uint8> eTickets;
		std::vector<std::vector<uint8>> certs;
	};

	NAPI_IASGetChallenge_Result IAS_GetChallenge(AuthInfo& authInfo);
	NAPI_IASGetRegistrationInfo_Result IAS_GetRegistrationInfo_QueryInfo(AuthInfo& authInfo, std::string challenge);

	NAPI_ECSGetAccountStatus_Result ECS_GetAccountStatus(AuthInfo& authInfo);
	NAPI_ECSAccountListETicketIds_Result ECS_AccountListETicketIds(AuthInfo& authInfo);
	NAPI_ECSAccountGetETickets_Result ECS_AccountGetETickets(AuthInfo& authInfo, sint64 ticketId);

	/* CCS */

	struct NAPI_CCSGetTMD_Result
	{
		bool isValid{ false };
		std::vector<uint8> tmdData;
	};

	struct NAPI_CCSGetETicket_Result
	{
		bool isValid{ false };
		std::vector<uint8> cetkData;
	};

	struct NAPI_CCSGetContentH3_Result
	{
		bool isValid{ false };
		std::vector<uint8> tmdData;
	};

	NAPI_CCSGetTMD_Result CCS_GetTMD(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion);
	NAPI_CCSGetTMD_Result CCS_GetTMD(AuthInfo& authInfo, uint64 titleId);
	NAPI_CCSGetETicket_Result CCS_GetCETK(NetworkService service, uint64 titleId, uint16 titleVersion);
	bool CCS_GetContentFile(NetworkService service, uint64 titleId, uint32 contentId, bool(*cbWriteCallback)(void* userData, const void* ptr, size_t len, bool isLast), void* userData);
	NAPI_CCSGetContentH3_Result CCS_GetContentH3File(NetworkService service, uint64 titleId, uint32 contentId);

	/* IDBE */

	struct IDBEIconDataV0
	{
		struct LanguageInfo
		{
			std::string GetGameNameUTF8();
			std::string GetGameLongNameUTF8();
			std::string GetPublisherNameUTF8();
		private:
			uint16be gameName[0x40]{};
			uint16be gameLongName[0x80]{};
			uint16be publisherName[0x40]{};
		};

		LanguageInfo& GetLanguageStrings(CafeConsoleLanguage index)
		{
			if ((uint32)index >= 16)
				return languageInfo[0];
			return languageInfo[(uint32)index];
		}

	private:
		/* +0x00000 */ uint32be titleIdHigh{};
		/* +0x00004 */ uint32be titleIdLow{};
		/* +0x00008 */ uint32 ukn00008;
		/* +0x0000C */ uint32 ukn0000C;
		/* +0x00010 */ uint32 ukn00010;
		/* +0x00014 */ uint32 ukn00014;
		/* +0x00018 */ uint32 ukn00018;
		/* +0x0001C */ uint32 ukn0001C;
		/* +0x00020 */ uint32 ukn00020;
		/* +0x00024 */ uint32 ukn00024;
		/* +0x00028 */ uint32 ukn00028;
		/* +0x0002C */ uint32 ukn0002C;
		/* +0x00030 */ LanguageInfo languageInfo[16];
		/* +0x02030 */ uint8 tgaData[0x10030]{};
	};

	static_assert(sizeof(IDBEIconDataV0) == 0x12060);

	struct IDBEHeader
	{
		uint8 formatVersion;
		uint8 keyIndex;
		uint8 hashSHA256[32];
	};

	static_assert(sizeof(IDBEHeader) == 2+32);

	std::optional<IDBEIconDataV0> IDBE_Request(NetworkService networkService, uint64 titleId);
	std::vector<uint8> IDBE_RequestRawEncrypted(NetworkService networkService, uint64 titleId); // same as IDBE_Request but doesn't strip the header and decrypt the IDBE

	/* Version list */

	struct NAPI_VersionListVersion_Result
	{
		bool isValid{false};
		uint32 version{0};
		std::string fqdnURL;
	};

	NAPI_VersionListVersion_Result TAG_GetVersionListVersion(AuthInfo& authInfo);

	struct NAPI_VersionList_Result
	{
		bool isValid{ false };
		std::unordered_map<uint64, uint16> titleVersionList; // key = titleId, value = version
	};

	NAPI_VersionList_Result TAG_GetVersionList(AuthInfo& authInfo, std::string_view fqdnURL, uint32 versionListVersion);

}

void NAPI_NUS_GetSystemUpdate();
void NAPI_NUS_GetSystemTitleHash();
