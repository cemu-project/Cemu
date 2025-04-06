#pragma once
#include "napi.h"
#include "curl/curl.h"
#include "pugixml.hpp"

typedef void CURL;

class CurlRequestHelper
{
	struct HeaderExtraField
	{
		HeaderExtraField(std::string_view name, std::string_view value)
		{
			data.assign(name);
			data.append(": ");
			data.append(value);
		};

		std::string data;
	};
public:
	enum class SERVER_SSL_CONTEXT
	{
		ACT, // account.nintendo.net
		ECS, // ecs.
		IAS, // ias.
		CCS, // ccs.
		IDBE, // idbe-wup.
		TAGAYA, // tagaya.wup.shop.nintendo.net
		OLIVE, // olv.
	};

	CurlRequestHelper();
	~CurlRequestHelper();

	CURL* getCURL()
	{
		return m_curl;
	}

	void initate(NetworkService service, std::string url, SERVER_SSL_CONTEXT sslContext);
	void addHeaderField(const char* fieldName, std::string_view value);
	void addPostField(const char* fieldName, std::string_view value);
	void setWriteCallback(bool(*cbWriteCallback)(void* userData, const void* ptr, size_t len, bool isLast), void* userData);
	void setTimeout(sint32 timeoutSeconds); // maximum duration of the request after connecting. Set to zero to disable limit

	bool submitRequest(bool isPost = false);

	std::vector<uint8>& getReceivedData()
	{
		return m_receiveBuffer;
	}

	void setUseMultipartFormData(bool isUsingMultipartFormData)
	{
		m_isUsingMultipartFormData = isUsingMultipartFormData;
	}

private:
	static size_t __curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

	CURL* m_curl;
	std::vector<uint8> m_receiveBuffer;
	// input parameters
	std::vector<HeaderExtraField> m_headerExtraFields;
	std::vector<uint8> m_postData;
	// write callback redirect
	bool (*m_cbWriteCallback)(void* userData, const void* ptr, size_t len, bool isLast);
	void* m_writeCallbackUserData{};

	bool m_isUsingMultipartFormData = false;
};

class CurlSOAPHelper // todo - make this use CurlRequestHelper
{
public:
	CurlSOAPHelper(NetworkService service);
	~CurlSOAPHelper();

	CURL* getCURL()
	{
		return m_curl;
	}

	void SOAP_initate(std::string_view serviceType, std::string url, std::string_view requestMethod, std::string_view requestVersion);

	void SOAP_addRequestField(const char* fieldName, std::string_view value);

	bool submitRequest();

	const std::vector<uint8>& getReceivedData() const
	{
		return m_receiveBuffer;
	}

private:
	void SOAP_generateEnvelope();

	static size_t __curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
	{
		CurlSOAPHelper* curlHelper = (CurlSOAPHelper*)userdata;
		sint32 writeByteSize = (sint32)(size * nmemb);
		curlHelper->m_receiveBuffer.insert(curlHelper->m_receiveBuffer.end(), ptr, ptr + writeByteSize);
		return writeByteSize;
	}

	CURL* m_curl;
	std::vector<uint8> m_receiveBuffer;
	// input parameters
	std::string m_serviceType;
	std::string m_requestMethod;
	std::string m_requestVersion;
	// generated / internal
	std::string m_envelopeStr;
	std::string m_envelopeExtraParam;
};

namespace NAPI
{
	bool _findXmlNode(pugi::xml_node& doc, pugi::xml_node& nodeOut, const char* name);
	bool _parseResponseInit(const CurlSOAPHelper& soapHelper, const char* responseNodeName, pugi::xml_node& node, _NAPI_CommonResultSOAP& result, pugi::xml_document& doc, pugi::xml_node& responseNode);

};