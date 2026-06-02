#pragma once
#include "base_arena.h"
#include "math_utils.h"

#include "animation.h"
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

typedef U64 GpuPtr;

// First GPU_FRAME_ARENA_TOTAL bytes of global_gpu_heap are reserved for
// triple-buffered per-frame dynamic data (uniforms, bone matrices, etc.).
// Permanent geometry allocations start after this region.
#define GPU_FRAME_ARENA_COUNT 3u
#define GPU_FRAME_ARENA_SIZE (1u * 1024u * 1024u) // 1 MB per frame
#define GPU_FRAME_ARENA_TOTAL                                                  \
  (GPU_FRAME_ARENA_COUNT * GPU_FRAME_ARENA_SIZE) // 3 MB total

struct GpuAllocator
{
  U64 capacity;
  U64 used;
};

inline GpuPtr gpuMalloc(GpuAllocator *allocator, U64 size)
{
  size = (size + 15) & ~15;
  if (allocator->used + size > allocator->capacity)
    return 0;
  GpuPtr offset = allocator->used;
  allocator->used += size;
  return offset;
}

#define MAX_BONES 128

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
  GpuPtr vertex_offset; // GPU heap offset
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
  GpuAllocator gpu_allocator;
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
  RenderGroupEntryType_UploadGeometry,
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

struct RenderGroupEntry_UploadGeometry
{
  GpuPtr offset;
  U32 size;
  // Raw geometry data immediately follows
};

struct Uniforms
{
  Mat4 mvp_matrix;
  Mat4 model_matrix;
  Vec3 light_dir;
  Vec3 light_color;
  Vec3 camera_pos;
  float ambient_intensity;
  U32 has_bones;
  U32 vertex_offset;
  // Byte offset into the shared GPU heap where this draw's bone matrices live.
  // The vertex shader fetches them directly via the existing heap root pointer,
  // so no extra buffer binding is required (fully bindless).
  U32 bone_matrix_offset;
  U32 albedo_tex;
  U32 normal_tex;
  U32 metallic_tex;
  U32 roughness_tex;
  U32 ao_tex;
};

struct RenderGroupEntry_DrawMesh
{
  Uniforms uniforms;
  U32 shader_type;
  U32 vertex_count;
  GpuPtr vertex_offset;
  // When uniforms.has_bones != 0, MAX_BONES Mat4 matrices immediately follow
  // this struct in the render group (mirroring how pixel data follows
  // RenderGroupEntry_UploadTexture).  The renderer bump-allocates them into
  // the current frame arena and fills uniforms.bone_matrix_offset before
  // issuing the draw.
};

struct RenderGroup
{
  U32 size;
  U32 max_size;
  U8 *base;
};

void PushClearCommand(RenderGroup *group, F32 r, F32 g, F32 b, F32 a);
void *PushUploadTextureCommand(RenderGroup *group, U32 handle, U32 width,
                               U32 height);
void *PushUploadGeometryCommand(RenderGroup *group, GpuPtr offset, U32 size);
void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         MaterialTextures textures, U32 shader_type,
                         U32 vertex_count, GpuPtr vertex_offset,
                         const Mat4 *bone_matrices = nullptr);

// -- Game Output --

struct GameOutput
{
  RenderGroup render_group;
};

extern "C" void GameUpdateAndRender(Arena *arena, GameInput *input, float dt,
                                    GameOutput *out_output);
