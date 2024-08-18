#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"

const std::string RendererOutputShader::s_copy_shader_source =
R"(#version 420

#ifdef VULKAN
layout(location = 0) in vec2 passUV;
layout(binding = 0) uniform sampler2D textureSrc;
layout(location = 0) out vec4 colorOut0;
#else
in vec2 passUV;
layout(binding=0) uniform sampler2D textureSrc;
layout(location = 0) out vec4 colorOut0;
#endif

void main()
{
	colorOut0 = vec4(texture(textureSrc, passUV).rgb,1.0);
}
)";

const std::string RendererOutputShader::s_bicubic_shader_source =
R"(
#version 420

#ifdef VULKAN
layout(location = 0) in vec2 passUV;
layout(binding = 0)  uniform sampler2D textureSrc;
layout(binding = 1)  uniform vec2 textureSrcResolution;
layout(location = 0) out vec4 colorOut0;
#else
in vec2 passUV;
layout(binding=0) uniform sampler2D textureSrc;
uniform vec2 textureSrcResolution;
layout(location = 0) out vec4 colorOut0;
#endif

vec4 cubic(float x)
{
	float x2 = x * x;
	float x3 = x2 * x;
	vec4 w;
	w.x = -x3 + 3 * x2 - 3 * x + 1;
	w.y = 3 * x3 - 6 * x2 + 4;
	w.z = -3 * x3 + 3 * x2 + 3 * x + 1;
	w.w = x3;
	return w / 6.0;
}

vec4 bcFilter(vec2 texcoord, vec2 texscale)
{
	float fx = fract(texcoord.x);
	float fy = fract(texcoord.y);
	texcoord.x -= fx;
	texcoord.y -= fy;

	vec4 xcubic = cubic(fx);
	vec4 ycubic = cubic(fy);

	vec4 c = vec4(texcoord.x - 0.5, texcoord.x + 1.5, texcoord.y - 0.5, texcoord.y + 1.5);
	vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	vec4 sample0 = texture(textureSrc, vec2(offset.x, offset.z) * texscale);
	vec4 sample1 = texture(textureSrc, vec2(offset.y, offset.z) * texscale);
	vec4 sample2 = texture(textureSrc, vec2(offset.x, offset.w) * texscale);
	vec4 sample3 = texture(textureSrc, vec2(offset.y, offset.w) * texscale);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
		mix(sample3, sample2, sx),
		mix(sample1, sample0, sx), sy);
}

void main(){
	colorOut0 = vec4(bcFilter(passUV*textureSrcResolution, vec2(1.0,1.0)/textureSrcResolution).rgb,1.0);
}
)";

const std::string RendererOutputShader::s_hermite_shader_source =
R"(#version 420

in vec4 gl_FragCoord;	
in vec2 passUV;
layout(binding=0) uniform sampler2D textureSrc;
uniform vec2 textureSrcResolution;
uniform vec2 outputResolution;
layout(location = 0) out vec4 colorOut0;

// https://www.shadertoy.com/view/MllSzX

vec3 CubicHermite (vec3 A, vec3 B, vec3 C, vec3 D, float t)
{
	float t2 = t*t;
    float t3 = t*t*t;
    vec3 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    vec3 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    vec3 c = -A/2.0 + C/2.0;
   	vec3 d = B;
    
    return a*t3 + b*t2 + c*t + d;
}


vec3 BicubicHermiteTexture(vec2 uv, vec4 texelSize)
{
	vec2 pixel = uv*texelSize.zw + 0.5;
	vec2 frac = fract(pixel);	
    pixel = floor(pixel) / texelSize.zw - vec2(texelSize.xy/2.0);
	
	vec4 doubleSize = texelSize*texelSize;

	vec3 C00 = texture(textureSrc, pixel + vec2(-texelSize.x ,-texelSize.y)).rgb;
    vec3 C10 = texture(textureSrc, pixel + vec2( 0.0        ,-texelSize.y)).rgb;
    vec3 C20 = texture(textureSrc, pixel + vec2( texelSize.x ,-texelSize.y)).rgb;
    vec3 C30 = texture(textureSrc, pixel + vec2( doubleSize.x,-texelSize.y)).rgb;
    
    vec3 C01 = texture(textureSrc, pixel + vec2(-texelSize.x , 0.0)).rgb;
    vec3 C11 = texture(textureSrc, pixel + vec2( 0.0        , 0.0)).rgb;
    vec3 C21 = texture(textureSrc, pixel + vec2( texelSize.x , 0.0)).rgb;
    vec3 C31 = texture(textureSrc, pixel + vec2( doubleSize.x, 0.0)).rgb;    
    
    vec3 C02 = texture(textureSrc, pixel + vec2(-texelSize.x , texelSize.y)).rgb;
    vec3 C12 = texture(textureSrc, pixel + vec2( 0.0        , texelSize.y)).rgb;
    vec3 C22 = texture(textureSrc, pixel + vec2( texelSize.x , texelSize.y)).rgb;
    vec3 C32 = texture(textureSrc, pixel + vec2( doubleSize.x, texelSize.y)).rgb;    
    
    vec3 C03 = texture(textureSrc, pixel + vec2(-texelSize.x , doubleSize.y)).rgb;
    vec3 C13 = texture(textureSrc, pixel + vec2( 0.0        , doubleSize.y)).rgb;
    vec3 C23 = texture(textureSrc, pixel + vec2( texelSize.x , doubleSize.y)).rgb;
    vec3 C33 = texture(textureSrc, pixel + vec2( doubleSize.x, doubleSize.y)).rgb;    
    
    vec3 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    vec3 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    vec3 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    vec3 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);
    
    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

