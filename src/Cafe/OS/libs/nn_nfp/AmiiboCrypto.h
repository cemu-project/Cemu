#pragma once
#include "util/crypto/aes128.h"
#include "openssl/evp.h"
#include "openssl/hmac.h"

typedef struct
{
	char typeString[14];
	uint8 magicBytes[16];
	uint8 magicBytesLen;
	uint8 xorPad[32];
	uint8 hmacKey[16];
}amiiboInternalKeys_t;

void amiiboCalcSeed(AmiiboInternal* internalData, uint8* seed)
{
	memcpy(seed + 0x00, (uint8*)internalData + 0x029, 0x02);
	memset(seed + 0x02, 0x00, 0x0E);
	memcpy(seed + 0x10, (uint8*)internalData + 0x1D4, 0x08);
	memcpy(seed + 0x18, (uint8*)internalData + 0x1D4, 0x08);
	memcpy(seed + 0x20, (uint8*)internalData + 0x1E8, 0x20);
}

void amiiboGenKeyInternalPrepare(amiiboInternalKeys_t* keys, uint8* seed, uint8* output, sint32& outputLen)
{
	sint32 index = 0;

	// concat:
	// typeString (including '\0')
	sint32 typeStringLen = (sint32)strlen(keys->typeString)+1;
	memcpy(output + index, keys->typeString, typeStringLen);
	index += typeStringLen;
	// seed (16 - magic byte len)
	sint32 seedPart1Len = 16 - keys->magicBytesLen;
	memcpy(output + index, seed, seedPart1Len);
	index += seedPart1Len;
	// magic bytes + 16 bytes at +0x10 from input seed
	memcpy(output + index, keys->magicBytes, keys->magicBytesLen);
	index += keys->magicBytesLen;
	// seed 16 bytes at +0x10
	memcpy(output + index, seed + 0x10, 16);
	index += 16;
	// 32 bytes at +0x20 from input seed xored with xor pad
	for (sint32 i = 0; i < 32; i++)
		output[index + i] = seed[i + 32] ^ keys->xorPad[i];
	index += 32;

	outputLen = index;
}

typedef struct 
{
	uint8 hmacKey[16];
	uint8 buffer[sizeof(uint16) + 480];
	sint32 bufferSize;
	uint16 counter;
}amiiboCryptCtx_t;

typedef struct
{
	uint8 raw[32];
}drgbOutput32_t;

void amiiboCryptInit(amiiboCryptCtx_t* ctx, const uint8* hmacKey, size_t hmacKeySize, const uint8* seed, sint32 seedSize)
{
	ctx->counter = 0;
	ctx->bufferSize = (sint32)sizeof(ctx->counter) + seedSize;
	memcpy(ctx->hmacKey, hmacKey, 16);

	// set static part of buffer
	memcpy(ctx->buffer + sizeof(uint16), seed, seedSize);
}

void amiiboCryptStep(amiiboCryptCtx_t* ctx, drgbOutput32_t* output)
{
	// update counter
	ctx->buffer[0] = ctx->counter >> 8;
	ctx->buffer[1] = ctx->counter >> 0;
	ctx->counter++;
	// generate bytes
	uint32 mdLen = (uint32)sizeof(drgbOutput32_t);
	HMAC(EVP_sha256(), ctx->hmacKey, sizeof(ctx->hmacKey), (const unsigned char *)ctx->buffer, ctx->bufferSize, output->raw, &mdLen);
}

typedef struct
{
	uint8 aesKey[16];
	uint8 aesIV[16];
	uint8 hmacKey[16];
}amiiboDerivedKeys_t;

static_assert(sizeof(amiiboDerivedKeys_t) == 0x30);

void amiiboCrypto_genKey(amiiboInternalKeys_t* keys, AmiiboInternal* internalData, amiiboDerivedKeys_t* derivedKeys)
{
	// generate seed
	uint8 seed[0x40];
	amiiboCalcSeed(internalData, seed);

	// generate internal seed
	uint8 internalKey[512];
	sint32 internalKeyLen = 0;
	amiiboGenKeyInternalPrepare(keys, seed, internalKey, internalKeyLen);

	// gen bytes for derived keys
	drgbOutput32_t temp[2];
	amiiboCryptCtx_t rngCtx;
	amiiboCryptInit(&rngCtx, keys->hmacKey, sizeof(keys->hmacKey), internalKey, internalKeyLen);
	amiiboCryptStep(&rngCtx, temp + 0);
	amiiboCryptStep(&rngCtx, temp + 1);
	memcpy(derivedKeys, temp, sizeof(amiiboDerivedKeys_t));
}

