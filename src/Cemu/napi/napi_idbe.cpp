#include "Common/precompiled.h"
#include "napi.h"
#include "napi_helper.h"

#include "curl/curl.h"
#include "pugixml.hpp"
#include "Cafe/IOSU/legacy/iosu_crypto.h"

#include "Cemu/ncrypto/ncrypto.h"
#include "openssl/sha.h"
#include "util/crypto/aes128.h"
#include "util/helpers/StringHelpers.h"
#include "config/ActiveSettings.h"
#include "config/NetworkSettings.h"

namespace NAPI
{

	std::string IDBEIconDataV0::LanguageInfo::GetGameNameUTF8()
	{
		return StringHelpers::ToUtf8(gameName);
	}

	std::string IDBEIconDataV0::LanguageInfo::GetGameLongNameUTF8()
	{
		return StringHelpers::ToUtf8(gameLongName);
	}

	std::string IDBEIconDataV0::LanguageInfo::GetPublisherNameUTF8()
	{
		return StringHelpers::ToUtf8(publisherName);
	}

	void _decryptIDBEAndHash(IDBEIconDataV0* iconData, uint8* hash, uint8 keyIndex)
	{
		static uint8 idbeAesKeys[4 * 16] =
		{
			0x4A,0xB9,0xA4,0x0E,0x14,0x69,0x75,0xA8,0x4B,0xB1,0xB4,0xF3,0xEC,0xEF,0xC4,0x7B,
			0x90,0xA0,0xBB,0x1E,0x0E,0x86,0x4A,0xE8,0x7D,0x13,0xA6,0xA0,0x3D,0x28,0xC9,0xB8,
			0xFF,0xBB,0x57,0xC1,0x4E,0x98,0xEC,0x69,0x75,0xB3,0x84,0xFC,0xF4,0x07,0x86,0xB5,
			0x80,0x92,0x37,0x99,0xB4,0x1F,0x36,0xA6,0xA7,0x5F,0xB8,0xB4,0x8C,0x95,0xF6,0x6F
		};

		static uint8 idbeAesIv[16] =
		{
			0xA4,0x69,0x87,0xAE,0x47,0xD8,0x2B,0xB4,0xFA,0x8A,0xBC,0x04,0x50,0x28,0x5F,0xA4
		};

		const uint8* aesKey = idbeAesKeys + 16 * keyIndex;

		uint8 iv[16];
		memcpy(iv, hash + 16, sizeof(iv));
		AES128_CBC_decrypt(hash, hash, 32, aesKey, idbeAesIv);
		AES128_CBC_decrypt((uint8*)iconData, (uint8*)iconData, sizeof(IDBEIconDataV0), aesKey, iv);
	}

	std::vector<uint8> IDBE_RequestRawEncrypted(NetworkService networkService, uint64 titleId)
	{
		CurlRequestHelper req;
		std::string requestUrl;
		switch (networkService)
		{
		case NetworkService::Pretendo:
			requestUrl = PretendoURLs::IDBEURL;
			break;
		case NetworkService::Custom:
			requestUrl = GetNetworkConfig().urls.IDBE.GetValue();
			break;
		case NetworkService::Nintendo:
		default:
			requestUrl = NintendoURLs::IDBEURL;
			break;
		}
		requestUrl.append(fmt::format(fmt::runtime("/{0:02X}/{1:016X}.idbe"), (uint32)((titleId >> 8) & 0xFF), titleId));
		req.initate(networkService, requestUrl, CurlRequestHelper::SERVER_SSL_CONTEXT::IDBE);

		if (!req.submitRequest(false))
		{
			cemuLog_log(LogType::Force, fmt::format("Failed to request IDBE icon for title {0:016X}", titleId));
			return {};
		}
		/*
			format:
			+0x00	uint8		version (0)
			+0x01	uint8		keyIndex
			+0x02	uint8[32]	hashSHA256
			+0x22	uint8[]		EncryptedIconData
		*/
		auto& receivedData = req.getReceivedData();
		return receivedData;
	}

	std::optional<IDBEIconDataV0> IDBE_Request(NetworkService networkService, uint64 titleId)
	{
		if (titleId == 0x000500301001500A ||
			titleId == 0x000500301001510A ||
			titleId == 0x000500301001520A)
		{
			// friend list has no icon, just fail immediately
			cemuLog_logDebug(LogType::Force, "Requesting IDBE for Friend List. Return none instead");
			return std::nullopt;
		}

		std::vector<uint8> idbeData = IDBE_RequestRawEncrypted(networkService, titleId);
		if (idbeData.size() < 0x22)
			return std::nullopt;
		if (idbeData[0] != 0)
		{
			cemuLog_log(LogType::Force, "IDBE_Request: File has invalid version");
			return std::nullopt;
		}
		uint8 keyIndex = idbeData[1];
		if (keyIndex >= 4)
		{
			cemuLog_log(LogType::Force, "IDBE_Request: Key index out of range");
			return std::nullopt;
		}
		if (idbeData.size() < (0x22 + sizeof(IDBEIconDataV0)))
		{
			cemuLog_log(LogType::Force, "IDBE_Request: File size does not match");
			return std::nullopt;
		}
		// extract hash and encrypted icon data
		uint8 hash[32];
		std::memcpy(hash, idbeData.data() + 0x2, 32);
		IDBEIconDataV0 iconDataV0;
		std::memcpy(&iconDataV0, idbeData.data() + 0x22, sizeof(IDBEIconDataV0));
		// decrypt icon data and hash
		_decryptIDBEAndHash(&iconDataV0, hash, keyIndex);
		// verify hash of decrypted data
		uint8 calcHash[SHA256_DIGEST_LENGTH];
		SHA256((const unsigned char*) &iconDataV0, sizeof(IDBEIconDataV0), calcHash);
		if (std::memcmp(calcHash, hash, SHA256_DIGEST_LENGTH) != 0)
		{
			cemuLog_log(LogType::Force, "IDBE_Request: Hash mismatch");
			return std::nullopt;
		}
		return std::optional(iconDataV0);
	}
};
