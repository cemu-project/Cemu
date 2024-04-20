#include "iosu_crypto.h"

#include "config/ActiveSettings.h"
#include "openssl/bn.h"
#include "openssl/obj_mac.h"
#include "openssl/ec.h"
#include "openssl/x509.h"
#include "openssl/ssl.h"
#include "openssl/ecdsa.h"

#include "util/crypto/aes128.h"
#include "Common/FileStream.h"

uint8 otpMem[1024];
bool hasOtpMem = false;

uint8 seepromMem[512];
bool hasSeepromMem = false;

struct  
{
	bool hasCertificates;
	struct  
	{
		bool isValid;
		sint32 id;
		X509* cert;
		std::vector<uint8> certData;
		RSA* pkey;
		std::vector<uint8> pkeyDERData;
	}certList[256];
	sint32 certListCount;
}iosuCryptoCertificates = { 0 };

CertECC_t g_wiiuDeviceCert;

void iosuCrypto_readOtpData(void* output, sint32 wordIndex, sint32 size)
{
	memcpy(output, otpMem + wordIndex * 4, size);
}

void iosuCrypto_readOtpData(uint32be& output, sint32 wordIndex)
{
	memcpy(&output, otpMem + wordIndex * 4, sizeof(uint32be));
}

void iosuCrypto_readSeepromData(void* output, sint32 wordIndex, sint32 size)
{
	memcpy(output, seepromMem + wordIndex * 2, size);
}

bool iosuCrypto_getDeviceId(uint32* deviceId)
{
	uint32be deviceIdBE;
	*deviceId = 0;
	if (!hasOtpMem)
		return false;
	iosuCrypto_readOtpData(&deviceIdBE, 0x87, sizeof(uint32));
	*deviceId = (uint32)deviceIdBE;
	return true;
}

void iosuCrypto_getDeviceSerialString(char* serialString)
{
	char serialStringPart0[32]; // code
	char serialStringPart1[32]; // serial
	if (hasSeepromMem == false)
	{
		strcpy(serialString, "FEH000000000");
		return;
	}
	memset(serialStringPart0, 0, sizeof(serialStringPart0));
	memset(serialStringPart1, 0, sizeof(serialStringPart1));
	iosuCrypto_readSeepromData(serialStringPart0, 0xAC, 8);
	iosuCrypto_readSeepromData(serialStringPart1, 0xB0, 0x10);
	sprintf(serialString, "%s%s", serialStringPart0, serialStringPart1);
}

static const char* base64_charset =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

