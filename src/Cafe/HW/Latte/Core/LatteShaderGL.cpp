#include "Common/GLInclude/GLInclude.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Core/LatteDraw.h"
#include "Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.h"
#include "util/helpers/StringBuf.h"

bool gxShader_checkIfSuccessfullyLinked(GLuint glProgram)
{
	int status = -1;
	glGetProgramiv(glProgram, GL_LINK_STATUS, &status);
	if( status == GL_TRUE )
		return true;
	// in debug mode, get and print shader error log
	char infoLog[48*1024];
	uint32 infoLogLength, tempLength;
	glGetProgramiv(glProgram, GL_INFO_LOG_LENGTH, (GLint *)&infoLogLength);
	tempLength = sizeof(infoLog)-1;
	glGetProgramInfoLog(glProgram, std::min(tempLength, infoLogLength), (GLsizei*)&tempLength, (GLcharARB*)infoLog);
	infoLog[tempLength] = '\0';
	cemuLog_log(LogType::Force, "Link error in raw shader");
	cemuLog_log(LogType::Force, infoLog);
	return false;
}

void LatteShader_prepareSeparableUniforms(LatteDecompilerShader* shader)
{
	if (g_renderer->GetType() == RendererAPI::Vulkan)
		return;

	auto shaderGL = (RendererShaderGL*)shader->shader;
	// setup uniform info
	if (shader->shaderType == LatteConst::ShaderType::Vertex)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedVS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterVS");
	}
	else if (shader->shaderType == LatteConst::ShaderType::Geometry)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedGS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterGS");
	}
	else if (shader->shaderType == LatteConst::ShaderType::Pixel)
	{
		shader->uniform.loc_remapped = glGetUniformLocation(shaderGL->GetProgram(), "uf_remappedPS");
		shader->uniform.loc_uniformRegister = glGetUniformLocation(shaderGL->GetProgram(), "uf_uniformRegisterPS");
	}
	catchOpenGLError();
	shader->uniform.loc_windowSpaceToClipSpaceTransform = glGetUniformLocation(shaderGL->GetProgram(), "uf_windowSpaceToClipSpaceTransform");
	shader->uniform.loc_alphaTestRef = glGetUniformLocation(shaderGL->GetProgram(), "uf_alphaTestRef");
	shader->uniform.loc_pointSize = glGetUniformLocation(shaderGL->GetProgram(), "uf_pointSize");
	shader->uniform.loc_fragCoordScale = glGetUniformLocation(shaderGL->GetProgram(), "uf_fragCoordScale");
	cemu_assert_debug(shader->uniform.list_ufTexRescale.empty());
	for (sint32 t = 0; t < LATTE_NUM_MAX_TEX_UNITS; t++)
	{
		char ufName[64];
		sprintf(ufName, "uf_tex%dScale", t);
		GLint uniformLocation = glGetUniformLocation(shaderGL->GetProgram(), ufName);
		if (uniformLocation >= 0)
		{
			LatteUniformTextureScaleEntry_t entry = { 0 };
			entry.texUnit = t;
			entry.uniformLocation = uniformLocation;
			shader->uniform.list_ufTexRescale.push_back(entry);
		}
	}
}
GLuint gpu7ShaderGLDepr_compileShader(const std::string& source, uint32_t type)
{
	cemu_assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
	const GLuint shader_object = glCreateShader(type);

	const char *c_str = source.c_str();
	const GLint size = (GLint)source.size();
	glShaderSource(shader_object, 1, &c_str, &size);
	glCompileShader(shader_object);

	GLint log_length;
	glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0)
	{
		char log[2048]{};
		GLsizei log_size;
		glGetShaderInfoLog(shader_object, std::min(log_length, (GLint)sizeof(log) - 1), &log_size, log);
		cemuLog_log(LogType::Force, "Error/Warning in vertex shader:");
		cemuLog_log(LogType::Force, log);
	}

	return shader_object;
}
GLuint gpu7ShaderGLDepr_compileVertexShader(const std::string& source)
{
	return gpu7ShaderGLDepr_compileShader(source, GL_VERTEX_SHADER);
}

