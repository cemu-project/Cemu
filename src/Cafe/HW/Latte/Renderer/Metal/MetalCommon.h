#pragma once

#include <Metal/Metal.hpp>

constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();

inline size_t Align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline std::string GetColorAttachmentTypeStr(uint32 index)
{
    return "COLOR_ATTACHMENT" + std::to_string(index) + "_TYPE";
}
