#include "Cafe/HW/Latte/Renderer/RendererOuputShader.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "HW/Latte/Renderer/Renderer.h"

const std::string RendererOutputShader::s_copy_shader_source =
R"(
void main()
{
	colorOut0 = vec4(texture(textureSrc, passUV).rgb,1.0);
}
)";

const std::string RendererOutputShader::s_copy_shader_source_mtl =
R"(#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float2 uv;
};

fragment float4 main0(VertexOut in [[stage_in]], texture2d<float> textureSrc [[texture(0)]], sampler samplr [[sampler(0)]]) {
	return float4(textureSrc.sample(samplr, in.uv).rgb, 1.0);
}
)";

const std::string RendererOutputShader::s_bicubic_shader_source =
R"(
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

vec4 bcFilter(vec2 uv, vec4 texelSize)
{
	vec2 pixel = uv*texelSize.zw - 0.5;
	vec2 pixelFrac = fract(pixel);
	vec2 pixelInt = pixel - pixelFrac;

	vec4 xcubic = cubic(pixelFrac.x);
	vec4 ycubic = cubic(pixelFrac.y);

	vec4 c = vec4(pixelInt.x - 0.5, pixelInt.x + 1.5, pixelInt.y - 0.5, pixelInt.y + 1.5);
	vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	vec4 sample0 = texture(textureSrc, vec2(offset.x, offset.z) * texelSize.xy);
	vec4 sample1 = texture(textureSrc, vec2(offset.y, offset.z) * texelSize.xy);
	vec4 sample2 = texture(textureSrc, vec2(offset.x, offset.w) * texelSize.xy);
	vec4 sample3 = texture(textureSrc, vec2(offset.y, offset.w) * texelSize.xy);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
		mix(sample3, sample2, sx),
		mix(sample1, sample0, sx), sy);
}

void main(){
	vec4 texelSize = vec4( 1.0 / textureSrcResolution.xy, textureSrcResolution.xy);
	colorOut0 = vec4(bcFilter(passUV, texelSize).rgb,1.0);
}
)";

const std::string RendererOutputShader::s_bicubic_shader_source_mtl =
R"(#include <metal_stdlib>
using namespace metal;

float4 cubic(float x) {
	float x2 = x * x;
	float x3 = x2 * x;
	float4 w;
	w.x = -x3 + 3 * x2 - 3 * x + 1;
	w.y = 3 * x3 - 6 * x2 + 4;
	w.z = -3 * x3 + 3 * x2 + 3 * x + 1;
	w.w = x3;
	return w / 6.0;
}

float4 bcFilter(texture2d<float> textureSrc, sampler samplr, float2 texcoord, float2 texscale) {
	float fx = fract(texcoord.x);
	float fy = fract(texcoord.y);
	texcoord.x -= fx;
	texcoord.y -= fy;

	float4 xcubic = cubic(fx);
	float4 ycubic = cubic(fy);

	float4 c = float4(texcoord.x - 0.5, texcoord.x + 1.5, texcoord.y - 0.5, texcoord.y + 1.5);
	float4 s = float4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	float4 offset = c + float4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	float4 sample0 = textureSrc.sample(samplr, float2(offset.x, offset.z) * texscale);
	float4 sample1 = textureSrc.sample(samplr, float2(offset.y, offset.z) * texscale);
	float4 sample2 = textureSrc.sample(samplr, float2(offset.x, offset.w) * texscale);
	float4 sample3 = textureSrc.sample(samplr, float2(offset.y, offset.w) * texscale);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return mix(
		mix(sample3, sample2, sx),
		mix(sample1, sample0, sx), sy);
}

struct VertexOut {
    float2 uv;
};

fragment float4 main0(VertexOut in [[stage_in]], texture2d<float> textureSrc [[texture(0)]], sampler samplr [[sampler(0)]]) {
    float2 textureSrcResolution = float2(textureSrc.get_width(), textureSrc.get_height());
	return float4(bcFilter(textureSrc, samplr, in.uv * textureSrcResolution, float2(1.0, 1.0) / textureSrcResolution).rgb, 1.0);
}
)";

const std::string RendererOutputShader::s_hermite_shader_source =
R"(
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

	vec4 doubleSize = texelSize*2.0;

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
	vec4 texelSize = vec4( 1.0 / textureSrcResolution.xy, textureSrcResolution.xy);
	colorOut0 = vec4(BicubicHermiteTexture(passUV, texelSize), 1.0);
}
)";

const std::string RendererOutputShader::s_hermite_shader_source_mtl =
R"(#include <metal_stdlib>
using namespace metal;

// https://www.shadertoy.com/view/MllSzX

float3 CubicHermite(float3 A, float3 B, float3 C, float3 D, float t) {
	float t2 = t*t;
    float t3 = t*t*t;
    float3 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    float3 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    float3 c = -A/2.0 + C/2.0;
   	float3 d = B;

    return a*t3 + b*t2 + c*t + d;
}


