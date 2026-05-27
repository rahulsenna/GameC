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
  RenderGroupEntryType_DrawTriangle,
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
  simd_float2 position;
  simd_float4 color;
};

struct RenderGroupEntry_DrawTriangle
{
  Vertex vertices[3];
};

struct RenderGroup
{
  U32 size;
  U32 max_size;
  U8 *base;
};

void PushClearCommand(RenderGroup *group, F32 r, F32 g, F32 b, F32 a);
void PushDrawTriangleCommand(RenderGroup *group, Vertex v0, Vertex v1,
                             Vertex v2);

// -- Game Output --

struct GameOutput
{
  RenderGroup render_group;
};

extern "C" void GameUpdateAndRender(Arena *arena, GameOutput *out_output);
