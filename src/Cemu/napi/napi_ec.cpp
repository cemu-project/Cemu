#include "Common/precompiled.h"
#include "napi.h"
#include "napi_helper.h"

#include "curl/curl.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"

#include "Cemu/ncrypto/ncrypto.h"
#include "util/crypto/md5.h"
#include "config/LaunchSettings.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "pugixml.hpp"
#include <charconv>

namespace NAPI
{
	/* Service URL manager */
	std::string s_serviceURL_ContentPrefixURL;
	std::string s_serviceURL_UncachedContentPrefixURL;
	std::string s_serviceURL_EcsURL;
	std::string s_serviceURL_IasURL;
	std::string s_serviceURL_CasURL;
	std::string s_serviceURL_NusURL;

	std::string _getNUSUrl()
	{
		if (!s_serviceURL_NusURL.empty())
			return s_serviceURL_NusURL;
			switch (ActiveSettings::GetNetworkService())
			{
			case NetworkService::Nintendo:
				return NintendoURLs::NUSURL;
				break;
			case NetworkService::Pretendo:
				return PretendoURLs::NUSURL;
				break;
			case NetworkService::Custom:
				return GetNetworkConfig().urls.NUS;
				break;
			default:
				return NintendoURLs::NUSURL;
				break;
			}
	}

	std::string _getIASUrl()
	{
		if (!s_serviceURL_IasURL.empty())
			return s_serviceURL_IasURL;
		switch (ActiveSettings::GetNetworkService())
			{
			case NetworkService::Nintendo:
				return NintendoURLs::IASURL;
				break;
			case NetworkService::Pretendo:
				return PretendoURLs::IASURL;
				break;
			case NetworkService::Custom:
				return GetNetworkConfig().urls.IAS;
				break;
			default:
				return NintendoURLs::IASURL;
				break;
			}
	}

	std::string _getECSUrl()
	{
		// this is the first url queried (GetAccountStatus). The others are dynamically set if provided by the server but will fallback to hardcoded defaults otherwise
		if (!s_serviceURL_EcsURL.empty())
			return s_serviceURL_EcsURL;
		return LaunchSettings::GetServiceURL_ecs(); // by default this is "https://ecs.wup.shop.nintendo.net/ecs/services/ECommerceSOAP"
	}

	std::string _getCCSUncachedUrl() // used for TMD requests
	{
		if (!s_serviceURL_UncachedContentPrefixURL.empty())
			return s_serviceURL_UncachedContentPrefixURL;
		switch (ActiveSettings::GetNetworkService())
			{
			case NetworkService::Nintendo:
				return NintendoURLs::CCSUURL;
				break;
			case NetworkService::Pretendo:
				return PretendoURLs::CCSUURL;
				break;
			case NetworkService::Custom:
				return GetNetworkConfig().urls.CCSU;
				break;
			default:
				return NintendoURLs::CCSUURL;
				break;
			}
	}

	std::string _getCCSUrl() // used for game data downloads
	{
		if (!s_serviceURL_ContentPrefixURL.empty())
			return s_serviceURL_ContentPrefixURL;
		switch (ActiveSettings::GetNetworkService())
			{
			case NetworkService::Nintendo:
				return NintendoURLs::CCSURL;
				break;
			case NetworkService::Pretendo:
				return PretendoURLs::CCSURL;
				break;
			case NetworkService::Custom:
				return GetNetworkConfig().urls.CCS;
				break;
			default:
				return NintendoURLs::CCSURL;
				break;
			}
	}

	/* NUS */

