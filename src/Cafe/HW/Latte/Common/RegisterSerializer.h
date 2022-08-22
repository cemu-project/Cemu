#pragma once
#include "util/helpers/Serializer.h"

namespace Latte
{
	struct GPUCompactedRegisterState
	{
		static constexpr int const NUM_REGS = 1854;

		// tied to g_gpuRegSerializerMapping_v1
		uint32 rawArray[NUM_REGS];
	};

	// convert GPU register state into compacted representation. Stores almost all registers, excluding ALU consts
	void StoreGPURegisterState(const LatteContextRegister& contextRegister, GPUCompactedRegisterState& registerStateOut);
	void LoadGPURegisterState(LatteContextRegister& contextRegisterOut, const GPUCompactedRegisterState& registerState);

	void SerializeRegisterState(GPUCompactedRegisterState& regState, MemStreamWriter& memWriter);
	bool DeserializeRegisterState(GPUCompactedRegisterState& regState, MemStreamReader& memReader);
}