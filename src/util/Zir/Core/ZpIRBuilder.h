#pragma once
#include "util/Zir/Core/IR.h"

namespace ZpIR
{

	// helper class for building a single basic block
	class BasicBlockBuilder
	{
	public:
		BasicBlockBuilder(ZpIRBasicBlock* basicBlock) : m_basicBlock(basicBlock) {};

		IRReg createReg(DataType type, uint8 elementCount = 1)
		{
			return m_basicBlock->createRegister(type, elementCount);
		}

		IRReg createReg(IRReg& r, DataType type, uint8 elementCount = 1)
		{
			r = m_basicBlock->createRegister(type, elementCount);
			return r;
		}

		// append a single instruction at the end
		void append(IR::__InsBase* ins)
		{
			assert_dbg();
		}

		void emit_EXPORT(LocationSymbolName exportSymbolName, IRReg r0)
		{
			m_basicBlock->appendInstruction(new IR::InsEXPORT(exportSymbolName, r0));
		}

		void emit_EXPORT(LocationSymbolName exportSymbolName, std::span<IRReg> regs)
		{
			m_basicBlock->appendInstruction(new IR::InsEXPORT(exportSymbolName, regs));
		}

		void emit_IMPORT(LocationSymbolName importSymbolName, IRReg r0)
		{
			m_basicBlock->appendInstruction(new IR::InsIMPORT(importSymbolName, r0));
		}

		// result is rA, operand is rB
		// for some opcodes both can be operands
		void emit_RR(IR::OpCode opcode, IRReg rA, IRReg rB)
		{
			m_basicBlock->appendInstruction(new IR::InsRR(opcode, rA, rB));
		}

		IRReg emit_RR(IR::OpCode opcode, DataType resultType, IRReg rB)
		{
			IRReg resultReg = m_basicBlock->createRegister(resultType);
			emit_RR(opcode, resultReg, rB);
			return resultReg;
		}

		// result is rA, operands are rB and rC
		// for some opcodes all three can be operands
		void emit_RRR(IR::OpCode opcode, IRReg rA, IRReg rB, IRReg rC)
		{
			m_basicBlock->appendInstruction(new IR::InsRRR(opcode, rA, rB, rC));
		}

		IRReg emit_RRR(IR::OpCode opcode, DataType resultType, IRReg rB, IRReg rC)
		{
			IRReg resultReg = m_basicBlock->createRegister(resultType);
			m_basicBlock->appendInstruction(new IR::InsRRR(opcode, resultReg, rB, rC));
			return resultReg;
		}

		void emit(IR::__InsBase* ins)
		{
			m_basicBlock->appendInstruction(ins);
		}
		
		// constant var creation

		IRReg createConstU32(uint32 v)
		{
			return m_basicBlock->createConstantU32(v);
		}

		IRReg createTypedConst(uint32 v, DataType type)
		{
			return m_basicBlock->createTypedConstant(v, type);
		}

		IRReg createConstS32(uint32 v)
		{
			return m_basicBlock->createConstantS32(v);
		}

		IRReg createConstF32(f32 v)
		{
			return m_basicBlock->createConstantF32(v);
		}

		IRReg createConstPointer(void* v)
		{
			return m_basicBlock->createConstantPointer(v);
		}

		// use templates to compact other types?

		DataType getRegType(IRReg reg)
		{
			return m_basicBlock->getRegType(reg);
		}

		void addImport(IRReg reg, LocationSymbolName importSymbolName)
		{
			m_basicBlock->addImport(reg, importSymbolName);
		}


	private:
		ZpIRBasicBlock* m_basicBlock;
	};

	// helper class for constructing multiple basic blocks with control flow
	class ZpIRBuilder
	{
	public:
		typedef uint64 BlockBranchTarget;

		static const inline BlockBranchTarget INVALID_BLOCK_NAME = 0xFFFFFFFFFFFFFFFFull;

		struct BasicBlockWorkbuffer
		{
			BlockBranchTarget name{ INVALID_BLOCK_NAME };
			BlockBranchTarget targetBranchNotTaken{ INVALID_BLOCK_NAME };
			BlockBranchTarget targetBranchTaken{ INVALID_BLOCK_NAME };
		};