int iosuCrypto_base64Encode(unsigned char const* bytes_to_encode, unsigned int inputLen, char* output)
{
	int i = 0;
	int j = 0;
	unsigned char charArray_3[3];
	unsigned char charArray_4[4];
	sint32 outputLength = 0;
	while (inputLen--)
	{
		charArray_3[i++] = *(bytes_to_encode++);
		if (i == 3)
		{
			charArray_4[0] = (charArray_3[0] & 0xfc) >> 2;
			charArray_4[1] = ((charArray_3[0] & 0x03) << 4) + ((charArray_3[1] & 0xf0) >> 4);
			charArray_4[2] = ((charArray_3[1] & 0x0f) << 2) + ((charArray_3[2] & 0xc0) >> 6);
			charArray_4[3] = charArray_3[2] & 0x3f;
			for (i = 0; (i < 4); i++)
			{
				output[outputLength] = base64_charset[charArray_4[i]];
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
			output[outputLength] = base64_charset[charArray_4[j]];
			outputLength++;
		}
		while (i++ < 3)
		{
			output[outputLength] = '=';
			outputLength++;
		}
	}
	return outputLength;
}

std::string iosuCrypto_base64Encode(unsigned char const* bytes_to_encode, unsigned int inputLen)
{
	int encodedLen = inputLen / 3 * 4 + 16;
	std::string strB64;
	strB64.resize(encodedLen);
	int outputLen = iosuCrypto_base64Encode(bytes_to_encode, inputLen, strB64.data());
	cemu_assert_debug(outputLen < strB64.size());
	strB64.resize(outputLen);
	return strB64;
}
static_assert(sizeof(CertECC_t) == sizeof(CertECC_t));

EC_KEY* ECCPubKey_getPublicKey(ECCPubKey& pubKey) // verified and works
{
	BIGNUM* bn_r = BN_new();
	BIGNUM* bn_s = BN_new();
	BN_bin2bn(pubKey.keyData + 0, 30, bn_r);
	BN_bin2bn(pubKey.keyData + 30, 30, bn_s);

	EC_KEY* ec_pubKey = EC_KEY_new_by_curve_name(NID_sect233r1);
	int r = EC_KEY_set_public_key_affine_coordinates(ec_pubKey, bn_r, bn_s);

	BN_free(bn_r);
	BN_free(bn_s);

	return ec_pubKey;
}

ECDSA_SIG* ECCPubKey_getSignature(CertECC_t& cert)
{
	BIGNUM* bn_r = BN_new();
	BIGNUM* bn_s = BN_new();
	BN_bin2bn(cert.signature + 0, 30, bn_r);
	BN_bin2bn(cert.signature + 30, 30, bn_s);

	//EC_KEY* ec_pubKey = EC_KEY_new_by_curve_name(NID_sect233r1);
	//int r = EC_KEY_set_public_key_affine_coordinates(ec_pubKey, bn_r, bn_s);
	ECDSA_SIG* ec_sig = ECDSA_SIG_new();
	//ECDSA_do_sign_ex
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	ECDSA_SIG_set0(ec_sig, bn_r, bn_s);
#else
	BN_copy(ec_sig->r, bn_r);
	BN_copy(ec_sig->s, bn_s);
#endif

	BN_free(bn_r);
	BN_free(bn_s);

	return ec_sig;
}

void ECCPubKey_setSignature(CertECC_t& cert, ECDSA_SIG* sig)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	const BIGNUM* sig_r = nullptr, * sig_s = nullptr;
	ECDSA_SIG_get0(sig, &sig_r, &sig_s);

	sint32 lenR = BN_num_bytes(sig_r);
	sint32 lenS = BN_num_bytes(sig_s);

	cemu_assert_debug(lenR <= 30);
	cemu_assert_debug(lenS <= 30);

	memset(cert.signature, 0, sizeof(cert.signature));
	BN_bn2bin(sig_r, cert.signature + (30 - lenR));
	BN_bn2bin(sig_s, cert.signature + 60 / 2 + (30 - lenS));
#else
	sint32 lenR = BN_num_bytes(sig->r);
	sint32 lenS = BN_num_bytes(sig->s);

	cemu_assert_debug(lenR <= 30);
	cemu_assert_debug(lenS <= 30);

	memset(cert.signature, 0, sizeof(cert.signature));
	BN_bn2bin(sig->r, cert.signature + (30 - lenR));
	BN_bn2bin(sig->s, cert.signature + 60 / 2 + (30 - lenS));
#endif
}

ECCPrivKey g_consoleCertPrivKey{};

void iosuCrypto_getDeviceCertPrivateKey(void* privKeyOut, sint32 len)
{
	cemu_assert(len == 30);
	memcpy(privKeyOut, g_consoleCertPrivKey.keyData, 30);
}

void iosuCrypto_getDeviceCertificate(void* certOut, sint32 len)
{
	cemu_assert(len == 0x180);
	memcpy(certOut, &g_wiiuDeviceCert, 0x180);
}

