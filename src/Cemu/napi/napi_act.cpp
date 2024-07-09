#include "Common/precompiled.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "napi.h"
#include "napi_helper.h"

#include "curl/curl.h"
#include "pugixml.hpp"
#include "Cafe/IOSU/legacy/iosu_crypto.h"

#include "config/ActiveSettings.h"
#include "util/helpers/StringHelpers.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "config/LaunchSettings.h"

namespace NAPI
{
	std::string _getACTUrl(NetworkService service)
	{
		switch (service)
		{
		case NetworkService::Nintendo:
			return NintendoURLs::ACTURL;
		case NetworkService::Pretendo:
			return PretendoURLs::ACTURL;
		case NetworkService::Custom:
			return GetNetworkConfig().urls.ACT.GetValue();
		default:
			return NintendoURLs::ACTURL;
		}
	}

	struct ACTOauthToken : public _NAPI_CommonResultACT
	{
		std::string token;
		std::string refreshToken;
	};

	bool _parseActResponse(CurlRequestHelper& requestHelper, _NAPI_CommonResultACT& result, pugi::xml_document& doc)
	{
		if (!doc.load_buffer(requestHelper.getReceivedData().data(), requestHelper.getReceivedData().size()))
		{
			cemuLog_log(LogType::Force, fmt::format("Invalid XML in account service response"));
			result.apiError = NAPI_RESULT::XML_ERROR;
			return false;
		}
		// check for error codes
		pugi::xml_node errors = doc.child("errors");
		if (errors)
		{
			pugi::xml_node error = errors.child("error");
			if (error)
			{
				std::string_view errorCodeStr = error.child_value("code");
				std::string_view errorCodeMsg = error.child_value("message");
				sint32 errorCode = StringHelpers::ToInt(errorCodeStr);
				if (errorCode == 0)
				{
					cemuLog_log(LogType::Force, "Account response with unexpected error code 0");
					result.apiError = NAPI_RESULT::XML_ERROR;
				}
				else
				{
					result.apiError = NAPI_RESULT::SERVICE_ERROR;
					result.serviceError = (ACT_ERROR_CODE)errorCode;
					cemuLog_log(LogType::Force, "Account response with error code {}", errorCode);
					if(!errorCodeMsg.empty())
						cemuLog_log(LogType::Force, "Message from server: {}", errorCodeMsg);
				}
			}
			else
			{
				result.apiError = NAPI_RESULT::XML_ERROR;
			}
			return false;
		}
		return true;
	}

	void _ACTSetCommonHeaderParameters(CurlRequestHelper& req, AuthInfo& authInfo)
	{
		req.addHeaderField("X-Nintendo-Platform-ID", "1");
		req.addHeaderField("X-Nintendo-Device-Type", "2");

		req.addHeaderField("X-Nintendo-Client-ID", "a2efa818a34fa16b8afbc8a74eba3eda");
		req.addHeaderField("X-Nintendo-Client-Secret", "c91cdb5658bd4954ade78533a339cf9a");

		req.addHeaderField("Accept", "*/*");

		if(authInfo.region == CafeConsoleRegion::USA)
			req.addHeaderField("X-Nintendo-System-Version", "0270");
		else
			req.addHeaderField("X-Nintendo-System-Version", "0260");
	}

	void _ACTSetDeviceParameters(CurlRequestHelper& req, AuthInfo& authInfo)
	{
		req.addHeaderField("X-Nintendo-Device-ID", fmt::format("{}", authInfo.deviceId)); // deviceId without platform field
		req.addHeaderField("X-Nintendo-Serial-Number", authInfo.serial);
	}

	void _ACTSetRegionAndCountryParameters(CurlRequestHelper& req, AuthInfo& authInfo)
	{
		req.addHeaderField("X-Nintendo-Region", fmt::format("{}", (uint32)authInfo.region));
		req.addHeaderField("X-Nintendo-Country", authInfo.country);
	}

