#pragma once
#include "Cafe/HW/Latte/Core/LatteTextureReadbackInfo.h"
#include "Common/GLInclude/GLInclude.h"

class LatteTextureReadbackInfoGL : public LatteTextureReadbackInfo
{
public:
	LatteTextureReadbackInfoGL(LatteTextureView* textureView);

	~LatteTextureReadbackInfoGL();

	void StartTransfer() override;
	bool IsFinished() override;
	void ForceFinish() override;

	uint8* GetData() override;
	void ReleaseData() override;

private:
	bool WaitForCompletion(GLuint64 timeout);
	GLuint m_texFormatGL;
	GLuint m_texDataTypeGL;

	// buffer
	GLuint texImageBufferGL = 0;
	// sync
	GLsync imageCopyFinSync = nullptr;
};

