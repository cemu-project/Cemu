#pragma once

#include <Metal/Metal.hpp>

#define MAX_MTL_BUFFERS 31
// Buffer index 30 is reserved for the support buffer, buffer indices 27-29 are reserved for the helper shaders
#define GET_MTL_VERTEX_BUFFER_INDEX(index) (MAX_MTL_BUFFERS - index - 5)
// TODO: don't harcdode the support buffer binding
#define MTL_SUPPORT_BUFFER_BINDING 30

#define MAX_MTL_TEXTURES 31
#define MAX_MTL_SAMPLERS 16

#define GET_HELPER_BUFFER_BINDING(index) (27 + index)
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