	// request ticket for titles which have a public eTicket (usually updates and system titles)
	NAPI_NUSGetSystemCommonETicket_Result NUS_GetSystemCommonETicket(AuthInfo& authInfo, uint64 titleId)
	{
		NAPI_NUSGetSystemCommonETicket_Result result{};

		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("nus", _getNUSUrl(), "GetSystemCommonETicket", "1.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform()));
		soapHelper.SOAP_addRequestField("RegionId", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("CountryCode", authInfo.country);
		soapHelper.SOAP_addRequestField("SerialNo", authInfo.serial);

		soapHelper.SOAP_addRequestField("TitleId", fmt::format("{:016X}", titleId));

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		// parse result
		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "GetSystemCommonETicketResponse", responseNode, result, doc, responseNode))
			return result;

		const char* eTicketsStr = responseNode.child_value("CommonETicket");
		result.eTicket = NCrypto::base64Decode(eTicketsStr);
		if (result.eTicket.empty())
		{
			cemuLog_log(LogType::Force, "GetSystemCommonETicketResponse: Invalid eTicket data in response");
			result.apiError = NAPI_RESULT::DATA_ERROR;
			return result;
		}

		for (pugi::xml_node certNode : responseNode.children("Certs"))
		{
			const char* certStringValue = certNode.child_value();
			auto certData = NCrypto::base64Decode(certStringValue);
			if (certData.empty())
			{
				cemuLog_log(LogType::Force, "GetSystemCommonETicketResponse: Invalid cert data in response");
				result.apiError = NAPI_RESULT::DATA_ERROR;
				return result;
			}
			result.certs.emplace_back(NCrypto::base64Decode(certStringValue));
		}

		return result;
	}

	/* IAS */

	NAPI_IASGetChallenge_Result IAS_GetChallenge(AuthInfo& authInfo)
	{
		NAPI_IASGetChallenge_Result result{};
		
		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("ias", _getIASUrl(), "GetChallenge", "2.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform())); // not validated but the generated Challenge is bound to this DeviceId
		soapHelper.SOAP_addRequestField("Region", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("Country", authInfo.country);

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		/* parse result */
		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "GetChallengeResponse", responseNode, result, doc, responseNode))
			return result;
		result.challenge = responseNode.child_value("Challenge");
		return result;
	}

	NAPI_IASGetRegistrationInfo_Result IAS_GetRegistrationInfo_QueryInfo(AuthInfo& authInfo, std::string challenge)
	{
		NAPI_IASGetRegistrationInfo_Result result;
		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("ias", _getIASUrl(), "GetRegistrationInfo", "2.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform())); // this must match the DeviceId used to generate Challenge
		soapHelper.SOAP_addRequestField("Region", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("Country", authInfo.country);

		// string to sign
		// differs depending on the call mode
		std::string signString;
		signString.reserve(1024);
		signString.append(fmt::format("<Challenge>{}</Challenge>", challenge));

		soapHelper.SOAP_addRequestField("Challenge", challenge);

		uint32 signerTitleIdHigh = 0x00050010;
		uint32 signerTitleIdLow = 0x10040300;

		NCrypto::CertECC deviceCert;
		if (!deviceCert.decodeFromBase64(authInfo.deviceCertBase64))
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		NCrypto::CHash256 hash;
		NCrypto::GenerateHashSHA256(signString.data(), signString.size(), hash);

		NCrypto::CertECC certChain;
		NCrypto::ECCSig signature = NCrypto::signHash(signerTitleIdHigh, signerTitleIdLow, hash.b, sizeof(hash.b), certChain);

		auto certChainStr = certChain.encodeToBase64();

		soapHelper.SOAP_addRequestField("Signature", signature.encodeToBase64());
		soapHelper.SOAP_addRequestField("CertChain", certChainStr);
		soapHelper.SOAP_addRequestField("DeviceCert", deviceCert.encodeToBase64());

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		// parse result
		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "GetRegistrationInfoResponse", responseNode, result, doc, responseNode))
			return result;

		result.accountId = responseNode.child_value("AccountId");
		result.deviceToken = responseNode.child_value("DeviceToken");

		if (boost::iequals(responseNode.child_value("DeviceTokenExpired"), "true"))
			forceLog_printf("Unexpected server response: Device token expired");

		/*
			example response:

			<Version>2.0</Version>
			<DeviceId>1234567</DeviceId>
			<MessageId>EC-1234-1234</MessageId>
			<TimeStamp>123456789</TimeStamp>
			<ErrorCode>0</ErrorCode>
			<ServiceStandbyMode>false/true</ServiceStandbyMode>
			<AccountId>123456</AccountId>
			<DeviceToken>DEVICE_TOKEN_STR</DeviceToken>
			<DeviceTokenExpired>false/true</DeviceTokenExpired>
			<Country>COUNTRYCODE</Country>
			<ExtAccountId></ExtAccountId>
			<DeviceStatus>R</DeviceStatus>
			<Currency>EUR</Currency>
		*/


		return result;
	}

