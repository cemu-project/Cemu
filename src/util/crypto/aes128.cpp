/*
	Original implementation based on Tiny-AES-c (2015)
	https://github.com/kokke/tiny-AES-c

	Modified by Exzap
*/


/*****************************************************************************/
/* Includes:                                                                 */
/*****************************************************************************/
#include "aes128.h"
#include "Common/cpu_features.h"

/*****************************************************************************/
/* Defines:                                                                  */
/*****************************************************************************/
// The number of columns comprising a state in AES. This is a constant in AES. Value=4
#define Nb 4
// The number of 32 bit words in a key.
#define Nk 4
// Key length in bytes [128 bit]
#define KEYLEN 16
// The number of rounds in AES Cipher.
#define Nr 10

typedef uint8 state_t[4][4];

typedef struct
{
	state_t* state;
	uint8 RoundKey[176];
}aes128Ctx_t;

#define stateVal(__x, __y) ((*aesCtx->state)[__x][__y])
#define stateValU32(__x) (*(uint32*)((*aesCtx->state)[__x]))

// The lookup-tables are marked const so they can be placed in read-only storage instead of RAM
// The numbers below can be computed dynamically trading ROM for RAM - 
// This can be useful in (embedded) bootloader applications, where ROM is often limited.
static const uint8 sbox[256] = {
	//0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

static const uint8 rsbox[256] =
{ 0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };


// The round constant word array, Rcon[i], contains the values given by 
// x to th e power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
// Note that i starts at 1, not 0).
static const uint8 Rcon[255] = {
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a,
  0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39,
  0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a,
  0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8,
  0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef,
  0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc,
  0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b,
  0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3,
  0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94,
  0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20,
  0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35,
  0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f,
  0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04,
  0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63,
  0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd,
  0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb };


/*****************************************************************************/
/* Private functions:                                                        */
/*****************************************************************************/
uint8 getSBoxValue(uint8 num)
{
	return sbox[num];
}

uint8 getSBoxInvert(uint8 num)
{
	return rsbox[num];
}

// This function produces Nb(Nr+1) round keys. The round keys are used in each round to decrypt the states. 
void KeyExpansion(aes128Ctx_t* aesCtx, const uint8* key)
{
	uint32 i, j, k;
	uint8 tempa[4]; // Used for the column/row operations

	// The first round key is the key itself.
	for (i = 0; i < Nk; ++i)
	{
		aesCtx->RoundKey[(i * 4) + 0] = key[(i * 4) + 0];
		aesCtx->RoundKey[(i * 4) + 1] = key[(i * 4) + 1];
		aesCtx->RoundKey[(i * 4) + 2] = key[(i * 4) + 2];
		aesCtx->RoundKey[(i * 4) + 3] = key[(i * 4) + 3];
	}

	// All other round keys are found from the previous round keys.
	for (; (i < (Nb * (Nr + 1))); ++i)
	{
		for (j = 0; j < 4; ++j)
		{
			tempa[j] = aesCtx->RoundKey[(i - 1) * 4 + j];
		}
		if (i % Nk == 0)
		{
			// This function rotates the 4 bytes in a word to the left once.
			// [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

			// Function RotWord()
			{
				k = tempa[0];
				tempa[0] = tempa[1];
				tempa[1] = tempa[2];
				tempa[2] = tempa[3];
				tempa[3] = k;
			}

			// SubWord() is a function that takes a four-byte input word and 
			// applies the S-box to each of the four bytes to produce an output word.

			// Function Subword()
			{
				tempa[0] = getSBoxValue(tempa[0]);
				tempa[1] = getSBoxValue(tempa[1]);
				tempa[2] = getSBoxValue(tempa[2]);
				tempa[3] = getSBoxValue(tempa[3]);
			}

			tempa[0] = tempa[0] ^ Rcon[i / Nk];
		}
		else if (Nk > 6 && i % Nk == 4)
		{
			// Function Subword()
			{
				tempa[0] = getSBoxValue(tempa[0]);
				tempa[1] = getSBoxValue(tempa[1]);
				tempa[2] = getSBoxValue(tempa[2]);
				tempa[3] = getSBoxValue(tempa[3]);
			}
		}
		aesCtx->RoundKey[i * 4 + 0] = aesCtx->RoundKey[(i - Nk) * 4 + 0] ^ tempa[0];
		aesCtx->RoundKey[i * 4 + 1] = aesCtx->RoundKey[(i - Nk) * 4 + 1] ^ tempa[1];
		aesCtx->RoundKey[i * 4 + 2] = aesCtx->RoundKey[(i - Nk) * 4 + 2] ^ tempa[2];
		aesCtx->RoundKey[i * 4 + 3] = aesCtx->RoundKey[(i - Nk) * 4 + 3] ^ tempa[3];
	}
}

// This function adds the round key to state.
// The round key is added to the state by an XOR function.
void AddRoundKey(aes128Ctx_t* aesCtx, uint8 round)
{
	// note: replacing this with two 64bit xor operations decreased performance in benchmarks, probably because the state bytes need to be stored back in memory

	stateVal(0, 0) ^= aesCtx->RoundKey[round * Nb * 4 + 0 * Nb + 0];
	stateVal(0, 1) ^= aesCtx->RoundKey[round * Nb * 4 + 0 * Nb + 1];
	stateVal(0, 2) ^= aesCtx->RoundKey[round * Nb * 4 + 0 * Nb + 2];
	stateVal(0, 3) ^= aesCtx->RoundKey[round * Nb * 4 + 0 * Nb + 3];

	stateVal(1, 0) ^= aesCtx->RoundKey[round * Nb * 4 + 1 * Nb + 0];
	stateVal(1, 1) ^= aesCtx->RoundKey[round * Nb * 4 + 1 * Nb + 1];
	stateVal(1, 2) ^= aesCtx->RoundKey[round * Nb * 4 + 1 * Nb + 2];
	stateVal(1, 3) ^= aesCtx->RoundKey[round * Nb * 4 + 1 * Nb + 3];

	stateVal(2, 0) ^= aesCtx->RoundKey[round * Nb * 4 + 2 * Nb + 0];
	stateVal(2, 1) ^= aesCtx->RoundKey[round * Nb * 4 + 2 * Nb + 1];
	stateVal(2, 2) ^= aesCtx->RoundKey[round * Nb * 4 + 2 * Nb + 2];
	stateVal(2, 3) ^= aesCtx->RoundKey[round * Nb * 4 + 2 * Nb + 3];

	stateVal(3, 0) ^= aesCtx->RoundKey[round * Nb * 4 + 3 * Nb + 0];
	stateVal(3, 1) ^= aesCtx->RoundKey[round * Nb * 4 + 3 * Nb + 1];
	stateVal(3, 2) ^= aesCtx->RoundKey[round * Nb * 4 + 3 * Nb + 2];
	stateVal(3, 3) ^= aesCtx->RoundKey[round * Nb * 4 + 3 * Nb + 3];
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void SubBytes(aes128Ctx_t* aesCtx)
{
	uint8 i, j;
	for (i = 0; i < 4; ++i)
	{
		for (j = 0; j < 4; ++j)
		{
			stateVal(j, i) = getSBoxValue(stateVal(j, i));
		}
	}
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
void ShiftRows(aes128Ctx_t* aesCtx)
{
	uint8 temp;

	// Rotate first row 1 columns to left  
	temp = stateVal(0, 1);
	stateVal(0, 1) = stateVal(1, 1);
	stateVal(1, 1) = stateVal(2, 1);
	stateVal(2, 1) = stateVal(3, 1);
	stateVal(3, 1) = temp;

	// Rotate second row 2 columns to left  
	temp = stateVal(0, 2);
	stateVal(0, 2) = stateVal(2, 2);
	stateVal(2, 2) = temp;

	temp = stateVal(1, 2);
	stateVal(1, 2) = stateVal(3, 2);
	stateVal(3, 2) = temp;

	// Rotate third row 3 columns to left
	temp = stateVal(0, 3);
	stateVal(0, 3) = stateVal(3, 3);
	stateVal(3, 3) = stateVal(2, 3);
	stateVal(2, 3) = stateVal(1, 3);
	stateVal(1, 3) = temp;
}

uint8 aes_xtime(uint8 x)
{
	return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

// MixColumns function mixes the columns of the state matrix
void MixColumns(aes128Ctx_t* aesCtx)
{
	uint8 i;
	uint8 Tmp, Tm, t;
	for (i = 0; i < 4; ++i)
	{
		t = stateVal(i, 0);
		Tmp = stateVal(i, 0) ^ stateVal(i, 1) ^ stateVal(i, 2) ^ stateVal(i, 3);
		Tm = stateVal(i, 0) ^ stateVal(i, 1); Tm = aes_xtime(Tm);  stateVal(i, 0) ^= Tm ^ Tmp;
		Tm = stateVal(i, 1) ^ stateVal(i, 2); Tm = aes_xtime(Tm);  stateVal(i, 1) ^= Tm ^ Tmp;
		Tm = stateVal(i, 2) ^ stateVal(i, 3); Tm = aes_xtime(Tm);  stateVal(i, 2) ^= Tm ^ Tmp;
		Tm = stateVal(i, 3) ^ t;             Tm = aes_xtime(Tm);  stateVal(i, 3) ^= Tm ^ Tmp;
	}
}

// Multiply is used to multiply numbers in the field GF(2^8)
#define Multiply(x, y)                                \
      (  ((y & 1) * x) ^                              \
      ((y>>1 & 1) * aes_xtime(x)) ^                       \
      ((y>>2 & 1) * aes_xtime(aes_xtime(x))) ^                \
      ((y>>3 & 1) * aes_xtime(aes_xtime(aes_xtime(x)))) ^         \
      ((y>>4 & 1) * aes_xtime(aes_xtime(aes_xtime(aes_xtime(x))))))

uint32 lookupTable_multiply[256];

//// MixColumns function mixes the columns of the state matrix.
//// The method used to multiply may be difficult to understand for the inexperienced.
//// Please use the references to gain more information.
//void InvMixColumns(aes128Ctx_t* aesCtx)
//{
//	int i;
//	uint8 a, b, c, d;
//	for (i = 0; i < 4; ++i)
//	{
//		a = stateVal(i, 0);
//		b = stateVal(i, 1);
//		c = stateVal(i, 2);
//		d = stateVal(i, 3);
//
//		uint32 _a = lookupTable_multiply[a];
//		uint32 _b = lookupTable_multiply[b];
//		uint32 _c = lookupTable_multiply[c];
//		uint32 _d = lookupTable_multiply[d];
//
//
//		//stateVal(i, 0) = entryA->vE ^ entryB->vB ^ entryC->vD ^ entryD->v9;
//		//stateVal(i, 1) = entryA->v9 ^ entryB->vE ^ entryC->vB ^ entryD->vD;
//		//stateVal(i, 2) = entryA->vD ^ entryB->v9 ^ entryC->vE ^ entryD->vB;
//		//stateVal(i, 3) = entryA->vB ^ entryB->vD ^ entryC->v9 ^ entryD->vE;
//
//		//stateVal(i, 0) = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
//		//stateVal(i, 1) = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
//		//stateVal(i, 2) = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
//		//stateVal(i, 3) = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
//
//		//stateVal(i, 0) = Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
//		//stateVal(i, 1) = Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
//		//stateVal(i, 2) = Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
//		//stateVal(i, 3) = Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
//
//		stateValU32(i) = _a ^ _rotl(_b, 8) ^ _rotl(_c, 16) ^ _rotl(_d, 24);
//
//	}
//}

// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
void InvMixColumns(aes128Ctx_t* aesCtx)
{
	uint8 a, b, c, d;
	// i0
	a = stateVal(0, 0);
	b = stateVal(0, 1);
	c = stateVal(0, 2);
	d = stateVal(0, 3);
	stateValU32(0) = lookupTable_multiply[a] ^ std::rotl<uint32>(lookupTable_multiply[b], 8) ^ std::rotl<uint32>(lookupTable_multiply[c], 16) ^ std::rotl<uint32>(lookupTable_multiply[d], 24);
	// i1
	a = stateVal(1, 0);
	b = stateVal(1, 1);
	c = stateVal(1, 2);
	d = stateVal(1, 3);
	stateValU32(1) = lookupTable_multiply[a] ^ std::rotl<uint32>(lookupTable_multiply[b], 8) ^ std::rotl<uint32>(lookupTable_multiply[c], 16) ^ std::rotl<uint32>(lookupTable_multiply[d], 24);
	// i2
	a = stateVal(2, 0);
	b = stateVal(2, 1);
	c = stateVal(2, 2);
	d = stateVal(2, 3);
	stateValU32(2) = lookupTable_multiply[a] ^ std::rotl<uint32>(lookupTable_multiply[b], 8) ^ std::rotl<uint32>(lookupTable_multiply[c], 16) ^ std::rotl<uint32>(lookupTable_multiply[d], 24);
	// i3
	a = stateVal(3, 0);
	b = stateVal(3, 1);
	c = stateVal(3, 2);
	d = stateVal(3, 3);
	stateValU32(3) = lookupTable_multiply[a] ^ std::rotl<uint32>(lookupTable_multiply[b], 8) ^ std::rotl<uint32>(lookupTable_multiply[c], 16) ^ std::rotl<uint32>(lookupTable_multiply[d], 24);
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void InvSubBytes(aes128Ctx_t* aesCtx)
{
	stateVal(0, 0) = rsbox[stateVal(0, 0)];
	stateVal(1, 0) = rsbox[stateVal(1, 0)];
	stateVal(2, 0) = rsbox[stateVal(2, 0)];
	stateVal(3, 0) = rsbox[stateVal(3, 0)];

	stateVal(0, 1) = rsbox[stateVal(0, 1)];
	stateVal(1, 1) = rsbox[stateVal(1, 1)];
	stateVal(2, 1) = rsbox[stateVal(2, 1)];
	stateVal(3, 1) = rsbox[stateVal(3, 1)];

	stateVal(0, 2) = rsbox[stateVal(0, 2)];
	stateVal(1, 2) = rsbox[stateVal(1, 2)];
	stateVal(2, 2) = rsbox[stateVal(2, 2)];
	stateVal(3, 2) = rsbox[stateVal(3, 2)];

	stateVal(0, 3) = rsbox[stateVal(0, 3)];
	stateVal(1, 3) = rsbox[stateVal(1, 3)];
	stateVal(2, 3) = rsbox[stateVal(2, 3)];
	stateVal(3, 3) = rsbox[stateVal(3, 3)];
}

void InvShiftRows(aes128Ctx_t* aesCtx)
{
	uint8 temp;

	// Rotate first row 1 columns to right  
	temp = stateVal(3, 1);
	stateVal(3, 1) = stateVal(2, 1);
	stateVal(2, 1) = stateVal(1, 1);
	stateVal(1, 1) = stateVal(0, 1);
	stateVal(0, 1) = temp;

	// Rotate second row 2 columns to right 
	temp = stateVal(0, 2);
	stateVal(0, 2) = stateVal(2, 2);
	stateVal(2, 2) = temp;

	temp = stateVal(1, 2);
	stateVal(1, 2) = stateVal(3, 2);
	stateVal(3, 2) = temp;

	// Rotate third row 3 columns to right
	temp = stateVal(0, 3);
	stateVal(0, 3) = stateVal(1, 3);
	stateVal(1, 3) = stateVal(2, 3);
	stateVal(2, 3) = stateVal(3, 3);
	stateVal(3, 3) = temp;
}


// Cipher is the main function that encrypts the PlainText.
void Cipher(aes128Ctx_t* aesCtx)
{
	uint8 round = 0;

	// Add the First round key to the state before starting the rounds.
	AddRoundKey(aesCtx, 0);

	// There will be Nr rounds.
	// The first Nr-1 rounds are identical.
	// These Nr-1 rounds are executed in the loop below.
	for (round = 1; round < Nr; ++round)
	{
		SubBytes(aesCtx);
		ShiftRows(aesCtx);
		MixColumns(aesCtx);
		AddRoundKey(aesCtx, round);
	}

	// The last round is given below.
	// The MixColumns function is not here in the last round.
	SubBytes(aesCtx);
	ShiftRows(aesCtx);
	AddRoundKey(aesCtx, Nr);
}

void InvCipher(aes128Ctx_t* aesCtx)
{
	uint8 round = 0;

	// Add the First round key to the state before starting the rounds.
	AddRoundKey(aesCtx, Nr);

	// There will be Nr rounds.
	// The first Nr-1 rounds are identical.
	// These Nr-1 rounds are executed in the loop below.
	for (round = Nr - 1; round > 0; round--)
	{
		InvShiftRows(aesCtx);
		InvSubBytes(aesCtx);
		AddRoundKey(aesCtx, round);
		InvMixColumns(aesCtx);
	}

	// The last round is given below.
	// The MixColumns function is not here in the last round.
	InvShiftRows(aesCtx);
	InvSubBytes(aesCtx);
	AddRoundKey(aesCtx, 0);
}

static void BlockCopy(uint8* output, uint8* input)
{
	uint8 i;
	for (i = 0; i < KEYLEN; ++i)
	{
		output[i] = input[i];
	}
}



/*****************************************************************************/
/* Public functions:                                                         */
/*****************************************************************************/

void __soft__AES128_ECB_encrypt(uint8* input, const uint8* key, uint8* output)
{
	aes128Ctx_t aesCtx;
	// Copy input to output, and work in-memory on output
	BlockCopy(output, input);
	aesCtx.state = (state_t*)output;

	KeyExpansion(&aesCtx, key);

	// The next function call encrypts the PlainText with the Key using AES algorithm.
	Cipher(&aesCtx);
}

void AES128_ECB_decrypt(uint8* input, const uint8* key, uint8 *output)
{
	aes128Ctx_t aesCtx;
	// Copy input to output, and work in-memory on output
	BlockCopy(output, input);
	aesCtx.state = (state_t*)output;
	
	// The KeyExpansion routine must be called before encryption.
	KeyExpansion(&aesCtx, key);

	InvCipher(&aesCtx);
}

void XorWithIv(uint8* buf, const uint8* iv)
{
	uint8 i;
	for (i = 0; i < KEYLEN; ++i)
	{
		buf[i] ^= iv[i];
	}
}

void AES128_CBC_encrypt(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv)
{
	aes128Ctx_t aesCtx;
	intptr_t i;
	uint8 remainders = length % KEYLEN; /* Remaining bytes in the last non-full block */

	BlockCopy(output, input);
	aesCtx.state = (state_t*)output;

	KeyExpansion(&aesCtx, key);

	const uint8* currentIv = iv;

	for (i = 0; i < length; i += KEYLEN)
	{
		XorWithIv(input, currentIv);
		BlockCopy(output, input);
		aesCtx.state = (state_t*)output;
		Cipher(&aesCtx);
		currentIv = output;
		input += KEYLEN;
		output += KEYLEN;
	}
	cemu_assert_debug(remainders == 0);
}

void __soft__AES128_CBC_decrypt(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv)
{
	aes128Ctx_t aesCtx;
	intptr_t i;
	uint8 remainders = length % KEYLEN;

	KeyExpansion(&aesCtx, key);

	uint8 currentIv[KEYLEN];
	uint8 nextIv[KEYLEN];
	if (iv)
		BlockCopy(currentIv, (uint8*)iv);
	else
		memset(currentIv, 0, sizeof(currentIv));

	for (i = 0; i < length; i += KEYLEN)
	{
		aesCtx.state = (state_t*)output;
		BlockCopy(output, input);
		BlockCopy(nextIv, input);
		InvCipher(&aesCtx);
		XorWithIv(output, currentIv);
		BlockCopy(currentIv, nextIv);
		output += KEYLEN;
		input += KEYLEN;
	}

	cemu_assert_debug(remainders == 0);
}

void AES128_CBC_decrypt_buffer_depr(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv)
{
	aes128Ctx_t aesCtx;
	intptr_t i;
	uint8 remainders = length % KEYLEN; /* Remaining bytes in the last non-full block */

	BlockCopy(output, input);

	KeyExpansion(&aesCtx, key);

	const uint8* currentIv = iv;

	for (i = 0; i < length; i += KEYLEN)
	{
		BlockCopy(output, input);
		aesCtx.state = (state_t*)output;
		InvCipher(&aesCtx);
		XorWithIv(output, currentIv);
		currentIv = input;
		input += KEYLEN;
		output += KEYLEN;
	}
	cemu_assert_debug(remainders == 0);
}

void AES128_CBC_decrypt_updateIV(uint8* output, uint8* input, uint32 length, const uint8* key, uint8* iv)
{
	length &= ~0xF;
	uint8 newIv[16];
	if (length == 0)
		return;
	cemu_assert_debug((length&0xF) == 0);
	memcpy(newIv, input + (length - 16), KEYLEN);
	AES128_CBC_decrypt(output, input, length, key, iv);
	memcpy(iv, newIv, KEYLEN);
}

#if defined(ARCH_X86_64)
ATTRIBUTE_AESNI inline __m128i AESNI128_ASSIST(
	__m128i temp1,
	__m128i temp2)
{
	__m128i temp3;
	temp2 = _mm_shuffle_epi32(temp2, 0xff);
	temp3 = _mm_slli_si128(temp1, 0x4);
	temp1 = _mm_xor_si128(temp1, temp3);
	temp3 =
		_mm_slli_si128(temp3, 0x4);
	temp1 =
		_mm_xor_si128(temp1, temp3);
	temp3 =
		_mm_slli_si128(temp3, 0x4);
	temp1 =
		_mm_xor_si128(temp1, temp3);
	temp1 = _mm_xor_si128(temp1, temp2);
	return temp1;
}

ATTRIBUTE_AESNI void AESNI128_KeyExpansionEncrypt(const unsigned char *userkey, unsigned char *key)
{
	__m128i temp1, temp2;
	__m128i *Key_Schedule = (__m128i*)key;
	temp1 = _mm_loadu_si128((__m128i*)userkey);
	Key_Schedule[0] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x1);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[1] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x2);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[2] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x4);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[3] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x8);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[4] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x10);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[5] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x20);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[6] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x40);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[7] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x80);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[8] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x1b);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[9] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x36);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[10] = temp1;
}

