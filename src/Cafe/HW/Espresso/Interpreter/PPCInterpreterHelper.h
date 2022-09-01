
static uint32 ppc_cmp_and_mask[8] = {
	0xfffffff0,
	0xffffff0f,
	0xfffff0ff,
	0xffff0fff,
	0xfff0ffff,
	0xff0fffff,
	0xf0ffffff,
	0x0fffffff,
};


#define ppc_word_rotl(_data, _n) (std::rotl<uint32>(_data,(_n)&0x1F))

static inline uint32 ppc_mask(int MB, int ME)
{
	uint32 maskMB = 0xFFFFFFFF >> MB;
	uint32 maskME = 0xFFFFFFFF << (31-ME);
	uint32 mask2 = (MB <= ME) ? maskMB & maskME : maskMB | maskME;
	return mask2;
}

static inline bool ppc_carry_3(uint32 a, uint32 b, uint32 c)
{
	if ((a+b) < a) {
		return true;
	}
	if ((a+b+c) < c) {
		return true;
	}
	return false;
}

#define PPC_getBits(__value, __index, __bitCount) ((__value>>(31-__index))&((1<<__bitCount)-1))

const static float LD_SCALE[] = {
1.000000f, 0.500000f, 0.250000f, 0.125000f, 0.062500f, 0.031250f, 0.015625f,
0.007813f, 0.003906f, 0.001953f, 0.000977f, 0.000488f, 0.000244f, 0.000122f,
0.000061f, 0.000031f, 0.000015f, 0.000008f, 0.000004f, 0.000002f, 0.000001f,
0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f,
0.000000f, 0.000000f, 0.000000f, 0.000000f, 4294967296.000000f, 2147483648.000000f,
1073741824.000000f, 536870912.000000f, 268435456.000000f, 134217728.000000f, 67108864.000000f,
33554432.000000f, 16777216.000000f, 8388608.000000f, 4194304.000000f, 2097152.000000f, 1048576.000000f,
524288.000000f, 262144.000000f, 131072.000000f, 65536.000000f, 32768.000000f, 16384.000000f,
8192.000000f, 4096.000000f, 2048.000000f, 1024.000000f, 512.000000f, 256.000000f, 128.000000f, 64.000000f, 32.000000f,
16.000000f, 8.000000f, 4.000000f, 2.000000f };

const static float ST_SCALE[] = {
1.000000f, 2.000000f, 4.000000f, 8.000000f,
16.000000f, 32.000000f, 64.000000f, 128.000000f,
256.000000f, 512.000000f, 1024.000000f, 2048.000000f,
4096.000000f, 8192.000000f, 16384.000000f, 32768.000000f,
65536.000000f, 131072.000000f, 262144.000000f, 524288.000000f,
1048576.000000f, 2097152.000000f, 4194304.000000f, 8388608.000000f,
16777216.000000f, 33554432.000000f, 67108864.000000f, 134217728.000000f,
268435456.000000f, 536870912.000000f, 1073741824.000000f, 2147483648.000000f,
0.000000f, 0.000000f, 0.000000f, 0.000000f,
0.000000f, 0.000000f, 0.000000f, 0.000000f,
0.000000f, 0.000000f, 0.000000f, 0.000000f,
0.000001f, 0.000002f, 0.000004f, 0.000008f,
0.000015f, 0.000031f, 0.000061f, 0.000122f,
0.000244f, 0.000488f, 0.000977f, 0.001953f,
0.003906f, 0.007813f, 0.015625f, 0.031250f,
0.062500f, 0.125000f, 0.250000f, 0.500000f };

static float dequantize(uint32 data, sint32 type, uint8 scale)
{
	float f;
	switch (type)
	{
	case 4: // u8
		f = (float)(uint8)data; 
		f *= LD_SCALE[scale];
		break;
	case 5: // u16
		f = (float)(uint16)data; 
		f *= LD_SCALE[scale];
		break;
	case 6: // s8
		f = (float)(sint8)data;
		f *= LD_SCALE[scale];
		break;
	case 7: // float
		f = (float)(sint16)data;
		f *= LD_SCALE[scale];
		break;
	case 0:
	default:
		f = *((float *)&data);
		// scale does not apply when loading floats
		break;
	}
	return f;
}

static uint32 quantize(float data, sint32 type, uint8 scale)
{
	uint32 val;

	switch (type)
	{
	case 4: // u8
		data *= ST_SCALE[scale];
		if (data < 0) data = 0;
		if (data > 255) data = 255;
		val = (uint8)(uint32)data; 
		break;
	case 5: // u16
		data *= ST_SCALE[scale];
		if (data < 0) data = 0;
		if (data > 65535) data = 65535;
		val = (uint16)(uint32)data; 
		break;
	case 6: // s8
		data *= ST_SCALE[scale];
		if (data < -128) data = -128;
		if (data > 127) data = 127;
		val = (sint8)(uint8)(sint32)(uint32)data;
		break;
	case 7: // s16
		data *= ST_SCALE[scale];
		if (data < -32768) data = -32768;
		if (data > 32767) data = 32767;
		val = (sint16)(uint16)(sint32)(uint32)data;
		break;
	case 0: // float
	default: 
		// scale does not apply when storing floats
		*((float*)&val) = data; 
		break;
	}
	return val;
}

#define _uint32_fastSignExtend(__v, __bits) (uint32)(((sint32)(__v)<<(31-(__bits)))>>(31-(__bits)));

static inline uint64 ConvertToDoubleNoFTZ(uint32 value)
{
	// http://www.freescale.com/files/product/doc/MPCFPE32B.pdf

	uint64 x = value;
	uint64 exp = (x >> 23) & 0xff;
	uint64 frac = x & 0x007fffff;

	if (exp > 0 && exp < 255)
	{
		uint64 y = !(exp >> 7);
		uint64 z = y << 61 | y << 60 | y << 59;
		return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
	}
	else if (exp == 0 && frac != 0) // denormal
	{
		exp = 1023 - 126;
		do
		{
			frac <<= 1;
			exp -= 1;
		} while ((frac & 0x00800000) == 0);

		return ((x & 0x80000000) << 32) | (exp << 52) | ((frac & 0x007fffff) << 29);
	}
	else  // QNaN, SNaN or Zero
	{
		uint64 y = exp >> 7;
		uint64 z = y << 61 | y << 60 | y << 59;
		return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
	}
}

static inline uint32 ConvertToSingleNoFTZ(uint64 x)
{
	uint32 exp = (x >> 52) & 0x7ff;
	if (exp > 896 || (x & ~0x8000000000000000ULL) == 0)
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
	else if (exp >= 874)
	{
		uint32 t = (uint32)(0x80000000 | ((x & 0x000FFFFFFFFFFFFFULL) >> 21));
		t = t >> (905 - exp);
		t |= (x >> 32) & 0x80000000;
		return t;
	}
	else
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
}
