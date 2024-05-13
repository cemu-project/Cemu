#include "Cemu/PPCAssembler/ppcAssembler.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/Interpreter/PPCInterpreterInternal.h"
#include "Cemu/ExpressionParser/ExpressionParser.h"
#include "util/helpers/helpers.h"

#include <boost/container/small_vector.hpp>
#include <boost/static_string/static_string.hpp>

struct ppcAssemblerStr_t
{
	ppcAssemblerStr_t(const char* start, const char* end) : str(start, end - start) {};
	ppcAssemblerStr_t(const char* start, size_t len) : str(start, len) {};

	std::string_view str;
};

struct PPCAssemblerContext
{
	PPCAssemblerInOut* ctx;
	boost::container::small_vector<ppcAssemblerStr_t, 8> listOperandStr;
	struct PPCInstructionDef* iDef;
	uint32 opcode;
};

// table based assembler/disassembler

#define operand0Bit	(1<<0)
#define operand1Bit	(1<<1)
#define operand2Bit	(1<<2)
#define operand3Bit	(1<<3)
#define operand4Bit	(1<<4)

#define OPC_NONE			(0xFFFF)
#define OPC_EXTENDED_BIT	(0x8000) // use extended sub opcode

enum
{
	OP_FORM_UNUSED,
	OP_FORM_XL_CR, // CRXOR, CRAND etc.
	OP_FORM_OP3_A_CMP, 
	OP_FORM_OP3_A_IMM, // rA, rS, rB is imm - has RC bit
	OP_FORM_BRANCH_S16,
	OP_FORM_BRANCH_S24,
	OP_FORM_OP2_D_HSIMM, // rD, signed imm shifted imm (high half)
	OP_FORM_RLWINM,
	OP_FORM_RLWINM_EXTENDED, // alternative mnemonics of rlwinm
	OP_FORM_RLWNM,
	OP_FORM_RLWNM_EXTENDED, // alternative mnemonics of rlwnm
	OP_FORM_CMP_SIMM, // cr, rD, r
	OP_FORM_NO_OPERAND,
	// FP
	OP_FORM_X_FP_CMP,

	// new generic form with operand encoding stored in the table entry, everything above is deprecated
	OP_FORM_DYNAMIC,
};

const char* ppcAssembler_getInstructionName(uint32 ppcAsmOp)
{
	switch (ppcAsmOp)
	{
	case PPCASM_OP_UKN: return "UKN";

	case PPCASM_OP_ADDI: return "ADDI";
	case PPCASM_OP_SUBI: return "SUBI";
	case PPCASM_OP_ADDIS: return "ADDIS";
	case PPCASM_OP_ADDIC: return "ADDIC";
	case PPCASM_OP_ADDIC_: return "ADDIC.";

	case PPCASM_OP_ADD: return "ADD";
	case PPCASM_OP_ADD_: return "ADD.";
	case PPCASM_OP_SUBF: return "SUBF";
	case PPCASM_OP_SUBF_: return "SUBF.";
	case PPCASM_OP_SUBFC: return "SUBFC";
	case PPCASM_OP_SUBFC_: return "SUBFC.";
	case PPCASM_OP_SUBFE: return "SUBFE";
	case PPCASM_OP_SUBFE_: return "SUBFE.";
	case PPCASM_OP_SUBFIC: return "SUBFIC";

	case PPCASM_OP_SUB: return "SUB";
	case PPCASM_OP_SUB_: return "SUB.";

	case PPCASM_OP_MULLI: return "MULLI";
	case PPCASM_OP_MULLW: return "MULLW";
	case PPCASM_OP_MULLW_: return "MULLW.";
	case PPCASM_OP_MULHW: return "MULHW";
	case PPCASM_OP_MULHW_: return "MULHW.";
	case PPCASM_OP_MULHWU: return "MULHWU";
	case PPCASM_OP_MULHWU_: return "MULHWU.";

	case PPCASM_OP_DIVW: return "DIVW";
	case PPCASM_OP_DIVW_: return "DIVW.";
	case PPCASM_OP_DIVWU: return "DIVWU";
	case PPCASM_OP_DIVWU_: return "DIVWU.";

	case PPCASM_OP_AND:		return "AND";
	case PPCASM_OP_AND_:	return "AND.";
	case PPCASM_OP_ANDC:	return "ANDC";
	case PPCASM_OP_ANDC_:	return "ANDC.";

	case PPCASM_OP_ANDI_:	return "ANDI.";
	case PPCASM_OP_ANDIS_:	return "ANDIS.";
	case PPCASM_OP_OR:		return "OR";
	case PPCASM_OP_OR_:		return "OR.";
	case PPCASM_OP_ORI:		return "ORI";
	case PPCASM_OP_ORIS:	return "ORIS";
	case PPCASM_OP_ORC:		return "ORC";
	case PPCASM_OP_XOR:		return "XOR";
	case PPCASM_OP_NOR:		return "NOR";
	case PPCASM_OP_NOR_:	return "NOR.";
	case PPCASM_OP_NOT:		return "NOT";
	case PPCASM_OP_NOT_:	return "NOT.";
	case PPCASM_OP_NEG:		return "NEG";
	case PPCASM_OP_NEG_:	return "NEG.";
	case PPCASM_OP_XORI:	return "XORI";
	case PPCASM_OP_XORIS:	return "XORIS";
	case PPCASM_OP_SRAW:	return "SRAW";
	case PPCASM_OP_SRAW_:	return "SRAW.";
	case PPCASM_OP_SRAWI:	return "SRAWI";
	case PPCASM_OP_SRAWI_:	return "SRAWI.";
	case PPCASM_OP_SLW:		return "SLW";
	case PPCASM_OP_SLW_:	return "SLW.";
	case PPCASM_OP_SRW:		return "SRW";
	case PPCASM_OP_SRW_:	return "SRW.";

	case PPCASM_OP_RLWINM:	return "RLWINM";
	case PPCASM_OP_RLWINM_:	return "RLWINM.";
	case PPCASM_OP_RLWIMI:	return "RLWIMI";
	case PPCASM_OP_RLWIMI_:	return "RLWIMI.";

	case PPCASM_OP_EXTLWI: return "EXTLWI";
	case PPCASM_OP_EXTLWI_: return "EXTLWI.";
	case PPCASM_OP_EXTRWI: return "EXTRWI";
	case PPCASM_OP_EXTRWI_: return "EXTRWI.";

	case PPCASM_OP_ROTLWI: return "ROTLWI";
	case PPCASM_OP_ROTLWI_: return "ROTLWI.";
	case PPCASM_OP_ROTRWI: return "ROTRWI";
	case PPCASM_OP_ROTRWI_: return "ROTRWI.";

	case PPCASM_OP_SLWI: return "SLWI";
	case PPCASM_OP_SLWI_: return "SLWI.";
	case PPCASM_OP_SRWI: return "SRWI";
	case PPCASM_OP_SRWI_: return "SRWI.";

	case PPCASM_OP_CLRLWI: return "CLRLWI";
	case PPCASM_OP_CLRLWI_: return "CLRLWI.";
	case PPCASM_OP_CLRRWI: return "CLRRWI";
	case PPCASM_OP_CLRRWI_: return "CLRRWI.";

	case PPCASM_OP_RLWNM: return "RLWNM";
	case PPCASM_OP_RLWNM_: return "RLWNM.";

	case PPCASM_OP_ROTLW: return "ROTLW";
	case PPCASM_OP_ROTLW_: return "ROTLW.";

	case PPCASM_OP_CMPWI: return "CMPWI";
	case PPCASM_OP_CMPLWI: return "CMPLWI";
	case PPCASM_OP_CMPW: return "CMPW";
	case PPCASM_OP_CMPLW: return "CMPLW";

	case PPCASM_OP_MR: return "MR";
	case PPCASM_OP_MR_: return "MR.";

	case PPCASM_OP_EXTSB: return "EXTSB";
	case PPCASM_OP_EXTSH: return "EXTSH";
	case PPCASM_OP_CNTLZW: return "CNTLZW";
	case PPCASM_OP_EXTSB_: return "EXTSB.";
	case PPCASM_OP_EXTSH_: return "EXTSH.";
	case PPCASM_OP_CNTLZW_: return "CNTLZW.";

	case PPCASM_OP_MFSPR: return "MFSPR";
	case PPCASM_OP_MTSPR: return "MTSPR";

	case PPCASM_OP_LMW: return "LMW";
	case PPCASM_OP_LWZ: return "LWZ";
	case PPCASM_OP_LWZU: return "LWZU";
	case PPCASM_OP_LWZX: return "LWZX";
	case PPCASM_OP_LWZUX: return "LWZUX";
	case PPCASM_OP_LHZ: return "LHZ";
	case PPCASM_OP_LHZU: return "LHZU";
	case PPCASM_OP_LHZX: return "LHZX";
	case PPCASM_OP_LHZUX: return "LHZUX";
	case PPCASM_OP_LHA: return "LHA";
	case PPCASM_OP_LHAU: return "LHAU";
	case PPCASM_OP_LHAX: return "LHAX";
	case PPCASM_OP_LHAUX: return "LHAUX";
	case PPCASM_OP_LBZ: return "LBZ";
	case PPCASM_OP_LBZU: return "LBZU";
	case PPCASM_OP_LBZX: return "LBZX";
	case PPCASM_OP_LBZUX: return "LBZUX";
	case PPCASM_OP_STMW: return "STMW";
	case PPCASM_OP_STW: return "STW";
	case PPCASM_OP_STWU: return "STWU";
	case PPCASM_OP_STWX: return "STWX";
	case PPCASM_OP_STWUX: return "STWUX";
	case PPCASM_OP_STH: return "STH";
	case PPCASM_OP_STHU: return "STHU";
	case PPCASM_OP_STHX: return "STHX";
	case PPCASM_OP_STHUX: return "STHUX";
	case PPCASM_OP_STB: return "STB";
	case PPCASM_OP_STBU: return "STBU";
	case PPCASM_OP_STBX: return "STBX";
	case PPCASM_OP_STBUX: return "STBUX";
	case PPCASM_OP_STSWI: return "STSWI";
	case PPCASM_OP_STWBRX: return "STWBRX";
	case PPCASM_OP_STHBRX: return "STHBRX";

	case PPCASM_OP_LWARX: return "LWARX";
	case PPCASM_OP_STWCX_: return "STWCX.";

	case PPCASM_OP_B: return "B";
	case PPCASM_OP_BL: return "BL";
	case PPCASM_OP_BA: return "BA";
	case PPCASM_OP_BLA: return "BLA";
		
	case PPCASM_OP_BC: return "BC";
	case PPCASM_OP_BNE: return "BNE";
	case PPCASM_OP_BEQ: return "BEQ";
	case PPCASM_OP_BGE: return "BGE";
	case PPCASM_OP_BGT: return "BGT";
	case PPCASM_OP_BLT: return "BLT";
	case PPCASM_OP_BLE:	return "BLE";
	case PPCASM_OP_BDZ: return "BDZ";
	case PPCASM_OP_BDNZ: return "BDNZ";

	case PPCASM_OP_BLR:   return "BLR";
	case PPCASM_OP_BLTLR: return "BLTLR";
	case PPCASM_OP_BLELR: return "BLELR";
	case PPCASM_OP_BEQLR: return "BEQLR";
	case PPCASM_OP_BGELR: return "BGELR";
	case PPCASM_OP_BGTLR: return "BGTLR";
	case PPCASM_OP_BNELR: return "BNELR";

	case PPCASM_OP_BCTR: return "BCTR";
	case PPCASM_OP_BCTRL: return "BCTRL";
		
	case PPCASM_OP_LFS:	return "LFS";
	case PPCASM_OP_LFSU: return "LFSU";
	case PPCASM_OP_LFSX: return "LFSX";
	case PPCASM_OP_LFSUX: return "LFSUX";
	case PPCASM_OP_LFD: return "LFD";
	case PPCASM_OP_LFDU: return "LFDU";
	case PPCASM_OP_LFDX: return "LFDX";
	case PPCASM_OP_LFDUX: return "LFDUX";
	case PPCASM_OP_STFS: return "STFS";
	case PPCASM_OP_STFSU: return "STFSU";
	case PPCASM_OP_STFSX: return "STFSX";
	case PPCASM_OP_STFSUX: return "STFSUX";
	case PPCASM_OP_STFD: return "STFD";
	case PPCASM_OP_STFDU: return "STFDU";
	case PPCASM_OP_STFDX: return "STFDX";
	case PPCASM_OP_STFDUX: return "STFDUX";

	case PPCASM_OP_STFIWX: return "STFIWX";

	case PPCASM_OP_PS_MERGE00: return "PS_MERGE00";
	case PPCASM_OP_PS_MERGE01: return "PS_MERGE01";
	case PPCASM_OP_PS_MERGE10: return "PS_MERGE10";
	case PPCASM_OP_PS_MERGE11: return "PS_MERGE11";
	case PPCASM_OP_PS_ADD: return "PS_ADD";
	case PPCASM_OP_PS_SUB: return "PS_SUB";
	case PPCASM_OP_PS_DIV: return "PS_DIV";
	case PPCASM_OP_PS_MUL: return "PS_MUL";
	case PPCASM_OP_PS_MADD: return "PS_MADD";
	case PPCASM_OP_PS_MSUB: return "PS_MSUB";
	case PPCASM_OP_PS_NMADD: return "PS_NMADD";
	case PPCASM_OP_PS_NMSUB: return "PS_NMSUB";

	case PPCASM_OP_FMR: return "FMR";
	case PPCASM_OP_FNEG: return "FNEG";
	case PPCASM_OP_FRSP: return "FRSP";
	case PPCASM_OP_FRSQRTE: return "FRSQRTE";
	case PPCASM_OP_FADD: return "FADD";
	case PPCASM_OP_FADDS: return "FADDS";
	case PPCASM_OP_FSUB: return "FSUB";
	case PPCASM_OP_FSUBS: return "FSUBS";
	case PPCASM_OP_FMUL: return "FMUL";
	case PPCASM_OP_FMULS: return "FMULS";
	case PPCASM_OP_FDIV: return "FDIV";
	case PPCASM_OP_FDIVS: return "FDIVS";

	case PPCASM_OP_FMADD: return "FMADD";
	case PPCASM_OP_FMADDS: return "FMADDS";
	case PPCASM_OP_FNMADD: return "FNMADD";
	case PPCASM_OP_FNMADDS: return "FNMADDS";
	case PPCASM_OP_FMSUB: return "FMSUB";
	case PPCASM_OP_FMSUBS: return "FMSUBS";
	case PPCASM_OP_FNMSUB: return "FNMSUB";
	case PPCASM_OP_FNMSUBS: return "FNMSUBS";

	case PPCASM_OP_FCTIWZ: return "FCTIWZ";

	case PPCASM_OP_FCMPU: return "FCMPU";
	case PPCASM_OP_FCMPO: return "FCMPO";

	case PPCASM_OP_ISYNC: return "ISYNC";

	case PPCASM_OP_NOP: return "NOP";
	case PPCASM_OP_LI: return "LI";
	case PPCASM_OP_LIS: return "LIS";

	case PPCASM_OP_MFLR: return "MFLR";
	case PPCASM_OP_MTLR: return "MTLR";
	case PPCASM_OP_MFCTR: return "MFCTR";
	case PPCASM_OP_MTCTR: return "MTCTR";

	case PPCASM_OP_CROR: return "CROR";
	case PPCASM_OP_CRNOR: return "CRNOR";
	case PPCASM_OP_CRORC: return "CRORC";
	case PPCASM_OP_CRXOR: return "CRXOR";
	case PPCASM_OP_CREQV: return "CREQV";
	case PPCASM_OP_CRAND: return "CRAND";
	case PPCASM_OP_CRNAND: return "CRNAND";
	case PPCASM_OP_CRANDC: return "CRANDC";

	case PPCASM_OP_CRSET: return "CRSET";
	case PPCASM_OP_CRCLR: return "CRCLR";
	case PPCASM_OP_CRMOVE: return "CRMOVE";
	case PPCASM_OP_CRNOT: return "CRNOT";


	default:
		return "UNDEF";
	}
	return "";
}

#define C_MASK_RC		(PPC_OPC_RC)
#define C_MASK_LK		(PPC_OPC_LK)
#define C_MASK_AA		(PPC_OPC_AA)
#define C_MASK_OE		(1<<10)
#define C_MASK_BO		(0x1F<<21)
#define C_MASK_BO_COND	(0x1E<<21) // BO mask for true/false conditions. With hint bit excluded
#define C_MASK_BI_CRBIT	(0x3<<16)
#define C_MASK_BI_CRIDX	(0x7<<18)

#define C_BIT_RC		(PPC_OPC_RC)
#define C_BIT_LK		(PPC_OPC_LK)
#define C_BIT_AA		(PPC_OPC_AA)
#define C_BIT_BO_FALSE	(0x4<<21)	// CR bit not set
#define C_BIT_BO_TRUE	(0xC<<21)	// CR bit set
#define C_BIT_BO_ALWAYS	(0x14<<21)
#define C_BIT_BO_DNZ	(0x10<<21)
#define C_BIT_BO_DZ		(0x12<<21)

#define C_BITS_BI_LT	(0x0<<16)	
#define C_BITS_BI_GT	(0x1<<16)	
#define C_BITS_BI_EQ	(0x2<<16)	
#define C_BITS_BI_SO	(0x3<<16)	

