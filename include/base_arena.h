#pragma once
#include "base_types.h"

#define ARENA_HEADER_SIZE 128

typedef U64 ArenaFlags;
enum
{
  ArenaFlag_NoChain = (1 << 0),
};

struct ArenaParams
{
  ArenaFlags flags;
  U64 reserve_size;
  U64 commit_size;
};

struct Arena
{
  Arena *prev;
  Arena *current;
  ArenaFlags flags;
  U64 cmt_size;
  U64 res_size;
  U64 base_pos;
  U64 pos;
  U64 cmt;
  U64 res;
  Arena *free_last;
};

Arena *ArenaAlloc(ArenaParams *params = nullptr);
Arena *ArenaAllocDefault(void);
void ArenaRelease(Arena *arena);
void *ArenaPush(Arena *arena, U64 size, U64 align = 8, B32 zero = 1);
void ArenaPopTo(Arena *arena, U64 pos);
void ArenaClear(Arena *arena);
U64 ArenaPos(Arena *arena);

#define PushArrayNoZeroAligned(a, type, count, align)                          \
  (type *)ArenaPush((a), sizeof(type) * (count), (align), 0)
#define PushArrayAligned(a, type, count, align)                                \
  (type *)ArenaPush((a), sizeof(type) * (count), (align), 1)
#define PushArrayNoZero(a, type, count)                                        \
  PushArrayNoZeroAligned(a, type, count, BASE_MAX(8, AlignOf(type)))
#define PushArray(a, type, count)                                              \
  PushArrayAligned(a, type, count, BASE_MAX(8, AlignOf(type)))
#define PushStructNoZero(a, type) PushArrayNoZero(a, type, 1)
#define PushStruct(a, type) PushArray(a, type, 1)

// Temporary memory scope
struct TempArena
{
  Arena *arena;
  U64 pos;
};

TempArena TempArenaBegin(Arena *arena);
void TempArenaEnd(TempArena temp);
