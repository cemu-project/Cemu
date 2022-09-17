#pragma once

namespace Espresso
{
	enum CR_BIT
	{
		CR_BIT_INDEX_LT = 0,
		CR_BIT_INDEX_GT = 1,
		CR_BIT_INDEX_EQ = 2,
		CR_BIT_INDEX_SO = 3,
	};

	enum class PrimaryOpcode
	{
		// underscore at the end of the name means that this instruction always updates CR0 (as if RC bit is set)
		ZERO = 0,
		VIRTUAL_HLE = 1,

		// 3 = TWI
		GROUP_4 = 4,
		MULLI = 7,
		SUBFIC = 8,
		CMPLI = 10,
		CMPI = 11,
		ADDIC = 12,
		ADDIC_ = 13,
		ADDI = 14,
		ADDIS = 15,
		BC = 16, // conditional branch
		GROUP_17 = 17, // SC
		B = 18, // unconditional branch
		GROUP_19 = 19,
		RLWIMI = 20,
		RLWINM = 21,
		// 22 ?
		RLWNM = 23,
		ORI = 24,
		ORIS = 25,
		XORI = 26,
		XORIS = 27,
		ANDI_ = 28,
		ANDIS_ = 29,
		GROUP_31 = 31,
		LWZ = 32,
		LWZU = 33,
		LBZ = 34,
		LBZU = 35,
		STW = 36,
		STWU = 37,
		STB = 38,
		STBU = 39,
		LHZ = 40,
		LHZU = 41,
		LHA = 42,
		LHAU = 43,
		STH = 44,
		STHU = 45,
		LMW = 46,
		STMW = 47,
		LFS = 48,
		LFSU = 49,
		LFD = 50,
		LFDU = 51,
		STFS = 52,
		STFSU = 53,
		STFD = 54,
		STFDU = 55,
		PSQ_L = 56,
		PSQ_LU = 57,
		// 58 ?
		GROUP_59 = 59,
		PSQ_ST = 60,
		PSQ_STU = 61,
		// 62 ?
		GROUP_63 = 63,
	};

	enum class Opcode19
	{
		MCRF = 0,
		BCLR = 16,
		CRNOR = 33,
		RFI = 50,
		CRANDC = 129,
		ISYNC = 150,
		CRXOR = 193,
		CRAND = 257,
		CREQV = 289,
		CRORC = 417,
		CROR = 449,
		BCCTR = 528
	};

	enum class OPCODE_31
	{

	};

	inline PrimaryOpcode GetPrimaryOpcode(uint32 opcode) { return (PrimaryOpcode)(opcode >> 26); };
	inline Opcode19 GetGroup19Opcode(uint32 opcode) { return (Opcode19)((opcode >> 1) & 0x3FF); };

	struct BOField 
	{
		BOField() = default;
		BOField(uint8 bo) : bo(bo) {};

		bool conditionInverted() const
		{
			return (bo & 8) == 0;
		}

		bool decrementerIgnore() const
		{
			return (bo & 4) != 0;
		}

		bool decrementerMustBeZero() const
		{
			return (bo & 2) != 0;
		}

		bool conditionIgnore() const
		{
			return (bo & 16) != 0;
		}

		bool branchAlways()
		{
			return conditionIgnore() && decrementerIgnore();
		}

		uint8 bo;
	};

	inline void _decodeForm_I(uint32 opcode, uint32& LI, bool& AA, bool& LK)
	{
		LI = opcode & 0x3fffffc;
		if (LI & 0x02000000)
			LI |= 0xfc000000;
		AA = (opcode & 2) != 0;
		LK = (opcode & 1) != 0;
	}

	inline void _decodeForm_D_branch(uint32 opcode, uint32& BD, BOField& BO, uint32& BI, bool& AA, bool& LK)
	{
		BD = opcode & 0xfffc;
		if (BD & 0x8000)
			BD |= 0xffff0000;
		BO = { (uint8)((opcode >> 21) & 0x1F) };
		BI = (opcode >> 16) & 0x1F;
		AA = (opcode & 2) != 0;
		LK = (opcode & 1) != 0;
	}

	inline void _decodeForm_D_SImm(uint32 opcode, uint32& rD, uint32& rA, uint32& imm)
	{
		rD = (opcode >> 21) & 0x1F;
		rA = (opcode >> 16) & 0x1F;
		imm = (uint32)(sint32)(sint16)(opcode & 0xFFFF);
	}

	inline void _decodeForm_XL(uint32 opcode, BOField& BO, uint32& BI, bool& LK)
	{
		BO = { (uint8)((opcode >> 21) & 0x1F) };
		BI = (opcode >> 16) & 0x1F;
		LK = (opcode & 1) != 0;
	}

	inline void decodeOp_ADDI(uint32 opcode, uint32& rD, uint32& rA, uint32& imm)
	{
		_decodeForm_D_SImm(opcode, rD, rA, imm);
	}

	inline void decodeOp_B(uint32 opcode, uint32& LI, bool& AA, bool& LK)
	{
		// form I
		_decodeForm_I(opcode, LI, AA, LK);
	}

	inline void decodeOp_BC(uint32 opcode, uint32& BD, BOField& BO, uint32& BI, bool& AA, bool& LK)
	{
		// form D
		_decodeForm_D_branch(opcode, BD, BO, BI, AA, LK);
	}

	inline void decodeOp_BCLR(uint32 opcode, BOField& BO, uint32& BI, bool& LK)
	{
		// form XL (with BD field expected to be zero)
		_decodeForm_XL(opcode, BO, BI, LK);
	}

	inline void decodeOp_BCCTR(uint32 opcode, BOField& BO, uint32& BI, bool& LK)
	{
		// form XL (with BD field expected to be zero)
		_decodeForm_XL(opcode, BO, BI, LK);
	}
}