void ppcAssembler_setError(PPCAssemblerInOut* ctx, std::string_view errorMsg);

bool ppcOp_extraCheck_extlwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);

	return MB == 0;
}

bool ppcOp_extraCheck_extrwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	if (ME != 31)
		return false;
	sint8 n = 32 - MB;
	sint8 b = SH - n;
	return b >= 0;
}

bool ppcOp_extraCheck_slwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	return MB == 0 && SH == (31 - ME);
}

bool ppcOp_extraCheck_srwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	return ME == 31 && (32 - SH) == MB;
}

bool ppcOp_extraCheck_clrlwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	return SH == 0 && ME == 31;
}

bool ppcOp_extraCheck_clrrwi(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	return SH == 0 && MB == 0;
}

bool ppcOp_extraCheck_rotlw(uint32 opcode)
{
	sint32 rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
	return MB == 0 && ME == 31;
}

#define FLG_DEFAULT	0
#define FLG_SKIP_OP0 operand0Bit
#define FLG_SKIP_OP1 operand1Bit
#define FLG_SKIP_OP2 operand2Bit
#define FLG_SKIP_OP3 operand3Bit
#define FLG_SKIP_OP4 operand4Bit

#define FLG_SWAP_OP0_OP1	(1<<6)	// todo - maybe this should be implemented as a fully configurable matrix of indices instead of predefined constants?
#define FLG_SWAP_OP1_OP2	(1<<7)
#define FLG_SWAP_OP2_OP3	(1<<8)

#define FLG_UNSIGNED_IMM	(1<<10) // always consider immediate unsigned but still allow signed values when assembling

class EncodedOperand_None
{
public:
	EncodedOperand_None() {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index < assemblerCtx->listOperandStr.size() && !assemblerCtx->listOperandStr[index].str.empty())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Too many operands");
			return false;
		}
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{

	}
};

static int _parseRegIndex(std::string_view sv, const char* prefix)
{
	while (*prefix)
	{
		if (sv.empty())
			return -1;
		char c = sv[0];
		if (c >= 'A' && c <= 'Z')
			c -= ('A' - 'a');
		if (c != *prefix)
			return -1;
		sv.remove_prefix(1);
		prefix++;
	}
	int r = 0;
	const std::from_chars_result result = std::from_chars(sv.data(), sv.data() + sv.size(), r);
	if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range)
		return -1;
	if (result.ptr != sv.data() + sv.size())
		return -1;
	return r;
}

template<bool TAllowEAZero = false> // if true, "r0" can be substituted with "0"
class EncodedOperand_GPR
{
public:
	EncodedOperand_GPR(uint8 bitPos) : m_bitPos(bitPos) {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		auto operandStr = assemblerCtx->listOperandStr[index].str;
		if constexpr (TAllowEAZero)
		{
			if (operandStr.size() == 1 && operandStr[0] == '0')
			{
				opcode &= ~((uint32)0x1F << m_bitPos);
				opcode |= ((uint32)0 << m_bitPos);
				return true;
			}
		}
		sint32 regIndex = _parseRegIndex(operandStr, "r");
		if (regIndex < 0 || regIndex >= 32)
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("Operand \"{}\" is not a valid GPR (expected r0 - r31)", assemblerCtx->listOperandStr[index].str));
			return false;
		}
		opcode &= ~((uint32)0x1F << m_bitPos);
		opcode |= ((uint32)regIndex << m_bitPos);
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 regIndex = (opcode >> m_bitPos) & 0x1F;
		disInstr->operandMask |= (1 << index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_GPR;
		disInstr->operand[index].registerIndex = regIndex;
	}
private:
	uint8 m_bitPos;
};

class EncodedOperand_FPR
{
public:
	EncodedOperand_FPR(uint8 bitPos) : m_bitPos(bitPos) {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		sint32 regIndex = _parseRegIndex(assemblerCtx->listOperandStr[index].str, "f");
		if (regIndex < 0 || regIndex >= 32)
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("Operand \"{}\" is not a valid FPR (expected f0 - f31)", assemblerCtx->listOperandStr[index].str));
			return false;
		}
		opcode &= ~((uint32)0x1F << m_bitPos);
		opcode |= ((uint32)regIndex << m_bitPos);
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 regIndex = (opcode >> m_bitPos) & 0x1F;
		disInstr->operandMask |= (1<<index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_FPR;
		disInstr->operand[index].registerIndex = regIndex;
	}
private:
	uint8 m_bitPos;
};

class EncodedOperand_SPR
{
public:
	EncodedOperand_SPR() {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		sint32 regIndex = _parseRegIndex(assemblerCtx->listOperandStr[index].str, "spr");
		if (regIndex < 0 || regIndex >= 1024)
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("Operand \"{}\" is not a valid GPR (expected spr0 - spr1023)", assemblerCtx->listOperandStr[index].str));
			return false;
		}
		sint32 sprLow = (regIndex) & 0x1F;
		sint32 sprHigh = (regIndex>>5) & 0x1F;
		opcode &= ~((uint32)0x1F << 16);
		opcode |= ((uint32)sprLow << 16);
		opcode &= ~((uint32)0x1F << 11);
		opcode |= ((uint32)sprHigh << 11);
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 sprLow = (opcode >> 16) & 0x1F;
		uint32 sprHigh = (opcode >> 11) & 0x1F;
		disInstr->operandMask |= (1 << index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_SPR;
		disInstr->operand[index].registerIndex = sprLow | (sprHigh << 5);
	}
};

class EncodedOperand_MemLoc
{
public:
	EncodedOperand_MemLoc()  {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		std::basic_string_view<char> svOpText(assemblerCtx->listOperandStr[index].str);
		// first we parse the memory register part at the end
		auto startPtr = svOpText.data();
		auto endPtr = startPtr + svOpText.size();
		// trim whitespaces
		while (endPtr > startPtr)
		{
			const char c = endPtr[-1];
			if (c != ' ' && c != '\t')
				break;
			endPtr--;
		}
		if (endPtr == startPtr || endPtr[-1] != ')')
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("\'{}\' does not end with valid memory register syntax. Memory operand must have the form offset(gpr). Example: 0x20(r3)", svOpText));
			return false;
		}
		endPtr--;
		// find parenthesis open
		auto memoryRegEnd = endPtr;
		const char* memoryRegBegin = nullptr;
		while (endPtr > startPtr)
		{
			const char c = endPtr[-1];
			if (c == '(')
			{
				memoryRegBegin = endPtr;
				endPtr--; // move end pointer beyond the parenthesis
				break;
			}
			endPtr--;
		}
		if (memoryRegBegin == nullptr)
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("\'{}\' does not end with valid memory register syntax. Memory operand must have the form offset(gpr). Example: 0x20(r3)", svOpText));
			return false;
		}
		std::string_view svExpressionPart(startPtr, endPtr - startPtr);
		std::string_view svRegPart(memoryRegBegin, memoryRegEnd - memoryRegBegin);
		sint32 memGpr = _parseRegIndex(svRegPart, "r");
		//if (_ppcAssembler_parseRegister(svRegPart, "r", memGpr) == false || (memGpr < 0 || memGpr >= 32))
		//{
		//	sprintf(_assemblerErrorMessageDepr, "\'%.*s\' is not a valid GPR", (int)(memoryRegEnd - memoryRegBegin), memoryRegBegin);
		//	ppcAssembler_setError(internalCtx.ctx, _assemblerErrorMessageDepr);
		//	return false;
		//}
		if (memGpr < 0 || memGpr >= 32)
		{
			ppcAssembler_setError(assemblerCtx->ctx, fmt::format("Memory operand register \"{}\" is not a valid GPR (expected r0 - r31)", svRegPart));
			return false;
		}
		opcode &= ~(0x1F << 16);
		opcode |= (memGpr << 16);
		// parse expression
		ExpressionParser ep;
		double immD = 0.0f;
		try
		{
			if (ep.IsConstantExpression(svExpressionPart))
			{
				immD = ep.Evaluate(svExpressionPart);
				sint32 imm = (sint32)immD;
				if (imm < -32768 || imm > 32767)
				{
					std::string msg = fmt::format("\"{}\" evaluates to offset out of range (Valid range is -32768 to 32767)", svExpressionPart);
					ppcAssembler_setError(assemblerCtx->ctx, msg);
					return false;
				}
				opcode &= ~0xFFFF;
				opcode |= ((uint32)imm & 0xFFFF);
			}
			else
			{
				assemblerCtx->ctx->list_relocs.emplace_back(PPCASM_RELOC::U32_MASKED_IMM, std::string(svExpressionPart), 0, 0, 16);
				return true;
			}
		}
		catch (std::exception* e)
		{
			std::string msg = fmt::format("\"{}\" could not be evaluated. Error: {}", svExpressionPart, e->what());
			ppcAssembler_setError(assemblerCtx->ctx, msg);
			return false;
		}
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 imm = (opcode & 0xFFFF);
		if (imm & 0x8000)
			imm |= 0xFFFF0000;
		uint32 regIndex = (opcode >> 16) & 0x1F;
		disInstr->operandMask |= (1 << index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_MEM;
		disInstr->operand[index].registerIndex = regIndex;
		disInstr->operand[index].immS32 = (sint32)imm;
		disInstr->operand[index].immWidth = 16;
		disInstr->operand[index].isSignedImm = true;
	}
};

bool _CanStoreInteger(uint32 value, uint32 numBits, bool isSigned)
{
	if (isSigned)
	{
		sint32 storedValue = (sint32)value;
		storedValue <<= (32 - numBits);
		storedValue >>= (32 - numBits);
		return (uint32)storedValue == value;
	}
	// unsigned
	uint32 storedValue = value;
	storedValue <<= (32 - numBits);
	storedValue >>= (32 - numBits);
	return storedValue == value;
}

class EncodedOperand_IMM
{
public:
	EncodedOperand_IMM(uint8 bitPos, uint8 bits, bool isSigned, bool negate = false, bool extendedRange = false) : m_bitPos(bitPos), m_bits(bits), m_isSigned(isSigned), m_negate(negate), m_extendedRange(extendedRange) {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		// parse expression
		std::string expressionString(assemblerCtx->listOperandStr[index].str);
		if (m_negate)
		{
			expressionString.insert(0, "0-(");
			expressionString.append(")");
		}
		ExpressionParser ep;
		double immD = 0.0f;
		try
		{
			if (ep.IsConstantExpression(expressionString))
			{
				immD = ep.Evaluate(expressionString);
			}
			else
			{
				assemblerCtx->ctx->list_relocs.emplace_back(PPCASM_RELOC::U32_MASKED_IMM, expressionString, 0, m_bitPos, m_bits);
				return true;
			}
		}
		catch (std::exception* e)
		{
			// check if expression is invalid or if it contains unknown constants
			std::string msg = fmt::format("\"{}\" could not be evaluated. Error: {}", expressionString.c_str(), e->what());
			ppcAssembler_setError(assemblerCtx->ctx, msg);
			return false;
		}
		uint32 imm = (uint32)(sint32)immD;
		bool canStore = _CanStoreInteger(imm, m_bits, m_isSigned);
		if(!canStore && m_extendedRange) // always allow unsigned
			canStore = _CanStoreInteger(imm, m_bits, false);
		if (!canStore)
		{
			std::string msg = fmt::format("Value of operand \"{}\" is out of range", assemblerCtx->listOperandStr[index].str);
			ppcAssembler_setError(assemblerCtx->ctx, msg);
			return false;
		}
		uint32 mask = (1<<m_bits)-1;
		imm &= mask;
		opcode &= ~(mask << m_bitPos);
		opcode |= (imm << m_bitPos);
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 mask = (1 << m_bits) - 1;
		uint32 immValue = (opcode >> m_bitPos) & mask;
		if (m_isSigned)
		{
			sint32 tmpValue = (sint32)immValue;
			tmpValue <<= (32 - m_bits);
			tmpValue >>= (32 - m_bits);
			immValue = (uint32)tmpValue;
		}
		disInstr->operandMask |= (1 << index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_IMM;
		disInstr->operand[index].immS32 = immValue;
		disInstr->operand[index].immWidth = m_bits;
		disInstr->operand[index].isSignedImm = m_isSigned;
	}
private:
	uint8 m_bitPos;
	uint8 m_bits;
	bool m_isSigned;
	bool m_negate;
	bool m_extendedRange;
};

class EncodedOperand_U5Reverse
{
public:
	EncodedOperand_U5Reverse(uint8 bitPos, uint8 base) : m_bitPos(bitPos), m_base(base) {}

	bool AssembleOperand(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode, size_t index)
	{
		if (index >= assemblerCtx->listOperandStr.size())
		{
			ppcAssembler_setError(assemblerCtx->ctx, "Missing operand");
			return false;
		}
		// parse expression
		std::string expressionString(assemblerCtx->listOperandStr[index].str);
		expressionString.insert(0, fmt::format("{}-(", m_base));
		expressionString.append(")");
		ExpressionParser ep;
		double immD = 0.0f;
		try
		{
			if (ep.IsConstantExpression(expressionString))
			{
				immD = ep.Evaluate(expressionString);
			}
			else
			{
				assemblerCtx->ctx->list_relocs.emplace_back(PPCASM_RELOC::U32_MASKED_IMM, expressionString, 0, m_bitPos, 5);
				return true;
			}
		}
		catch (std::exception* e)
		{
			// check if expression is invalid or if it contains unknown constants
			std::string msg = fmt::format("\"{}\" could not be evaluated. Error: {}", expressionString.c_str(), e->what());
			ppcAssembler_setError(assemblerCtx->ctx, msg);
			return false;
		}
		uint32 imm = (uint32)(sint32)immD;
		bool canStore = _CanStoreInteger(imm, 5, false);
		if (!canStore)
		{
			std::string msg = fmt::format("Value of operand \"{}\" is out of range", assemblerCtx->listOperandStr[index].str);
			ppcAssembler_setError(assemblerCtx->ctx, msg);
			return false;
		}
		uint32 mask = (1 << 5) - 1;
		imm &= mask;
		opcode &= ~(mask << m_bitPos);
		opcode |= (imm << m_bitPos);
		return true;
	}

	void DisassembleOperand(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode, size_t index)
	{
		uint32 mask = (1 << 5) - 1;
		uint32 immValue = (opcode >> m_bitPos) & mask;
		immValue = m_base - immValue;
		disInstr->operandMask |= (1 << index);
		disInstr->operand[index].type = PPCASM_OPERAND_TYPE_IMM;
		disInstr->operand[index].immS32 = immValue;
		disInstr->operand[index].immWidth = 5;
		disInstr->operand[index].isSignedImm = false;
	}
private:
	uint8 m_bitPos;
	uint8 m_base;
};

class EncodedConstraint_None
{
public:
	EncodedConstraint_None() {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		return true;
	}
};

class EncodedConstraint_MirrorU5
{
public:
	EncodedConstraint_MirrorU5(uint8 srcBitPos, uint8 dstBitPos) : m_srcBitPos(srcBitPos), m_dstBitPos(dstBitPos) {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
		uint32 regIndex = (opcode >> m_srcBitPos) & 0x1F;
		opcode &= ~((uint32)0x1F << m_dstBitPos);
		opcode |= ((uint32)regIndex << m_dstBitPos);
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		uint32 regSrc = (opcode >> m_srcBitPos) & 0x1F;
		uint32 regDst = (opcode >> m_dstBitPos) & 0x1F;
		return regSrc == regDst;
	}

private:
	const uint8 m_srcBitPos, m_dstBitPos;
};

// same as _MirrorU5, but the destination must match invBase - src
class EncodedConstraint_MirrorReverseU5
{
public:
	EncodedConstraint_MirrorReverseU5(uint8 srcBitPos, uint8 dstBitPos, uint8 invBase) : m_srcBitPos(srcBitPos), m_dstBitPos(dstBitPos), m_invBase(invBase) {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
		uint32 regIndex = (opcode >> m_srcBitPos) & 0x1F;
		regIndex = m_invBase - regIndex;
		opcode &= ~((uint32)0x1F << m_dstBitPos);
		opcode |= ((uint32)regIndex << m_dstBitPos);
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		uint32 regSrc = (opcode >> m_srcBitPos) & 0x1F;
		uint32 regDst = (opcode >> m_dstBitPos) & 0x1F;
		regSrc = m_invBase - regSrc;
		if (regSrc >= 32)
			return false;
		return regSrc == regDst;
	}

private:
	const uint8 m_srcBitPos, m_dstBitPos, m_invBase;
};

class EncodedConstraint_FixedU5
{
public:
	EncodedConstraint_FixedU5(uint8 bitPos, uint8 expectedReg) : m_bitPos(bitPos), m_expectedReg(expectedReg) {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
		uint32 regIndex = m_expectedReg;
		opcode &= ~((uint32)0x1F << m_bitPos);
		opcode |= ((uint32)regIndex << m_bitPos);
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		uint32 reg = (opcode >> m_bitPos) & 0x1F;
		return (uint8)reg == m_expectedReg;
	}

private:
	const uint8 m_bitPos;
	const uint8 m_expectedReg;
};

using EncodedConstraint_FixedRegister = EncodedConstraint_FixedU5;
using EncodedConstraint_MirrorRegister = EncodedConstraint_MirrorU5;

// checks bit value, but does not overwrite it on assemble
class EncodedConstraint_CheckSignBit
{
public:
	EncodedConstraint_CheckSignBit(uint8 bitPos, uint8 expectedValue) : m_bitPos(bitPos), m_expectedValue(expectedValue) {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
		// dont overwrite the existing sign bit
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		return ((opcode >> m_bitPos) & 1) == m_expectedValue;
	}

private:
	const uint8 m_bitPos;
	const uint8 m_expectedValue;
};

//class EncodedConstraint_ExpectBit
//{
//public:
//	EncodedConstraint_ExpectBit(uint8 bitPos, bool val) : m_bitPos(bitPos), m_expectedValue(val ? 1 : 0) {}
//
//	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
//	{
//		if (m_expectedValue)
//			opcode |= (1 << m_bitPos);
//		else
//			opcode &= ~(1 << m_bitPos);
//	}
//
//	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
//	{
//		return ((opcode >> m_bitPos) & 1) == m_expectedValue;
//	}
//
//private:
//	const uint8 m_bitPos;
//	const uint8 m_expectedValue;
//};

class EncodedConstraint_FixedSPR
{
public:
	EncodedConstraint_FixedSPR(uint16 sprIndex) : m_sprIndex(sprIndex) {}

