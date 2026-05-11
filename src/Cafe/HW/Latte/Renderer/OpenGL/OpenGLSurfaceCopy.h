#pragma once

typedef struct
{
	GLuint glProgamId;
	struct
	{
		GLuint uniformLoc_textureSrc;
		GLuint uniformLoc_vertexOffsets;
	}copyShaderUniforms;
}LatteGLDefaultShader_t;

LatteGLDefaultShader_t* LatteGLDefaultShader_getPixelCopyShader_depthToColor();
LatteGLDefaultShader_t* LatteGLDefaultShader_getPixelCopyShader_colorToDepth();