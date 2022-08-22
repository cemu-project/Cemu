#pragma once
#include "util/Zir/Core/IR.h"
#include "util/Zir/Core/ZpIRPasses.h"
#include "util/helpers/StringBuf.h"

class DualStringBuffer;

namespace ZirEmitter
{
	class GLSL
	{
	public:
		GLSL() {};

		// emit function code and append to output string buffer
		void Emit(ZpIR::ZpIRFunction* irFunction, StringBuf* output);

	private:
		void GenerateBasicBlockCode(ZpIR::ZpIRBasicBlock& basicBlock);

		void HandleInstruction(ZpIR::IR::InsRR* ins);
		void HandleInstruction(ZpIR::IR::InsRRR* ins);
		void HandleInstruction(ZpIR::IR::InsIMPORT* ins);
		void HandleInstruction(ZpIR::IR::InsEXPORT* ins);

		void appendSourceString(DualStringBuffer* buf, ZpIR::IRReg irReg);
		void getRegisterName(char buf[16], ZpIR::IRReg irReg);

	private:
		ZpIR::ZpIRFunction* m_irFunction{};
		StringBuf* m_glslSource{};

		struct 
		{
			ZpIR::ZpIRBasicBlock* currentBasicBlock{ nullptr };
			std::vector<uint8> regReadTracking;
			std::vector<DualStringBuffer*> regInlinedExpression;

			bool CanInlineRegister(ZpIR::IRReg reg) const 
			{
				cemu_assert_debug(ZpIR::isRegVar(reg));
				return regReadTracking[ZpIR::getRegIndex(reg)] <= 1;
			};
		}m_blockContext;

		void AssignResult(ZpIR::IRReg irReg, DualStringBuffer* buf, bool forceNoInline = false);

		// inlined expression cache
		void SetRegInlinedExpression(ZpIR::IRReg irReg, DualStringBuffer* buf);
		void ResetRegInlinedExpression(ZpIR::IRReg irReg);
		DualStringBuffer* GetRegInlinedExpression(ZpIR::IRReg irReg);

		// memory pool for StringBuffer
		DualStringBuffer* GetStringBuffer();
		void ReleaseStringBuffer(DualStringBuffer* buf);

		std::vector<DualStringBuffer*> m_stringBufferCache;
	};

}