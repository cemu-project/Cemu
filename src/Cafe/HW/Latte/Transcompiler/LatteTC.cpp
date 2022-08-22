#include "Cafe/HW/Latte/Transcompiler/LatteTC.h"
#include "Cafe/HW/Latte/ISA/LatteInstructions.h"

#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZpIRBuilder.h"
#include "util/Zir/Core/ZpIRDebug.h"

class CFBlockNode
{
public:
	CFBlockNode(uint32 cfAddress, const LatteCFInstruction& cfInstruction) : cfAddress(cfAddress), cfNext(cfAddress + sizeof(LatteCFInstruction))
	{
		m_cfInstructions.emplace_back(cfInstruction);
		irBasicBlock = new ZpIR::ZpIRBasicBlock();
	};

	void addInstruction(const LatteCFInstruction& cfInstruction)
	{
		m_cfInstructions.emplace_back(cfInstruction);
	}

	// next CF address after this block of CF instructions (assuming no branches)
	void setNextAddress(uint32 addr)
	{
		cfNext = addr;
	}

	uint32 cfAddress; // offset of the first cf instruction
	uint32 cfNext{ 0 }; // offset of the next cf instruction if no branch happens

	ZpIR::ZpIRBasicBlock* irBasicBlock{};

	std::vector<LatteCFInstruction> m_cfInstructions;

private:
};

// emit IR code for all clauses in a DAG node
void LatteTCGenIR::genIRForNode(CFBlockNode& node)
{
	m_irGenContext.reset();
	m_irGenContext.irBuilder = new ZpIR::BasicBlockBuilder(node.irBasicBlock);
	m_irGenContext.isEntryBasicBlock = (&node == m_ctx.mainFunctionDAG.GetEntryNode());

	// for vertex shaders add initialization code to main()
	if (&node == m_ctx.mainFunctionDAG.GetEntryNode() && m_ctx.shaderType == SHADER_TYPE::VERTEX)
	{
		for (uint32 channel = 0; channel < 4; channel++)
		{
			ZpIR::IRReg irReg = m_irGenContext.irBuilder->createReg(ZpIR::DataType::U32);
			m_irGenContext.irBuilder->emit_RR(ZpIR::IR::OpCode::MOV, irReg, m_irGenContext.irBuilder->createConstU32(0));
			this->m_irGenContext.activeVars.set(0, channel, irReg);
		}
		// todo - correctly init R0 based on currently set context register state
	}


	for (auto& itr : node.m_cfInstructions)
	{
		const auto opcode = itr.getField_Opcode();
		if (const auto cfInstr = itr.getParserIfOpcodeMatch<LatteCFInstruction_DEFAULT>())
		{
			if(opcode == LatteCFInstruction::OPCODE::INST_CALL_FS)
				processCF_CALL_FS(*cfInstr);
			else
				assert_dbg();
		}
		else if (const auto cfInstr = itr.getParserIfOpcodeMatch<LatteCFInstruction_ALU>())
		{
			if (opcode == LatteCFInstruction::OPCODE::INST_ALU)
				processCF_ALU(*cfInstr);
			else
				assert_dbg();
		}
		else if (const auto cfInstr = itr.getParserIfOpcodeMatch<LatteCFInstruction_EXPORT_IMPORT>())
		{
			if (opcode == LatteCFInstruction::OPCODE::INST_EXPORT || 
				opcode == LatteCFInstruction::OPCODE::INST_EXPORT_DONE)
				processCF_EXPORT(*cfInstr);
			else
				assert_dbg();
		}
		else
		{
			debug_printf("Missing implementation for CF opcode 0x%02x\n", itr.getField_Opcode());
			assert_dbg(); // todo
		}
	}
}

