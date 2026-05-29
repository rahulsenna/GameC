#include "base_arena.h"
#include <stdio.h>
#include <string.h> // for memset
#include <sys/mman.h>
#include <unistd.h>

static U64 GetPageSize()
{
  static U64 page_size = 0;
  if (page_size == 0)
  {
    page_size = (U64)sysconf(_SC_PAGESIZE);
  }
  return page_size;
}

Arena *ArenaAlloc(ArenaParams *params)
{
  U64 reserve_size = params ? params->reserve_size : GB(1);
  U64 commit_size = params ? params->commit_size : MB(1);
  ArenaFlags flags = params ? params->flags : 0;

  U64 page_size = GetPageSize();
  reserve_size = AlignPow2(reserve_size, page_size);
  commit_size = AlignPow2(commit_size, page_size);

  void *base = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
  Assert(base != MAP_FAILED);

  int mprotect_result = mprotect(base, commit_size, PROT_READ | PROT_WRITE);
  Assert(mprotect_result == 0);
  AsanPoisonMemoryRegion(base, commit_size);

  AsanUnpoisonMemoryRegion(base, ARENA_HEADER_SIZE);
  Arena *arena = (Arena *)base;
  arena->prev = nullptr;
  arena->current = arena;
  arena->flags = flags;
  arena->cmt_size = commit_size;
  arena->res_size = reserve_size;
  arena->base_pos = 0;
  arena->pos = ARENA_HEADER_SIZE;
  arena->cmt = commit_size;
  arena->res = reserve_size;
  arena->free_last = nullptr;

  return arena;
}

Arena *ArenaAllocDefault(void)
{
  return ArenaAlloc(nullptr);
}

void ArenaRelease(Arena *arena)
{
  for (Arena *n = arena->current, *prev = nullptr; n != nullptr; n = prev)
  {
    prev = n->prev;
    AsanUnpoisonMemoryRegion(n, n->cmt);
    munmap(n, n->res);
  }
}

void *ArenaPush(Arena *arena, U64 size, U64 align, B32 zero)
{
  Arena *current = arena->current;
  U64 pos_pre = AlignPow2(current->pos, align);
  U64 pos_pst = pos_pre + size;

  U64 size_to_zero = 0;
  if (zero)
  {
    size_to_zero = BASE_MIN(current->cmt, pos_pst) - pos_pre;
  }

  // Chain if needed
  if (current->res < pos_pst && !(arena->flags & ArenaFlag_NoChain))
  {
    Arena *new_block = nullptr;

    // Check free list
    Arena *prev_block;
    for (new_block = arena->free_last, prev_block = nullptr;
         new_block != nullptr;
         prev_block = new_block, new_block = new_block->prev)
    {
      if (new_block->res >= AlignPow2(new_block->pos, align) + size)
      {
        if (prev_block)
        {
          prev_block->prev = new_block->prev;
        }
        else
        {
          arena->free_last = new_block->prev;
        }
        break;
      }
    }

    if (new_block == nullptr)
    {
      ArenaParams params = {0};
      params.flags = current->flags;
      params.reserve_size = current->res_size;
      params.commit_size = current->cmt_size;

      if (size + ARENA_HEADER_SIZE > params.reserve_size)
      {
        params.reserve_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
        params.commit_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
      }

      new_block = ArenaAlloc(&params);
      size_to_zero = 0;
    }
    else
    {
      size_to_zero = size;
    }

    new_block->base_pos = current->base_pos + current->res;
    new_block->prev = current;
    arena->current = new_block;

    current = new_block;
    pos_pre = AlignPow2(current->pos, align);
    pos_pst = pos_pre + size;
  }

  // Commit if needed
  if (current->cmt < pos_pst)
  {
    U64 page_size = GetPageSize();
    U64 cmt_pst_aligned = AlignPow2(pos_pst, page_size);
    U64 cmt_pst_clamped = BASE_MIN(cmt_pst_aligned, current->res);
    U64 cmt_size = cmt_pst_clamped - current->cmt;

    U8 *cmt_ptr = (U8 *)current + current->cmt;
    mprotect(cmt_ptr, cmt_size, PROT_READ | PROT_WRITE);
    AsanPoisonMemoryRegion(cmt_ptr, cmt_size);
    current->cmt = cmt_pst_clamped;
  }

  void *result = nullptr;
  if (current->cmt >= pos_pst)
  {
    result = (U8 *)current + pos_pre;
    current->pos = pos_pst;
    AsanUnpoisonMemoryRegion(result, size);
    if (size_to_zero > 0)
    {
      memset(result, 0, size_to_zero);
    }
  }
  else
  {
    Assert(false); // Out of memory
  }

  return result;
}

U64 ArenaPos(Arena *arena)
{
  Arena *current = arena->current;
  return current->base_pos + current->pos;
}

void ArenaPopTo(Arena *arena, U64 pos)
{
  U64 big_pos = BASE_MAX(ARENA_HEADER_SIZE, pos);
  Arena *current = arena->current;

  for (Arena *prev = nullptr; current->base_pos >= big_pos; current = prev)
  {
    prev = current->prev;
    current->pos = ARENA_HEADER_SIZE;

    // Push onto free list
    current->prev = arena->free_last;
    arena->free_last = current;

    AsanPoisonMemoryRegion((U8 *)current + ARENA_HEADER_SIZE,
                           current->res - ARENA_HEADER_SIZE);
  }

  arena->current = current;
  U64 new_pos = big_pos - current->base_pos;
  Assert(new_pos <= current->pos);
  AsanPoisonMemoryRegion((U8 *)current + new_pos, (current->pos - new_pos));
  current->pos = new_pos;
}

void ArenaClear(Arena *arena)
{
  ArenaPopTo(arena, ARENA_HEADER_SIZE);
}

TempArena TempArenaBegin(Arena *arena)
{
  TempArena temp;
  temp.arena = arena;
  temp.pos = ArenaPos(arena);
  return temp;
}

void TempArenaEnd(TempArena temp)
{
  ArenaPopTo(temp.arena, temp.pos);
}
