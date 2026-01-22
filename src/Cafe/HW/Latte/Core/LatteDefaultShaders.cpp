#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/Core/LatteShader.h"
#include "Cafe/HW/Latte/Core/LatteDefaultShaders.h"
#include "util/helpers/StringBuf.h"

LatteDefaultShader_t* _copyShader_depthToColor;
LatteDefaultShader_t* _copyShader_colorToDepth;

void LatteDefaultShader_pixelCopyShader_generateVSBody(StringBuf* vs)
{
	vs->add("#version 420\r\n");
	vs->add("out vec2 passUV;\r\n");
	vs->add("uniform vec4 uf_vertexOffsets[4];\r\n");
	vs->add("\r\n");
	vs->add("void main(){\r\n");
	vs->add("int vID = gl_VertexID;\r\n");
	vs->add("passUV = uf_vertexOffsets[vID].zw;\r\n");
	vs->add("gl_Position = vec4(uf_vertexOffsets[vID].xy, 0.0, 1.0);\r\n");
	vs->add("}\r\n");
}

GLuint gxShaderDepr_compileRaw(StringBuf* strSourceVS, StringBuf* strSourceFS);
GLuint gxShaderDepr_compileRaw(const std::string& vertex_source, const std::string& fragment_source);

LatteDefaultShader_t* LatteDefaultShader_getPixelCopyShader_depthToColor()
{
	if (_copyShader_depthToColor != 0)
		return _copyShader_depthToColor;
	catchOpenGLError();
	LatteDefaultShader_t* defaultShader = (LatteDefaultShader_t*)malloc(sizeof(LatteDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteDefaultShader_t));

	StringBuf fCStr_vertexShader(1024 * 16);
	LatteDefaultShader_pixelCopyShader_generateVSBody(&fCStr_vertexShader);

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("colorOut0 = vec4(texture(textureSrc, passUV).r,0.0,0.0,1.0);\r\n");
	fCStr_defaultFragShader.add("}\r\n");

	defaultShader->glProgamId = gxShaderDepr_compileRaw(&fCStr_vertexShader, &fCStr_defaultFragShader);
	catchOpenGLError();

	defaultShader->copyShaderUniforms.uniformLoc_textureSrc = glGetUniformLocation(defaultShader->glProgamId, "textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets = glGetUniformLocation(defaultShader->glProgamId, "uf_vertexOffsets");

	_copyShader_depthToColor = defaultShader;
	catchOpenGLError();
	return defaultShader;
}

LatteDefaultShader_t* LatteDefaultShader_getPixelCopyShader_colorToDepth()
{
	if (_copyShader_colorToDepth != 0)
		return _copyShader_colorToDepth;
	catchOpenGLError();
	LatteDefaultShader_t* defaultShader = (LatteDefaultShader_t*)malloc(sizeof(LatteDefaultShader_t));
	memset(defaultShader, 0, sizeof(LatteDefaultShader_t));

	StringBuf fCStr_vertexShader(1024 * 16);
	LatteDefaultShader_pixelCopyShader_generateVSBody(&fCStr_vertexShader);

	StringBuf fCStr_defaultFragShader(1024 * 16);
	fCStr_defaultFragShader.add("#version 420\r\n");
	fCStr_defaultFragShader.add("in vec2 passUV;\r\n");
	fCStr_defaultFragShader.add("uniform sampler2D textureSrc;\r\n");
	fCStr_defaultFragShader.add("layout(location = 0) out vec4 colorOut0;\r\n");
	fCStr_defaultFragShader.add("\r\n");
	fCStr_defaultFragShader.add("void main(){\r\n");
	fCStr_defaultFragShader.add("gl_FragDepth = texture(textureSrc, passUV).r;\r\n");
	fCStr_defaultFragShader.add("}\r\n");


	defaultShader->glProgamId = gxShaderDepr_compileRaw(&fCStr_vertexShader, &fCStr_defaultFragShader);
	defaultShader->copyShaderUniforms.uniformLoc_textureSrc = glGetUniformLocation(defaultShader->glProgamId, "textureSrc");
	defaultShader->copyShaderUniforms.uniformLoc_vertexOffsets = glGetUniformLocation(defaultShader->glProgamId, "uf_vertexOffsets");

	_copyShader_colorToDepth = defaultShader;
	catchOpenGLError();
	return defaultShader;
}