void main(){
	vec4 texelSize = vec4( 1.0 / outputResolution.xy, outputResolution.xy);
	colorOut0 = vec4(BicubicHermiteTexture(passUV, texelSize), 1.0);
}
)";

RendererOutputShader::RendererOutputShader(const std::string& vertex_source, const std::string& fragment_source)
{
	m_vertex_shader = g_renderer->shader_create(RendererShader::ShaderType::kVertex, 0, 0, vertex_source, false, false);
	m_fragment_shader = g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, fragment_source, false, false);

	m_vertex_shader->PreponeCompilation(true);
	m_fragment_shader->PreponeCompilation(true);

	if (!m_vertex_shader->WaitForCompiled())
		throw std::exception();

	if(!m_fragment_shader->WaitForCompiled())
		throw std::exception();

	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		m_attributes[0].m_loc_texture_src_resolution = m_vertex_shader->GetUniformLocation("textureSrcResolution");
		m_attributes[0].m_loc_input_resolution = m_vertex_shader->GetUniformLocation("inputResolution");
		m_attributes[0].m_loc_output_resolution = m_vertex_shader->GetUniformLocation("outputResolution");

		m_attributes[1].m_loc_texture_src_resolution = m_fragment_shader->GetUniformLocation("textureSrcResolution");
		m_attributes[1].m_loc_input_resolution = m_fragment_shader->GetUniformLocation("inputResolution");
		m_attributes[1].m_loc_output_resolution = m_fragment_shader->GetUniformLocation("outputResolution");
	}
	else
	{
		cemuLog_logDebug(LogType::Force, "RendererOutputShader() - todo for Vulkan");
		m_attributes[0].m_loc_texture_src_resolution = -1;
		m_attributes[0].m_loc_input_resolution = -1;
		m_attributes[0].m_loc_output_resolution = -1;

		m_attributes[1].m_loc_texture_src_resolution = -1;
		m_attributes[1].m_loc_input_resolution = -1;
		m_attributes[1].m_loc_output_resolution = -1;
	}

}

void RendererOutputShader::SetUniformParameters(const LatteTextureView& texture_view, const Vector2i& input_res, const Vector2i& output_res) const
{
	float res[2];
	// vertex shader
	if (m_attributes[0].m_loc_texture_src_resolution != -1)
	{ 
		res[0] = (float)texture_view.baseTexture->width;
		res[1] = (float)texture_view.baseTexture->height;
		m_vertex_shader->SetUniform2fv(m_attributes[0].m_loc_texture_src_resolution, res, 1);
	}

	if (m_attributes[0].m_loc_input_resolution != -1)
	{
		res[0] = (float)input_res.x;
		res[1] = (float)input_res.y;
		m_vertex_shader->SetUniform2fv(m_attributes[0].m_loc_input_resolution, res, 1);
	}

	if (m_attributes[0].m_loc_output_resolution != -1)
	{
		res[0] = (float)output_res.x;
		res[1] = (float)output_res.y;
		m_vertex_shader->SetUniform2fv(m_attributes[0].m_loc_output_resolution, res, 1);
	}

	// fragment shader
	if (m_attributes[1].m_loc_texture_src_resolution != -1)
	{
		res[0] = (float)texture_view.baseTexture->width;
		res[1] = (float)texture_view.baseTexture->height;
		m_fragment_shader->SetUniform2fv(m_attributes[1].m_loc_texture_src_resolution, res, 1);
	}

	if (m_attributes[1].m_loc_input_resolution != -1)
	{
		res[0] = (float)input_res.x;
		res[1] = (float)input_res.y;
		m_fragment_shader->SetUniform2fv(m_attributes[1].m_loc_input_resolution, res, 1);
	}

	if (m_attributes[1].m_loc_output_resolution != -1)
	{
		res[0] = (float)output_res.x;
		res[1] = (float)output_res.y;
		m_fragment_shader->SetUniform2fv(m_attributes[1].m_loc_output_resolution, res, 1);
	}
}

RendererOutputShader* RendererOutputShader::s_copy_shader;
RendererOutputShader* RendererOutputShader::s_copy_shader_ud;

