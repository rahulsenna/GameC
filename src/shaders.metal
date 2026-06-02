#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float3 position;
  float3 normal;
  float3 tangent;
  float2 tex_coord;
  packed_uint4 bone_indices;
  packed_float4 bone_weights;
};

struct Uniforms
{
  float4x4 mvp_matrix;
  float4x4 model_matrix;
  float3 light_dir;
  float3 light_color;
  float3 camera_pos;
  float ambient_intensity;
  uint has_bones;
  uint vertex_offset;
  // Byte offset into the shared GPU heap where bone matrices for this draw
  // live. The vertex shader reads them directly through the existing heap root
  // pointer.
  uint bone_matrix_offset;

  uint albedo_tex;
  uint normal_tex;
  uint metallic_tex;
  uint roughness_tex;
  uint ao_tex;
};

struct RootData
{
  device VertexIn *vertex_heap;
};

struct TextureHeap
{
  array<texture2d<float>, 1024> textures;
};

struct RasterizerData
{
  float4 position [[position]];
  float3 world_position;
  float3 world_normal;
  float3 world_tangent;
  float2 tex_coord;
};

vertex RasterizerData vertex_main(uint vertexID [[vertex_id]],
                                  constant RootData *root [[buffer(0)]],
                                  constant Uniforms &uniforms [[buffer(1)]])
{
  RasterizerData out;

  uint id = uniforms.vertex_offset + vertexID;
  float4 local_pos = float4(root->vertex_heap[id].position, 1.0);
  float4 local_norm = float4(root->vertex_heap[id].normal, 0.0);
  float4 local_tangent = float4(root->vertex_heap[id].tangent, 0.0);

  if (uniforms.has_bones > 0)
  {
    // Bone matrices live in the same GPU heap as vertex data.  Reach them via
    // the heap root pointer with a simple byte offset — fully bindless.
    device float4x4 *bone_mats =
        (device float4x4 *)((device char *)root->vertex_heap +
                            uniforms.bone_matrix_offset);

    local_pos = float4(0.0);
    local_norm = float4(0.0);
    local_tangent = float4(0.0);
    for (int i = 0; i < 4; i++)
    {
      float weight = root->vertex_heap[id].bone_weights[i];
      if (weight > 0.0)
      {
        uint bone_idx = root->vertex_heap[id].bone_indices[i];
        float4x4 bone_matrix = bone_mats[bone_idx];
        local_pos +=
            (bone_matrix * float4(root->vertex_heap[id].position, 1.0)) *
            weight;
        local_norm +=
            (bone_matrix * float4(root->vertex_heap[id].normal, 0.0)) * weight;
        local_tangent +=
            (bone_matrix * float4(root->vertex_heap[id].tangent, 0.0)) * weight;
      }
    }
    local_pos.w = 1.0;
  }

  out.position = uniforms.mvp_matrix * local_pos;
  out.world_position = (uniforms.model_matrix * local_pos).xyz;

  // Rotate the normal using the model matrix (ignoring translation)
  float3x3 normal_matrix =
      float3x3(uniforms.model_matrix[0].xyz, uniforms.model_matrix[1].xyz,
               uniforms.model_matrix[2].xyz);
  out.world_normal = normal_matrix * local_norm.xyz;
  out.world_tangent = normal_matrix * local_tangent.xyz;
  out.tex_coord = root->vertex_heap[id].tex_coord;

  return out;
}

constant float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / max(denom, 0.0000001);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