	struct OAuthTokenCacheEntry 
	{
		OAuthTokenCacheEntry(std::string_view accountId, std::array<uint8, 32>& passwordHash, std::string_view token, std::string_view refreshToken, uint64 expiresIn, NetworkService service) : accountId(accountId), passwordHash(passwordHash), token(token), refreshToken(refreshToken), service(service)
		{
			expires = HighResolutionTimer::now().getTickInSeconds() + expiresIn;
		};

		bool CheckIfSameAccount(const AuthInfo& authInfo) const
		{
			return authInfo.accountId == accountId && authInfo.passwordHash == passwordHash;
		}

		bool CheckIfExpired() const
		{
			return HighResolutionTimer::now().getTickInSeconds() >= expires;
		}
		std::string accountId;
		std::array<uint8, 32> passwordHash;
		std::string token;
		std::string refreshToken;
		uint64 expires;
		NetworkService service;
	};

	std::vector<OAuthTokenCacheEntry> g_oauthTokenCache;
	std::mutex g_oauthTokenCacheMtx;

	// look up oauth token in cache, otherwise request from server
	ACTOauthToken ACT_GetOauthToken_WithCache(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion)
	{
		ACTOauthToken result{};
		
		// check cache first
		NetworkService service = authInfo.GetService();
		g_oauthTokenCacheMtx.lock();
		auto cacheItr = g_oauthTokenCache.begin();
		while (cacheItr != g_oauthTokenCache.end())
		{
			if (cacheItr->CheckIfSameAccount(authInfo) && cacheItr->service == service)
			{
				if (cacheItr->CheckIfExpired())
				{
					cacheItr = g_oauthTokenCache.erase(cacheItr);
					continue;
				}
				result.token = cacheItr->token;
				result.refreshToken = cacheItr->refreshToken;
				result.apiError = NAPI_RESULT::SUCCESS;
				g_oauthTokenCacheMtx.unlock();
				return result;
			}
			cacheItr++;
		}
		g_oauthTokenCacheMtx.unlock();
		// token not cached, request from server via oauth2
		CurlRequestHelper req;

		req.initate(authInfo.GetService(), fmt::format("{}/v1/api/oauth20/access_token/generate", _getACTUrl(authInfo.GetService())), CurlRequestHelper::SERVER_SSL_CONTEXT::ACT);
		_ACTSetCommonHeaderParameters(req, authInfo);
		_ACTSetDeviceParameters(req, authInfo);
		_ACTSetRegionAndCountryParameters(req, authInfo);
		req.addHeaderField("X-Nintendo-Device-Cert", authInfo.deviceCertBase64);

		req.addHeaderField("X-Nintendo-FPD-Version", "0000");
		req.addHeaderField("X-Nintendo-Environment", "L1");
		req.addHeaderField("X-Nintendo-Title-ID", fmt::format("{:016x}", titleId));
		uint32 uniqueId = ((titleId >> 8) & 0xFFFFF);
		req.addHeaderField("X-Nintendo-Unique-ID", fmt::format("{:05x}", uniqueId));
		req.addHeaderField("X-Nintendo-Application-Version", fmt::format("{:04x}", titleVersion));

		// convert password hash to string
		char passwordHashString[128];
		for (sint32 i = 0; i < 32; i++)
			sprintf(passwordHashString + i * 2, "%02x", authInfo.passwordHash[i]);
		req.addPostField("grant_type", "password");
		req.addPostField("user_id", authInfo.accountId);
		req.addPostField("password", passwordHashString);
		req.addPostField("password_type", "hash");

		req.addHeaderField("Content-type", "application/x-www-form-urlencoded");

		if (!req.submitRequest(true))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed request /oauth20/access_token/generate"));
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		/*
		 Response example:
		 <OAuth20>
		 <access_token>
		 <token>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</token>
		 <refresh_token>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</refresh_token>
		 <expires_in>3600</expires_in>
		 </access_token>
		 </OAuth20>
		*/

		// parse result
		pugi::xml_document doc;
		if (!_parseActResponse(req, result, doc))
			return result;
		pugi::xml_node node = doc.child("OAuth20");
		if (!node)
		{
			cemuLog_log(LogType::Force, fmt::format("Response does not contain OAuth20 node"));
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}
		node = node.child("access_token");
		if (!node)
		{
			cemuLog_log(LogType::Force, fmt::format("Response does not contain OAuth20/access_token node"));
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}

		result.token = node.child_value("token");
		result.refreshToken = node.child_value("refresh_token");
		std::string_view expires_in = node.child_value("expires_in");
		result.apiError = NAPI_RESULT::SUCCESS;

		if (result.token.empty())
			cemuLog_log(LogType::Force, "OAuth20/token is empty");
		sint64 expiration = StringHelpers::ToInt64(expires_in);
		expiration = std::max(expiration - 30LL, 0LL); // subtract a few seconds to compensate for the web request delay

		// update cache
		if (expiration > 0)
		{
			g_oauthTokenCacheMtx.lock();
			g_oauthTokenCache.emplace_back(authInfo.accountId, authInfo.passwordHash, result.token, result.refreshToken, expiration, service);
			g_oauthTokenCacheMtx.unlock();
		}
		return result;
	}

