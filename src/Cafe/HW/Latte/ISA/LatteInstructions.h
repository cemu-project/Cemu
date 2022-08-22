#pragma once
#include "Cafe/HW/Latte/Core/LatteConst.h"

namespace Latte
{
	using GPRType = uint8;
};

class LatteCFInstruction
{
public:
	enum class CF_COND
	{
		CF_COND_ACTIVE = 0,
		CF_COND_FALSE = 1,
		CF_COND_BOOL = 2,
		CF_COND_NOT_BOOL = 3,
	};

	enum OPCODE
	{
		// SQ_CF_INST_*
		INST_NOP = 0x00,
		INST_TEX = 0x01,
		INST_VTX = 0x02, // vertex fetch clause, used only in GS copy program?
		INST_VTX_TC = 0x03, // vertex fetch clause, through texture cache
		INST_LOOP_START = 0x04, // DX9 style loop
		INST_LOOP_END = 0x05,
		INST_LOOP_START_DX10 = 0x06,
		INST_LOOP_BREAK = 0x09,
		INST_JUMP = 0x0A,
		INST_ELSE = 0x0D,
		INST_POP = 0x0E,
		INST_ELSE_AFTER = 0x0F,
		INST_CALL = 0x12,
		INST_CALL_FS = 0x13,
		INST_RETURN = 0x14,
		INST_EMIT_VERTEX = 0x15, // only available in geometry shader
		INST_MEM_STREAM0_WRITE = 0x20, // for stream out (index selects buffer)
		INST_MEM_STREAM1_WRITE = 0x21,
		INST_MEM_STREAM2_WRITE = 0x22,
		INST_MEM_STREAM3_WRITE = 0x23,
		INST_MEM_RING_WRITE = 0x26, // used to pass data to/from geometry shader
		INST_EXPORT = 0x27,
		INST_EXPORT_DONE = 0x28, // last export

		// ALU instructions
		MASK_ALU = 0x40, // mask to differentiate ALU instructions
		INST_ALU = (0x08 | MASK_ALU),
		INST_ALU_PUSH_BEFORE = (0x09 | MASK_ALU),
		INST_ALU_POP_AFTER = (0x0A | MASK_ALU),
		INST_ALU_POP2_AFTER = (0x0B | MASK_ALU),
		// reserved
		INST_ALU_CONTINUE = (0x0D | MASK_ALU),
		INST_ALU_BREAK = (0x0E | MASK_ALU),
		INST_ALU_ELSE_AFTER = (0x0F | MASK_ALU),
	};

	OPCODE getField_Opcode() const
	{
		uint32 cf_inst23_7 = (word1 >> 23) & 0x7F;
		// check the bigger opcode fields first
		if (cf_inst23_7 < 0x40) // starting at 0x40 the bits overlap with the ALU instruction encoding
		{
			// cf_inst23_7 is opcode
			return (OPCODE)cf_inst23_7;
		}
		uint32 cf_inst26_4 = ((word1 >> 26) & 0xF);
		// cf_inst26_4 is ALU opcode
		return (OPCODE)(cf_inst26_4 | OPCODE::MASK_ALU);
	}

	bool getField_END_OF_PROGRAM() const
	{
		// shared by all CF instruction types except ALU
		cemu_assert_debug((getField_Opcode() & OPCODE::MASK_ALU) != OPCODE::MASK_ALU);
		return ((word1 >> 21) & 1) != 0;
	}

	const class LatteCFInstruction_ALU* getParser_ALU() const
	{
		cemu_assert_debug((getField_Opcode() & OPCODE::MASK_ALU) == OPCODE::MASK_ALU);
		return (const LatteCFInstruction_ALU*)this;
	}

	// EXPORT is for:
	// SQ_CF_INST_MEM_STREAM0 - SQ_CF_INST_MEM_STREAM3
	// SQ_CF_INST_MEM_SCRATCH
	// SQ_CF_INST_MEM_REDUCTION
	// SQ_CF_INST_MEM_RING
	// SQ_CF_INST_EXPORT
	// SQ_CF_INST_EXPORT_DONE
	const class LatteCFInstruction_EXPORT_IMPORT* getParser_EXPORT() const
	{
		return (const LatteCFInstruction_EXPORT_IMPORT*)this;
	}

