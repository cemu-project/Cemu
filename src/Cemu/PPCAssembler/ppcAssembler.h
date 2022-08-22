#pragma once

#include <boost/container/small_vector.hpp>

#define PPCASM_OPERAND_COUNT		5

#define PPCASM_OPERAND_TYPE_GPR		0 // r0 - r31
#define PPCASM_OPERAND_TYPE_FPR		1 // f0 - f31
#define PPCASM_OPERAND_TYPE_SPR		2 // spr0 - spr511
#define PPCASM_OPERAND_TYPE_IMM		3 // integer constants. E.g. 0x123 
#define PPCASM_OPERAND_TYPE_MEM		4 // [r0 + 1234]
#define PPCASM_OPERAND_TYPE_CIMM	5 // virtual addr of code destination (used for branches)
#define PPCASM_OPERAND_TYPE_CR		6 // cr0-cr7
#define PPCASM_OPERAND_TYPE_CR_BIT	7 // cr bit 0-31. Example display form: '4*cr1+eq'
#define PPCASM_OPERAND_TYPE_PSQMODE	8 // single or paired mode control for PSQ_L*/PSQ_ST* instructions

enum PPCASM_OP
{
	PPCASM_OP_UKN,

	PPCASM_OP_ADDI,
	PPCASM_OP_SUBI, // special form of ADDI
	PPCASM_OP_ADDIS,
	PPCASM_OP_ADDIC,
	PPCASM_OP_ADDIC_,

	PPCASM_OP_ADD,
	PPCASM_OP_ADD_,
	PPCASM_OP_SUBF,
	PPCASM_OP_SUBF_,
	PPCASM_OP_SUBFC,
	PPCASM_OP_SUBFC_,
	PPCASM_OP_SUBFE,
	PPCASM_OP_SUBFE_,
	PPCASM_OP_SUBFIC,

	PPCASM_OP_SUB, // alias mnemonic for subf with second and third operand swapped
	PPCASM_OP_SUB_,

	PPCASM_OP_MULLI,
	PPCASM_OP_MULLW,
	PPCASM_OP_MULLW_,
	PPCASM_OP_MULHW,
	PPCASM_OP_MULHW_,
	PPCASM_OP_MULHWU,
	PPCASM_OP_MULHWU_,

	PPCASM_OP_DIVW,
	PPCASM_OP_DIVW_,
	PPCASM_OP_DIVWU,
	PPCASM_OP_DIVWU_,

	PPCASM_OP_AND,
	PPCASM_OP_AND_,
	PPCASM_OP_ANDC,
	PPCASM_OP_ANDC_,
	PPCASM_OP_OR,
	PPCASM_OP_OR_,
	PPCASM_OP_ORC,
	PPCASM_OP_XOR,
	PPCASM_OP_NOR,
	PPCASM_OP_NOR_,
	PPCASM_OP_NOT, // alias to NOR
	PPCASM_OP_NOT_,
	PPCASM_OP_NEG,
	PPCASM_OP_NEG_,
	PPCASM_OP_ANDI_,
	PPCASM_OP_ANDIS_,
	PPCASM_OP_ORI,
	PPCASM_OP_ORIS,
	PPCASM_OP_XORI,
	PPCASM_OP_XORIS,
	PPCASM_OP_CNTLZW,
	PPCASM_OP_EXTSB,
	PPCASM_OP_EXTSH,
	PPCASM_OP_CNTLZW_,
	PPCASM_OP_EXTSB_,
	PPCASM_OP_EXTSH_,
	PPCASM_OP_SRAW,
	PPCASM_OP_SRAW_,
	PPCASM_OP_SRAWI,
	PPCASM_OP_SRAWI_,
	PPCASM_OP_SLW,
	PPCASM_OP_SLW_,
	PPCASM_OP_SRW,
	PPCASM_OP_SRW_,
	PPCASM_OP_RLWINM,
	PPCASM_OP_RLWINM_,
	PPCASM_OP_RLWIMI,
	PPCASM_OP_RLWIMI_,

	// rlwinm extended mnemonics
	PPCASM_OP_EXTLWI,
	PPCASM_OP_EXTLWI_,
	PPCASM_OP_EXTRWI,
	PPCASM_OP_EXTRWI_,

	PPCASM_OP_ROTLWI,
	PPCASM_OP_ROTLWI_,
	PPCASM_OP_ROTRWI,
	PPCASM_OP_ROTRWI_,

	PPCASM_OP_SLWI,
	PPCASM_OP_SLWI_,
	PPCASM_OP_SRWI,
	PPCASM_OP_SRWI_,
	PPCASM_OP_CLRLWI,
	PPCASM_OP_CLRLWI_,
	PPCASM_OP_CLRRWI,
	PPCASM_OP_CLRRWI_,

