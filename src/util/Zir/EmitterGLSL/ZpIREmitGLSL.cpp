#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZirUtility.h"
#include "util/Zir/Core/ZpIRPasses.h"
#include "util/Zir/EmitterGLSL/ZpIREmitGLSL.h"
#include "util/Zir/Core/ZpIRScheduler.h"

// string buffer helper class which keeps buffer space at the front and end, allow fast prepend and append
class DualStringBuffer
{
	static constexpr size_t N = 1024;

public:
	DualStringBuffer() : m_offsetBegin(N / 2), m_offsetEnd(N / 2) { }

	~DualStringBuffer()
	{
	}

	static_assert(sizeof(char) == sizeof(uint8));

	void reset()
	{
		m_offsetBegin = N / 2;
		m_offsetEnd = m_offsetBegin;
	}

	void append(std::string_view strView)
	{
		cemu_assert_debug((m_offsetEnd + strView.size()) <= N);
		std::memcpy(m_strBuffer + m_offsetEnd, strView.data(), strView.size());
		m_offsetEnd += (uint32)strView.size();
	}

	template <typename... Args>
	void appendFmt(const char* format_str, Args... args)
	{
		char* buf = (char*)(m_strBuffer + m_offsetEnd);
		char* r = fmt::format_to(buf, fmt::runtime(format_str), std::forward<Args>(args)...);
		cemu_assert_debug(r <= (char*)(m_strBuffer + N));
		m_offsetEnd += (uint32)(r - buf);
	}

	void prepend(std::string_view strView)
	{
		assert_dbg();
	}

	size_t size() const
	{
		return m_offsetEnd - m_offsetBegin;
	}

	operator std::string_view()
	{
		return std::basic_string_view<char>((char*)(m_strBuffer + m_offsetBegin), m_offsetEnd - m_offsetBegin);
	}

private:
	//void resizeBuffer(uint32 spaceRequiredFront, uint32 spaceRequiredBack)
	//{
	//	uint32 newTotalSize = spaceRequiredFront + size() + spaceRequiredBack;
	//	// round to next multiple of 32 and add extra buffer
	//	newTotalSize = (newTotalSize + 31) & ~31;
	//	newTotalSize += (newTotalSize / 4);
	//	// 
	//}

	//uint8* m_bufferPtr{ nullptr };
	//size_t m_bufferSize{ 0 };
	//std::vector<uint8> m_buffer;
	uint32 m_offsetBegin;
	uint32 m_offsetEnd;

	uint8 m_strBuffer[N];
};



namespace ZirEmitter
{
	static const char g_idx_to_element[] = { 'x' , 'y', 'z', 'w'};

	void GLSL::Emit(ZpIR::ZpIRFunction* irFunction, StringBuf* output)
	{
		m_irFunction = irFunction;
		m_glslSource = output;

		cemu_assert_debug(m_irFunction->m_entryBlocks.size() == 1);

		cemu_assert_debug(m_irFunction->m_basicBlocks.size() == 1); // other sizes are todo

		m_glslSource->add("void main()\r\n{\r\n");
		GenerateBasicBlockCode(*m_irFunction->m_entryBlocks[0]);
		m_glslSource->add("}\r\n");
	}

	void GLSL::GenerateBasicBlockCode(ZpIR::ZpIRBasicBlock& basicBlock)
	{
		// init context		
#ifndef PUBLIC_RELEASE
		for (auto& itr : m_blockContext.regInlinedExpression)
		{
			cemu_assert_debug(itr == nullptr); // leaked buffer
		}
#endif
		m_blockContext.regReadTracking.clear();
		m_blockContext.regReadTracking.resize(basicBlock.m_regs.size());
		m_blockContext.regInlinedExpression.resize(basicBlock.m_regs.size());

		m_blockContext.currentBasicBlock = &basicBlock;

		// we first do an analysis pass in which we determine the read count for each register
		// every register which is only consumed once can be directly inlined instead of storing and referencing it via a variable
		ZpIR::IR::__InsBase* instruction = basicBlock.m_instructionFirst;
		while (instruction)
		{
			ZpIR::ZpIRCmdUtil::forEachAccessedReg(basicBlock, instruction,
				[this](ZpIR::IRReg readReg)
			{
				if (readReg >= 0x8000)
					assert_dbg();
				// read access
				auto& entry = m_blockContext.regReadTracking.at(readReg);
				if (entry < 255)
					entry++;
			},
				[](ZpIR::IRReg writtenReg)
			{
			});

			instruction = instruction->next;
		}

		// emit GLSL for this block
		instruction = basicBlock.m_instructionFirst;
		while (instruction)
		{
			if (auto ins = ZpIR::IR::InsRR::getIfForm(instruction))
				HandleInstruction(ins);
			else if (auto ins = ZpIR::IR::InsRRR::getIfForm(instruction))
				HandleInstruction(ins);
			else if (auto ins = ZpIR::IR::InsIMPORT::getIfForm(instruction))
				HandleInstruction(ins);
			else if (auto ins = ZpIR::IR::InsEXPORT::getIfForm(instruction))
				HandleInstruction(ins);
			else
			{
				assert_dbg();
			}

			instruction = instruction->next;
		}
	}