ATTRIBUTE_AESNI void AESNI128_KeyExpansionDecrypt(const unsigned char *userkey, unsigned char *key)
{
	__m128i temp1, temp2;
	__m128i *Key_Schedule = (__m128i*)key;
	temp1 = _mm_loadu_si128((__m128i*)userkey);
	Key_Schedule[0] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x1);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[1] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x2);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[2] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x4);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[3] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x8);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[4] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x10);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[5] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x20);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[6] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x40);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[7] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x80);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[8] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x1b);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[9] = temp1;
	temp2 = _mm_aeskeygenassist_si128(temp1, 0x36);
	temp1 = AESNI128_ASSIST(temp1, temp2);
	Key_Schedule[10] = temp1;
	// inverse
	for (sint32 i = 1; i < 10; i++)
	{
		Key_Schedule[i] = _mm_aesimc_si128(Key_Schedule[i]);
	}
}

ATTRIBUTE_AESNI void AESNI128_CBC_encrypt(const unsigned char *in,
	unsigned char *out,
	unsigned char ivec[16],
	unsigned long length,
	unsigned char *key,
	int number_of_rounds)
{
	__m128i feedback, data;
	int j;
	if (length % 16)
		length = length / 16 + 1;
	else length /= 16;
	feedback = _mm_loadu_si128((__m128i*)ivec);
	for (unsigned long i = 0; i < length; i++) 
	{
		data =
			_mm_loadu_si128(&((__m128i*)in)[i]);
		feedback = _mm_xor_si128(data, feedback);
		feedback = _mm_xor_si128(feedback, ((__m128i*)key)[0]);
		for (j = 1; j < number_of_rounds; j++)
			feedback =
			_mm_aesenc_si128(feedback, ((__m128i*)key)[j]);
		feedback =
			_mm_aesenclast_si128(feedback, ((__m128i*)key)[j]);
		_mm_storeu_si128(&((__m128i*)out)[i], feedback);
	}
}

