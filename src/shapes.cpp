#include "shapes.h"

struct MeshData
{
  U32 vertex_count;
  Vertex *vertices;
};

static FBXModel ToFBXModel(MeshData mesh)
{
  FBXModel model = {};
  model.num_nodes = 1;
  model.nodes[0].vertex_count = mesh.vertex_count;
  model.nodes[0].vertices = mesh.vertices;
  return model;
}
#include "stb_image.h"
#include "ufbx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AddVertex(Vertex *vertices, U32 &index, float px, float py,
                      float pz, float nx, float ny, float nz, float u, float v,
                      float tx = 0.0f, float ty = 0.0f, float tz = 0.0f)
{
  if (tx == 0.0f && ty == 0.0f && tz == 0.0f)
  {
    if (fabsf(ny) > 0.999f)
    {
      tx = 1.0f;
      ty = 0.0f;
      tz = 0.0f;
    }
    else
    {
      tx = nz;
      ty = 0.0f;
      tz = -nx;
      float l = sqrtf(tx * tx + ty * ty + tz * tz);
      tx /= l;
      ty /= l;
      tz /= l;
    }
  }
  vertices[index++] = {{px, py, pz}, {nx, ny, nz}, {tx, ty, tz},
                       {u, v},       {0, 0, 0, 0}, {0.0f, 0.0f, 0.0f, 0.0f}};
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

static MeshData _CreateCuboid(Arena *arena, float width, float height,
                              float depth)
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

static MeshData _CreateCube(Arena *arena, float size)
{
  return _CreateCuboid(arena, size, size, size);
}

static MeshData _CreateSphere(Arena *arena, float radius, int sectors,
                              int stacks)
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

static MeshData _CreateTorus(Arena *arena, float main_radius, float tube_radius,
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

static MeshData _CreateCylinder(Arena *arena, float radius, float height,
                                int sectors)
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

static MeshData _CreateCone(Arena *arena, float radius, float height,
                            int sectors)
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

static MeshData _CreateTriangularPyramid(Arena *arena, float size)
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

static MeshData _CreateSquarePyramid(Arena *arena, float base_size,
                                     float height)
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

static MeshData _CreateTriangularPrism(Arena *arena, float width, float height,
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

static MeshData _CreatePlane(Arena *arena, float size)
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

FBXModel CreateCuboid(Arena *arena, float width, float height, float depth)
{
  return ToFBXModel(_CreateCuboid(arena, width, height, depth));
}
FBXModel CreateCube(Arena *arena, float size)
{
  return ToFBXModel(_CreateCube(arena, size));
}
FBXModel CreateSphere(Arena *arena, float radius, int sectors, int stacks)
{
  return ToFBXModel(_CreateSphere(arena, radius, sectors, stacks));
}
FBXModel CreateTorus(Arena *arena, float main_radius, float tube_radius,
                     int main_segments, int tube_segments)
{
  return ToFBXModel(_CreateTorus(arena, main_radius, tube_radius, main_segments,
                                 tube_segments));
}
FBXModel CreateCylinder(Arena *arena, float radius, float height, int sectors)
{
  return ToFBXModel(_CreateCylinder(arena, radius, height, sectors));
}
FBXModel CreateCone(Arena *arena, float radius, float height, int sectors)
{
  return ToFBXModel(_CreateCone(arena, radius, height, sectors));
}
FBXModel CreateTriangularPyramid(Arena *arena, float size)
{
  return ToFBXModel(_CreateTriangularPyramid(arena, size));
}
FBXModel CreateSquarePyramid(Arena *arena, float base_size, float height)
{
  return ToFBXModel(_CreateSquarePyramid(arena, base_size, height));
}
FBXModel CreateTriangularPrism(Arena *arena, float width, float height,
                               float depth)
{
  return ToFBXModel(_CreateTriangularPrism(arena, width, height, depth));
}
FBXModel CreatePlane(Arena *arena, float size)
{
  return ToFBXModel(_CreatePlane(arena, size));
}
FBXModel LoadFBX(Arena *arena, const char *filepath, RenderGroup *render_group,
                 U32 *next_texture_handle, MaterialTextures default_textures)
{
  FBXModel model = {};
  model.num_nodes = 0;
  model.ufbx_scene_ptr = NULL;
  model.ufbx_anim_ptr = NULL;
  model.has_animation = 0;

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
  LoadedTex loaded_tex[256] = {};
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
    node.textures = default_textures;

    if (ufbx_m->materials.count > 0)
    {
      ufbx_material *mat = ufbx_m->materials.data[0];
      for (size_t tex_i = 0; tex_i < mat->textures.count; tex_i++)
      {
        ufbx_material_texture mat_tex = mat->textures.data[tex_i];

        bool is_albedo =
            strcmp(mat_tex.material_prop.data, "DiffuseColor") == 0 ||
            strcmp(mat_tex.material_prop.data, "base_color") == 0;
        bool is_normal =
            strcmp(mat_tex.material_prop.data, "NormalMap") == 0 ||
            strcmp(mat_tex.material_prop.data, "normal_map") == 0 ||
            strcmp(mat_tex.material_prop.data, "Bump") == 0;
        bool is_metallic =
            strcmp(mat_tex.material_prop.data, "metalness") == 0 ||
            strcmp(mat_tex.material_prop.data, "metallic") == 0;
        bool is_roughness =
            strcmp(mat_tex.material_prop.data, "Shininess") == 0 ||
            strcmp(mat_tex.material_prop.data, "ShininessExponent") == 0 ||
            strcmp(mat_tex.material_prop.data, "roughness") == 0;
        bool is_ao =
            strcmp(mat_tex.material_prop.data, "AmbientColor") == 0 ||
            strcmp(mat_tex.material_prop.data, "ambient_occlusion") == 0;

        if (is_albedo || is_normal || is_metallic || is_roughness || is_ao)
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

            U32 final_handle = cached_handle;
            if (cached_handle == 0)
            {
              int width, height, channels;
              unsigned char *pixels = stbi_load_from_memory(
                  (const stbi_uc *)tex->content.data, (int)tex->content.size,
                  &width, &height, &channels, 4);

              if (pixels)
              {
                if (is_roughness && strcmp(mat_tex.material_prop.data,
                                           "ShininessExponent") == 0)
                {
                  for (int i = 0; i < width * height * 4; i += 4)
                  {
                    pixels[i] = 255 - pixels[i];         // R
                    pixels[i + 1] = 255 - pixels[i + 1]; // G
                    pixels[i + 2] = 255 - pixels[i + 2]; // B
                  }
                }
                final_handle = (*next_texture_handle)++;
                PushUploadTextureCommand(render_group, final_handle, width,
                                         height, pixels);
                stbi_image_free(pixels);

                if (num_loaded_tex < 256)
                {
                  loaded_tex[num_loaded_tex].data = tex->content.data;
                  loaded_tex[num_loaded_tex].handle = final_handle;
                  num_loaded_tex++;
                }
              }
            }

            if (final_handle != 0)
            {
              if (is_albedo)
                node.textures.albedo = final_handle;
              else if (is_normal)
                node.textures.normal = final_handle;
              else if (is_metallic)
                node.textures.metallic = final_handle;
              else if (is_roughness)
                node.textures.roughness = final_handle;
              else if (is_ao)
                node.textures.ao = final_handle;
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

    // --- 2.5 Extract Skinning Information ---
    ufbx_skin_deformer *skin = NULL;
    node.num_bones = 0;
    node.bone_nodes = NULL;
    node.inverse_bind_matrices = NULL;
    if (ufbx_m->skin_deformers.count > 0)
    {
      skin = ufbx_m->skin_deformers.data[0];
      node.bone_nodes = PushArray(arena, void *, MAX_BONES);
      node.inverse_bind_matrices = PushArray(arena, Mat4, MAX_BONES);
      for (size_t c = 0; c < skin->clusters.count; c++)
      {
        if (node.num_bones >= MAX_BONES)
          break;
        ufbx_skin_cluster *cluster = skin->clusters.data[c];
        node.bone_nodes[node.num_bones] = cluster->bone_node;

        Mat4 inv_bind = {};
        inv_bind.columns[0] = {(float)cluster->geometry_to_bone.m00,
                               (float)cluster->geometry_to_bone.m10,
                               (float)cluster->geometry_to_bone.m20, 0.0f};
        inv_bind.columns[1] = {(float)cluster->geometry_to_bone.m01,
                               (float)cluster->geometry_to_bone.m11,
                               (float)cluster->geometry_to_bone.m21, 0.0f};
        inv_bind.columns[2] = {(float)cluster->geometry_to_bone.m02,
                               (float)cluster->geometry_to_bone.m12,
                               (float)cluster->geometry_to_bone.m22, 0.0f};
        inv_bind.columns[3] = {(float)cluster->geometry_to_bone.m03,
                               (float)cluster->geometry_to_bone.m13,
                               (float)cluster->geometry_to_bone.m23, 1.0f};
        node.inverse_bind_matrices[node.num_bones] = inv_bind;
        node.num_bones++;
      }
    }

    uint32_t *tri_indices =
        (uint32_t *)malloc(ufbx_m->max_face_triangles * 3 * sizeof(uint32_t));
    if (!tri_indices)
      continue;

    for (size_t fi = 0; fi < ufbx_m->faces.count; fi++)
    {
      ufbx_face face = ufbx_m->faces.data[fi];
      if (face.num_indices < 3)
        continue;

      uint32_t num_tris = ufbx_triangulate_face(
          tri_indices, ufbx_m->max_face_triangles * 3, ufbx_m, face);

      for (uint32_t i = 0; i < num_tris * 3; i += 3)
      {
        uint32_t i0 = tri_indices[i];
        uint32_t i1 = tri_indices[i + 1];
        uint32_t i2 = tri_indices[i + 2];

        ufbx_vec3 p0 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i0);
        ufbx_vec3 p1 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i1);
        ufbx_vec3 p2 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i2);

        ufbx_vec2 uv0 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i0);
        ufbx_vec2 uv1 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i1);
        ufbx_vec2 uv2 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i2);

        float dx1 = p1.x - p0.x;
        float dy1 = p1.y - p0.y;
        float dz1 = p1.z - p0.z;
        float dx2 = p2.x - p0.x;
        float dy2 = p2.y - p0.y;
        float dz2 = p2.z - p0.z;

        float du1 = uv1.x - uv0.x;
        float dv1 = uv1.y - uv0.y;
        float du2 = uv2.x - uv0.x;
        float dv2 = uv2.y - uv0.y;

        float r = 1.0f / (du1 * dv2 - dv1 * du2);
        float tx = (dv2 * dx1 - dv1 * dx2) * r;
        float ty = (dv2 * dy1 - dv1 * dy2) * r;
        float tz = (dv2 * dz1 - dv1 * dz2) * r;

        if (isnan(tx) || isinf(tx))
        {
          tx = 1.0f;
          ty = 0.0f;
          tz = 0.0f;
        }

        for (int v = 0; v < 3; v++)
        {
          uint32_t v_idx = tri_indices[i + v];
          ufbx_vec3 pos = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, v_idx);
          ufbx_vec3 norm = ufbx_get_vertex_vec3(&ufbx_m->vertex_normal, v_idx);
          ufbx_vec2 uv = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, v_idx);

          float dot_nt = norm.x * tx + norm.y * ty + norm.z * tz;
          float t_x = tx - dot_nt * norm.x;
          float t_y = ty - dot_nt * norm.y;
          float t_z = tz - dot_nt * norm.z;
          float len = sqrtf(t_x * t_x + t_y * t_y + t_z * t_z);
          if (len > 0.0001f)
          {
            t_x /= len;
            t_y /= len;
            t_z /= len;
          }
          else
          {
            t_x = 1.0f;
            t_y = 0.0f;
            t_z = 0.0f;
          }

          U32 vert_index_in_array = idx; // Store before AddVertex increments it
          AddVertex(node.vertices, idx, pos.x, pos.y, pos.z, norm.x, norm.y,
                    norm.z, uv.x, uv.y, t_x, t_y, t_z);

          if (skin && v_idx < ufbx_m->vertex_indices.count)
          {
            uint32_t geom_v_idx = ufbx_m->vertex_indices.data[v_idx];
            if (geom_v_idx < skin->vertices.count)
            {
              ufbx_skin_vertex skin_vertex = skin->vertices.data[geom_v_idx];

              // Collect weights
              struct WeightInfo
              {
                U32 index;
                F32 weight;
              };
              WeightInfo weights[4] = {};

              size_t num_w = skin_vertex.num_weights;
              if (num_w > 4)
                num_w = 4; // limit to 4 highest (ufbx sorts them by weight)

              float total_weight = 0.0f;
              for (size_t w = 0; w < num_w; w++)
              {
                if ((skin_vertex.weight_begin + w) < skin->weights.count)
                {
                  ufbx_skin_weight skin_weight =
                      skin->weights.data[skin_vertex.weight_begin + w];
                  if (skin_weight.cluster_index < MAX_BONES)
                  {
                    weights[w].index = skin_weight.cluster_index;
                    weights[w].weight = (F32)skin_weight.weight;
                    total_weight += weights[w].weight;
                  }
                }
              }

              // Normalize
              if (total_weight > 0.0001f)
              {
                for (int w = 0; w < 4; w++)
                {
                  node.vertices[vert_index_in_array].bone_indices[w] =
                      weights[w].index;
                  node.vertices[vert_index_in_array].bone_weights[w] =
                      total_weight > 0.0f ? weights[w].weight / total_weight
                                          : 0.0f;
                }
              }
            }
          }
        }
      }
    }
    free(tri_indices);
    node.vertex_count = idx;

    model.nodes[model.num_nodes++] = node;
  }

  if (scene->anim_stacks.count > 0)
  {
    model.has_animation = 1;
    model.ufbx_scene_ptr = scene;
    model.ufbx_anim_ptr = scene->anim_stacks.data[0]->anim;
  }
  else
  {
    ufbx_free_scene(scene);
  }

  return model;
}