	template<typename TCFEncoding>
	const TCFEncoding* getParserIfOpcodeMatch() const
	{
		auto opcode = getField_Opcode();
		if (TCFEncoding::MatchesOpcode(opcode))
			return (const TCFEncoding*)this;
		return nullptr;
	}

	// writing
	void setField_Opcode(OPCODE opcode)
	{
		cemu_assert_debug(((uint32)opcode & (uint32)OPCODE::MASK_ALU) == 0);
		word1 &= ~(0xF << 23);
		word1 |= ((uint32)opcode << 23);
	}

protected:
	uint32 word0;
	uint32 word1;
};

// default encoding, CF_DWORD0 + CF_DWORD1
// used for opcodes: See list in MatchesOpcode()
class LatteCFInstruction_DEFAULT : public LatteCFInstruction
{
public:
	LatteCFInstruction_DEFAULT()
	{
		word0 = 0;
		word1 = 0;
	}

	static bool MatchesOpcode(const OPCODE opcode)
	{
		return opcode == OPCODE::INST_NOP ||
			opcode == OPCODE::INST_VTX ||
			opcode == OPCODE::INST_VTX_TC ||
			opcode == OPCODE::INST_TEX ||
			opcode == OPCODE::INST_CALL_FS ||
			opcode == OPCODE::INST_CALL ||
			opcode == OPCODE::INST_RETURN ||
			opcode == OPCODE::INST_LOOP_START ||
			opcode == OPCODE::INST_LOOP_END ||
			opcode == OPCODE::INST_LOOP_START_DX10 ||
			//opcode == OPCODE::INST_LOOP_CONTINUE ||
			//opcode == OPCODE::INST_LOOP_BREAK ||
			opcode == OPCODE::INST_JUMP ||
			//opcode == OPCODE::INST_PUSH ||
			//opcode == OPCODE::INST_PUSH_ELSE ||
			opcode == OPCODE::INST_ELSE ||
			opcode == OPCODE::INST_POP ||
			//opcode == OPCODE::INST_POP_JUMP ||
			opcode == OPCODE::INST_JUMP ||
			//opcode == OPCODE::INST_POP_PUSH ||
			//opcode == OPCODE::INST_PUSH ||
			//opcode == OPCODE::INST_POP_PUSH_ELSE ||
			//opcode == OPCODE::INST_PUSH_ELSE ||
			opcode == OPCODE::INST_EMIT_VERTEX
			//opcode == OPCODE::INST_EMIT_CUT_VERTEX ||
			//opcode == OPCODE::INST_CUT_VERTEX ||
			//opcode == OPCODE::INST_KILL
			;
	}

	// returns offset in bytes
	uint32 getField_ADDR() const // returns offset in bytes
	{
		return word0 << 3;
	}

	uint32 getField_POP_COUNT() const
	{
		return (word1 >> 0) & 7;
	}

	uint32 getField_CF_CONST() const
	{
		return (word1 >> 3) & 0x1F;
	}

	CF_COND getField_COND() const
	{
		return (CF_COND)((word1 >> 8) & 0x3);
	}

	uint32 getField_COUNT() const
	{		
		uint32 count = (word1 >> 10) & 0x7; // R600 field
		count |= ((word1 >> 16)&0x8); // R700 has an extra bit at 19
		return count + 1;
	}

	uint32 getField_CALL_COUNT() const
	{
		return (word1 >> 13) & 0x3F;
	}

	uint32 getField_VALID_PIXEL_MODE() const
	{
		return (word1 >> 22) & 1;
	}

	uint32 getField_WHOLE_QUAD_MODE() const
	{
		return (word1 >> 30) & 1;
	}

	uint32 getField_BARRIER() const
	{
		return (word1 >> 31) & 1;
	}

