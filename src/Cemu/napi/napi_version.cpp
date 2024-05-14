#include "Common/precompiled.h"
#include "napi.h"
#include "napi_helper.h"

#include "curl/curl.h"
#include "pugixml.hpp"

#include "Cemu/ncrypto/ncrypto.h"
#include <charconv>
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"

namespace NAPI
{
	NAPI_VersionListVersion_Result TAG_GetVersionListVersion(AuthInfo& authInfo)
	{
		NAPI_VersionListVersion_Result result;
		CurlRequestHelper req;

		std::string requestUrl;
		switch (authInfo.GetService())
		{
		case NetworkService::Pretendo:
			requestUrl = PretendoURLs::TAGAYAURL;
			break;
		case NetworkService::Custom:
			requestUrl = GetNetworkConfig().urls.TAGAYA.GetValue();
			break;
		case NetworkService::Nintendo:
		default:
			requestUrl = NintendoURLs::TAGAYAURL;
			break;
		}
		requestUrl.append(fmt::format(fmt::runtime("/{}/{}/latest_version"), NCrypto::GetRegionAsString(authInfo.region), authInfo.country.empty() ? "NN" : authInfo.country));
		req.initate(authInfo.GetService(), requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::TAGAYA);

		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request version of update list"));
			return result;
		}
		auto& receivedData = req.getReceivedData();

		pugi::xml_document doc;
		if (!doc.load_buffer(receivedData.data(), receivedData.size()))
		{
			cemuLog_log(LogType::Force, "Failed to parse title list XML");
			return result;
		}

		if (!doc.child("version_list_info").child("version") || !doc.child("version_list_info").child("fqdn"))
		{
			cemuLog_log(LogType::Force, "Title list XML has missing field");
			return result;
		}
		result.version = atoi(doc.child("version_list_info").child("version").child_value());
		result.fqdnURL = doc.child("version_list_info").child("fqdn").child_value();
		result.isValid = true;
		return result;
	}

	NAPI_VersionList_Result TAG_GetVersionList(AuthInfo& authInfo, std::string_view fqdnURL, uint32 versionListVersion)
	{
		NAPI_VersionList_Result result;
		CurlRequestHelper req;
		req.initate(authInfo.GetService(), fmt::format("https://{}/tagaya/versionlist/{}/{}/list/{}.versionlist", fqdnURL, NCrypto::GetRegionAsString(authInfo.region), authInfo.country.empty() ? "NN" : authInfo.country, versionListVersion), CurlRequestHelper::SERVER_SSL_CONTEXT::TAGAYA);
		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request update list"));
			return result;
		}
		auto& receivedData = req.getReceivedData();

		pugi::xml_document doc;
		if (!doc.load_buffer(receivedData.data(), receivedData.size()))
		{
			cemuLog_log(LogType::Force, "Failed to parse update list XML");
			return result;
		}
		// example:
		// <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		// <version_list format_version="1.0"><version>1615</version>
		// <titles>
		//    <title><id>0005000E10100600</id><version>16</version></title>
		//    <title><id>0005000E10101B00</id><version>16</version></title>
		// ...

		for (pugi::xml_node title : doc.child("version_list").child("titles").children("title"))
		{
			uint64 titleId = 0;
			uint32 titleVersion = 0;
			std::string_view str = title.child_value("id");
			if (const auto res = std::from_chars(str.data(), str.data() + str.size(), titleId, 16); res.ec != std::errc())
				continue;
			str = title.child_value("version");
			if (const auto res = std::from_chars(str.data(), str.data() + str.size(), titleVersion, 10); res.ec != std::errc())
				continue;
			result.titleVersionList.emplace(titleId, titleVersion);
		}
		result.isValid = true;
		return result;
	}
};