float3 BicubicHermiteTexture(texture2d<float> textureSrc, sampler samplr, float2 uv, float4 texelSize) {
	float2 pixel = uv*texelSize.zw + 0.5;
	float2 frac = fract(pixel);
    pixel = floor(pixel) / texelSize.zw - float2(texelSize.xy/2.0);

	float4 doubleSize = texelSize*texelSize;

	float3 C00 = textureSrc.sample(samplr, pixel + float2(-texelSize.x ,-texelSize.y)).rgb;
    float3 C10 = textureSrc.sample(samplr, pixel + float2( 0.0        ,-texelSize.y)).rgb;
    float3 C20 = textureSrc.sample(samplr, pixel + float2( texelSize.x ,-texelSize.y)).rgb;
    float3 C30 = textureSrc.sample(samplr, pixel + float2( doubleSize.x,-texelSize.y)).rgb;

    float3 C01 = textureSrc.sample(samplr, pixel + float2(-texelSize.x , 0.0)).rgb;
    float3 C11 = textureSrc.sample(samplr, pixel + float2( 0.0        , 0.0)).rgb;
    float3 C21 = textureSrc.sample(samplr, pixel + float2( texelSize.x , 0.0)).rgb;
    float3 C31 = textureSrc.sample(samplr, pixel + float2( doubleSize.x, 0.0)).rgb;

    float3 C02 = textureSrc.sample(samplr, pixel + float2(-texelSize.x , texelSize.y)).rgb;
    float3 C12 = textureSrc.sample(samplr, pixel + float2( 0.0        , texelSize.y)).rgb;
    float3 C22 = textureSrc.sample(samplr, pixel + float2( texelSize.x , texelSize.y)).rgb;
    float3 C32 = textureSrc.sample(samplr, pixel + float2( doubleSize.x, texelSize.y)).rgb;

    float3 C03 = textureSrc.sample(samplr, pixel + float2(-texelSize.x , doubleSize.y)).rgb;
    float3 C13 = textureSrc.sample(samplr, pixel + float2( 0.0        , doubleSize.y)).rgb;
    float3 C23 = textureSrc.sample(samplr, pixel + float2( texelSize.x , doubleSize.y)).rgb;
    float3 C33 = textureSrc.sample(samplr, pixel + float2( doubleSize.x, doubleSize.y)).rgb;

    float3 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    float3 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    float3 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    float3 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);

    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

fragment float4 main0(VertexOut in [[stage_in]], texture2d<float> textureSrc [[texture(0)]], sampler samplr [[sampler(0)]], constant float2& outputResolution [[buffer(0)]]) {
	float4 texelSize = float4(1.0 / outputResolution.xy, outputResolution.xy);
	return float4(BicubicHermiteTexture(textureSrc, samplr, in.uv, texelSize), 1.0);
}
)";

RendererOutputShader::RendererOutputShader(const std::string& vertex_source, const std::string& fragment_source)
{
    std::string finalFragmentSrc;
    if (g_renderer->GetType() == RendererAPI::Metal)
        finalFragmentSrc = fragment_source;
    else
        finalFragmentSrc = PrependFragmentPreamble(fragment_source);

	m_vertex_shader = g_renderer->shader_create(RendererShader::ShaderType::kVertex, 0, 0, vertex_source, false, false);
	m_fragment_shader = g_renderer->shader_create(RendererShader::ShaderType::kFragment, 0, 0, finalFragmentSrc, false, false);

	m_vertex_shader->PreponeCompilation(true);
	m_fragment_shader->PreponeCompilation(true);

	if (!m_vertex_shader->WaitForCompiled())
		throw std::exception();

	if(!m_fragment_shader->WaitForCompiled())
		throw std::exception();

	if (g_renderer->GetType() == RendererAPI::OpenGL)
	{
		m_uniformLocations[0].m_loc_textureSrcResolution = m_vertex_shader->GetUniformLocation("textureSrcResolution");
		m_uniformLocations[0].m_loc_nativeResolution = m_vertex_shader->GetUniformLocation("nativeResolution");
		m_uniformLocations[0].m_loc_outputResolution = m_vertex_shader->GetUniformLocation("outputResolution");

		m_uniformLocations[1].m_loc_textureSrcResolution = m_fragment_shader->GetUniformLocation("textureSrcResolution");
		m_uniformLocations[1].m_loc_nativeResolution = m_fragment_shader->GetUniformLocation("nativeResolution");
		m_uniformLocations[1].m_loc_outputResolution = m_fragment_shader->GetUniformLocation("outputResolution");
	}
}