	std::span<uint8> getClauseCode(std::span<uint8> programCode) const
	{
		cemu_assert_debug(getField_Opcode() == LatteCFInstruction::INST_VTX || getField_Opcode() == LatteCFInstruction::INST_VTX_TC);
		cemu_assert_debug(getField_ADDR() <= programCode.size());
		cemu_assert_debug((programCode.size() - getField_ADDR()) >= getField_COUNT() * 16);
		return programCode.subspan(getField_ADDR(), getField_COUNT() * 16);
	}

	// writing
	void setField_ADDR(uint32 addrInBytes) // in bytes
	{
		word0 = addrInBytes >> 3;
	}

	void setField_COUNT(uint32 count)
	{
		cemu_assert_debug(count > 0 && count <= 16);
		count--;
		word1 &= ~((0x7 << 10) | (1 << 19));
		word1 |= ((count & 0x7) << 10);
		word1 |= ((count << 16) & (1<<19));
	}

	void setField_BARRIER(bool isEnabled)
	{
		if (isEnabled)
			word1 |= (1 << 31);
		else
			word1 &= ~(1 << 31);
	}

};

// CF_ALLOC_EXPORT_DWORD0 + CF_ALLOC_EXPORT_DWORD1_BUF / CF_ALLOC_EXPORT_DWORD1_SWIZ
// this has two different encoding. Use isEncodingBUF() to determine which fields are valid
class LatteCFInstruction_EXPORT_IMPORT : public LatteCFInstruction // CF_ALLOC_EXPORT_DWORD1_SWIZ
{
public:
	static bool MatchesOpcode(const OPCODE opcode)
	{
		return opcode == OPCODE::INST_MEM_STREAM0_WRITE ||
			opcode == OPCODE::INST_MEM_STREAM1_WRITE ||
			opcode == OPCODE::INST_MEM_STREAM2_WRITE ||
			opcode == OPCODE::INST_MEM_STREAM3_WRITE ||
			//opcode == OPCODE::INST_MEM_SCRATCH ||
			//opcode == OPCODE::INST_MEM_REDUCTION ||
			opcode == OPCODE::INST_MEM_RING_WRITE ||
			opcode == OPCODE::INST_EXPORT ||
			opcode == OPCODE::INST_EXPORT_DONE;
	}

	enum EXPORT_TYPE : uint8
	{
		PIXEL = 0,
		POSITION = 1,
		PARAMETER = 2,
		UNUSED_VAL = 3
	};

	enum COMPSEL : uint8
	{
		X,
		Y,
		Z,
		W,
		CONST_0F,
		CONST_1F,
		RESERVED,
		MASKED
	};

	EXPORT_TYPE getField_TYPE() const
	{
		return (EXPORT_TYPE)((word0 >> 13) & 0x3);
	}

	uint32 getField_ARRAY_BASE() const
	{
		return (word0 >> 0) & 0x1FFF;
	}

	uint32 getField_INDEX_GPR() const
	{
		return (word0 >> 23) & 0x7F;
	}

	// read/write GPR (source/destination)
	uint32 getField_RW_GPR() const
	{
		return (word0 >> 15) & 0x7F;
	}

	// if true, RW_GPR is indexed
	bool getField_RW_REL() const
	{
		return ((word0 >> 22) & 0x1) != 0;
	}

	uint8 getField_BURST_COUNT() const
	{
		return ((word1 >> 17) & 0xF) + 1;
	}

	uint8 getField_ELEM_SIZE() const
	{
		return ((word0 >> 30) & 0x3) + 1;
	}

	// word1 bits 0-15 differ depending on BUF/SWIZ encoding
	// returns true if BUF encoding is used (BUF fields valid, SWIZ invalid). Otherwise SWIZ encoding is used (BUF fields invalid, SWIZ fields valid)
	bool isEncodingBUF() const
	{
		return ((word1 >> 12) & 0xF) != 0;
	}

	// fields specific to SWIZ encoding 