GLuint gpu7ShaderGLDepr_compileFragmentShader(const std::string& source)
{
	return gpu7ShaderGLDepr_compileShader(source, GL_FRAGMENT_SHADER);
}

GLuint gpu7ShaderGLDepr_compileVertexShader(const char* shaderSource, sint32 shaderSourceLength)
{
	uint32 shaderObject = glCreateShader(GL_VERTEX_SHADER);
	GLchar* srcPtr = (GLchar*)shaderSource;
	GLint srcLen = shaderSourceLength;
	glShaderSource(shaderObject, 1, &srcPtr, &srcLen);
	glCompileShader(shaderObject);
	uint32 shaderLogLengthInfo, shaderLogLen;
	glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, (GLint *)&shaderLogLengthInfo);
	if (shaderLogLengthInfo > 0)
	{
		char messageLog[2048]{};
		glGetShaderInfoLog(shaderObject, std::min<uint32>(shaderLogLengthInfo, sizeof(messageLog) - 1), (GLsizei*)&shaderLogLen, (GLcharARB*)messageLog);
		cemuLog_log(LogType::Force, "Error/Warning in vertex shader:");
		cemuLog_log(LogType::Force, messageLog);
	}
	return shaderObject;
}

GLuint gpu7ShaderGLDepr_compileFragmentShader(const char* shaderSource, sint32 shaderSourceLength)
{
	uint32 shaderObject = glCreateShader(GL_FRAGMENT_SHADER);
	GLchar* srcPtr = (GLchar*)shaderSource;
	GLint srcLen = shaderSourceLength;
	glShaderSource(shaderObject, 1, &srcPtr, &srcLen);
	glCompileShader(shaderObject);
	uint32 shaderLogLengthInfo, shaderLogLen;
	char messageLog[2048];
	glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, (GLint *)&shaderLogLengthInfo);
	if (shaderLogLengthInfo > 0)
	{
		memset(messageLog, 0, sizeof(messageLog));
		glGetShaderInfoLog(shaderObject, std::min<uint32>(shaderLogLengthInfo, sizeof(messageLog) - 1), (GLsizei*)&shaderLogLen, (GLcharARB*)messageLog);
		cemuLog_log(LogType::Force, "Error/Warning in fragment shader:");
		cemuLog_log(LogType::Force, messageLog);
	}
	return shaderObject;
}

GLuint gxShaderDepr_compileRaw(StringBuf* strSourceVS, StringBuf* strSourceFS)
{
	GLuint glShaderProgram = glCreateProgram();
	GLuint vertexShader = gpu7ShaderGLDepr_compileVertexShader(strSourceVS->c_str(), strSourceVS->getLen());
	glAttachShader(glShaderProgram, vertexShader);
	GLuint fragmentShader = gpu7ShaderGLDepr_compileFragmentShader(strSourceFS->c_str(), strSourceFS->getLen());
	glAttachShader(glShaderProgram, fragmentShader);
	glLinkProgram(glShaderProgram);
	if( gxShader_checkIfSuccessfullyLinked(glShaderProgram) == false )
	{
		return 0;
	}
	return glShaderProgram;
}

GLuint gxShaderDepr_compileRaw(const std::string& vertex_source, const std::string& fragment_source)
{
	const GLuint programm = glCreateProgram();

	auto vertex_shader = std::async(std::launch::deferred, gpu7ShaderGLDepr_compileShader, vertex_source, GL_VERTEX_SHADER);
	auto fragment_shader = std::async(std::launch::deferred, gpu7ShaderGLDepr_compileShader, fragment_source, GL_FRAGMENT_SHADER);

	glAttachShader(programm, vertex_shader.get());
	glAttachShader(programm, fragment_shader.get());

	glLinkProgram(programm);
	return gxShader_checkIfSuccessfullyLinked(programm) ? programm : 0;
}
