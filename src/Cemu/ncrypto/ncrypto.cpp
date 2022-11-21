#include "Common/precompiled.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "util/helpers/helpers.h"

#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"
#include "openssl/sha.h"
#include "openssl/ecdsa.h"

#include "util/crypto/aes128.h"

void iosuCrypto_getDeviceCertificate(void* certOut, sint32 len);
void iosuCrypto_getDeviceCertPrivateKey(void* privKeyOut, sint32 len);
bool iosuCrypto_getDeviceId(uint32* deviceId);
void iosuCrypto_getDeviceSerialString(char* serialString);

void iosuCrypto_readOtpData(void* output, sint32 wordIndex, sint32 size);

void iosuCrypto_readSeepromData(void* output, sint32 wordIndex, sint32 size);

extern bool hasSeepromMem; // remove later

namespace NCrypto
{
	std::string base64Encode(const void* inputMem, size_t inputLen)
	{
		const uint8* input = (const uint8*)inputMem;

		static const char* base64_charset =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789+/";

		std::string strBase64;
		strBase64.resize((inputLen * 4) / 3 + 16);

		int i = 0;
		int j = 0;
		unsigned char charArray_3[3];
		unsigned char charArray_4[4];
		sint32 outputLength = 0;
		while (inputLen--)
		{
			charArray_3[i++] = *(input++);
			if (i == 3)
			{
				charArray_4[0] = (charArray_3[0] & 0xfc) >> 2;
				charArray_4[1] = ((charArray_3[0] & 0x03) << 4) + ((charArray_3[1] & 0xf0) >> 4);
				charArray_4[2] = ((charArray_3[1] & 0x0f) << 2) + ((charArray_3[2] & 0xc0) >> 6);
				charArray_4[3] = charArray_3[2] & 0x3f;
				for (i = 0; (i < 4); i++)
				{
					strBase64[outputLength] = base64_charset[charArray_4[i]];
					outputLength++;
				}
				i = 0;
			}
		}
		if (i)
		{
			for (j = i; j < 3; j++)
				charArray_3[j] = '\0';

			charArray_4[0] = (charArray_3[0] & 0xfc) >> 2;
			charArray_4[1] = ((charArray_3[0] & 0x03) << 4) + ((charArray_3[1] & 0xf0) >> 4);
			charArray_4[2] = ((charArray_3[1] & 0x0f) << 2) + ((charArray_3[2] & 0xc0) >> 6);
			charArray_4[3] = charArray_3[2] & 0x3f;

			for (j = 0; j < (i + 1); j++)
			{
				strBase64[outputLength] = base64_charset[charArray_4[j]];
				outputLength++;
			}
			while (i++ < 3)
			{
				strBase64[outputLength] = '=';
				outputLength++;
			}
		}

		cemu_assert(outputLength <= strBase64.size());
		strBase64.resize(outputLength);
		return strBase64;
	}

	std::vector<uint8> base64Decode(std::string_view inputStr)
	{
		static constexpr unsigned char kDecodingTable[] = {
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
		  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
		  64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
		  64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
		};

		size_t in_len = inputStr.size();
		if (in_len <= 3 || (in_len & 3) != 0)
			return std::vector<uint8>(); // invalid length

		std::vector<uint8> output;

		size_t out_len = in_len / 4 * 3;
		if (inputStr[in_len - 1] == '=') out_len--;
		if (inputStr[in_len - 2] == '=') out_len--;

		output.resize(out_len);

		for (size_t i = 0, j = 0; i < in_len;) 
		{
			uint32 a = inputStr[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(inputStr[i++])];
			uint32 b = inputStr[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(inputStr[i++])];
			uint32 c = inputStr[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(inputStr[i++])];
			uint32 d = inputStr[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(inputStr[i++])];

			uint32 triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

			if (j < out_len) output[j++] = (triple >> 2 * 8) & 0xFF;
			if (j < out_len) output[j++] = (triple >> 1 * 8) & 0xFF;
			if (j < out_len) output[j++] = (triple >> 0 * 8) & 0xFF;
		}

		return output;
	}

	void base64Tests()
	{
		std::vector<uint8> random;
		for (sint32 i = 0; i < 100; i++)
		{
			random.resize(0 + i);
			for (size_t x = 0; x < random.size(); x++)
				random[x] = (uint8)(i * 21 + x * 133);

			std::string b64 = base64Encode(random.data(), random.size());
			std::vector<uint8> dec = base64Decode(b64);

			cemu_assert(random == dec);
		}
	}

