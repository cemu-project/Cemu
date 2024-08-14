#include "Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteToMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/MetalCommon.h"
#include "Cemu/FileCache/FileCache.h"
#include "config/ActiveSettings.h"

#include "Cemu/Logging/CemuLogging.h"
#include "Common/precompiled.h"

bool s_isLoadingShadersMtl{ false };
class FileCache* s_mslCache{nullptr};

extern std::atomic_int g_compiled_shaders_total;
extern std::atomic_int g_compiled_shaders_async;

RendererShaderMtl::RendererShaderMtl(MetalRenderer* mtlRenderer, ShaderType type, uint64 baseHash, uint64 auxHash, bool isGameShader, bool isGfxPackShader, const std::string& mslCode)
	: RendererShader(type, baseHash, auxHash, isGameShader, isGfxPackShader), m_mtlr{mtlRenderer}
{
    if (LoadBinary())
        return;

    if (m_type == ShaderType::kFragment)
    {
        // Fragment functions are compiled just-in-time
        m_mslCode = mslCode;
    }
    else
    {
        Compile(mslCode);
    }

    // Store the compiled shader in the cache
    StoreBinary();

	// Count shader compilation
	if (!s_isLoadingShadersMtl)
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

void RendererShaderMtl::ShaderCacheLoading_begin(uint64 cacheTitleId)
{
    if (s_mslCache)
	{
		delete s_mslCache;
	}
	uint32 spirvCacheMagic = GeneratePrecompiledCacheId();
	const std::string cacheFilename = fmt::format("{:016x}_msl.bin", cacheTitleId);
	const fs::path cachePath = ActiveSettings::GetCachePath("shaderCache/precompiled/{}", cacheFilename);
	s_mslCache = FileCache::Open(cachePath, true, spirvCacheMagic);
	if (!s_mslCache)
		cemuLog_log(LogType::Force, "Unable to open MSL cache {}", cacheFilename);
	s_isLoadingShadersMtl = true;
}

void RendererShaderMtl::ShaderCacheLoading_end()
{
	s_isLoadingShadersMtl = false;
}

void RendererShaderMtl::ShaderCacheLoading_Close()
{
    delete s_mslCache;
    g_compiled_shaders_total = 0;
    g_compiled_shaders_async = 0;
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

bool RendererShaderMtl::LoadBinary()
{
    // HACK: since fragment functions are compiled just-in-time, we cannot load them from the cache
    if (m_type == ShaderType::kFragment)
        return false;

    uint64 h1, h2;
    GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
	if (!s_mslCache->GetFile({h1, h2 }, m_binary))
		return false;

	// TODO: implement
	return false;

	return true;
}

void RendererShaderMtl::StoreBinary()
{
    if (m_binary.size() == 0)
    {
        // TODO: retrieve the binary from the function
        return;
    }

    uint64 h1, h2;
	GenerateShaderPrecompiledCacheFilename(m_type, m_baseHash, m_auxHash, h1, h2);
	s_mslCache->AddFileAsync({h1, h2 }, m_binary.data(), m_binary.size());
}
