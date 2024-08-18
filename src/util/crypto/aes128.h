#ifndef _AES_H_
#define _AES_H_

void AES128_init();

extern void(*AES128_ECB_encrypt)(uint8* input, const uint8* key, uint8* output);

void AES128_ECB_decrypt(uint8* input, const uint8* key, uint8 *output);

void AES128_CBC_encrypt(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv);

extern void(*AES128_CBC_decrypt)(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv);

void AES128_CBC_decrypt_updateIV(uint8* output, uint8* input, uint32 length, const uint8* key, uint8* iv);

void AES128CTR_transform(uint8* data, sint32 length, uint8* key, uint8* nonceIv);

#endif //_AES_H_