	/* Hashing */

	void GenerateHashSHA1(const void* data, size_t len, CHash160& hashOut)
	{
		SHA1((const unsigned char*) data, len, hashOut.b);
	}

	void GenerateHashSHA256(const void* data, size_t len, CHash256& hashOut)
	{
		SHA256((const unsigned char*) data, len, hashOut.b);
	}

	/* Ticket */
	struct ETicketFileHeaderWiiU
	{
		/*
			Ticket version 0:
				SHA1 hash
			Ticket version 1:
				SHA256 hash + has item rights
		
		*/

		/* +0x000 */ uint32be signatureType;
		/* +0x004 */ uint8 sig[0x100];
		/* +0x104 */ uint8 _ukn104[0x180 - 0x104];

		/* +0x180 */ ECCPubKey publicKey;
		/* +0x1BC */ uint8 ticketFormatVersion;
		/* +0x1BD */ uint8 _ukn1BD;
		/* +0x1BE */ uint8 _ukn1BE;

		/* +0x1BF */ uint8 encryptedTitleKey[16];
		/* +0x1CF */ uint8 _ukn1CF; // probably padding

		/* +0x1D0 */ uint32be ticketIdHigh;
		/* +0x1D4 */ uint32be ticketIdLow;

		/* +0x1D8 */ uint32be deviceId; // ticket personalized to this deviceId. Zero if not personalized

		/* +0x1DC */ uint32be titleIdHigh;
		/* +0x1E0 */ uint32be titleIdLow;
		/* +0x1E4 */ uint16be ukn1E4;
		/* +0x1E6 */ uint16be titleVersion; // also used as ticket version (for AOC content)?

		/* +0x1E8 */ uint8 _ukn1E8[0x21C - 0x1E8];

		/* +0x21C */ uint32be accountId;

		/* V1 extension header starts at +0x2A4 */ 
	};

	struct ETicketFileHeaderExtV1
	{
		/* starts at +0x2A4 */
		/* +0x000 */ uint16be headerVersion;
		/* +0x002 */ uint16be headerSize;
		/* +0x004 */ uint32be ukn008;
		/* +0x008 */ uint32be sectionTableOffset;
		/* +0x00C */ uint16be sectionTableNumEntries;
		/* +0x00E */ uint16be sectionTableEntrySize;
		/**/
	};

	struct ETicketFileHeaderExtV1SectionHeader
	{
		enum
		{
			SECTION_TYPE_CONTENT_RIGHTS = 3, // content rights
		};

		/* +0x00 */ uint32be sectionOffset;
		/* +0x04 */ uint32be entryCount;
		/* +0x08 */ uint32be entrySize;
		/* +0x0C */ uint32be sectionSize;
		/* +0x10 */ uint16be type;
		/* +0x12 */ uint16be ukn00;
	};

	static_assert(sizeof(ETicketFileHeaderWiiU) == 0x220);

	struct ETicketV1_ContentRights
	{
		uint32be baseIndex; // first index for bitmask?
		uint8be rightBitmask[0x80];

		uint32 GetRightsCount() const
		{
			return sizeof(rightBitmask) * 8;
		}

		bool GetRight(uint32 index) const
		{
			cemu_assert_debug(index < GetRightsCount());
			if (index >= GetRightsCount())
				return false;
			return ((rightBitmask[(index/8)]>>(index & 7)) & 1) != 0;
		}
	};

	static_assert(sizeof(ETicketV1_ContentRights) == 0x84);

