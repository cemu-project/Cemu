inline const char* utilityShaderSource = \
"#include <metal_stdlib>\n" \
"using namespace metal;\n" \
"\n" \
"constant float2 positions[] = {float2(-1.0, -3.0), float2(-1.0, 1.0), float2(3.0, 1.0)};\n"
"\n" \
"struct VertexOut {\n" \
"   float4 position [[position]];\n" \
"   float2 texCoord;\n" \
"};\n" \
"\n" \
"vertex VertexOut vertexFullscreen(ushort vid [[vertex_id]]) {\n" \
"   VertexOut out;\n" \
"   out.position = float4(positions[vid], 0.0, 1.0);\n" \
"   out.texCoord = positions[vid] * 0.5 + 0.5;\n" \
"   out.texCoord.y = 1.0 - out.texCoord.y;\n" \
"\n" \
"   return out;\n" \
"}\n" \
"\n" \
"fragment float4 fragmentPresent(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {\n" \
"   return tex.sample(samplr, in.texCoord);\n" \
"}\n" \
"\n" \
"struct CopyParams {\n" \
"   uint width;\n" \
"   uint srcMip;\n" \
"   uint srcSlice;\n" \
"   uint dstMip;\n" \
"   uint dstSlice;\n" \
"};\n" \
"\n" \
"vertex void vertexCopyTextureToTexture(uint vid [[vertex_id]], texture2d_array<float, access::read> src [[texture(0)]], texture2d_array<float, access::write> dst [[texture(1)]], constant CopyParams& params) {\n" \
"   uint2 coord = uint2(vid % params.width, vid / params.width);\n" \
"   return dst.write(float4(src.read(coord, params.srcSlice, params.srcMip).r, 0.0, 0.0, 0.0), coord, params.dstSlice, params.dstMip);\n" \
"}\n" \
"\n";
