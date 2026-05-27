#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float3 position;
  float3 normal;
  float2 tex_coord;
};

struct Uniforms
{
  float4x4 mvp_matrix;
  float4x4 model_matrix;
};

struct RasterizerData
{
  float4 position [[position]];
  float3 world_normal;
  float2 tex_coord;
};

vertex RasterizerData vertex_main(uint vertexID [[vertex_id]],
                                  constant VertexIn *vertices [[buffer(0)]],
                                  constant Uniforms &uniforms [[buffer(1)]])
{
  RasterizerData out;
  out.position = uniforms.mvp_matrix * float4(vertices[vertexID].position, 1.0);
  
  // Rotate the normal using the model matrix (ignoring translation)
  float3x3 normal_matrix = float3x3(uniforms.model_matrix[0].xyz,
                                    uniforms.model_matrix[1].xyz,
                                    uniforms.model_matrix[2].xyz);
  out.world_normal = normal_matrix * vertices[vertexID].normal;
  out.tex_coord = vertices[vertexID].tex_coord;
  
  return out;
}

fragment float4 fragment_main(RasterizerData in [[stage_in]],
                              texture2d<float> colorTexture [[texture(0)]])
{
  constexpr sampler textureSampler (mag_filter::linear, min_filter::linear);
  float4 base_color = colorTexture.sample(textureSampler, in.tex_coord);
  
  // Simple Directional Lighting
  float3 light_dir = normalize(float3(1.0, 1.0, -1.0));
  float3 normal = normalize(in.world_normal);
  
  float diffuse = max(dot(normal, light_dir), 0.0);
  float ambient = 0.2;
  
  float light_intensity = diffuse + ambient;
  return float4(base_color.rgb * light_intensity, base_color.a);
}