	bool ETicketParser::parse(const uint8* data, size_t size)
	{
		auto readStruct = [&](uint32 offset, uint32 readSize) -> void*
		{
			if ((offset + readSize) > size)
				return nullptr;
			return (void*)((const uint8*)data + offset);
		};

		if (size < sizeof(ETicketFileHeaderWiiU))
			return false;
		ETicketFileHeaderWiiU* header = (ETicketFileHeaderWiiU*)readStruct(0, sizeof(ETicketFileHeaderWiiU));
		if (!header)
			return false;
		m_titleId = MakeU64(header->titleIdHigh, header->titleIdLow);
		m_ticketId = MakeU64(header->ticketIdHigh, header->ticketIdLow);
		m_ticketFormatVersion = header->ticketFormatVersion;
		m_titleVersion = header->titleVersion;
		uint32 titleIdHigh = (m_titleId >> 32);
		if ((titleIdHigh >> 16) != 0x5)
			return false; // title id should start with 0005... (Wii U platform id?)

		m_isPersonalized = header->deviceId != 0;
		m_deviceId = header->deviceId;
		m_publicKey = header->publicKey;

		std::memcpy(m_encryptedTitleKey, header->encryptedTitleKey, 16);

		cemu_assert_debug(header->ukn1E4 == 0);

		// read V1 extension
		if (m_ticketFormatVersion >= 1)
		{
			if ((titleIdHigh) == 0x0005000c)
			{
				ETicketFileHeaderExtV1* extHeader = (ETicketFileHeaderExtV1*)readStruct(0x2A4, sizeof(ETicketFileHeaderExtV1)); //  (ETicketFileHeaderExtV1*)((uint8*)header + 0x2A4);
				if (!extHeader)
					return false;
				cemu_assert_debug(extHeader->sectionTableEntrySize == 0x14);
				for (uint32 i = 0; i < extHeader->sectionTableNumEntries; i++)
				{
					ETicketFileHeaderExtV1SectionHeader* sectHeader = (ETicketFileHeaderExtV1SectionHeader*)readStruct(0x2A4 + extHeader->sectionTableOffset, sizeof(ETicketFileHeaderExtV1SectionHeader));
					if (!sectHeader)
						return false;
					if (sectHeader->type == ETicketFileHeaderExtV1SectionHeader::SECTION_TYPE_CONTENT_RIGHTS)
					{
						if (sectHeader->entrySize != sizeof(ETicketV1_ContentRights))
						{
							cemuLog_log(LogType::Force, "ETicket: Failed to parse ticket with invalid rights size");
							return false;
						}
						cemu_assert_debug(sectHeader->entryCount == 1);
						for (uint32 r = 0; r < sectHeader->entryCount; r++)
						{
							ETicketV1_ContentRights* rights = (ETicketV1_ContentRights*)readStruct(0x2A4 + sectHeader->sectionOffset + r * sectHeader->entrySize, sizeof(ETicketV1_ContentRights));
							cemu_assert_debug(rights->baseIndex == 0);
							if (rights->baseIndex > 0x1000)
							{
								cemuLog_log(LogType::Force, "ETicket: Invalid content rights index ({})", (uint32)rights->baseIndex);
								continue;
							}
							size_t maxRightsCount = rights->baseIndex + rights->GetRightsCount();
							if (maxRightsCount > m_contentRights.size())
								m_contentRights.resize(maxRightsCount);
							for (uint32 x = 0; x < rights->GetRightsCount(); x++)
								m_contentRights[x + rights->baseIndex] = rights->GetRight(x);
						}
					}
					else
					{
						cemu_assert_debug(false);
					}
				}
			}
		}
		return true;
	}

	void ETicketParser::GetTitleKey(AesKey& key)
	{
		// the key is encrypted using the titleId as IV + 8 zero bytes
		uint8 iv[16]{};
		*(uint64be*)iv = m_titleId;

		uint8 commonKey[16] = { 0xD7,0xB0,0x04,0x02,0x65,0x9B,0xA2,0xAB,0xD2,0xCB,0x0D,0xB2,0x7F,0xA2,0xB6,0x56 };

		AES128_CBC_decrypt(key.b, m_encryptedTitleKey, 16, commonKey, iv);
	}

