#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"

#include "Cafe/HW/Latte/Core/LattePerformanceMonitor.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"

#include "Cafe/OS/libs/gx2/GX2.h" // todo - remove dependency

void OpenGLRenderer::streamout_setupXfbBuffer(uint32 bufferIndex, sint32 ringBufferOffset, uint32 rangeAddr, uint32 rangeSize)
{
	catchOpenGLError();
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, bufferIndex, glStreamoutCacheRingBuffer, ringBufferOffset, rangeSize);
	catchOpenGLError();
}

void OpenGLRenderer::streamout_begin()
{
	// get primitive mode
	GLenum glTransformFeedbackPrimitiveMode;
	auto primitiveMode = LatteGPUState.contextNew.VGT_PRIMITIVE_TYPE.get_PRIMITIVE_MODE();
	if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::POINTS)
		glTransformFeedbackPrimitiveMode = GL_POINTS;
	else if(primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::TRIANGLES)
		glTransformFeedbackPrimitiveMode = GL_POINTS;
	else if (primitiveMode == Latte::LATTE_VGT_PRIMITIVE_TYPE::E_PRIMITIVE_TYPE::QUADS)
		glTransformFeedbackPrimitiveMode = GL_POINTS;
	else
	{
		debug_printf("Unsupported streamout primitive mode 0x%02x\n", primitiveMode);
		cemu_assert_debug(false);
	}
	cemu_assert_debug(m_isXfbActive == false);
	glEnable(GL_RASTERIZER_DISCARD_EXT);
	glBeginTransformFeedback(glTransformFeedbackPrimitiveMode);
	catchOpenGLError();
	m_isXfbActive = true;
}

void OpenGLRenderer::bufferCache_copyStreamoutToMainBuffer(uint32 srcOffset, uint32 dstOffset, uint32 size)
{
	if (glCopyNamedBufferSubData)
		glCopyNamedBufferSubData(glStreamoutCacheRingBuffer, glAttributeCacheAB, srcOffset, dstOffset, size);
	else
		cemuLog_log(LogType::Force, "glCopyNamedBufferSubData() not supported");
}

void OpenGLRenderer::streamout_rendererFinishDrawcall()
{
	if (m_isXfbActive)
	{
		glEndTransformFeedback();
		glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
		glDisable(GL_RASTERIZER_DISCARD_EXT);

		m_isXfbActive = false;
	}

}
