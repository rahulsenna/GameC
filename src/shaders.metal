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
  float3 light_dir;
  float3 light_color;
  float3 camera_pos;
  float ambient_intensity;
};

struct RasterizerData
{
  float4 position [[position]];
  float3 world_position;
  float3 world_normal;
  float2 tex_coord;
};

vertex RasterizerData vertex_main(uint vertexID [[vertex_id]],
                                  constant VertexIn *vertices [[buffer(0)]],
                                  constant Uniforms &uniforms [[buffer(1)]])
{
  RasterizerData out;
  out.position = uniforms.mvp_matrix * float4(vertices[vertexID].position, 1.0);
  out.world_position = (uniforms.model_matrix * float4(vertices[vertexID].position, 1.0)).xyz;
  
  // Rotate the normal using the model matrix (ignoring translation)
  float3x3 normal_matrix = float3x3(uniforms.model_matrix[0].xyz,
                                    uniforms.model_matrix[1].xyz,
                                    uniforms.model_matrix[2].xyz);
  out.world_normal = normal_matrix * vertices[vertexID].normal;
  out.tex_coord = vertices[vertexID].tex_coord;
  
  return out;
}

fragment float4 fragment_main(RasterizerData in [[stage_in]],
                              texture2d<float> colorTexture [[texture(0)]],
                              constant Uniforms &uniforms [[buffer(1)]])
{
  constexpr sampler textureSampler (mag_filter::linear, min_filter::linear, s_address::repeat, t_address::repeat);
  float4 base_color = colorTexture.sample(textureSampler, in.tex_coord);
  
  // Directional Lighting from Uniforms
  float3 light_dir = normalize(uniforms.light_dir);
  float3 normal = normalize(in.world_normal);
  
  float diffuse = max(dot(normal, light_dir), 0.0);
  
  float3 light_intensity = (diffuse * uniforms.light_color) + uniforms.ambient_intensity;
  return float4(base_color.rgb * light_intensity, base_color.a);
}

fragment float4 grid_fragment_main(RasterizerData in [[stage_in]],
                                   constant Uniforms &uniforms [[buffer(1)]])
{
    float2 coord = in.world_position.xz;
    float2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);

    // Thicker lines every 10 units
    float2 coord10 = coord / 10.0;
    float2 grid10 = abs(fract(coord10 - 0.5) - 0.5) / fwidth(coord10);
    float line10 = min(grid10.x, grid10.y);
    float alpha10 = 1.0 - min(line10, 1.0);
    
    alpha = max(alpha * 0.3, alpha10); // Dim 1-unit lines, bright 10-unit lines

    float dist = length(in.world_position.xyz - uniforms.camera_pos);
    float fade = 1.0 - smoothstep(10.0, 100.0, dist);

    if (alpha * fade < 0.01) discard_fragment();

    return float4(0.8, 0.8, 0.8, alpha * fade);
}
