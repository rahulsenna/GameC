#pragma once
#include "base_arena.h"
#include "math_utils.h"

// Ozz-animation headers
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/simd_math.h"
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
  B32 key_p;
  B32 key_shift;
  B32 key_ctrl;
  B32 key_space;
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
  U32 num_soa_joints;
  ozz::math::SoaTransform *local_transforms;
  ozz::math::Float4x4 *model_matrices;
};

struct OzzAnimation
{
  ozz::animation::Animation *anim;
  ozz::animation::SamplingJob::Context *cache;
};

enum AnimLayer
{
  LAYER_IDLE = 0,
  LAYER_IDLE_PREV,
  LAYER_WALK,
  LAYER_JOG,
  LAYER_FASTRUN,
  LAYER_JUMP,
  NUM_ANIM_LAYERS
};

enum AnimClip
{
  CLIP_IDLE_1 = 0,
  CLIP_IDLE_2,
  CLIP_IDLE_3,
  CLIP_IDLE_4,
  CLIP_WALK,
  CLIP_JOG,
  CLIP_FASTRUN,
  CLIP_JUMP_RUN,
  CLIP_JUMP_STAND_1,
  CLIP_JUMP_STAND_2,
  CLIP_JUMP_STAND_3,
  NUM_ANIM_CLIPS
};

struct PlayerController
{
  // Physical State
  Vec3 position;
  float yaw;

  // Input/Movement State
  float speed_param;
  int sticky_speed_mode;
  float last_shift_time;
  B32 was_shift_down;

  // Jumping State
  B32 is_jumping;
  float jump_anim_time;
  float jump_duration;
  int current_jump_anim_index;

  // Idle State
  float anim_time;
  int current_idle_anim_index;
  int prev_idle_anim_index;
  float idle_crossfade_time;

  // Animations
  OzzAnimation anim_clips[NUM_ANIM_CLIPS];

  // Blending & Root Motion
  ozz::math::SoaTransform *local_layers[NUM_ANIM_LAYERS];
  ozz::math::SoaTransform *blended_locals;
  Vec3 prev_root_trans[NUM_ANIM_LAYERS];
};

struct GameState
{
  B32 is_initialized;
  F32 time;
  Camera camera;
  U32 num_models;
  FBXModel *models;

  B32 is_third_person;
  B32 prev_key_p;
  float total_time;

  PlayerController player;
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