	// rlwimi extended mnemonics
	//PPCASM_OP_INSLWI, rlwimi
	//PPCASM_OP_INSLWI_, rlwimi
	//PPCASM_OP_INSRWI, rlwimi
	//PPCASM_OP_INSRWI_, rlwimi

	// rlwnm extended mnemonics
	PPCASM_OP_RLWNM,
	PPCASM_OP_RLWNM_,
	PPCASM_OP_ROTLW,
	PPCASM_OP_ROTLW_,

	PPCASM_OP_CMPWI,
	PPCASM_OP_CMPLWI,

	PPCASM_OP_CMPW,
	PPCASM_OP_CMPLW,

	PPCASM_OP_MR,
	PPCASM_OP_MR_,

	PPCASM_OP_MFSPR,
	PPCASM_OP_MTSPR,

	PPCASM_OP_LMW,
	PPCASM_OP_LWZ,
	PPCASM_OP_LWZU,
	PPCASM_OP_LWZX,
	PPCASM_OP_LWZUX,
	PPCASM_OP_LHZ,
	PPCASM_OP_LHZU,
	PPCASM_OP_LHZX,
	PPCASM_OP_LHZUX,
	PPCASM_OP_LHA,
	PPCASM_OP_LHAU,
	PPCASM_OP_LHAX,
	PPCASM_OP_LHAUX,
	PPCASM_OP_LBZ,
	PPCASM_OP_LBZU,
	PPCASM_OP_LBZX,
	PPCASM_OP_LBZUX,

	PPCASM_OP_STMW,
	PPCASM_OP_STW,
	PPCASM_OP_STWU,
	PPCASM_OP_STWX,
	PPCASM_OP_STWUX,
	PPCASM_OP_STH,
	PPCASM_OP_STHU,
	PPCASM_OP_STHX,
	PPCASM_OP_STHUX,
	PPCASM_OP_STB,
	PPCASM_OP_STBU,
	PPCASM_OP_STBX,
	PPCASM_OP_STBUX,
	PPCASM_OP_STWBRX,
	PPCASM_OP_STHBRX,
	PPCASM_OP_STSWI,
	PPCASM_OP_LWARX,
	PPCASM_OP_STWCX_,

	PPCASM_OP_B,
	PPCASM_OP_BA,
	PPCASM_OP_BL,
	PPCASM_OP_BLA,

	PPCASM_OP_BC,
	PPCASM_OP_BNE,
	PPCASM_OP_BEQ,
	PPCASM_OP_BGE,
	PPCASM_OP_BGT,
	PPCASM_OP_BLT,
	PPCASM_OP_BLE,
	PPCASM_OP_BDZ,
	PPCASM_OP_BDNZ,

	PPCASM_OP_BLR,
	PPCASM_OP_BLTLR, // less
	PPCASM_OP_BLELR, // less or equal
	PPCASM_OP_BEQLR, // equal
	PPCASM_OP_BGELR, // greater or equal
	PPCASM_OP_BGTLR, // greater
	PPCASM_OP_BNELR, // not equal

	PPCASM_OP_BCTR,
	PPCASM_OP_BCTRL,

	// CR
	PPCASM_OP_CROR,
	PPCASM_OP_CRORC,
	PPCASM_OP_CRXOR,
	PPCASM_OP_CREQV,
	PPCASM_OP_CRNOR,
	PPCASM_OP_CRAND,
	PPCASM_OP_CRNAND,
	PPCASM_OP_CRANDC,
	PPCASM_OP_CRSET, // simplified mnemonic for CREQV
	PPCASM_OP_CRCLR, // simplified mnemonic for CRXOR
	PPCASM_OP_CRMOVE, // simplified mnemonic for CROR
	PPCASM_OP_CRNOT, // simplified mnemonic for CRNOR

	// floating point load/store
	PPCASM_OP_LFS,
	PPCASM_OP_LFSU,
	PPCASM_OP_LFSX,
	PPCASM_OP_LFSUX,
	PPCASM_OP_LFD,
	PPCASM_OP_LFDU,
	PPCASM_OP_LFDX,
	PPCASM_OP_LFDUX,

	PPCASM_OP_STFS,
	PPCASM_OP_STFSU,
	PPCASM_OP_STFSX,
	PPCASM_OP_STFSUX,
	PPCASM_OP_STFD,
	PPCASM_OP_STFDU,
	PPCASM_OP_STFDX,
	PPCASM_OP_STFDUX,

	PPCASM_OP_STFIWX,