ATTRIBUTE_AESNI void AESNI128_CBC_decryptWithExpandedKey(const unsigned char *in,
	unsigned char *out,
	const unsigned char ivec[16],
	unsigned long length,
	unsigned char *key)
{
	__m128i data, feedback, lastin;
	int j;
	if (length % 16)
		length = length / 16 + 1;
	else length /= 16;
	feedback = _mm_loadu_si128((__m128i*)ivec);
	for (unsigned long i = 0; i < length; i++)
	{
		lastin = _mm_loadu_si128(&((__m128i*)in)[i]);
		data = _mm_xor_si128(lastin, ((__m128i*)key)[10]);
		for (j = 9; j > 0; j--)
		{
			data = _mm_aesdec_si128(data, ((__m128i*)key)[j]);
		}
		data = _mm_aesdeclast_si128(data, ((__m128i*)key)[0]);
		data = _mm_xor_si128(data, feedback);
		_mm_storeu_si128(&((__m128i*)out)[i], data);
		feedback = lastin;
	}
}

ATTRIBUTE_AESNI void __aesni__AES128_CBC_decrypt(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv)
{
	alignas(16) uint8 expandedKey[11 * 16];
	AESNI128_KeyExpansionDecrypt(key, expandedKey);
	if (iv)
	{
		AESNI128_CBC_decryptWithExpandedKey(input, output, iv, length, expandedKey);
	}
	else
	{
		uint8 zeroIv[16] = { 0 };
		AESNI128_CBC_decryptWithExpandedKey(input, output, zeroIv, length, expandedKey);
	}
}

