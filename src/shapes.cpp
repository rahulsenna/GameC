#include "shapes.h"
#include "stb_image.h"
#include "ufbx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AddVertex(Vertex *vertices, U32 &index, float px, float py,
                      float pz, float nx, float ny, float nz, float u, float v)
{
  vertices[index++] = {{px, py, pz}, {nx, ny, nz}, {u, v}};
}

static void AddTriangle(Vertex *vertices, U32 &index, float p1x, float p1y,
                        float p1z, float u1, float v1, float p2x, float p2y,
                        float p2z, float u2, float v2, float p3x, float p3y,
                        float p3z, float u3, float v3, float nx, float ny,
                        float nz)
{
  AddVertex(vertices, index, p1x, p1y, p1z, nx, ny, nz, u1, v1);
  AddVertex(vertices, index, p2x, p2y, p2z, nx, ny, nz, u2, v2);
  AddVertex(vertices, index, p3x, p3y, p3z, nx, ny, nz, u3, v3);
}

static void
AddQuad(Vertex *vertices, U32 &index, float p1x, float p1y, float p1z, float u1,
        float v1,                                            // Bottom-left
        float p2x, float p2y, float p2z, float u2, float v2, // Bottom-right
        float p3x, float p3y, float p3z, float u3, float v3, // Top-right
        float p4x, float p4y, float p4z, float u4, float v4, // Top-left
        float nx, float ny, float nz)
{
  // Triangle 1
  AddVertex(vertices, index, p1x, p1y, p1z, nx, ny, nz, u1, v1);
  AddVertex(vertices, index, p2x, p2y, p2z, nx, ny, nz, u2, v2);
  AddVertex(vertices, index, p3x, p3y, p3z, nx, ny, nz, u3, v3);
  // Triangle 2
  AddVertex(vertices, index, p1x, p1y, p1z, nx, ny, nz, u1, v1);
  AddVertex(vertices, index, p3x, p3y, p3z, nx, ny, nz, u3, v3);
  AddVertex(vertices, index, p4x, p4y, p4z, nx, ny, nz, u4, v4);
}

MeshData CreateCuboid(Arena *arena, float width, float height, float depth)
{
  MeshData mesh;
  mesh.vertex_count = 36;
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;

  float hw = width * 0.5f;
  float hh = height * 0.5f;
  float hd = depth * 0.5f;

  // Front (0, 0, 1)
  AddQuad(mesh.vertices, idx, -hw, -hh, hd, 0, 1, hw, -hh, hd, 1, 1, hw, hh, hd,
          1, 0, -hw, hh, hd, 0, 0, 0, 0, 1);
  // Back (0, 0, -1)
  AddQuad(mesh.vertices, idx, hw, -hh, -hd, 0, 1, -hw, -hh, -hd, 1, 1, -hw, hh,
          -hd, 1, 0, hw, hh, -hd, 0, 0, 0, 0, -1);
  // Top (0, 1, 0)
  AddQuad(mesh.vertices, idx, -hw, hh, hd, 0, 1, hw, hh, hd, 1, 1, hw, hh, -hd,
          1, 0, -hw, hh, -hd, 0, 0, 0, 1, 0);
  // Bottom (0, -1, 0)
  AddQuad(mesh.vertices, idx, -hw, -hh, -hd, 0, 1, hw, -hh, -hd, 1, 1, hw, -hh,
          hd, 1, 0, -hw, -hh, hd, 0, 0, 0, -1, 0);
  // Right (1, 0, 0)
  AddQuad(mesh.vertices, idx, hw, -hh, hd, 0, 1, hw, -hh, -hd, 1, 1, hw, hh,
          -hd, 1, 0, hw, hh, hd, 0, 0, 1, 0, 0);
  // Left (-1, 0, 0)
  AddQuad(mesh.vertices, idx, -hw, -hh, -hd, 0, 1, -hw, -hh, hd, 1, 1, -hw, hh,
          hd, 1, 0, -hw, hh, -hd, 0, 0, -1, 0, 0);

  return mesh;
}

