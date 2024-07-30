inline const char* presentLibrarySource = \
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
"vertex VertexOut presentVertex(ushort vid [[vertex_id]]) {\n" \
"   VertexOut out;\n" \
"   out.position = float4(positions[vid], 0.0, 1.0);\n" \
"   out.texCoord = positions[vid] * 0.5 + 0.5;\n" \
"\n" \
"   return out;\n" \
"}\n" \
"\n" \
"fragment float4 presentFragment(VertexOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samplr [[sampler(0)]]) {\n" \
"   return tex.sample(samplr, in.texCoord);\n" \
"}\n";
