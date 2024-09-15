#include "Common/precompiled.h"
#include "napi.h"

#include "curl/curl.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"

#include "Cemu/ncrypto/ncrypto.h"
#include "napi_helper.h"
#include "util/highresolutiontimer/HighResolutionTimer.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"
#include "config/LaunchSettings.h"
#include "pugixml.hpp"
#include <charconv>

#include"openssl/bn.h"
#include"openssl/x509.h"
#include"openssl/ssl.h"

CURLcode _sslctx_function_NUS(CURL* curl, void* sslctx, void* param)
{
	if (iosuCrypto_addCACertificate(sslctx, 102) == false)
	{
		cemuLog_log(LogType::Force, "Invalid CA certificate (102)");
	}
	if (iosuCrypto_addCACertificate(sslctx, 0x69) == false)
	{
		cemuLog_log(LogType::Force, "Invalid CA certificate (105)");
	}

	if (iosuCrypto_addClientCertificate(sslctx, 3) == false)
	{
		cemuLog_log(LogType::Force, "Certificate error");
	}
	SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
	SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, nullptr);
	return CURLE_OK;
}

CURLcode _sslctx_function_IDBE(CURL* curl, void* sslctx, void* param)
{
	if (iosuCrypto_addCACertificate(sslctx, 105) == false)
	{
		cemuLog_log(LogType::Force, "Invalid CA certificate (105)");
	}
	SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);

	return CURLE_OK;
}

CURLcode _sslctx_function_SOAP(CURL* curl, void* sslctx, void* param)
{
	if (iosuCrypto_addCACertificate(sslctx, 102) == false)
	{
		cemuLog_log(LogType::Force, "Invalid CA certificate (102)");
		cemuLog_log(LogType::Force, "Certificate error");
	}
	if (iosuCrypto_addClientCertificate(sslctx, 1) == false)
	{
		cemuLog_log(LogType::Force, "Certificate error");
	}
	SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
	SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, nullptr);
	return CURLE_OK;
}

CURLcode _sslctx_function_OLIVE(CURL* curl, void* sslctx, void* param)
{
	if (iosuCrypto_addCACertificate(sslctx, 105) == false)
	{
		cemuLog_log(LogType::Force, "Invalid CA certificate (105)");
		cemuLog_log(LogType::Force, "Certificate error");
	}
	if (iosuCrypto_addClientCertificate(sslctx, 7) == false)
	{
		cemuLog_log(LogType::Force, "Olive client certificate error");
	}

	// NSSLAddServerPKIGroups(sslCtx, 3, &x, &y);
	{
		std::vector<sint16> certGroups = {
			100,  101,  102,   103,  104,  105,
			1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009,
			1010, 1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018, 1019,
			1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029,
			1030, 1031, 1032, 1033
		};

		for (auto& certId : certGroups)
			iosuCrypto_addCACertificate(sslctx, certId);
	}

	SSL_CTX_set_mode((SSL_CTX*)sslctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_verify_depth((SSL_CTX*)sslctx, 2);
	SSL_CTX_set_verify((SSL_CTX*)sslctx, SSL_VERIFY_PEER, nullptr);
	return CURLE_OK;
}

CurlRequestHelper::CurlRequestHelper()
{
	m_curl = curl_easy_init();
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, __curlWriteCallback);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);

	curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 2);
	curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

	if(GetConfig().proxy_server.GetValue() != "")
	{
		curl_easy_setopt(m_curl, CURLOPT_PROXY, GetConfig().proxy_server.GetValue().c_str());
	}
}

CurlRequestHelper::~CurlRequestHelper()
{
	curl_easy_cleanup(m_curl);
}

