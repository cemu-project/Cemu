#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cemu/Logging/CemuLogging.h"

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader)
{
    NS::Error* error = nullptr;
	MTL::Library* library = mtlRenderer->GetDevice()->newLibrary(NS::String::string(mslCode.c_str(), NS::ASCIIStringEncoding), nullptr, &error);
	if (error)
    {
        printf("Failed to create library (error: %s) -> source:\n%s", error->localizedDescription()->utf8String(), mslCode.c_str());
        error->release();
        return;
    }
    m_function = library->newFunction(NS::String::string("main0", NS::ASCIIStringEncoding));
    library->release();
}

RendererShaderMtl::~RendererShaderMtl()
{
    if (m_function)
        m_function->release();
}

void RendererShaderMtl::ShaderCacheLoading_begin(uint64 cacheTitleId)
{
	cemuLog_log(LogType::MetalLogging, "RendererShaderMtl::ShaderCacheLoading_begin not implemented!");
}

void RendererShaderMtl::ShaderCacheLoading_end()
{
	cemuLog_log(LogType::MetalLogging, "RendererShaderMtl::ShaderCacheLoading_end not implemented!");
}

void RendererShaderMtl::ShaderCacheLoading_Close()
{
    cemuLog_log(LogType::MetalLogging, "RendererShaderMtl::ShaderCacheLoading_Close not implemented!");
}
