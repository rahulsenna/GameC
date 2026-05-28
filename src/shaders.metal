#include <metal_stdlib>
using namespace metal;

struct VertexIn
{
  float3 position;
  float3 normal;
  float3 tangent;
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
  float3 world_tangent;
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
  out.world_tangent = normal_matrix * vertices[vertexID].tangent;
  out.tex_coord = vertices[vertexID].tex_coord;
  
  return out;
}

constant float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

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
                              texture2d<float> albedoMap [[texture(0)]],
                              texture2d<float> normalMap [[texture(1)]],
                              texture2d<float> metallicMap [[texture(2)]],
                              texture2d<float> roughnessMap [[texture(3)]],
                              texture2d<float> aoMap [[texture(4)]],
                              constant Uniforms &uniforms [[buffer(1)]])
{
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear, mip_filter::linear, s_address::repeat, t_address::repeat);
    
    // Albedo is often in sRGB, so convert to linear space for calculations
    float4 albedo_rgba = albedoMap.sample(textureSampler, in.tex_coord);
    
    // Discard almost completely invisible pixels to prevent them from writing to the depth buffer and occluding the face behind them
    if (albedo_rgba.a < 0.1) {
        discard_fragment();
    }
    
    // We rely on pipeline alpha blending now, so just pass the soft alpha through
    float3 albedo = pow(albedo_rgba.rgb, float3(2.2));
    
    float metallic = metallicMap.sample(textureSampler, in.tex_coord).r;
    float roughness = roughnessMap.sample(textureSampler, in.tex_coord).r;
    float ao = aoMap.sample(textureSampler, in.tex_coord).r;
    
    // Normal mapping using vertex tangents
    float3 N = normalize(in.world_normal);
    float3 T = normalize(in.world_tangent);
    
    // Gram-Schmidt orthogonalize (in case of interpolation drift)
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);

    float3 tangentNormal = normalMap.sample(textureSampler, in.tex_coord).rgb * 2.0 - 1.0;
    
    // Most normal maps (like OpenGL) expect Y to go UP. If it looks like lizard skin (concave),
    // it's possible it's a DirectX normal map (Y DOWN). For now we assume OpenGL.
    // tangentNormal.y = -tangentNormal.y;
    
    N = normalize(TBN * tangentNormal);
    
    // Avoid black spots if N becomes zero (should not happen with proper tangents)
    if (length(N) < 0.1 || isunordered(N.x, 0.0)) {
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
    float G   = GeometrySmith(N, V, L, roughness);    
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);      
    
    float3 numerator    = NDF * G * F;
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
    
    float3 diffuseIrradiance = mix(groundColor, skyColor, N.y * 0.5 + 0.5) * uniforms.ambient_intensity;
    // Specular irradiance shouldn't be strictly bound by the low ambient_intensity, 
    // metals should reflect the bright sky!
    float3 specularIrradiance = mix(groundColor, skyColor, R.y * 0.5 + 0.5);
    
    float3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    float3 kS_ambient = F_ambient;
    float3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    
    // Dimming the reflection by (1-roughness) loses energy. 
    // Instead, a rougher surface scatters the reflection, making it act more like diffuse light.
    float3 specularAmbient = mix(specularIrradiance, diffuseIrradiance, roughness) * F_ambient;
    float3 diffuseAmbient = kD_ambient * diffuseIrradiance * albedo;
    
    float3 ambient = (diffuseAmbient + specularAmbient) * ao;
    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + float3(1.0));
    // Gamma correct
    color = pow(color, float3(1.0/2.2)); 

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

    if (alpha * fade < 0.01) discard_fragment();

    return float4(0.8, 0.8, 0.8, alpha * fade);
}