	bool ACT_GetProfile(AuthInfo& authInfo, ACTGetProfileResult& result)
	{
		CurlRequestHelper req;

		req.initate(authInfo.GetService(), fmt::format("{}/v1/api/people/@me/profile", _getACTUrl(authInfo.GetService())), CurlRequestHelper::SERVER_SSL_CONTEXT::ACT);

		_ACTSetCommonHeaderParameters(req, authInfo);
		_ACTSetDeviceParameters(req, authInfo);

		// get oauth2 token
		ACTOauthToken oauthToken = ACT_GetOauthToken_WithCache(authInfo, 0x0005001010001C00, 0x0001C);

		cemu_assert_unimplemented();
		return true;
	}

	struct NexTokenCacheEntry
	{
		NexTokenCacheEntry(std::string_view accountId, std::array<uint8, 32>& passwordHash, NetworkService networkService, uint32 gameServerId, ACTNexToken& nexToken) : accountId(accountId), passwordHash(passwordHash), networkService(networkService), nexToken(nexToken), gameServerId(gameServerId) {};

		bool IsMatch(const AuthInfo& authInfo, const uint32 gameServerId) const
		{
			return authInfo.accountId == accountId && authInfo.passwordHash == passwordHash && authInfo.GetService() == networkService && this->gameServerId == gameServerId;
		}

		std::string accountId;
		std::array<uint8, 32> passwordHash;
		NetworkService networkService;
		uint32 gameServerId;

		ACTNexToken nexToken;
	};

	std::vector<NexTokenCacheEntry> g_nexTokenCache;
	std::mutex g_nexTokenCacheMtx;

