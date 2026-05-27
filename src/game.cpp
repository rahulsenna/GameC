#include "game.h"
#include "math_utils.h"
#include "shapes.h"
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

void PushUploadTextureCommand(RenderGroup *group, U32 handle, U32 width,
                              U32 height, void *pixels)
{
  U32 pixel_data_size = width * height * 4;
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_UploadTexture) + pixel_data_size;
  if (group->size + total_size > group->max_size) {
      printf("ASSERT FAILURE: size=%u, total_size=%u, max_size=%u, w=%u, h=%u\n", group->size, total_size, group->max_size, width, height);
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

  void *dst_pixels = (void *)(entry + 1);
  for (U32 i = 0; i < pixel_data_size; ++i)
  {
    ((U8 *)dst_pixels)[i] = ((U8 *)pixels)[i];
  }

  group->size += total_size;
}

void PushDrawMeshCommand(RenderGroup *group, Uniforms uniforms,
                         U32 texture_handle, U32 shader_type, U32 vertex_count, Vertex *vertices)
{
  U32 total_size = sizeof(RenderGroupEntryHeader) +
                   sizeof(RenderGroupEntry_DrawMesh) +
                   sizeof(Vertex) * vertex_count;
  Assert(group->size + total_size <= group->max_size);

  RenderGroupEntryHeader *header =
      (RenderGroupEntryHeader *)(group->base + group->size);
  header->type = RenderGroupEntryType_DrawMesh;

  RenderGroupEntry_DrawMesh *entry = (RenderGroupEntry_DrawMesh *)(header + 1);
  entry->uniforms = uniforms;
  entry->texture_handle = texture_handle;
  entry->shader_type = shader_type;
  entry->vertex_count = vertex_count;

  Vertex *dst_vertices = (Vertex *)(entry + 1);
  for (U32 i = 0; i < vertex_count; ++i)
  {
    dst_vertices[i] = vertices[i];
  }

  group->size += total_size;
}

