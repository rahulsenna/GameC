#pragma once
#include "base_arena.h"

// Game State that will persist across frames
struct GameState
{
  B32 is_initialized;
  F32 time;
};

// What the game tells the platform layer to do
struct GameOutput
{
  F32 clear_color[4];
};

extern "C" void GameUpdateAndRender(Arena *arena, GameOutput *out_output);