MeshData CreateCube(Arena *arena, float size)
{
  return CreateCuboid(arena, size, size, size);
}

MeshData CreateSphere(Arena *arena, float radius, int sectors, int stacks)
{
  MeshData mesh;
  mesh.vertex_count = sectors * stacks * 6; // 6 vertices per quad (2 triangles)
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;

  float PI = 3.14159265359f;

  for (int i = 0; i < stacks; ++i)
  {
    float lat0 = PI * (-0.5f + (float)(i) / stacks);
    float z0 = radius * sinf(lat0);
    float zr0 = radius * cosf(lat0);
    float v0 = (float)i / stacks;

    float lat1 = PI * (-0.5f + (float)(i + 1) / stacks);
    float z1 = radius * sinf(lat1);
    float zr1 = radius * cosf(lat1);
    float v1 = (float)(i + 1) / stacks;

    for (int j = 0; j < sectors; ++j)
    {
      float lng0 = 2 * PI * (float)(j) / sectors;
      float x0 = cosf(lng0);
      float y0 = sinf(lng0);
      float u0 = (float)j / sectors;

      float lng1 = 2 * PI * (float)(j + 1) / sectors;
      float x1 = cosf(lng1);
      float y1 = sinf(lng1);
      float u1 = (float)(j + 1) / sectors;

      // p1
      float px1 = x0 * zr0;
      float py1 = y0 * zr0;
      float pz1 = z0;
      float nx1 = px1 / radius;
      float ny1 = py1 / radius;
      float nz1 = pz1 / radius;
      // p2
      float px2 = x1 * zr0;
      float py2 = y1 * zr0;
      float pz2 = z0;
      float nx2 = px2 / radius;
      float ny2 = py2 / radius;
      float nz2 = pz2 / radius;
      // p3
      float px3 = x1 * zr1;
      float py3 = y1 * zr1;
      float pz3 = z1;
      float nx3 = px3 / radius;
      float ny3 = py3 / radius;
      float nz3 = pz3 / radius;
      // p4
      float px4 = x0 * zr1;
      float py4 = y0 * zr1;
      float pz4 = z1;
      float nx4 = px4 / radius;
      float ny4 = py4 / radius;
      float nz4 = pz4 / radius;

      // Triangle 1: p1 -> p3 -> p2
      AddVertex(mesh.vertices, idx, px1, pz1, py1, nx1, nz1, ny1, u0,
                v0); // Swapping Y and Z for y-up coordinate system
      AddVertex(mesh.vertices, idx, px3, pz3, py3, nx3, nz3, ny3, u1, v1);
      AddVertex(mesh.vertices, idx, px2, pz2, py2, nx2, nz2, ny2, u1, v0);

      // Triangle 2: p1 -> p4 -> p3
      AddVertex(mesh.vertices, idx, px1, pz1, py1, nx1, nz1, ny1, u0, v0);
      AddVertex(mesh.vertices, idx, px4, pz4, py4, nx4, nz4, ny4, u0, v1);
      AddVertex(mesh.vertices, idx, px3, pz3, py3, nx3, nz3, ny3, u1, v1);
    }
  }

  mesh.vertex_count = idx; // Adjust actual vertex count
  return mesh;
}