	void GLSL::HandleInstruction(ZpIR::IR::InsRR* ins)
	{
		DualStringBuffer* expressionBuf = GetStringBuffer();
		bool forceNoInline = false;

		switch (ins->opcode)
		{
		case ZpIR::IR::OpCode::BITCAST:
		{
			auto srcType = m_blockContext.currentBasicBlock->getRegType(ins->rB);
			auto dstType = m_blockContext.currentBasicBlock->getRegType(ins->rA);

			if (srcType == ZpIR::DataType::U32 && dstType == ZpIR::DataType::F32)
				expressionBuf->append("uintBitsToFloat(");
			else if (srcType == ZpIR::DataType::S32 && dstType == ZpIR::DataType::F32)
				expressionBuf->append("intBitsToFloat(");
			else if (srcType == ZpIR::DataType::F32 && dstType == ZpIR::DataType::U32)
				expressionBuf->append("floatBitsToUint(");
			else if (srcType == ZpIR::DataType::F32 && dstType == ZpIR::DataType::S32)
				expressionBuf->append("floatBitsToInt(");
			else
				assert_dbg();
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(")");
			break;
		}
		case ZpIR::IR::OpCode::SWAP_ENDIAN:
		{
			auto srcType = m_blockContext.currentBasicBlock->getRegType(ins->rB);
			auto dstType = m_blockContext.currentBasicBlock->getRegType(ins->rA);
			cemu_assert_debug(srcType == dstType);

			// todo - should we store expressionBuf in a temporary variable? We reference it multiple times and reducing complexity would be good
			if (srcType == ZpIR::DataType::U32)
			{
				expressionBuf->append("(((");
				appendSourceString(expressionBuf, ins->rB);
				expressionBuf->append(")>>24)");

				expressionBuf->append("|");

				expressionBuf->append("(((");
				appendSourceString(expressionBuf, ins->rB);
				expressionBuf->append(")>>8)&0xFF00)");

				expressionBuf->append("|");

				expressionBuf->append("(((");
				appendSourceString(expressionBuf, ins->rB);
				expressionBuf->append(")<<8)&0xFF0000)");

				expressionBuf->append("|");

				expressionBuf->append("((");
				appendSourceString(expressionBuf, ins->rB);
				expressionBuf->append(")<<24))");

				// (v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24))
			}
			else
				assert_dbg();
			forceNoInline = true; // avoid inlining endian-swapping, since it would add too much complexity to expressions
			break;
		}
		case ZpIR::IR::OpCode::MOV:
			appendSourceString(expressionBuf, ins->rB);
			break;
		case ZpIR::IR::OpCode::CONVERT_FLOAT_TO_INT:
		{
			auto srcType = m_blockContext.currentBasicBlock->getRegType(ins->rB);
			auto dstType = m_blockContext.currentBasicBlock->getRegType(ins->rA);
			cemu_assert_debug(srcType == ZpIR::DataType::F32);
			cemu_assert_debug(dstType == ZpIR::DataType::S32 || dstType == ZpIR::DataType::U32);
			if(dstType == ZpIR::DataType::U32)
				expressionBuf->append("uint(");
			else
				expressionBuf->append("int(");
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(")");
			break;
		}
		case ZpIR::IR::OpCode::CONVERT_INT_TO_FLOAT:
		{
			auto srcType = m_blockContext.currentBasicBlock->getRegType(ins->rB);
			auto dstType = m_blockContext.currentBasicBlock->getRegType(ins->rA);
			cemu_assert_debug(srcType == ZpIR::DataType::S32 || srcType == ZpIR::DataType::U32);
			cemu_assert_debug(dstType == ZpIR::DataType::F32);
			expressionBuf->append("float(");
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(")");
			break;
		}
		default:
			assert_dbg();
		}
		AssignResult(ins->rA, expressionBuf, forceNoInline);
	}

