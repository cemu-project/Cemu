#pragma once

#include "Cafe/HW/Latte/Core/LatteTexture.h"
#include "Common/GLInclude/GLInclude.h"

class LatteTextureViewGL : public LatteTextureView
{
public:
	LatteTextureViewGL(class LatteTextureGL* texture, Latte::E_DIM dim, Latte::E_GX2SURFFMT format, sint32 firstMip, sint32 mipCount, sint32 firstSlice, sint32 sliceCount, bool registerView = true, bool forceCreateNewTexId = false);
	~LatteTextureViewGL();

	LatteTextureViewGL* GetAlternativeView();

	// GL specific states
	LatteSamplerState samplerState{};
	uint8 swizzleR;
	uint8 swizzleG;
	uint8 swizzleB;
	uint8 swizzleA;

	GLuint glTexId;
	GLint glTexTarget;
	sint32 glInternalFormat;

	LatteTextureViewGL* m_alternativeView{ nullptr };
private:
	void InitAliasView();
};