MeshData CreateTorus(Arena *arena, float main_radius, float tube_radius,
                     int main_segments, int tube_segments)
{
  MeshData mesh;
  mesh.vertex_count = main_segments * tube_segments * 6;
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;
  float PI = 3.14159265359f;

  for (int i = 0; i < main_segments; ++i)
  {
    float theta0 = 2.0f * PI * (float)i / main_segments;
    float theta1 = 2.0f * PI * (float)(i + 1) / main_segments;
    float u0 = (float)i / main_segments;
    float u1 = (float)(i + 1) / main_segments;

    for (int j = 0; j < tube_segments; ++j)
    {
      float phi0 = 2.0f * PI * (float)j / tube_segments;
      float phi1 = 2.0f * PI * (float)(j + 1) / tube_segments;
      float v0 = (float)j / tube_segments;
      float v1 = (float)(j + 1) / tube_segments;

      auto calc_pos = [=](float th, float ph, float &px, float &py, float &pz,
                          float &nx, float &ny, float &nz)
      {
        px = (main_radius + tube_radius * cosf(ph)) * cosf(th);
        py = tube_radius * sinf(ph);
        pz = (main_radius + tube_radius * cosf(ph)) * sinf(th);
        nx = cosf(ph) * cosf(th);
        ny = sinf(ph);
        nz = cosf(ph) * sinf(th);
      };

      float px00, py00, pz00, nx00, ny00, nz00;
      float px10, py10, pz10, nx10, ny10, nz10;
      float px11, py11, pz11, nx11, ny11, nz11;
      float px01, py01, pz01, nx01, ny01, nz01;

      calc_pos(theta0, phi0, px00, py00, pz00, nx00, ny00, nz00);
      calc_pos(theta1, phi0, px10, py10, pz10, nx10, ny10, nz10);
      calc_pos(theta1, phi1, px11, py11, pz11, nx11, ny11, nz11);
      calc_pos(theta0, phi1, px01, py01, pz01, nx01, ny01, nz01);

      AddVertex(mesh.vertices, idx, px00, py00, pz00, nx00, ny00, nz00, u0, v0);
      AddVertex(mesh.vertices, idx, px11, py11, pz11, nx11, ny11, nz11, u1, v1);
      AddVertex(mesh.vertices, idx, px10, py10, pz10, nx10, ny10, nz10, u1, v0);

      AddVertex(mesh.vertices, idx, px00, py00, pz00, nx00, ny00, nz00, u0, v0);
      AddVertex(mesh.vertices, idx, px01, py01, pz01, nx01, ny01, nz01, u0, v1);
      AddVertex(mesh.vertices, idx, px11, py11, pz11, nx11, ny11, nz11, u1, v1);
    }
  }
  return mesh;
}

MeshData CreateCylinder(Arena *arena, float radius, float height, int sectors)
{
  MeshData mesh;
  // Top, Bottom faces = sectors * 3 each
  // Side faces = sectors * 6
  mesh.vertex_count = sectors * 12;
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;
  float PI = 3.14159265359f;
  float hh = height * 0.5f;

  for (int i = 0; i < sectors; ++i)
  {
    float a0 = 2.0f * PI * (float)i / sectors;
    float a1 = 2.0f * PI * (float)(i + 1) / sectors;

    float c0 = cosf(a0), s0 = sinf(a0);
    float c1 = cosf(a1), s1 = sinf(a1);
    float u0 = (float)i / sectors, u1 = (float)(i + 1) / sectors;

    // Side
    AddQuad(mesh.vertices, idx, c1 * radius, -hh, s1 * radius, u1, 1.0f,
            c0 * radius, -hh, s0 * radius, u0, 1.0f, c0 * radius, hh,
            s0 * radius, u0, 0.0f, c1 * radius, hh, s1 * radius, u1, 0.0f,
            (c0 + c1) * 0.5f, 0, (s0 + s1) * 0.5f); // normal approx

    // Top
    AddTriangle(mesh.vertices, idx, 0, hh, 0, 0.5f, 0.5f, c1 * radius, hh,
                s1 * radius, c1 * 0.5f + 0.5f, s1 * 0.5f + 0.5f, c0 * radius,
                hh, s0 * radius, c0 * 0.5f + 0.5f, s0 * 0.5f + 0.5f, 0, 1, 0);

    // Bottom
    AddTriangle(mesh.vertices, idx, 0, -hh, 0, 0.5f, 0.5f, c0 * radius, -hh,
                s0 * radius, c0 * 0.5f + 0.5f, s0 * 0.5f + 0.5f, c1 * radius,
                -hh, s1 * radius, c1 * 0.5f + 0.5f, s1 * 0.5f + 0.5f, 0, -1, 0);
  }
  // Fix side normals
  for (U32 i = 0; i < sectors * 6; ++i)
  {
    float x = mesh.vertices[i].position.x;
    float z = mesh.vertices[i].position.z;
    float len = sqrtf(x * x + z * z);
    if (len > 0.0001f)
    {
      mesh.vertices[i].normal.x = x / len;
      mesh.vertices[i].normal.y = 0;
      mesh.vertices[i].normal.z = z / len;
    }
  }
  return mesh;
}