	// personalized tickets have an extra layer of encryption for the title key
	bool ETicketParser::Depersonalize(uint8* ticketData, size_t ticketSize, uint32 deviceId, const ECCPrivKey& devicePrivKey)
	{
		ETicketParser ticketParser;
		if (!ticketParser.parse(ticketData, ticketSize))
			return false;
		if (!ticketParser.IsPersonalized())
			return false;
		if (ticketParser.m_deviceId != deviceId)
		{
			cemuLog_log(LogType::Force, "Personalized ticket does not match deviceId");
			return false;
		}

		// decrypt personalized titlekey
		EC_KEY* ec_privKey = devicePrivKey.getPrivateKey();
		EC_POINT* ec_publicKey = ticketParser.m_publicKey.getPublicKeyAsPoint();

		uint8 sharedKey[128]{};
		int sharedKeyLen = ECDH_compute_key(sharedKey, sizeof(sharedKey), ec_publicKey, ec_privKey, nullptr);
		cemu_assert(sharedKeyLen > 16);
		EC_KEY_free(ec_privKey);
		EC_POINT_free(ec_publicKey);

		NCrypto::CHash160 sharedKeySHA1;
		NCrypto::GenerateHashSHA1(sharedKey, sharedKeyLen, sharedKeySHA1);

		uint8 aesSharedKey[16]{};
		std::memcpy(aesSharedKey, sharedKeySHA1.b, 16);

		uint8 iv[16]{};
		*(uint64be*)iv = ticketParser.m_ticketId;

		uint8 ticketKey[16];
		AES128_CBC_decrypt(ticketKey, ticketParser.m_encryptedTitleKey, 16, aesSharedKey, iv);

		// store de-personalized key and remove personal data from ticket
		ETicketFileHeaderWiiU* header = (ETicketFileHeaderWiiU*)ticketData;
		std::memcpy(header->encryptedTitleKey, ticketKey, 16);
		header->deviceId = 0;
		header->accountId = 0;

		return true;
	}

	/* Title meta data */

	struct TMDFileHeaderWiiU
	{
		/* +0x000 */ uint32be signatureType;

		/* +0x004 */ uint8be sig[0x100];
		/* +0x104 */ uint8be _padding104[0x140 - 0x104];
		/* +0x140 */ uint8be _ukn140[0x40];

		/* +0x180 */ uint8be tmdVersion;
		/* +0x181 */ uint8be _ukn181;
		/* +0x182 */ uint8be _ukn182;
		/* +0x183 */ uint8be isVWii;
		/* +0x184 */ uint32be iosTitleIdHigh;
		/* +0x188 */ uint32be iosTitleIdLow;
		/* +0x18C */ uint32be titleIdHigh;
		/* +0x190 */ uint32be titleIdLow;
		/* +0x194 */ uint32be titleType;
		/* +0x198 */ uint16be group;
		/* +0x19A */ uint16be _ukn19A;
		/* +0x19C */ uint16be region;
		/* +0x19E */ uint8be ratings[16];
		/* +0x1AE */ uint8be _ukn1AE[12];
		/* +0x1BA */ uint8be _ipcMask[12];
		/* +0x1C6 */ uint8be _ukn1C6[18];
		/* +0x1D8 */ uint32be accessRightsMask;
		/* +0x1DC */ uint16be titleVersion;
		/* +0x1DE */ uint16be numContent;
		/* +0x1E0 */ uint32be _ukn1E0;
		/* +0x1E4 */ uint8 uknHash[32]; // hash of array at 0x204

		/* +0x204 */
		struct 
		{
			// pointer to cert data and cert hash?
			uint16 ukn00; // index?
			uint16 ukn02;
			uint8 hash[32];
		}ContentInfo[64];
	};

	static_assert(sizeof(TMDFileHeaderWiiU) == 0x204 + 64*36);

	struct TMDFileContentEntryWiiU
	{
		/* +0x00 */ uint32be contentId;
		/* +0x04 */ uint16be index;
		/* +0x06 */ uint16be type;
		/* +0x08 */ uint32be sizeHigh;
		/* +0x0C */ uint32be sizeLow;
		/* +0x10 */ uint8 hashSHA256[32]; // only the first 20 bytes of the hash seem to be stored?
	};

	static_assert(sizeof(TMDFileContentEntryWiiU) == 0x30);

	bool TMDParser::parse(const uint8* data, size_t size)
	{
		if (size < sizeof(TMDFileHeaderWiiU))
		{
			cemuLog_force("TMD size {} below minimum size of {}", size, sizeof(TMDFileHeaderWiiU));
			return false;
		}
		TMDFileHeaderWiiU* header = (TMDFileHeaderWiiU*)data;
		m_titleId = ((uint64)header->titleIdHigh << 32) | ((uint64)header->titleIdLow);
		m_titleVersion = header->titleVersion;
		size_t expectedSize = sizeof(TMDFileHeaderWiiU) + sizeof(TMDFileContentEntryWiiU) * header->numContent;
		if (size < expectedSize)
		{
			cemuLog_force("TMD size {} below expected size of {}. Content count: {}", size, expectedSize, (uint16)header->numContent);
			return false;
		}

		// parse content
		TMDFileContentEntryWiiU* contentEntry = (TMDFileContentEntryWiiU*)(header + 1);
		for (uint32 i = 0; i < header->numContent; i++)
		{
			ContentEntry c{};
			c.contentId = contentEntry->contentId;
			c.index = contentEntry->index;
			c.size = MakeU64(contentEntry->sizeHigh, contentEntry->sizeLow);
			c.contentFlags = static_cast<TMDParser::TMDContentFlags>((uint16)contentEntry->type);
			std::memcpy(c.hash32, contentEntry->hashSHA256, sizeof(c.hash32));
			m_content.emplace_back(c);
			contentEntry++;
		}

		// todo - parse certificates
		return true;
	}

