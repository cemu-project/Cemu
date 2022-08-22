#include "Cafe/HW/Latte/Transcompiler/LatteTC.h"
#include "Cafe/HW/Latte/ISA/LatteInstructions.h"
#include "util/Zir/Core/ZpIRBuilder.h"

void LatteTCGenIR::CF_CALL_FS_emitFetchAttribute(LatteParsedFetchShaderAttribute_t& attribute, Latte::GPRType dstGPR)
{
	auto irBuilder = m_irGenContext.irBuilder;

	// extract each channel
	for (sint32 t = 0; t < 4; t++)
	{
		uint32 gprElementIndex = (uint32)dstGPR * 4 + t;
		LatteConst::VertexFetchDstSel ds = (LatteConst::VertexFetchDstSel)attribute.ds[t];

		switch (ds)
		{
		case LatteConst::VertexFetchDstSel::X:
		case LatteConst::VertexFetchDstSel::Y:
		case LatteConst::VertexFetchDstSel::Z:
		case LatteConst::VertexFetchDstSel::W:
		{
			uint8 channelIndex = (uint8)((uint32)ds - (uint32)LatteConst::VertexFetchDstSel::X);

			ZpIR::IRReg resultHolder = m_irGenContext.irBuilder->createReg(ZpIR::DataType::U32);
			ZpIR::LocationSymbolName importSource = ZpIR::ShaderSubset::ShaderImportLocation().SetVertexAttribute(attribute.semanticId, channelIndex);
			m_irGenContext.irBuilder->emit_IMPORT(importSource, resultHolder);

			// swap endianness
			if (attribute.endianSwap == LatteConst::VertexFetchEndianMode::SWAP_U32)
			{
				// todo - this may be more complex depending on type
				ZpIR::IRReg elementResult;
				irBuilder->emit_RR(ZpIR::IR::OpCode::SWAP_ENDIAN, irBuilder->createReg(elementResult, ZpIR::DataType::U32), resultHolder);
				resultHolder = elementResult;
			}

			bool isSigned = attribute.isSigned;

			// transform
			LatteConst::VertexFetchFormat fmt = (LatteConst::VertexFetchFormat)attribute.format;
			LatteClauseInstruction_VTX::NUM_FORMAT_ALL nfa = (LatteClauseInstruction_VTX::NUM_FORMAT_ALL)attribute.nfa;

			if (fmt == LatteConst::VertexFetchFormat::VTX_FMT_32_32_32_FLOAT ||
				fmt == LatteConst::VertexFetchFormat::VTX_FMT_32_32_FLOAT)
			{
				uint32 numComp;
				if (fmt == LatteConst::VertexFetchFormat::VTX_FMT_32_32_32_FLOAT)
					numComp = 3;
				else if (fmt == LatteConst::VertexFetchFormat::VTX_FMT_32_32_FLOAT)
					numComp = 2;
				else
				{
					cemu_assert_debug(false);
				}

				cemu_assert_debug(attribute.endianSwap == LatteConst::VertexFetchEndianMode::SWAP_U32);
				cemu_assert_debug(nfa == LatteClauseInstruction_VTX::NUM_FORMAT_ALL::NUM_FORMAT_SCALED);
				cemu_assert_debug(channelIndex < numComp);

				ZpIR::IRReg elementResult;
				irBuilder->emit_RR(ZpIR::IR::OpCode::BITCAST, irBuilder->createReg(elementResult, ZpIR::DataType::F32), resultHolder);
				resultHolder = elementResult;
			}
			else if (fmt == LatteConst::VertexFetchFormat::VTX_FMT_8_8_8_8)
			{
				uint32 numComp;
				switch (fmt)
				{
				case LatteConst::VertexFetchFormat::VTX_FMT_8_8_8_8:
					numComp = 4;
					break;
				case LatteConst::VertexFetchFormat::VTX_FMT_8_8_8:
					numComp = 3;
					break;
				case LatteConst::VertexFetchFormat::VTX_FMT_8_8:
					numComp = 2;
					break;
				case LatteConst::VertexFetchFormat::VTX_FMT_8:
					numComp = 1;
					break;
				}

				cemu_assert_debug(attribute.endianSwap == LatteConst::VertexFetchEndianMode::SWAP_NONE);
				cemu_assert_debug(channelIndex < numComp);

				if (nfa == LatteClauseInstruction_VTX::NUM_FORMAT_ALL::NUM_FORMAT_NORM)
				{
					// scaled
					if (isSigned)
					{
						assert_dbg();
						// we can fake sign extend by subtracting 128? Would be faster than the AND + Conditional OR
					}
					else
					{
						resultHolder = irBuilder->emit_RR(ZpIR::IR::OpCode::CONVERT_INT_TO_FLOAT, ZpIR::DataType::F32, resultHolder);
						resultHolder = irBuilder->emit_RRR(ZpIR::IR::OpCode::DIV, ZpIR::DataType::F32, resultHolder, irBuilder->createConstF32(255.0f));
					}
				}
				else
				{
					assert_dbg();
				}
			}
			else
			{
				assert_dbg();
			}

			// todo - we need a sign-extend instruction for this which should take arbitrary bit count


			this->m_irGenContext.activeVars.set(dstGPR, t, resultHolder); // set GPR.channel to the result
			break;
		}
		case LatteConst::VertexFetchDstSel::CONST_0F:
		{
			// todo - this could also be an integer zero. Use attribute format / other channel info to determine if this type is integer/float
			ZpIR::IRReg resultHolder;
			irBuilder->emit_RR(ZpIR::IR::OpCode::MOV, irBuilder->createReg(resultHolder, ZpIR::DataType::F32), irBuilder->createConstF32(0.0f));
			this->m_irGenContext.activeVars.set(dstGPR, t, resultHolder);
			break;
		}
		case LatteConst::VertexFetchDstSel::CONST_1F:
		{
			ZpIR::IRReg resultHolder;
			irBuilder->emit_RR(ZpIR::IR::OpCode::MOV, irBuilder->createReg(resultHolder, ZpIR::DataType::F32), irBuilder->createConstF32(1.0f));
			this->m_irGenContext.activeVars.set(dstGPR, t, resultHolder);
			break;
		}
		default:
			assert_dbg();
		}
	}
}