		void beginBlock(BlockBranchTarget name)
		{
			m_currentBasicBlock = new ZpIRBasicBlock();
			BasicBlockWorkbuffer* wb = new BasicBlockWorkbuffer();
			m_currentBasicBlock->setWorkbuffer(wb);
			wb->name = name;
			m_blocks.emplace_back(m_currentBasicBlock);
			m_blocksByName.emplace(name, m_currentBasicBlock);
		}

		ZpIRBasicBlock* endBlock()
		{
			ZpIRBasicBlock* block = m_currentBasicBlock;
			m_currentBasicBlock = nullptr;
			BasicBlockWorkbuffer* wb = (BasicBlockWorkbuffer*)block->getWorkbuffer();
			wb->targetBranchNotTaken = m_targetBranchNotTaken;
			wb->targetBranchTaken = m_targetBranchTaken;
			m_targetBranchNotTaken = INVALID_BLOCK_NAME;
			m_targetBranchTaken = INVALID_BLOCK_NAME;
			return block;
		}

		ZpIRFunction* finish()
		{
			if (m_currentBasicBlock)
				assert_dbg();
			// create function
			ZpIRFunction* func = new ZpIRFunction();
			// link all blocks
			// and also collect a list of entry and exit nodes
			for (auto& itr : m_blocks)
			{
				BasicBlockWorkbuffer* wb = (BasicBlockWorkbuffer*)itr->getWorkbuffer();
				if (wb->targetBranchNotTaken != INVALID_BLOCK_NAME)
				{
					auto target = m_blocksByName.find(wb->targetBranchNotTaken);
					if (target == m_blocksByName.end())
					{
						assert_dbg();
					}
					itr->m_branchNotTaken = target->second;
				}
				if (wb->targetBranchTaken != INVALID_BLOCK_NAME)
				{
					auto target = m_blocksByName.find(wb->targetBranchTaken);
					if (target == m_blocksByName.end())
					{
						assert_dbg();
					}
					itr->m_branchTaken = target->second;
				}
				delete wb;
				itr->setWorkbuffer(nullptr);
				func->m_basicBlocks.emplace_back(itr);
				// todo - track entry and exit blocks (set block flags for entry/exit during block gen)
			}
			return func;
		}

		IRReg createBlockRegister(DataType type, uint8 elementCount = 1)
		{
			return m_currentBasicBlock->createRegister(type, elementCount);
		}

		IRReg createConstU32(uint32 v)
		{
			return m_currentBasicBlock->createConstantU32(v);
		}

		IRReg createConstPointer(void* v)
		{
			return m_currentBasicBlock->createConstantPointer(v);
		}

		IRReg createConstPointerV(size_t v)
		{
			return m_currentBasicBlock->createConstantPointer((void*)v);
		}

		void addImport(IRReg reg, LocationSymbolName importName)
		{
			m_currentBasicBlock->addImport(reg, importName);
		}

		void addExport(IRReg reg, LocationSymbolName importName)
		{
			m_currentBasicBlock->addExport(reg, importName);
		}

		void setBlockTargetBranchTaken(BlockBranchTarget target)
		{
			if (m_targetBranchTaken != INVALID_BLOCK_NAME)
				assert_dbg();
			m_targetBranchTaken = target;
		}

		void setBlockTargetBranchNotTaken(BlockBranchTarget target)
		{
			if (m_targetBranchNotTaken != INVALID_BLOCK_NAME)
				assert_dbg();
			m_targetBranchNotTaken = target;
		}

	private:
		ZpIRBasicBlock* m_currentBasicBlock{};
		std::vector<ZpIRBasicBlock*> m_blocks;
		std::unordered_map<BlockBranchTarget, ZpIRBasicBlock*> m_blocksByName;

		BlockBranchTarget m_targetBranchNotTaken{ INVALID_BLOCK_NAME };
		BlockBranchTarget m_targetBranchTaken{ INVALID_BLOCK_NAME };

	};

}
