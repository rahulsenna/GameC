#include "game.h"
#include "math_utils.h"
#include <math.h>

void PushClearCommand(RenderGroup *group, F32 r, F32 g, F32 b, F32 a)
{
  U32 total_size =
      sizeof(RenderGroupEntryHeader) + sizeof(RenderGroupEntry_Clear);
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_Clear;

  RenderGroupEntry_Clear *entry = (RenderGroupEntry_Clear *)(header + 1);
  entry->color[0] = r;
  entry->color[1] = g;
  entry->color[2] = b;
  entry->color[3] = a;

  group->size += total_size;
}

void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         U32 vertex_count, Vertex *vertices)
{
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_DrawMesh) +
                   sizeof(Vertex) * vertex_count;
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_DrawMesh;

  RenderGroupEntry_DrawMesh *entry = (RenderGroupEntry_DrawMesh *)(header + 1);
  entry->uniforms = uniforms;
  entry->vertex_count = vertex_count;

  Vertex *dst_vertices = (Vertex *)(entry + 1);
  for (U32 i = 0; i < vertex_count; ++i)
  {
    dst_vertices[i] = vertices[i];
  }

  group->size += total_size;
}

extern "C" void GameUpdateAndRender(Arena *arena, GameOutput *out_output)
{
  if (arena->pos == sizeof(Arena))
  {
    PushStruct(arena, GameState);
  }

  GameState *state = (GameState *)((U8 *)arena + sizeof(Arena));

  if (!state->is_initialized)
  {
    state->is_initialized = 1;
    state->time = 0.0f;
  }

  state->time += 0.016f;

  out_output->render_group.size = 0;
  PushClearCommand(&out_output->render_group, 0.1f, 0.1f, 0.1f, 1.0f);

  // Define a Cube (36 vertices)
  Vertex cube_vertices[] = {// Front Face (Red)
                            {{-0.5, -0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},
                            {{0.5, -0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},
                            {{-0.5, 0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},
                            {{0.5, -0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},
                            {{0.5, 0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},
                            {{-0.5, 0.5, 0.5}, {1.0, 0.0, 0.0, 1.0}},

                            // Back Face (Green)
                            {{0.5, -0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
                            {{-0.5, -0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
                            {{0.5, 0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
                            {{-0.5, -0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
                            {{-0.5, 0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},
                            {{0.5, 0.5, -0.5}, {0.0, 1.0, 0.0, 1.0}},

                            // Top Face (Blue)
                            {{-0.5, 0.5, 0.5}, {0.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, 0.5}, {0.0, 0.0, 1.0, 1.0}},
                            {{-0.5, 0.5, -0.5}, {0.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, 0.5}, {0.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, -0.5}, {0.0, 0.0, 1.0, 1.0}},
                            {{-0.5, 0.5, -0.5}, {0.0, 0.0, 1.0, 1.0}},

                            // Bottom Face (Yellow)
                            {{-0.5, -0.5, -0.5}, {1.0, 1.0, 0.0, 1.0}},
                            {{0.5, -0.5, -0.5}, {1.0, 1.0, 0.0, 1.0}},
                            {{-0.5, -0.5, 0.5}, {1.0, 1.0, 0.0, 1.0}},
                            {{0.5, -0.5, -0.5}, {1.0, 1.0, 0.0, 1.0}},
                            {{0.5, -0.5, 0.5}, {1.0, 1.0, 0.0, 1.0}},
                            {{-0.5, -0.5, 0.5}, {1.0, 1.0, 0.0, 1.0}},

                            // Right Face (Magenta)
                            {{0.5, -0.5, 0.5}, {1.0, 0.0, 1.0, 1.0}},
                            {{0.5, -0.5, -0.5}, {1.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, 0.5}, {1.0, 0.0, 1.0, 1.0}},
                            {{0.5, -0.5, -0.5}, {1.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, -0.5}, {1.0, 0.0, 1.0, 1.0}},
                            {{0.5, 0.5, 0.5}, {1.0, 0.0, 1.0, 1.0}},

                            // Left Face (Cyan)
                            {{-0.5, -0.5, -0.5}, {0.0, 1.0, 1.0, 1.0}},
                            {{-0.5, -0.5, 0.5}, {0.0, 1.0, 1.0, 1.0}},
                            {{-0.5, 0.5, -0.5}, {0.0, 1.0, 1.0, 1.0}},
                            {{-0.5, -0.5, 0.5}, {0.0, 1.0, 1.0, 1.0}},
                            {{-0.5, 0.5, 0.5}, {0.0, 1.0, 1.0, 1.0}},
                            {{-0.5, 0.5, -0.5}, {0.0, 1.0, 1.0, 1.0}}};

  // 1. Model Matrix (Rotate over time)
  simd_float4x4 mat_rot_y = math_make_rotation_y(state->time);
  simd_float4x4 mat_rot_x = math_make_rotation_x(state->time * 0.5f);
  simd_float4x4 model_matrix = simd_mul(mat_rot_y, mat_rot_x);

  // 2. View Matrix (Move camera backwards along Z)
  simd_float4x4 view_matrix =
      math_make_translation(simd_make_float3(0, 0, -3.0f));

  // 3. Projection Matrix
  float fov = 60.0f * (3.14159f / 180.0f);
  float aspect = 800.0f / 600.0f; // Typical aspect ratio
  simd_float4x4 proj_matrix = math_make_perspective(fov, aspect, 0.1f, 100.0f);

  // MVP = Proj * View * Model
  simd_float4x4 vp_matrix = simd_mul(proj_matrix, view_matrix);
  simd_float4x4 mvp_matrix = simd_mul(vp_matrix, model_matrix);

  Uniforms uniforms = {mvp_matrix};

  PushDrawMeshCommand(&out_output->render_group, uniforms, 36, cube_vertices);
}
