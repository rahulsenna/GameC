#include "base_arena.h"
#include <sys/mman.h>
#include <unistd.h>

static U64 GetPageSize()
{
  return (U64)sysconf(_SC_PAGESIZE);
}

Arena *ArenaAlloc(U64 reserve_size, U64 commit_size)
{
  U64 page_size = GetPageSize();
  reserve_size = AlignPow2(reserve_size, page_size);
  commit_size = AlignPow2(commit_size, page_size);

  // Reserve memory without committing (PROT_NONE)
  void *base = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
  Assert(base != MAP_FAILED);

  // Commit the initial amount of memory
  mprotect(base, commit_size, PROT_READ | PROT_WRITE);

  Arena *arena = (Arena *)base;
  arena->prev = 0;
  arena->current = arena;
  arena->base_pos = 0;
  arena->pos = sizeof(Arena);
  arena->cmt = commit_size;
  arena->res = reserve_size;

  return arena;
}

void ArenaRelease(Arena *arena)
{
  munmap(arena, arena->res);
}

void *ArenaPush(Arena *arena, U64 size)
{
  U64 align = 8;
  U64 pos = AlignPow2(arena->pos, align);
  U64 new_pos = pos + size;

  if (new_pos > arena->cmt)
  {
    U64 page_size = GetPageSize();
    U64 new_cmt = AlignPow2(new_pos, page_size);
    Assert(new_cmt <= arena->res); // Out of memory!

    // Commit more pages
    mprotect((U8 *)arena + arena->cmt, new_cmt - arena->cmt,
             PROT_READ | PROT_WRITE);
    arena->cmt = new_cmt;
  }

  void *result = (U8 *)arena + pos;
  arena->pos = new_pos;

  // Zero initialize
  for (U64 i = 0; i < size; ++i)
  {
    ((U8 *)result)[i] = 0;
  }

  return result;
}

void ArenaPopTo(Arena *arena, U64 pos)
{
  arena->pos = pos;
}

void ArenaClear(Arena *arena)
{
  ArenaPopTo(arena, sizeof(Arena));
}

TempArena TempArenaBegin(Arena *arena)
{
  TempArena temp;
  temp.arena = arena;
  temp.pos = arena->pos;
  return temp;
}

void TempArenaEnd(TempArena temp)
{
  ArenaPopTo(temp.arena, temp.pos);
}
