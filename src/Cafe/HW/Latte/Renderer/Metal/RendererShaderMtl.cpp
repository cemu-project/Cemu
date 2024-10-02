#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
//#include "Cemu/FileCache/FileCache.h"
//#include "config/ActiveSettings.h"

#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"
#include "config/CemuConfig.h"

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_mtlr{mtlRenderer}
{
    MTL::CompileOptions* options = MTL::CompileOptions::alloc()->init();
    if (GetConfig().fast_math)
        options->setFastMathEnabled(true);

    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(ToNSString(mslCode), options, &error);
	options->release();
	if (error)
    {
        cemuLog_log(LogType::Force, "failed to create library: {} -> {}", error->localizedDescription()->utf8String(), mslCode.c_str());
        error->release();
        return;
    }
    m_function = library->newFunction(ToNSString("main0"));
    library->release();

	// Count shader compilation
	g_compiled_shaders_total++;
}

RendererShaderMtl::~RendererShaderMtl()
{
    if (m_function)
        m_function->release();
}