	COMPSEL getSwizField_SEL_X() const { cemu_assert_debug(!isEncodingBUF()); return (COMPSEL)((word1 >> 0) & 0x7); }
	COMPSEL getSwizField_SEL_Y() const { cemu_assert_debug(!isEncodingBUF()); return (COMPSEL)((word1 >> 3) & 0x7); }
	COMPSEL getSwizField_SEL_Z() const { cemu_assert_debug(!isEncodingBUF()); return (COMPSEL)((word1 >> 6) & 0x7); }
	COMPSEL getSwizField_SEL_W() const { cemu_assert_debug(!isEncodingBUF()); return (COMPSEL)((word1 >> 9) & 0x7); }

	// fields specific to BUF encoding (word1 bits 0-15)
	
	uint32 getBufField_ARRAY_SIZE() const
	{
		cemu_assert_debug(isEncodingBUF());
		return (word1 >> 0) & 0xFFF;
	}

	// applies only to writes
	uint32 getBufField_COMP_MASK() const
	{
		cemu_assert_debug(isEncodingBUF());
		return (word1 >> 12) & 0xF;
	}

	// these are not specific to EXPORT instr? Move to LatteCFInstruction?
	bool getValidPixelMode() const
	{
		return ((word1 >> 22) & 0x1) != 0;
	}

	bool getWholeQuadMode() const
	{
		return ((word1 >> 30) & 0x1) != 0;

	}
};

// encoding for CF_ALU_DWORD0 + CF_ALU_DWORD1
// used for: 
// CF_INST_ALU
// CF_INST_ALU_PUSH_BEFORE
// CF_INST_ALU_POP_AFTER
// CF_INST_ALU_POP2_AFTER
// CF_INST_ALU_CONTINUE
// CF_INST_ALU_BREAK
// CF_INST_ALU_ELSE_AFTER
class LatteCFInstruction_ALU : public LatteCFInstruction
{
public:
	static bool MatchesOpcode(const OPCODE opcode)
	{
		return opcode == OPCODE::INST_ALU ||
			opcode == OPCODE::INST_ALU_PUSH_BEFORE ||
			opcode == OPCODE::INST_ALU_POP_AFTER ||
			opcode == OPCODE::INST_ALU_POP2_AFTER ||
			opcode == OPCODE::INST_ALU_CONTINUE ||
			opcode == OPCODE::INST_ALU_BREAK ||
			opcode == OPCODE::INST_ALU_ELSE_AFTER;
	}

	uint32 getField_ADDR() const
	{
		return (word0 >> 0) & 0x3FFFFF;
	}

	uint32 getField_COUNT() const
	{
		return ((word1 >> 18) & 0x7F) + 1;
	}

	uint32 getField_KCACHE_BANK0() const
	{
		return (word0 >> 22) & 0xF;
	}

	uint32 getField_KCACHE_BANK1() const
	{
		return (word0 >> 26) & 0xF;
	}

	uint32 getField_KCACHE_ADDR0() const
	{
		return (word1 >> 2) & 0xFF;
	}

	uint32 getField_KCACHE_ADDR1() const
	{
		return (word1 >> 10) & 0xFF;
	}

	bool getField_USES_WATERFALL() const
	{
		return ((word1 >> 25)&1) !=0;
	}

	// todo - KCACHE_MODE0, KCACHE_MODE1, WHOLE_QUAD_MODE, BARRIER
};

static_assert(sizeof(LatteCFInstruction) == 8);
static_assert(sizeof(LatteCFInstruction_DEFAULT) == 8);
static_assert(sizeof(LatteCFInstruction_EXPORT_IMPORT) == 8);
static_assert(sizeof(LatteCFInstruction_ALU) == 8);

/* Latte instructions */

class LatteClauseInstruction_VTX // used by CF VTX and VTX_TC clauses
{
public:
	LatteClauseInstruction_VTX()
	{
		word0 = 0;
		word1 = 0;
		word2 = 0;
		word3 = 0;
	}