void CurlRequestHelper::initate(NetworkService service, std::string url, SERVER_SSL_CONTEXT sslContext)
{
	// reset parameters
	m_headerExtraFields.clear();
	m_postData.clear();
	m_cbWriteCallback = nullptr;

	curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(m_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
	curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 60);

	// SSL
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
	if (IsNetworkServiceSSLDisabled(service))
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	else if (sslContext == SERVER_SSL_CONTEXT::ACT || sslContext == SERVER_SSL_CONTEXT::TAGAYA)
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_NUS);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
	}
	else if (sslContext == SERVER_SSL_CONTEXT::IDBE)
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_IDBE);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
	}
	else if (sslContext == SERVER_SSL_CONTEXT::IAS || sslContext == SERVER_SSL_CONTEXT::ECS)
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_SOAP);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
	}
	else if (sslContext == SERVER_SSL_CONTEXT::CCS)
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_SOAP);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
	}
	else if (sslContext == SERVER_SSL_CONTEXT::OLIVE)
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_OLIVE);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
	}
	else
	{
		cemu_assert(false);
	}
}

void CurlRequestHelper::addHeaderField(const char* fieldName, std::string_view value)
{
	m_headerExtraFields.emplace_back(fieldName, value);
}

void CurlRequestHelper::addPostField(const char* fieldName, std::string_view value)
{
	if (!m_postData.empty())
		m_postData.emplace_back('&');
	m_postData.insert(m_postData.end(), (uint8*)fieldName, (uint8*)fieldName + strlen(fieldName));
	m_postData.emplace_back('=');
	m_postData.insert(m_postData.end(), (uint8*)value.data(), (uint8*)value.data() + value.size());
}

void CurlRequestHelper::setWriteCallback(bool(*cbWriteCallback)(void* userData, const void* ptr, size_t len, bool isLast), void* userData)
{
	m_cbWriteCallback = cbWriteCallback;
	m_writeCallbackUserData = userData;
}

void CurlRequestHelper::setTimeout(sint32 time)
{
	curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, time);
}

size_t CurlRequestHelper::__curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	size_t writeByteSize = (size_t)(size * nmemb);
	CurlRequestHelper* curlHelper = (CurlRequestHelper*)userdata;
	if (curlHelper->m_cbWriteCallback)
	{
		if (!curlHelper->m_cbWriteCallback(curlHelper->m_writeCallbackUserData, ptr, writeByteSize, false))
			return 0;
		return writeByteSize;
	}
	curlHelper->m_receiveBuffer.insert(curlHelper->m_receiveBuffer.end(), ptr, ptr + writeByteSize);
	return writeByteSize;
}

bool CurlRequestHelper::submitRequest(bool isPost)
{
	// HTTP headers
	struct curl_slist* headers = nullptr;
	for (auto& itr : m_headerExtraFields)
		headers = curl_slist_append(headers, itr.data.c_str());
	curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);

	// post
	if (isPost)
	{
		if (!m_isUsingMultipartFormData) {
			curl_easy_setopt(m_curl, CURLOPT_POST, 1);
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_postData.data());
			curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, m_postData.size());
		}
	}
	else
		curl_easy_setopt(m_curl, CURLOPT_POST, 0);

	// submit
	int res = curl_easy_perform(m_curl);
	if (res != CURLE_OK)
	{
		cemuLog_log(LogType::Force, "CURL web request failed with error {}. Retrying...", res);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		// retry
		res = curl_easy_perform(m_curl);
		if (res != CURLE_OK)
			return false;
	}
	
	// check response code
	long httpCode = 0;
	curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
	if (httpCode != 200)
	{
		cemuLog_log(LogType::Force, "HTTP request received response {} but expected 200", httpCode);
		// error status codes (4xx or 5xx range) are always considered a failed request, except for code 400 which is usually returned as a response to failed logins etc.
		if (httpCode >= 400 && httpCode <= 599 && httpCode != 400)
			return false;
		// for other status codes we assume success if the message is non-empty
		if(m_receiveBuffer.empty())
			return false;
	}

	if (m_cbWriteCallback)
		m_cbWriteCallback(m_writeCallbackUserData, nullptr, 0, true); // flush write

	return true;
}