	void AssembleConstraint(PPCAssemblerContext* assemblerCtx, PPCInstructionDef* iDef, uint32& opcode)
	{
		sint32 sprLow = (m_sprIndex) & 0x1F;
		sint32 sprHigh = (m_sprIndex >> 5) & 0x1F;
		opcode &= ~((uint32)0x1F << 16);
		opcode |= ((uint32)sprLow << 16);
		opcode &= ~((uint32)0x1F << 11);
		opcode |= ((uint32)sprHigh << 11);
	}

	bool DisassembleCheckConstraint(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, const uint32 opcode)
	{
		uint32 sprLow = (opcode >> 16) & 0x1F;
		uint32 sprHigh = (opcode >> 11) & 0x1F;
		uint32 sprIndex = sprLow | (sprHigh << 5);
		return (uint16)sprIndex == m_sprIndex;
	}

private:
	const uint16 m_sprIndex;
};

struct PPCInstructionDef
{
	uint16 ppcAsmOp; // instruction type identifier
	uint8 priority;
	uint16 opc0;
	uint16 opc1;
	uint16 opc2;
	uint8 instructionForm; // encoding
	uint16 flags;
	uint32 maskBits;
	uint32 compareBits;
	bool(*extraCheck)(uint32 opcode); // used for unique criteria (e.g. SRWI checks SH/mask) -> Replaced by constraints

	std::array<std::variant<EncodedOperand_None, EncodedOperand_GPR<false>, EncodedOperand_GPR<true>, EncodedOperand_FPR, EncodedOperand_SPR, EncodedOperand_IMM, EncodedOperand_U5Reverse, EncodedOperand_MemLoc>, 4> encodedOperands{}; // note: The default constructor of std::variant will default-construct the first type (which we want to be EncodedOperand_None)
	std::array<std::variant<EncodedConstraint_None, EncodedConstraint_MirrorRegister, EncodedConstraint_MirrorReverseU5, EncodedConstraint_FixedRegister, EncodedConstraint_FixedSPR, EncodedConstraint_CheckSignBit>, 3> constraints{};
};

PPCInstructionDef ppcInstructionTable[] =
{
	{PPCASM_OP_PS_MERGE00, 0, 4, 16, 16, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_MERGE01, 0, 4, 16, 17, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_MERGE10, 0, 4, 16, 18, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_MERGE11, 0, 4, 16, 19, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},

	{PPCASM_OP_PS_DIV, 0, 4, 18, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_SUB, 0, 4, 20, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_ADD, 0, 4, 21, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_MUL, 0, 4, 25, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6)}},

	{PPCASM_OP_PS_MSUB, 0, 4, 28, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_MADD, 0, 4, 29, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_NMSUB, 0, 4, 30, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11)}},
	{PPCASM_OP_PS_NMADD, 0, 4, 31, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11)}},

	{PPCASM_OP_MULLI, 0, 7, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true)}},

	{PPCASM_OP_SUBFIC, 0, 8, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true)} },

	{PPCASM_OP_CMPLWI, 0, 10, OPC_NONE, OPC_NONE, OP_FORM_CMP_SIMM, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CMPWI, 0, 11, OPC_NONE, OPC_NONE, OP_FORM_CMP_SIMM, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_ADDIC, 0, 12, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true)}},
	{PPCASM_OP_ADDIC_, 0, 13, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true)}},
	{PPCASM_OP_ADDI, 0, 14, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true)}},
	{PPCASM_OP_SUBI, 1, 14, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true, true)}, {EncodedConstraint_CheckSignBit(15, 1)}}, // special form of ADDI for negative immediate
	{PPCASM_OP_LI, 1, 14, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, true, false, true)}, {EncodedConstraint_FixedRegister(16, 0)}},
	{PPCASM_OP_ADDIS, 0, 15, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(0, 16, true, false, true)}},
	{PPCASM_OP_LIS, 1, 15, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, true, false, true)}, {EncodedConstraint_FixedRegister(16, 0)}},

	{PPCASM_OP_BC, 0, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, 0, 0, nullptr},

	{PPCASM_OP_BNE, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_FALSE | C_BITS_BI_EQ, nullptr},
	{PPCASM_OP_BEQ, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_TRUE | C_BITS_BI_EQ, nullptr},
	{PPCASM_OP_BGE, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_FALSE | C_BITS_BI_LT, nullptr},
	{PPCASM_OP_BGT, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_TRUE | C_BITS_BI_GT, nullptr},
	{PPCASM_OP_BLT, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_TRUE | C_BITS_BI_LT, nullptr},
	{PPCASM_OP_BLE, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO_COND | C_MASK_BI_CRBIT, C_BIT_BO_FALSE | C_BITS_BI_GT, nullptr},

	{PPCASM_OP_BDZ, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO, C_BIT_BO_DZ, nullptr},
	{PPCASM_OP_BDNZ, 1, 16, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S16, FLG_DEFAULT, C_MASK_BO, C_BIT_BO_DNZ, nullptr},

	{PPCASM_OP_B, 0, 18, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S24, FLG_DEFAULT, C_MASK_LK | C_MASK_AA, 0, nullptr},
	{PPCASM_OP_BL, 0, 18, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S24, FLG_DEFAULT, C_MASK_LK | C_MASK_AA, C_BIT_LK, nullptr},
	{PPCASM_OP_BA, 0, 18, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S24, FLG_DEFAULT, C_MASK_LK | C_MASK_AA, C_BIT_AA, nullptr},
	{PPCASM_OP_BLA, 0, 18, OPC_NONE, OPC_NONE, OP_FORM_BRANCH_S24, FLG_DEFAULT, C_MASK_LK | C_MASK_AA, C_BIT_LK|C_BIT_AA, nullptr},

	{PPCASM_OP_BLR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_LK, C_BIT_BO_ALWAYS, nullptr},

	{PPCASM_OP_BLTLR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_TRUE | C_BITS_BI_LT, nullptr}, // less
	{PPCASM_OP_BGTLR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_TRUE | C_BITS_BI_GT, nullptr}, // greater
	{PPCASM_OP_BEQLR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_TRUE | C_BITS_BI_EQ, nullptr}, // equal
	{PPCASM_OP_BLELR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_FALSE | C_BITS_BI_GT, nullptr}, // less or equal (not greater)
	{PPCASM_OP_BGELR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_FALSE | C_BITS_BI_LT, nullptr}, // greater or equal (not less)
	{PPCASM_OP_BNELR, 0, 19, 16, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_BI_CRBIT | C_MASK_LK, C_BIT_BO_FALSE | C_BITS_BI_EQ, nullptr}, // not equal

	{PPCASM_OP_ISYNC, 0, 19, 150, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, 0, 0, nullptr},

	{PPCASM_OP_CRNOR, 0, 19, 33, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CRANDC, 0, 19, 129, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CRXOR, 0, 19, 193, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CRNAND, 0, 19, 255, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CRAND, 0, 19, 257, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CREQV, 0, 19, 289, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CRORC, 0, 19, 417, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},
	{PPCASM_OP_CROR, 0, 19, 449, OPC_NONE, OP_FORM_XL_CR, FLG_DEFAULT, 0, 0, nullptr},

	{PPCASM_OP_BCTR, 0, 19, 528, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_LK, C_BIT_BO_ALWAYS, nullptr},
	{PPCASM_OP_BCTRL, 0, 19, 528, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, C_MASK_BO | C_MASK_LK, C_BIT_BO_ALWAYS | C_BIT_LK, nullptr},

	{PPCASM_OP_RLWIMI, 0, 20, OPC_NONE, OPC_NONE, OP_FORM_RLWINM, FLG_DEFAULT, C_MASK_RC, 0, nullptr},
	{PPCASM_OP_RLWIMI_, 0, 20, OPC_NONE, OPC_NONE, OP_FORM_RLWINM, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr},
	{PPCASM_OP_RLWINM, 0, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM, FLG_DEFAULT, C_MASK_RC, 0, nullptr},
	{PPCASM_OP_RLWINM_, 0, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr},

	{PPCASM_OP_ROTLWI, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(11, 5, false)}, {EncodedConstraint_FixedU5(6, 0), EncodedConstraint_FixedU5(1, 31)}},
	{PPCASM_OP_ROTLWI_, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(11, 5, false)}, {EncodedConstraint_FixedU5(6, 0), EncodedConstraint_FixedU5(1, 31)}},

	// rotrwi RA, RS, n -> rlwinm RA, RS, 32-n, 0, 31
	// only assembler
	{PPCASM_OP_ROTRWI, 0, 21, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_U5Reverse(11, 32)}, {EncodedConstraint_FixedU5(6, 0), EncodedConstraint_FixedU5(1, 31)}},
	{PPCASM_OP_ROTRWI_, 0, 21, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_U5Reverse(11, 32)}, {EncodedConstraint_FixedU5(6, 0), EncodedConstraint_FixedU5(1, 31)}},

	{PPCASM_OP_EXTLWI, 1, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_extlwi},
	{PPCASM_OP_EXTLWI_, 1, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_extlwi},
	{PPCASM_OP_EXTRWI, 1, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_extrwi},
	{PPCASM_OP_EXTRWI_, 1, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_extrwi},

	{PPCASM_OP_SLWI, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_slwi},
	{PPCASM_OP_SLWI_, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_slwi},
	{PPCASM_OP_SRWI, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_srwi},
	{PPCASM_OP_SRWI_, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_srwi},

	{PPCASM_OP_CLRLWI, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_clrlwi},
	{PPCASM_OP_CLRLWI_, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_clrlwi},
	{PPCASM_OP_CLRRWI, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_clrrwi},
	{PPCASM_OP_CLRRWI_, 2, 21, OPC_NONE, OPC_NONE, OP_FORM_RLWINM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_clrrwi},

	{PPCASM_OP_RLWNM, 0, 23, OPC_NONE, OPC_NONE, OP_FORM_RLWNM, FLG_DEFAULT, C_MASK_RC, 0, nullptr},
	{PPCASM_OP_RLWNM_, 0, 23, OPC_NONE, OPC_NONE, OP_FORM_RLWNM, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr},

	{PPCASM_OP_ROTLW, 0, 23, OPC_NONE, OPC_NONE, OP_FORM_RLWNM_EXTENDED, FLG_DEFAULT, C_MASK_RC, 0, ppcOp_extraCheck_rotlw},
	{PPCASM_OP_ROTLW_, 0, 23, OPC_NONE, OPC_NONE, OP_FORM_RLWNM_EXTENDED, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, ppcOp_extraCheck_rotlw},


	{PPCASM_OP_ORI, 0, 24, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, false)}},
	{PPCASM_OP_NOP, 1, 24, OPC_NONE, OPC_NONE, OP_FORM_NO_OPERAND, FLG_DEFAULT, 0x3FFFFFF, 0, nullptr}, // ORI r0, r0, 0 -> NOP
	{PPCASM_OP_ORIS, 0, 25, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, true, false, true)}},

	{PPCASM_OP_XORI, 0, 26, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, false)} },
	{PPCASM_OP_XORIS, 0, 27, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, true, false, true)} },

	{PPCASM_OP_ANDI_, 0, 28, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, false)} },
	{PPCASM_OP_ANDIS_, 0, 29, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_IMM(0, 16, true, false, true)} },

	// group 31
	{PPCASM_OP_CMPW, 0, 31, 0, OPC_NONE, OP_FORM_OP3_A_CMP, FLG_DEFAULT, 0, 0, nullptr},

	{PPCASM_OP_SUBFC, 1, 31, 8, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SUBFC_, 1, 31, 8, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_MULHWU, 0, 31, 11, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_MULHWU_, 0, 31, 11, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_CMPLW, 0, 31, 32, OPC_NONE, OP_FORM_OP3_A_CMP, FLG_DEFAULT, 0, 0, nullptr},

	{PPCASM_OP_SUBF, 1, 31, 40, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SUBF_, 1, 31, 40, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SUB, 0, 31, 40, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(11), EncodedOperand_GPR(16)} },
	{PPCASM_OP_SUB_, 0, 31, 40, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(11), EncodedOperand_GPR(16)} },

	{PPCASM_OP_MULHW, 0, 31, 75, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_MULHW_, 0, 31, 75, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_NEG, 0, 31, 104, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC | C_MASK_OE, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16)}},
	{PPCASM_OP_NEG_, 0, 31, 104, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC | C_MASK_OE, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16)} },

	{PPCASM_OP_NOR, 0, 31, 124, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_NOR_, 0, 31, 124, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },

	// alias for NOR where rA == rB
	{PPCASM_OP_NOT, 1, 31, 124, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(11)}, {EncodedConstraint_MirrorRegister(11, 21)}},
	{PPCASM_OP_NOT_, 1, 31, 124, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(11)}, {EncodedConstraint_MirrorRegister(11, 21)} },

	{PPCASM_OP_SUBFE, 1, 31, 136, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SUBFE_, 1, 31, 136, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_MULLW, 0, 31, 235, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_MULLW_, 0, 31, 235, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_MFSPR, 0, 31, 339, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_SPR()} },
	{PPCASM_OP_MTSPR, 0, 31, 467, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_SPR(), EncodedOperand_GPR(21)} },

	{PPCASM_OP_MFLR, 0, 31, 339, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21)}, {EncodedConstraint_FixedSPR(8)}},
	{PPCASM_OP_MTLR, 0, 31, 467, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21)}, {EncodedConstraint_FixedSPR(8)} },
	{PPCASM_OP_MFCTR, 0, 31, 339, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21)}, {EncodedConstraint_FixedSPR(9)} },
	{PPCASM_OP_MTCTR, 0, 31, 467, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21)}, {EncodedConstraint_FixedSPR(9)} },

	{PPCASM_OP_ADD, 0, 31, 266, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_ADD_, 0, 31, 266, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_SLW, 0, 31, 24, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SLW_, 0, 31, 24, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SRW, 0, 31, 536, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SRW_, 0, 31, 536, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SRAW, 0, 31, 792, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_SRAW_, 0, 31, 792, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },

	{PPCASM_OP_AND, 0, 31, 28, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_AND_, 0, 31, 28, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },

	{PPCASM_OP_ANDC, 0, 31, 60, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_ANDC_, 0, 31, 60, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },

	{PPCASM_OP_XOR, 0, 31, 316, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },

	{PPCASM_OP_ORC, 0, 31, 412, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} }, // has RC?
	{PPCASM_OP_OR, 0, 31, 444, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_OR_, 0, 31, 444, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21), EncodedOperand_GPR(11)} },
	{PPCASM_OP_MR, 1, 31, 444, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(11)}, {EncodedConstraint_MirrorRegister(11, 21)} },
	{PPCASM_OP_MR_, 1, 31, 444, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(11)}, {EncodedConstraint_MirrorRegister(11, 21)} },

	{PPCASM_OP_DIVWU, 0, 31, 459, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_DIVWU_, 0, 31, 459, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_DIVW, 0, 31, 491, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_DIVW_, 0, 31, 491, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_SRAWI, 0, 31, 824, OPC_NONE, OP_FORM_OP3_A_IMM, FLG_DEFAULT, C_MASK_RC, 0, nullptr},
	{PPCASM_OP_SRAWI_, 0, 31, 824, OPC_NONE, OP_FORM_OP3_A_IMM, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr},

	{PPCASM_OP_CNTLZW, 0, 31, 26, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },
	{PPCASM_OP_EXTSB, 0, 31, 954, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },
	{PPCASM_OP_EXTSH, 0, 31, 922, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, 0, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },

	{PPCASM_OP_CNTLZW_, 0, 31, 26, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },
	{PPCASM_OP_EXTSB_, 0, 31, 954, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },
	{PPCASM_OP_EXTSH_, 0, 31, 922, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(16), EncodedOperand_GPR(21)} },

	{PPCASM_OP_LWZX, 0, 31, 23, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LWZUX, 0, 31, 55, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LBZX, 0, 31, 87, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LBZUX, 0, 31, 119, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LHZX, 0, 31, 279, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LHZUX, 0, 31, 311, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LHAX, 0, 31, 343, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LHAUX, 0, 31, 375, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STWX, 0, 31, 151, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STWUX, 0, 31, 183, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STHX, 0, 31, 407, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STHUX, 0, 31, 439, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STBX, 0, 31, 215, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STBUX, 0, 31, 247, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STWBRX, 0, 31, 662, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STHBRX, 0, 31, 918, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_LFSX, 0, 31, 535, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LFSUX, 0, 31, 567, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STFSX, 0, 31, 663, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STFSUX, 0, 31, 695, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_STSWI, 0, 31, 725, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR(16), EncodedOperand_IMM(11, 5, false)} },

	{PPCASM_OP_LWARX, 0, 31, 20, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STWCX_, 0, 31, 150, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, C_MASK_RC, C_BIT_RC, nullptr, {EncodedOperand_GPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_LFDX, 0, 31, 599, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_LFDUX, 0, 31, 631, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STFDX, 0, 31, 727, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },
	{PPCASM_OP_STFDUX, 0, 31, 759, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },

	{PPCASM_OP_STFIWX, 0, 31, 983, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_GPR<true>(16), EncodedOperand_GPR(11)} },

	// load/store
	{PPCASM_OP_LWZ, 0, 32, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LWZU, 0, 33, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LBZ, 0, 34, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LBZU, 0, 35, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STW, 0, 36, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STWU, 0, 37, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STB, 0, 38, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STBU, 0, 39, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LHZ, 0, 40, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LHZU, 0, 41, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LHA, 0, 42, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LHAU, 0, 43, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STH, 0, 44, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STHU, 0, 45, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LMW, 0, 46, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STMW, 0, 47, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_GPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LFS, 0, 48, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LFSU, 0, 49, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LFD, 0, 50, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_LFDU, 0, 51, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STFS, 0, 52, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STFSU, 0, 53, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STFD, 0, 54, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },
	{PPCASM_OP_STFDU, 0, 55, OPC_NONE, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_MemLoc()} },

	// FP
	{PPCASM_OP_FDIVS, 0, 59, 18, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FSUBS, 0, 59, 20, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FADDS, 0, 59, 21, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMULS, 0, 59, 25, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6) } },
	{PPCASM_OP_FMSUBS, 0, 59, 28, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMADDS, 0, 59, 29, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FNMSUBS, 0, 59, 30, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FNMADDS, 0, 59, 31, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },

	{PPCASM_OP_FCMPU, 0, 63, 0, OPC_NONE, OP_FORM_X_FP_CMP, FLG_DEFAULT, 0, 0, nullptr },
	{PPCASM_OP_FCTIWZ, 0, 63, 15, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, {EncodedOperand_FPR(21), EncodedOperand_FPR(11)}},
	{PPCASM_OP_FDIV, 0, 63, 18, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FSUB, 0, 63, 20, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FADD, 0, 63, 21, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMUL, 0, 63, 25, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6) } },
	{PPCASM_OP_FRSQRTE, 0, 63, 26, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMSUB, 0, 63, 28, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMADD, 0, 63, 29, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FNMSUB, 0, 63, 30, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FNMADD, 0, 63, 31, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(16), EncodedOperand_FPR(6), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FRSP, 0, 63, 12, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(11) } },

	// 63 extended opcode
	{PPCASM_OP_FCMPO, 0, 63, 32|OPC_EXTENDED_BIT, OPC_NONE, OP_FORM_X_FP_CMP, FLG_DEFAULT, 0, 0, nullptr },
	{PPCASM_OP_FNEG, 0, 63, 40|OPC_EXTENDED_BIT, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(11) } },
	{PPCASM_OP_FMR, 0, 63, 72|OPC_EXTENDED_BIT, OPC_NONE, OP_FORM_DYNAMIC, FLG_DEFAULT, 0, 0, nullptr, { EncodedOperand_FPR(21), EncodedOperand_FPR(11) } },

};

