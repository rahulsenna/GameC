#pragma once
#include "base_arena.h"
#include "math_utils.h"

// Ozz-animation headers
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/soa_transform.h"

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
  Vec3 position;
  float pitch;
  float yaw;
};

#define MAX_BONES 64

struct Vertex
{
  Vec3 position;
  Vec3 normal;
  Vec3 tangent;
  Vec2 tex_coord;
  U32 bone_indices[4];
  F32 bone_weights[4];
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
  U32 num_bones;
  void **bone_nodes;
  U16 *ozz_joint_mapping; // maps from local bone index to ozz skeleton joint
                          // index
  Mat4 *inverse_bind_matrices;
};

struct FBXModel
{
  U32 num_nodes;
  FBXNode nodes[32]; // Support up to 32 sub-meshes
  void *ufbx_scene_ptr;

  B32 has_animation;
  ozz::animation::Skeleton *ozz_skeleton;
  ozz::animation::Animation *ozz_animation;
  ozz::animation::SamplingJob::Context *ozz_cache;

  U32 num_soa_joints;
  ozz::math::SoaTransform *local_transforms;
  ozz::math::Float4x4 *model_matrices;
};

struct GameState
{
  B32 is_initialized;
  F32 time;
  Camera camera;
  U32 num_models;
  FBXModel *models;
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
  Mat4 mvp_matrix;
  Mat4 model_matrix;
  Vec3 light_dir;
  Vec3 light_color;
  Vec3 camera_pos;
  float ambient_intensity;
  Mat4 bone_matrices[MAX_BONES];
  U32 has_bones;
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