CurlSOAPHelper::CurlSOAPHelper(NetworkService service)
{
	m_curl = curl_easy_init();
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, __curlWriteCallback);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

	// SSL
	if (!IsNetworkServiceSSLDisabled(service))
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_FUNCTION, _sslctx_function_SOAP);
		curl_easy_setopt(m_curl, CURLOPT_SSL_CTX_DATA, NULL);
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
	}
	else
	{
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	}
	if (GetConfig().proxy_server.GetValue() != "")
	{
		curl_easy_setopt(m_curl, CURLOPT_PROXY, GetConfig().proxy_server.GetValue().c_str());
	}
}

CurlSOAPHelper::~CurlSOAPHelper()
{
	curl_easy_cleanup(m_curl);
}

void CurlSOAPHelper::SOAP_initate(std::string_view serviceType, std::string url, std::string_view requestMethod, std::string_view requestVersion)
{
	curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	m_serviceType = serviceType;
	m_requestMethod = requestMethod;
	m_requestVersion = requestVersion;

	m_envelopeExtraParam.reserve(512);
	m_envelopeExtraParam.clear();
}

void CurlSOAPHelper::SOAP_addRequestField(const char* fieldName, std::string_view value)
{
	m_envelopeExtraParam.append(fmt::format("<{}:{}>{}</{}:{}>", m_serviceType, fieldName, value, m_serviceType, fieldName));
}

void CurlSOAPHelper::SOAP_generateEnvelope()
{
	m_envelopeStr.reserve(4096);
	m_envelopeStr.clear();

	m_envelopeStr.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

	m_envelopeStr.append("<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"\n");
	m_envelopeStr.append(" xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding/\"\n");
	m_envelopeStr.append(" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n");
	m_envelopeStr.append(" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"\n");
	m_envelopeStr.append(fmt::format(" xmlns:{}=\"urn:{}.wsapi.broadon.com\">\n", m_serviceType, m_serviceType));
	m_envelopeStr.append("<SOAP-ENV:Body>\n");
	m_envelopeStr.append(fmt::format("<{}:{} xsi:type=\"{}:{}RequestType\">\n", m_serviceType, m_requestMethod, m_serviceType, m_requestMethod));
	m_envelopeStr.append(fmt::format("<{}:Version>{}</{}:Version>\n", m_serviceType, m_requestVersion, m_serviceType));

	// the server echos the message id
	static uint64 s_msgHigh = 1 + (uint64)HighResolutionTimer::now().getTick()/7;
	uint64 msgId_high = s_msgHigh; // usually this is set to the deviceId
	uint64 msgId_low = (uint64)HighResolutionTimer::now().getTick(); // uptime
	m_envelopeStr.append(fmt::format("<{}:MessageId>EC-{}-{}</{}:MessageId>", m_serviceType, msgId_high, msgId_low, m_serviceType));

	m_envelopeStr.append(m_envelopeExtraParam);

	// some fields are specific to services?
	// ECS doesnt seem to like RegionId and CountryCode

	// following fields are shared:
	// >>> Region, Country, Language
	// following fields are present when NUS:
	// >>> RegionId (instead of Region), CountryCode (instead of Country)
	// following fields are present when CAS or ECS:
	// >>> ApplicationId, TIN, SerialNo
	// following fields are present when CAS:
	// >>> Age
	// following fields are present when ECS:
	// >>> SessionHandle, ServiceTicket, ServiceId
	// following fields for anything that isn't BGS:
	// >>> DeviceId (the serial)

	// DeviceId -> All except BGS (deviceId is the console serial)
	// DeviceToken -> Everything except BGS and NUS

	// ECS:
	//m_envelopeStr.append(fmt::format("<{}:Region>EUR</{}:Region>", serviceType, serviceType));
	//m_envelopeStr.append(fmt::format("<{}:Country>AT</{}:Country>", serviceType, serviceType));


	// device token format:
	// <ECS:DeviceToken>WT-<md5hash_in_hex></ECS:DeviceToken>

	// unknown fields:
	// VirtualDeviceType (shared but optional?)


	// device cert not needed for ECS:GetAccountStatus ? (it complains if present)
	//char deviceCertStr[1024 * 4];
	//iosuCrypto_getDeviceCertificateBase64Encoded(deviceCertStr);
	//m_envelopeStr.append(fmt::format("<{}:DeviceCert>{}</{}:DeviceCert>", serviceType, deviceCertStr, serviceType));

	// only device token needed
	// DeviceToken comes from GetRegistrationInfo and is then stored in ec_account_info.exi

	m_envelopeStr.append(fmt::format("</{}:{}>\n", m_serviceType, m_requestMethod));

	m_envelopeStr.append("</SOAP-ENV:Body>\n");
	m_envelopeStr.append("</SOAP-ENV:Envelope>\n");


}

sint32 iosuCrypto_getDeviceCertificateBase64Encoded(char* output);

bool CurlSOAPHelper::submitRequest()
{
	// generate and set envelope
	SOAP_generateEnvelope();
	curl_easy_setopt(m_curl, CURLOPT_POST, 1);
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, m_envelopeStr.c_str());
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, m_envelopeStr.size());
	// generate and set headers
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
	headers = curl_slist_append(headers, "Accept-Charset: UTF-8");
	headers = curl_slist_append(headers, fmt::format("SOAPAction: urn:{}.wsapi.broadon.com/{}", m_serviceType, m_requestMethod).c_str());
	headers = curl_slist_append(headers, "Accept: */*");
	curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "EVL NUP 040800 Sep 18 2012 20:20:02");

	// send request
	auto res = curl_easy_perform(m_curl);
	return res == CURLE_OK;
}