	// FP
	PPCASM_OP_FMR,
	PPCASM_OP_FNEG,
	PPCASM_OP_FRSP,
	PPCASM_OP_FRSQRTE,
	PPCASM_OP_FADD,
	PPCASM_OP_FADDS,
	PPCASM_OP_FSUB,
	PPCASM_OP_FSUBS,
	PPCASM_OP_FMUL,
	PPCASM_OP_FMULS,
	PPCASM_OP_FDIV,
	PPCASM_OP_FDIVS,

	PPCASM_OP_FMADD,
	PPCASM_OP_FMADDS,
	PPCASM_OP_FNMADD,
	PPCASM_OP_FNMADDS,
	PPCASM_OP_FMSUB,
	PPCASM_OP_FMSUBS,
	PPCASM_OP_FNMSUB,
	PPCASM_OP_FNMSUBS,

	PPCASM_OP_FCTIWZ,

	PPCASM_OP_FCMPU,
	PPCASM_OP_FCMPO,

	// PS
	PPCASM_OP_PS_MERGE00,
	PPCASM_OP_PS_MERGE01,
	PPCASM_OP_PS_MERGE10,
	PPCASM_OP_PS_MERGE11,

	PPCASM_OP_PS_ADD,
	PPCASM_OP_PS_SUB,
	PPCASM_OP_PS_DIV,
	PPCASM_OP_PS_MUL,

	PPCASM_OP_PS_MADD,
	PPCASM_OP_PS_MSUB,
	PPCASM_OP_PS_NMADD,
	PPCASM_OP_PS_NMSUB,

	// cache & misc
	PPCASM_OP_ISYNC,

	// extended mnemonics
	PPCASM_OP_NOP, // ORI
	PPCASM_OP_LI, // ADDI
	PPCASM_OP_LIS, // ADDIS
	PPCASM_OP_MFLR, // MFSPR
	PPCASM_OP_MTLR, // MTSPR
	PPCASM_OP_MFCTR, // MFSPR
	PPCASM_OP_MTCTR, // MTSPR
};

struct PPCDisassemblerOperand
{
	uint8 type;
	uint16 registerIndex;
	union
	{
		sint32 immS32;
		uint32 immU32;
	};
	bool isSignedImm;
	uint8 immWidth; // in bits. E.g. 16 for ADDI
};

struct PPCDisassembledInstruction
{
	// output
	uint32 ppcAsmCode;
	uint8 operandMask;
	PPCDisassemblerOperand operand[PPCASM_OPERAND_COUNT];
	// conditional branches
	bool branchHintBitSet{false};
};

const char* ppcAssembler_getInstructionName(uint32 ppcAsmOp);

void ppcAssembler_disassemble(uint32 virtualAddress, uint32 opcode, PPCDisassembledInstruction* disInstr);

enum class PPCASM_RELOC
{
	// code
	U32_MASKED_IMM,
	BRANCH_S16,
	BRANCH_S26,
	// data constants
	FLOAT, // 4 byte float data
	DOUBLE, // 8 byte double precision float data
	U32, // 4 byte unsigned integer
	U16, // 2 byte unsigned integer
	U8, // 1 byte unsigned integer

};

struct PPCAssemblerReloc
{
	PPCAssemblerReloc(PPCASM_RELOC relocType, std::string expression, uint32 byteOffset, uint8 bitOffset, uint8 bitCount) : m_relocType(relocType), m_expression(expression), m_byteOffset(byteOffset), m_bitOffset(bitOffset), m_bitCount(bitCount) {};
	PPCASM_RELOC m_relocType;
	std::string m_expression;
	uint32 m_byteOffset;
	uint8 m_bitOffset;
	uint8 m_bitCount;
	bool m_isApplied{};
	bool isApplied() { return m_isApplied; };
	void setApplied() { m_isApplied = true; };
};

struct PPCAssemblerInOut
{
	// in
	uint32 virtualAddress;
	bool forceNoAlignment{}; // if set, alignment will always be set to 1 (even for .align directive!)
	// out
	boost::container::small_vector<uint8, 16> outputData;
	std::vector<PPCAssemblerReloc> list_relocs;
	std::string errorMsg;
	uint32 alignmentRequirement{}; // alignment requirement, 0 if none
	uint32 alignmentPaddingSize{}; // number of bytes to fill with alignment padding before instruction
	uint32 virtualAddressAligned{}; // effective virtualAddress
};

bool ppcAssembler_assembleSingleInstruction(char const* text, PPCAssemblerInOut* ctx);

static uint32 ppcAssembler_generateMaskRLW(int MB, int ME)
{
	uint32 maskMB = 0xFFFFFFFF >> MB;
	uint32 maskME = 0xFFFFFFFF << (31 - ME);
	uint32 mask = (MB <= ME) ? maskMB & maskME : maskMB | maskME;
	return mask;
}