	ACTGetNexTokenResult ACT_GetNexToken_WithCache(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion, uint32 serverId)
	{
		ACTGetNexTokenResult result{};
		// check cache
		g_nexTokenCacheMtx.lock();
		for (auto& itr : g_nexTokenCache)
		{
			if (itr.IsMatch(authInfo, serverId))
			{
				result.nexToken = itr.nexToken;
				result.apiError = NAPI_RESULT::SUCCESS;
				g_nexTokenCacheMtx.unlock();
				return result;

			}
		}
		g_nexTokenCacheMtx.unlock();
		// get Nex token
		ACTOauthToken oauthToken = ACT_GetOauthToken_WithCache(authInfo, titleId, titleVersion);
		if (!oauthToken.isValid())
		{
			cemuLog_log(LogType::Force, "ACT_GetNexToken(): Failed to retrieve OAuth token");
			if (oauthToken.apiError == NAPI_RESULT::SERVICE_ERROR)
			{
				result.apiError = NAPI_RESULT::SERVICE_ERROR;
				result.serviceError = oauthToken.serviceError;
			}
			else
			{
				result.apiError = NAPI_RESULT::DATA_ERROR;
			}
			return result;
		}
		// do request
		CurlRequestHelper req;
		req.initate(authInfo.GetService(), fmt::format("{}/v1/api/provider/nex_token/@me?game_server_id={:08X}", _getACTUrl(authInfo.GetService()), serverId), CurlRequestHelper::SERVER_SSL_CONTEXT::ACT);
		_ACTSetCommonHeaderParameters(req, authInfo);
		_ACTSetDeviceParameters(req, authInfo);
		_ACTSetRegionAndCountryParameters(req, authInfo);
		req.addHeaderField("X-Nintendo-FPD-Version", "0000");
		req.addHeaderField("X-Nintendo-Environment", "L1");
		req.addHeaderField("X-Nintendo-Title-ID", fmt::format("{:016x}", titleId));
		uint32 uniqueId = ((titleId >> 8) & 0xFFFFF);
		req.addHeaderField("X-Nintendo-Unique-ID", fmt::format("{:05x}", uniqueId));
		req.addHeaderField("X-Nintendo-Application-Version", fmt::format("{:04x}", titleVersion));

		req.addHeaderField("Authorization", fmt::format("Bearer {}", oauthToken.token));

		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed request /provider/nex_token/@me"));
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		/*
		Example response (success):
		<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		<nex_token>
		<host>HOST</host>
		<nex_password>xxxxxxxxxxxxxxxx</nex_password>
		<pid>123456</pid>
		<port>60200</port>
		<token>xxxxxxxxxxxxxxx</token>
		</nex_token>


		Example response (error case):
		<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		<errors>
		<error>
		<code>1021</code>
		<message>The requested game server was not found.</message>
		</error>
		</errors>
		*/

		// code 0124 -> Version lower than useable registered

		// parse result
		pugi::xml_document doc;
		if (!_parseActResponse(req, result, doc))
			return result;
		pugi::xml_node tokenNode = doc.child("nex_token");
		if (!tokenNode)
		{
			cemuLog_log(LogType::Force, "Response does not contain NexToken node");
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}

		std::string_view host = tokenNode.child_value("host");
		std::string_view nex_password = tokenNode.child_value("nex_password");
		std::string_view port = tokenNode.child_value("port");
		std::string_view token = tokenNode.child_value("token");

		memset(&result.nexToken, 0, sizeof(ACTNexToken));
		if (host.size() > 15)
			cemuLog_log(LogType::Force, "NexToken response: host field too long");
		if (nex_password.size() > 64)
			cemuLog_log(LogType::Force, "NexToken response: nex_password field too long");
		if (token.size() > 512)
			cemuLog_log(LogType::Force, "NexToken response: token field too long");
		for (size_t i = 0; i < std::min(host.size(), (size_t)15); i++)
			result.nexToken.host[i] = host[i];
		for (size_t i = 0; i < std::min(nex_password.size(), (size_t)64); i++)
			result.nexToken.nexPassword[i] = nex_password[i];
		for (size_t i = 0; i < std::min(token.size(), (size_t)512); i++)
			result.nexToken.token[i] = token[i];
		result.nexToken.port = (uint16)StringHelpers::ToInt(port);
		result.apiError = NAPI_RESULT::SUCCESS;
		g_nexTokenCacheMtx.lock();
		g_nexTokenCache.emplace_back(authInfo.accountId, authInfo.passwordHash, authInfo.GetService(), serverId, result.nexToken);
		g_nexTokenCacheMtx.unlock();
		return result;
	}

	struct IndependentTokenCacheEntry
	{
		IndependentTokenCacheEntry(std::string_view accountId, std::array<uint8, 32>& passwordHash, NetworkService networkService, std::string_view clientId, std::string_view independentToken, sint64 expiresIn) : accountId(accountId), passwordHash(passwordHash), networkService(networkService), clientId(clientId), independentToken(independentToken)
		{
			expires = HighResolutionTimer::now().getTickInSeconds() + expiresIn;
		};

		bool IsMatch(const AuthInfo& authInfo, const std::string_view clientId) const
		{
			return authInfo.accountId == accountId && authInfo.passwordHash == passwordHash && authInfo.GetService() == networkService && this->clientId == clientId;
		}

		bool CheckIfExpired() const
		{
			return (sint64)HighResolutionTimer::now().getTickInSeconds() >= expires;
		}

		std::string accountId;
		std::array<uint8, 32> passwordHash;
		NetworkService networkService;
		std::string clientId;
		sint64 expires;

