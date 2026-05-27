#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float3 position;
  float4 color;
};

struct Uniforms
{
  float4x4 mvp_matrix;
};

struct RasterizerData
{
  float4 position [[position]];
  float4 color;
};

vertex RasterizerData vertex_main(uint vertexID [[vertex_id]],
                                  constant VertexIn *vertices [[buffer(0)]],
                                  constant Uniforms &uniforms [[buffer(1)]])
{
  RasterizerData out;
  out.position = uniforms.mvp_matrix * float4(vertices[vertexID].position, 1.0);
  out.color = vertices[vertexID].color;
  return out;
}

fragment float4 fragment_main(RasterizerData in [[stage_in]])
{
  return in.color;
}