	/* ECC PrivateKey helper functions */

	void ECCPrivKey::setPrivateKey(EC_KEY* key)
	{
		const BIGNUM* bnPrivKey = EC_KEY_get0_private_key(key);
		memset(this->keyData, 0, sizeof(this->keyData));
		BN_bn2binpad(bnPrivKey, this->keyData, sizeof(this->keyData));
	}

	EC_KEY* ECCPrivKey::getPrivateKey() const
	{
		BIGNUM* bn_privKey = BN_new();
		BN_bin2bn(this->keyData, sizeof(this->keyData), bn_privKey);
		EC_KEY* ec_privKey = EC_KEY_new_by_curve_name(NID_sect233r1);
		EC_KEY_set_private_key(ec_privKey, bn_privKey);
		BN_free(bn_privKey);
		return ec_privKey;
	}

	ECCPrivKey ECCPrivKey::getDeviceCertPrivateKey()
	{
		ECCPrivKey key{};
		iosuCrypto_getDeviceCertPrivateKey(key.keyData, sizeof(key.keyData));
		return key;
	}

	/* ECC PublicKey helper functions */

	EC_KEY* ECCPubKey::getPublicKey()
	{
		BIGNUM* bn_x = BN_new();
		BIGNUM* bn_y = BN_new();
		BN_bin2bn(this->x, sizeof(this->x), bn_x);
		BN_bin2bn(this->y, sizeof(this->y), bn_y);

		EC_KEY* ec_pubKey = EC_KEY_new_by_curve_name(NID_sect233r1);
		int r = EC_KEY_set_public_key_affine_coordinates(ec_pubKey, bn_x, bn_y);

		BN_free(bn_x);
		BN_free(bn_y);

		return ec_pubKey;
	}

