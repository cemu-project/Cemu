#pragma once
#include "Cafe/HW/Latte/ISA/LatteInstructions.h"
#include "Cafe/HW/Latte/Core/FetchShader.h"
#include "Cafe/HW/Latte/Core/LatteConst.h"
#include "util/Zir/Core/IR.h"

#include <boost/container/static_vector.hpp>

namespace ZpIR
{
	class BasicBlockBuilder;
}

// transforms GPU7/Latte shader binaries into an SSA IR (ZpIR) representation
class LatteTCGenIR
{
	//friend class CFNodeInfo;
	friend struct CFNodeInfo_CALL_FS;
private:
	struct NodeDAG // one per function (shaders usually have main only)
	{
		std::vector<class CFBlockNode*> m_nodes;
		// class CFBlockNode* entrypoint;

		class CFBlockNode* GetEntryNode()
		{
			return m_nodes[0];
		}
	};
	
public:
	typedef unsigned short GPRElementIndex; // grpIndex*4+channel

	enum SHADER_TYPE
	{
		VERTEX,
		GEOMETRY,
		PIXEL,
	};

	void setVertexShaderContext(const LatteFetchShader* parsedFetchShader, const uint32* vtxSemanticTable);

	ZpIR::ZpIRFunction* transcompileLatteToIR(const void* programData, uint32 programSize, SHADER_TYPE shaderType);

private:
	ZpIR::IRReg getIRRegFromGPRElement(uint32 gprIndex, uint32 channel, ZpIR::DataType typeHint);
	ZpIR::IRReg getTypedIRRegFromGPRElement(uint32 gprIndex, uint32 channel, ZpIR::DataType type);

	ZpIR::IRReg loadALUOperand(LatteALUSrcSel srcSel, uint8 srcChan, bool isNeg, bool isAbs, bool isRel, uint8 indexMode, const uint32* literalData, ZpIR::DataType typeHint, bool convertOnTypeMismatch);
	void emitALUGroup(const class LatteClauseInstruction_ALU* aluUnit[5], const uint32* literalData);
	
	void genIRForNode(class CFBlockNode& node);

	void parseCFToDAG();
	void parseCF_createNodes(NodeDAG& nodeDAG);
	void emitIR();
	void cleanup();

	// IR emitter
	void processCF_CALL_FS(const LatteCFInstruction_DEFAULT& cfInstruction);
	void processCF_ALU(const LatteCFInstruction_ALU& cfInstruction);
	void processCF_EXPORT(const LatteCFInstruction_EXPORT_IMPORT& cfInstruction);

	// helpers
	void CF_CALL_FS_emitFetchAttribute(LatteParsedFetchShaderAttribute_t& attribute, Latte::GPRType dstGPR);

private:
	// tracks mapping GPR<->IR variable for current basic block
	struct IREmitterActiveVars
	{
		bool get(uint32 gprIndex, uint32 channelIndex, ZpIR::IRReg& reg)
		{
			size_t index = gprIndex * 4 + channelIndex;
			if (!m_present.test(index))
				return false;
			reg = m_irReg[index];
			return true;
		}

		void set(uint32 gprIndex, uint32 channelIndex, ZpIR::IRReg reg)
		{
			size_t index = gprIndex * 4 + channelIndex;
			m_present.set(index);
			m_irReg[index] = reg;
		}

		// set register after current group
		struct DelayedAssignment
		{
			uint16 index;
			ZpIR::IRReg reg;
			bool isSet{false};
		};

		struct DelayedAssignmentPVPS
		{
			ZpIR::IRReg reg;
			bool isSet{ false };
		};

		void setAfterGroup(uint8 aluUnit, uint32 gprIndex, uint32 channelIndex, ZpIR::IRReg reg)
		{
			cemu_assert_debug(aluUnit < 5);
			cemu_assert_debug(!m_delayedAssignments[aluUnit].isSet);
			m_delayedAssignments[aluUnit].reg = reg;
			m_delayedAssignments[aluUnit].index = gprIndex * 4 + channelIndex;
			m_delayedAssignments[aluUnit].isSet = true;
		}

		void setAfterGroupPVPS(uint8 aluUnit, ZpIR::IRReg reg)
		{
			cemu_assert_debug(aluUnit < 5);
			cemu_assert_debug(!m_delayedAssignmentsPSPV[aluUnit].isSet);
			m_delayedAssignmentsPSPV[aluUnit].reg = reg;
			m_delayedAssignmentsPSPV[aluUnit].isSet = true;
		}

		void applyDelayedAfterGroup()
		{
			// GPRs
			for (size_t i = 0; i < 5; i++)
			{
				auto& assignment = m_delayedAssignments[i];
				if(!assignment.isSet)
					continue;
				setGPR(assignment.index, assignment.reg);
				assignment.isSet = false;
			}
			// PV/PS
			for (size_t i = 0; i < 5; i++)
			{
				auto& assignment = m_delayedAssignmentsPSPV[i];
				if (!assignment.isSet)
					continue;
				setPVPS((uint32)i, assignment.reg);
				assignment.isSet = false;
			}
		}

		void reset()
		{
			m_present.reset();
		}

	private:
		void setGPR(uint32 gprChannelindex, ZpIR::IRReg reg)
		{
			m_present.set(gprChannelindex);
			m_irReg[gprChannelindex] = reg;
		}

		void setPVPS(uint32 unitIndex, ZpIR::IRReg reg)
		{
			m_presentPVPS.set(unitIndex);
			m_irRegPVPS[unitIndex] = reg;
		}

		ZpIR::IRReg m_irReg[128 * 4]{};
		ZpIR::IRReg m_irRegPVPS[5]{};
		std::bitset<128 * 4> m_present;
		std::bitset<5> m_presentPVPS;
		DelayedAssignment m_delayedAssignments[5]{};
		DelayedAssignmentPVPS m_delayedAssignmentsPSPV[5]{};
	};

	struct
	{
		IREmitterActiveVars activeVars;
		ZpIR::BasicBlockBuilder* irBuilder;
		bool isEntryBasicBlock{};

		void reset()
		{
			activeVars.reset();
		}
	}m_irGenContext;

	struct  
	{
		const uint32* programData;
		uint32 programSize;
		//std::vector<struct CFNodeInfo*> list_irNodesCtx;
		// first node in flow of main() function
		//struct CFNodeInfo* mainEntry;
		
		SHADER_TYPE shaderType;

		NodeDAG mainFunctionDAG;
		// current IR object
		struct ZpIR::ZpIRFunction* irObject;
	}m_ctx;


	// vertex shader info
	struct
	{
		const LatteFetchShader* parsedFetchShader{};
		const uint32* vtxSemanticTable{};
	}m_vertexShaderCtx{};
};