void iosuCrypto_generateDeviceCertificate()
{
	static_assert(sizeof(g_wiiuDeviceCert) == 0x180);
	memset(&g_wiiuDeviceCert, 0, sizeof(g_wiiuDeviceCert));
	if (!hasOtpMem)
		return; // cant generate certificate without OPT

	// set header based on otp security mode
	g_wiiuDeviceCert.sigType = CertECC_t::SIGTYPE::ECC_SHA256;

	g_wiiuDeviceCert.ukn0C0[0] = 0x00;
	g_wiiuDeviceCert.ukn0C0[1] = 0x00;
	g_wiiuDeviceCert.ukn0C0[2] = 0x00;
	g_wiiuDeviceCert.ukn0C0[3] = 0x02;


	iosuCrypto_readOtpData(g_wiiuDeviceCert.signature, 0xA3, 0x3C);

	uint32be caValue;
	iosuCrypto_readOtpData(caValue, 0xA1);
	uint32be msValue;
	iosuCrypto_readOtpData(msValue, 0xA0);

	sprintf(g_wiiuDeviceCert.certificateSubject, "Root-CA%08x-MS%08x", (uint32)caValue, (uint32)msValue);

	uint32be ngNameValue;
	iosuCrypto_readOtpData(ngNameValue, 0x87);
	sprintf(g_wiiuDeviceCert.ngName, "NG%08x", (uint32)ngNameValue);

	iosuCrypto_readOtpData(&g_wiiuDeviceCert.ngKeyId, 0xA2, sizeof(uint32));


	uint8 privateKey[0x20];
	memset(privateKey, 0, sizeof(privateKey));
	iosuCrypto_readOtpData(privateKey, 0x88, 0x1E);
	memcpy(g_consoleCertPrivKey.keyData, privateKey, 30);

	auto context = BN_CTX_new();
	BN_CTX_start(context);
	BIGNUM* bn_privKey = BN_CTX_get(context);
	BN_bin2bn(privateKey, 0x1E, bn_privKey);

	EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_sect233r1);
	EC_POINT *pubkey = EC_POINT_new(group);

	EC_POINT_mul(group, pubkey, bn_privKey, NULL, NULL, NULL);

	BIGNUM* bn_x = BN_CTX_get(context);
	BIGNUM* bn_y = BN_CTX_get(context);	

	EC_POINT_get_affine_coordinates(group, pubkey, bn_x, bn_y, NULL);

	uint8 publicKeyOutput[0x3C];
	memset(publicKeyOutput, 0, sizeof(publicKeyOutput));

	sint32 lenX = BN_num_bytes(bn_x);
	sint32 lenY = BN_num_bytes(bn_y);

	BN_bn2bin(bn_x, publicKeyOutput + (0x1E - lenX)); // todo - verify if the bias is correct
	BN_bn2bin(bn_y, publicKeyOutput + 0x3C / 2 + (0x1E - lenY));

	memcpy(g_wiiuDeviceCert.publicKey, publicKeyOutput, 0x3C);

	// clean up
	EC_POINT_free(pubkey);
	BN_CTX_end(context); // clears all BN variables
	BN_CTX_free(context);
}

sint32 iosuCrypto_getDeviceCertificateBase64Encoded(char* output)
{
	iosuCrypto_base64Encode((uint8*)&g_wiiuDeviceCert, sizeof(g_wiiuDeviceCert), output);
	sint32 len = sizeof(g_wiiuDeviceCert) / 3 * 4;
	output[len] = '\0';
	return len;
}