void LatteTCGenIR::processCF_CALL_FS(const LatteCFInstruction_DEFAULT& cfInstruction)
{
	auto fetchShader = m_vertexShaderCtx.parsedFetchShader;
	auto semanticTable = m_vertexShaderCtx.vtxSemanticTable;

	// generate IR to decode vertex attributes
	cemu_assert_debug(fetchShader->bufferGroupsInvalid.size() == 0); // todo
	for(auto& bufferGroup : fetchShader->bufferGroups)
	{
		for (sint32 i = 0; i < bufferGroup.attribCount; i++)
		{
			auto& attribute = bufferGroup.attrib[i];

			uint32 dstGPR = 0;
			// get register index based on vtx semantic table
			uint32 attributeShaderLoc = 0xFFFFFFFF;
			for (sint32 f = 0; f < 32; f++)
			{
				if (semanticTable[f] == attribute.semanticId)
				{
					attributeShaderLoc = f;
					break;
				}
			}
			if (attributeShaderLoc == 0xFFFFFFFF)
				continue; // attribute is not mapped to VS input
			dstGPR = attributeShaderLoc + 1; // R0 is skipped

			// emit IR code for attribute import (decode into GPR)
			CF_CALL_FS_emitFetchAttribute(attribute, dstGPR);
		}
	}
}

// get IRReg for Latte GPR (single channel) typeHint is used when register has to be imported
// if convertOnTypeMismatch is set then we bitcast the register on type mismatch
ZpIR::IRReg LatteTCGenIR::getIRRegFromGPRElement(uint32 gprIndex, uint32 channel, ZpIR::DataType typeHint)
{
	// get IR register for <GPR>.<channel> from currently active context
	ZpIR::IRReg r;
	if (m_irGenContext.activeVars.get(gprIndex, channel, r))
		return r;

	// if GPR.channel is not known
	// in the entry basic block we can assume a value of zero because there is nowhere to import from
	if (m_irGenContext.isEntryBasicBlock)
	{
		if (typeHint == ZpIR::DataType::F32)
			return m_irGenContext.irBuilder->createConstF32(0.0f);
		else if (typeHint == ZpIR::DataType::U32)
			return m_irGenContext.irBuilder->createConstU32(0);
		else if (typeHint == ZpIR::DataType::S32)
			return m_irGenContext.irBuilder->createConstS32(0);
		cemu_assert_debug(false);
	}

	// otherwise create import and resolve later during register allocation
	r = m_irGenContext.irBuilder->createReg(typeHint);
	m_irGenContext.irBuilder->addImport(r, gprIndex*4 + channel);

	return r;
}

