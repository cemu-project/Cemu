#include "Cafe/HW/Espresso/Interpreter/PPCSandboxInstructionPolicy.h"

#include <cstdlib>

namespace {

void Check(bool condition)
{
	if (!condition) std::abort();
}

std::uint32_t Extended(std::uint32_t primary, std::uint32_t extended)
{
	return (primary << 26) | (extended << 1);
}

std::uint32_t Spr(std::uint32_t extended, std::uint32_t spr)
{
	return Extended(31, extended) | ((spr & 0x1fU) << 16) | ((spr >> 5U) << 11);
}

}

int main()
{
	using cemod_sandbox::IsInstructionAllowed;
	Check(IsInstructionAllowed(0x4e800020)); // blr
	Check(IsInstructionAllowed(0x80610008)); // lwz r3,8(r1)
	Check(IsInstructionAllowed(0x90610008)); // stw r3,8(r1)
	Check(IsInstructionAllowed(Extended(31, 20))); // lwarx
	Check(IsInstructionAllowed(Extended(31, 150))); // stwcx.
	Check(IsInstructionAllowed(Extended(31, 1014))); // dcbz, checked-memory implementation
	Check(IsInstructionAllowed(Spr(339, 8))); // mflr
	Check(IsInstructionAllowed(Spr(467, 9))); // mtctr
	Check(IsInstructionAllowed(Spr(371, 268))); // mftbl, isolated timebase

	Check(!IsInstructionAllowed(0)); // unknown primary opcode
	Check(!IsInstructionAllowed(Extended(19, 50))); // rfi
	Check(!IsInstructionAllowed(Extended(31, 4))); // tw / host debugger
	Check(!IsInstructionAllowed(Extended(31, 54))); // dcbst / host GPU cache
	Check(!IsInstructionAllowed(Extended(31, 83))); // mfmsr
	Check(!IsInstructionAllowed(Extended(31, 146))); // mtmsr
	Check(!IsInstructionAllowed(Extended(31, 306))); // tlbie
	Check(!IsInstructionAllowed(Extended(31, 982))); // icbi / host code cache
	Check(!IsInstructionAllowed(Spr(339, 22))); // decrementer/global timing
	Check(!IsInstructionAllowed(Spr(467, 922))); // DMAU
	Check(!IsInstructionAllowed(Extended(31, 1023))); // unknown extended opcode
	return 0;
}