bool iosuCrypto_loadCertificate(uint32 id, std::wstring_view mlcSubpath, std::wstring_view pkeyMlcSubpath)
{
	X509* cert = nullptr;
	// load cert data
	const auto certPath = ActiveSettings::GetMlcPath(mlcSubpath);
	auto certData = FileStream::LoadIntoMemory(certPath);
	if (!certData)
		return false; // file missing
	// get optional aes encrypted private key data
	std::optional<std::vector<uint8>> pkeyData;
	if (!pkeyMlcSubpath.empty())
	{
		const auto pkeyPath = ActiveSettings::GetMlcPath(pkeyMlcSubpath);
		pkeyData = FileStream::LoadIntoMemory(pkeyPath);
		if (!pkeyData || pkeyData->empty())
		{
			cemuLog_log(LogType::Force, "Unable to load private key file {}", pkeyPath.generic_string());
			return false;
		}
		else if ((pkeyData->size() % 16) != 0)
		{
			cemuLog_log(LogType::Force, "Private key file has invalid length. Possibly corrupted? File: {}", pkeyPath.generic_string());
			return false;
		}
	}
	// load certificate
	unsigned char* tempPtr = (unsigned char*)certData->data();
	cert = d2i_X509(nullptr, (const unsigned char**)&tempPtr, certData->size());
	if (cert == nullptr)
	{
		cemuLog_log(LogType::Force, "IOSU_CRYPTO: Unable to load certificate \"{}\"", boost::nowide::narrow(std::wstring(mlcSubpath)));
		return false;
	}
	// load optional rsa key
	RSA* pkeyRSA = nullptr;
	if (pkeyData)
	{
		cemu_assert((pkeyData->size() & 15) == 0);
		uint8 aesKey[16];
		uint8 iv[16] = { 0 };
		uint8 pkeyDecryptedData[4096];
		// decrypt pkey
		iosuCrypto_readOtpData(aesKey, 0x120 / 4, 16);
		AES128_CBC_decrypt(pkeyDecryptedData, pkeyData->data(), pkeyData->size(), aesKey, iv);
		// convert to OpenSSL RSA pkey
		unsigned char* pkeyTempPtr = pkeyDecryptedData;
		pkeyRSA = d2i_RSAPrivateKey(nullptr, (const unsigned char **)&pkeyTempPtr, pkeyData->size());
		if (pkeyRSA == nullptr)
		{
			cemuLog_log(LogType::Force, "IOSU_CRYPTO: Unable to decrypt private key \"{}\"", boost::nowide::narrow(std::wstring(pkeyMlcSubpath)));
			return false;
		}
		// encode private key as DER
		EVP_PKEY *evpPkey = EVP_PKEY_new();
		EVP_PKEY_assign_RSA(evpPkey, pkeyRSA);
		std::vector<uint8> derPKeyData(1024 * 32);
		unsigned char* derPkeyTemp = derPKeyData.data();
		sint32 derPkeySize = i2d_PrivateKey(evpPkey, &derPkeyTemp);
		derPKeyData.resize(derPkeySize);
		derPKeyData.shrink_to_fit();
		iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].pkeyDERData = derPKeyData;
	}

	// register certificate and optional pkey
	iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].cert = cert;
	iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].certData = *certData;
	iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].pkey = pkeyRSA;
	iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].id = id;
	iosuCryptoCertificates.certList[iosuCryptoCertificates.certListCount].isValid = true;
	iosuCryptoCertificates.certListCount++;
	return true;
}

bool iosuCrypto_addClientCertificate(void* sslctx, sint32 certificateId)
{
	SSL_CTX* ctx = (SSL_CTX*)sslctx;
	// find entry
	for (sint32 i = 0; i < iosuCryptoCertificates.certListCount; i++)
	{
		if (iosuCryptoCertificates.certList[i].isValid && iosuCryptoCertificates.certList[i].id == certificateId)
		{
			if (SSL_CTX_use_certificate(ctx, iosuCryptoCertificates.certList[i].cert) != 1)
			{
				cemuLog_log(LogType::Force, "Unable to setup certificate {}", certificateId);
				return false;
			}
			if (SSL_CTX_use_RSAPrivateKey(ctx, iosuCryptoCertificates.certList[i].pkey) != 1)
			{
				cemuLog_log(LogType::Force, "Unable to setup certificate {} RSA private key", certificateId);
				return false;
			}

			if (SSL_CTX_check_private_key(ctx) == false)
			{
				cemuLog_log(LogType::Force, "Certificate private key could not be validated (verify required files for online mode or disable online mode)");
			}

			return true;
		}
	}
	cemuLog_log(LogType::Force, "Certificate not found (verify required files for online mode or disable online mode)");
	return false;
}

bool iosuCrypto_addCACertificate(void* sslctx, sint32 certificateId)
{
	SSL_CTX* ctx = (SSL_CTX*)sslctx;
	// find entry
	for (sint32 i = 0; i < iosuCryptoCertificates.certListCount; i++)
	{
		if (iosuCryptoCertificates.certList[i].isValid && iosuCryptoCertificates.certList[i].id == certificateId)
		{
			X509_STORE* store = SSL_CTX_get_cert_store((SSL_CTX*)sslctx);
			X509_STORE_add_cert(store, iosuCryptoCertificates.certList[i].cert);
			return true;
		}
	}
	return false;
}

bool iosuCrypto_addCustomCACertificate(void* sslctx, uint8* certData, sint32 certLength)
{
	SSL_CTX* ctx = (SSL_CTX*)sslctx;
	X509_STORE* store = SSL_CTX_get_cert_store((SSL_CTX*)sslctx);
	unsigned char* tempPtr = (unsigned char*)certData;
	X509* cert = d2i_X509(NULL, (const unsigned char**)&tempPtr, certLength);
	if (cert == nullptr)
	{
		cemuLog_log(LogType::Force, "Invalid custom server PKI certificate");
		return false;
	}
	X509_STORE_add_cert(store, cert);
	return true;
}

