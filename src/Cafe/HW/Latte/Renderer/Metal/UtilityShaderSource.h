#pragma once

#define __STRINGIFY(x) #x
#define _STRINGIFY(x) __STRINGIFY(x)

constexpr const char* utilityShaderSource = _STRINGIFY((
constant float2 positions[] = {float2(-1.0, -3.0), float2(-1.0, 1.0), float2(3.0, 1.0)};

struct VertexOut {
   float4 position [[position]];
   float2 texCoord;
};

vertex VertexOut vertexFullscreen(ushort vid [[vertex_id]]) {
   VertexOut out;
   out.position = float4(positions[vid], 0.0, 1.0);
   out.texCoord = positions[vid] * 0.5 + 0.5;
   out.texCoord.y = 1.0 - out.texCoord.y;

   return out;
}

fragment float4 fragmentPresent(VertexOut in [[stage_in]], texture2d<float> tex [[texture(GET_TEXTURE_BINDING(0))]], sampler samplr [[sampler(GET_SAMPLER_BINDING(0))]]) {
   return tex.sample(samplr, in.texCoord);
}

struct CopyParams {
   uint width;
   uint srcMip;
   uint srcSlice;
   uint dstMip;
   uint dstSlice;
};

vertex void vertexCopyTextureToTexture(uint vid [[vertex_id]], texture2d_array<float, access::read> src [[texture(GET_TEXTURE_BINDING(0))]], texture2d_array<float, access::write> dst [[texture(GET_TEXTURE_BINDING(1))]], constant CopyParams& params [[buffer(GET_BUFFER_BINDING(0))]]) {
   uint2 coord = uint2(vid % params.width, vid / params.width);
   return dst.write(float4(src.read(coord, params.srcSlice, params.srcMip).r, 0.0, 0.0, 0.0), coord, params.dstSlice, params.dstMip);
}

struct RestrideParams {
   uint oldStride;
   uint newStride;
};

/* TODO: use uint32? Since that would require less iterations */
vertex void vertexRestrideBuffer(uint vid [[vertex_id]], device uint8_t* src [[buffer(GET_BUFFER_BINDING(0))]], device uint8_t* dst [[buffer(GET_BUFFER_BINDING(1))]], constant RestrideParams& params [[buffer(GET_BUFFER_BINDING(2))]]) {
   for (uint32_t i = 0; i < params.oldStride; i++) {
       dst[vid * params.newStride + i] = src[vid * params.oldStride + i];
   }
}
));
