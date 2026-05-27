#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float2 position;
  float4 color;
};

struct RasterizerData
{
  float4 position [[position]];
  float4 color;
};

vertex RasterizerData vertex_main(uint vertexID [[vertex_id]],
                                  constant VertexIn *vertices [[buffer(0)]])
{
  RasterizerData out;
  out.position = float4(vertices[vertexID].position, 0.0, 1.0);
  out.color = vertices[vertexID].color;
  return out;
}

fragment float4 fragment_main(RasterizerData in [[stage_in]])
{
  return in.color;
}
