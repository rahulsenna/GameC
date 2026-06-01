#include "animation.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include <new>

void LoadAnimation(Arena *arena, const char *path, OzzAnimation &anim_out,
                   ozz::animation::Skeleton *skeleton)
{
  anim_out.anim = PushStruct(arena, ozz::animation::Animation);
  anim_out.cache = PushStruct(arena, ozz::animation::SamplingJob::Context);
  new (anim_out.anim) ozz::animation::Animation();
  new (anim_out.cache) ozz::animation::SamplingJob::Context();

  ozz::io::File file(path, "rb");
  if (file.opened())
  {
    ozz::io::IArchive archive(&file);
    if (archive.TestTag<ozz::animation::Animation>())
    {
      archive >> *anim_out.anim;
    }
  }
  anim_out.cache->Resize(skeleton->num_joints());
}

ozz::animation::Skeleton *LoadSkeleton(Arena *arena, const char *path)
{
  ozz::animation::Skeleton *skeleton =
      PushStruct(arena, ozz::animation::Skeleton);
  new (skeleton) ozz::animation::Skeleton();

  ozz::io::File file(path, "rb");
  if (file.opened())
  {
    ozz::io::IArchive archive(&file);
    if (archive.TestTag<ozz::animation::Skeleton>())
    {
      archive >> *skeleton;
    }
  }
  return skeleton;
}
