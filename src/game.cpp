#include "game.h"
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

void PushDrawTriangleCommand(RenderGroup *group, Vertex v0, Vertex v1,
                             Vertex v2)
{
  U32 total_size =
      sizeof(RenderGroupEntryHeader) + sizeof(RenderGroupEntry_DrawTriangle);
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_DrawTriangle;

  RenderGroupEntry_DrawTriangle *entry =
      (RenderGroupEntry_DrawTriangle *)(header + 1);
  entry->vertices[0] = v0;
  entry->vertices[1] = v1;
  entry->vertices[2] = v2;

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

  // Initialize RenderGroup. For this simple phase, we'll just allocate a
  // temporary chunk of memory from the arena that the platform layer will use
  // and then we reset it next frame. Actually, we can just use the arena for
  // temporary allocation and pop it later. Or we just give it a fixed size
  // static buffer for now. Let's push onto the arena, and the platform layer
  // doesn't need to pop, we can pop at the end of the frame in the platform
  // layer. We'll just allocate the RenderGroup memory directly in the platform
  // layer and pass it down in GameOutput. For safety, let's assume
  // `out_output->render_group.base` and `max_size` are pre-filled by
  // osx_main.mm!
  out_output->render_group.size = 0;

  F32 r = 0.1f;
  F32 g = 0.1f;
  F32 b = 0.1f;
  F32 a = 1.0f;
  PushClearCommand(&out_output->render_group, r, g, b, a);

  // Draw a spinning triangle!
  F32 scale = 0.5f;
  F32 c = cosf(state->time);
  F32 s = sinf(state->time);

  // Top vertex (0, 1)
  Vertex v0 = {{(0.0f * c - 1.0f * s) * scale, (0.0f * s + 1.0f * c) * scale},
               {1.0f, 0.0f, 0.0f, 1.0f}};

  // Bottom Right (1, -1)
  Vertex v1 = {
      {(1.0f * c - (-1.0f) * s) * scale, (1.0f * s + (-1.0f) * c) * scale},
      {0.0f, 1.0f, 0.0f, 1.0f}};

  // Bottom Left (-1, -1)
  Vertex v2 = {
      {(-1.0f * c - (-1.0f) * s) * scale, (-1.0f * s + (-1.0f) * c) * scale},
      {0.0f, 0.0f, 1.0f, 1.0f}};

  PushDrawTriangleCommand(&out_output->render_group, v0, v1, v2);
}
