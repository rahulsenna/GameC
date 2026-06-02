#include "game.h"
#include "math_utils.h"
#include "shapes.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void *PushUploadTextureCommand(RenderGroup *group, U32 handle, U32 width,
                               U32 height, U32 format, U32 num_mips,
                               U32 data_size)
{
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_UploadTexture) + data_size;
  if (group->size + total_size > group->max_size)
  {
    printf("ASSERT FAILURE: size=%u, total_size=%u, max_size=%u, w=%u, h=%u\n",
           group->size, total_size, group->max_size, width, height);
  }
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_UploadTexture;

  RenderGroupEntry_UploadTexture *entry =
      (RenderGroupEntry_UploadTexture *)(header + 1);
  entry->handle = handle;
  entry->width = width;
  entry->height = height;
  entry->format = format;
  entry->num_mips = num_mips;
  entry->data_size = data_size;

  void *dst_pixels = (void *)(entry + 1);
  group->size += total_size;
  return dst_pixels;
}

void *PushUploadGeometryCommand(RenderGroup *group, GpuPtr offset, U32 size)
{
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_UploadGeometry) + size;
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_UploadGeometry;

  RenderGroupEntry_UploadGeometry *entry =
      (RenderGroupEntry_UploadGeometry *)(header + 1);
  entry->offset = offset;
  entry->size = size;

  void *dst_data = (void *)(entry + 1);
  group->size += total_size;
  return dst_data;
}

void EvaluateBoneMatrices(FBXNode *node, FBXModel *model,
                          Mat4 *out_bone_matrices,
                          Vec3 current_root_translation)
{
  for (U32 b = 0; b < node->num_bones; b++)
  {
    U16 ozz_j = node->ozz_joint_mapping[b];
    const ozz::math::Float4x4 &ozz_mat = model->model_matrices[ozz_j];
    const float *m = reinterpret_cast<const float *>(&ozz_mat);

    Mat4 eval_geom = {};
    eval_geom.columns[0] = {m[0], m[1], m[2], m[3]};
    eval_geom.columns[1] = {m[4], m[5], m[6], m[7]};
    eval_geom.columns[2] = {m[8], m[9], m[10], m[11]};
    eval_geom.columns[3] = {m[12] * 100.f, m[13] * 100.f, m[14] * 100.f, m[15]};

    eval_geom.columns[3].x -= current_root_translation.x * 100.f;
    eval_geom.columns[3].y -= current_root_translation.y * 100.f;
    eval_geom.columns[3].z -= current_root_translation.z * 100.f;

    out_bone_matrices[b] = eval_geom * node->inverse_bind_matrices[b];
  }
}

void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         MaterialTextures textures, U32 shader_type,
                         U32 vertex_count, GpuPtr vertex_offset,
                         Vec3 bounds_center, F32 bounds_radius,
                         const Mat4 *bone_matrices)
{
  // The DrawMesh entry is followed by MAX_BONES * sizeof(Mat4) of bone matrix
  // data when has_bones is set.  The renderer will bump-allocate these into
  // the current frame arena and patch uniforms.bone_matrix_offset before draw.
  U32 bone_data_size =
      (bone_matrices && uniforms.has_bones) ? MAX_BONES * sizeof(Mat4) : 0;
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_DrawMesh) + bone_data_size;
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_DrawMesh;

  RenderGroupEntry_DrawMesh *entry = (RenderGroupEntry_DrawMesh *)(header + 1);
  entry->uniforms = uniforms;
  entry->uniforms.vertex_offset = vertex_offset / sizeof(Vertex);
  entry->uniforms.albedo_tex = textures.albedo;
  entry->uniforms.normal_tex = textures.normal;
  entry->uniforms.metallic_tex = textures.metallic;
  entry->uniforms.roughness_tex = textures.roughness;
  entry->uniforms.ao_tex = textures.ao;

  entry->shader_type = shader_type;
  entry->vertex_count = vertex_count;
  entry->vertex_offset = vertex_offset;
  entry->vertex_count = vertex_count;
  entry->vertex_offset = vertex_offset;

  Vec4 center4 = {bounds_center.x, bounds_center.y, bounds_center.z, 1.0f};
  center4 = uniforms.model_matrix * center4;
  entry->bounds_center = {center4.x, center4.y, center4.z};

  float sx2 =
      uniforms.model_matrix.columns[0].x * uniforms.model_matrix.columns[0].x +
      uniforms.model_matrix.columns[0].y * uniforms.model_matrix.columns[0].y +
      uniforms.model_matrix.columns[0].z * uniforms.model_matrix.columns[0].z;
  float sy2 =
      uniforms.model_matrix.columns[1].x * uniforms.model_matrix.columns[1].x +
      uniforms.model_matrix.columns[1].y * uniforms.model_matrix.columns[1].y +
      uniforms.model_matrix.columns[1].z * uniforms.model_matrix.columns[1].z;
  float sz2 =
      uniforms.model_matrix.columns[2].x * uniforms.model_matrix.columns[2].x +
      uniforms.model_matrix.columns[2].y * uniforms.model_matrix.columns[2].y +
      uniforms.model_matrix.columns[2].z * uniforms.model_matrix.columns[2].z;
  float max_scale = sqrtf(fmaxf(sx2, fmaxf(sy2, sz2)));
  entry->bounds_radius = bounds_radius * max_scale;

  if (bone_data_size)
  {
    // Copy all MAX_BONES slots so the renderer doesn't read uninitialised
    // memory when uploading the full block to the frame arena.
    memcpy((Mat4 *)(entry + 1), bone_matrices, bone_data_size);
  }

  group->size += total_size;
}

// LoadTexture now delegates to the cooked binary loader.
// Kept as a convenience wrapper for backward compatibility.
U32 LoadTexture(const char *path, RenderGroup *render_group,
                U32 *next_tex_handle)
{
  return LoadCookedTexture(path, render_group, next_tex_handle);
}