// parse CF program and create unlinked DAG
void LatteTCGenIR::parseCF_createNodes(NodeDAG& nodeDAG)
{
	const LatteCFInstruction* cfCode = (const LatteCFInstruction*)m_ctx.programData;
	const size_t cfMaxCount = m_ctx.programSize / 8;

	// quick prepass to gather a list of jump destinations used by the next pass
	// todo

	// linear pass where we turn uninterrupted sequences of CF instructions (no branch to or from) into CFBlockNode
	// algorithm description:
	// 1) Create CFBlockNode from first CF instruction. Make it the currently active node
	// 2) For each remaining (1 .. n) CF instruction of program
	// 2.1) If CF instruction can be merged into active node (no branch destination, no conditionals or other control flow branches) then add it to the currently active node
	// 2.2) Otherwise finalize active node, add it to node list. Then create new CFBlockNode node from CF instruction and make it active node
	// 3) Finalize active node and add to node list

	cemu_assert_debug(cfMaxCount != 0); // zero not allowed

	CFBlockNode* activeNode = new CFBlockNode(0, cfCode[0]); // first instruction becomes the initial node
	size_t cfIndex = 1;
	//m_nodes.emplace_back(activeNode);
	while (cfIndex < cfMaxCount)
	{
		const LatteCFInstruction* baseInstr = cfCode + cfIndex;
		cfIndex++;
		bool canMerge;
		bool isALU = false;
		if (const auto cfInstr = baseInstr->getParserIfOpcodeMatch<LatteCFInstruction_DEFAULT>())
		{
			cemu_assert_debug(cfInstr->getField_WHOLE_QUAD_MODE() == 0);

			cemu_assert_debug(cfInstr->getField_CALL_COUNT() == 0); // todo
			cemu_assert_debug(cfInstr->getField_POP_COUNT() == 0); // todo

			auto cond = cfInstr->getField_COND();

			assert_dbg();
			//cfInstr->getField_COND() == LatteCFInstruction::CF_COND::CF_COND_ACTIVE;
		}
		else if (const auto cfInstr = baseInstr->getParserIfOpcodeMatch<LatteCFInstruction_ALU>())
		{
			// always merge ALU clauses since they dont have their own condition modes?
			// todo - except if they are a jump target
			canMerge = true;
			isALU = true;
		}
		else if (const auto cfInstr = baseInstr->getParserIfOpcodeMatch<LatteCFInstruction_EXPORT_IMPORT>())
		{
			// no extra conditions, always merge
			canMerge = true;
		}
		else
		{
			debug_printf("Missing implementation for CF opcode 0x%02x\n", baseInstr->getField_Opcode());
			assert_dbg(); // todo
		}

		if (canMerge)
		{
			activeNode->addInstruction(*baseInstr);
		}
		else
		{
			activeNode->setNextAddress((uint32)cfIndex - 1);
			nodeDAG.m_nodes.emplace_back(activeNode);
			// start new active node
			activeNode = new CFBlockNode((uint32)cfIndex - 1, *baseInstr);
		}

		if (!isALU && baseInstr->getField_END_OF_PROGRAM())
			break;
	}
	// finalize last node
	cemu_assert_debug(!activeNode->m_cfInstructions.empty());
	nodeDAG.m_nodes.emplace_back(activeNode);
}

void LatteTCGenIR::parseCFToDAG()
{
	// parse CF and create preliminary node DAG
	parseCF_createNodes(m_ctx.mainFunctionDAG);
	// link up the nodes
	cemu_assert_debug(m_ctx.mainFunctionDAG.m_nodes.size() == 1);
	// assign to ir object
	for (auto& itr : m_ctx.mainFunctionDAG.m_nodes)
		m_ctx.irObject->m_basicBlocks.emplace_back(itr->irBasicBlock);
	m_ctx.irObject->m_entryBlocks.emplace_back(m_ctx.mainFunctionDAG.m_nodes[0]->irBasicBlock);
}

void LatteTCGenIR::emitIR()
{
	cemu_assert_debug(m_ctx.mainFunctionDAG.m_nodes.size() == 1);

	for (auto& itr : m_ctx.mainFunctionDAG.m_nodes)
	{
		genIRForNode(*itr);
	}

}

void LatteTCGenIR::cleanup()
{
	// clean up
	//for (auto itr : m_ctx.list_irNodesCtx)
	//	delete itr;
	//m_ctx.list_irNodesCtx.clear();
}

void LatteTCGenIR::setVertexShaderContext(const LatteFetchShader* parsedFetchShader, const uint32* vtxSemanticTable)
{
	m_vertexShaderCtx.parsedFetchShader = parsedFetchShader;
	m_vertexShaderCtx.vtxSemanticTable = vtxSemanticTable;
}


ZpIR::ZpIRFunction* LatteTCGenIR::transcompileLatteToIR(const void* programData, uint32 programSize, SHADER_TYPE shaderType)
{
	//return nullptr;
	ZpIR::ZpIRFunction* irObject = new ZpIR::ZpIRFunction();

	// init context
	m_ctx = {};
	m_ctx.programData = (const uint32*)programData;
	m_ctx.programSize = programSize;
	m_ctx.irObject = irObject;
	m_ctx.shaderType = shaderType;

	// parse control flow instructions and convert it to list of CFBlockNode
	// each node is a single IR basic block, consisting of one or multiple CF instructions
	parseCFToDAG();
	// process clauses and emit IR nodes
	emitIR();
	// cleanup
	cleanup();
	return irObject;
}