MeshData CreateCone(Arena *arena, float radius, float height, int sectors)
{
  MeshData mesh;
  // Bottom faces = sectors * 3, Side faces = sectors * 3
  mesh.vertex_count = sectors * 6;
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;
  float PI = 3.14159265359f;
  float hh = height * 0.5f;

  for (int i = 0; i < sectors; ++i)
  {
    float a0 = 2.0f * PI * (float)i / sectors;
    float a1 = 2.0f * PI * (float)(i + 1) / sectors;

    float c0 = cosf(a0), s0 = sinf(a0);
    float c1 = cosf(a1), s1 = sinf(a1);
    float u0 = (float)i / sectors, u1 = (float)(i + 1) / sectors;

    float nx = (c0 + c1) * 0.5f * height; // Simplified normal
    float nz = (s0 + s1) * 0.5f * height;
    float ny = radius;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    nx /= len;
    ny /= len;
    nz /= len;

    // Side
    AddTriangle(mesh.vertices, idx, c1 * radius, -hh, s1 * radius, u1, 1.0f,
                c0 * radius, -hh, s0 * radius, u0, 1.0f, 0, hh, 0,
                u0 + (u1 - u0) * 0.5f, 0.0f, nx, ny, nz);

    // Bottom
    AddTriangle(mesh.vertices, idx, 0, -hh, 0, 0.5f, 0.5f, c0 * radius, -hh,
                s0 * radius, c0 * 0.5f + 0.5f, s0 * 0.5f + 0.5f, c1 * radius,
                -hh, s1 * radius, c1 * 0.5f + 0.5f, s1 * 0.5f + 0.5f, 0, -1, 0);
  }
  return mesh;
}

MeshData CreateTriangularPyramid(Arena *arena, float size)
{
  MeshData mesh;
  mesh.vertex_count = 12; // 4 triangles
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;

  float r = size;
  // Base triangle
  float ax = 0, ay = -r * 0.5f, az = r;
  float bx = -r * 0.866f, by = -r * 0.5f, bz = -r * 0.5f;
  float cx = r * 0.866f, cy = -r * 0.5f, cz = -r * 0.5f;
  // Top
  float tx = 0, ty = r, tz = 0;

  // Bottom (swap B and C)
  AddTriangle(mesh.vertices, idx, ax, ay, az, 0.5f, 1, bx, by, bz, 0, 0, cx, cy,
              cz, 1, 0, 0, -1, 0);
  // Face A-B-Top (swap Top and B)
  AddTriangle(mesh.vertices, idx, ax, ay, az, 1, 1, tx, ty, tz, 0.5f, 0, bx, by,
              bz, 0, 1, -0.866f, 0.5f, 0.5f);
  // Face B-C-Top (swap Top and C)
  AddTriangle(mesh.vertices, idx, bx, by, bz, 0, 1, tx, ty, tz, 0.5f, 0, cx, cy,
              cz, 1, 1, 0, 0.5f, -1);
  // Face C-A-Top (swap Top and A)
  AddTriangle(mesh.vertices, idx, cx, cy, cz, 0, 1, tx, ty, tz, 0.5f, 0, ax, ay,
              az, 1, 1, 0.866f, 0.5f, 0.5f);
  return mesh;
}