void PushDrawDynamicMeshCommand(RenderGroup *group, Uniforms uniforms,
                                MaterialTextures textures, U32 shader_type,
                                U32 vertex_count, const Vertex *vertices,
                                Vec3 bounds_center, F32 bounds_radius)
{
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_DrawDynamicMesh) +
                   vertex_count * sizeof(Vertex);
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_DrawDynamicMesh;

  RenderGroupEntry_DrawDynamicMesh *entry =
      (RenderGroupEntry_DrawDynamicMesh *)(header + 1);
  entry->uniforms = uniforms;
  entry->uniforms.albedo_tex = textures.albedo;
  entry->uniforms.normal_tex = textures.normal;
  entry->uniforms.metallic_tex = textures.metallic;
  entry->uniforms.roughness_tex = textures.roughness;
  entry->uniforms.ao_tex = textures.ao;
  entry->shader_type = shader_type;
  entry->vertex_count = vertex_count;
  entry->vertex_count = vertex_count;

  Vec4 center4 = {bounds_center.x, bounds_center.y, bounds_center.z, 1.0f};
  center4 = uniforms.model_matrix * center4;
  entry->bounds_center = {center4.x, center4.y, center4.z};

  float sx2 =
      uniforms.model_matrix.columns[0].x * uniforms.model_matrix.columns[0].x +
      uniforms.model_matrix.columns[0].y * uniforms.model_matrix.columns[0].y +
      uniforms.model_matrix.columns[0].z * uniforms.model_matrix.columns[0].z;
  float sy2 =
      uniforms.model_matrix.columns[1].x * uniforms.model_matrix.columns[1].x +
      uniforms.model_matrix.columns[1].y * uniforms.model_matrix.columns[1].y +
      uniforms.model_matrix.columns[1].z * uniforms.model_matrix.columns[1].z;
  float sz2 =
      uniforms.model_matrix.columns[2].x * uniforms.model_matrix.columns[2].x +
      uniforms.model_matrix.columns[2].y * uniforms.model_matrix.columns[2].y +
      uniforms.model_matrix.columns[2].z * uniforms.model_matrix.columns[2].z;
  float max_scale = sqrtf(fmaxf(sx2, fmaxf(sy2, sz2)));
  entry->bounds_radius = bounds_radius * max_scale;

  Vertex *dst_verts = (Vertex *)(entry + 1);
  memcpy(dst_verts, vertices, vertex_count * sizeof(Vertex));

  group->size += total_size;
}

Font LoadCookedFont(Arena *arena, const char *path, RenderGroup *render_group,
                    U32 *next_tex_handle)
{
  Font font = {};
  FILE *f = fopen(path, "rb");
  if (!f)
    return font;

  CookedFontHeader header;
  fread(&header, sizeof(header), 1, f);

  if (header.magic != FONT_MAGIC || header.version != FONT_VERSION)
  {
    fclose(f);
    return font;
  }

  font.line_height = header.line_height;
  font.num_glyphs = header.num_glyphs;
  font.glyphs = PushArray(arena, CookedGlyph, header.num_glyphs);
  fread(font.glyphs, sizeof(CookedGlyph), header.num_glyphs, f);

  CookedTexFileHeader tex_header;
  fread(&tex_header, sizeof(tex_header), 1, f);

  U32 tex_size = tex_header.width * tex_header.height; // R8Unorm
  U32 handle = (*next_tex_handle)++;
  void *dst_pixels =
      PushUploadTextureCommand(render_group, handle, tex_header.width,
                               tex_header.height, 3, 1, tex_size);
  fread(dst_pixels, 1, tex_size, f);

  fclose(f);
  font.texture_handle = handle;
  return font;
}

void PushDrawTextCommand(RenderGroup *group, Font *font, Uniforms base_uniforms,
                         const char *text, Vec3 position, float scale)
{
  if (!font->texture_handle || !text)
    return;

  U32 len = strlen(text);
  if (len == 0)
    return;

  if (len > 1024)
    len = 1024;
  Vertex verts[1024 * 6];
  U32 v_count = 0;

  float start_x = position.x;
  float start_y = position.y;
  float z = position.z;

  for (U32 i = 0; i < len; ++i)
  {
    unsigned char c = text[i];
    if (c >= 32 && c < 128)
    {
      CookedGlyph &g = font->glyphs[c - 32];

      float x0 = start_x + g.x0 * scale;
      float y0 = start_y - g.y0 * scale;
      float x1 = start_x + g.x1 * scale;
      float y1 = start_y - g.y1 * scale;

      float u0 = g.u0;
      float v0 = g.v0;
      float u1 = g.u1;
      float v1 = g.v1;

      Vertex v[4] = {};
      v[0].position = {x0, y0, z};
      v[0].tex_coord = {u0, v0};
      v[1].position = {x1, y0, z};
      v[1].tex_coord = {u1, v0};
      v[2].position = {x1, y1, z};
      v[2].tex_coord = {u1, v1};
      v[3].position = {x0, y1, z};
      v[3].tex_coord = {u0, v1};

      for (int j = 0; j < 4; j++)
      {
        v[j].normal = {0, 0, 1};
        v[j].tangent = {1, 0, 0};
      }

      verts[v_count++] = v[0]; // TL
      verts[v_count++] = v[3]; // BL
      verts[v_count++] = v[2]; // BR

      verts[v_count++] = v[0]; // TL
      verts[v_count++] = v[2]; // BR
      verts[v_count++] = v[1]; // TR

      start_x += g.advance * scale;
    }
    else if (c == '\n')
    {
      start_x = position.x;
      start_y -= font->line_height * scale;
    }
  }

  if (v_count > 0)
  {
    MaterialTextures tex = {};
    tex.albedo = font->texture_handle;
    PushDrawDynamicMeshCommand(group, base_uniforms, tex, 2, v_count, verts,
                               Vec3{0, 0, 0}, 1e6f);
  }
}

static void UploadModelGeometry(GameState *state, RenderGroup *render_group,
                                FBXModel *model)
{
  for (U32 i = 0; i < model->num_nodes; ++i)
  {
    FBXNode *node = &model->nodes[i];
    if (node->vertex_count == 0)
      continue;
    U32 byte_size = node->vertex_count * sizeof(Vertex);
    node->vertex_offset = gpuMalloc(&state->gpu_allocator, byte_size);

    void *dst =
        PushUploadGeometryCommand(render_group, node->vertex_offset, byte_size);
    memcpy(dst, node->vertices, byte_size);
  }
}