uint8* iosuCrypto_getCertificateDataById(sint32 certificateId, sint32* certificateSize)
{
	for (sint32 i = 0; i < iosuCryptoCertificates.certListCount; i++)
	{
		if (iosuCryptoCertificates.certList[i].isValid && iosuCryptoCertificates.certList[i].id == certificateId)
		{
			*certificateSize = iosuCryptoCertificates.certList[i].certData.size();
			return iosuCryptoCertificates.certList[i].certData.data();
		}
	}
	return nullptr;
}

uint8* iosuCrypto_getCertificatePrivateKeyById(sint32 certificateId, sint32* certificateSize)
{
	for (sint32 i = 0; i < iosuCryptoCertificates.certListCount; i++)
	{
		if (iosuCryptoCertificates.certList[i].isValid && iosuCryptoCertificates.certList[i].id == certificateId)
		{
			*certificateSize = iosuCryptoCertificates.certList[i].pkeyDERData.size();
			return iosuCryptoCertificates.certList[i].pkeyDERData.data();
		}
	}
	return nullptr;
}

struct
{
	const int id;
	const wchar_t name[256];
	const wchar_t key[256];
} const g_certificates[] = {
	// NINTENDO CLIENT CERTS
	{ 1, L"ccerts/WIIU_COMMON_1_CERT.der", L"ccerts/WIIU_COMMON_1_RSA_KEY.aes" },
	{ 3, L"ccerts/WIIU_ACCOUNT_1_CERT.der", L"ccerts/WIIU_ACCOUNT_1_RSA_KEY.aes" },
	{ 4, L"ccerts/WIIU_OLIVE_1_CERT.der", L"ccerts/WIIU_OLIVE_1_RSA_KEY.aes" },
	{ 5, L"ccerts/WIIU_VINO_1_CERT.der", L"ccerts/WIIU_VINO_1_RSA_KEY.aes" },
	{ 6, L"ccerts/WIIU_WOOD_1_CERT.der", L"ccerts/WIIU_WOOD_1_RSA_KEY.aes" },
	{ 7, L"ccerts/WIIU_OLIVE_1_CERT.der", L"ccerts/WIIU_OLIVE_1_RSA_KEY.aes" },
	{ 8, L"ccerts/WIIU_WOOD_1_CERT.der", L"ccerts/WIIU_WOOD_1_RSA_KEY.aes" },

	// NINTENDO CA CERTS
	{ 100, L"scerts/CACERT_NINTENDO_CA.der", L"" },
	{ 101, L"scerts/CACERT_NINTENDO_CA_G2.der", L"" },
	{ 102, L"scerts/CACERT_NINTENDO_CA_G3.der", L"" },
	{ 103, L"scerts/CACERT_NINTENDO_CLASS2_CA.der", L"" },
	{ 104, L"scerts/CACERT_NINTENDO_CLASS2_CA_G2.der", L"" },
	{ 105, L"scerts/CACERT_NINTENDO_CLASS2_CA_G3.der", L"" },

	// COMMERCIAL CA CERTS
	{ 1001, L"scerts/BALTIMORE_CYBERTRUST_ROOT_CA.der", L"" },
	{ 1002, L"scerts/CYBERTRUST_GLOBAL_ROOT_CA.der", L"" },
	{ 1003, L"scerts/VERIZON_GLOBAL_ROOT_CA.der", L"" },
	{ 1004, L"scerts/GLOBALSIGN_ROOT_CA.der", L"" },
	{ 1005, L"scerts/GLOBALSIGN_ROOT_CA_R2.der", L"" },
	{ 1006, L"scerts/GLOBALSIGN_ROOT_CA_R3.der", L"" },
	{ 1007, L"scerts/VERISIGN_CLASS3_PUBLIC_PRIMARY_CA_G3.der", L"" },
	{ 1008, L"scerts/VERISIGN_UNIVERSAL_ROOT_CA.der", L"" },
	{ 1009, L"scerts/VERISIGN_CLASS3_PUBLIC_PRIMARY_CA_G5.der", L"" },
	{ 1010, L"scerts/THAWTE_PRIMARY_ROOT_CA_G3.der", L"" },
	{ 1011, L"scerts/THAWTE_PRIMARY_ROOT_CA.der", L"" },
	{ 1012, L"scerts/GEOTRUST_GLOBAL_CA.der", L"" },
	{ 1013, L"scerts/GEOTRUST_GLOBAL_CA2.der", L"" },
	{ 1014, L"scerts/GEOTRUST_PRIMARY_CA.der", L"" },
	{ 1015, L"scerts/GEOTRUST_PRIMARY_CA_G3.der", L"" },
	{ 1016, L"scerts/ADDTRUST_EXT_CA_ROOT.der", L"" },
	{ 1017, L"scerts/COMODO_CA.der", L"" },
	{ 1018, L"scerts/UTN_DATACORP_SGC_CA.der", L"" },
	{ 1019, L"scerts/UTN_USERFIRST_HARDWARE_CA.der" , L"" },
	{ 1020, L"scerts/DIGICERT_HIGH_ASSURANCE_EV_ROOT_CA.der", L"" },
	{ 1021, L"scerts/DIGICERT_ASSURED_ID_ROOT_CA.der", L"" },
	{ 1022, L"scerts/DIGICERT_GLOBAL_ROOT_CA.der", L"" },
	{ 1023, L"scerts/GTE_CYBERTRUST_GLOBAL_ROOT.der", L"" },
	{ 1024, L"scerts/VERISIGN_CLASS3_PUBLIC_PRIMARY_CA.der", L"" },
	{ 1025, L"scerts/THAWTE_PREMIUM_SERVER_CA.der", L"" },
	{ 1026, L"scerts/EQUIFAX_SECURE_CA.der", L"" },
	{ 1027, L"scerts/ENTRUST_SECURE_SERVER_CA.der", L"" },
	{ 1028, L"scerts/VERISIGN_CLASS3_PUBLIC_PRIMARY_CA_G2.der", L"" },
	{ 1029, L"scerts/ENTRUST_CA_2048.der", L"" },
	{ 1030, L"scerts/ENTRUST_ROOT_CA.der", L"" },
	{ 1031, L"scerts/ENTRUST_ROOT_CA_G2.der", L"" },
	{ 1032, L"scerts/DIGICERT_ASSURED_ID_ROOT_CA_G2.der", L"" },
	{ 1033, L"scerts/DIGICERT_GLOBAL_ROOT_CA_G2.der", L"" },
};

