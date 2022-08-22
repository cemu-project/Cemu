#pragma once
#include "Cafe/HW/Latte/Core/LatteTextureReadbackInfo.h"

class LatteTextureReadbackInfoVk : public LatteTextureReadbackInfo
{
public:
	LatteTextureReadbackInfoVk(VkDevice device, LatteTextureView* textureView);
	~LatteTextureReadbackInfoVk();

	static uint32 GetImageSize(LatteTextureView* textureView);

	void StartTransfer() override;

	bool IsFinished() override;
	void ForceFinish() override;

	uint8* GetData() override
	{
		return m_buffer_ptr + m_buffer_offset;
	}

	uint32 GetImageSize() const
	{
		return m_image_size;
	}

	void SetBuffer(VkBuffer buffer, uint8* buffer_ptr, const uint32 buffer_offset)
	{
		m_buffer = buffer;
		m_buffer_ptr = buffer_ptr;
		m_buffer_offset = buffer_offset;
	}

private:
	VkDevice m_device = nullptr;

	VkBuffer m_buffer = nullptr;
	uint8* m_buffer_ptr = nullptr;
	uint32 m_buffer_offset = 0;
	uint64 m_associatedCommandBufferId = 0;
};