extern "C" void GameUpdateAndRender(Arena *arena, GameInput *input, float dt,
                                    GameOutput *out_output)
{
  if (arena->pos == ARENA_HEADER_SIZE)
  {
    PushStruct(arena, GameState);
  }

  GameState *state = (GameState *)((U8 *)arena + ARENA_HEADER_SIZE);
  out_output->render_group.size = 0;

  if (!state->is_initialized)
  {
    state->is_initialized = 1;
    state->gpu_allocator.capacity = 512 * 1024 * 1024; // 512 MB
    // The first GPU_FRAME_ARENA_TOTAL bytes are reserved for triple-buffered
    // per-frame arenas managed by the renderer.  All permanent geometry
    // allocations start after that region.
    state->gpu_allocator.used = GPU_FRAME_ARENA_TOTAL;
    state->time = 0.0f;
    U32 next_tex_handle = 1;

    // Default textures
    U32 default_albedo = next_tex_handle++;
    void *albedo_dst = PushUploadTextureCommand(&out_output->render_group,
                                                default_albedo, 1, 1, 0, 1, 4);
    *(U32 *)albedo_dst = 0xFFFFFFFF; // White

    U32 default_normal = next_tex_handle++;
    void *normal_dst = PushUploadTextureCommand(&out_output->render_group,
                                                default_normal, 1, 1, 0, 1, 4);
    *(U32 *)normal_dst = 0xFFFF8080; // Flat normal

    U32 default_metallic = next_tex_handle++;
    void *metallic_dst = PushUploadTextureCommand(
        &out_output->render_group, default_metallic, 1, 1, 0, 1, 4);
    *(U32 *)metallic_dst = 0xFF000000; // Black

    U32 default_roughness = next_tex_handle++;
    void *roughness_dst = PushUploadTextureCommand(
        &out_output->render_group, default_roughness, 1, 1, 0, 1, 4);
    *(U32 *)roughness_dst = 0xFFFFFFFF; // White

    U32 default_ao = next_tex_handle++;
    void *ao_dst = PushUploadTextureCommand(&out_output->render_group,
                                            default_ao, 1, 1, 0, 1, 4);
    *(U32 *)ao_dst = 0xFFFFFFFF; // White

    MaterialTextures default_textures = {};
    default_textures.albedo = default_albedo;
    default_textures.normal = default_normal;
    default_textures.metallic = default_metallic;
    default_textures.roughness = default_roughness;
    default_textures.ao = default_ao;

    U32 green_albedo = next_tex_handle++;
    void *green_albedo_dst = PushUploadTextureCommand(
        &out_output->render_group, green_albedo, 1, 1, 0, 1, 4);
    *(U32 *)green_albedo_dst = 0xFF267326; // ABGR -> A=FF, B=26, G=73, R=26

    MaterialTextures green_textures = default_textures;
    green_textures.albedo = green_albedo;

    state->camera.position = Vec3{0.0f, 2.0f, 5.0f};
    state->camera.yaw = -90.0f; // Look down -Z
    state->camera.pitch = 0.0f;
    state->is_third_person = 0;
    state->prev_key_p = 0;
    state->player.position = Vec3{0.0f, -0.5f, -2.5f};
    state->player.yaw = 0.0f;
    state->player.speed_param = 0.0f;
    state->player.sticky_speed_mode = 0;
    state->player.last_shift_time = 0.0f;
    state->player.was_shift_down = 0;
    state->player.is_jumping = 0;
    state->player.jump_anim_time = 0.0f;
    state->player.jump_duration = 0.0f;
    state->player.current_jump_anim_index = 0;
    state->player.anim_time = 0.0f;
    state->player.current_idle_anim_index = 0;
    state->player.prev_idle_anim_index = 0;
    state->player.idle_crossfade_time = 0.0f;

    for (int i = 0; i < NUM_ANIM_LAYERS; i++)
    {
      state->player.prev_root_trans[i] = {0, 0, 0};
    }

    // Load checker texture from cooked assets
    default_textures.albedo =
        LoadCookedTexture("assets_cooked/CustomUVChecker_byValle_1K.tex",
                          &out_output->render_group, &next_tex_handle);

    // Generate Shapes
    state->num_models = 12;
    state->models = PushArray(arena, FBXModel, state->num_models);

    MaterialTextures default_textures_local = default_textures;

    // Load alien panels textures from cooked ORM-packed set
    MaterialTextures alien_textures_local = {};
    alien_textures_local.albedo = LoadCookedTexture(
        "assets_cooked/alien-panels-bl/alien-panels-bl_albedo.tex",
        &out_output->render_group, &next_tex_handle);
    alien_textures_local.normal = LoadCookedTexture(
        "assets_cooked/alien-panels-bl/alien-panels-bl_normal.tex",
        &out_output->render_group, &next_tex_handle);
    // ORM packed texture: R=AO, G=Roughness, B=Metallic
    // For now we load the ORM as the metallic slot; the shader still
    // samples separate channels, so this is a first-pass integration.
    U32 orm_handle = LoadCookedTexture(
        "assets_cooked/alien-panels-bl/alien-panels-bl_orm.tex",
        &out_output->render_group, &next_tex_handle);
    alien_textures_local.metallic = orm_handle;
    alien_textures_local.roughness = orm_handle;
    alien_textures_local.ao = orm_handle;

    state->models[0] = CreateSphere(arena);
    state->models[0].nodes[0].textures = alien_textures_local;

    state->models[1] = CreateTorus(arena);
    state->models[1].nodes[0].textures = default_textures_local;

    state->models[2] = CreateCylinder(arena);
    state->models[2].nodes[0].textures = default_textures_local;

    state->models[3] = CreateCone(arena);
    state->models[3].nodes[0].textures = default_textures_local;

    state->models[4] = CreateCube(arena);
    state->models[4].nodes[0].textures = default_textures_local;

    state->models[5] = CreateCuboid(arena, 1.5f, 0.5f, 1.0f);
    state->models[5].nodes[0].textures = default_textures_local;

    state->models[6] = CreateTriangularPyramid(arena);
    state->models[6].nodes[0].textures = default_textures_local;

    state->models[7] = CreateSquarePyramid(arena);
    state->models[7].nodes[0].textures = default_textures_local;

    state->models[8] = CreateTriangularPrism(arena);
    state->models[8].nodes[0].textures = default_textures_local;

    state->models[9] = CreatePlane(arena, 1000.0f);
    state->models[9].nodes[0].textures = green_textures;

    state->models[10] = LoadCookedMesh(
        arena, "assets_cooked/Sophie.mesh", &out_output->render_group,
        &next_tex_handle, default_textures_local);
    state->models[11] = LoadCookedMesh(
        arena, "assets_cooked/banana_leaves.mesh", &out_output->render_group,
        &next_tex_handle, default_textures_local);

    state->main_font =
        LoadCookedFont(arena, "assets_cooked/manifoldcf-regular.font",
                       &out_output->render_group, &next_tex_handle);

    for (U32 i = 0; i < state->num_models; ++i)
    {
      UploadModelGeometry(state, &out_output->render_group, &state->models[i]);
    }

    // Initialize ozz types
    state->models[10].ozz_skeleton =
        LoadSkeleton(arena, "assets_cooked/animations/Sophie_skeleton.ozz");
    state->models[10].has_animation = 1;

    auto sophie_skeleton = state->models[10].ozz_skeleton;

    LoadAnimation(arena, "assets_cooked/animations/Idle_animation.ozz",
                  state->player.anim_clips[CLIP_IDLE_1], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/Idle-2_animation.ozz",
                  state->player.anim_clips[CLIP_IDLE_2], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/Idle-3_animation.ozz",
                  state->player.anim_clips[CLIP_IDLE_3], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/Standing Idle_animation.ozz",
                  state->player.anim_clips[CLIP_IDLE_4], sophie_skeleton);

    LoadAnimation(arena, "assets_cooked/animations/WalkingFemale_animation.ozz",
                  state->player.anim_clips[CLIP_WALK], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/Jogging_animation.ozz",
                  state->player.anim_clips[CLIP_JOG], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/FastRun_animation.ozz",
                  state->player.anim_clips[CLIP_FASTRUN], sophie_skeleton);

    LoadAnimation(arena, "assets_cooked/animations/RunningJump_animation.ozz",
                  state->player.anim_clips[CLIP_JUMP_RUN], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/StandingJump1_animation.ozz",
                  state->player.anim_clips[CLIP_JUMP_STAND_1], sophie_skeleton);
    LoadAnimation(arena, "assets_cooked/animations/StandingJump2_animation.ozz",
                  state->player.anim_clips[CLIP_JUMP_STAND_2], sophie_skeleton);
    LoadAnimation(arena,
                  "assets_cooked/animations/StandingJumpHigher_animation.ozz",
                  state->player.anim_clips[CLIP_JUMP_STAND_3], sophie_skeleton);

    state->models[10].num_soa_joints =
        state->models[10].ozz_skeleton->num_soa_joints();

    for (int i = 0; i < NUM_ANIM_LAYERS; i++)
    {
      state->player.local_layers[i] = PushArray(
          arena, ozz::math::SoaTransform, state->models[10].num_soa_joints);
    }
    state->player.blended_locals = PushArray(arena, ozz::math::SoaTransform,
                                             state->models[10].num_soa_joints);
    state->models[10].model_matrices =
        PushArray(arena, ozz::math::Float4x4,
                  state->models[10].ozz_skeleton->num_joints());

    // Build joint mapping for Sophie model (model 10) against WalkingFemale
    // skeleton (model 10)
    for (U32 n = 0; n < state->models[10].num_nodes; n++)
    {
      FBXNode *node = &state->models[10].nodes[n];
      node->ozz_joint_mapping = PushArray(arena, U16, node->num_bones);
      for (U32 b = 0; b < node->num_bones; b++)
      {
        node->ozz_joint_mapping[b] = 0;
        // bone_nodes[b] now points to a char* bone name string
        // (from cooked asset loader)
        const char *bone_name = (const char *)node->bone_nodes[b];
        auto joint_names = state->models[10].ozz_skeleton->joint_names();
        for (int j = 0; j < state->models[10].ozz_skeleton->num_joints(); j++)
        {
          const char *joint_name = joint_names[j];
          if (strcmp(bone_name, joint_name) == 0)
          {
            node->ozz_joint_mapping[b] = j;
            break;
          }
        }
      }
    }
  }

  state->time += dt;

  // --- Camera Update ---
  if (input->key_p && !state->prev_key_p)
  {
    state->is_third_person = !state->is_third_person;
  }
  state->prev_key_p = input->key_p;

  float move_speed = 3.0f * dt;
  float look_speed = 90.0f * dt; // degrees per second

  Vec3 front = {0, 0, 0};

  if (state->is_third_person)
  {
    if (input->key_up)
      state->camera.pitch += look_speed;
    if (input->key_down)
      state->camera.pitch -= look_speed;
    if (input->key_left)
      state->camera.yaw -= look_speed;
    if (input->key_right)
      state->camera.yaw += look_speed;

    if (state->camera.pitch > 89.0f)
      state->camera.pitch = 89.0f;
    if (state->camera.pitch < -89.0f)
      state->camera.pitch = -89.0f;

    front.x = cosf(state->camera.yaw * (M_PI / 180.0f)) *
              cosf(state->camera.pitch * (M_PI / 180.0f));
    front.y = sinf(state->camera.pitch * (M_PI / 180.0f));
    front.z = sinf(state->camera.yaw * (M_PI / 180.0f)) *
              cosf(state->camera.pitch * (M_PI / 180.0f));
    front = math_normalize(front);

    Vec3 forward = math_normalize(Vec3{front.x, 0.0f, front.z});
    Vec3 right = math_normalize(math_cross(forward, Vec3{0.0f, 1.0f, 0.0f}));

    Vec3 movement = {0, 0, 0};
    if (input->key_w)
      movement += forward;
    if (input->key_s)
      movement -= forward;
    if (input->key_a)
      movement -= right;
    if (input->key_d)
      movement += right;

    state->total_time += dt;

    bool is_moving =
        (input->key_w || input->key_s || input->key_a || input->key_d);

    if (!is_moving)
    {
      state->player.sticky_speed_mode = 0;
    }
    else
    {
      if (input->key_shift && !state->player.was_shift_down)
      {
        if (state->total_time - state->player.last_shift_time < 0.4f)
        {
          state->player.sticky_speed_mode = 2; // FastRun
        }
        else
        {
          state->player.sticky_speed_mode = 1; // Jog
        }
        state->player.last_shift_time = state->total_time;
      }
    }
    state->player.was_shift_down = input->key_shift;

    float target_speed_param = 0.0f;
    if (movement.x != 0.0f || movement.z != 0.0f)
    {
      movement = math_normalize(movement);
      state->player.yaw = atan2f(movement.x, movement.z) * (180.0f / M_PI);

      if (state->player.sticky_speed_mode == 2)
        target_speed_param = 3.0f; // FastRun
      else if (state->player.sticky_speed_mode == 1)
        target_speed_param = 2.0f; // Jog
      else
        target_speed_param = 1.0f; // Walk
    }
    state->player.speed_param +=
        (target_speed_param - state->player.speed_param) * 10.0f * dt;

    if (input->key_space && !state->player.is_jumping)
    {
      state->player.is_jumping = 1;
      state->player.jump_anim_time = 0.0f;
      if (state->player.speed_param < 0.5f)
      {
        state->player.current_jump_anim_index = 1 + (rand() % 3);
      }
      else
      {
        state->player.current_jump_anim_index = 0;
      }

      ozz::animation::Animation *anim = nullptr;
      if (state->player.current_jump_anim_index == 0)
        anim = state->player.anim_clips[CLIP_JUMP_RUN].anim;
      else if (state->player.current_jump_anim_index == 1)
        anim = state->player.anim_clips[CLIP_JUMP_STAND_1].anim;
      else if (state->player.current_jump_anim_index == 2)
        anim = state->player.anim_clips[CLIP_JUMP_STAND_2].anim;
      else if (state->player.current_jump_anim_index == 3)
        anim = state->player.anim_clips[CLIP_JUMP_STAND_3].anim;

      state->player.jump_duration = anim->duration();
    }

    Vec3 target = state->player.position + Vec3{0, 1.0f, 0};
    state->camera.position = target - front * 4.0f;
  }
  else
  {
    if (input->key_up)
      state->camera.pitch += look_speed;
    if (input->key_down)
      state->camera.pitch -= look_speed;
    if (input->key_left)
      state->camera.yaw -= look_speed;
    if (input->key_right)
      state->camera.yaw += look_speed;

    if (state->camera.pitch > 89.0f)
      state->camera.pitch = 89.0f;
    if (state->camera.pitch < -89.0f)
      state->camera.pitch = -89.0f;

    front.x = cosf(state->camera.yaw * (M_PI / 180.0f)) *
              cosf(state->camera.pitch * (M_PI / 180.0f));
    front.y = sinf(state->camera.pitch * (M_PI / 180.0f));
    front.z = sinf(state->camera.yaw * (M_PI / 180.0f)) *
              cosf(state->camera.pitch * (M_PI / 180.0f));
    front = math_normalize(front);

    Vec3 up = Vec3{0.0f, 1.0f, 0.0f};
    Vec3 right = math_normalize(math_cross(front, up));

    if (input->key_w)
      state->camera.position += front * move_speed;
    if (input->key_s)
      state->camera.position -= front * move_speed;
    if (input->key_a)
      state->camera.position -= right * move_speed;
    if (input->key_d)
      state->camera.position += right * move_speed;
  }

  // --- Render ---
  PushClearCommand(&out_output->render_group, 0.1f, 0.1f, 0.1f, 1.0f);

  Vec3 up = Vec3{0.0f, 1.0f, 0.0f};

  Mat4 view_matrix = math_make_look_at(state->camera.position,
                                       state->camera.position + front, up);

  float fov = 60.0f * (3.14159f / 180.0f);
  float aspect = 2880.0f / 1864.0f;
  Mat4 proj_matrix = math_make_perspective(fov, aspect, 0.1f, 100.0f);

  Mat4 vp_matrix = proj_matrix * view_matrix;

  // --- Common Scene Uniforms ---
  Uniforms base_uniforms = {};
  base_uniforms.light_dir = math_normalize(Vec3{1.0f, 1.0f, 1.0f});
  base_uniforms.light_color = Vec3{1.0f, 1.0f, 1.0f};
  base_uniforms.light_color = Vec3{1.0f, 1.0f, 1.0f};
  base_uniforms.camera_pos = state->camera.position;
  base_uniforms.camera_front = front;
  base_uniforms.ambient_intensity = 0.2f;

  float cascade_ends[4] = {0.1f, 15.0f, 45.0f, 120.0f};
  base_uniforms.cascade_splits = {cascade_ends[1], cascade_ends[2],
                                  cascade_ends[3], 0.0f};

  for (int c = 0; c < 3; ++c)
  {
    float n = cascade_ends[c];
    float f = cascade_ends[c + 1];

    float h_near = tanf(fov * 0.5f) * n;
    float w_near = h_near * aspect;
    float h_far = tanf(fov * 0.5f) * f;
    float w_far = h_far * aspect;

    Vec3 right = math_normalize(math_cross(front, up));
    Vec3 cam_up = math_cross(right, front);

    Vec3 center_near = state->camera.position + front * n;
    Vec3 center_far = state->camera.position + front * f;

    Vec3 corners[8] = {center_near + (cam_up * h_near) - (right * w_near),
                       center_near + (cam_up * h_near) + (right * w_near),
                       center_near - (cam_up * h_near) - (right * w_near),
                       center_near - (cam_up * h_near) + (right * w_near),
                       center_far + (cam_up * h_far) - (right * w_far),
                       center_far + (cam_up * h_far) + (right * w_far),
                       center_far - (cam_up * h_far) - (right * w_far),
                       center_far - (cam_up * h_far) + (right * w_far)};

    Vec3 frustum_center = {0, 0, 0};
    for (int i = 0; i < 8; i++)
      frustum_center += corners[i];
    frustum_center = frustum_center / 8.0f;

    float max_dist_sq = 0;
    for (int i = 0; i < 8; i++)
    {
      float dx = corners[i].x - frustum_center.x;
      float dy = corners[i].y - frustum_center.y;
      float dz = corners[i].z - frustum_center.z;
      float d_sq = dx * dx + dy * dy + dz * dz;
      if (d_sq > max_dist_sq)
        max_dist_sq = d_sq;
    }
    float radius = sqrtf(max_dist_sq);

    Vec3 light_pos = frustum_center + base_uniforms.light_dir * radius;
    Mat4 light_view =
        math_make_look_at(light_pos, frustum_center, Vec3{0, 1, 0});

    // Texel snapping to fix shimmering
    float shadow_map_res = 4096.0f;
    float texel_size = (radius * 2.0f) / shadow_map_res;

    Vec4 center_ls = light_view * Vec4{frustum_center.x, frustum_center.y,
                                       frustum_center.z, 1.0f};

    float snapped_x = floorf(center_ls.x / texel_size) * texel_size;
    float snapped_y = floorf(center_ls.y / texel_size) * texel_size;

    float dx = snapped_x - center_ls.x;
    float dy = snapped_y - center_ls.y;

    Vec3 light_right = {light_view.columns[0].x, light_view.columns[1].x,
                        light_view.columns[2].x};
    Vec3 light_up = {light_view.columns[0].y, light_view.columns[1].y,
                     light_view.columns[2].y};

    // Shift camera and target to matched snapped grid
    light_pos = light_pos - (light_right * dx) - (light_up * dy);
    frustum_center = frustum_center - (light_right * dx) - (light_up * dy);

    // Rebuild view
    light_view = math_make_look_at(light_pos, frustum_center, Vec3{0, 1, 0});

    Mat4 light_proj = math_make_orthographic(-radius, radius, -radius, radius,
                                             -radius * 5.0f, radius * 2.0f);

    base_uniforms.light_vp_matrices[c] = light_proj * light_view;
  }

  // 1. Draw Infinite Plane
  {
    Mat4 model_matrix = Mat4{
        Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0},
        Vec4{0, -0.5f, 0, 1} // Set plane to ground level (-0.5 is the exact
                             // bottom bound for most of our shapes)
    };
    Mat4 mvp_matrix = vp_matrix * model_matrix;

    Uniforms uniforms = base_uniforms;
    uniforms.mvp_matrix = mvp_matrix;
    uniforms.model_matrix = model_matrix;

    for (U32 n = 0; n < state->models[9].num_nodes; n++)
    {
      FBXNode *node = &state->models[9].nodes[n];
      // Draw solid PBR floor
      PushDrawMeshCommand(&out_output->render_group, uniforms, node->textures,
                          0, node->vertex_count, node->vertex_offset,
                          node->bounds_center, node->bounds_radius);
    }
  }

  // 1.5 Draw Infinite Grid Lines on top of floor
  {
    Mat4 model_matrix = Mat4{
        Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0},
        Vec4{0, -0.49f, 0, 1} // Slightly above the floor to avoid z-fighting
    };
    Mat4 mvp_matrix = vp_matrix * model_matrix;

    Uniforms uniforms = base_uniforms;
    uniforms.mvp_matrix = mvp_matrix;
    uniforms.model_matrix = model_matrix;

    for (U32 n = 0; n < state->models[9].num_nodes; n++)
    {
      FBXNode *node = &state->models[9].nodes[n];
      // Draw grid lines
      PushDrawMeshCommand(&out_output->render_group, uniforms, node->textures,
                          1, node->vertex_count, node->vertex_offset,
                          node->bounds_center, node->bounds_radius);
    }
  }

  // 2. Draw 9 Shapes
  float shape_y_offsets[9] = {
      0.0f,   // 0: Sphere (bottom at -0.5)
      -0.3f,  // 1: Torus (bottom at -0.2 -> needs to move down 0.3 to rest at
              // -0.5)
      0.0f,   // 2: Cylinder (bottom at -0.5)
      0.0f,   // 3: Cone (bottom at -0.5)
      0.0f,   // 4: Cube (bottom at -0.5)
      -0.25f, // 5: Cuboid (height 0.5 -> bottom at -0.25 -> needs to move down
              // 0.25 to rest at -0.5)
      0.0f,   // 6: TriangularPyramid (bottom at -0.5)
      0.0f,   // 7: SquarePyramid (bottom at -0.5)
      0.0f    // 8: TriangularPrism (bottom at -0.5)
  };

  for (int i = 0; i < 9; ++i)
  {
    float x = (i % 3) * 2.5f - 2.5f;
    float z = (i / 3) * 2.5f - 2.5f;
    float y = shape_y_offsets[i];
    Mat4 trans_matrix = Mat4{Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0},
                             Vec4{0, 0, 1, 0}, Vec4{x, y, z, 1}};

    float angle = state->time * 0.5f + i;
    Mat4 rot_y = Mat4{Vec4{cosf(angle), 0, -sinf(angle), 0}, Vec4{0, 1, 0, 0},
                      Vec4{sinf(angle), 0, cosf(angle), 0}, Vec4{0, 0, 0, 1}};

    Mat4 model_matrix = trans_matrix * rot_y;
    Mat4 mvp_matrix = vp_matrix * model_matrix;

    Uniforms uniforms = base_uniforms;
    uniforms.mvp_matrix = mvp_matrix;
    uniforms.model_matrix = model_matrix;

    for (U32 n = 0; n < state->models[i].num_nodes; n++)
    {
      FBXNode *node = &state->models[i].nodes[n];
      PushDrawMeshCommand(&out_output->render_group, uniforms, node->textures,
                          0, node->vertex_count, node->vertex_offset,
                          node->bounds_center, node->bounds_radius);
    }
  }

  // 3. Draw FBX Model in front of the shapes (closer to camera)
  {
    Mat4 trans_matrix =
        Mat4{Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0},
             Vec4{state->player.position.x, state->player.position.y,
                  state->player.position.z, 1}};

    float yaw_rad = state->player.yaw * (M_PI / 180.0f);
    Mat4 rot_y =
        Mat4{Vec4{cosf(yaw_rad), 0, -sinf(yaw_rad), 0}, Vec4{0, 1, 0, 0},
             Vec4{sinf(yaw_rad), 0, cosf(yaw_rad), 0}, Vec4{0, 0, 0, 1}};

    float s = 0.01f;
    Mat4 scale_matrix = Mat4{Vec4{s, 0, 0, 0}, Vec4{0, s, 0, 0},
                             Vec4{0, 0, s, 0}, Vec4{0, 0, 0, 1}};

    Mat4 rot_scale = rot_y * scale_matrix;
    Mat4 model_matrix = trans_matrix * rot_scale;
    Mat4 mvp_matrix = vp_matrix * model_matrix;

    Uniforms uniforms = base_uniforms;
    uniforms.mvp_matrix = mvp_matrix;
    uniforms.model_matrix = model_matrix;

    Vec3 current_root_translation = {0.0f, 0.0f, 0.0f};
    if (state->models[10].has_animation)
    {
      auto *skel = state->models[10].ozz_skeleton;

      OzzAnimation &active_idle =
          state->player
              .anim_clips[CLIP_IDLE_1 + state->player.current_idle_anim_index];

      float duration_idle = active_idle.anim->duration();
      float duration_walk =
          state->player.anim_clips[CLIP_WALK].anim->duration();
      float duration_jog = state->player.anim_clips[CLIP_JOG].anim->duration();
      float duration_fast =
          state->player.anim_clips[CLIP_FASTRUN].anim->duration();

      float s = state->player.speed_param;
      float w_idle = 0.0f, w_walk = 0.0f, w_jog = 0.0f, w_fast = 0.0f;
      if (s < 1.0f)
      {
        w_walk = s;
        w_idle = 1.0f - s;
      }
      else if (s < 2.0f)
      {
        w_jog = s - 1.0f;
        w_walk = 1.0f - w_jog;
      }
      else
      {
        w_fast = s - 2.0f;
        if (w_fast > 1.0f)
          w_fast = 1.0f;
        w_jog = 1.0f - w_fast;
      }

      float blended_duration = w_idle * duration_idle + w_walk * duration_walk +
                               w_jog * duration_jog + w_fast * duration_fast;
      if (blended_duration < 0.001f)
        blended_duration = 1.0f;

      state->player.anim_time += dt / blended_duration;
      bool wrapped = false;
      if (state->player.anim_time >= 1.0f)
      {
        state->player.anim_time -= 1.0f;
        wrapped = true;
        if (state->player.speed_param < 0.05f && !state->player.is_jumping)
        {
          state->player.prev_idle_anim_index =
              state->player.current_idle_anim_index;
          state->player.current_idle_anim_index = rand() % 4;
          state->player.idle_crossfade_time = 0.75f;
        }
      }

      if (state->player.idle_crossfade_time > 0.0f)
      {
        state->player.idle_crossfade_time -= dt;
        if (state->player.idle_crossfade_time < 0.0f)
          state->player.idle_crossfade_time = 0.0f;
      }

      if (state->player.is_jumping)
      {
        state->player.jump_anim_time += dt;
        if (state->player.jump_anim_time >= state->player.jump_duration)
        {
          state->player.is_jumping = 0;
          if (state->player.position.y < 0.1f)
            state->player.position.y = 0.0f;
        }
      }
      float ratio = state->player.anim_time;

      ozz::animation::SamplingJob sampling_job;
      sampling_job.ratio = ratio;

      sampling_job.animation = active_idle.anim;
      sampling_job.context = active_idle.cache;
      sampling_job.output = ozz::span<ozz::math::SoaTransform>(
          state->player.local_layers[LAYER_IDLE],
          state->models[10].num_soa_joints);
      sampling_job.Run();

      if (state->player.idle_crossfade_time > 0.0f)
      {
        OzzAnimation &prev_idle =
            state->player
                .anim_clips[CLIP_IDLE_1 + state->player.prev_idle_anim_index];

        ozz::animation::SamplingJob sampling_job_prev;
        sampling_job_prev.ratio = ratio;
        sampling_job_prev.animation = prev_idle.anim;
        sampling_job_prev.context = prev_idle.cache;
        sampling_job_prev.output = ozz::span<ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_IDLE_PREV],
            state->models[10].num_soa_joints);
        sampling_job_prev.Run();
      }

      sampling_job.animation = state->player.anim_clips[CLIP_WALK].anim;
      sampling_job.context = state->player.anim_clips[CLIP_WALK].cache;
      sampling_job.output = ozz::span<ozz::math::SoaTransform>(
          state->player.local_layers[LAYER_WALK],
          state->models[10].num_soa_joints);
      sampling_job.Run();

      sampling_job.animation = state->player.anim_clips[CLIP_JOG].anim;
      sampling_job.context = state->player.anim_clips[CLIP_JOG].cache;
      sampling_job.output = ozz::span<ozz::math::SoaTransform>(
          state->player.local_layers[LAYER_JOG],
          state->models[10].num_soa_joints);
      sampling_job.Run();

      sampling_job.animation = state->player.anim_clips[CLIP_FASTRUN].anim;
      sampling_job.context = state->player.anim_clips[CLIP_FASTRUN].cache;
      sampling_job.output = ozz::span<ozz::math::SoaTransform>(
          state->player.local_layers[LAYER_FASTRUN],
          state->models[10].num_soa_joints);
      sampling_job.Run();

      if (state->player.is_jumping)
      {
        OzzAnimation *jump_anim_struct = nullptr;
        if (state->player.current_jump_anim_index == 0)
          jump_anim_struct = &state->player.anim_clips[CLIP_JUMP_RUN];
        else if (state->player.current_jump_anim_index == 1)
          jump_anim_struct = &state->player.anim_clips[CLIP_JUMP_STAND_1];
        else if (state->player.current_jump_anim_index == 2)
          jump_anim_struct = &state->player.anim_clips[CLIP_JUMP_STAND_2];
        else if (state->player.current_jump_anim_index == 3)
          jump_anim_struct = &state->player.anim_clips[CLIP_JUMP_STAND_3];

        ozz::animation::SamplingJob jump_job;
        jump_job.ratio =
            state->player.jump_anim_time / state->player.jump_duration;
        if (jump_job.ratio > 1.0f)
          jump_job.ratio = 1.0f;
        jump_job.animation = jump_anim_struct->anim;
        jump_job.context = jump_anim_struct->cache;
        jump_job.output = ozz::span<ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_JUMP],
            state->models[10].num_soa_joints);
        jump_job.Run();
      }

      auto get_root_trans = [](ozz::math::SoaTransform *locals) -> Vec3
      {
        return {ozz::math::GetX(locals[0].translation.x),
                ozz::math::GetX(locals[0].translation.y),
                ozz::math::GetX(locals[0].translation.z)};
      };

      Vec3 root_idle = get_root_trans(state->player.local_layers[LAYER_IDLE]);
      Vec3 root_walk = get_root_trans(state->player.local_layers[LAYER_WALK]);
      Vec3 root_jog = get_root_trans(state->player.local_layers[LAYER_JOG]);
      Vec3 root_fastrun =
          get_root_trans(state->player.local_layers[LAYER_FASTRUN]);

      Vec3 delta_idle =
          wrapped ? Vec3{0, 0, 0}
                  : (root_idle - state->player.prev_root_trans[LAYER_IDLE]);
      Vec3 delta_walk =
          wrapped ? Vec3{0, 0, 0}
                  : (root_walk - state->player.prev_root_trans[LAYER_WALK]);
      Vec3 delta_jog =
          wrapped ? Vec3{0, 0, 0}
                  : (root_jog - state->player.prev_root_trans[LAYER_JOG]);
      Vec3 delta_fastrun =
          wrapped
              ? Vec3{0, 0, 0}
              : (root_fastrun - state->player.prev_root_trans[LAYER_FASTRUN]);

      state->player.prev_root_trans[LAYER_IDLE] = root_idle;
      state->player.prev_root_trans[LAYER_WALK] = root_walk;
      state->player.prev_root_trans[LAYER_JOG] = root_jog;
      state->player.prev_root_trans[LAYER_FASTRUN] = root_fastrun;

      Vec3 root_jump = {0, 0, 0};
      Vec3 delta_jump = {0, 0, 0};
      if (state->player.is_jumping)
      {
        root_jump = get_root_trans(state->player.local_layers[LAYER_JUMP]);
        delta_jump =
            (state->player.jump_anim_time <= dt)
                ? Vec3{0, 0, 0}
                : (root_jump - state->player.prev_root_trans[LAYER_JUMP]);
        state->player.prev_root_trans[LAYER_JUMP] = root_jump;
      }

      float w_jump = state->player.is_jumping ? 1.0f : 0.0f;
      if (state->player.is_jumping)
      {
        float fade = 0.1f;
        if (state->player.jump_anim_time < fade)
          w_jump = state->player.jump_anim_time / fade;
        else if (state->player.jump_duration - state->player.jump_anim_time <
                 fade)
          w_jump =
              (state->player.jump_duration - state->player.jump_anim_time) /
              fade;
      }

      w_idle *= (1.0f - w_jump);
      w_walk *= (1.0f - w_jump);
      w_jog *= (1.0f - w_jump);
      w_fast *= (1.0f - w_jump);

      Vec3 blended_local_delta = delta_idle * w_idle + delta_walk * w_walk +
                                 delta_jog * w_jog + delta_fastrun * w_fast +
                                 delta_jump * w_jump;

      float crossfade_weight = 0.0f;
      if (state->player.idle_crossfade_time > 0.0f)
      {
        crossfade_weight = state->player.idle_crossfade_time / 0.75f;
      }

      float active_idle_weight = w_idle * (1.0f - crossfade_weight);
      float prev_idle_weight = w_idle * crossfade_weight;

      ozz::animation::BlendingJob::Layer layers[6];
      int num_layers = 0;
      if (active_idle_weight > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_IDLE],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = active_idle_weight;
        num_layers++;
      }
      if (prev_idle_weight > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_IDLE_PREV],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = prev_idle_weight;
        num_layers++;
      }
      if (w_walk > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_WALK],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = w_walk;
        num_layers++;
      }
      if (w_jog > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_JOG],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = w_jog;
        num_layers++;
      }
      if (w_fast > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_FASTRUN],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = w_fast;
        num_layers++;
      }
      if (w_jump > 0.0f)
      {
        layers[num_layers].transform = ozz::span<const ozz::math::SoaTransform>(
            state->player.local_layers[LAYER_JUMP],
            state->models[10].num_soa_joints);
        layers[num_layers].weight = w_jump;
        num_layers++;
      }

      ozz::animation::BlendingJob blend_job;
      blend_job.layers = ozz::span<const ozz::animation::BlendingJob::Layer>(
          layers, num_layers);
      blend_job.rest_pose = skel->joint_rest_poses();
      blend_job.output = ozz::span<ozz::math::SoaTransform>(
          state->player.blended_locals, state->models[10].num_soa_joints);
      blend_job.Run();

      ozz::animation::LocalToModelJob ltm_job;
      ltm_job.skeleton = skel;
      ltm_job.input = ozz::span<ozz::math::SoaTransform>(
          state->player.blended_locals, state->models[10].num_soa_joints);
      ltm_job.output = ozz::span<ozz::math::Float4x4>(
          state->models[10].model_matrices, skel->num_joints());
      ltm_job.Run();

      const ozz::math::Float4x4 &root_mat = state->models[10].model_matrices[0];
      const float *rm = reinterpret_cast<const float *>(&root_mat);
      current_root_translation = {rm[12], 0.0f,
                                  rm[14]}; // raw unscaled translation

      if (!wrapped)
      {
        float yaw_rad = state->player.yaw * (M_PI / 180.0f);

        float s_y = sinf(yaw_rad);
        float c_y = cosf(yaw_rad);

        Vec3 world_delta;
        world_delta.x =
            blended_local_delta.x * c_y + blended_local_delta.z * s_y;
        world_delta.y = blended_local_delta.y;
        world_delta.z =
            -blended_local_delta.x * s_y + blended_local_delta.z * c_y;

        if (state->player.speed_param > 0.05f)
        {
          state->player.position += world_delta;
        }
      }
    }

    for (U32 n = 0; n < state->models[10].num_nodes; n++)
    {
      FBXNode *node = &state->models[10].nodes[n];
      if (node->vertex_count > 0)
      {
        uniforms.has_bones = 0;
        uniforms.bone_matrix_offset = 0;

        Mat4 bone_mats[MAX_BONES] = {};
        if (state->models[10].has_animation && node->num_bones > 0)
        {
          uniforms.has_bones = 1;
          EvaluateBoneMatrices(node, &state->models[10], bone_mats,
                               current_root_translation);
        }

        PushDrawMeshCommand(&out_output->render_group, uniforms, node->textures,
                            0, node->vertex_count, node->vertex_offset,
                            node->bounds_center, node->bounds_radius,
                            uniforms.has_bones ? bone_mats : nullptr);
      }
    }
  }

  // 5. Draw Banana in front of camera on the ground
  {
    Mat4 trans_matrix = Mat4{Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0},
                             Vec4{0, 0, 1, 0}, Vec4{-2.f, -0.5f, -1.0f, 1}};

    float s = 1.0f;
    Mat4 scale_matrix = Mat4{Vec4{s, 0, 0, 0}, Vec4{0, s, 0, 0},
                             Vec4{0, 0, s, 0}, Vec4{0, 0, 0, 1}};

    Mat4 model_matrix = trans_matrix * scale_matrix;
    Mat4 mvp_matrix = vp_matrix * model_matrix;

    Uniforms uniforms = base_uniforms;
    uniforms.mvp_matrix = mvp_matrix;
    uniforms.model_matrix = model_matrix;

    for (U32 n = 0; n < state->models[11].num_nodes; n++)
    {
      FBXNode *node = &state->models[11].nodes[n];
      if (node->vertex_count > 0)
      {
        PushDrawMeshCommand(&out_output->render_group, uniforms, node->textures,
                            0, node->vertex_count, node->vertex_offset,
                            node->bounds_center, node->bounds_radius);
      }
    }
  }

  // 6. Draw UI Text (FPS & Frame Time)
  {
    static float avg_fps = 0.0f;
    static float avg_dt = 0.0f;
    if (dt > 0.0001f)
    {
      avg_fps = avg_fps * 0.95f + (1.0f / dt) * 0.05f;
      avg_dt = avg_dt * 0.95f + (dt * 1000.0f) * 0.05f;
    }

    char ui_text[256];
    snprintf(ui_text, sizeof(ui_text), "FPS: %.0f\nFrame: %.2f ms", avg_fps,
             avg_dt);

    Mat4 ui_proj = math_make_orthographic(0, 800, 0, 800, -1, 1);
    Uniforms ui_uniforms = base_uniforms;
    ui_uniforms.model_matrix = Mat4{Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0},
                                    Vec4{0, 0, 1, 0}, Vec4{0, 0, 0, 1}};
    ui_uniforms.mvp_matrix = ui_proj * ui_uniforms.model_matrix;

    PushDrawTextCommand(&out_output->render_group, &state->main_font,
                        ui_uniforms, ui_text, Vec3{20.0f, 750.0f, 0.0f}, 0.5f);
  }
}