void iosuCrypto_loadSSLCertificates()
{
	if (iosuCryptoCertificates.hasCertificates)
		return;

	if (!hasOtpMem)
		return; // cant load certificates without OTP keys

	// load CA certificate
	bool hasAllCertificates = true;
	for( const auto& c : g_certificates )
	{
		std::wstring certDir = L"sys/title/0005001b/10054000/content/";
		std::wstring certFilePath = certDir + c.name;
		std::wstring keyFilePath;		

		if( *c.key )
			keyFilePath = certDir + c.key;
		else
			keyFilePath.clear();

		if (iosuCrypto_loadCertificate(c.id, certFilePath, keyFilePath) == false)
		{
			cemuLog_log(LogType::Force, "Unable to load certificate \"{}\"", boost::nowide::narrow(certFilePath));
			hasAllCertificates = false;
		}
	}
	iosuCryptoCertificates.hasCertificates = hasAllCertificates; // true
}

void iosuCrypto_init()
{
	// load OTP dump
	if (std::ifstream otp_file(ActiveSettings::GetUserDataPath("otp.bin"), std::ifstream::in | std::ios::binary); otp_file.is_open())
	{
		otp_file.seekg(0, std::ifstream::end);
		const auto length = otp_file.tellg();
		otp_file.seekg(0, std::ifstream::beg);
		// verify if OTP is ok
		if (length != 1024) // todo - should also check some fixed values to verify integrity of otp dump
		{
			cemuLog_log(LogType::Force, "IOSU_CRYPTO: otp.bin has wrong size (must be 1024 bytes)");
			hasOtpMem = false;
		}
		else
		{
			otp_file.read((char*)otpMem, 1024);
			hasOtpMem = (bool)otp_file;
		}
	}
	else
	{
		cemuLog_log(LogType::Force, "IOSU_CRYPTO: No otp.bin found. Online mode cannot be used");
		hasOtpMem = false;
	}

	if (std::ifstream seeprom_file(ActiveSettings::GetUserDataPath("seeprom.bin"), std::ifstream::in | std::ios::binary); seeprom_file.is_open())
	{
		seeprom_file.seekg(0, std::ifstream::end);
		const auto length = seeprom_file.tellg();
		seeprom_file.seekg(0, std::ifstream::beg);
		// verify if seeprom is ok
		if (length != 512) // todo - maybe check some known values to verify integrity of seeprom
		{
			cemuLog_log(LogType::Force, "IOSU_CRYPTO: seeprom.bin has wrong size (must be 512 bytes)");
			hasSeepromMem = false;
		}
		else
		{
			seeprom_file.read((char*)seepromMem, 512);
			hasSeepromMem = (bool)seeprom_file;
		}
	}
	else
	{
		cemuLog_log(LogType::Force, "IOSU_CRYPTO: No Seeprom.bin found. Online mode cannot be used");
		hasSeepromMem = false;
	}
	
	// generate device certificate
	iosuCrypto_generateDeviceCertificate();
	// load SSL certificates
	iosuCrypto_loadSSLCertificates();
}