	/* ECS */

	std::string _getDeviceTokenWT(std::string_view deviceToken)
	{
		uint8 wtHash[16];
		MD5_CTX md5Ctx;
		MD5_Init(&md5Ctx);
		MD5_Update(&md5Ctx, deviceToken.data(), (unsigned long)deviceToken.size());
		MD5_Final(wtHash, &md5Ctx);

		std::string wtString;
		wtString.reserve(4 + 32);
		wtString.append("WT-");
		for (uint8& b : wtHash)
		{
			wtString.append(fmt::format("{0:02x}", b));
		}
		return wtString;
	}

	NAPI_ECSGetAccountStatus_Result ECS_GetAccountStatus(AuthInfo& authInfo)
	{
		NAPI_ECSGetAccountStatus_Result result{};

		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("ecs", _getECSUrl(), "GetAccountStatus", "2.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform()));
		soapHelper.SOAP_addRequestField("Region", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("Country", authInfo.country);

		if(!authInfo.IASToken.accountId.empty())
			soapHelper.SOAP_addRequestField("AccountId", authInfo.IASToken.accountId);
		if (!authInfo.IASToken.deviceToken.empty())
			soapHelper.SOAP_addRequestField("DeviceToken", _getDeviceTokenWT(authInfo.IASToken.deviceToken));

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "GetAccountStatusResponse", responseNode, result, doc, responseNode))
			return result;

		result.accountId = responseNode.child_value("AccountId");
		const char* accountStatusStr = responseNode.child_value("AccountStatus");
		if (accountStatusStr)
		{
			if (accountStatusStr[0] == 'R')
				result.accountStatus = NAPI_ECSGetAccountStatus_Result::AccountStatus::REGISTERED;
			else if (accountStatusStr[0] == 'T')
				result.accountStatus = NAPI_ECSGetAccountStatus_Result::AccountStatus::TRANSFERRED;
			else if (accountStatusStr[0] == 'U')
				result.accountStatus = NAPI_ECSGetAccountStatus_Result::AccountStatus::UNREGISTERED;
			else
			{
				cemuLog_force("ECS_GetAccountStatus: Account has unknown status code {}", accountStatusStr);
			}
		}
		// extract service URLs
		for (pugi::xml_node serviceURLNode : responseNode.children("ServiceURLs"))
		{
			std::string_view serviceType = serviceURLNode.child_value("Name");
			std::string_view url = serviceURLNode.child_value("URI");
			if(serviceType.empty() || url.empty())
				continue;
			if (boost::iequals(serviceType, "ContentPrefixURL"))
				result.serviceURLs.ContentPrefixURL = url;
			else if (boost::iequals(serviceType, "UncachedContentPrefixURL"))
				result.serviceURLs.UncachedContentPrefixURL = url;
			else if (boost::iequals(serviceType, "SystemContentPrefixURL"))
				result.serviceURLs.SystemContentPrefixURL = url;
			else if (boost::iequals(serviceType, "SystemUncachedContentPrefixURL"))
				result.serviceURLs.SystemUncachedContentPrefixURL = url;
			else if (boost::iequals(serviceType, "EcsURL"))
				result.serviceURLs.EcsURL = url;
			else if (boost::iequals(serviceType, "IasURL"))
				result.serviceURLs.IasURL = url;
			else if (boost::iequals(serviceType, "CasURL"))
				result.serviceURLs.CasURL = url;
			else if (boost::iequals(serviceType, "NusURL"))
				result.serviceURLs.NusURL = url;
			else
				forceLog_printf("GetAccountStatus: Unknown service URI type {}", serviceType);
		}

		// assign service URLs
		if (!result.serviceURLs.ContentPrefixURL.empty())
			s_serviceURL_ContentPrefixURL = result.serviceURLs.ContentPrefixURL;
		if (!result.serviceURLs.UncachedContentPrefixURL.empty())
			s_serviceURL_UncachedContentPrefixURL = result.serviceURLs.UncachedContentPrefixURL;
		if (!result.serviceURLs.IasURL.empty())
			s_serviceURL_IasURL = result.serviceURLs.IasURL;
		if (!result.serviceURLs.CasURL.empty())
			s_serviceURL_CasURL = result.serviceURLs.CasURL;
		if (!result.serviceURLs.NusURL.empty())
			s_serviceURL_NusURL = result.serviceURLs.NusURL;
		if (!result.serviceURLs.EcsURL.empty())
			s_serviceURL_EcsURL = result.serviceURLs.EcsURL;

		return result;
	}