ATTRIBUTE_AESNI void __aesni__AES128_ECB_encrypt(uint8* input, const uint8* key, uint8* output)
{
	alignas(16) uint8 expandedKey[11 * 16];
	AESNI128_KeyExpansionEncrypt(key, expandedKey);
	// encrypt single ECB block
	__m128i feedback;
	feedback = _mm_loadu_si128(&((__m128i*)input)[0]);
	feedback = _mm_xor_si128(feedback, ((__m128i*)expandedKey)[0]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[1]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[2]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[3]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[4]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[5]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[6]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[7]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[8]);
	feedback = _mm_aesenc_si128(feedback, ((__m128i*)expandedKey)[9]);
	feedback = _mm_aesenclast_si128(feedback, ((__m128i*)expandedKey)[10]);
	_mm_storeu_si128(&((__m128i*)output)[0], feedback);
}
#endif

void(*AES128_ECB_encrypt)(uint8* input, const uint8* key, uint8* output);
void (*AES128_CBC_decrypt)(uint8* output, uint8* input, uint32 length, const uint8* key, const uint8* iv) = nullptr;

// AES128-CTR encrypt/decrypt
void AES128CTR_transform(uint8* data, sint32 length, uint8* key, uint8* nonceIv)
{
	for (sint32 i = 0; i < length; i += 16)
	{
		uint8* d = data + i;
		uint8 tempArray[16];
		AES128_ECB_encrypt(nonceIv, key, tempArray);
		for (sint32 f = 0; f < 16; f++)
		{
			d[f] ^= tempArray[f];
		}
		// increase nonce
		*(uint32*)(nonceIv + 0xC) = _swapEndianU32(_swapEndianU32(*(uint32*)(nonceIv + 0xC)) + 1);
		if (*(uint32*)(nonceIv + 0xC) == 0)
		{
			*(uint32*)(nonceIv + 0x8) = _swapEndianU32(_swapEndianU32(*(uint32*)(nonceIv + 0x8)) + 1);
			if (*(uint32*)(nonceIv + 0x8) == 0)
			{
				*(uint32*)(nonceIv + 0x4) = _swapEndianU32(_swapEndianU32(*(uint32*)(nonceIv + 0x4)) + 1);
				if (*(uint32*)(nonceIv + 0x4) == 0)
				{
					*(uint32*)(nonceIv + 0) = _swapEndianU32(_swapEndianU32(*(uint32*)(nonceIv + 0)) + 1);
				}
			}
		}
	}
}