#define opcMask_setBits(__index, __bitCount, __value) opcodeMask |= (((1<<__bitCount)-1)<<(31-__index)); opcodeBits |= ((__value)<<(31-__index));

void ppcAssembler_buildOpcMask(PPCInstructionDef* iDef, uint32& maskOut, uint32& bitsOut)
{
	uint32 opc0 = iDef->opc0;
	uint32 opc1 = iDef->opc1;
	uint32 opc2 = iDef->opc2;

	uint32 opcodeMask = 0x3F<<26;
	uint32 opcodeBits = opc0<<26;

	// handle groups
	if (opc0 == 31)
	{
		// group 31
		opcMask_setBits(30, 10, opc1);
	}
	else if (opc0 == 19)
	{
		// group 19
		opcMask_setBits(30, 10, opc1);
	}
	else if (opc0 == 4)
	{
		// group 4 (paired single)
		opcMask_setBits(30, 5, opc1);
		//opc1 = PPC_getBits(opcode, 30, 5);
		if (opc1 == 16)
		{
			// group 4->16 (ps_merge)
			opcMask_setBits(25, 5, opc2);
			//opc2 = PPC_getBits(opcode, 25, 5);
		}
	}
	else if (opc0 == 59)
	{
		// group 59 (FP float)
		opcMask_setBits(30, 5, opc1);
		//opc1 = PPC_getBits(opcode, 30, 5);
	}
	else if (opc0 == 63)
	{
		// group 63 (FP double)
		if ((opc1&OPC_EXTENDED_BIT) != 0)
		{
			opcMask_setBits(30, 10, (opc1&~OPC_EXTENDED_BIT));
		}
		else
		{
			opcMask_setBits(30, 5, opc1);
		}
	}
	maskOut = opcodeMask;
	bitsOut = opcodeBits;
}

// given an internal instruction operand index, return the text encoding index
sint32 _getOpIndex(PPCInstructionDef* iDef, sint32 operandIndex)
{
	if ((operandIndex == 0 || operandIndex == 1) && (iDef->flags & FLG_SWAP_OP0_OP1) != 0)
		operandIndex ^= 1;
	if ((iDef->flags & FLG_SWAP_OP1_OP2) != 0)
	{
		if (operandIndex == 1) operandIndex = 2;
		else if (operandIndex == 2) operandIndex = 1;
	}
	if ((iDef->flags & FLG_SWAP_OP2_OP3) != 0)
	{
		if (operandIndex == 2) operandIndex = 3;
		else if (operandIndex == 3) operandIndex = 2;
	}
	return operandIndex;
}

// given an internal instruction operand index, return the operand index for the text encoding. Returns -1 when operand is not present
// replaces _getOpIndex
//sint32 _operandInternalToTextIndex(ppcInstructionDef_t* iDef, sint32 operandIndex)
//{
//	if ((operandIndex == 0 || operandIndex == 1) && (iDef->flags & FLG_SWAP_OP0_OP1) != 0)
//		operandIndex ^= 1;
//	if ((operandIndex == 2 || operandIndex == 3) && (iDef->flags & FLG_SWAP_OP2_OP3) != 0)
//		operandIndex ^= 1;
//	sint32 outputIndex = operandIndex;
//	if ((iDef->flags & (1 << outputIndex)))
//		return -1;
//	for (sint32 i = 0; i < operandIndex; i++)
//	{
//		if ((iDef->flags & (1 << i)))
//			outputIndex--;
//	}
//	return outputIndex;
//}
//
//int _operandTextToInternalIndex(ppcInstructionDef_t* iDef, sint32 operandIndex)
//{
//	for (sint32 i = 0; i < 8; i++)
//	{
//		if (_operandInternalToTextIndex(iDef, i) == operandIndex)
//			return i;
//	}
//	return -1;
//}

void _disasmOpGPR(PPCDisassembledInstruction* disInstr, PPCInstructionDef* iDef, uint32 operandIndex, sint32 regIndex)
{
	if ((iDef->flags & (FLG_SKIP_OP0 << operandIndex)) != 0)
		return;
	disInstr->operandMask |= (1 << operandIndex);
	sint32 opIdx = _getOpIndex(iDef, operandIndex);
	disInstr->operand[opIdx].type = PPCASM_OPERAND_TYPE_GPR;
	disInstr->operand[opIdx].registerIndex = regIndex;
}

void ppcAssembler_disassemble(uint32 virtualAddress, uint32 opcode, PPCDisassembledInstruction* disInstr)
{
	auto makeOpCRBit = [&](size_t opIndex, uint8 regIndex)
	{
		disInstr->operandMask |= (1 << opIndex);
		disInstr->operand[opIndex].type = PPCASM_OPERAND_TYPE_CR_BIT;
		disInstr->operand[opIndex].registerIndex = regIndex;
	};

	auto makeOpFPR = [&](size_t opIndex, uint8 regIndex)
	{
		disInstr->operandMask |= (1 << opIndex);
		disInstr->operand[opIndex].type = PPCASM_OPERAND_TYPE_FPR;
		disInstr->operand[opIndex].registerIndex = regIndex;
	};

	memset(disInstr, 0, sizeof(PPCDisassembledInstruction));
	// find match in table
	sint32 bestMatchIndex = -1;
	uint8 bestMatchPriority = 0;
	for (sint32 i = 0; i < sizeof(ppcInstructionTable) / sizeof(PPCInstructionDef); i++)
	{
		PPCInstructionDef* iDef = ppcInstructionTable + i;
		// check opcode
		uint32 opcMask;
		uint32 opcBits;
		ppcAssembler_buildOpcMask(iDef, opcMask, opcBits);
		if ((opcode&opcMask) != opcBits)
			continue;		
		// check bits
		if((opcode&iDef->maskBits) != iDef->compareBits)
			continue;
		// check special condition
		if(iDef->extraCheck && iDef->extraCheck(opcode) == false )
			continue;
		// check priority
		if(iDef->priority < bestMatchPriority)
			continue;
		// check constraints
		bool allConstraintsMatch = true;
		for (auto& it : iDef->constraints)
		{
			if (!std::visit([&](auto&& op) -> bool {
				return op.DisassembleCheckConstraint(disInstr, iDef, opcode);
				}, it))
			{
				allConstraintsMatch = false;
				break;
			}
		}
		if (!allConstraintsMatch)
			continue;
		// select entry
		bestMatchIndex = i;
		bestMatchPriority = iDef->priority;
	}
	// if we have found an entry, parse operand data
	if (bestMatchIndex >= 0)
	{
		PPCInstructionDef* iDef = ppcInstructionTable + bestMatchIndex;
		// setup general instruction info
		disInstr->ppcAsmCode = iDef->ppcAsmOp;
		// parse operands
		if (iDef->instructionForm == OP_FORM_DYNAMIC)
		{
			disInstr->operandMask = 0;
			for (size_t i = 0; i < iDef->encodedOperands.size(); i++)
			{
				std::visit([&](auto&& op) {
					op.DisassembleOperand(disInstr, iDef, opcode, i);
					}, iDef->encodedOperands[i]);
			}
		}
		else if (iDef->instructionForm == OP_FORM_OP3_A_CMP)
		{
			sint32 rS, rA, rB;
			PPC_OPC_TEMPL_X(opcode, rS, rA, rB);
			disInstr->operandMask = (rS!=0?operand0Bit:0);
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CR;
			disInstr->operand[0].registerIndex = rS >> 2;
			if((rS & 3) != 0)
				cemuLog_log(LogType::Force, "[PPC-Disassembler] Unexpected CR encoding for instruction 0x{0:08x}", opcode);
			_disasmOpGPR(disInstr, iDef, 1, rA);
			_disasmOpGPR(disInstr, iDef, 2, rB);
		}
		else if (iDef->instructionForm == OP_FORM_OP3_A_IMM)
		{
			sint32 rS, rA, SH;
			PPC_OPC_TEMPL_X(opcode, rS, rA, SH);
			disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
			sint32 op0Idx = _getOpIndex(iDef, 0);
			sint32 op1Idx = _getOpIndex(iDef, 1);
			sint32 op2Idx = _getOpIndex(iDef, 2);
			// operand 0
			disInstr->operand[op0Idx].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[op0Idx].registerIndex = rA;
			// operand 1
			disInstr->operand[op1Idx].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[op1Idx].registerIndex = rS;
			// operand 2
			disInstr->operand[op2Idx].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[op2Idx].immS32 = SH;
			disInstr->operand[op2Idx].immWidth = 5;
			disInstr->operand[op2Idx].isSignedImm = false;
		}
		else if (iDef->instructionForm == OP_FORM_BRANCH_S16)
		{
			uint32 BO, BI, dest;
			PPC_OPC_TEMPL_B(opcode, BO, BI, dest);
			uint8 crIndex = BI >> 2;
			if ((opcode & PPC_OPC_AA) == 0)
				dest += virtualAddress;
			if (iDef->ppcAsmOp == PPCASM_OP_BC)
			{
				// generic conditional branch of form <BO>, CR+LT/GT/EQ/SO, <dst>
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				// operand 0
				disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CIMM;
				disInstr->operand[0].registerIndex = BO;
				// operand 1
				disInstr->operand[1].type = PPCASM_OPERAND_TYPE_CR_BIT;
				disInstr->operand[1].registerIndex = BI;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_CIMM;
				disInstr->operand[2].immU32 = dest;
				// hint bit
				disInstr->branchHintBitSet = (BO & 1) != 0;
			}
			else
			{
				if (crIndex != 0)
				{
					disInstr->operandMask = operand0Bit | operand1Bit;
					// operand 0
					disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CR;
					disInstr->operand[0].registerIndex = crIndex;
					// operand 1
					disInstr->operand[1].type = PPCASM_OPERAND_TYPE_CIMM;
					disInstr->operand[1].immU32 = dest;
				}
				else
				{
					disInstr->operandMask = operand0Bit;
					disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CIMM;
					disInstr->operand[0].immU32 = dest;
				}
				// hint bit
				disInstr->branchHintBitSet = (BO & 1) != 0;
			}
		}
		else if (iDef->instructionForm == OP_FORM_BRANCH_S24)
		{
			uint32 dest;
			PPC_OPC_TEMPL_I(opcode, dest);
			if ((opcode & PPC_OPC_AA) == 0)
				dest += virtualAddress;
			disInstr->operandMask = operand0Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CIMM;
			disInstr->operand[0].immU32 = dest;
		}
		else if (iDef->instructionForm == OP_FORM_OP2_D_HSIMM)
		{
			sint32 rD, rA;
			uint32 imm;
			PPC_OPC_TEMPL_D_SImm(opcode, rD, rA, imm);
			disInstr->operandMask = operand0Bit | operand1Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[0].registerIndex = rD;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[1].immS32 = imm & 0xFFFF;
			disInstr->operand[1].immWidth = 16;
			disInstr->operand[1].isSignedImm = true;
		}
		else if (iDef->instructionForm == OP_FORM_CMP_SIMM)
		{
			uint32 cr;
			sint32 rA;
			uint32 imm;
			PPC_OPC_TEMPL_D_SImm(opcode, cr, rA, imm);
			cr /= 4;
			// rS is cr
			disInstr->operandMask = operand1Bit | operand2Bit;
			if (cr != 0)
				disInstr->operandMask |= operand0Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CR;
			disInstr->operand[0].registerIndex = cr;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[1].registerIndex = rA;
			// operand 2
			disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[2].immS32 = imm;
			disInstr->operand[2].immWidth = 16;
			disInstr->operand[2].isSignedImm = true;
		}
		else if (iDef->instructionForm == OP_FORM_NO_OPERAND)
		{
			disInstr->operandMask = 0;
		}
		else if (iDef->instructionForm == OP_FORM_X_FP_CMP)
		{
			sint32 rA, crIndex, rB;
			PPC_OPC_TEMPL_X(opcode, crIndex, rA, rB);
			cemu_assert_debug((crIndex % 4) == 0);
			crIndex /= 4;
			disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_CR;
			disInstr->operand[0].registerIndex = crIndex;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_FPR;
			disInstr->operand[1].registerIndex = rA;
			// operand 2
			disInstr->operand[2].type = PPCASM_OPERAND_TYPE_FPR;
			disInstr->operand[2].registerIndex = rB;
		}
		else if (iDef->instructionForm == OP_FORM_RLWINM)
		{
			sint32 rS, rA, SH, MB, ME;
			PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);
			disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit | operand3Bit | operand4Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[0].registerIndex = rA;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[1].registerIndex = rS;
			// operand 2
			disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[2].immS32 = SH;
			disInstr->operand[2].immWidth = 8;
			// operand 3
			disInstr->operand[3].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[3].immS32 = MB;
			disInstr->operand[3].immWidth = 8;
			// operand 4
			disInstr->operand[4].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[4].immS32 = ME;
			disInstr->operand[4].immWidth = 8;
		}
		else if (iDef->instructionForm == OP_FORM_RLWINM_EXTENDED)
		{
			sint32 rS, rA, SH, MB, ME;
			PPC_OPC_TEMPL_M(opcode, rS, rA, SH, MB, ME);

			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[0].registerIndex = rA;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[1].registerIndex = rS;

			if (iDef->ppcAsmOp == PPCASM_OP_EXTLWI || iDef->ppcAsmOp == PPCASM_OP_EXTLWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit | operand3Bit;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = ME + 1;
				disInstr->operand[2].immWidth = 8;
				// operand 3
				disInstr->operand[3].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[3].immS32 = SH;
				disInstr->operand[3].immWidth = 8;
			}		
			else if (iDef->ppcAsmOp == PPCASM_OP_EXTRWI || iDef->ppcAsmOp == PPCASM_OP_EXTRWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit | operand3Bit;
				sint8 n = 32 - MB;
				sint8 b = SH - n;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = n;
				disInstr->operand[2].immWidth = 8;
				// operand 3
				disInstr->operand[3].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[3].immS32 = b;
				disInstr->operand[3].immWidth = 8;
			}
			else if (iDef->ppcAsmOp == PPCASM_OP_SLWI || iDef->ppcAsmOp == PPCASM_OP_SLWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				sint8 n = SH;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = n;
				disInstr->operand[2].immWidth = 8;
			}
			else if (iDef->ppcAsmOp == PPCASM_OP_SRWI || iDef->ppcAsmOp == PPCASM_OP_SRWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				sint8 n = 32 - SH;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = n;
				disInstr->operand[2].immWidth = 8;
			}
			else if (iDef->ppcAsmOp == PPCASM_OP_CLRLWI || iDef->ppcAsmOp == PPCASM_OP_CLRLWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				sint8 n = MB;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = n;
				disInstr->operand[2].immWidth = 8;
			}
			else if (iDef->ppcAsmOp == PPCASM_OP_CLRRWI || iDef->ppcAsmOp == PPCASM_OP_CLRRWI_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				sint8 n = 31 - ME;
				// operand 2
				disInstr->operand[2].type = PPCASM_OPERAND_TYPE_IMM;
				disInstr->operand[2].immS32 = n;
				disInstr->operand[2].immWidth = 8;
			}
			else
			{
				cemu_assert_debug(false);
			}
		}
		else if (iDef->instructionForm == OP_FORM_RLWNM)
		{
			sint32 rS, rA, rB, MB, ME;
			PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
			disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit | operand3Bit | operand4Bit;
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[0].registerIndex = rA;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[1].registerIndex = rS;
			// operand 2
			disInstr->operand[2].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[2].registerIndex = rB;
			// operand 3
			disInstr->operand[3].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[3].immS32 = MB;
			disInstr->operand[3].immWidth = 8;
			// operand 4
			disInstr->operand[4].type = PPCASM_OPERAND_TYPE_IMM;
			disInstr->operand[4].immS32 = ME;
			disInstr->operand[4].immWidth = 8;
		}
		else if (iDef->instructionForm == OP_FORM_RLWNM_EXTENDED)
		{
			sint32 rS, rA, rB, MB, ME;
			PPC_OPC_TEMPL_M(opcode, rS, rA, rB, MB, ME);
			// operand 0
			disInstr->operand[0].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[0].registerIndex = rA;
			// operand 1
			disInstr->operand[1].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[1].registerIndex = rS;
			// operand 2
			disInstr->operand[2].type = PPCASM_OPERAND_TYPE_GPR;
			disInstr->operand[2].registerIndex = rB;

			if (iDef->ppcAsmOp == PPCASM_OP_ROTLW || iDef->ppcAsmOp == PPCASM_OP_ROTLW_)
			{
				disInstr->operandMask = operand0Bit | operand1Bit | operand2Bit;
				// no additional operands displayed
			}
		}
		else if (iDef->instructionForm == OP_FORM_XL_CR)
		{
			PPC_OPC_TEMPL_X_CR();
			// todo - detect and emit simplified mnemonics (e.g. CRSET)
			makeOpCRBit(0, crD);
			makeOpCRBit(1, crA);
			makeOpCRBit(2, crB);
		}
		else
		{
			cemu_assert_debug(false);
		}
		disInstr->operandMask &= ~(iDef->flags & 0x1F);
	}
	else
	{
		// no match
		disInstr->ppcAsmCode = PPCASM_OP_UKN;
	}
}