	NAPI_ECSAccountListETicketIds_Result ECS_AccountListETicketIds(AuthInfo& authInfo)
	{
		NAPI_ECSAccountListETicketIds_Result result{};

		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("ecs", _getECSUrl(), "AccountListETicketIds", "2.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform()));
		soapHelper.SOAP_addRequestField("Region", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("Country", authInfo.country);

		if (!authInfo.IASToken.accountId.empty())
			soapHelper.SOAP_addRequestField("AccountId", authInfo.IASToken.accountId);
		if (!authInfo.IASToken.deviceToken.empty())
			soapHelper.SOAP_addRequestField("DeviceToken", _getDeviceTokenWT(authInfo.IASToken.deviceToken));

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "AccountListETicketIdsResponse", responseNode, result, doc, responseNode))
			return result;

		// extract ticket IVs
		for (pugi::xml_node tivNode : responseNode.children("TIV"))
		{
			// TIV is encoded as <ticketId>.<ticketVersion>
			// ticketVersion starts at 0 and increments every time the ticket is updated (e.g. for AOC content, when purchasing additional DLC packages)
			const char* tivValue = tivNode.child_value();
			const char* tivValueEnd = tivValue + strlen(tivValue);
			const char* tivValueSeparator = std::strchr(tivValue, '.');
			if (tivValueSeparator == nullptr)
				tivValueSeparator = tivValueEnd;

			// parse ticket id
			sint64 ticketId = 0;
			std::from_chars_result fcr = std::from_chars(tivValue, tivValueSeparator, ticketId);
			if (fcr.ec == std::errc::invalid_argument || fcr.ec == std::errc::result_out_of_range)
			{
				result.apiError = NAPI_RESULT::XML_ERROR;
				return result;
			}
			// parse ticket version
			uint32 ticketVersion = 0;
			fcr = std::from_chars(tivValueSeparator+1, tivValueEnd, ticketVersion);
			if (fcr.ec == std::errc::invalid_argument || fcr.ec == std::errc::result_out_of_range)
			{
				result.apiError = NAPI_RESULT::XML_ERROR;
				return result;
			}
			result.tivs.push_back({ ticketId , ticketVersion });
		}
		return result;
	}

	NAPI_ECSAccountGetETickets_Result ECS_AccountGetETickets(AuthInfo& authInfo, sint64 ticketId)
	{
		NAPI_ECSAccountGetETickets_Result result{};

		CurlSOAPHelper soapHelper;
		soapHelper.SOAP_initate("ecs", _getECSUrl(), "AccountGetETickets", "2.0");

		soapHelper.SOAP_addRequestField("DeviceId", fmt::format("{}", authInfo.getDeviceIdWithPlatform()));
		soapHelper.SOAP_addRequestField("Region", NCrypto::GetRegionAsString(authInfo.region));
		soapHelper.SOAP_addRequestField("Country", authInfo.country);

		if (!authInfo.IASToken.accountId.empty())
			soapHelper.SOAP_addRequestField("AccountId", authInfo.IASToken.accountId);
		if (!authInfo.IASToken.deviceToken.empty())
			soapHelper.SOAP_addRequestField("DeviceToken", _getDeviceTokenWT(authInfo.IASToken.deviceToken));

		soapHelper.SOAP_addRequestField("DeviceCert", authInfo.deviceCertBase64);

		soapHelper.SOAP_addRequestField("TicketId", fmt::format("{}", ticketId));

		if (!soapHelper.submitRequest())
		{
			result.apiError = NAPI_RESULT::FAILED;
			return result;
		}

		// parse result
		pugi::xml_document doc;
		pugi::xml_node responseNode;
		if (!_parseResponseInit(soapHelper, "AccountGetETicketsResponse", responseNode, result, doc, responseNode))
			return result;

		const char* eTicketsStr = responseNode.child_value("ETickets");
		result.eTickets = NCrypto::base64Decode(eTicketsStr);
		if (result.eTickets.empty())
		{
			cemuLog_log(LogType::Force, "AccountGetETickets: Invalid eTickets data in response");
			result.apiError = NAPI_RESULT::DATA_ERROR;
			return result;
		}

		for (pugi::xml_node certNode : responseNode.children("Certs"))
		{
			const char* certStringValue = certNode.child_value();
			auto certData = NCrypto::base64Decode(certStringValue);
			if (certData.empty())
			{
				cemuLog_log(LogType::Force, "AccountGetETickets: Invalid cert data in response");
				result.apiError = NAPI_RESULT::DATA_ERROR;
				return result;
			}
			result.certs.emplace_back(NCrypto::base64Decode(certStringValue));
		}

		/*
		example response:
			<ETickets>BASE64_TIK?</ETickets>
			<Certs>BASE64_CERT</Certs>
			<Certs>BASE64_CERT</Certs>
		 */

		return result;
	}