void amiiboCrypto_nfcFormatToInternal(AmiiboRawNFCData* nfcData, AmiiboInternal* internalData)
{
	uint8* tag = (uint8*)nfcData;
	uint8* intl = (uint8*)internalData;

	memcpy(intl + 0x000, tag + 0x008, 0x008);
	memcpy(intl + 0x008, tag + 0x080, 0x020); // tag hmac
	memcpy(intl + 0x028, tag + 0x010, 0x024);
	memcpy(intl + 0x04C, tag + 0x0A0, 0x168);
	memcpy(intl + 0x1B4, tag + 0x034, 0x020); // data hmac
	memcpy(intl + 0x1D4, tag + 0x000, 0x008);
	memcpy(intl + 0x1DC, tag + 0x054, 0x02C);
}

void amiiboCrypto_internalToNfcFormat(AmiiboInternal* internalData, AmiiboRawNFCData* nfcData)
{
	uint8* tag = (uint8*)nfcData;
	uint8* intl = (uint8*)internalData;

	memcpy(tag + 0x008, intl + 0x000, 0x008);
	memcpy(tag + 0x080, intl + 0x008, 0x020); // tag hmac
	memcpy(tag + 0x010, intl + 0x028, 0x024);
	memcpy(tag + 0x0A0, intl + 0x04C, 0x168);
	memcpy(tag + 0x034, intl + 0x1B4, 0x020); // data hmac
	memcpy(tag + 0x000, intl + 0x1D4, 0x008);
	memcpy(tag + 0x054, intl + 0x1DC, 0x02C);
}

void amiiboCrypto_transform(const amiiboDerivedKeys_t * keys, uint8 * in, uint8 * out)
{
	uint8 nonce[16];

	memcpy(nonce, keys->aesIV, sizeof(nonce));
	
	AmiiboInternal internalCopy;
	memcpy(&internalCopy, in, sizeof(internalCopy));

	if (out != in)
		memcpy(out, in, 0x188 + 0x2C);
	AES128CTR_transform(out + 0x2C, 0x188, (uint8*)keys->aesKey, nonce);

	memcpy(out + 0x000, ((uint8*)&internalCopy) + 0x000, 0x008);
	memcpy(out + 0x028, ((uint8*)&internalCopy) + 0x028, 0x004);
	memcpy(out + 0x1D4, ((uint8*)&internalCopy) + 0x1D4, 0x034);
}

void AES128_init();

void amiiboInitMasterKeys(amiiboInternalKeys_t& kData, amiiboInternalKeys_t& kTag)
{
	uint8 dataKey_hmacKey[16] = { 0x1D, 0x16, 0x4B, 0x37, 0x5B, 0x72, 0xA5, 0x57, 0x28, 0xB9, 0x1D, 0x64, 0xB6, 0xA3, 0xC2, 0x05 };
	uint8 dataKey_typeString[14] = { "unfixed infos" };
	uint8 dataKey_magicBytesSize = 0x0E;
	uint8 dataKey_magicBytes[14] = { 0xDB, 0x4B, 0x9E, 0x3F, 0x45, 0x27, 0x8F, 0x39, 0x7E, 0xFF, 0x9B, 0x4F, 0xB9, 0x93 };
	uint8 dataKey_xorPad[32] = { 0x04, 0x49, 0x17, 0xDC, 0x76, 0xB4, 0x96, 0x40, 0xD6, 0xF8, 0x39, 0x39, 0x96, 0x0F, 0xAE, 0xD4, 0xEF, 0x39, 0x2F, 0xAA, 0xB2, 0x14, 0x28, 0xAA, 0x21, 0xFB, 0x54, 0xE5, 0x45, 0x05, 0x47, 0x66 };

	uint8 tagKey_hmacKey[16] = { 0x7F, 0x75, 0x2D, 0x28, 0x73, 0xA2, 0x00, 0x17, 0xFE, 0xF8, 0x5C, 0x05, 0x75, 0x90, 0x4B, 0x6D };
	uint8 tagKey_typeString[14] = { "locked secret" };
	uint8 tagKey_magicBytesSize = 0x10;
	uint8 tagKey_magicBytes[16] = { 0xFD, 0xC8, 0xA0, 0x76, 0x94, 0xB8, 0x9E, 0x4C, 0x47, 0xD3, 0x7D, 0xE8, 0xCE, 0x5C, 0x74, 0xC1 };
	uint8 tagKey_xorPad[32] = { 0x04, 0x49, 0x17, 0xDC, 0x76, 0xB4, 0x96, 0x40, 0xD6, 0xF8, 0x39, 0x39, 0x96, 0x0F, 0xAE, 0xD4, 0xEF, 0x39, 0x2F, 0xAA, 0xB2, 0x14, 0x28, 0xAA, 0x21, 0xFB, 0x54, 0xE5, 0x45, 0x05, 0x47, 0x66 };

	memcpy(kData.hmacKey, dataKey_hmacKey, 16);
	memcpy(kData.typeString, dataKey_typeString, 13);
	kData.magicBytesLen = dataKey_magicBytesSize;
	memcpy(kData.magicBytes, dataKey_magicBytes, 14);
	memcpy(kData.xorPad, dataKey_xorPad, 32);

	memcpy(kTag.hmacKey, tagKey_hmacKey, 16);
	memcpy(kTag.typeString, tagKey_typeString, 13);
	kTag.magicBytesLen = tagKey_magicBytesSize;
	memcpy(kTag.magicBytes, tagKey_magicBytes, 16);
	memcpy(kTag.xorPad, tagKey_xorPad, 32);
}