void ppcAssembler_setError(PPCAssemblerInOut* ctx, char const* errorMsg)
{
	ctx->errorMsg = errorMsg;
}

void ppcAssembler_setError(PPCAssemblerInOut* ctx, std::string_view errorMsg)
{
	ctx->errorMsg = errorMsg;
}

char _assemblerErrorMessageDepr[1024];

bool _ppcAssembler_getOperandTextIndex(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32& textIndex)
{
	// get swapped index
	operandIndex = _getOpIndex(internalCtx.iDef, operandIndex);
	// check if operand is optional
	if ((internalCtx.iDef->flags&(1 << operandIndex)))
	{
		// operand not used
		textIndex = -1;
		return true;
	}
	// get operand text index
	sint32 opTextIdx = 0;
	for (sint32 i = 0; i < operandIndex; i++)
	{
		if ((internalCtx.iDef->flags&(1 << i)))
			continue; // skip operand
		opTextIdx++;
	}
	if (opTextIdx >= internalCtx.listOperandStr.size())
	{
		ppcAssembler_setError(internalCtx.ctx, "Missing operand");
		return false;
	}
	textIndex = opTextIdx;
	return true;
}

bool _ppcAssembler_parseRegister(std::string_view str, const char* prefix, sint32& regIndex)
{
	const char* startPtr = str.data();
	const char* endPtr = str.data() + str.size();
	// skip whitespaces
	while (startPtr < endPtr)
	{
		if (*startPtr != ' ')
			break;
		startPtr++;
	}
	// trim whitespaces at end
	while (endPtr > startPtr)
	{
		if (endPtr[-1] != ' ')
			break;
		endPtr--;
	}
	// parse register name
	if (startPtr + 2 > endPtr)
		return false;
	while (true)
	{
		if (startPtr >= endPtr)
			return false;
		if (*prefix == '\0')
			break;
		if (tolower(*prefix) != tolower(*startPtr))
			return false;
		prefix++;
		startPtr++;
	}
	sint32 parsedIndex = 0;
	while (startPtr < endPtr)
	{
		parsedIndex *= 10;
		if (*startPtr < '0' || *startPtr > '9')
			return false;
		parsedIndex += (*startPtr - '0');
		startPtr++;
	}
	if (parsedIndex >= 32)
		return false;
	regIndex = parsedIndex;
	return true;
}

bool _ppcAssembler_processRegisterOperand(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex, const char* prefix, const sint32 registerCount)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	// parse GPR
	sint32 gprIndex;
	if (_ppcAssembler_parseRegister(internalCtx.listOperandStr[opTextIdx].str, prefix, gprIndex) == false)
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("\'{0}\' is not a valid register operand (must be {1}0 to {1}{2})", internalCtx.listOperandStr[opTextIdx].str, prefix, registerCount-1));
		return false;
	}
	if (gprIndex < 0 || gprIndex >= registerCount)
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("\'{0}\' is not a valid register operand (must be {1}0 to {1}{2})", internalCtx.listOperandStr[opTextIdx].str, prefix, registerCount - 1));
		return false;
	}
	internalCtx.opcode |= (gprIndex << bitIndex);
	return true;
}

bool _ppcAssembler_processGPROperand(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex)
{
	return _ppcAssembler_processRegisterOperand(internalCtx, operandIndex, bitIndex, "r", 32);
}

bool _ppcAssembler_processCROperand(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex, bool isEncodedAsBitIndex)
{
	return _ppcAssembler_processRegisterOperand(internalCtx, operandIndex, bitIndex + (isEncodedAsBitIndex?2:0), "cr", 8);
}

template<typename TExprResult>
bool _ppcAssembler_evaluateConstantExpression(PPCAssemblerContext& internalCtx, TExpressionParser<TExprResult>& ep, std::string_view expr, TExprResult& result)
{
	if (!ep.IsValidExpression(expr))
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("'{}' is not a valid expression", expr));
		return false;
	}
	if (!ep.IsConstantExpression(expr))
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("'{}' does not evaluate to a constant expression", expr));
		return false;
	}
	try
	{
		result = ep.Evaluate(expr);
	}
	catch (std::exception* e)
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("\'{0}\' could not be evaluated. Error: {1}", expr, e->what()));
		return false;
	}
	return true;
}

bool _ppcAssembler_processBIOperand(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	// parse expression
	// syntax examples:
	// single condition bit of cr0: lt, gt, eq, so
	// condition bit with cr index: 4*cr1+lt
	TExpressionParser<sint32> ep;
	ep.AddConstant("lt", 0);
	ep.AddConstant("gt", 1);
	ep.AddConstant("eq", 2);
	ep.AddConstant("so", 3);
	ep.AddConstant("cr0", 0);
	ep.AddConstant("cr1", 1);
	ep.AddConstant("cr2", 2);
	ep.AddConstant("cr3", 3);
	ep.AddConstant("cr4", 4);
	ep.AddConstant("cr5", 5);
	ep.AddConstant("cr6", 6);
	ep.AddConstant("cr7", 7);
	std::string_view expr = internalCtx.listOperandStr[opTextIdx].str;
	sint32 bi = 0;
	if (!_ppcAssembler_evaluateConstantExpression(internalCtx, ep, expr, bi))
		return false;
	if (bi < 0 || bi >= (8 * 4))
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("CR bit operand \'{0}\' evaluated to {1} which is out of range", expr, bi));
		return false;
	}
	internalCtx.opcode &= ~(0x1F << bitIndex);
	internalCtx.opcode |= (bi << bitIndex);
	return true;
}

bool _ppcAssembler_processFPROperand(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex)
{
	return _ppcAssembler_processRegisterOperand(internalCtx, operandIndex, bitIndex, "f", 32);
}

bool _ppcAssembler_processImmediateOperandS16(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex, bool isNegative = false)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	// parse expression
	std::string expressionString(internalCtx.listOperandStr[opTextIdx].str);

	if (isNegative)
	{
		expressionString.insert(0, "0-(");
		expressionString.append(")");
	}
	ExpressionParser ep;
	double immD = 0.0f;
	try
	{
		if (ep.IsConstantExpression(expressionString))
		{
			immD = ep.Evaluate(expressionString);
		}
		else
		{
			internalCtx.ctx->list_relocs.emplace_back(PPCASM_RELOC::U32_MASKED_IMM, expressionString, 0, bitIndex, 16);
			return true;
		}
	}
	catch (std::exception* e)
	{
		// check if expression is invalid or if it contains unknown constants
		sprintf(_assemblerErrorMessageDepr, "\'%s\' could not be evaluated. Error: %s", expressionString.c_str(), e->what());
		ppcAssembler_setError(internalCtx.ctx, _assemblerErrorMessageDepr);
		return false;
	}
	uint32 imm = (uint32)(sint32)immD;
	imm &= 0xFFFF;
	internalCtx.opcode |= (imm << bitIndex);
	return true;
}

bool _ppcAssembler_processImmediateOperandU5(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint32 bitIndex)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	// parse expression
	ExpressionParser ep;
	double immD = 0.0f;
	try
	{
		immD = ep.Evaluate(internalCtx.listOperandStr[opTextIdx].str);
	}
	catch (std::exception*)
	{
		// check if expression is invalid or if it contains unknown constants
		ppcAssembler_setError(internalCtx.ctx, fmt::format("\'{}\' is not a valid expression", internalCtx.listOperandStr[opTextIdx].str));
		return false;
	}
	sint32 immS32 = (sint32)immD;
	if (immS32 < 0 || immS32 >= 32)
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("\'{}\' is not in range 0-31", internalCtx.listOperandStr[opTextIdx].str));
		return false;
	}
	uint32 imm = (uint32)immS32;
	imm &= 0x1F;
	internalCtx.opcode |= (imm << bitIndex);
	return true;
}

bool _ppcAssembler_processImmediateOperandU5Const(PPCAssemblerContext& internalCtx, sint32 operandIndex, sint8& value)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	// parse expression
	TExpressionParser<sint32> ep;
	auto expr = internalCtx.listOperandStr[opTextIdx].str;
	sint32 r;
	if (!_ppcAssembler_evaluateConstantExpression(internalCtx, ep, expr, r))
		return false;
	if (r < 0 || r >= 32)
	{
		ppcAssembler_setError(internalCtx.ctx, fmt::format("Expression '\'{0}\' which evaluates to {1} is not in range 0-31", expr, r));
		return false;
	}
	value = (sint8)r;
	return true;
}

bool _ppcAssembler_isConstantBranchTargetExpr(std::string& expressionString, sint32& relativeAddr)
{
	if (expressionString.length() > 0 && expressionString[0] == '.')
	{
		ExpressionParser ep;
		double branchDistance = 0.0f;
		try
		{
			branchDistance = ep.Evaluate(expressionString.substr(1).insert(0, "0"));
		}
		catch (std::exception&)
		{
			return false;
		}
		relativeAddr = (sint32)branchDistance;
		return true;
	}
	return false;
}

bool _ppcAssembler_processBranchOperandS16(PPCAssemblerContext& internalCtx, sint32 operandIndex)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	std::string expressionString(internalCtx.listOperandStr[opTextIdx].str);
	sint32 relativeAddr;
	if (_ppcAssembler_isConstantBranchTargetExpr(expressionString, relativeAddr))
	{
		if (relativeAddr < -32768 || relativeAddr > 32767)
		{
			sprintf(_assemblerErrorMessageDepr, "Branch target out of range");
			ppcAssembler_setError(internalCtx.ctx, _assemblerErrorMessageDepr);
			return false;
		}
		if (relativeAddr&3)
		{
			sprintf(_assemblerErrorMessageDepr, "Branch target must be aligned to 4");
			ppcAssembler_setError(internalCtx.ctx, _assemblerErrorMessageDepr);
			return false;
		}
		internalCtx.opcode |= (relativeAddr & 0xFFFC);
		return true;
	}			 
	// create reloc
	internalCtx.ctx->list_relocs.emplace_back(PPCASM_RELOC::BRANCH_S16, expressionString, 0, 0, 0);
	return true;
}

bool _ppcAssembler_processBranchOperandS26(PPCAssemblerContext& internalCtx, sint32 operandIndex)
{
	sint32 opTextIdx;
	if (_ppcAssembler_getOperandTextIndex(internalCtx, operandIndex, opTextIdx) == false)
		return false;
	if (opTextIdx < 0)
		return true; // skipped operand
	std::string expressionString(internalCtx.listOperandStr[opTextIdx].str);
	sint32 relativeAddr;
	if (_ppcAssembler_isConstantBranchTargetExpr(expressionString, relativeAddr))
	{
		if (relativeAddr < -33554432 || relativeAddr > 33554431)
		{
			ppcAssembler_setError(internalCtx.ctx, "Branch target out of range");
			return false;
		}
		if (relativeAddr & 3)
		{
			ppcAssembler_setError(internalCtx.ctx, "Branch target must be aligned to 4");
			return false;
		}
		internalCtx.opcode |= (relativeAddr & 0x3FFFFFC);
		return true;
	}
	// create reloc
	internalCtx.ctx->list_relocs.emplace_back(PPCASM_RELOC::BRANCH_S26, expressionString, 0, 0, 0);
	return true;
}

void _ppcAssembler_setAlignment(PPCAssemblerContext& internalInfo, uint32 alignment)
{
	if (internalInfo.ctx->forceNoAlignment)
	{
		internalInfo.ctx->alignmentRequirement = 1;
		internalInfo.ctx->alignmentPaddingSize = 0;
		internalInfo.ctx->virtualAddressAligned = internalInfo.ctx->virtualAddress;
		return;
	}
	internalInfo.ctx->alignmentRequirement = alignment;
	if (alignment == 0)
	{
		internalInfo.ctx->alignmentPaddingSize = 0;
		internalInfo.ctx->virtualAddressAligned = internalInfo.ctx->virtualAddress;
		return;
	}
	uint32 alignedVA = (internalInfo.ctx->virtualAddress + alignment - 1) & ~(alignment - 1);
	internalInfo.ctx->alignmentPaddingSize = alignedVA - internalInfo.ctx->virtualAddress;
	internalInfo.ctx->virtualAddressAligned = alignedVA;
}

enum class ASM_DATA_DIRECTIVE
{
	NONE,
	FLOAT,
	DOUBLE,
	U32,
	U16,
	U8, // alias to .string
};

bool _ppcAssembler_emitDataDirective(PPCAssemblerContext& internalInfo, ASM_DATA_DIRECTIVE dataDirective)
{
	PPCASM_RELOC relocType;

	switch (dataDirective)
	{
	case ASM_DATA_DIRECTIVE::FLOAT:
		relocType = PPCASM_RELOC::FLOAT;
		break;
	case ASM_DATA_DIRECTIVE::DOUBLE:
		relocType = PPCASM_RELOC::DOUBLE;
		break;
	case ASM_DATA_DIRECTIVE::U32:
		relocType = PPCASM_RELOC::U32;
		break;
	case ASM_DATA_DIRECTIVE::U16:
		relocType = PPCASM_RELOC::U16;
		break;
	case ASM_DATA_DIRECTIVE::U8:
		relocType = PPCASM_RELOC::U8;
		break;
	default:
		cemu_assert_debug(false);
		return false;
	}

	uint32 elementSize = 0;
	
	if (relocType == PPCASM_RELOC::FLOAT)
		elementSize = 4;
	else if (relocType == PPCASM_RELOC::DOUBLE)
		elementSize = 8;
	else if (relocType == PPCASM_RELOC::U32)
		elementSize = 4;
	else if (relocType == PPCASM_RELOC::U16)
		elementSize = 2;
	else if (relocType == PPCASM_RELOC::U8)
		elementSize = 1;
	else
		cemu_assert_debug(false);
	
	_ppcAssembler_setAlignment(internalInfo, elementSize);
	size_t elementCount = internalInfo.listOperandStr.size();
	internalInfo.ctx->outputData.reserve(elementSize * elementCount);
	size_t writeIndex = 0;
	for (size_t i = 0; i < elementCount; i++)
	{
		std::string_view expressionStr = internalInfo.listOperandStr[i].str;
		// handle string constants
		if (internalInfo.listOperandStr[i].str.size() >= 1 && expressionStr[0] == '"')
		{
			if (dataDirective != ASM_DATA_DIRECTIVE::U8)
			{
				ppcAssembler_setError(internalInfo.ctx, "Strings constants are only allowed in .byte or .string data directives");
				return false;
			}
			if (expressionStr.size() < 2 || expressionStr[expressionStr.size()-1] != '"')
			{
				ppcAssembler_setError(internalInfo.ctx, "String constants must end with a quotation mark. Example: \"text\"");
				return false;
			}
			// write string bytes + null-termination character
			size_t strConstantLength = expressionStr.size() - 2;
			internalInfo.ctx->outputData.insert(internalInfo.ctx->outputData.end(), expressionStr.data() + 1, expressionStr.data() + 1 + strConstantLength);
			internalInfo.ctx->outputData.emplace_back(0);
			continue;
		}
		// numeric constants
		internalInfo.ctx->outputData.resize(writeIndex + elementSize);
		uint8* elementPtr = internalInfo.ctx->outputData.data() + writeIndex;
		for (uint32 b = 0; b < elementSize; b++)
		{
			internalInfo.ctx->outputData[writeIndex] = 0;
			writeIndex++;
		}
		ExpressionParser ep;
		if (ep.IsConstantExpression(expressionStr))
		{
			double solution = ep.Evaluate(expressionStr);
			switch (relocType)
			{
			case PPCASM_RELOC::U32:
				*(uint32be*)elementPtr = (uint32)solution;
				break;
			case PPCASM_RELOC::U16:
				*(uint16be*)elementPtr = (uint16)solution;
				break;
			case PPCASM_RELOC::U8:
				*(uint8be*)elementPtr = (uint8)solution;
				break;
			case PPCASM_RELOC::DOUBLE:
				*(float64be*)elementPtr = solution;
				break;
			case PPCASM_RELOC::FLOAT:
				*(float32be*)elementPtr = (float)solution;
				break;
			default:
				cemu_assert_debug(false);
			}

		}
		else
		{
			// generate reloc if we cant resolve immediately
			internalInfo.ctx->list_relocs.emplace_back(relocType, std::string(internalInfo.listOperandStr[i].str), (uint32)(elementPtr - internalInfo.ctx->outputData.data()), 0, 0);
		}
	}
	return true;
}