		std::string independentToken;
	};

	std::vector<IndependentTokenCacheEntry> g_IndependentTokenCache;
	std::mutex g_IndependentTokenCacheMtx;

	ACTGetIndependentTokenResult ACT_GetIndependentToken_WithCache(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion, std::string_view clientId)
	{
		ACTGetIndependentTokenResult result{};
		// check cache
		g_IndependentTokenCacheMtx.lock();
		auto itr = g_IndependentTokenCache.begin();
		while(itr != g_IndependentTokenCache.end())
		{
			if (itr->CheckIfExpired())
			{
				itr = g_IndependentTokenCache.erase(itr);
				continue;
			}
			else if (itr->IsMatch(authInfo, clientId))
			{
				result.token = itr->independentToken;
				result.expiresIn = std::max(itr->expires - (sint64)HighResolutionTimer::now().getTickInSeconds(), (sint64)0);
				result.apiError = NAPI_RESULT::SUCCESS;
				g_IndependentTokenCacheMtx.unlock();
				return result;
			}
			itr++;
		}
		g_IndependentTokenCacheMtx.unlock();
		// get Independent token
		ACTOauthToken oauthToken = ACT_GetOauthToken_WithCache(authInfo, titleId, titleVersion);
		if (!oauthToken.isValid())
		{
			cemuLog_log(LogType::Force, "ACT_GetIndependentToken(): Failed to retrieve OAuth token");
			if (oauthToken.apiError == NAPI_RESULT::SERVICE_ERROR)
			{
				result.apiError = NAPI_RESULT::SERVICE_ERROR;
				result.serviceError = oauthToken.serviceError;
			}
			else
			{
				result.apiError = NAPI_RESULT::DATA_ERROR;
			}
			return result;
		}
		// do request
		CurlRequestHelper req;
		req.initate(authInfo.GetService(), fmt::format("{}/v1/api/provider/service_token/@me?client_id={}", _getACTUrl(authInfo.GetService()), clientId), CurlRequestHelper::SERVER_SSL_CONTEXT::ACT);
		_ACTSetCommonHeaderParameters(req, authInfo);
		_ACTSetDeviceParameters(req, authInfo);
		_ACTSetRegionAndCountryParameters(req, authInfo);
		req.addHeaderField("X-Nintendo-FPD-Version", "0000");
		req.addHeaderField("X-Nintendo-Environment", "L1");
		req.addHeaderField("X-Nintendo-Title-ID", fmt::format("{:016x}", titleId));
		uint32 uniqueId = ((titleId >> 8) & 0xFFFFF);
		req.addHeaderField("X-Nintendo-Unique-ID", fmt::format("{:05x}", uniqueId));
		req.addHeaderField("X-Nintendo-Application-Version", fmt::format("{:04x}", titleVersion));

		req.addHeaderField("Authorization", fmt::format("Bearer {}", oauthToken.token));

		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed request /provider/service_token/@me"));
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		/*
		Example response (success):
		<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		<service_token>
		<token>xxxxxxxxxxxx</token>
		</service_token>
		*/

		// parse result
		pugi::xml_document doc;
		if (!_parseActResponse(req, result, doc))
			return result;
		pugi::xml_node tokenNode = doc.child("service_token");
		if (!tokenNode)
		{
			cemuLog_log(LogType::Force, "Response does not contain service_token node");
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}

		std::string_view token = tokenNode.child_value("token");
		result.token = token;
		result.apiError = NAPI_RESULT::SUCCESS;

		g_IndependentTokenCacheMtx.lock();
		g_IndependentTokenCache.emplace_back(authInfo.accountId, authInfo.passwordHash, authInfo.GetService(), clientId, result.token, 3600);
		g_IndependentTokenCacheMtx.unlock();
		return result;
	}