	EC_POINT* ECCPubKey::getPublicKeyAsPoint()
	{
		BN_CTX* ctx = BN_CTX_new();
		BIGNUM* bn_x = BN_new();
		BIGNUM* bn_y = BN_new();
		BN_bin2bn(this->x, sizeof(this->x), bn_x);
		BN_bin2bn(this->y, sizeof(this->y), bn_y);
		EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_sect233r1);
		EC_POINT* pubkey = EC_POINT_new(group);
		EC_POINT_set_affine_coordinates(group, pubkey, bn_x, bn_y, ctx);
		EC_GROUP_free(group);
		BN_CTX_free(ctx);
		BN_free(bn_x);
		BN_free(bn_y);
		return pubkey;
	}

	ECCPubKey ECCPubKey::generateFromPrivateKey(ECCPrivKey& privKey)
	{
		BIGNUM* bn_privKey = BN_new();
		BN_bin2bn(privKey.keyData, sizeof(privKey.keyData), bn_privKey);

		// gen public key from private key
		EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_sect233r1);
		EC_POINT* pubkey = EC_POINT_new(group);
		EC_POINT_mul(group, pubkey, bn_privKey, NULL, NULL, NULL);
		BIGNUM* bn_x = BN_new();
		BIGNUM* bn_y = BN_new();
		EC_POINT_get_affine_coordinates(group, pubkey, bn_x, bn_y, NULL);

		// store public key
		ECCPubKey genPubKey;
		BN_bn2binpad(bn_x, genPubKey.x, sizeof(genPubKey.x));
		BN_bn2binpad(bn_y, genPubKey.y, sizeof(genPubKey.y));
	
		// clean up and return
		EC_POINT_free(pubkey);
		BN_free(bn_y);
		BN_free(bn_x);
		BN_free(bn_privKey);

		return genPubKey;
	}

	/* Signature helper functions */

	ECDSA_SIG* ECCSig::getSignature()
	{
		BIGNUM* bn_r = BN_new();
		BIGNUM* bn_s = BN_new();
		BN_bin2bn(this->r, 30, bn_r);
		BN_bin2bn(this->s, 30, bn_s);

		ECDSA_SIG* ec_sig = ECDSA_SIG_new();
		ECDSA_SIG_set0(ec_sig, bn_r, bn_s); // ownership of bn_r and bn_s transferred to SIG as well, do not free manually

		return ec_sig;
	}

	void ECCSig::setSignature(ECDSA_SIG* sig)
	{
		const BIGNUM* sig_r = nullptr, * sig_s = nullptr;
		ECDSA_SIG_get0(sig, &sig_r, &sig_s);

		sint32 lenR = BN_num_bytes(sig_r);
		sint32 lenS = BN_num_bytes(sig_s);
		cemu_assert_debug(lenR <= 30);
		cemu_assert_debug(lenS <= 30);

		memset(this->r, 0, sizeof(this->r));
		memset(this->s, 0, sizeof(this->s));
		BN_bn2binpad(sig_r, this->r, 30);
		BN_bn2binpad(sig_s, this->s, 30);
	}

	/* Certificate */

	bool CertECC::decodeFromBase64(std::string_view input)
	{
		auto v = base64Decode(input);
		if (v.size() != sizeof(CertECC))
			return false;
		memcpy(this, v.data(), sizeof(CertECC));
		return true;
	}

	std::string CertECC::encodeToBase64()
	{
		return base64Encode(this, sizeof(CertECC));
	}

	bool CertECC::verifySignatureViaPubKey(ECCPubKey& signerPubKey)
	{
		uint8 hash[SHA256_DIGEST_LENGTH];
		SHA256((const unsigned char *) this->issuer, 0x100, hash);

		EC_KEY* ecPubKey = signerPubKey.getPublicKey();
		ECDSA_SIG* ecSig = this->signature.getSignature();

		int r = ECDSA_do_verify(hash, sizeof(hash), ecSig, ecPubKey);

		ECDSA_SIG_free(ecSig);
		EC_KEY_free(ecPubKey);

		return r == 1; // true if valid signature
	}

	void CertECC::sign(ECCPrivKey& signerPrivKey)
	{
		uint8 hash[SHA256_DIGEST_LENGTH];
		SHA256((const unsigned char *) this->issuer, 0x100, hash);

		// generate signature
		EC_KEY* ec_privKey = signerPrivKey.getPrivateKey();
		ECDSA_SIG* sig = ECDSA_do_sign(hash, sizeof(hash), ec_privKey);
		EC_KEY_free(ec_privKey);

		// store signature
		const BIGNUM* bn_r = nullptr, *bn_s = nullptr;
		ECDSA_SIG_get0(sig, &bn_r, &bn_s);
		BN_bn2binpad(bn_r, this->signature.r, sizeof(this->signature.r));
		BN_bn2binpad(bn_s, this->signature.s, sizeof(this->signature.s));

		ECDSA_SIG_free(sig);
	}

	CertECC CertECC::GetDeviceCertificate()
	{
		CertECC deviceCert{};
		iosuCrypto_getDeviceCertificate(&deviceCert, sizeof(CertECC));
		return deviceCert;
	}

	// generate a new public key + certificate from privateKey. Certificate is signed with the device key
	CertECC CertECC::generateCertificate(uint32 signerTitleIdHigh, uint32 signerTitleIdLow, ECCPrivKey& privKeySigner, ECCPrivKey& privKeyIn, ECCPubKey& pubKeyOut)
	{
		CertECC deviceCert = GetDeviceCertificate();

		CertECC newCert = deviceCert;

		cemu_assert(newCert.signatureType == CertECC::SIGTYPE::ECC_SHA256);
		// update date
		newCert.date = 0;
		// update issuer
		strcat(newCert.issuer, "-");
		strcat(newCert.issuer, newCert.ngName);

		// update subject
		memset(newCert.ngName, 0, sizeof(newCert.ngName));
		sprintf(newCert.ngName, "AP%08x%08x", signerTitleIdHigh, signerTitleIdLow);

		// calculate public key from input private key
		newCert.publicKey = ECCPubKey::generateFromPrivateKey(privKeyIn);
		pubKeyOut = newCert.publicKey;

		// sign certificate
		newCert.sign(privKeySigner);

		return newCert;
	}

	ECCSig signHash(uint32 signerTitleIdHigh, uint32 signerTitleIdLow, uint8* hash, sint32 hashLen, CertECC& certChainOut)
	{
		// generate key pair (we only care about the private key)
		EC_KEY* ec_keyPair = EC_KEY_new_by_curve_name(NID_sect233r1);
		EC_KEY_generate_key(ec_keyPair);

		ECCPrivKey privKey;
		privKey.setPrivateKey(ec_keyPair);

		EC_KEY_free(ec_keyPair);

		// get public key and certificate
		ECCPubKey pubKey;
		ECCPrivKey signerPrivKey = ECCPrivKey::getDeviceCertPrivateKey();
		certChainOut = CertECC::generateCertificate(signerTitleIdHigh, signerTitleIdLow, signerPrivKey, privKey, pubKey);
		
		// generate signature
		cemu_assert_debug(hashLen == 32);
		EC_KEY* ec_privKey = privKey.getPrivateKey();
		ECDSA_SIG* sig = ECDSA_do_sign(hash, hashLen, ec_privKey);
		EC_KEY_free(ec_privKey);

		// verify
		EC_KEY* ec_pubKey = pubKey.getPublicKey();
		bool isValid = ECDSA_do_verify(hash, hashLen, sig, ec_pubKey) == 1;
		EC_KEY_free(ec_pubKey);
		cemu_assert(isValid);

		// store signature
		ECCSig eccSig;
		const BIGNUM* bn_r = nullptr, * bn_s = nullptr;
		ECDSA_SIG_get0(sig, &bn_r, &bn_s);
		BN_bn2binpad(bn_r, eccSig.r, sizeof(eccSig.r));
		BN_bn2binpad(bn_s, eccSig.s, sizeof(eccSig.s));

		ECDSA_SIG_free(sig);

		return eccSig;
	}

	bool verifyHashSignature(uint8* hash, sint32 hashLen, ECCPubKey& certChainPubKey, ECCSig& sig)
	{
		EC_KEY* ec_pubKey = certChainPubKey.getPublicKey();

		ECDSA_SIG* ecdsa_sig = sig.getSignature();

		bool r = ECDSA_do_verify(hash, hashLen, ecdsa_sig, ec_pubKey) == 1;

		EC_KEY_free(ec_pubKey);
		ECDSA_SIG_free(ecdsa_sig);
		return r;
	}

	bool verifyCert(CertECC& cert, NCrypto::ECCPubKey& signerPubKey)
	{
		CertECC::SIGTYPE sigType = cert.signatureType;

		if (sigType == CertECC::SIGTYPE::ECC_SHA256)
		{
			// used for Wii U certs
			NCrypto::CHash256 hash;
			NCrypto::GenerateHashSHA256(cert.issuer, 0x100, hash);
			return NCrypto::verifyHashSignature(hash.b, 32, signerPubKey, cert.signature);
		}
		else if (sigType == CertECC::SIGTYPE::ECC_SHA1)
		{
			// from Wii era
			NCrypto::CHash160 hash;
			NCrypto::GenerateHashSHA1(cert.issuer, 0x100, hash);
			return NCrypto::verifyHashSignature(hash.b, 20, signerPubKey, cert.signature);
		}
		else
		{
			cemu_assert_unimplemented();
		}
		return false;
	}

	uint32 GetDeviceId()
	{
		uint32 deviceId;
		if (!iosuCrypto_getDeviceId(&deviceId))
			return 0x11223344;
		return deviceId;
	}

	std::string GetSerial()
	{
		char serialBuffer[128]{};
		iosuCrypto_getDeviceSerialString(serialBuffer);
		return serialBuffer;
	}

	bool SEEPROM_IsPresent()
	{
		return hasSeepromMem;
	}

	CafeConsoleRegion SEEPROM_GetRegion()
	{
		uint8 seepromRegionU32[4] = {};
		iosuCrypto_readSeepromData(seepromRegionU32, 0xA4, 4);
		if (seepromRegionU32[3] == 0)
		{
			cemuLog_log(LogType::Force, "SEEPROM region is invalid (0)");
		}
		return (CafeConsoleRegion)seepromRegionU32[3];
	}

	std::string GetRegionAsString(CafeConsoleRegion regionCode)
	{
		if (regionCode == CafeConsoleRegion::EUR)
			return "EUR";
		else if (regionCode == CafeConsoleRegion::USA)
			return "USA";
		else if (regionCode == CafeConsoleRegion::JPN)
			return "JPN";
		else if (regionCode == CafeConsoleRegion::CHN)
			return "CHN";
		else if (regionCode == CafeConsoleRegion::KOR)
			return "KOR";
		else if (regionCode == CafeConsoleRegion::TWN)
			return "TWN";
		cemuLog_force("Unknown region code 0x{:x}", (uint32)regionCode);
		return "UKN";
	}

	const std::unordered_map<sint32, const char*> g_countryTable = {
		{1,"JP"},
		{8,"AI"},
		{9,"AG"},
		{10,"AR"},
		{11,"AW"},
		{12,"BS"},
		{13,"BB"},
		{14,"BZ"},
		{15,"BO"},
		{16,"BR"},
		{17,"VG"},
		{18,"CA"},
		{19,"KY"},
		{20,"CL"},
		{21,"CO"},
		{22,"CR"},
		{23,"DM"},
		{24,"DO"},
		{25,"EC"},
		{26,"SV"},
		{27,"GF"},
		{28,"GD"},
		{29,"GP"},
		{30,"GT"},
		{31,"GY"},
		{32,"HT"},
		{33,"HN"},
		{34,"JM"},
		{35,"MQ"},
		{36,"MX"},
		{37,"MS"},
		{38,"AN"},
		{39,"NI"},
		{40,"PA"},
		{41,"PY"},
		{42,"PE"},
		{43,"KN"},
		{44,"LC"},
		{45,"VC"},
		{46,"SR"},
		{47,"TT"},
		{48,"TC"},
		{49,"US"},
		{50,"UY"},
		{51,"VI"},
		{52,"VE"},
		{64,"AL"},
		{65,"AU"},
		{66,"AT"},
		{67,"BE"},
		{68,"BA"},
		{69,"BW"},
		{70,"BG"},
		{71,"HR"},
		{72,"CY"},
		{73,"CZ"},
		{74,"DK"},
		{75,"EE"},
		{76,"FI"},
		{77,"FR"},
		{78,"DE"},
		{79,"GR"},
		{80,"HU"},
		{81,"IS"},
		{82,"IE"},
		{83,"IT"},
		{84,"LV"},
		{85,"LS"},
		{86,"LI"},
		{87,"LT"},
		{88,"LU"},
		{89,"MK"},
		{90,"MT"},
		{91,"ME"},
		{92,"MZ"},
		{93,"NA"},
		{94,"NL"},
		{95,"NZ"},
		{96,"NO"},
		{97,"PL"},
		{98,"PT"},
		{99,"RO"},
		{100,"RU"},
		{101,"RS"},
		{102,"SK"},
		{103,"SI"},
		{104,"ZA"},
		{105,"ES"},
		{106,"SZ"},
		{107,"SE"},
		{108,"CH"},
		{109,"TR"},
		{110,"GB"},
		{111,"ZM"},
		{112,"ZW"},
		{113,"AZ"},
		{114,"MR"},
		{115,"ML"},
		{116,"NE"},
		{117,"TD"},
		{118,"SD"},
		{119,"ER"},
		{120,"DJ"},
		{121,"SO"},
		{122,"AD"},
		{123,"GI"},
		{124,"GG"},
		{125,"IM"},
		{126,"JE"},
		{127,"MC"},
		{128,"TW"},
		{136,"KR"},
		{144,"HK"},
		{145,"MO"},
		{152,"ID"},
		{153,"SG"},
		{154,"TH"},
		{155,"PH"},
		{156,"MY"},
		{160,"CN"},
		{168,"AE"},
		{170,"EG"},
		{171,"OM"},
		{172,"QA"},
		{173,"KW"},
		{174,"SA"},
		{175,"SY"},
		{176,"BH"},
		{177,"JO"},
		{184,"SM"},
		{185,"VA"},
		{186,"BM"},
		{187,"IN"},
		{192,"NG"},
		{193,"AO"},
		{194,"GH"}
	};

	const char* GetCountryAsString(sint32 index)
	{
		const auto it = g_countryTable.find(index);
		if (it == g_countryTable.cend())
			return "NN";
		return it->second;
	}

	void unitTests()
	{
		base64Tests();
	}

};
