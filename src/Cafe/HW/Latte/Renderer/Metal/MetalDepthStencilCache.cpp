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
        return depthStencilState;

	// Depth stencil state
	bool depthEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_ENABLE();
	auto depthFunc = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_FUNC();
	bool depthWriteEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_Z_WRITE_ENABLE();

	MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();
	desc->setDepthWriteEnabled(depthWriteEnable);

	if (depthEnable)
	{
	    desc->setDepthCompareFunction(GetMtlCompareFunc(depthFunc));
    }

	// Stencil state
	bool stencilEnable = LatteGPUState.contextNew.DB_DEPTH_CONTROL.get_STENCIL_ENABLE();
	if (stencilEnable)
	{
	    // get stencil control parameters
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
    	uint32 stencilCompareMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILMASK_B();
    	uint32 stencilWriteMaskBack = LatteGPUState.contextNew.DB_STENCILREFMASK_BF.get_STENCILWRITEMASK_B();

    	MTL::StencilDescriptor* frontStencil = MTL::StencilDescriptor::alloc()->init();
    	// TODO: set reference
    	//depthStencilState.front.reference = stencilRefFront;
    	frontStencil->setReadMask(stencilCompareMaskFront);
    	frontStencil->setWriteMask(stencilWriteMaskFront);
    	frontStencil->setStencilCompareFunction(GetMtlCompareFunc(frontStencilFunc));
    	frontStencil->setDepthFailureOperation(GetMtlStencilOp(frontStencilZFail));
    	frontStencil->setStencilFailureOperation(GetMtlStencilOp(frontStencilFail));
    	frontStencil->setDepthStencilPassOperation(GetMtlStencilOp(frontStencilZPass));
    	desc->setFrontFaceStencil(frontStencil);

    	MTL::StencilDescriptor* backStencil = MTL::StencilDescriptor::alloc()->init();
    	if (backStencilEnable)
    	{
           	// TODO: set reference
           	//depthStencilState.back.reference = stencilRefBack;
           	backStencil->setReadMask(stencilCompareMaskBack);
           	backStencil->setWriteMask(stencilWriteMaskBack);
           	backStencil->setStencilCompareFunction(GetMtlCompareFunc(backStencilFunc));
           	backStencil->setDepthFailureOperation(GetMtlStencilOp(backStencilZFail));
           	backStencil->setStencilFailureOperation(GetMtlStencilOp(backStencilFail));
           	backStencil->setDepthStencilPassOperation(GetMtlStencilOp(backStencilZPass));
    	}
    	else
    	{
    	    // TODO: set reference
           	//depthStencilState.back.reference = stencilRefFront;
           	backStencil->setReadMask(stencilCompareMaskFront);
           	backStencil->setWriteMask(stencilWriteMaskFront);
           	backStencil->setStencilCompareFunction(GetMtlCompareFunc(frontStencilFunc));
           	backStencil->setDepthFailureOperation(GetMtlStencilOp(frontStencilZFail));
           	backStencil->setStencilFailureOperation(GetMtlStencilOp(frontStencilFail));
           	backStencil->setDepthStencilPassOperation(GetMtlStencilOp(frontStencilZPass));
    	}
    	desc->setBackFaceStencil(backStencil);

        frontStencil->release();
        backStencil->release();
	}

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
