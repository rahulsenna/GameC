#include "game.h"
#include <math.h>

extern "C" void GameUpdateAndRender(Arena *arena, GameOutput *out_output)
{
  // We allocate the GameState at the very beginning of our Arena
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

  state->time += 0.016f; // Fake 60fps delta for now

  // Animate the clear color using math to prove the game loop is updating
  out_output->clear_color[0] = 0.5f + 0.5f * sinf(state->time * 2.0f);
  out_output->clear_color[1] = 0.2f;
  out_output->clear_color[2] = 0.5f + 0.5f * cosf(state->time * 2.0f);
  out_output->clear_color[3] = 1.0f;
}