void RendererOutputShader::SetUniformParameters(const LatteTextureView& texture_view, const Vector2i& output_res) const
{
	sint32 effectiveWidth, effectiveHeight;
	texture_view.baseTexture->GetEffectiveSize(effectiveWidth, effectiveHeight, 0);
	auto setUniforms = [&](RendererShader* shader, const UniformLocations& locations){
	  float res[2];
	  if (locations.m_loc_textureSrcResolution != -1)
	  {
		  res[0] = (float)effectiveWidth;
		  res[1] = (float)effectiveHeight;
		  shader->SetUniform2fv(locations.m_loc_textureSrcResolution, res, 1);
	  }

	  if (locations.m_loc_nativeResolution != -1)
	  {
		  res[0] = (float)texture_view.baseTexture->width;
		  res[1] = (float)texture_view.baseTexture->height;
		  shader->SetUniform2fv(locations.m_loc_nativeResolution, res, 1);
	  }

	  if (locations.m_loc_outputResolution != -1)
	  {
		  res[0] = (float)output_res.x;
		  res[1] = (float)output_res.y;
		  shader->SetUniform2fv(locations.m_loc_outputResolution, res, 1);
	  }
	};
	setUniforms(m_vertex_shader, m_uniformLocations[0]);
	setUniforms(m_fragment_shader, m_uniformLocations[1]);
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

std::string RendererOutputShader::GetMetalVertexSource(bool render_upside_down)
{
	// vertex shader
	std::ostringstream vertex_source;
		vertex_source <<
			R"(#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut main0(ushort vid [[vertex_id]]) {
	VertexOut out;
	float2 pos;
	if (vid == 0) pos = float2(-1.0, -3.0);
	else if (vid == 1) pos = float2(-1.0, 1.0);
	else if (vid == 2) pos = float2(3.0, 1.0);
	out.uv = pos * 0.5 + 0.5;
	out.uv.y = 1.0 - out.uv.y;
)";

		if (render_upside_down)
		{
			vertex_source <<
				R"(	pos.y = -pos.y;
	)";
		}

		vertex_source <<
			R"(	out.position = float4(pos, 0.0, 1.0);
	return out;
}
)";
		return vertex_source.str();
}

std::string RendererOutputShader::PrependFragmentPreamble(const std::string& shaderSrc)
{
	return R"(#version 430
#ifdef VULKAN
layout(push_constant) uniform pc {
	vec2 textureSrcResolution;
	vec2 nativeResolution;
	vec2 outputResolution;
};
#else
uniform vec2 textureSrcResolution;
uniform vec2 nativeResolution;
uniform vec2 outputResolution;
#endif

layout(location = 0) in vec2 passUV;
layout(binding = 0) uniform sampler2D textureSrc;
layout(location = 0) out vec4 colorOut0;
)" + shaderSrc;
}
void RendererOutputShader::InitializeStatic()
{
    if (g_renderer->GetType() == RendererAPI::Metal)
    {
        std::string vertex_source = GetMetalVertexSource(false);
        std::string vertex_source_ud = GetMetalVertexSource(true);

       	s_copy_shader = new RendererOutputShader(vertex_source, s_copy_shader_source_mtl);
       	s_copy_shader_ud = new RendererOutputShader(vertex_source_ud, s_copy_shader_source_mtl);

       	s_bicubic_shader = new RendererOutputShader(vertex_source, s_bicubic_shader_source_mtl);
       	s_bicubic_shader_ud = new RendererOutputShader(vertex_source_ud, s_bicubic_shader_source_mtl);

       	s_hermit_shader = new RendererOutputShader(vertex_source, s_hermite_shader_source_mtl);
       	s_hermit_shader_ud = new RendererOutputShader(vertex_source_ud, s_hermite_shader_source_mtl);
    }
    else
    {
    	std::string vertex_source, vertex_source_ud;
    	// vertex shader
    	if (g_renderer->GetType() == RendererAPI::OpenGL)
    	{
    		vertex_source = GetOpenGlVertexSource(false);
    		vertex_source_ud = GetOpenGlVertexSource(true);
    	}
    	else if (g_renderer->GetType() == RendererAPI::Vulkan)
    	{
    		vertex_source = GetVulkanVertexSource(false);
    		vertex_source_ud = GetVulkanVertexSource(true);
    	}
    	s_copy_shader = new RendererOutputShader(vertex_source, s_copy_shader_source);
    	s_copy_shader_ud = new RendererOutputShader(vertex_source_ud, s_copy_shader_source);

    	s_bicubic_shader = new RendererOutputShader(vertex_source, s_bicubic_shader_source);
    	s_bicubic_shader_ud = new RendererOutputShader(vertex_source_ud, s_bicubic_shader_source);

    	s_hermit_shader = new RendererOutputShader(vertex_source, s_hermite_shader_source);
    	s_hermit_shader_ud = new RendererOutputShader(vertex_source_ud, s_hermite_shader_source);
    }
}