void amiiboDecrypt()
{
	amiiboInternalKeys_t kData{};
	amiiboInternalKeys_t kTag{};
	amiiboInitMasterKeys(kData, kTag);

	amiiboDerivedKeys_t derivedKeysData{};
	amiiboDerivedKeys_t derivedKeysTag{};

	amiiboCrypto_nfcFormatToInternal(&nfp_data.amiiboNFCData, &nfp_data.amiiboInternal);

	amiiboCrypto_genKey(&kData, &nfp_data.amiiboInternal, &derivedKeysData);
	amiiboCrypto_genKey(&kTag, &nfp_data.amiiboInternal, &derivedKeysTag);

	amiiboCrypto_transform(&derivedKeysData, (uint8*)&nfp_data.amiiboInternal, (uint8*)&nfp_data.amiiboInternal);

	// generate tag hmac
	uint32 mdLen = 32;
	HMAC(EVP_sha256(), derivedKeysTag.hmacKey, 16, (((uint8*)&nfp_data.amiiboInternal) + 0x1D4), 0x34,
		nfp_data.amiiboInternal.tagHMAC, &mdLen);
	// generate data hmac
	mdLen = 32;
	HMAC(EVP_sha256(), derivedKeysData.hmacKey, 16, (((uint8*)&nfp_data.amiiboInternal) + 0x29), 0x1DF,
		((uint8*)&nfp_data.amiiboInternal) + 0x8, &mdLen);

	bool isValidTagHMAC = memcmp(nfp_data.amiiboNFCData.tagHMAC, nfp_data.amiiboInternal.tagHMAC, 32) == 0;
	bool isValidDataHMAC = memcmp(nfp_data.amiiboNFCData.dataHMAC, nfp_data.amiiboInternal.dataHMAC, 32) == 0;
	if (!isValidTagHMAC)
		cemuLog_log(LogType::Force, "Decrypt amiibo has invalid tag HMAC");
	if (!isValidDataHMAC)
		cemuLog_log(LogType::Force, "Decrypt amiibo has invalid data HMAC");
	nfp_data.hasInvalidHMAC = !isValidTagHMAC || !isValidDataHMAC;
}

void amiiboEncrypt(AmiiboRawNFCData* nfcOutput)
{
	amiiboInternalKeys_t kData{};
	amiiboInternalKeys_t kTag{};
	amiiboInitMasterKeys(kData, kTag);

	amiiboDerivedKeys_t derivedKeysData{};
	amiiboDerivedKeys_t derivedKeysTag{};

	// copy internal Amiibo data
	AmiiboInternal internalCopy;
	memcpy(&internalCopy, &nfp_data.amiiboInternal, sizeof(AmiiboInternal));

	// gen key
	amiiboCrypto_genKey(&kData, &internalCopy, &derivedKeysData);
	amiiboCrypto_genKey(&kTag, &internalCopy, &derivedKeysTag);

	// generate new HMACs
	uint8 tagHMAC[32];
	uint32 mdLen = 32;
	HMAC(EVP_sha256(), derivedKeysTag.hmacKey, 16, (((uint8*)&internalCopy) + 0x1D4), 0x34,
		tagHMAC, &mdLen);
	mdLen = 32;
	uint8 dataHMAC[32];
	HMAC(EVP_sha256(), derivedKeysData.hmacKey, 16, (((uint8*)&internalCopy) + 0x29), 0x1DF,
		dataHMAC, &mdLen);

	memset(internalCopy.tagHMAC, 0, 32);
	memset(internalCopy.dataHMAC, 0, 32);

	// transform
	amiiboCrypto_transform(&derivedKeysData, (uint8*)&internalCopy, (uint8*)&internalCopy);

	// set HMACs
	memcpy(internalCopy.tagHMAC, tagHMAC, 32);
	memcpy(internalCopy.dataHMAC, dataHMAC, 32);

	// convert back to nfc order
	amiiboCrypto_internalToNfcFormat(&internalCopy, nfcOutput);

	// restore NFC values that aren't part of the internal representation
	memcpy(nfcOutput->dynamicLock, nfp_data.amiiboNFCData.dynamicLock, 4);
	memcpy(nfcOutput->cfg0, nfp_data.amiiboNFCData.cfg0, 4);
	memcpy(nfcOutput->cfg1, nfp_data.amiiboNFCData.cfg1, 4);
}