bool iosuCrypto_checkRequirementMLCFile(std::string_view mlcSubpath, std::string& additionalErrorInfo_filePath)
{
	const auto path = ActiveSettings::GetMlcPath(mlcSubpath);
	additionalErrorInfo_filePath = _pathToUtf8(path);
	sint32 fileDataSize = 0;
	auto fileData = FileStream::LoadIntoMemory(path);
	if (!fileData)
		return false;
	return true;
}

sint32 iosuCrypt_checkRequirementsForOnlineMode(std::string& additionalErrorInfo)
{
	std::error_code ec;
	// check if otp.bin is present
	const auto otp_file = ActiveSettings::GetUserDataPath("otp.bin");
	if(!fs::exists(otp_file, ec))
		return IOS_CRYPTO_ONLINE_REQ_OTP_MISSING;
	if(fs::file_size(otp_file, ec) != 1024)
		return IOS_CRYPTO_ONLINE_REQ_OTP_CORRUPTED;
	// check if seeprom.bin is present
	const auto seeprom_file = ActiveSettings::GetUserDataPath("seeprom.bin");
	if (!fs::exists(seeprom_file, ec))
		return IOS_CRYPTO_ONLINE_REQ_SEEPROM_MISSING;
	if (fs::file_size(seeprom_file, ec) != 512)
		return IOS_CRYPTO_ONLINE_REQ_SEEPROM_CORRUPTED;
	
	for (const auto& c : g_certificates)
	{
		std::string subPath = fmt::format("sys/title/0005001b/10054000/content/{}", boost::nowide::narrow(c.name));
		if (iosuCrypto_checkRequirementMLCFile(subPath, additionalErrorInfo) == false)
		{
			cemuLog_log(LogType::Force, "Missing dumped file for online mode: {}", subPath);
			return IOS_CRYPTO_ONLINE_REQ_MISSING_FILE;
		}

		if (*c.key)
		{
			std::string subPath = fmt::format("sys/title/0005001b/10054000/content/{}", boost::nowide::narrow(c.key));
			if (iosuCrypto_checkRequirementMLCFile(subPath, additionalErrorInfo) == false)
			{
				cemuLog_log(LogType::Force, "Missing dumped file for online mode: {}", subPath);
				return IOS_CRYPTO_ONLINE_REQ_MISSING_FILE;
			}
		}
	}
	return IOS_CRYPTO_ONLINE_REQ_OK;
}

std::vector<const wchar_t*> iosuCrypt_getCertificateKeys()
{
	std::vector<const wchar_t*> result;
	result.reserve(std::size(g_certificates));
	for (const auto& c : g_certificates)
	{
		if (c.key[0] == '\0')
			continue;
		
		result.emplace_back(c.key);
	}
	return result;
}

std::vector<const wchar_t*> iosuCrypt_getCertificateNames()
{
	std::vector<const wchar_t*> result;
	result.reserve(std::size(g_certificates));
	for (const auto& c : g_certificates)
	{
		result.emplace_back(c.name);
	}
	return result;
}