/* helper functions */

namespace NAPI
{
	bool _findXmlNode(pugi::xml_node& doc, pugi::xml_node& nodeOut, const char* name)
	{
		for (auto& itr : doc.children())
		{
			if (boost::iequals(itr.name(), name))
			{
				nodeOut = itr;
				return true;
			}
			if (_findXmlNode(itr, nodeOut, name))
				return true;
		}
		return false;
	}

	bool _parseResponseInit(const CurlSOAPHelper& soapHelper, const char* responseNodeName, pugi::xml_node& node, _NAPI_CommonResultSOAP& result, pugi::xml_document& doc, pugi::xml_node& responseNode)
	{
		// parse XML response
		if (!doc.load_buffer(soapHelper.getReceivedData().data(), soapHelper.getReceivedData().size()))
		{
			cemuLog_log(LogType::Force, "Failed to parse GetRegistrationInfo() response");
			result.apiError = NAPI_RESULT::XML_ERROR;
			return false;
		}
		if (!_findXmlNode(doc, node, responseNodeName))
		{
			result.apiError = NAPI_RESULT::XML_ERROR;
			return false;
		}
		// parse error code
		auto errorCodeStr = node.child_value("ErrorCode");
		if (!errorCodeStr)
		{
			result.apiError = NAPI_RESULT::XML_ERROR;
			return false;
		}
		int parsedErrorCode = 0;
		std::from_chars_result fcr = std::from_chars(errorCodeStr, errorCodeStr + strlen(errorCodeStr), parsedErrorCode);
		if (fcr.ec == std::errc::invalid_argument || fcr.ec == std::errc::result_out_of_range)
		{
			result.apiError = NAPI_RESULT::XML_ERROR;
			return false;
		}
		if (parsedErrorCode != 0)
		{
			result.serviceError = (EC_ERROR_CODE)parsedErrorCode;
			result.apiError = NAPI_RESULT::SERVICE_ERROR;
			return false;
		}
		result.apiError = NAPI_RESULT::SUCCESS;
		return true;
	}
};