void _ppcAssembler_emitAlignDirective(PPCAssemblerContext& internalInfo, sint32 alignmentValue)
{
	_ppcAssembler_setAlignment(internalInfo, 1);
	uint32 currentAddr = internalInfo.ctx->virtualAddress;
	while ((currentAddr % (uint32)alignmentValue))
	{
		internalInfo.ctx->outputData.emplace_back(0);
		currentAddr++;
	}
}

void _ppcAssembler_translateAlias(boost::static_string<32>& instructionName)
{
	if (instructionName.compare("BNL") == 0)
		instructionName.assign("BGT");
	if (instructionName.compare("BNG") == 0)
		instructionName.assign("BLE");
}

bool ppcAssembler_assembleSingleInstruction(char const* text, PPCAssemblerInOut* ctx)
{
	PPCAssemblerContext internalInfo;
	internalInfo.ctx = ctx;
	// tokenize input
	char const* currentPtr = text;
	char const* startPtr = text;
	char const* endPtr = startPtr+strlen(startPtr);
	// cut off comments
	for (const char* itrPtr = startPtr; itrPtr < endPtr; itrPtr++)
	{
		if (*itrPtr == '#')
		{
			endPtr = itrPtr;
			break;
		}
	}
	// trim whitespaces at end and beginning
	while (endPtr > currentPtr)
	{
		if (endPtr[-1] != ' ' && endPtr[-1] != '\t')
			break;
		endPtr--;
	}
	while (currentPtr < endPtr)
	{
		if (currentPtr[0] != ' ' && currentPtr[0] != '\t')
			break;
		currentPtr++;
	}
	// parse name of instruction
	boost::static_string<32> instructionName;
	while (*currentPtr != ' ' && *currentPtr != '\t' && currentPtr < endPtr)
	{
		if (instructionName.size() >= instructionName.capacity())
		{
			ppcAssembler_setError(ctx, "Instruction name exceeds maximum allowed length");
			return false;
		}
		instructionName.push_back((char)toupper(*currentPtr));
		currentPtr++;
	}
	if (instructionName.empty())
	{
		ppcAssembler_setError(ctx, "Instruction name is invalid");
		return false;
	}
	// handle branch hint suffix
	bool hasBranchHint = false;
	bool branchHintTaken = false; // '+' -> true, '-' -> false
	if (instructionName.back() == '+')
	{
		hasBranchHint = true;
		branchHintTaken = true;
		instructionName.pop_back();
	}
	if (instructionName.back() == '-')
	{
		hasBranchHint = true;
		branchHintTaken = false;
		instructionName.pop_back();
	}
	// handle instruction name aliases
	_ppcAssembler_translateAlias(instructionName);
	// parse operands
	internalInfo.listOperandStr.clear();

	bool isInString = false;

	while (currentPtr < endPtr)
	{
		currentPtr++;
		startPtr = currentPtr;
		// find end of operand
		while (currentPtr < endPtr)
		{
			if (*currentPtr == '"')
				isInString=!isInString;

			if (*currentPtr == ',' && !isInString)
				break;
			currentPtr++;
		}
		// trim whitespaces at the beginning and end
		const char* operandStartPtr = startPtr;
		const char* operandEndPtr = currentPtr;
		while (operandStartPtr < endPtr)
		{
			if (*operandStartPtr != ' ' && *operandStartPtr != '\t')
				break;
			operandStartPtr++;
		}
		while (operandEndPtr > operandStartPtr)
		{
			if (operandEndPtr[-1] != ' ' && operandEndPtr[-1] != '\t')
				break;
			operandEndPtr--;
		}
		internalInfo.listOperandStr.emplace_back(operandStartPtr, operandEndPtr);
	}
	// check for data directives
	ASM_DATA_DIRECTIVE dataDirective = ASM_DATA_DIRECTIVE::NONE;
	if (instructionName.compare(".FLOAT") == 0)
		dataDirective = ASM_DATA_DIRECTIVE::FLOAT;
	else if (instructionName.compare(".DOUBLE") == 0)
		dataDirective = ASM_DATA_DIRECTIVE::DOUBLE;
	else if (instructionName.compare(".INT") == 0 || instructionName.compare(".UINT") == 0 || instructionName.compare(".PTR") == 0 || instructionName.compare(".U32") == 0 || instructionName.compare(".LONG") == 0)
		dataDirective = ASM_DATA_DIRECTIVE::U32;
	else if (instructionName.compare(".WORD") == 0 || instructionName.compare(".U16") == 0 || instructionName.compare(".SHORT") == 0)
		dataDirective = ASM_DATA_DIRECTIVE::U16;
	else if (instructionName.compare(".BYTE") == 0 || instructionName.compare(".STRING") == 0 || instructionName.compare(".U8") == 0 || instructionName.compare(".CHAR") == 0)
		dataDirective = ASM_DATA_DIRECTIVE::U8;

	if (dataDirective != ASM_DATA_DIRECTIVE::NONE)
	{
		if (internalInfo.listOperandStr.size() < 1)
		{
			ppcAssembler_setError(ctx, fmt::format("Value expected after data directive {}", instructionName.c_str()));
			return false;
		}
		return _ppcAssembler_emitDataDirective(internalInfo, dataDirective);
	}
	// handle .align directive
	if (instructionName.compare(".ALIGN") == 0)
	{
		// handle align data directive
		if (internalInfo.listOperandStr.size() != 1)
		{
			ppcAssembler_setError(ctx, ".align directive must have exactly one operand");
			return false;
		}
		ExpressionParser ep;
		try
		{
			if (ep.IsConstantExpression(internalInfo.listOperandStr[0].str))
			{
				sint32 alignmentValue = ep.Evaluate<sint32>(internalInfo.listOperandStr[0].str);
				if (alignmentValue <= 0 || alignmentValue >= 256)
				{
					ppcAssembler_setError(ctx, fmt::format("Alignment value \'{}\' is not within the allowed range (1-256)", alignmentValue));
					return false;
				}
				_ppcAssembler_emitAlignDirective(internalInfo, alignmentValue);
				return true;
			}
			else
			{
				ppcAssembler_setError(ctx, fmt::format("Expression \'{}\' for .align directive is not a constant", internalInfo.listOperandStr[0].str));
				return false;
			}
		}
		catch (std::exception* e)
		{
			ppcAssembler_setError(ctx, fmt::format("Expression \'{}\' for .align directive could not be evaluated. Error: {}", internalInfo.listOperandStr[0].str, e->what()));
			return false;
		}
	}
	// find match in instruction definition table
	PPCInstructionDef* iDef = nullptr;
	for (sint32 i = 0; i < sizeof(ppcInstructionTable) / sizeof(PPCInstructionDef); i++)
	{
		PPCInstructionDef* instr = ppcInstructionTable + i;
		if (instructionName.compare(ppcAssembler_getInstructionName(instr->ppcAsmOp)) == 0)
		{
			iDef = instr;
			internalInfo.iDef = iDef;
			break;
		}
	}
	if (iDef == nullptr)
	{
		ppcAssembler_setError(ctx, fmt::format("Instruction \'{}\' is unknown or not supported", instructionName.c_str()));
		return false;
	}
	// build opcode
	_ppcAssembler_setAlignment(internalInfo, 4);
	uint32 opcMask;
	uint32 opcBits;
	ppcAssembler_buildOpcMask(iDef, opcMask, opcBits);
	internalInfo.opcode = opcBits & opcMask;
	internalInfo.opcode |= (iDef->compareBits&iDef->maskBits);
	// handle operands
	if (iDef->instructionForm == OP_FORM_DYNAMIC)
	{
		for (size_t i = 0; i < iDef->encodedOperands.size(); i++)
		{
			bool r = std::visit([&](auto&& op) -> bool {
				return op.AssembleOperand(&internalInfo, iDef, internalInfo.opcode, i);
				}, iDef->encodedOperands[i]);
			if (!r)
				return false;
		}
		for(auto& it : iDef->constraints)
		{
			std::visit([&](auto&& op) {
				op.AssembleConstraint(&internalInfo, iDef, internalInfo.opcode);
				}, it);
		}
	}
	else if (iDef->instructionForm == OP_FORM_NO_OPERAND)
	{
		// do nothing
	}
	else if (iDef->instructionForm == OP_FORM_OP3_A_CMP)
	{
		if (internalInfo.listOperandStr.size() == 2)
		{
			// implicit cr0
			if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
				return false;
			if (_ppcAssembler_processGPROperand(internalInfo, 1, 11) == false)
				return false;
		}
		else
		{
			if (_ppcAssembler_processCROperand(internalInfo, 0, 21, true) == false)
				return false;
			if (_ppcAssembler_processGPROperand(internalInfo, 1, 16) == false)
				return false;
			if (_ppcAssembler_processGPROperand(internalInfo, 2, 11) == false)
				return false;
		}
	}
	else if (iDef->instructionForm == OP_FORM_CMP_SIMM)
	{
		if (internalInfo.listOperandStr.size() == 2)
		{
			// implicit cr0
			if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
				return false;
			if (_ppcAssembler_processImmediateOperandS16(internalInfo, 1, 0) == false)
				return false;
		}
		else
		{
			if (_ppcAssembler_processCROperand(internalInfo, 0, 21, true) == false)
				return false;
			if (_ppcAssembler_processGPROperand(internalInfo, 1, 16) == false)
				return false;
			if (_ppcAssembler_processImmediateOperandS16(internalInfo, 2, 0) == false)
				return false;
		}
	}
	else if (iDef->instructionForm == OP_FORM_OP3_A_IMM)
	{
		if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 1, 21) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, _getOpIndex(iDef, 2), 11) == false)
			return false;
	}
	else if (iDef->instructionForm == OP_FORM_X_FP_CMP)
	{
		if (internalInfo.listOperandStr.size() == 2)
		{
			// implicit cr0
			if (_ppcAssembler_processFPROperand(internalInfo, 0, 16) == false)
				return false;
			if (_ppcAssembler_processFPROperand(internalInfo, 1, 11) == false)
				return false;
		}
		else
		{
			if (_ppcAssembler_processCROperand(internalInfo, 0, 21, true) == false)
				return false;
			if (_ppcAssembler_processFPROperand(internalInfo, 1, 16) == false)
				return false;
			if (_ppcAssembler_processFPROperand(internalInfo, 2, 11) == false)
				return false;
		}
	}
	else if (iDef->instructionForm == OP_FORM_RLWINM)
	{
		if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 1, 21) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, 2, 11) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, 3, 6) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, 4, 1) == false)
			return false;
	}
	else if (iDef->instructionForm == OP_FORM_RLWINM_EXTENDED)
	{
		if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 1, 21) == false)
			return false;

		uint8 SH = 0;
		uint8 MB = 0;
		uint8 ME = 0;

		if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_EXTLWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_EXTLWI_)
		{
			sint8 n, b;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 3, b))
				return false;

			SH = b;
			MB = 0;
			ME = n - 1;
		}
		else if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_EXTRWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_EXTRWI_)
		{
			sint8 n, b;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 3, b))
				return false;

			SH = b + n;
			MB = 32 - n;
			ME = 31;
		}
		else if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_SLWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_SLWI_)
		{
			sint8 n;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;

			SH = n;
			MB = 0;
			ME = 31 - n;
		}
		else if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_SRWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_SRWI_)
		{
			sint8 n;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;
			SH = 32 - n;
			MB = n;
			ME = 31;
		}
		else if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_CLRLWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_CLRLWI_)
		{
			sint8 n;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;
			SH = 0;
			MB = n;
			ME = 31;
		}
		else if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_CLRRWI || internalInfo.iDef->ppcAsmOp == PPCASM_OP_CLRRWI_)
		{
			sint8 n;
			if (!_ppcAssembler_processImmediateOperandU5Const(internalInfo, 2, n))
				return false;
			SH = 0;
			MB = 0;
			ME = 31 - n;
		}
		else
		{
			cemu_assert_debug(false);
		}

		internalInfo.opcode |= (SH << 11);
		internalInfo.opcode |= (MB << 6);
		internalInfo.opcode |= (ME << 1);
	}
	else if (iDef->instructionForm == OP_FORM_RLWNM)
	{
		if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 1, 21) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 2, 11) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, 3, 6) == false)
			return false;
		if (_ppcAssembler_processImmediateOperandU5(internalInfo, 4, 1) == false)
			return false;
	}
	else if (iDef->instructionForm == OP_FORM_RLWNM_EXTENDED)
	{
		if (_ppcAssembler_processGPROperand(internalInfo, 0, 16) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 1, 21) == false)
			return false;
		if (_ppcAssembler_processGPROperand(internalInfo, 2, 11) == false)
			return false;
		uint32 MB = 0, ME = 0;
		if (internalInfo.iDef->ppcAsmOp == PPCASM_OP_ROTLW || internalInfo.iDef->ppcAsmOp == PPCASM_OP_ROTLW_)
		{
			MB = 0;
			ME = 31;
		}
		internalInfo.opcode |= (MB << 6);
		internalInfo.opcode |= (ME << 1);
	}
	else if (iDef->instructionForm == OP_FORM_BRANCH_S16)
	{
		if (iDef->ppcAsmOp == PPCASM_OP_BC)
		{
			// generic conditional branch
			// BC <BO>, <BI>, <dst>
			sint8 bo;
			if (_ppcAssembler_processImmediateOperandU5Const(internalInfo, 0, bo) == false)
				return false;
			if (_ppcAssembler_processBIOperand(internalInfo, 1, 16) == false)
				return false;
			if (_ppcAssembler_processBranchOperandS16(internalInfo, 2) == false)
				return false;
			internalInfo.opcode |= (bo << 21);
		}
		else
		{
			// BLE, BGT etc.
			if (internalInfo.listOperandStr.size() == 2)
			{
				// explicit CR
				if (_ppcAssembler_processCROperand(internalInfo, 0, 16, true) == false)
					return false;
				if (_ppcAssembler_processBranchOperandS16(internalInfo, 1) == false)
					return false;
			}
			else
			{
				if (_ppcAssembler_processBranchOperandS16(internalInfo, 0) == false)
					return false;
			}
			if (hasBranchHint)
				internalInfo.opcode |= (1 << 21);
		}
	}
	else if (iDef->instructionForm == OP_FORM_BRANCH_S24)
	{
		if (_ppcAssembler_processBranchOperandS26(internalInfo, 0) == false)
			return false;
	}
	else if (iDef->instructionForm == OP_FORM_XL_CR)
	{
		if (_ppcAssembler_processBIOperand(internalInfo, 0, 21) == false)
			return false;
		if (_ppcAssembler_processBIOperand(internalInfo, 1, 16) == false)
			return false;
		if (_ppcAssembler_processBIOperand(internalInfo, 2, 11) == false)
			return false;
	}
	else
	{
		cemu_assert_debug(false); // unsupported instruction form
		return false;
	}
	ctx->outputData.resize(4);
	ctx->outputData[0] = (uint8)((internalInfo.opcode >> 24) & 0xFF);
	ctx->outputData[1] = (uint8)((internalInfo.opcode >> 16) & 0xFF);
	ctx->outputData[2] = (uint8)((internalInfo.opcode >>  8) & 0xFF);
	ctx->outputData[3] = (uint8)((internalInfo.opcode >>  0) & 0xFF);
	return true;
}