// similar to getIRRegFromGPRElement() but will bitcast the type if it mismatches
ZpIR::IRReg LatteTCGenIR::getTypedIRRegFromGPRElement(uint32 gprIndex, uint32 channel, ZpIR::DataType type)
{
	auto irReg = getIRRegFromGPRElement(gprIndex, channel, type);
	if (m_irGenContext.irBuilder->getRegType(irReg) == type)
		return irReg;
	// type does not match, bitcast into new reg
	auto newReg = m_irGenContext.irBuilder->createReg(type);
	m_irGenContext.irBuilder->emit_RR(ZpIR::IR::OpCode::BITCAST, newReg, irReg);
	// remember converted register since its likely that it is accessed with the same type again
	// todo - ideally, we would keep track of all the types. But it has to be efficient
	m_irGenContext.activeVars.set(gprIndex, channel, newReg);
	return newReg;
}

// try to determine the type of the constant from the raw u32 value
ZpIR::DataType _guessTypeFromConstantValue(uint32 bits)
{
	if (bits == 0x3F800000) // float 1.0
		return ZpIR::DataType::F32;
	return ZpIR::DataType::S32;
}

// maybe pass a type hint parameter
ZpIR::IRReg LatteTCGenIR::loadALUOperand(LatteALUSrcSel srcSel, uint8 srcChan, bool isNeg, bool isAbs, bool isRel, uint8 indexMode, const uint32* literalData, ZpIR::DataType typeHint, bool convertOnTypeMismatch)
{
	if (srcSel.isGPR())
	{
		//LatteTCGenIR::GPRElement gprElement = srcSel.getGPR() * 4 + srcChan;
		if (isRel)
			assert_dbg();

		ZpIR::IRReg reg;
		
		if(convertOnTypeMismatch)
			reg = getTypedIRRegFromGPRElement(srcSel.getGPR(), srcChan, typeHint);
		else
			reg = getIRRegFromGPRElement(srcSel.getGPR(), srcChan, typeHint);

		// if additional transformations are applied then we create a temporary IRReg here
		// todo - is caching&recycling the transformed registers worth it?
		if (isAbs || isNeg)
		{
			// create new var and apply transformation
			assert_dbg();
		}

		return reg;
	}
	else if (srcSel.isAnyConst())
	{
		if (srcSel.isConst_0F())
		{
			return m_irGenContext.irBuilder->createConstF32(0.0f); // todo - could also be integer type constant? Try to find a way to predict the type correctly
		}
		else
			assert_dbg();
	}
	else if (srcSel.isLiteral())
	{
		// literal constant
		// we guess the type
		return m_irGenContext.irBuilder->createTypedConst(literalData[srcChan], _guessTypeFromConstantValue(literalData[srcChan]));
	}
	else if (srcSel.isCFile())
	{
		// constant registers / uniform registers
		uint32 cfileIndex = srcSel.getCFile();
		auto newReg = m_irGenContext.irBuilder->createReg(typeHint);
		ZpIR::LocationSymbolName importSource = ZpIR::ShaderSubset::ShaderImportLocation().SetUniformRegister(cfileIndex*4 + srcChan);
		m_irGenContext.irBuilder->emit_IMPORT(importSource, newReg);
		return newReg;
	}
	else
		assert_dbg();

	return 0;
}

