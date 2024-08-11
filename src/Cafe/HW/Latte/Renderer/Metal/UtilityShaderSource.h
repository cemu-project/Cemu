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
"vertex void vertexCopyDepthToColor(uint vid [[vertex_id]], depth2d<float, access::read> src [[texture(0)]], texture2d<float, access::write> dst [[texture(1)]], constant uint& copyWidth) {\n" \
"   uint2 coord = uint2(vid % copyWidth, vid / copyWidth);\n" \
"   return dst.write(float4(src.read(coord), 0.0, 0.0, 0.0), coord);\n" \
"}\n" \
"\n" \
"vertex void vertexCopyColorToDepth(uint vid [[vertex_id]], texture2d<float, access::read> src [[texture(0)]], texture2d<float, access::write> dst [[texture(1)]], constant uint& copyWidth) {\n" \
"   uint2 coord = uint2(vid % copyWidth, vid / copyWidth);\n" \
"   return dst.write(float4(src.read(coord).r), coord);\n" \
"}\n" \
"\n";