	// VTX_DWORD0
	enum class VTX_INST
	{
		_VTX_INST_FETCH = 0,
		_VTX_INST_SEMANTIC = 1,
		_VTX_INST_MEM = 2
	};

	enum class SRC_SEL
	{
		SEL_X = 0,
		SEL_Y = 1,
		SEL_Z = 2,
		SEL_W = 3,
	};

	enum class DST_SEL
	{
		SEL_X = 0,
		SEL_Y = 1,
		SEL_Z = 2,
		SEL_W = 3,
		SEL_0 = 4, // constant 0.0
		SEL_1 = 5, // constant 1.0
		SEL_RESERVED = 6,
		SEL_MASK = 7,
	};

	enum class NUM_FORMAT_ALL // NFA
	{
		NUM_FORMAT_NORM = 0, // normalized to float (-1.0 to 1.0 for signed, 0.0 to 1.0 for unsigned)
		NUM_FORMAT_INT = 1, // interpreted as integer
		NUM_FORMAT_SCALED = 2,
	};

	enum class FORMAT_COMP
	{
		COMP_UNSIGNED = 0,
		COMP_SIGNED = 1,
	};

	enum class SRF_MODE
	{
		SRF_MODE_ZERO_CLAMP_MINUS_ONE = 0,
		SRF_MODE_NO_ZERO = 1,
	};

	// fields todo:
	// FETCH_WHOLE_QUAD
	// MEGA_FETCH_COUNT
	// MEGA_FETCH (word2)
	// ALT_CONST (word2)

	VTX_INST getField_VTX_INST() const // alias opcode
	{
		return (VTX_INST)((word0 >> 0) & 0x1F);
	}

	LatteConst::VertexFetchType2 getField_FETCH_TYPE() const
	{
		return (LatteConst::VertexFetchType2)((word0 >> 5) & 0x3);
	}

	uint32 getField_BUFFER_ID() const
	{
		return (word0 >> 8) & 0xFF;
	}

	uint32 getField_SRC_GPR() const
	{
		return (word0 >> 16) & 0x7F;
	}
	
	bool getField_SRC_REL() const
	{
		return ((word0 >> 23) & 1) != 0;
	}

	SRC_SEL getField_SRC_SEL_X() const
	{
		return (SRC_SEL)((word0 >> 24) & 0x3);
	}

	// WORD1 depends on instruction type but some fields are shared
	// VTX_DWORD1 / VTX_DWORD1_GPR / VTX_DWORD1_SEM

	DST_SEL getField_DST_SEL(uint32 index) const // shared field
	{
		cemu_assert_debug(index <= 3);
		return (DST_SEL)((word1 >> (9 + index*3)) & 0x7);
	}

	bool getField_USE_CONST_FIELDS() const  // shared field
	{
		return ((word1 >> 21) & 1) != 0;
	}

	LatteConst::VertexFetchFormat getField_DATA_FORMAT() const // shared field
	{
		return (LatteConst::VertexFetchFormat)((word1 >> 22) & 0x3F);
	}

	NUM_FORMAT_ALL getField_NUM_FORMAT_ALL() const // shared field
	{
		return (NUM_FORMAT_ALL)((word1 >> 28) & 3);
	}

	FORMAT_COMP getField_FORMAT_COMP_ALL() const // shared field
	{
		return (FORMAT_COMP)((word1 >> 30) & 1);
	}

	SRF_MODE getField_SRF_MODE_ALL() const // shared field
	{
		return (SRF_MODE)((word1 >> 30) & 1);
	}

	// VTX_DWORD1_SEM specific fields (VTX_INST_SEMANTIC)
	uint32 getFieldSEM_SEMANTIC_ID() const
	{
		return ((word1 >> 0) & 0xFF);
	}

	// VTX_DWORD1_GPR specific fields (VTX_INST_FETCH?)
	// todo

	// VTX_DWORD2
	uint32 getField_OFFSET() const // shared field
	{
		return ((word2 >> 0) & 0xFFFF);
	}

