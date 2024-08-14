#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
//#include "Cemu/FileCache/FileCache.h"
//#include "config/ActiveSettings.h"

#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_mtlr{mtlRenderer}
{
    if (m_type == ShaderType::kFragment)
    {
        // Fragment functions are compiled just-in-time
        m_mslCode = mslCode;
    }
    else
    {
        Compile(mslCode);
    }

	// Count shader compilation
	g_compiled_shaders_total++;
}

RendererShaderMtl::~RendererShaderMtl()
{
    if (m_function)
        m_function->release();
}

void RendererShaderMtl::CompileFragmentFunction(CachedFBOMtl* activeFBO)
{
    cemu_assert_debug(m_type == ShaderType::kFragment);

    if (m_function)
        m_function->release();

    std::string fullCode;

    // Define color attachment data types
    for (uint8 i = 0; i < 8; i++)
	{
	    const auto& colorBuffer = activeFBO->colorBuffer[i];
		if (!colorBuffer.texture)
		{
		    continue;
		}
		auto dataType = GetMtlPixelFormatInfo(colorBuffer.texture->format, false).dataType;
		fullCode += "#define " + GetColorAttachmentTypeStr(i) + " ";
		switch (dataType)
		{
		case MetalDataType::INT:
		    fullCode += "int4";
			break;
		case MetalDataType::UINT:
		    fullCode += "uint4";
			break;
		case MetalDataType::FLOAT:
		    fullCode += "float4";
			break;
		default:
		    cemu_assert_suspicious();
			break;
		}
		fullCode += "\n";
	}

    fullCode += m_mslCode;
    Compile(fullCode);
}

void RendererShaderMtl::Compile(const std::string& mslCode)
{
    NS::Error* error = nullptr;
	MTL::Library* library = m_mtlr->GetDevice()->newLibrary(NS::String::string(mslCode.c_str(), NS::ASCIIStringEncoding), nullptr, &error);
	if (error)
    {
        printf("failed to create library (error: %s) -> source:\n%s\n", error->localizedDescription()->utf8String(), mslCode.c_str());
        error->release();
        return;
    }
    m_function = library->newFunction(NS::String::string("main0", NS::ASCIIStringEncoding));
    library->release();
}
