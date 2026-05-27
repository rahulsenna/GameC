#pragma once
#include "base_arena.h"
#include <simd/simd.h>

struct GameState
{
  B32 is_initialized;
  F32 time;
};

// -- Render Command Structures --

enum RenderGroupEntryType
{
  RenderGroupEntryType_Clear,
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

struct Vertex
{
  simd_float3 position;
  simd_float4 color;
};

struct Uniforms
{
  simd_float4x4 mvp_matrix;
};

struct RenderGroupEntry_DrawMesh
{
  Uniforms uniforms;
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
void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         U32 vertex_count, Vertex *vertices);

// -- Game Output --

struct GameOutput
{
  RenderGroup render_group;
};

extern "C" void GameUpdateAndRender(Arena *arena, GameOutput *out_output);
