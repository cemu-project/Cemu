#include "RendererCore.h"
#include "Cafe/HW/Latte/Renderer/Renderer.h"
#include "Cafe/HW/Latte/ISA/RegDefines.h"
#include "HW/Latte/Core/LatteShader.h"
#include "config/CemuConfig.h"

#ifdef ENABLE_OPENGL
#include "Common/GLInclude/GLInclude.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.h"
#endif

void LatteDraw_handleSpecialState8_clearAsDepth()
{
	if (LatteGPUState.contextNew.GetSpecialStateValues()[0] == 0)
		cemuLog_logDebug(LogType::Force, "Special state 8 requires special state 0 but it is not set?");
	// get depth buffer information
	uint32 regDepthBuffer = LatteGPUState.contextRegister[mmDB_HTILE_DATA_BASE];
	uint32 regDepthSize = LatteGPUState.contextRegister[mmDB_DEPTH_SIZE];
	uint32 regDepthBufferInfo = LatteGPUState.contextRegister[mmDB_DEPTH_INFO];
	// get format and tileMode from info reg
	uint32 depthBufferTileMode = (regDepthBufferInfo >> 15) & 0xF;

	MPTR depthBufferPhysMem = regDepthBuffer << 8;
	uint32 depthBufferPitch = (((regDepthSize >> 0) & 0x3FF) + 1);
	uint32 depthBufferHeight = ((((regDepthSize >> 10) & 0xFFFFF) + 1) / depthBufferPitch);
	depthBufferPitch <<= 3;
	depthBufferHeight <<= 3;
	uint32 depthBufferWidth = depthBufferPitch;

	sint32 sliceIndex = 0; // todo
	sint32 mipIndex = 0;

	// clear all color buffers that match the format of the depth buffer
	sint32 searchIndex = 0;
	bool targetFound = false;
	while (true)
	{
		LatteTextureView* view = LatteTC_LookupTextureByData(depthBufferPhysMem, depthBufferWidth, depthBufferHeight, depthBufferPitch, 0, 1, sliceIndex, 1, &searchIndex);
		if (!view)
		{
			// should we clear in RAM instead?
			break;
		}
		sint32 effectiveClearWidth = view->baseTexture->width;
		sint32 effectiveClearHeight = view->baseTexture->height;
		LatteTexture_scaleToEffectiveSize(view->baseTexture, &effectiveClearWidth, &effectiveClearHeight, 0);

		// hacky way to get clear color
		float* regClearColor = (float*)(LatteGPUState.contextRegister + 0xC000 + 0); // REG_BASE_ALU_CONST

		uint8 clearColor[4] = { 0 };
		clearColor[0] = (uint8)(regClearColor[0] * 255.0f);
		clearColor[1] = (uint8)(regClearColor[1] * 255.0f);
		clearColor[2] = (uint8)(regClearColor[2] * 255.0f);
		clearColor[3] = (uint8)(regClearColor[3] * 255.0f);

		// todo - use fragment shader software emulation (evoke for one pixel) to determine clear color
		// todo - dont clear entire slice, use effectiveClearWidth, effectiveClearHeight

		switch (g_renderer->GetType())
		{
#ifdef ENABLE_OPENGL
		case RendererAPI::OpenGL:
		{
			//cemu_assert_debug(false); // implement g_renderer->texture_clearColorSlice properly for OpenGL renderer
			if (glClearTexSubImage)
				glClearTexSubImage(((LatteTextureViewGL*)view)->glTexId, mipIndex, 0, 0, 0, effectiveClearWidth, effectiveClearHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
			break;
		}
#endif
		default:
		{
			if (view->baseTexture->isDepth)
				g_renderer->texture_clearDepthSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, true, view->baseTexture->hasStencil, 0.0f, 0);
			else
				g_renderer->texture_clearColorSlice(view->baseTexture, sliceIndex + view->firstSlice, mipIndex + view->firstMip, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		}
		}
	}
}

/* rects emulation */

void rectsEmulationGS_outputSingleVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 vIdx, const LatteContextRegister& latteRegister)
{
	auto parameterMask = vertexShader->outputParameterMask;
	for (uint32 i = 0; i < 32; i++)
	{
		if ((parameterMask & (1 << i)) == 0)
			continue;
		sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(latteRegister.GetRawView(), i);
		if (vsSemanticId < 0)
			continue;
		// make sure PS has matching input
		if (!psInputTable->hasPSImportForSemanticId(vsSemanticId))
			continue;
		gsSrc.append(fmt::format("passParameterSem{}Out = passParameterSem{}In[{}];\r\n", vsSemanticId, vsSemanticId, vIdx));
	}
	gsSrc.append(fmt::format("gl_Position = gl_in[{}].gl_Position;\r\n", vIdx));
	gsSrc.append("EmitVertex();\r\n");
}

void rectsEmulationGS_outputGeneratedVertex(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, const char* variant, const LatteContextRegister& latteRegister)
{
	auto parameterMask = vertexShader->outputParameterMask;
	for (uint32 i = 0; i < 32; i++)
	{
		if ((parameterMask & (1 << i)) == 0)
			continue;
		sint32 vsSemanticId = psInputTable->getVertexShaderOutParamSemanticId(latteRegister.GetRawView(), i);
		if (vsSemanticId < 0)
			continue;
		// make sure PS has matching input
		if (!psInputTable->hasPSImportForSemanticId(vsSemanticId))
			continue;
		gsSrc.append(fmt::format("passParameterSem{}Out = gen4thVertex{}(passParameterSem{}In[0], passParameterSem{}In[1], passParameterSem{}In[2]);\r\n", vsSemanticId, variant, vsSemanticId, vsSemanticId, vsSemanticId));
	}
	gsSrc.append(fmt::format("gl_Position = gen4thVertex{}(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);\r\n", variant));
	gsSrc.append("EmitVertex();\r\n");
}

void rectsEmulationGS_outputVerticesCode(std::string& gsSrc, LatteDecompilerShader* vertexShader, LatteShaderPSInputTable* psInputTable, sint32 p0, sint32 p1, sint32 p2, sint32 p3, const char* variant, const LatteContextRegister& latteRegister)
{
	sint32 pList[4] = { p0, p1, p2, p3 };
	for (sint32 i = 0; i < 4; i++)
	{
		if (pList[i] == 3)
			rectsEmulationGS_outputGeneratedVertex(gsSrc, vertexShader, psInputTable, variant, latteRegister);
		else
			rectsEmulationGS_outputSingleVertex(gsSrc, vertexShader, psInputTable, pList[i], latteRegister);
	}
}