	ACTConvertNnidToPrincipalIdResult ACT_ACTConvertNnidToPrincipalId(AuthInfo& authInfo, std::string_view nnid)
	{
		ACTConvertNnidToPrincipalIdResult result{};
		// get Independent token
		ACTOauthToken oauthToken = ACT_GetOauthToken_WithCache(authInfo, 0x0005001010001C00, 0);
		if (!oauthToken.isValid())
		{
			cemuLog_log(LogType::Force, "ACT_ACTConvertNnidToPrincipalId(): Failed to retrieve OAuth token");
			if (oauthToken.apiError == NAPI_RESULT::SERVICE_ERROR)
			{
				result.apiError = NAPI_RESULT::SERVICE_ERROR;
				result.serviceError = oauthToken.serviceError;
			}
			else
			{
				result.apiError = NAPI_RESULT::DATA_ERROR;
			}
			return result;
		}
		// do request
		CurlRequestHelper req;
		req.initate(authInfo.GetService(), fmt::format("{}/v1/api/admin/mapped_ids?input_type=user_id&output_type=pid&input={}", _getACTUrl(authInfo.GetService()), nnid), CurlRequestHelper::SERVER_SSL_CONTEXT::ACT);
		_ACTSetCommonHeaderParameters(req, authInfo);
		_ACTSetDeviceParameters(req, authInfo);
		_ACTSetRegionAndCountryParameters(req, authInfo);
		req.addHeaderField("X-Nintendo-FPD-Version", "0000");
		req.addHeaderField("X-Nintendo-Environment", "L1");
		req.addHeaderField("X-Nintendo-Title-ID", fmt::format("{:016x}", 0x0005001010001C00));
		uint32 uniqueId = 0x50010;
		req.addHeaderField("X-Nintendo-Unique-ID", fmt::format("{:05x}", uniqueId));
		req.addHeaderField("X-Nintendo-Application-Version", fmt::format("{:04x}", 0));

		req.addHeaderField("Authorization", fmt::format("Bearer {}", oauthToken.token));

		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed request /admin/mapped_ids"));
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		/*
		Example response (success):
		<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		<mapped_ids>
		<mapped_id>
		<in_id>input-nnid</in_id>
		<out_id>12345</out_id>
		</mapped_id>
		</mapped_ids>
		*/

		// parse result
		pugi::xml_document doc;
		if (!_parseActResponse(req, result, doc))
			return result;
		pugi::xml_node tokenNode = doc.child("mapped_ids");
		if (!tokenNode)
		{
			cemuLog_log(LogType::Force, "Response does not contain mapped_ids node");
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}
		tokenNode = tokenNode.child("mapped_id");
		if (!tokenNode)
		{
			cemuLog_log(LogType::Force, "Response does not contain mapped_id node");
			result.apiError = NAPI_RESULT::XML_ERROR;
			return result;
		}
		std::string_view pidString = tokenNode.child_value("out_id");
		if (!pidString.empty())
		{
			result.isFound = true;
			result.principalId = StringHelpers::ToInt(pidString);
		}
		else
		{
			result.isFound = false;
			result.principalId = 0;
		}
		result.apiError = NAPI_RESULT::SUCCESS;
		return result;
	}

	bool NAPI_MakeAuthInfoFromCurrentAccount(AuthInfo& authInfo)
	{
		authInfo = {};
		if (!NCrypto::SEEPROM_IsPresent())
			return false;
		const Account& account = Account::GetCurrentAccount();
		authInfo.accountId = account.GetAccountId();
		auto passwordHash = account.GetAccountPasswordCache();
		authInfo.passwordHash = passwordHash;
		if (std::all_of(passwordHash.cbegin(), passwordHash.cend(), [](uint8 v) { return v == 0; }))
		{
			static bool s_showedLoginError = false;
			if (!s_showedLoginError)
			{
				cemuLog_log(LogType::Force, "Account login is impossible because the cached password hash is not set");
				s_showedLoginError = true;
			}
			return false; // password hash not set
		}
		authInfo.deviceId = NCrypto::GetDeviceId();		
		authInfo.serial = NCrypto::GetSerial();
		authInfo.region = NCrypto::SEEPROM_GetRegion();
		authInfo.country = NCrypto::GetCountryAsString(account.GetCountry());
		authInfo.deviceCertBase64 = NCrypto::CertECC::GetDeviceCertificate().encodeToBase64();

		return true;
	}
}
