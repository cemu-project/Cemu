#pragma once

inline size_t align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}