	LatteConst::VertexFetchEndianMode getField_ENDIAN_SWAP() const // shared field
	{
		return (LatteConst::VertexFetchEndianMode)((word2 >> 16) & 0x3);
	}

	bool getField_CONST_BUF_NO_STRIDE() const // shared field
	{
		return ((word2 >> 18) & 0x1) != 0;
	}

	// writing
	LatteClauseInstruction_VTX& setField_VTX_INST(VTX_INST inst)
	{
		word0 &= ~(0x1F << 0);
		word0 |= ((uint32)inst << 0);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_FETCH_TYPE(LatteConst::VertexFetchType2 fetchType)
	{
		word0 &= ~(0x3 << 5);
		word0 |= ((uint32)fetchType << 5);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_BUFFER_ID(uint32 bufferId)
	{
		word0 &= ~(0xFF << 8);
		word0 |= (bufferId << 8);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_SRC_GPR(uint32 srcGPR)
	{
		word0 &= ~(0x7F << 16);
		word0 |= (srcGPR << 16);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_SRC_REL(bool isRel)
	{
		if(isRel)
			word0 |= (1 << 23);
		else
			word0 &= ~(0x1 << 23);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_SRC_SEL_X(SRC_SEL srcSel)
	{
		word0 &= ~(0x3 << 24);
		word0 |= ((uint32)srcSel << 24);
		return *this;
	}

	LatteClauseInstruction_VTX& setFieldSEM_SEMANTIC_ID(uint32 semanticId)
	{
		cemu_assert_debug(semanticId <= 0xFF);
		word1 &= ~(0xFF);
		word1 |= ((uint32)semanticId);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_OFFSET(uint32 offset)
	{
		cemu_assert_debug(offset < 0x10000);
		word2 &= ~(0xFFFF << 0);
		word2 |= ((uint32)offset);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_DATA_FORMAT(LatteConst::VertexFetchFormat fetchFormat)
	{
		 word1 &= ~(0x3F << 22);
		 word1 |= ((uint32)fetchFormat << 22);
		 return *this;
	}

	LatteClauseInstruction_VTX& setField_NUM_FORMAT_ALL(NUM_FORMAT_ALL nfa)
	{
		word1 &= ~(0x3 << 28);
		word1 |= ((uint32)nfa << 28);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_FORMAT_COMP_ALL(FORMAT_COMP componentFormat)
	{
		word1 &= ~(0x1 << 30);
		word1 |= ((uint32)componentFormat << 30);
		return *this;
	}

	LatteClauseInstruction_VTX& setField_DST_SEL(uint32 index, DST_SEL dstSel)
	{
		cemu_assert_debug(index <= 3);
		word1 &= ~(0x7 << (9 + index * 3));
		word1 |= ((uint32)dstSel << (9 + index * 3));
		return *this;
	}

	LatteClauseInstruction_VTX& setField_ENDIAN_SWAP(LatteConst::VertexFetchEndianMode endianSwap)
	{
		word2 &= ~(0x3 << 16);
		word2 |= ((uint32)endianSwap << 16);
		return *this;
	}


protected:
	uint32 word0;
	uint32 word1;
	uint32 word2;
	uint32 word3; // not used
};

static_assert(sizeof(LatteClauseInstruction_VTX) == 16);

class LatteClauseInstruction_ALU
{
public:
	enum OPCODE_OP2
	{
		ADD = 0x00,
		MUL = 0x01,
		MUL_IEEE = 0x02,
		//#define ALU_OP2_INST_MAX			(0x003)
		//#define ALU_OP2_INST_MIN			(0x004)
		//#define ALU_OP2_INST_MAX_DX10		(0x005)
		//#define ALU_OP2_INST_SETE			(0x008)
		//#define ALU_OP2_INST_SETGT			(0x009)
		//#define ALU_OP2_INST_SETGE			(0x00A)
		//#define ALU_OP2_INST_SETNE			(0x00B)
		//#define ALU_OP2_INST_SETE_DX10		(0x00C)
		//#define ALU_OP2_INST_SETGT_DX10		(0x00D)
		//#define ALU_OP2_INST_SETGE_DX10		(0x00E)
		//#define ALU_OP2_INST_SETNE_DX10		(0x00F)
		//#define ALU_OP2_INST_FLOOR			(0x014)
		//#define ALU_OP2_INST_FRACT			(0x010)
		//#define ALU_OP2_INST_TRUNC			(0x011)
		//#define ALU_OP2_INST_RNDNE			(0x013)
		//#define ALU_OP2_INST_MOVA_FLOOR		(0x016) // changes address register
		//#define ALU_OP2_INST_MOVA_INT		(0x018) // changes address register
		MOV = 0x19,
		//#define ALU_OP2_INST_NOP			(0x01A)
		//#define ALU_OP2_INST_PRED_SETE		(0x020)
		//#define ALU_OP2_INST_PRED_SETGT		(0x021)
		//#define ALU_OP2_INST_PRED_SETGE		(0x022)
		//#define ALU_OP2_INST_PRED_SETNE		(0x023)
		//#define ALU_OP2_INST_AND_INT		(0x030) // integer instruction
		//#define ALU_OP2_INST_OR_INT			(0x031) // integer instruction
		//#define ALU_OP2_INST_XOR_INT		(0x032) // integer instruction
		//#define ALU_OP2_INST_NOT_INT		(0x033) // integer instruction
		//#define ALU_OP2_INST_ADD_INT		(0x034) // integer instruction
		//#define ALU_OP2_INST_SUB_INT		(0x035) // integer instruction
		//#define ALU_OP2_INST_MAX_INT		(0x036) // integer instruction
		//#define ALU_OP2_INST_MIN_INT		(0x037) // integer instruction
		//#define ALU_OP2_INST_SETE_INT		(0x03A) // integer instruction
		//#define ALU_OP2_INST_SETGT_INT		(0x03B) // integer instruction
		//#define ALU_OP2_INST_SETGE_INT		(0x03C) // integer instruction
		//#define ALU_OP2_INST_SETNE_INT		(0x03D) // integer instruction
		//#define ALU_OP2_INST_SETGT_UINT		(0x03E) // integer instruction
		//#define ALU_OP2_INST_SETGE_UINT		(0x03F) // integer instruction
		//#define ALU_OP2_INST_PRED_SETE_INT	(0x042) // integer instruction 
		//#define ALU_OP2_INST_PRED_SETGT_INT	(0x043) // integer instruction
		//#define ALU_OP2_INST_PRED_SETGE_INT	(0x044) // integer instruction
		//#define ALU_OP2_INST_PRED_SETNE_INT	(0x045) // integer instruction 
		//#define ALU_OP2_INST_KILLE			(0x02C)
		//#define ALU_OP2_INST_KILLGT			(0x02D)
		//#define ALU_OP2_INST_KILLGE			(0x02E)
		//#define ALU_OP2_INST_KILLE_INT		(0x046)
		//#define ALU_OP2_INST_KILLGT_INT		(0x047)
		//#define ALU_OP2_INST_KILLNE_INT		(0x049)
		DOT4 = 0x50,
		//#define ALU_OP2_INST_DOT4_IEEE		(0x051)
		CUBE = 0x52,
		EXP_IEEE = 0x61,
		LOG_CLAMPED = 0x62,
		LOG_IEEE = 0x63,
		SQRT_IEEE = 0x6A,
		SIN = 0x06E,
		COS = 0x06F,
		RECIP_FF = 0x65,
		RECIP_IEEE = 0x66,
		RECIPSQRT_CLAMPED = 0x67,
		RECIPSQRT_FF = 0x68,
		RECIPSQRT_IEEE = 0x69,
		FLT_TO_INT = 0x6B,
		INT_TO_FLOAT = 0x6C,
		UINT_TO_FLOAT = 0x6D,
		ASHR_INT = 0x70,
		LSHR_INT = 0x71,
		LSHL_INT = 0x72,
		MULLO_INT = 0x73,
		MULLO_UINT = 0x75,
		FLT_TO_UINT = 0x79,
	};

	bool isOP3() const
	{
		uint32 alu_inst13_5 = (word1 >> 13) & 0x1F;
		return alu_inst13_5 >= 0x8;
	}

	OPCODE_OP2 getOP2Code() const
	{
		uint32 alu_inst7_11 = (word1 >> 7) & 0x7FF;
		return (OPCODE_OP2)alu_inst7_11;
	}

	bool isLastInGroup() const
	{
		return (word0 & 0x80000000) != 0;
	}

	const class LatteClauseInstruction_ALU_OP2* getOP2Instruction() const
	{
		return (const class LatteClauseInstruction_ALU_OP2*)this;
	}

protected:
	const uint32 word0 = 0;
	const uint32 word1 = 0;
};

class LatteALUSrcSel
{
public:
	LatteALUSrcSel(const uint16 op) : m_op(op) {};

	bool isGPR() const { return m_op < 128; };
	bool isAnyConst() const { return m_op >= 248 && m_op <= 252; };
	bool isConst_0F() const { return m_op == 248; };
	bool isLiteral() const { return m_op == 253; };
	bool isCFile() const { return m_op >= 256; };

	uint32 getGPR() const { return m_op; };
	uint32 getCFile() const { return (m_op & 0xFF); };

private:
	const uint16 m_op; // 0 - 511
};

class LatteClauseInstruction_ALU_OP2 : public LatteClauseInstruction_ALU
{
public:
	uint32 getDestGpr() const
	{
		return (word1 >> 21) & 0x7F;
	}

	uint32 getDestElem() const
	{
		return (word1 >> 29) & 3;
	}

	bool getDestRel() const
	{
		return ((word1 >> 28) & 1) != 0;
	}

	bool getDestClamp() const
	{
		return ((word1 >> 31) & 1) != 0;
	}

	bool getWriteMask() const
	{
		return ((word1 >> 4) & 1) != 0;
	}

	uint8 getOMod() const // use enum?
	{
		return (word1 >> 5) & 3;
	}

	const LatteALUSrcSel getSrc0Sel() const
	{
		return LatteALUSrcSel((word0 >> 0) & 0x1FF);
	}

	const uint8 getSrc0Chan() const
	{
		return (word0 >> 10) & 0x3;
	}

	const LatteALUSrcSel getSrc1Sel() const
	{
		return LatteALUSrcSel((word0 >> 13) & 0x1FF);
	}

	const uint8 getSrc1Chan() const
	{
		return (word0 >> 23) & 0x3;
	}

	bool isSrc0Neg() const
	{
		return ((word0 >> 12) & 0x1) != 0;
	}

	bool isSrc1Neg() const
	{
		return ((word0 >> 25) & 0x1) != 0;
	}

	bool isSrc0Rel() const
	{
		return ((word0 >> 9) & 0x1) != 0;
	}

	bool isSrc1Rel() const
	{
		return ((word0 >> 22) & 0x1) != 0;
	}

	uint8 getIndexMode() const
	{
		return (word0 >> 26) & 7;;
	}

	bool isTranscedentalUnit() const
	{
		const uint32 op2 = this->getOP2Code();
		switch (op2)
		{
		case COS:
		case SIN:
		case RECIP_FF: // todo: verify
		case RECIP_IEEE: // todo: verify
		case RECIPSQRT_IEEE: // todo: verify
		case RECIPSQRT_CLAMPED: // todo: verify
		case RECIPSQRT_FF: // todo: verify
		case MULLO_INT:
		case MULLO_UINT:
		case FLT_TO_INT:
		case FLT_TO_UINT:
		case INT_TO_FLOAT:
		case UINT_TO_FLOAT:
		case LOG_CLAMPED:
		case LOG_IEEE:
		case EXP_IEEE:
		case SQRT_IEEE:
			return true;
		default:
			break;
		}
		return false;
	}

};

static_assert(sizeof(LatteClauseInstruction_ALU) == 8);

