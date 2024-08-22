#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

struct MetalPixelFormatSupport
{
	bool m_supportsR8Unorm_sRGB;
	bool m_supportsRG8Unorm_sRGB;
	bool m_supportsPacked16BitFormats;
	bool m_supportsDepth24Unorm_Stencil8;

	MetalPixelFormatSupport() = default;
	MetalPixelFormatSupport(MTL::Device* device)
	{
        m_supportsR8Unorm_sRGB = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsRG8Unorm_sRGB = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsPacked16BitFormats = device->supportsFamily(MTL::GPUFamilyApple1);
        m_supportsDepth24Unorm_Stencil8 = device->supportsFamily(MTL::GPUFamilyMac2);
	}
};

#define MAX_MTL_BUFFERS 31
// Buffer indices 28-30 are reserved for the helper shaders
#define GET_MTL_VERTEX_BUFFER_INDEX(index) (MAX_MTL_BUFFERS - index - 4)

#define MAX_MTL_TEXTURES 31
#define MAX_MTL_SAMPLERS 16

#define GET_HELPER_BUFFER_BINDING(index) (28 + index)
#define GET_HELPER_TEXTURE_BINDING(index) (29 + index)
#define GET_HELPER_SAMPLER_BINDING(index) (14 + index)

constexpr uint32 INVALID_UINT32 = std::numeric_limits<uint32>::max();
constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();

inline size_t Align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline std::string GetColorAttachmentTypeStr(uint32 index)
{
    return "COLOR_ATTACHMENT" + std::to_string(index) + "_TYPE";
}

// Cast from const char* to NS::String*
inline NS::String* ToNSString(const char* str)
{
    return NS::String::string(str, NS::ASCIIStringEncoding);
}

// Cast from std::string to NS::String*
inline NS::String* ToNSString(const std::string& str)
{
    return ToNSString(str.c_str());
}

inline NS::String* GetLabel(const std::string& label, const void* identifier)
{
    return ToNSString(label + " (" + std::to_string(reinterpret_cast<uintptr_t>(identifier)) + ")");
}