void AES128_init()
{
	for (uint32 i = 0; i <= 0xFF; i++)
	{
		uint32 vE = Multiply((uint8)(i & 0xFF), 0x0E) & 0xFF;
		uint32 v9 = Multiply((uint8)(i & 0xFF), 0x09) & 0xFF;
		uint32 vD = Multiply((uint8)(i & 0xFF), 0x0D) & 0xFF;
		uint32 vB = Multiply((uint8)(i & 0xFF), 0x0B) & 0xFF;
		lookupTable_multiply[i] = (vE << 0) | (v9 << 8) | (vD << 16) | (vB << 24);
	}
	// check if AES-NI is available
	#if defined(ARCH_X86_64)
	if (g_CPUFeatures.x86.aesni)
	{
		// AES-NI implementation
		AES128_CBC_decrypt = __aesni__AES128_CBC_decrypt;
		AES128_ECB_encrypt = __aesni__AES128_ECB_encrypt;
	}
	else
	{
		// basic software implementation
		AES128_CBC_decrypt = __soft__AES128_CBC_decrypt;
		AES128_ECB_encrypt = __soft__AES128_ECB_encrypt;
	}
    #else
	AES128_CBC_decrypt = __soft__AES128_CBC_decrypt;
	AES128_ECB_encrypt = __soft__AES128_ECB_encrypt;
    #endif
}
