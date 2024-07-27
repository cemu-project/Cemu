#pragma once

#include "Cafe/HW/Latte/ISA/LatteReg.h"

class MetalMemoryManager
{
public:
    MetalMemoryManager() = default;

    void* GetTextureUploadBuffer(size_t size)
    {
        if (m_textureUploadBuffer.size() < size)
        {
            m_textureUploadBuffer.resize(size);
        }

        return m_textureUploadBuffer.data();
    }

private:
    std::vector<uint8> m_textureUploadBuffer;
};