void _testAsm(uint32 expected, const char* iText)
{
	PPCAssemblerInOut ctx{};
	ctx.virtualAddress = 0;
	if (!ppcAssembler_assembleSingleInstruction(iText, &ctx))
	{
		cemu_assert_debug(false);
		return;
	}
	cemu_assert_debug(ctx.outputData.size() == 4);
	uint32 opcode = 0;
	opcode |= ((uint32)ctx.outputData[0] << 24);
	opcode |= ((uint32)ctx.outputData[1] << 16);
	opcode |= ((uint32)ctx.outputData[2] << 8);
	opcode |= ((uint32)ctx.outputData[3] << 0);
	cemu_assert_debug(expected == opcode);

}

void _testAsmFail(const char* iText)
{
	PPCAssemblerInOut ctx{};
	ctx.virtualAddress = 0;
	if (!ppcAssembler_assembleSingleInstruction(iText, &ctx))
		return;
	cemu_assert_debug(false); // should fail
}

void _testAsmArray(std::vector<uint8> expectedArray, const char* iText)
{
	PPCAssemblerInOut ctx{};
	ctx.virtualAddress = 0;
	if (!ppcAssembler_assembleSingleInstruction(iText, &ctx))
	{
		cemu_assert_debug(false);
		return;
	}
	cemu_assert_debug(ctx.outputData.size() == expectedArray.size());
	for (size_t offset = 0; offset < expectedArray.size(); offset++)
	{
		cemu_assert_debug(ctx.outputData[offset] == expectedArray[offset]);
	}
}

