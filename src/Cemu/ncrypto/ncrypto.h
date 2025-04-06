#pragma once
#include "Common/betype.h"
#include "config/CemuConfig.h" // for ConsoleRegion

/* OpenSSL forward declarations */
typedef struct ec_key_st EC_KEY;
typedef struct ec_point_st EC_POINT;
typedef struct ECDSA_SIG_st ECDSA_SIG;

namespace NCrypto
{
	/* Base 64 */
	std::string base64Encode(const void* inputMem, size_t inputLen);
	std::vector<uint8> base64Decode(std::string_view inputStr);
	
	/* key helper struct */
	struct AesKey 
	{
		uint8 b[16];
	};

	/* ECC Certificate */
	struct ECCPrivKey
	{
		uint8 keyData[30];

		void setPrivateKey(EC_KEY* key);
		EC_KEY* getPrivateKey() const;

		static ECCPrivKey getDeviceCertPrivateKey();
	};

	struct ECCPubKey
	{
		uint8 x[30];
		uint8 y[30];

		EC_KEY* getPublicKey();
		EC_POINT* getPublicKeyAsPoint();

		[[nodiscard]] static ECCPubKey generateFromPrivateKey(ECCPrivKey& privKey);
	};

	struct ECCSig
	{
		uint8 r[30];
		uint8 s[30];

		ECDSA_SIG* getSignature();
		void setSignature(ECDSA_SIG* sig);

		bool doesMatch(ECCSig& otherSig) const
		{
			return memcmp(this->r, otherSig.r, sizeof(this->r)) == 0 && memcmp(this->s, otherSig.s, sizeof(this->s)) == 0;
		}

		std::string encodeToBase64()
		{
			return base64Encode(this, 60);
		}
	};

	struct CHash256 // SHA256
	{
		uint8 b[32];
	};

	struct CHash160 // SHA1
	{
		uint8 b[20];
	};

	struct CertECC
	{
		enum class SIGTYPE : uint32
		{
			ECC_SHA1   = 0x00010002, // guessed
			ECC_SHA256 = 0x00010005
		};

		/* +0x000 */ betype<SIGTYPE>	signatureType; // 01 00 02 00
		/* +0x004 */ ECCSig				signature; //uint8				signature[0x3C]; // from OTP 0xA3*4
		/* +0x040 */ uint8				ukn040[0x40]; // seems to be just padding
		/* +0x080 */ char				issuer[0x40]; // "Root - CA%08x - MS%08x" 
		/* +0x0C0 */ char				ukn0C0[0x4]; // ??? 00 00 00 02 ?
		/* +0x0C4 */ char				ngName[0x40]; // "NG%08X"
		/* +0x104 */ uint32				date; // big endian? (from OTP 0xA2*4)
		/* +0x108 */ ECCPubKey			publicKey; //uint8				publicKey[0x3C];
		/* +0x144 */ uint8				padding[0x180 - 0x144];

		bool decodeFromBase64(std::string_view input);
		std::string encodeToBase64();

		bool verifySignatureViaPubKey(ECCPubKey& signerPubKey);
		void sign(ECCPrivKey& signerPrivKey);

		static CertECC GetDeviceCertificate();
		static CertECC generateCertificate(uint32 signerTitleIdHigh, uint32 signerTitleIdLow, ECCPrivKey& privKeySigner, ECCPrivKey& privKeyIn, ECCPubKey& pubKeyOut);
	};

	static_assert(sizeof(CertECC) == 0x180);


	/* ETicket */

	class ETicketParser  // .tik parser
	{
	public:
		bool parse(const uint8* data, size_t size);
		static bool Depersonalize(uint8* ticketData, size_t ticketSize, uint32 deviceId, const ECCPrivKey& devicePrivKey);

		uint64 GetTicketId() const
		{
			return m_ticketId;
		}

		uint64 GetTitleId() const
		{
			return m_titleId;
		}

		uint16 GetTicketVersion() const
		{
			return m_titleVersion;
		}

		void GetTitleKey(AesKey& key);

		bool IsPersonalized()
		{
			return m_isPersonalized;
		}

		bool CheckRight(size_t index)
		{
			if (index >= m_contentRights.size())
				return false;
			return m_contentRights[index];
		}

	private:
		uint64 m_titleId;
		uint16 m_titleVersion;
		uint64 m_ticketId;
		uint8 m_ticketFormatVersion;
		uint8 m_encryptedTitleKey[16];
		// personalized data
		bool m_isPersonalized;
		uint32 m_deviceId;
		ECCPubKey m_publicKey;
		// rights
		std::vector<bool> m_contentRights;
	};

	/* Title meta data */

	class TMDParser
	{
	public:
		enum class TMDContentFlags : uint16
		{
			// 0x0001 -> Is encrypted?
			FLAG_SHA1 = 0x2000, // if not set, use SHA256
			FLAG_HASHED_CONTENT = 0x0002, // .app uses hashed format
		};

		struct ContentEntry
		{
			uint32 contentId;
			uint16 index;
			TMDContentFlags contentFlags;
			uint64 size;
			uint8 hash32[32];
		};

		uint64 getTitleId() const
		{
			return m_titleId;
		}

		uint16 getTitleVersion() const
		{
			return m_titleVersion;
		}

		bool parse(const uint8* data, size_t size);
		std::vector<ContentEntry>& GetContentList()
		{
			return m_content;
		}

	private:
		uint64 m_titleId;
		uint16 m_titleVersion;
		std::vector<ContentEntry> m_content;
	};

	DEFINE_ENUM_FLAG_OPERATORS(TMDParser::TMDContentFlags);

	void GenerateHashSHA256(const void* data, size_t len, CHash256& hashOut);
	ECCSig signHash(uint32 signerTitleIdHigh, uint32 signerTitleIdLow, uint8* hash, sint32 hashLen, CertECC& certChainOut);

	uint32 GetDeviceId();
	std::string GetSerial();
	CafeConsoleRegion SEEPROM_GetRegion();
	std::string GetRegionAsString(CafeConsoleRegion regionCode);
	const char* GetCountryAsString(sint32 index); // returns NN if index is not valid or known
	size_t GetCountryCount();
	bool SEEPROM_IsPresent();
	bool OTP_IsPresent();

	bool HasDataForConsoleCert();

	void unitTests();
}