extern "C" void GameUpdateAndRender(Arena *arena, GameInput *input,
                                    GameOutput *out_output)
{
  if (arena->pos == sizeof(Arena))
  {
    PushStruct(arena, GameState);
  }

  GameState *state = (GameState *)((U8 *)arena + sizeof(Arena));
  out_output->render_group.size = 0;

  if (!state->is_initialized)
  {
    state->is_initialized = 1;
    state->time = 0.0f;
    state->texture_handle = 1;

    state->camera.position = simd_make_float3(0.0f, 2.0f, 5.0f);
    state->camera.yaw = -90.0f; // Look down -Z
    state->camera.pitch = 0.0f;

    // Load texture
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("assets/CustomUVChecker_byValle_1K.png", &width, &height,
                                    &channels, 4); // Force RGBA
    if (data)
    {
      PushUploadTextureCommand(&out_output->render_group, state->texture_handle,
                               width, height, data);
      stbi_image_free(data);
    }
    
    // Generate Shapes
    state->shapes[0] = CreateSphere(arena);
    state->shapes[1] = CreateTorus(arena);
    state->shapes[2] = CreateCylinder(arena);
    state->shapes[3] = CreateCone(arena);
    state->shapes[4] = CreateCube(arena);
    state->shapes[5] = CreateCuboid(arena, 1.5f, 0.5f, 1.0f);
    state->shapes[6] = CreateTriangularPyramid(arena);
    state->shapes[7] = CreateSquarePyramid(arena);
    state->shapes[8] = CreateTriangularPrism(arena);
    state->shapes[9] = CreatePlane(arena, 1000.0f);
    
    U32 next_tex_handle = 100;
    state->fbx_model = LoadFBX(arena, "assets/Sophie.fbx", &out_output->render_group, &next_tex_handle);
  }

  float dt = 0.016f;
  state->time += dt;

  // --- Camera Update ---
  float move_speed = 3.0f * dt;
  float look_speed = 90.0f * dt; // degrees per second

  if (input->key_up)
    state->camera.pitch += look_speed;
  if (input->key_down)
    state->camera.pitch -= look_speed;
  if (input->key_left)
    state->camera.yaw -= look_speed;
  if (input->key_right)
    state->camera.yaw += look_speed;

  // Prevent gimbal lock
  if (state->camera.pitch > 89.0f)
    state->camera.pitch = 89.0f;
  if (state->camera.pitch < -89.0f)
    state->camera.pitch = -89.0f;

  simd_float3 front;
  front.x = cosf(state->camera.yaw * (M_PI / 180.0f)) *
            cosf(state->camera.pitch * (M_PI / 180.0f));
  front.y = sinf(state->camera.pitch * (M_PI / 180.0f));
  front.z = sinf(state->camera.yaw * (M_PI / 180.0f)) *
            cosf(state->camera.pitch * (M_PI / 180.0f));
  front = math_normalize(front);

  simd_float3 up = simd_make_float3(0.0f, 1.0f, 0.0f);
  simd_float3 right = math_normalize(math_cross(front, up));

  if (input->key_w)
    state->camera.position += front * move_speed;
  if (input->key_s)
    state->camera.position -= front * move_speed;
  if (input->key_a)
    state->camera.position -= right * move_speed;
  if (input->key_d)
    state->camera.position += right * move_speed;

  // --- Render ---
  PushClearCommand(&out_output->render_group, 0.1f, 0.1f, 0.1f, 1.0f);

  simd_float4x4 view_matrix = math_make_look_at(
      state->camera.position, state->camera.position + front, up);

  float fov = 60.0f * (3.14159f / 180.0f);
  float aspect = 800.0f / 800.0f;
  simd_float4x4 proj_matrix = math_make_perspective(fov, aspect, 0.1f, 100.0f);

  simd_float4x4 vp_matrix = simd_mul(proj_matrix, view_matrix);

  // 1. Draw Infinite Grid Plane
  {
      simd_float4x4 model_matrix = simd_matrix(
          simd_make_float4(1, 0, 0, 0),
          simd_make_float4(0, 1, 0, 0),
          simd_make_float4(0, 0, 1, 0),
          simd_make_float4(0, -0.5f, 0, 1) // Set plane to ground level (-0.5 is the exact bottom bound for most of our shapes)
      );
      simd_float4x4 mvp_matrix = simd_mul(vp_matrix, model_matrix);

      Uniforms uniforms = {};
      uniforms.mvp_matrix = mvp_matrix;
      uniforms.model_matrix = model_matrix;
      uniforms.light_dir = math_normalize(simd_make_float3(1.0f, 1.0f, -1.0f));
      uniforms.light_color = simd_make_float3(1.0f, 1.0f, 1.0f);
      uniforms.camera_pos = state->camera.position;
      uniforms.ambient_intensity = 0.2f;

      PushDrawMeshCommand(&out_output->render_group, uniforms,
                          state->texture_handle, 1, state->shapes[9].vertex_count, state->shapes[9].vertices);
  }

  // 2. Draw 9 Shapes
  float shape_y_offsets[9] = {
      0.0f,    // 0: Sphere (bottom at -0.5)
      -0.3f,   // 1: Torus (bottom at -0.2 -> needs to move down 0.3 to rest at -0.5)
      0.0f,    // 2: Cylinder (bottom at -0.5)
      0.0f,    // 3: Cone (bottom at -0.5)
      0.0f,    // 4: Cube (bottom at -0.5)
      -0.25f,  // 5: Cuboid (height 0.5 -> bottom at -0.25 -> needs to move down 0.25 to rest at -0.5)
      0.0f,    // 6: TriangularPyramid (bottom at -0.5)
      0.0f,    // 7: SquarePyramid (bottom at -0.5)
      0.0f     // 8: TriangularPrism (bottom at -0.5)
  };

  for (int i = 0; i < 9; ++i)
  {
      float x = (i % 3) * 2.5f - 2.5f;
      float z = (i / 3) * 2.5f - 2.5f;
      float y = shape_y_offsets[i];
      simd_float4x4 trans_matrix =
          simd_matrix(simd_make_float4(1, 0, 0, 0), simd_make_float4(0, 1, 0, 0),
                      simd_make_float4(0, 0, 1, 0), simd_make_float4(x, y, z, 1));

      float angle = state->time * 0.5f + i;
      simd_float4x4 rot_y = simd_matrix(
          simd_make_float4(cosf(angle), 0, -sinf(angle), 0),
          simd_make_float4(0, 1, 0, 0),
          simd_make_float4(sinf(angle), 0, cosf(angle), 0),
          simd_make_float4(0, 0, 0, 1)
      );
      
      simd_float4x4 model_matrix = simd_mul(trans_matrix, rot_y);
      simd_float4x4 mvp_matrix = simd_mul(vp_matrix, model_matrix);

      Uniforms uniforms = {};
      uniforms.mvp_matrix = mvp_matrix;
      uniforms.model_matrix = model_matrix;
      uniforms.light_dir = math_normalize(simd_make_float3(1.0f, 1.0f, -1.0f));
      uniforms.light_color = simd_make_float3(1.0f, 1.0f, 1.0f);
      uniforms.camera_pos = state->camera.position;
      uniforms.ambient_intensity = 0.2f;

      PushDrawMeshCommand(&out_output->render_group, uniforms,
                          state->texture_handle, 0, state->shapes[i].vertex_count, state->shapes[i].vertices);
  }

  // 3. Draw FBX Model in front of the shapes (closer to camera)
  {
      simd_float4x4 trans_matrix =
          simd_matrix(simd_make_float4(1, 0, 0, 0), simd_make_float4(0, 1, 0, 0),
                      simd_make_float4(0, 0, 1, 0), simd_make_float4(0, -0.5f, -2.5f, 1));
      
      simd_float4x4 rot_y = simd_matrix(
          simd_make_float4(1, 0, 0, 0),
          simd_make_float4(0, 1, 0, 0),
          simd_make_float4(0, 0, 1, 0),
          simd_make_float4(0, 0, 0, 1)
      );

      // Scale it down (often FBX files are in cm)
      float s = 0.01f;
      simd_float4x4 scale_matrix = simd_matrix(
          simd_make_float4(s, 0, 0, 0),
          simd_make_float4(0, s, 0, 0),
          simd_make_float4(0, 0, s, 0),
          simd_make_float4(0, 0, 0, 1)
      );

      simd_float4x4 rot_scale = simd_mul(rot_y, scale_matrix);
      simd_float4x4 model_matrix = simd_mul(trans_matrix, rot_scale);
      simd_float4x4 mvp_matrix = simd_mul(vp_matrix, model_matrix);

      Uniforms uniforms = {};
      uniforms.mvp_matrix = mvp_matrix;
      uniforms.model_matrix = model_matrix;
      uniforms.light_dir = math_normalize(simd_make_float3(1.0f, 1.0f, 1.0f));
      uniforms.light_color = simd_make_float3(1.0f, 1.0f, 1.0f);
      uniforms.camera_pos = state->camera.position;
      uniforms.ambient_intensity = 0.2f;

      for (U32 n = 0; n < state->fbx_model.num_nodes; n++)
      {
          FBXNode *node = &state->fbx_model.nodes[n];
          if (node->vertex_count > 0)
          {
              PushDrawMeshCommand(&out_output->render_group, uniforms,
                                  node->texture_handle, 0, node->vertex_count, node->vertices);
          }
      }
  }
}
