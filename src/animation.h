#pragma once
#include "base_arena.h"

// Ozz-animation headers
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"

struct OzzAnimation
{
  ozz::animation::Animation *anim;
  ozz::animation::SamplingJob::Context *cache;
};

enum AnimLayer
{
  LAYER_IDLE = 0,
  LAYER_IDLE_PREV,
  LAYER_WALK,
  LAYER_JOG,
  LAYER_FASTRUN,
  LAYER_JUMP,
  NUM_ANIM_LAYERS
};

enum AnimClip
{
  CLIP_IDLE_1 = 0,
  CLIP_IDLE_2,
  CLIP_IDLE_3,
  CLIP_IDLE_4,
  CLIP_WALK,
  CLIP_JOG,
  CLIP_FASTRUN,
  CLIP_JUMP_RUN,
  CLIP_JUMP_STAND_1,
  CLIP_JUMP_STAND_2,
  CLIP_JUMP_STAND_3,
  NUM_ANIM_CLIPS
};

void LoadAnimation(Arena *arena, const char *path, OzzAnimation &anim_out,
                   ozz::animation::Skeleton *skeleton);
ozz::animation::Skeleton *LoadSkeleton(Arena *arena, const char *path);