	/* CCS (content server for raw files, does not use SOAP API) */

	NAPI_CCSGetTMD_Result CCS_GetTMD(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion)
	{
		NAPI_CCSGetTMD_Result result{};
		CurlRequestHelper req;
		req.initate(fmt::format("{}/{:016x}/tmd.{}?deviceId={}&accountId={}", _getCCSUncachedUrl(), titleId, titleVersion, authInfo.getDeviceIdWithPlatform(), authInfo.IASToken.accountId), CurlRequestHelper::SERVER_SSL_CONTEXT::CCS);
		req.setTimeout(180);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request TMD for title {0:016X} v{1}", titleId, titleVersion));
			return result;
		}
		result.tmdData = req.getReceivedData();
		result.isValid = true;
		return result;
	}

	NAPI_CCSGetTMD_Result CCS_GetTMD(AuthInfo& authInfo, uint64 titleId)
	{
		NAPI_CCSGetTMD_Result result{};
		CurlRequestHelper req;
		req.initate(fmt::format("{}/{:016x}/tmd?deviceId={}&accountId={}", _getCCSUncachedUrl(), titleId, authInfo.getDeviceIdWithPlatform(), authInfo.IASToken.accountId), CurlRequestHelper::SERVER_SSL_CONTEXT::CCS);
		req.setTimeout(180);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request TMD for title {0:016X}", titleId));
			return result;
		}
		result.tmdData = req.getReceivedData();
		result.isValid = true;
		return result;
	}

	NAPI_CCSGetETicket_Result CCS_GetCETK(AuthInfo& authInfo, uint64 titleId, uint16 titleVersion)
	{
		NAPI_CCSGetETicket_Result result{};
		CurlRequestHelper req;
		req.initate(fmt::format("{}/{:016x}/cetk", _getCCSUncachedUrl(), titleId), CurlRequestHelper::SERVER_SSL_CONTEXT::CCS);
		req.setTimeout(180);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request eTicket for title {0:016X} v{1}", titleId, titleVersion));
			return result;
		}
		result.cetkData = req.getReceivedData();
		result.isValid = true;
		return result;
	}

	bool CCS_GetContentFile(uint64 titleId, uint32 contentId, bool(*cbWriteCallback)(void* userData, const void* ptr, size_t len, bool isLast), void* userData)
	{
		CurlRequestHelper req;
		req.initate(fmt::format("{}/{:016x}/{:08x}", _getCCSUrl(), titleId, contentId), CurlRequestHelper::SERVER_SSL_CONTEXT::CCS);
		req.setWriteCallback(cbWriteCallback, userData);
		req.setTimeout(0);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request content file {:08x} for title {:016X}", contentId, titleId));
			return false;
		}
		return true;
	}

	NAPI_CCSGetContentH3_Result CCS_GetContentH3File(uint64 titleId, uint32 contentId)
	{
		NAPI_CCSGetContentH3_Result result{};
		CurlRequestHelper req;
		req.initate(fmt::format("{}/{:016x}/{:08x}.h3", _getCCSUrl(), titleId, contentId), CurlRequestHelper::SERVER_SSL_CONTEXT::CCS);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request content hash file {:08x}.h3 for title {:016X}", contentId, titleId));
			return result;
		}
		result.tmdData = req.getReceivedData();
		result.isValid = true;
		return result;
	}

};