MeshData CreateSquarePyramid(Arena *arena, float base_size, float height)
{
  MeshData mesh;
  mesh.vertex_count = 18; // base (2 tri) + 4 side tris = 6 tris
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;

  float hb = base_size * 0.5f;
  float hh = height * 0.5f;

  // Base
  AddQuad(mesh.vertices, idx, -hb, -hh, -hb, 0, 1, hb, -hh, -hb, 1, 1, hb, -hh,
          hb, 1, 0, -hb, -hh, hb, 0, 0, 0, -1, 0);

  // Front
  AddTriangle(mesh.vertices, idx, -hb, -hh, hb, 0, 1, hb, -hh, hb, 1, 1, 0, hh,
              0, 0.5f, 0, 0, 1, 1);
  // Back
  AddTriangle(mesh.vertices, idx, hb, -hh, -hb, 0, 1, -hb, -hh, -hb, 1, 1, 0,
              hh, 0, 0.5f, 0, 0, 1, -1);
  // Left
  AddTriangle(mesh.vertices, idx, -hb, -hh, -hb, 0, 1, -hb, -hh, hb, 1, 1, 0,
              hh, 0, 0.5f, 0, -1, 1, 0);
  // Right
  AddTriangle(mesh.vertices, idx, hb, -hh, hb, 0, 1, hb, -hh, -hb, 1, 1, 0, hh,
              0, 0.5f, 0, 1, 1, 0);

  return mesh;
}

MeshData CreateTriangularPrism(Arena *arena, float width, float height,
                               float depth)
{
  MeshData mesh;
  mesh.vertex_count = 24; // 2 bases (2 tris) + 3 rectangular sides (6 tris) ->
                          // 8 tris -> 24 verts
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;

  float hw = width * 0.5f;
  float hh = height * 0.5f;
  float hd = depth * 0.5f;

  // Front triangle
  AddTriangle(mesh.vertices, idx, -hw, -hh, hd, 0, 1, hw, -hh, hd, 1, 1, 0, hh,
              hd, 0.5f, 0, 0, 0, 1);
  // Back triangle
  AddTriangle(mesh.vertices, idx, hw, -hh, -hd, 0, 1, -hw, -hh, -hd, 1, 1, 0,
              hh, -hd, 0.5f, 0, 0, 0, -1);

  // Bottom Quad
  AddQuad(mesh.vertices, idx, -hw, -hh, -hd, 0, 1, hw, -hh, -hd, 1, 1, hw, -hh,
          hd, 1, 0, -hw, -hh, hd, 0, 0, 0, -1, 0);
  // Left Quad
  AddQuad(mesh.vertices, idx, -hw, -hh, -hd, 0, 1, -hw, -hh, hd, 1, 1, 0, hh,
          hd, 1, 0, 0, hh, -hd, 0, 0, -1, 1, 0); // Approximate normal
  // Right Quad
  AddQuad(mesh.vertices, idx, hw, -hh, hd, 0, 1, hw, -hh, -hd, 1, 1, 0, hh, -hd,
          1, 0, 0, hh, hd, 0, 0, 1, 1, 0); // Approximate normal

  return mesh;
}

MeshData CreatePlane(Arena *arena, float size)
{
  MeshData mesh;
  mesh.vertex_count = 6;
  mesh.vertices = PushArray(arena, Vertex, mesh.vertex_count);
  U32 idx = 0;
  float hs = size * 0.5f;

  // Plane on XZ axis (Y = 0)
  AddQuad(mesh.vertices, idx, -hs, 0.0f, hs, 0, 1, hs, 0.0f, hs, 1, 1, hs, 0.0f,
          -hs, 1, 0, -hs, 0.0f, -hs, 0, 0, 0, 1, 0);

  return mesh;
}