void ppcAsmTestDisassembler()
{
	cemu_assert_debug(_CanStoreInteger(5, 10, false));
	cemu_assert_debug(_CanStoreInteger(1023, 10, false));
	cemu_assert_debug(!_CanStoreInteger(1024, 10, false));
	cemu_assert_debug(_CanStoreInteger(511, 10, true));
	cemu_assert_debug(!_CanStoreInteger(512, 10, true));
	cemu_assert_debug(_CanStoreInteger(-511, 10, true));
	cemu_assert_debug(_CanStoreInteger(-512, 10, true));
	cemu_assert_debug(!_CanStoreInteger(-513, 10, true));

	PPCDisassembledInstruction disasm;

	auto disassemble = [&](uint32 opcode, PPCASM_OP ppcAsmCode)
	{
		disasm = { 0 };
		ppcAssembler_disassemble(0x10000000, opcode, &disasm);
		cemu_assert_debug(disasm.ppcAsmCode == ppcAsmCode);
	};

	auto checkOperandMask = [&](bool op0 = false, bool op1 = false, bool op2 = false, bool op3 = false)
	{
		bool hasOp0 = (disasm.operandMask & (1 << 0));
		bool hasOp1 = (disasm.operandMask & (1 << 1));
		bool hasOp2 = (disasm.operandMask & (1 << 2));
		bool hasOp3 = (disasm.operandMask & (1 << 3));

		cemu_assert_debug(hasOp0 == op0);
		cemu_assert_debug(hasOp1 == op1);
		cemu_assert_debug(hasOp2 == op2);
		cemu_assert_debug(hasOp3 == op3);
	};

	auto checkOpCR = [&](size_t index, uint8 crIndex)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_CR);
		cemu_assert_debug(disasm.operand[index].registerIndex == crIndex);
	};

	auto checkOpCRBit = [&](size_t index, uint8 crIndex)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_CR_BIT);
		cemu_assert_debug(disasm.operand[index].registerIndex == crIndex);
	};

	auto checkOpFPR = [&](size_t index, uint8 fprIndex)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_FPR);
		cemu_assert_debug(disasm.operand[index].registerIndex == fprIndex);
	};

	auto checkOpGPR = [&](size_t index, uint8 gprIndex)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_GPR);
		cemu_assert_debug(disasm.operand[index].registerIndex == gprIndex);
	};

	auto checkOpImm = [&](size_t index, sint32 imm)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_IMM);
		cemu_assert_debug(disasm.operand[index].immS32 == imm);
	};

	auto checkOpBranchDst = [&](size_t index, sint32 relOffset)
	{
		cemu_assert_debug(disasm.operand[index].type == PPCASM_OPERAND_TYPE_CIMM);
		cemu_assert_debug(disasm.operand[index].immU32 == 0x10000000 + relOffset);
	};

	auto checkBranchHint = [&](bool isSet)
	{
		cemu_assert_debug(disasm.branchHintBitSet == isSet);
	};

	// addi / subi
	_testAsm(0x3863FFFF, "addi r3, r3, -1");
	_testAsm(0x3863FFFF, "subi r3, r3, 1");
	_testAsm(0x387E0134, "addi r3, r30, 0x134");
	_testAsm(0x387E0134, "subi r3, r30, 0-0x134");
	disassemble(0x387E0134, PPCASM_OP_ADDI);
	checkOperandMask(true, true, true);
	checkOpGPR(0, 3);
	checkOpGPR(1, 30);
	checkOpImm(2, 0x134);

	// mulli
	_testAsm(0x1D1E0005, "mulli r8, r30, 5");
	disassemble(0x1D1E0005, PPCASM_OP_MULLI);
	checkOperandMask(true, true, true);
	checkOpGPR(0, 8);
	checkOpGPR(1, 30);
	checkOpImm(2, 5);

	// mulli
	_testAsm(0x1CFEFFF8, "mulli r7, r30, 0-(2 + 2 + 2 + 2)"); // -8
	disassemble(0x1CFEFFF8, PPCASM_OP_MULLI);
	checkOperandMask(true, true, true);
	checkOpGPR(0, 7);
	checkOpGPR(1, 30);
	checkOpImm(2, -8);

	// subfic
	_testAsm(0x2304002F, "subfic r24, r4, 0x2F");
	disassemble(0x2304002F, PPCASM_OP_SUBFIC);
	checkOperandMask(true, true, true);
	checkOpGPR(0, 24);
	checkOpGPR(1, 4);
	checkOpImm(2, 0x2F);

	// fcmpu cr7, f1, f0
	_testAsm(0xFF810000, "fcmpu cr7, f1, f0");
	disassemble(0xFF810000, PPCASM_OP_FCMPU);
	checkOperandMask(true, true, true);
	checkOpCR(0, 7);
	checkOpFPR(1, 1);
	checkOpFPR(2, 0);

	// fcmpu cr0, f1, f0
	_testAsm(0xFC010000, "fcmpu cr0, f1, f0");
	disassemble(0xFC010000, PPCASM_OP_FCMPU);
	checkOperandMask(true, true, true);
	checkOpCR(0, 0);
	checkOpFPR(1, 1);
	checkOpFPR(2, 0);

	// cmpwi r9, -1
	_testAsm(0x2C09FFFF, "cmpwi r9, -1");
	disassemble(0x2C09FFFF, PPCASM_OP_CMPWI);
	checkOperandMask(false, true, true);
	checkOpGPR(1, 9);
	checkOpImm(2, -1);
	// cmpwi cr7, r9, 0
	_testAsm(0x2F890000, "cmpwi cr7, r9, 0");
	disassemble(0x2F890000, PPCASM_OP_CMPWI);
	checkOperandMask(true, true, true);
	checkOpCR(0, 7);
	checkOpGPR(1, 9);
	checkOpImm(2, 0);
	// cmplwi r3, 0xF
	_testAsm(0x2803000F, "cmplwi r3, 0xF");
	disassemble(0x2803000F, PPCASM_OP_CMPLWI);
	checkOperandMask(false, true, true);
	checkOpGPR(1, 3);
	checkOpImm(2, 0xF);
	// cmpw cr7, r4, r10
	_testAsm(0x7F845000, "cmpw cr7, r4, r10");
	disassemble(0x7F845000, PPCASM_OP_CMPW);
	checkOperandMask(true, true, true);
	checkOpCR(0, 7);
	checkOpGPR(1, 4);
	checkOpGPR(2, 10);
	// cmplw cr7, r31, r9
	_testAsm(0x7F9F4840, "cmplw cr7, r31, r9");
	disassemble(0x7F9F4840, PPCASM_OP_CMPLW);
	checkOperandMask(true, true, true);
	checkOpCR(0, 7);
	checkOpGPR(1, 31);
	checkOpGPR(2, 9);
	// cmplw r24, r28
	_testAsm(0x7C18E040, "cmplw r24, r28");
	disassemble(0x7C18E040, PPCASM_OP_CMPLW);
	checkOperandMask(false, true, true);
	checkOpGPR(1, 24);
	checkOpGPR(2, 28);

	// b .+0x18
	_testAsm(0x48000018, "b .+0x18");
	// b 0x10000018
	//_testAsm(0x48000018, "b 0x10000018");

	// bgt cr7, .+0x40
	_testAsm(0x419D0040, "bgt cr7, .+0x40");
	disassemble(0x419D0040, PPCASM_OP_BGT);
	checkOperandMask(true, true);
	checkOpCR(0, 7);
	checkOpBranchDst(1, 0x40);
	checkBranchHint(false);
	// bnl cr7, .+0x40 (alias to bgt)
	_testAsm(0x419D0040, "bnl cr7, .+0x40");
	// beq cr7, .+0x14
	_testAsm(0x419E0014, "beq cr7, .+0x14");
	disassemble(0x419E0014, PPCASM_OP_BEQ);
	checkOperandMask(true, true);
	checkOpCR(0, 7);
	checkOpBranchDst(1, 0x14);
	checkBranchHint(false);
	// beq .-0x4C
	_testAsm(0x4182FFB4, "beq .-0x4C");
	disassemble(0x4182FFB4, PPCASM_OP_BEQ);
	checkOperandMask(true);
	checkOpBranchDst(0, -0x4C);
	checkBranchHint(false);
	// beq+ .+8
	_testAsm(0x41A20008, "beq+ .+8");
	disassemble(0x41A20008, PPCASM_OP_BEQ);
	checkOperandMask(true, false);
	checkOpBranchDst(0, +0x8);
	checkBranchHint(true);

	// cror
	_testAsm(0x4F5DF382, "cror 4*cr6+eq, 4*cr7+gt, 4*cr7+eq");
	_testAsm(0x4C411B82, "cror eq, gt, so");
	disassemble(0x4F5DF382, PPCASM_OP_CROR);
	checkOperandMask(true, true, true);
	checkOpCRBit(0, 6 * 4 + 2);
	checkOpCRBit(1, 7 * 4 + 1);
	checkOpCRBit(2, 7 * 4 + 2);

	// slw & srw
	_testAsm(0x7D202030, "slw r0, r9, r4");
	_testAsm(0x7D80FC31, "srw. r0, r12, r31");
	disassemble(0x7D202030, PPCASM_OP_SLW);
	checkOperandMask(true, true, true);
	checkOpGPR(0, 0);
	checkOpGPR(1, 9);
	checkOpGPR(2, 4);

	// FADD
	_testAsm(0xFC29502A, "fadd f1, f9, f10");
	disassemble(0xFC29502A, PPCASM_OP_FADD);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 1);
	checkOpFPR(1, 9);
	checkOpFPR(2, 10);

	// FSUB
	_testAsm(0xFDAB4028, "fsub f13, f11, f8");
	_testAsm(0xFD0D4828, "fsub f8, f13, f9");
	disassemble(0xFD0D4828, PPCASM_OP_FSUB);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 8);
	checkOpFPR(1, 13);
	checkOpFPR(2, 9);

	// FMUL
	_testAsm(0xFD4B0332, "fmul f10, f11, f12");
	disassemble(0xFD4B0332, PPCASM_OP_FMUL);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 10);
	checkOpFPR(1, 11);
	checkOpFPR(2, 12);

	// FDIV
	_testAsm(0xFD885824, "fdiv f12, f8, f11");
	disassemble(0xFD885824, PPCASM_OP_FDIV);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 12);
	checkOpFPR(1, 8);
	checkOpFPR(2, 11);

	// FMADD
	_testAsm(0xFC2A0A3A, "fmadd f1, f10, f8, f1");
	_testAsm(0xFC0A0B7A, "fmadd f0, f10, f13, f1");
	_testAsm(0xFFE0F33A, "fmadd f31, f0, f12, f30");
	disassemble(0xFFE0F33A, PPCASM_OP_FMADD);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 31);
	checkOpFPR(1, 0);
	checkOpFPR(2, 12);
	checkOpFPR(3, 30);

	// FNMADDS
	_testAsm(0xED8A627E, "fnmadds f12, f10, f9, f12");
	disassemble(0xED8A627E, PPCASM_OP_FNMADDS);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 12);
	checkOpFPR(1, 10);
	checkOpFPR(2, 9);
	checkOpFPR(3, 12);

	// still missing test cases: FNMADD, FMADDS

	// FMSUB
	_testAsm(0xFC1C0338, "fmsub f0, f28, f12, f0");
	_testAsm(0xFD204B38, "fmsub f9, f0, f12, f9");
	disassemble(0xFD204B38, PPCASM_OP_FMSUB);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 9);
	checkOpFPR(1, 0);
	checkOpFPR(2, 12);
	checkOpFPR(3, 9);

	// FNMSUB
	_testAsm(0xFD7DCB3C, "fnmsub f11, f29, f12, f25");
	disassemble(0xFD7DCB3C, PPCASM_OP_FNMSUB);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 11);
	checkOpFPR(1, 29);
	checkOpFPR(2, 12);
	checkOpFPR(3, 25);

	// FMSUBS
	_testAsm(0xEE3D5838, "fmsubs f17, f29, f0, f11");
	_testAsm(0xEDB84AB8, "fmsubs f13, f24, f10, f9");
	disassemble(0xEDB84AB8, PPCASM_OP_FMSUBS);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 13);
	checkOpFPR(1, 24);
	checkOpFPR(2, 10);
	checkOpFPR(3, 9);

	// FNMSUBS
	_testAsm(0xED4951BC, "fnmsubs f10, f9, f6, f10");
	_testAsm(0xED253AFC, "fnmsubs f9, f5, f11, f7");
	disassemble(0xED253AFC, PPCASM_OP_FNMSUBS);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 9);
	checkOpFPR(1, 5);
	checkOpFPR(2, 11);
	checkOpFPR(3, 7);

	// FRSQRTE
	_testAsm(0xFCE06034, "frsqrte f7, f12");
	_testAsm(0xFCC06834, "frsqrte f6, f13");
	disassemble(0xFCC06834, PPCASM_OP_FRSQRTE);
	checkOperandMask(true, true);
	checkOpFPR(0, 6);
	checkOpFPR(1, 13);

	// FRSP
	_testAsm(0xFFA03018, "frsp f29, f6");
	disassemble(0xFFA03018, PPCASM_OP_FRSP);
	checkOperandMask(true, true);
	checkOpFPR(0, 29);
	checkOpFPR(1, 6);

	// FNEG
	_testAsm(0xFDA05850, "fneg f13, f11");
	disassemble(0xFDA05850, PPCASM_OP_FNEG);
	checkOperandMask(true, true);
	checkOpFPR(0, 13);
	checkOpFPR(1, 11);

	// FMR
	_testAsm(0xFCE06090, "fmr f7, f12");
	disassemble(0xFCE06090, PPCASM_OP_FMR);
	checkOperandMask(true, true);
	checkOpFPR(0, 7);
	checkOpFPR(1, 12);

	// FCTIWZ
	_testAsm(0xFD80401E, "fctiwz f12, f8");
	disassemble(0xFD80401E, PPCASM_OP_FCTIWZ);
	checkOperandMask(true, true);
	checkOpFPR(0, 12);
	checkOpFPR(1, 8);

	// PS_MADD
	_testAsm(0x1168637A, "ps_madd   f11, f8, f13, f12");
	disassemble(0x1168637A, PPCASM_OP_PS_MADD);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 11);
	checkOpFPR(1, 8);
	checkOpFPR(2, 13);
	checkOpFPR(3, 12);

	// PS_MSUB
	_testAsm(0x11A55FF8, "ps_msub f13, f5, f31, f11");
	disassemble(0x11A55FF8, PPCASM_OP_PS_MSUB);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 13);
	checkOpFPR(1, 5);
	checkOpFPR(2, 31);
	checkOpFPR(3, 11);

	// PS_NMADD
	_testAsm(0x118850FE, "ps_nmadd f12, f8, f3, f10");
	disassemble(0x118850FE, PPCASM_OP_PS_NMADD);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 12);
	checkOpFPR(1, 8);
	checkOpFPR(2, 3);
	checkOpFPR(3, 10);

	// PS_NMSUB
	_testAsm(0x112832FC, "ps_nmsub f9, f8, f11, f6");
	disassemble(0x112832FC, PPCASM_OP_PS_NMSUB);
	checkOperandMask(true, true, true, true);
	checkOpFPR(0, 9);
	checkOpFPR(1, 8);
	checkOpFPR(2, 11);
	checkOpFPR(3, 6);

	// PS_ADD
	_testAsm(0x112A582A, "ps_add f9, f10, f11");
	disassemble(0x112A582A, PPCASM_OP_PS_ADD);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 9);
	checkOpFPR(1, 10);
	checkOpFPR(2, 11);

	// PS_SUB
	_testAsm(0x11663828, "ps_sub f11, f6, f7");
	disassemble(0x11663828, PPCASM_OP_PS_SUB);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 11);
	checkOpFPR(1, 6);
	checkOpFPR(2, 7);

	// PS_MUL
	_testAsm(0x11680332, "ps_mul f11, f8, f12");
	disassemble(0x11680332, PPCASM_OP_PS_MUL);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 11);
	checkOpFPR(1, 8);
	checkOpFPR(2, 12);

	// PS_DIV
	_testAsm(0x10A45824, "ps_div f5, f4, f11");
	disassemble(0x10A45824, PPCASM_OP_PS_DIV);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 5);
	checkOpFPR(1, 4);
	checkOpFPR(2, 11);

	// PS_MERGE10
	_testAsm(0x10CC6C20, "ps_merge00 f6, f12, f13");
	disassemble(0x10CC6C20, PPCASM_OP_PS_MERGE00);
	checkOperandMask(true, true, true);
	checkOpFPR(0, 6);
	checkOpFPR(1, 12);
	checkOpFPR(2, 13);

	// random extra tests
	_testAsm(0x419D0040, "bgt cr7, .+0x40");
	_testAsm(0x50AB042E, "rlwimi r11, r5, 0,16,23");
	_testAsm(0xFF810000, "fcmpu cr7, f1, f0");
	_testAsm(0xFC010000, "fcmpu cr0, f1, f0");
	_testAsm(0x7F845000, "cmpw cr7, r4, r10");
	_testAsm(0x2C090000, "cmpwi r9, 0"); // implicit cr0
	_testAsm(0x2C090000, "cmpwi cr0, r9, 0");
	_testAsm(0x2F9E001F, "cmpwi cr7, r30, 0x1F");
	_testAsm(0x7D573850, "subf r10, r23, r7");
	_testAsm(0x7D573850, "sub r10, r7, r23"); // alias for subf
	_testAsm(0x7D862851, "subf. r12, r6, r5");
	_testAsm(0x7D7C1896, "mulhw r11, r28, r3");
	_testAsm(0x7D436016, "mulhwu r10, r3, r12");
	_testAsm(0x7FE318F8, "nor r3, r31, r3");
	_testAsm(0x7D29F8F8, "nor r9, r9, r31");
	_testAsm(0x7F26C8F8, "nor r6, r25, r25");
	_testAsm(0x7F26C8F8, "not r6, r25"); // alias for nor where rA == rB
	_testAsm(0x7FE4FB78, "mr r4, r31"); // alias for or where rA == rB
	_testAsm(0x7C7EEAA6, "mfspr r3, spr958");
	_testAsm(0x7C78E3A6, "mtspr spr920, r3");
	_testAsm(0x7D6802A6, "mflr r11");
	_testAsm(0x7E6902A6, "mfctr r19");
	_testAsm(0x7D430034, "cntlzw r3, r10");
	_testAsm(0x7C640774, "extsb r4, r3");
	_testAsm(0x1C8A00B8, "mulli r4, r10, 0xB8");
	_testAsm(0x1CFEFFF8, "mulli r7, r30, -8");
	_testAsm(0x1CFE8000, "mulli r7, r30, -0x8000");
	_testAsm(0x1CFE7FFF, "mulli r7, r30, 0x7FFF");
	_testAsmFail("mulli r7, r30, -0x8001");
	_testAsmFail("mulli r7, r30, 0x8000");
	_testAsm(0x38E0FFFF, "li r7, 0xFFFF"); // li/lis allows both signed and unsigned 16-bit range
	_testAsm(0x38E0FFFF, "li r7, -1");
	_testAsm(0x38E08000, "li r7, -0x8000");
	_testAsmFail("li r7, -0x8001");
	_testAsmFail("li r7, 0xFFFF+1");

	// test set 2
	_testAsm(0x7c0903a6, "mtctr     r0");
	_testAsm(0x7c6903a6, "mtctr     r3");
	_testAsm(0x7d0903a6, "mtctr     r8");
	_testAsm(0x7d2903a6, "mtctr     r9");
	_testAsm(0x7c0803a6, "mtlr      r0");
	_testAsm(0x7c6803a6, "mtlr      r3");
	_testAsm(0x7c8803a6, "mtlr      r4");
	_testAsm(0x7c065896, "mulhw     r0, r6, r11");
	_testAsm(0x7ce85096, "mulhw     r7, r8, r10");
	_testAsm(0x7d494096, "mulhw     r10, r9, r8");
	_testAsm(0x7ca4c016, "mulhwu    r5, r4, r24");
	_testAsm(0x7ce53016, "mulhwu    r7, r5, r6");
	_testAsm(0x7d373816, "mulhwu    r9, r23, r7");
	_testAsm(0x7d664816, "mulhwu    r11, r6, r9");
	_testAsm(0x1d2ae09e, "mulli     r9, r10, -0x1F62");
	_testAsm(0x1fe900ff, "mulli     r31, r9, 0xFF");
	_testAsm(0x1ffefffe, "mulli     r31, r30, -2");
	_testAsm(0x7e4531d6, "mullw     r18, r5, r6");
	_testAsm(0x7fe429d6, "mullw     r31, r4, r5");
	_testAsm(0x7d0951d7, "mullw.    r8, r9, r10");
	_testAsm(0x7f0941d7, "mullw.    r24, r9, r8");
	_testAsm(0x7c6600d0, "neg       r3, r6");
	_testAsm(0x7df600d0, "neg       r15, r22");
	_testAsm(0x7eeb00d0, "neg       r23, r11");
	_testAsm(0x7ca428f8, "not       r4, r5");
	_testAsm(0x7df578f8, "not       r21, r15");
	_testAsm(0x7fe9f8f8, "not       r9, r31");
	_testAsm(0x7fc83378, "or        r8, r30, r6");
	_testAsm(0x7de6a379, "or.       r6, r15, r20");
	_testAsm(0x7f29fb79, "or.       r9, r25, r31");
	_testAsm(0x7ceafb38, "orc       r10, r7, r31");
	_testAsm(0x7fbe4338, "orc       r30, r29, r8");
	_testAsm(0x63a90001, "ori       r9, r29, 1");
	_testAsm(0x63c3b46e, "ori       r3, r30, 0xB46E");
	_testAsm(0x63e4ffff, "ori       r4, r31, 0xFFFF");
	_testAsm(0x650a8000, "oris      r10, r8, 0x8000");
	_testAsm(0x676c792e, "oris      r12, r27, 0x792E");
	_testAsm(0x6788ffff, "oris      r8, r28, 0xFFFF");
	_testAsm(0x67ea3fe0, "oris      r10, r31, 0x3FE0");
	_testAsm(0x51231fb8, "rlwimi    r3, r9, 3,30,28");
	_testAsm(0x5249fefe, "rlwimi    r9, r18, 31,27,31");
	_testAsm(0x52513a20, "rlwimi    r17, r18, 7,8,16");
	_testAsm(0x53494420, "rlwimi    r9, r26, 8,16,16");
	_testAsm(0x53657269, "rlwimi.   r5, r27, 14,9,20");
	_testAsm(0x546906f2, "rlwinm    r9, r3, 0,27,25");
	_testAsm(0x55284ad6, "rlwinm    r8, r9, 9,11,11");
	_testAsm(0x57c90428, "rlwinm    r9, r30, 0,16,20");
	_testAsm(0x57e5d57a, "rlwinm    r5, r31, 26,21,29");
	_testAsm(0x57ea34b0, "rlwinm    r10, r31, 6,18,24");
	_testAsm(0x54e1bbb1, "rlwinm.   r1, r7, 23,14,24");
	_testAsm(0x550707fb, "rlwinm.   r7, r8, 0,31,29");
	_testAsm(0x552003f1, "rlwinm.   r0, r9, 0,15,24");
	_testAsm(0x5527022f, "rlwinm.   r7, r9, 0,8,23");
	_testAsm(0x55270777, "rlwinm.   r7, r9, 0,29,27");
	_testAsm(0x552a0777, "rlwinm.   r10, r9, 0,29,27");
	_testAsm(0x552a07b9, "rlwinm.   r10, r9, 0,30,28");
	_testAsm(0x5d5f583e, "rotlw     r31, r10, r11");
	_testAsm(0x5eea703e, "rotlw     r10, r23, r14");
	_testAsm(0x5f0ab03e, "rotlw     r10, r24, r22");
	_testAsm(0x541a783e, "rotlwi    r26, r0, 15");
	_testAsm(0x5488383e, "rotlwi    r8, r4, 7");
	_testAsm(0x5779683e, "rotlwi    r25, r27, 13");
	_testAsm(0x54e4c83e, "rotrwi    r4, r7, 7");
	_testAsm(0x7eeaf830, "slw       r10, r23, r31");
	_testAsm(0x7f68c030, "slw       r8, r27, r24");
	_testAsm(0x7fe8f030, "slw       r8, r31, r30");
	_testAsm(0x7f484831, "slw.      r8, r26, r9");
	_testAsm(0x7f844031, "slw.      r4, r28, r8");
	_testAsm(0x57e42036, "slwi      r4, r31, 4");
	_testAsm(0x57e42834, "slwi      r4, r31, 5");
	_testAsm(0x57f37820, "slwi      r19, r31, 15");
	_testAsm(0x5488083d, "slwi.     r8, r4, 1");
	_testAsm(0x552a3033, "slwi.     r10, r9, 6");
	_testAsm(0x5647083d, "slwi.     r7, r18, 1");
	_testAsm(0x7c879e30, "sraw      r7, r4, r19");
	_testAsm(0x7c891e30, "sraw      r9, r4, r3");
	_testAsm(0x7fca4e30, "sraw      r10, r30, r9");
	_testAsm(0x7d380e70, "srawi     r24, r9, 1");
	_testAsm(0x7c79fe71, "srawi.    r25, r3, 0x1F");
	_testAsm(0x7fb91e71, "srawi.    r25, r29, 3");
	_testAsm(0x7cc33c30, "srw       r3, r6, r7");
	_testAsm(0x7d09e430, "srw       r9, r8, r28");
	_testAsm(0x7d3d4430, "srw       r29, r9, r8");
	_testAsm(0x5403d97e, "srwi      r3, r0, 5");
	_testAsm(0x57dc843e, "srwi      r28, r30, 16");
	_testAsm(0x552a463f, "srwi.     r10, r9, 24");
	_testAsm(0x5532c9ff, "srwi.     r18, r9, 7");
	_testAsm(0x57f8f87f, "srwi.     r24, r31, 1");
	_testAsm(0x9be80201, "stb       r31, 0x201(r8)");
	_testAsm(0x9bfafffc, "stb       r31, -4(r26)");
	_testAsm(0x9bfeffff, "stb       r31, -1(r30)");
	_testAsm(0x9dc3ffff, "stbu      r14, -1(r3)");
	_testAsm(0x9e55006f, "stbu      r18, 0x6F(r21)");
	_testAsm(0x9fdc2008, "stbu      r30, 0x2008(r28)");
	_testAsm(0x7ce521ee, "stbux     r7, r5, r4");
	_testAsm(0x7d4919ee, "stbux     r10, r9, r3");
	_testAsm(0x7f6919ee, "stbux     r27, r9, r3");
	_testAsm(0x7c0531ae, "stbx      r0, r5, r6");
	_testAsm(0x7d4941ae, "stbx      r10, r9, r8");
	_testAsm(0x7fdfe9ae, "stbx      r30, r31, r29");
	_testAsm(0xdb690038, "stfd      f27, 0x38(r9)");
	_testAsm(0xdb890040, "stfd      f28, 0x40(r9)");
	_testAsm(0xde230008, "stfdu     f17, 8(r3)");
	_testAsm(0x7c1235ae, "stfdx     f0, r18, r6");
	_testAsm(0x7c1f3dae, "stfdx     f0, r31, r7");
	_testAsm(0x7c1f45ae, "stfdx     f0, r31, r8");
	_testAsm(0x7c001fae, "stfiwx    f0, 0, r3");
	_testAsm(0x7c3e4fae, "stfiwx    f1, r30, r9");
	_testAsm(0x7fa04fae, "stfiwx    f29, 0, r9");
	_testAsm(0xd008fffc, "stfs      f0, -4(r8)");
	_testAsm(0xd3ff1604, "stfs      f31, 0x1604(r31)");
	_testAsm(0xd41e001c, "stfsu     f0, 0x1C(r30)");
	_testAsm(0x7d87356e, "stfsux    f12, r7, r6");
	_testAsm(0x7c104d2e, "stfsx     f0, r16, r9");
	_testAsm(0x7f5d452e, "stfsx     f26, r29, r8");
	_testAsm(0xb3bf000e, "sth       r29, 0xE(r31)");
	_testAsm(0xb3f8fffc, "sth       r31, -4(r24)");
	_testAsm(0xb3fa0000, "sth       r31, 0(r26)");
	_testAsm(0xb3fd0000, "sth       r31, 0(r29)");
	_testAsm(0xb528105a, "sthu      r9, 0x105A(r8)");
	_testAsm(0xb52afffe, "sthu      r9, -2(r10)");
	_testAsm(0xb53f0002, "sthu      r9, 2(r31)");
	_testAsm(0x7c69536e, "sthux     r3, r9, r10");
	_testAsm(0x7c8a3b6e, "sthux     r4, r10, r7");
	_testAsm(0x7c0ca32e, "sthx      r0, r12, r20");
	_testAsm(0x7d242b2e, "sthx      r9, r4, r5");
	_testAsm(0x7fe93b2e, "sthx      r31, r9, r7");
	_testAsm(0xbdd0b4d0, "stmw      r14, -0x4B30(r16)");
	_testAsm(0xbed182d1, "stmw      r22, -0x7D2F(r17)");
	_testAsm(0x7cbb65aa, "stswi     r5, r27, 0xC");
	_testAsm(0x7cbe05aa, "stswi     r5, r30, 0");
	_testAsm(0x91348378, "stw       r9, -0x7C88(r20)");
	_testAsm(0x93e9fddc, "stw       r31, -0x224(r9)");
	_testAsm(0x93fe45c8, "stw       r31, 0x45C8(r30)");
	_testAsm(0x7c604d2c, "stwbrx    r3, 0, r9");
	_testAsm(0x7d20f52c, "stwbrx    r9, 0, r30");
	_testAsm(0x7fa04d2c, "stwbrx    r29, 0, r9");
	_testAsm(0x7d20512d, "stwcx.    r9, 0, r10");
	_testAsm(0x94650208, "stwu      r3, 0x208(r5)");
	_testAsm(0x953ffffc, "stwu      r9, -4(r31)");
	_testAsm(0x97fe0208, "stwu      r31, 0x208(r30)");
	_testAsm(0x7d21196e, "stwux     r9, r1, r3");
	_testAsm(0x7d23516e, "stwux     r9, r3, r10");
	_testAsm(0x7d41496e, "stwux     r10, r1, r9");
	_testAsm(0x7cbc512e, "stwx      r5, r28, r10");
	_testAsm(0x7d5ce92e, "stwx      r10, r28, r29");
	_testAsm(0x7ffee92e, "stwx      r31, r30, r29");
	_testAsm(0x7c1ec050, "subf      r0, r30, r24");
	_testAsm(0x7fbee050, "subf      r29, r30, r28");
	_testAsm(0x7fe91850, "subf      r31, r9, r3");
	_testAsm(0x7c1e5851, "subf.     r0, r30, r11");
	_testAsm(0x7fc42851, "subf.     r30, r4, r5");
	_testAsm(0x7fc74851, "subf.     r30, r7, r9");
	_testAsm(0x7cffd810, "subfc     r7, r31, r27");
	_testAsm(0x7d053010, "subfc     r8, r5, r6");
	_testAsm(0x7d093810, "subfc     r8, r9, r7");
	_testAsm(0x7f884910, "subfe     r28, r8, r9");
	_testAsm(0x7fe95110, "subfe     r31, r9, r10");
	_testAsm(0x7fea4910, "subfe     r31, r10, r9");
	_testAsm(0x20d0b2d1, "subfic    r6, r16, -0x4D2F");
	_testAsm(0x2109fc02, "subfic    r8, r9, -0x3FE");
	_testAsm(0x2144001f, "subfic    r10, r4, 0x1F");
	_testAsm(0x7c04f278, "xor       r4, r0, r30");
	_testAsm(0x7c08fa78, "xor       r8, r0, r31");
	_testAsm(0x7c0b4a78, "xor       r11, r0, r9");
	_testAsm(0x68640008, "xori      r4, r3, 8");
	_testAsm(0x68652064, "xori      r5, r3, 0x2064");
	_testAsm(0x692076c3, "xori      r0, r9, 0x76C3");
	_testAsm(0x6c0a8000, "xoris     r10, r0, 0x8000");
	_testAsm(0x6c68ffff, "xoris     r8, r3, 0xFFFF");
	_testAsm(0x6c617267, "xoris     r1, r3, 0x7267");
	_testAsm(0x6fe98000, "xoris     r9, r31, 0x8000");
	_testAsm(0x6fe9f000, "xoris     r9, r31, 0xF000");
	_testAsm(0x6fe9ff00, "xoris     r9, r31, 0xFF00");
	_testAsm(0x6fe9ffff, "xoris     r9, r31, 0xFFFF");

	// data directives
	_testAsmArray({ 0x00, 0x00, 0x00, 0x01 }, ".int 1");
	_testAsmArray({ 0x00, 0x00, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44 }, ".int 1, 0x11223344");
	_testAsmArray({ 0x42, 0xf6, 0x00, 0x00 }, ".float 123.0");
	_testAsmArray({ 0x7f }, ".byte 0x7f");
	_testAsmArray({ 0x74, 0x65, 0x73, 0x74, 0x00 }, ".byte \"test\"");
	_testAsmArray({ 0x41, 0x42, 0x43, 0x00, 0x74, 0x65, 0x73, 0x74, 0x00 }, ".byte \"ABC\", \"test\"");

}

void ppcAsmTest()
{
#ifdef CEMU_DEBUG_ASSERT
	ppcAsmTestDisassembler();
#endif
}