void LatteTCGenIR::emitALUGroup(const LatteClauseInstruction_ALU* aluUnit[5], const uint32* literalData)
{
	//struct
	//{
	//	uint32 gprElementIndex;
	//	ZpIR::IRReg irReg;
	//	bool isSet;
	//}groupOutput[5] = {};

	ZpIR::BasicBlockBuilder* irBuilder = m_irGenContext.irBuilder;

	// used by MOV instruction which can be used with any 32bit type (float, int, uint)
	auto getMOVSourceType = [&](const LatteClauseInstruction_ALU_OP2* instrOP2) -> ZpIR::DataType
	{
		auto sel = instrOP2->getSrc0Sel();
		if (sel.isGPR())
		{
			ZpIR::IRReg r;
			if (!m_irGenContext.activeVars.get(sel.getGPR(), instrOP2->getSrc0Chan(), r))
			{
				// import, do we have an alternative way to guess the type?
				// for now lets assume float because it will be correct more often than not
				// getting the type wrong means a temporary register and two bit cast instructions will be spawned
				return ZpIR::DataType::F32;
			}
			return m_irGenContext.irBuilder->getRegType(r);
		}
		else if (sel.isLiteral())
		{
			return _guessTypeFromConstantValue(literalData[instrOP2->getSrc0Chan()]);
		}
		else
			assert_dbg();
		return ZpIR::DataType::S32;
	};

	auto getOp0Reg = [&](const LatteClauseInstruction_ALU_OP2* instrOP2, ZpIR::DataType type) -> ZpIR::IRReg
	{
		// todo - pass type hint, so internally correct type is used if register needs to be created
		ZpIR::IRReg r = loadALUOperand(instrOP2->getSrc0Sel(), instrOP2->getSrc0Chan(), instrOP2->isSrc0Neg(), false, instrOP2->isSrc0Rel(), instrOP2->getIndexMode(), literalData, type, true);
		// make sure type matches with 'type' (loadALUOperand should convert)
		cemu_assert_debug(irBuilder->getRegType(r) == type);
		return r;
	};

	auto getOp1Reg = [&](const LatteClauseInstruction_ALU_OP2* instrOP2, ZpIR::DataType type) -> ZpIR::IRReg
	{
		// todo - pass type hint, so internally correct type is used if register needs to be created
		ZpIR::IRReg r = loadALUOperand(instrOP2->getSrc1Sel(), instrOP2->getSrc1Chan(), instrOP2->isSrc1Neg(), false, instrOP2->isSrc1Rel(), instrOP2->getIndexMode(), literalData, type, true);
		// make sure type matches with 'type' (loadALUOperand should convert)
		cemu_assert_debug(irBuilder->getRegType(r) == type);
		return r;
	};

	auto getResultReg = [&](uint8 aluUnit, const LatteClauseInstruction_ALU_OP2* instrOP2, ZpIR::DataType type) -> ZpIR::IRReg
	{
		// create output register
		ZpIR::IRReg r = m_irGenContext.irBuilder->createReg(type);

		cemu_assert_debug(instrOP2->getDestClamp() == 0); // todo
		cemu_assert_debug(instrOP2->getDestRel() == 0); // todo
		cemu_assert_debug(instrOP2->getOMod() == 0); // todo

		if (instrOP2->getWriteMask())
		{
			// output to GPR
			m_irGenContext.activeVars.setAfterGroup(aluUnit, instrOP2->getDestGpr(), instrOP2->getDestElem(), r);
		}
		else
		{
			// output only to PV/PS
			assert_dbg();
		}
		// output to PV/PS
		// todo

		// check for disabled destination GPR?

		// also assign PV/PS
		// todo

		//ZpIR::IRReg r;
		//if (m_irGenContext.activeVars.get(gprIndex, channel, r))
		//	return r;

		//// if GPR not present then create import for it
		//r = m_irGenContext.irBuilder->createReg(typeHint);
		//m_irGenContext.irBuilder->addImport(r, 0x12345678);

		return r;
	};

	for (sint32 aluUnitIndex = 0; aluUnitIndex < 5; aluUnitIndex++)
	{
		const LatteClauseInstruction_ALU* instr = aluUnit[aluUnitIndex];
		if (instr == nullptr)
			continue;
		if (instr->isOP3())
		{
			assert_dbg();
		}
		else
		{
			auto opcode2 = instr->getOP2Code();
			auto instrOP2 = instr->getOP2Instruction();

			// prepare operands, load them into IRVars if they aren't already
			uint8 indexMode = instrOP2->getIndexMode();

			switch (opcode2)
			{
			case LatteClauseInstruction_ALU::OPCODE_OP2::MUL:
			case LatteClauseInstruction_ALU::OPCODE_OP2::MUL_IEEE:
			{
				// how to implement this with least amount of copy paste and still having very good performance?
				// maybe use lambdas? Or functions?
				irBuilder->emit_RRR(ZpIR::IR::OpCode::MUL, getResultReg(aluUnitIndex, instrOP2, ZpIR::DataType::F32), getOp0Reg(instrOP2, ZpIR::DataType::F32), getOp1Reg(instrOP2, ZpIR::DataType::F32));
				break;
			}
			case LatteClauseInstruction_ALU::OPCODE_OP2::MOV:
			{
				// MOV is type-agnostic, but some flags might make it apply float operations
				ZpIR::DataType guessedType = getMOVSourceType(instrOP2);

				irBuilder->emit_RR(ZpIR::IR::OpCode::MOV, getResultReg(aluUnitIndex, instrOP2, guessedType), getOp0Reg(instrOP2, guessedType));
				break;
			}
			case LatteClauseInstruction_ALU::OPCODE_OP2::DOT4:
			{
				// reduction opcode
				// must be mirrored to .xyzw units
				cemu_assert_debug(aluUnitIndex == 0);
				cemu_assert_debug(aluUnit[0]->getOP2Code() == aluUnit[1]->getOP2Code());
				cemu_assert_debug(aluUnit[1]->getOP2Code() == aluUnit[2]->getOP2Code());
				cemu_assert_debug(aluUnit[2]->getOP2Code() == aluUnit[3]->getOP2Code());

				auto unit_x = instrOP2;
				auto unit_y = aluUnit[1]->getOP2Instruction();
				auto unit_z = aluUnit[2]->getOP2Instruction();
				auto unit_w = aluUnit[3]->getOP2Instruction();

				cemu_assert_debug(unit_x->getDestClamp() == false);
				cemu_assert_debug(unit_x->getOMod() == 0);
				cemu_assert_debug(unit_x->getDestRel() == false);

				ZpIR::IRReg productX = irBuilder->emit_RRR(ZpIR::IR::OpCode::MUL, ZpIR::DataType::F32, getOp0Reg(unit_x, ZpIR::DataType::F32), getOp1Reg(unit_x, ZpIR::DataType::F32));
				ZpIR::IRReg productY = irBuilder->emit_RRR(ZpIR::IR::OpCode::MUL, ZpIR::DataType::F32, getOp0Reg(unit_y, ZpIR::DataType::F32), getOp1Reg(unit_y, ZpIR::DataType::F32));
				ZpIR::IRReg productZ = irBuilder->emit_RRR(ZpIR::IR::OpCode::MUL, ZpIR::DataType::F32, getOp0Reg(unit_z, ZpIR::DataType::F32), getOp1Reg(unit_z, ZpIR::DataType::F32));
				ZpIR::IRReg productW = irBuilder->emit_RRR(ZpIR::IR::OpCode::MUL, ZpIR::DataType::F32, getOp0Reg(unit_w, ZpIR::DataType::F32), getOp1Reg(unit_w, ZpIR::DataType::F32));

				ZpIR::IRReg sum = irBuilder->emit_RRR(ZpIR::IR::OpCode::ADD, ZpIR::DataType::F32, productX, productY);
				sum = irBuilder->emit_RRR(ZpIR::IR::OpCode::ADD, ZpIR::DataType::F32, sum, productZ);
				sum = irBuilder->emit_RRR(ZpIR::IR::OpCode::ADD, ZpIR::DataType::F32, sum, productW);

				// assign result
				if (unit_x->getWriteMask())
					m_irGenContext.activeVars.setAfterGroup(0, unit_x->getDestGpr(), unit_x->getDestElem(), sum);
				if (unit_y->getWriteMask())
					m_irGenContext.activeVars.setAfterGroup(1, unit_y->getDestGpr(), unit_y->getDestElem(), sum);
				if (unit_z->getWriteMask())
					m_irGenContext.activeVars.setAfterGroup(2, unit_z->getDestGpr(), unit_z->getDestElem(), sum);
				if (unit_w->getWriteMask())
					m_irGenContext.activeVars.setAfterGroup(3, unit_w->getDestGpr(), unit_w->getDestElem(), sum);

				// also set result in PV.x
				m_irGenContext.activeVars.setAfterGroupPVPS(0, sum);
				// todo - do we need to update the other units?

				aluUnitIndex += 3;
				continue;;
			}
			default:
				assert_dbg();
			}

			// handle dest clamp
			if (instrOP2->getDestClamp())
			{
				assert_dbg();
			}

			//uint32 src0Sel = (aluWord0 >> 0) & 0x1FF; // source selection
			//uint32 src1Sel = (aluWord0 >> 13) & 0x1FF;
			//uint32 src0Rel = (aluWord0 >> 9) & 0x1; // relative addressing mode
			//uint32 src1Rel = (aluWord0 >> 22) & 0x1;
			//uint32 src0Chan = (aluWord0 >> 10) & 0x3; // component selection x/y/z/w
			//uint32 src1Chan = (aluWord0 >> 23) & 0x3;
			//uint32 src0Neg = (aluWord0 >> 12) & 0x1; // negate input
			//uint32 src1Neg = (aluWord0 >> 25) & 0x1;
			//uint32 indexMode = (aluWord0 >> 26) & 7;
			//uint32 predSel = (aluWord0 >> 29) & 3;

			//uint32 src0Abs = (aluWord1 >> 0) & 1;
			//uint32 src1Abs = (aluWord1 >> 1) & 1;
			//uint32 updateExecuteMask = (aluWord1 >> 2) & 1;
			//uint32 updatePredicate = (aluWord1 >> 3) & 1;
			//uint32 writeMask = (aluWord1 >> 4) & 1;
			//uint32 omod = (aluWord1 >> 5) & 3;

			//uint32 destGpr = (aluWord1 >> 21) & 0x7F;
			//uint32 destRel = (aluWord1 >> 28) & 1;
			//uint32 destElem = (aluWord1 >> 29) & 3;
			//uint32 destClamp = (aluWord1 >> 31) & 1;

		}
	}
	// update IR vars with outputs from group
	m_irGenContext.activeVars.applyDelayedAfterGroup();
}