FBXModel LoadFBX(Arena *arena, const char *filepath, RenderGroup *render_group,
                 U32 *next_texture_handle)
{
  FBXModel model = {};
  model.num_nodes = 0;

  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_y_up;
  opts.target_unit_meters = 1.0f;

  ufbx_error error;
  ufbx_scene *scene = ufbx_load_file(filepath, &opts, &error);

  if (!scene)
  {
    printf("Failed to load FBX: %s\n", error.description.data);
    return model;
  }

  struct LoadedTex
  {
    const void *data;
    U32 handle;
  };
  LoadedTex loaded_tex[32] = {};
  U32 num_loaded_tex = 0;

  for (size_t mesh_i = 0; mesh_i < scene->meshes.count; mesh_i++)
  {
    if (model.num_nodes >= 32)
      break;

    ufbx_mesh *ufbx_m = scene->meshes.data[mesh_i];
    if (!ufbx_m)
      continue;

    FBXNode node = {};

    // --- 1. Find Texture ---
    node.texture_handle = 1; // Default to checkerboard

    if (ufbx_m->materials.count > 0)
    {
      ufbx_material *mat = ufbx_m->materials.data[0];
      for (size_t tex_i = 0; tex_i < mat->textures.count; tex_i++)
      {
        ufbx_material_texture mat_tex = mat->textures.data[tex_i];
        if (strcmp(mat_tex.material_prop.data, "DiffuseColor") == 0 ||
            strcmp(mat_tex.material_prop.data, "base_color") == 0)
        {

          ufbx_texture *tex = mat_tex.texture;
          if (tex && tex->type == UFBX_TEXTURE_FILE && tex->content.data)
          {

            U32 cached_handle = 0;
            for (U32 t = 0; t < num_loaded_tex; t++)
            {
              if (loaded_tex[t].data == tex->content.data)
              {
                cached_handle = loaded_tex[t].handle;
                break;
              }
            }

            if (cached_handle != 0)
            {
              node.texture_handle = cached_handle;
            }
            else
            {
              int width, height, channels;
              unsigned char *pixels = stbi_load_from_memory(
                  (const stbi_uc *)tex->content.data, (int)tex->content.size,
                  &width, &height, &channels, 4);

              if (pixels)
              {
                node.texture_handle = (*next_texture_handle)++;
                PushUploadTextureCommand(render_group, node.texture_handle,
                                         width, height, pixels);
                stbi_image_free(pixels);

                loaded_tex[num_loaded_tex].data = tex->content.data;
                loaded_tex[num_loaded_tex].handle = node.texture_handle;
                num_loaded_tex++;
              }
            }
          }
        }
      }
    }

    // --- 2. Extract Geometry ---
    size_t max_tris = 0;
    for (size_t i = 0; i < ufbx_m->faces.count; i++)
    {
      max_tris += ufbx_m->max_face_triangles;
    }

    node.vertices = PushArray(arena, Vertex, max_tris * 3);
    U32 idx = 0;
    uint32_t *tri_indices =
        (uint32_t *)malloc(ufbx_m->max_face_triangles * 3 * sizeof(uint32_t));

    for (size_t fi = 0; fi < ufbx_m->faces.count; fi++)
    {
      ufbx_face face = ufbx_m->faces.data[fi];
      if (face.num_indices < 3)
        continue;

      uint32_t num_tris = ufbx_triangulate_face(
          tri_indices, ufbx_m->max_face_triangles * 3, ufbx_m, face);

      for (uint32_t i = 0; i < num_tris * 3; i++)
      {
        uint32_t v_idx = tri_indices[i];

        ufbx_vec3 pos = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, v_idx);
        ufbx_vec3 norm = ufbx_get_vertex_vec3(&ufbx_m->vertex_normal, v_idx);
        ufbx_vec2 uv = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, v_idx);

        AddVertex(node.vertices, idx, pos.x, pos.y, pos.z, norm.x, norm.y,
                  norm.z, uv.x, uv.y);
      }
    }
    free(tri_indices);
    node.vertex_count = idx;

    model.nodes[model.num_nodes++] = node;
  }

  ufbx_free_scene(scene);
  return model;
}
