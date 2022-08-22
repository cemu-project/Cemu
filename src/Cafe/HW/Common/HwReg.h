#pragma once

namespace HWREG
{

#define REGISTER_FULL_FIELD(__regname) \
auto& set_##__regname(uint32 newValue) \
{	\
	v = newValue; \
    return *this; \
} \
uint32 get_##__regname() const \
{	\
	return v; \
}

#define REGISTER_BITFIELD(__regname, __bitIndex, __bitWidth) \
auto& set_##__regname(uint32 newValue) \
{	\
	cemu_assert_debug(newValue < (1u << (__bitWidth))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (newValue << (__bitIndex)); \
    return *this; \
} \
uint32 get_##__regname() const \
{	\
	return (v >> (__bitIndex))&((1u << (__bitWidth)) - 1u); \
}

#define REGISTER_BITFIELD_SIGNED(__regname, __bitIndex, __bitWidth) \
auto& set_##__regname(sint32 newValue) \
{	\
	cemu_assert_debug(newValue < (1 << ((__bitWidth)-1))); \
	cemu_assert_debug(newValue >= -(1 << ((__bitWidth)-1))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (((uint32)newValue & ((1u << (__bitWidth)) - 1u)) << (__bitIndex)); \
    return *this; \
} \
sint32 get_##__regname() const \
{	\
    sint32 r = (v >> (__bitIndex))&((1u << (__bitWidth)) - 1u); \
    r = (r << (32 - (__bitWidth))); \
    r = (r >> (32 - (__bitWidth))); \
	return r; \
}

#define REGISTER_BITFIELD_BOOL(__regname, __bitIndex) \
auto& set_##__regname(bool newValue) \
{	\
	if(newValue) \
	v |= (1u << (__bitIndex)); \
	else \
	v &= ~(1u << (__bitIndex)); \
    return *this; \
} \
bool get_##__regname() const \
{	\
	return (v&(1u << (__bitIndex))) != 0; \
}

#define REGISTER_BITFIELD_TYPED(__regname, __bitIndex, __bitWidth, __typename) \
auto& set_##__regname(__typename newValue) \
{	\
	cemu_assert_debug(static_cast<uint32>(newValue) < (1u << (__bitWidth))); \
	v &= ~((((1u << (__bitWidth)) - 1u) << (__bitIndex))); \
	v |= (static_cast<uint32>(newValue) << (__bitIndex)); \
    return *this; \
} \
__typename get_##__regname() const \
{	\
	return static_cast<__typename>((v >> (__bitIndex))&((1u << (__bitWidth)) - 1u)); \
}

#define REGISTER_BITFIELD_FLOAT(__regname) \
auto& set_##__regname(float newValue) \
{	\
	*(float*)&v = newValue; \
    return *this; \
} \
float get_##__regname() const \
{	\
	return *(float*)&v; \
}

	class HWREG
	{
	public:
		uint32 getRawValue() const
		{
			return v;
		}

		uint32 getRawValueBE() const
		{
			return _swapEndianU32(v);
		}

		void setFromRaw(uint32 regValue)
		{
			v = regValue;
		}

	protected:
		uint32 v{};
	};

	/* ACR */

	struct ACR_VI_DATA : HWREG // 0x0D00021C - official name unknown
	{
		REGISTER_FULL_FIELD(DATA);
	};

	struct ACR_VI_ADDR : HWREG // 0x0D000224 - official name unknown
	{
		REGISTER_FULL_FIELD(ADDR);
	};

	struct ACR_VI_CTRL : HWREG // 0x0D000228 - official name unknown
	{
		REGISTER_BITFIELD_BOOL(HAS_OWNERSHIP, 0); // exact purpose not understood
		// other fields unknown
	};


	/* SI */

	struct SICOUTBUF : HWREG // 0x6400/0x640C/0x6418/0x6424
	{
		REGISTER_BITFIELD(OUTPUT1, 0, 8);
		REGISTER_BITFIELD(OUTPUT0, 8, 8);
		REGISTER_BITFIELD(CMD, 16, 8);

	};

	struct SICINBUFH : HWREG // 0x6404/0x6410/0x641C/0x6428
	{
		REGISTER_BITFIELD(INPUT3, 0, 8);
		REGISTER_BITFIELD(INPUT2, 8, 8);
		REGISTER_BITFIELD(INPUT1, 16, 8);
		REGISTER_BITFIELD(INPUT0, 24, 6);
		REGISTER_BITFIELD(ERRLATCH, 30, 1);
		REGISTER_BITFIELD(ERRSTAT, 31, 1);
	};

	struct SICINBUFL : HWREG // 0x6408/0x6414/0x6420/0x642C
	{
		REGISTER_BITFIELD(INPUT4, 0, 8);
		REGISTER_BITFIELD(INPUT5, 8, 8);
		REGISTER_BITFIELD(INPUT6, 16, 8);
		REGISTER_BITFIELD(INPUT7, 24, 8);
	};

	struct SIPOLL : HWREG // 0x6430
	{
		REGISTER_BITFIELD(X, 16, 10);
		REGISTER_BITFIELD(Y, 8, 8);
		REGISTER_BITFIELD(EN, 4, 4);
		REGISTER_BITFIELD(VBCPY, 0, 4);
	};

	struct SICOMCSR : HWREG // 0x6434
	{
		REGISTER_BITFIELD(TCINT, 31, 1);
		REGISTER_BITFIELD(TCINTMASK, 30, 1);
		REGISTER_BITFIELD(COMERR, 29, 1);
		REGISTER_BITFIELD(RDSTINT, 28, 1);
		REGISTER_BITFIELD(RDSTINTMSK, 27, 1);
		REGISTER_BITFIELD(UKN_CHANNEL_NUM_MAYBE, 25, 2);
		REGISTER_BITFIELD(CHANNELENABLE, 24, 1);
		REGISTER_BITFIELD(OUTLNGTH, 16, 7);
		REGISTER_BITFIELD(INLNGTH, 8, 7);
		REGISTER_BITFIELD(COMMAND_ENABLE, 7, 1);
		REGISTER_BITFIELD(CALLBACK_ENABLE, 6, 1);
		REGISTER_BITFIELD(CHANNEL, 1, 2);
		REGISTER_BITFIELD(TRANSFER_START, 0, 1);
	};

	struct SISR : HWREG // 0x6438
	{
		REGISTER_BITFIELD(WR, 31, 1);
		// joy-channel 0
		REGISTER_BITFIELD(RDST0, 29, 1);
		REGISTER_BITFIELD(WRST0, 28, 1);
		REGISTER_BITFIELD(NOREP0, 27, 1);
		REGISTER_BITFIELD(COLL0, 26, 1);
		REGISTER_BITFIELD(OVRUN0, 25, 1);
		REGISTER_BITFIELD(UNRUN0, 24, 1);
		// joy-channel 1
		REGISTER_BITFIELD(RDST1, 21, 1);
		REGISTER_BITFIELD(WRST1, 20, 1);
		REGISTER_BITFIELD(NOREP1, 19, 1);
		REGISTER_BITFIELD(COLL1, 18, 1);
		REGISTER_BITFIELD(OVRUN1, 17, 1);
		REGISTER_BITFIELD(UNRUN1, 16, 1);
		// joy-channel 2
		REGISTER_BITFIELD(RDST2, 13, 1);
		REGISTER_BITFIELD(WRST2, 12, 1);
		REGISTER_BITFIELD(NOREP2, 11, 1);
		REGISTER_BITFIELD(COLL2, 10, 1);
		REGISTER_BITFIELD(OVRUN2, 9, 1);
		REGISTER_BITFIELD(UNRUN2, 8, 1);
		// joy-channel 3
		REGISTER_BITFIELD(RDST3, 5, 1);
		REGISTER_BITFIELD(WRST3, 4, 1);
		REGISTER_BITFIELD(NOREP3, 3, 1);
		REGISTER_BITFIELD(COLL3, 2, 1);
		REGISTER_BITFIELD(OVRUN3, 1, 1);
		REGISTER_BITFIELD(UNRUN3, 0, 1);
	};

}