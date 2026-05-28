#pragma once
#include "base_arena.h"
#include <simd/simd.h>

struct GameInput
{
  B32 key_w;
  B32 key_a;
  B32 key_s;
  B32 key_d;
  B32 key_up;
  B32 key_down;
  B32 key_left;
  B32 key_right;
};

struct Camera
{
  simd_float3 position;
  float pitch;
  float yaw;
};

struct Vertex
{
  simd_float3 position;
  simd_float3 normal;
  simd_float3 tangent;
  simd_float2 tex_coord;
};

struct MeshData
{
  U32 vertex_count;
  Vertex *vertices;
};

struct MaterialTextures
{
  U32 albedo;
  U32 normal;
  U32 metallic;
  U32 roughness;
  U32 ao;
};

struct FBXNode
{
  U32 vertex_count;
  Vertex *vertices;
  MaterialTextures textures;
};

struct FBXModel
{
  U32 num_nodes;
  FBXNode nodes[32]; // Support up to 32 sub-meshes
};

struct GameState
{
  B32 is_initialized;
  F32 time;
  MaterialTextures default_textures;
  MaterialTextures alien_textures;
  Camera camera;
  MeshData shapes[10]; // Back to 10 for procedurals
  FBXModel fbx_model;
  FBXModel banana_model;
};

// -- Render Command Structures --

enum RenderGroupEntryType
{
  RenderGroupEntryType_Clear,
  RenderGroupEntryType_UploadTexture,
  RenderGroupEntryType_DrawMesh,
};

struct RenderGroupEntryHeader
{
  RenderGroupEntryType type;
};

struct RenderGroupEntry_Clear
{
  F32 color[4];
};

struct RenderGroupEntry_UploadTexture
{
  U32 handle;
  U32 width;
  U32 height;
  // Raw pixel data (RGBA, 8 bits per channel) immediately follows this struct
};

// Removed duplicate Vertex struct from here

struct Uniforms
{
  simd_float4x4 mvp_matrix;
  simd_float4x4 model_matrix;
  simd_float3 light_dir;
  simd_float3 light_color;
  simd_float3 camera_pos;
  float ambient_intensity;
};

struct RenderGroupEntry_DrawMesh
{
  Uniforms uniforms;
  MaterialTextures textures;
  U32 shader_type;
  U32 vertex_count;
  // Note: Vertices array is stored immediately following this struct in memory!
};

struct RenderGroup
{
  U32 size;
  U32 max_size;
  U8 *base;
};

void PushClearCommand(RenderGroup *group, F32 r, F32 g, F32 b, F32 a);
void PushUploadTextureCommand(RenderGroup *group, U32 handle, U32 width,
                              U32 height, void *pixels);
void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         MaterialTextures textures, U32 shader_type,
                         U32 vertex_count, Vertex *vertices);

// -- Game Output --

struct GameOutput
{
  RenderGroup render_group;
};

extern "C" void GameUpdateAndRender(Arena *arena, GameInput *input,
                                    GameOutput *out_output);