void LatteTCGenIR::processCF_ALU(const LatteCFInstruction_ALU& cfInstruction)
{
	uint32 aluAddr = cfInstruction.getField_ADDR();
	uint32 aluCount = cfInstruction.getField_COUNT();

	const uint32* clauseCode = m_ctx.programData + aluAddr * 2;
	uint32 clauseLength = aluCount;
	
	const LatteClauseInstruction_ALU* aluCode = (const LatteClauseInstruction_ALU*)clauseCode;
	
	const LatteClauseInstruction_ALU* aluUnit[5] = {};
	
	const LatteClauseInstruction_ALU* instr = aluCode;
	const LatteClauseInstruction_ALU* instrLast = aluCode + clauseLength;
	
	// process instructions in groups
	uint8 literalMask = 0;
	while (instr < instrLast)
	{
		if (instr->isOP3())
		{
			assert_dbg();
		}
		else
		{
			LatteClauseInstruction_ALU::OPCODE_OP2 opcode2 = instr->getOP2Code();
			const LatteClauseInstruction_ALU_OP2* op = instr->getOP2Instruction();
			uint32 unitIndex = 0;
			if (op->isTranscedentalUnit())
				unitIndex = 4;
			else
			{
				unitIndex = op->getDestElem();
				if (aluUnit[unitIndex]) // unit already occupied, use transcendental unit instead
					unitIndex = 4;
			}
			cemu_assert_debug(!aluUnit[unitIndex]); // unit already used
			aluUnit[unitIndex] = op;
	
			// check for literal access
			if (op->getSrc0Sel().isLiteral())
				literalMask |= (op->getSrc0Chan() >= 2 ? 2 : 1);
			if (op->getSrc1Sel().isLiteral())
				literalMask |= (op->getSrc1Chan() >= 2 ? 2 : 1);
		}
	
		if (instr->isLastInGroup())
		{
			// emit code for group
			// extract literal constants
			const uint32* literalData = nullptr;
			if (literalMask)
			{
				literalData = (const uint32*)(instr + 1);
				if (literalMask & 2)
					instr += 2;
				else
					instr += 1;
				if ((instr + 1) > instrLast)
					assert_dbg(); // out of bounds
			}
			// generate code for group
			emitALUGroup(aluUnit, literalData);
			// reset group
			std::fill(aluUnit, aluUnit + 5, nullptr);
			literalMask = 0;
		}
		instr++;
	}
	if (aluUnit[0] || aluUnit[1] || aluUnit[2] || aluUnit[3] || aluUnit[4])
		assert_dbg();
}