RendererOutputShader* RendererOutputShader::s_bicubic_shader;
RendererOutputShader* RendererOutputShader::s_bicubic_shader_ud;

RendererOutputShader* RendererOutputShader::s_hermit_shader;
RendererOutputShader* RendererOutputShader::s_hermit_shader_ud;

std::string RendererOutputShader::GetOpenGlVertexSource(bool render_upside_down)
{
	// vertex shader
	std::ostringstream vertex_source;
		vertex_source <<
			R"(#version 400
out vec2 passUV;

out gl_PerVertex 
{ 
   vec4 gl_Position; 
};

void main(){
	vec2 vPos;
	vec2 vUV;
	int vID = gl_VertexID;
)";

		if (render_upside_down)
		{
			vertex_source <<
				R"(	if( vID == 0 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,0.0); }
	else if( vID == 1 ) { vPos = vec2(-1.0,1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 2 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 3 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 4 ) { vPos = vec2(1.0,-1.0); vUV = vec2(1.0,1.0); }
	else if( vID == 5 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,0.0); }
	)";
		}
		else
		{
			vertex_source <<
				R"(	if( vID == 0 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,1.0); }
	else if( vID == 1 ) { vPos = vec2(-1.0,1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 2 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 3 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 4 ) { vPos = vec2(1.0,-1.0); vUV = vec2(1.0,0.0); }
	else if( vID == 5 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,1.0); }
	)";
		}

		vertex_source <<
			R"(	passUV = vUV;
	gl_Position = vec4(vPos, 0.0, 1.0);	
}
)";
		return vertex_source.str();
}

std::string RendererOutputShader::GetVulkanVertexSource(bool render_upside_down)
{
	// vertex shader
	std::ostringstream vertex_source;
		vertex_source <<
			R"(#version 450
layout(location = 0) out vec2 passUV;

out gl_PerVertex 
{ 
   vec4 gl_Position; 
};

void main(){
	vec2 vPos;
	vec2 vUV;
	int vID = gl_VertexIndex;
)";

		if (render_upside_down)
		{
			vertex_source <<
				R"(	if( vID == 0 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,0.0); }
	else if( vID == 1 ) { vPos = vec2(-1.0,1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 2 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 3 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 4 ) { vPos = vec2(1.0,-1.0); vUV = vec2(1.0,1.0); }
	else if( vID == 5 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,0.0); }
	)";
		}
		else
		{
			vertex_source <<
				R"(	if( vID == 0 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,1.0); }
	else if( vID == 1 ) { vPos = vec2(-1.0,1.0); vUV = vec2(0.0,1.0); }
	else if( vID == 2 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 3 ) { vPos = vec2(-1.0,-1.0); vUV = vec2(0.0,0.0); }
	else if( vID == 4 ) { vPos = vec2(1.0,-1.0); vUV = vec2(1.0,0.0); }
	else if( vID == 5 ) { vPos = vec2(1.0,1.0); vUV = vec2(1.0,1.0); }
	)";
		}

		vertex_source <<
			R"(	passUV = vUV;
	gl_Position = vec4(vPos, 0.0, 1.0);	
}
)";
		return vertex_source.str();
}
void RendererOutputShader::InitializeStatic()
{
	std::string vertex_source, vertex_source_ud;
	// vertex shader
	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		vertex_source = GetOpenGlVertexSource(false);
		vertex_source_ud = GetOpenGlVertexSource(true);

		s_copy_shader = new RendererOutputShader(vertex_source, s_copy_shader_source);
		s_copy_shader_ud = new RendererOutputShader(vertex_source_ud, s_copy_shader_source);

		s_bicubic_shader = new RendererOutputShader(vertex_source, s_bicubic_shader_source);
		s_bicubic_shader_ud = new RendererOutputShader(vertex_source_ud, s_bicubic_shader_source);

		s_hermit_shader = new RendererOutputShader(vertex_source, s_hermite_shader_source);
		s_hermit_shader_ud = new RendererOutputShader(vertex_source_ud, s_hermite_shader_source);
	}
	else
	{
		vertex_source = GetVulkanVertexSource(false);
		vertex_source_ud = GetVulkanVertexSource(true);

		s_copy_shader = new RendererOutputShader(vertex_source, s_copy_shader_source);
		s_copy_shader_ud = new RendererOutputShader(vertex_source_ud, s_copy_shader_source);

	/*	s_bicubic_shader = new RendererOutputShader(vertex_source, s_bicubic_shader_source); TODO
		s_bicubic_shader_ud = new RendererOutputShader(vertex_source_ud, s_bicubic_shader_source);

		s_hermit_shader = new RendererOutputShader(vertex_source, s_hermite_shader_source);
		s_hermit_shader_ud = new RendererOutputShader(vertex_source_ud, s_hermite_shader_source);*/
	}
}
