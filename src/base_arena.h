#pragma once
#include "base_types.h"

struct Arena
{
  Arena *prev;
  Arena *current;
  U64 base_pos;
  U64 pos;
  U64 cmt;
  U64 res;
};

// 1GB reserve, 1MB initial commit by default
Arena *ArenaAlloc(U64 reserve_size = 1ull << 30, U64 commit_size = 1ull << 20);
void ArenaRelease(Arena *arena);
void *ArenaPush(Arena *arena, U64 size);
void ArenaPopTo(Arena *arena, U64 pos);
void ArenaClear(Arena *arena);

#define PushArray(arena, type, count)                                          \
  (type *)ArenaPush((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray(arena, type, 1)

// Temporary memory scope
struct TempArena
{
  Arena *arena;
  U64 pos;
};

TempArena TempArenaBegin(Arena *arena);
void TempArenaEnd(TempArena temp);