void LatteTCGenIR::processCF_EXPORT(const LatteCFInstruction_EXPORT_IMPORT& cfInstruction)
{
	auto exportType = cfInstruction.getField_TYPE();

	cemu_assert_debug(cfInstruction.getField_BURST_COUNT() == 1); // todo

	uint32 arrayBase = cfInstruction.getField_ARRAY_BASE();

	cemu_assert_debug(cfInstruction.isEncodingBUF() == false); // todo

	LatteCFInstruction_EXPORT_IMPORT::COMPSEL sel[4];
	sel[0] = cfInstruction.getSwizField_SEL_X();
	sel[1] = cfInstruction.getSwizField_SEL_Y();
	sel[2] = cfInstruction.getSwizField_SEL_Z();
	sel[3] = cfInstruction.getSwizField_SEL_W();

	uint32 sourceGPR = cfInstruction.getField_RW_GPR();

	ZpIR::DataType typeHint;
	if (exportType == LatteCFInstruction_EXPORT_IMPORT::EXPORT_TYPE::POSITION)
		typeHint = ZpIR::DataType::F32;
	else if (exportType == LatteCFInstruction_EXPORT_IMPORT::EXPORT_TYPE::PARAMETER)
	{
		// todo - determine correct type for parameter
		typeHint = ZpIR::DataType::F32;
	}
	else
		assert_dbg();

	// get xyzw registers
	ZpIR::IRReg regArray[4];
	size_t regExportCount = 0; // number of exported registers/channels, number of valid regArray entries
	for (size_t i = 0; i < 4; i++)
	{
		switch (sel[i])
		{
		case LatteCFInstruction_EXPORT_IMPORT::COMPSEL::X:
		case LatteCFInstruction_EXPORT_IMPORT::COMPSEL::Y:
		case LatteCFInstruction_EXPORT_IMPORT::COMPSEL::Z:
		case LatteCFInstruction_EXPORT_IMPORT::COMPSEL::W:
		{
			uint32 channelIndex = (uint32)sel[i];
			regArray[regExportCount] = getTypedIRRegFromGPRElement(sourceGPR, channelIndex, typeHint);
			regExportCount++;
			break;
		}
		default:
		{
			assert_dbg();
			break;
		}
		}
		//ZpIR::IRReg r;
		//if (m_irGenContext.activeVars.get(gprIndex, channel, r))
		//	return r;
	}

	//ZpIR::LocationSymbolName exportSymbolName;
	ZpIR::ShaderSubset::ShaderExportLocation loc;

	if (exportType == LatteCFInstruction_EXPORT_IMPORT::EXPORT_TYPE::POSITION)
	{
		loc.SetPosition();
	}
	else if (exportType == LatteCFInstruction_EXPORT_IMPORT::EXPORT_TYPE::PARAMETER)
	{
		loc.SetOutputAttribute(arrayBase);
		//exportSymbolName = 0x20000 + arrayBase;
	}
	else
	{
		// todo
		assert_dbg();
	}

	cemu_assert_debug(regExportCount == 4); // todo - encode channel mask (e.g. xyz, xw, w, etc.) into export symbol name

	m_irGenContext.irBuilder->emit_EXPORT(loc, std::span(regArray, regArray + regExportCount));

}
