#include "Cafe/HW/Latte/Renderer/Metal/MetalDepthStencilCache.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "HW/Latte/ISA/RegDefines.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Metal/MTLDepthStencil.hpp"

MetalDepthStencilCache::~MetalDepthStencilCache()
{
    for (auto& pair : m_depthStencilCache)
    {
        pair.second->release();
    }
    m_depthStencilCache.clear();
}

MTL::DepthStencilState* MetalDepthStencilCache::GetDepthStencilState(const LatteContextRegister& lcr)
{
    uint64 stateHash = CalculateDepthStencilHash(lcr);
    auto& depthStencilState = m_depthStencilCache[stateHash];
    if (depthStencilState)
    {
        return depthStencilState;
    }

	// Depth stencil state
	bool depthEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_ENABLE();
	auto depthFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_FUNC();
	bool depthWriteEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_WRITE_ENABLE();

	MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();
	desc->setDepthWriteEnabled(depthWriteEnable);

	auto depthCompareFunc = GetMtlCompareFunc(depthFunc);
	if (!depthEnable)
	{
	    depthCompareFunc = MTL::CompareFunctionAlways;
    }
	desc->setDepthCompareFunction(depthCompareFunc);

	// TODO: stencil state
	/*
	// get stencil control parameters
	bool stencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	bool backStencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_BACK_STENCIL_ENABLE();
	auto frontStencilFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FUNC_F();
	auto frontStencilZPass = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZPASS_F();
	auto frontStencilZFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZFAIL_F();
	auto frontStencilFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FAIL_F();
	auto backStencilFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FUNC_B();
	auto backStencilZPass = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZPASS_B();
	auto backStencilZFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ZFAIL_B();
	auto backStencilFail = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_FAIL_B();
	// get stencil control parameters
	uint32 stencilCompareMaskFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILMASK_F();
	uint32 stencilWriteMaskFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILWRITEMASK_F();
	uint32 stencilRefFront = LatteGPUState.contextNew.DB_STENCILREFMASK.get_STENCILREF_F();
	uint32 stencilCompareMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILMASK_B();
	uint32 stencilWriteMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILWRITEMASK_B();
	uint32 stencilRefBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILREF_B();

	static const VkStencilOp stencilOpTable[8] = {
		VK_STENCIL_OP_KEEP,
		VK_STENCIL_OP_ZERO,
		VK_STENCIL_OP_REPLACE,
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,
		VK_STENCIL_OP_DECREMENT_AND_CLAMP,
		VK_STENCIL_OP_INVERT,
		VK_STENCIL_OP_INCREMENT_AND_WRAP,
		VK_STENCIL_OP_DECREMENT_AND_WRAP
	};

	depthStencilState.stencilTestEnable = stencilEnable ? VK_TRUE : VK_FALSE;

	depthStencilState.front.reference = stencilRefFront;
	depthStencilState.front.compareMask = stencilCompareMaskFront;
	depthStencilState.front.writeMask = stencilWriteMaskBack;
	depthStencilState.front.compareOp = vkDepthCompareTable[(size_t)frontStencilFunc];
	depthStencilState.front.depthFailOp = stencilOpTable[(size_t)frontStencilZFail];
	depthStencilState.front.failOp = stencilOpTable[(size_t)frontStencilFail];
	depthStencilState.front.passOp = stencilOpTable[(size_t)frontStencilZPass];

	if (backStencilEnable)
	{
		depthStencilState.back.reference = stencilRefBack;
		depthStencilState.back.compareMask = stencilCompareMaskBack;
		depthStencilState.back.writeMask = stencilWriteMaskBack;
		depthStencilState.back.compareOp = vkDepthCompareTable[(size_t)backStencilFunc];
		depthStencilState.back.depthFailOp = stencilOpTable[(size_t)backStencilZFail];
		depthStencilState.back.failOp = stencilOpTable[(size_t)backStencilFail];
		depthStencilState.back.passOp = stencilOpTable[(size_t)backStencilZPass];
	}
	else
	{
		depthStencilState.back.reference = stencilRefFront;
		depthStencilState.back.compareMask = stencilCompareMaskFront;
		depthStencilState.back.writeMask = stencilWriteMaskFront;
		depthStencilState.back.compareOp = vkDepthCompareTable[(size_t)frontStencilFunc];
		depthStencilState.back.depthFailOp = stencilOpTable[(size_t)frontStencilZFail];
		depthStencilState.back.failOp = stencilOpTable[(size_t)frontStencilFail];
		depthStencilState.back.passOp = stencilOpTable[(size_t)frontStencilZPass];
	}
	*/

	depthStencilState = m_mtlr->GetDevice()->newDepthStencilState(desc);
	desc->release();

	return depthStencilState;
}

uint64 MetalDepthStencilCache::CalculateDepthStencilHash(const LatteContextRegister& lcr)
{
    uint32* ctxRegister = lcr.GetRawView();

    // Hash
    uint64 stateHash = 0;
    uint32 depthControl = ctxRegister[Latte::REGADDR::DB_DEPTH_CONTROL];
	bool stencilTestEnable = depthControl & 1;
	if (stencilTestEnable)
	{
		stateHash += ctxRegister[mmDB_STENCILREFMASK];
		stateHash = std::rotl<uint64>(stateHash, 17);
		if(depthControl & (1<<7)) // back stencil enable
		{
			stateHash += ctxRegister[mmDB_STENCILREFMASK_BF];
			stateHash = std::rotl<uint64>(stateHash, 13);
		}
	}
	else
	{
		// zero out stencil related bits (8-31)
		depthControl &= 0xFF;
	}

	stateHash = std::rotl<uint64>(stateHash, 17);
	stateHash += depthControl;

	return stateHash;
}