	void GLSL::HandleInstruction(ZpIR::IR::InsRRR* ins)
	{
		DualStringBuffer* expressionBuf = GetStringBuffer();

		switch (ins->opcode)
		{
		case ZpIR::IR::OpCode::ADD:
		{
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(" + ");
			appendSourceString(expressionBuf, ins->rC);
			break;
		}
		case ZpIR::IR::OpCode::MUL:
		{
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(" * ");
			appendSourceString(expressionBuf, ins->rC);
			break;
		}
		case ZpIR::IR::OpCode::DIV:
		{
			appendSourceString(expressionBuf, ins->rB);
			expressionBuf->append(" / ");
			appendSourceString(expressionBuf, ins->rC);
			break;
		}
		default:
			assert_dbg();
		}
		AssignResult(ins->rA, expressionBuf);
	}

	void GLSL::HandleInstruction(ZpIR::IR::InsIMPORT* ins)
	{
		ZpIR::ShaderSubset::ShaderImportLocation loc(ins->importSymbol);
		DualStringBuffer* buf = GetStringBuffer();
		if (loc.IsUniformRegister())
		{
			uint16 index;
			loc.GetUniformRegister(index);
			// todo - this is complex. Solve via callback
			buf->appendFmt("uf_remappedVS[{}].{}", index/4, g_idx_to_element[index&3]);
			AssignResult(ins->regArray[0], buf);
		}
		else if (loc.IsVertexAttribute())
		{
			uint16 attributeIndex;
			uint16 channelIndex;
			loc.GetVertexAttribute(attributeIndex, channelIndex);

			cemu_assert_debug(ins->count == 1);
			cemu_assert_debug(ZpIR::isRegVar(ins->regArray[0]));
			cemu_assert_debug(channelIndex < 4);
			cemu_assert_debug(m_blockContext.currentBasicBlock->getRegType(ins->regArray[0]) == ZpIR::DataType::U32);

			buf->appendFmt("attrDataSem{}.{}", attributeIndex, g_idx_to_element[channelIndex]);

			AssignResult(ins->regArray[0], buf);
		}
		else
		{
			cemu_assert_debug(false);
		}
	}

	void GLSL::HandleInstruction(ZpIR::IR::InsEXPORT* ins)
	{
		ZpIR::ShaderSubset::ShaderExportLocation loc(ins->exportSymbol);
		DualStringBuffer* buf = GetStringBuffer();
		if (loc.IsPosition())
		{
			// todo - support for output mask (e.g. xyzw, x_zw) ?
			buf->append("SET_POSITION(vec4(");
			cemu_assert_debug(ins->count == 4);
			for (uint32 i = 0; i < ins->count; i++)
			{
				if(i > 0)
					buf->append(", ");
				appendSourceString(buf, ins->regArray[i]);
			}
			m_glslSource->add(*buf);
			m_glslSource->add("));\r\n");
		}
		else if (loc.IsOutputAttribute())
		{
			uint16 attributeIndex;
			loc.GetOutputAttribute(attributeIndex);
			buf->appendFmt("passParameterSem{} = vec4(", attributeIndex);
			cemu_assert_debug(ins->count == 4);
			for (uint32 i = 0; i < ins->count; i++)
			{
				if (i > 0)
					buf->append(", ");
				appendSourceString(buf, ins->regArray[i]);
			}
			m_glslSource->add(*buf);
			m_glslSource->add(");\r\n");
		}
		else
		{
			assert_dbg();
		}
		ReleaseStringBuffer(buf);
	}

	void GLSL::AssignResult(ZpIR::IRReg irReg, DualStringBuffer* buf, bool forceNoInline)
	{
		if (buf->size() > 100)
			forceNoInline = true; // expression too long

		if (m_blockContext.CanInlineRegister(irReg) && !forceNoInline)
		{
			SetRegInlinedExpression(irReg, buf);
		}
		else
		{
			ZpIR::DataType regType = m_blockContext.currentBasicBlock->getRegType(irReg);
			if (regType == ZpIR::DataType::F32)
				m_glslSource->add("float ");
			else if (regType == ZpIR::DataType::S32)
				m_glslSource->add("int ");
			else if (regType == ZpIR::DataType::U32)
				m_glslSource->add("uint ");
			else
			{
				cemu_assert_debug(false);
			}

			char regName[16];
			getRegisterName(regName, irReg);
			m_glslSource->add(regName);
			m_glslSource->add(" = ");
			m_glslSource->add(*buf);
			m_glslSource->add(";\r\n");
			ReleaseStringBuffer(buf);
		}
	}

