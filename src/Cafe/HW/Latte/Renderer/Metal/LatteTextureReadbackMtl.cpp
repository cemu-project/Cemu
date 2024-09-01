#include "Cafe/HW/Latte/Renderer/Metal/MetalRenderer.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureReadbackMtl.h"
#include "Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.h"
#include "HW/Latte/Renderer/Metal/LatteToMtl.h"

LatteTextureReadbackInfoMtl::~LatteTextureReadbackInfoMtl()
{
}

void LatteTextureReadbackInfoMtl::StartTransfer()
{
	cemu_assert(m_textureView);

	auto* baseTexture = (LatteTextureMtl*)m_textureView->baseTexture;

	cemu_assert_debug(m_textureView->firstSlice == 0);
	cemu_assert_debug(m_textureView->firstMip == 0);
	cemu_assert_debug(m_textureView->baseTexture->dim != Latte::E_DIM::DIM_3D);

	size_t bytesPerRow = GetMtlTextureBytesPerRow(baseTexture->format, baseTexture->IsDepth(), baseTexture->width);
	size_t bytesPerImage = GetMtlTextureBytesPerImage(baseTexture->format, baseTexture->IsDepth(), baseTexture->height, bytesPerRow);

	auto blitCommandEncoder = m_mtlr->GetBlitCommandEncoder();

	blitCommandEncoder->copyFromTexture(baseTexture->GetTexture(), 0, 0, MTL::Origin{0, 0, 0}, MTL::Size{(uint32)baseTexture->width, (uint32)baseTexture->height, 1}, m_mtlr->GetTextureReadbackBuffer(), m_bufferOffset, bytesPerRow, bytesPerImage);

	m_commandBuffer = m_mtlr->GetCurrentCommandBuffer();
}

bool LatteTextureReadbackInfoMtl::IsFinished()
{
    // TODO: is this needed?
    if (!m_commandBuffer)
        return false;

    // TODO: remove this?
    // Command buffer wasn't even comitted, let's commit immediately
    if (m_mtlr->GetCurrentCommandBuffer() == m_commandBuffer)
        m_mtlr->CommitCommandBuffer();

    return m_mtlr->CommandBufferCompleted(m_commandBuffer);
}

void LatteTextureReadbackInfoMtl::ForceFinish()
{
    m_mtlr->WaitForCommandBufferCompletion(m_commandBuffer);
}

uint8* LatteTextureReadbackInfoMtl::GetData()
{
	return (uint8*)m_mtlr->GetTextureReadbackBuffer()->contents() + m_bufferOffset;
}