fragment float4 fragment_main(RasterizerData in [[stage_in]],
                              constant RootData *root [[buffer(0)]],
                              constant Uniforms &uniforms [[buffer(1)]],
                              constant TextureHeap &texture_heap [[buffer(2)]])
{
  constexpr sampler textureSampler(mag_filter::linear, min_filter::linear,
                                   mip_filter::linear, s_address::repeat,
                                   t_address::repeat);

  float4 albedo_rgba = float4(1.0);
  if (uniforms.albedo_tex != 0)
  {
    albedo_rgba = texture_heap.textures[uniforms.albedo_tex].sample(
        textureSampler, in.tex_coord);
  }

  if (albedo_rgba.a < 0.1)
  {
    discard_fragment();
  }

  float3 albedo = pow(albedo_rgba.rgb, float3(2.2));

  float metallic = 0.0;
  if (uniforms.metallic_tex != 0)
  {
    metallic = texture_heap.textures[uniforms.metallic_tex]
                   .sample(textureSampler, in.tex_coord)
                   .r;
  }
  float roughness = 1.0;
  if (uniforms.roughness_tex != 0)
  {
    roughness = texture_heap.textures[uniforms.roughness_tex]
                    .sample(textureSampler, in.tex_coord)
                    .r;
  }
  float ao = 1.0;
  if (uniforms.ao_tex != 0)
  {
    ao = texture_heap.textures[uniforms.ao_tex]
             .sample(textureSampler, in.tex_coord)
             .r;
  }

  // Normal mapping using vertex tangents
  float3 N = normalize(in.world_normal);
  float3 T = normalize(in.world_tangent);

  // Gram-Schmidt orthogonalize (in case of interpolation drift)
  T = normalize(T - dot(T, N) * N);
  float3 B = cross(N, T);

  float3x3 TBN = float3x3(T, B, N);

  float3 tangentNormal = float3(0, 0, 1);
  if (uniforms.normal_tex != 0)
  {
    tangentNormal = texture_heap.textures[uniforms.normal_tex]
                            .sample(textureSampler, in.tex_coord)
                            .rgb *
                        2.0 -
                    1.0;
  }

  // Most normal maps (like OpenGL) expect Y to go UP. If it looks like lizard
  // skin (concave), it's possible it's a DirectX normal map (Y DOWN). For now
  // we assume OpenGL. tangentNormal.y = -tangentNormal.y;

  N = normalize(TBN * tangentNormal);

  // Avoid black spots if N becomes zero (should not happen with proper
  // tangents)
  if (length(N) < 0.1 || isunordered(N.x, 0.0))
  {
    N = normalize(in.world_normal);
  }

  float3 V = normalize(uniforms.camera_pos - in.world_position);
  float3 L = normalize(uniforms.light_dir);
  float3 H = normalize(V + L);

  float3 F0 = float3(0.04);
  F0 = mix(F0, albedo, metallic);

  // Calculate per-light radiance
  float3 radiance = uniforms.light_color;

  // Cook-Torrance BRDF
  float NDF = DistributionGGX(N, H, roughness);
  float G = GeometrySmith(N, V, L, roughness);
  float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

  float3 numerator = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
  float3 specular = numerator / denominator;

  float3 kS = F;
  float3 kD = float3(1.0) - kS;
  kD *= 1.0 - metallic;

  float NdotL = max(dot(N, L), 0.0);

  float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

  // Fake IBL for ambient lighting
  float3 R = reflect(-V, N);

  // Simulate a simple sky/ground environment
  // Sky is a soft blue, ground is a dark brown/grey
  float3 skyColor = float3(0.5, 0.7, 1.0) * 2.0; // Boosted sky brightness
  float3 groundColor = float3(0.1, 0.08, 0.05);

  float3 diffuseIrradiance =
      mix(groundColor, skyColor, N.y * 0.5 + 0.5) * uniforms.ambient_intensity;
  // Specular irradiance shouldn't be strictly bound by the low
  // ambient_intensity, metals should reflect the bright sky!
  float3 specularIrradiance = mix(groundColor, skyColor, R.y * 0.5 + 0.5);

  float3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
  float3 kS_ambient = F_ambient;
  float3 kD_ambient = 1.0 - kS_ambient;
  kD_ambient *= 1.0 - metallic;

  // Dimming the reflection by (1-roughness) loses energy.
  // Instead, a rougher surface scatters the reflection, making it act more like
  // diffuse light.
  float3 specularAmbient =
      mix(specularIrradiance, diffuseIrradiance, roughness) * F_ambient;
  float3 diffuseAmbient = kD_ambient * diffuseIrradiance * albedo;

  float3 ambient = (diffuseAmbient + specularAmbient) * ao;
  float3 color = ambient + Lo;

  // HDR tonemapping
  color = color / (color + float3(1.0));
  // Gamma correct
  color = pow(color, float3(1.0 / 2.2));

  return float4(color, albedo_rgba.a);
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

  if (alpha * fade < 0.01)
    discard_fragment();

  return float4(0.8, 0.8, 0.8, alpha * fade);
}