	void GLSL::appendSourceString(DualStringBuffer* buf, ZpIR::IRReg irReg)
	{
		if (ZpIR::isConstVar(irReg))
		{
			ZpIR::IRRegConstDef* constDef = m_blockContext.currentBasicBlock->getConstant(irReg);
			if (constDef->type == ZpIR::DataType::U32)
			{
				buf->appendFmt("{}", constDef->value_u32);
				return;
			}
			else if (constDef->type == ZpIR::DataType::S32)
			{
				buf->appendFmt("{}", constDef->value_s32);
				return;
			}
			else if (constDef->type == ZpIR::DataType::F32)
			{
				buf->appendFmt("{}", constDef->value_f32);
				return;
			}
			assert_dbg();
		}
		else
		{
			cemu_assert_debug(ZpIR::isRegVar(irReg));
			uint16 regIndex = ZpIR::getRegIndex(irReg);
			DualStringBuffer* expressionBuf = m_blockContext.regInlinedExpression[regIndex];
			if (expressionBuf)
			{
				buf->append(*expressionBuf);
				return;
			}
			char regName[16];
			getRegisterName(regName, irReg);
			buf->append(regName);
		}
	}

	void GLSL::getRegisterName(char buf[16], ZpIR::IRReg irReg)
	{
		auto& regData = m_blockContext.currentBasicBlock->m_regs[(uint16)irReg & 0x7FFF];
		cemu_assert_debug(regData.hasAssignedPhysicalRegister());
		ZpIR::ZpIRPhysicalReg physReg = regData.physicalRegister;

		char typeChar;
		if (ZirPass::RegisterAllocatorForGLSL::IsPhysRegTypeF32(physReg))
			typeChar = 'f';
		else if (ZirPass::RegisterAllocatorForGLSL::IsPhysRegTypeS32(physReg))
			typeChar = 'i';
		else if (ZirPass::RegisterAllocatorForGLSL::IsPhysRegTypeU32(physReg))
			typeChar = 'u';
		else
		{
			typeChar = 'x';
			cemu_assert_debug(false);
		}

		auto r = fmt::format_to(buf, "r{}{}", ZirPass::RegisterAllocatorForGLSL::GetPhysRegIndex(physReg), typeChar);
		*r = '\0';
	}

	void GLSL::SetRegInlinedExpression(ZpIR::IRReg irReg, DualStringBuffer* buf)
	{
		cemu_assert_debug(ZpIR::isRegVar(irReg));
		uint16 dstIndex = (uint16)irReg;
		if (m_blockContext.regInlinedExpression[dstIndex])
			ReleaseStringBuffer(m_blockContext.regInlinedExpression[dstIndex]);
		m_blockContext.regInlinedExpression[dstIndex] = buf;
	}

	void GLSL::ResetRegInlinedExpression(ZpIR::IRReg irReg)
	{
		cemu_assert_debug(ZpIR::isRegVar(irReg));
		uint16 dstIndex = (uint16)irReg;
		if (m_blockContext.regInlinedExpression[dstIndex])
		{
			ReleaseStringBuffer(m_blockContext.regInlinedExpression[dstIndex]);
			m_blockContext.regInlinedExpression[dstIndex] = nullptr;
		}
	}

	DualStringBuffer* GLSL::GetRegInlinedExpression(ZpIR::IRReg irReg)
	{
		cemu_assert_debug(ZpIR::isRegVar(irReg));
		uint16 dstIndex = (uint16)irReg;
		return m_blockContext.regInlinedExpression[dstIndex];
	}

	DualStringBuffer* GLSL::GetStringBuffer()
	{
		if (m_stringBufferCache.empty())
			return new DualStringBuffer();
		DualStringBuffer* buf = m_stringBufferCache.back();
		m_stringBufferCache.pop_back();
		buf->reset();
		return buf;
	}

	void GLSL::ReleaseStringBuffer(DualStringBuffer* buf)
	{
		m_stringBufferCache.emplace_back(buf);
	}
};
