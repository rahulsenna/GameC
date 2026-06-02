// ============================================================================
// AssetBuilder - Offline Asset Cooking Tool
//
// Standalone CLI that converts raw DCC files from assets_src/ into
// engine-ready binary files in assets_cooked/.
//
// Usage: ./build/asset_builder [--force]
//   --force : Re-cook all assets regardless of timestamps
// ============================================================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ufbx.h"

#include "asset_formats.h"
#include "base_types.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_BONES 128

namespace fs = std::filesystem;

// ============================================================================
// Utility
// ============================================================================

static bool NeedsRecook(const fs::path &src, const fs::path &dst)
{
  if (!fs::exists(dst))
    return true;
  auto src_time = fs::last_write_time(src);
  auto dst_time = fs::last_write_time(dst);
  return src_time > dst_time;
}

static void EnsureDir(const fs::path &dir)
{
  if (!fs::exists(dir))
    fs::create_directories(dir);
}

// ============================================================================
// Texture Cooking
// ============================================================================

static bool CookTexture(const fs::path &src_path, const fs::path &dst_path,
                        bool is_orm)
{
  int width, height, channels;
  stbi_set_flip_vertically_on_load(true);
  unsigned char *data =
      stbi_load(src_path.c_str(), &width, &height, &channels, 4);
  if (!data)
  {
    printf("  [ERROR] Failed to load texture: %s\n", src_path.c_str());
    return false;
  }

  EnsureDir(dst_path.parent_path());

  FILE *f = fopen(dst_path.c_str(), "wb");
  if (!f)
  {
    printf("  [ERROR] Failed to write: %s\n", dst_path.c_str());
    stbi_image_free(data);
    return false;
  }

  CookedTexFileHeader header = {};
  header.magic = TEX_MAGIC;
  header.version = TEX_VERSION;
  header.width = (U32)width;
  header.height = (U32)height;
  header.channels = 4;
  header.num_mips = 1;
  header.is_orm = is_orm ? 1 : 0;

  fwrite(&header, sizeof(header), 1, f);
  fwrite(data, width * height * 4, 1, f);

  fclose(f);
  stbi_image_free(data);
  return true;
}

// Cook an ORM texture from separate AO, Roughness, and Metallic maps.
// R = AO, G = Roughness, B = Metallic
static bool CookORMTexture(const fs::path &ao_path,
                           const fs::path &roughness_path,
                           const fs::path &metallic_path,
                           const fs::path &dst_path, bool invert_roughness)
{
  int w_ao = 0, h_ao = 0, w_r = 0, h_r = 0, w_m = 0, h_m = 0;
  int ch;
  stbi_set_flip_vertically_on_load(true);

  unsigned char *ao_data =
      fs::exists(ao_path) ? stbi_load(ao_path.c_str(), &w_ao, &h_ao, &ch, 1)
                          : nullptr;
  unsigned char *roughness_data =
      fs::exists(roughness_path)
          ? stbi_load(roughness_path.c_str(), &w_r, &h_r, &ch, 1)
          : nullptr;
  unsigned char *metallic_data =
      fs::exists(metallic_path)
          ? stbi_load(metallic_path.c_str(), &w_m, &h_m, &ch, 1)
          : nullptr;

  // Determine output dimensions from whichever map is available
  int width = 0, height = 0;
  if (ao_data)
  {
    width = w_ao;
    height = h_ao;
  }
  if (roughness_data)
  {
    width = BASE_MAX(width, w_r);
    height = BASE_MAX(height, h_r);
  }
  if (metallic_data)
  {
    width = BASE_MAX(width, w_m);
    height = BASE_MAX(height, h_m);
  }

  if (width == 0 || height == 0)
  {
    printf("  [WARN] No valid ORM source maps found, skipping ORM pack\n");
    if (ao_data)
      stbi_image_free(ao_data);
    if (roughness_data)
      stbi_image_free(roughness_data);
    if (metallic_data)
      stbi_image_free(metallic_data);
    return false;
  }

  // Allocate RGBA output
  unsigned char *out = (unsigned char *)malloc(width * height * 4);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int i = y * width + x;

      // Sample from each map (nearest-neighbor if sizes differ)
      unsigned char r_ao = 255;
      if (ao_data && w_ao > 0 && h_ao > 0)
      {
        int sx = x * w_ao / width;
        int sy = y * h_ao / height;
        r_ao = ao_data[sy * w_ao + sx];
      }

      unsigned char g_rough = 255;
      if (roughness_data && w_r > 0 && h_r > 0)
      {
        int sx = x * w_r / width;
        int sy = y * h_r / height;
        g_rough = roughness_data[sy * w_r + sx];
        if (invert_roughness)
          g_rough = 255 - g_rough;
      }

      unsigned char b_metal = 0;
      if (metallic_data && w_m > 0 && h_m > 0)
      {
        int sx = x * w_m / width;
        int sy = y * h_m / height;
        b_metal = metallic_data[sy * w_m + sx];
      }

      out[i * 4 + 0] = r_ao;
      out[i * 4 + 1] = g_rough;
      out[i * 4 + 2] = b_metal;
      out[i * 4 + 3] = 255;
    }
  }

  EnsureDir(dst_path.parent_path());

  FILE *f = fopen(dst_path.c_str(), "wb");
  if (!f)
  {
    printf("  [ERROR] Failed to write: %s\n", dst_path.c_str());
    free(out);
    if (ao_data)
      stbi_image_free(ao_data);
    if (roughness_data)
      stbi_image_free(roughness_data);
    if (metallic_data)
      stbi_image_free(metallic_data);
    return false;
  }

  CookedTexFileHeader header = {};
  header.magic = TEX_MAGIC;
  header.version = TEX_VERSION;
  header.width = (U32)width;
  header.height = (U32)height;
  header.channels = 4;
  header.num_mips = 1;
  header.is_orm = 1;

  fwrite(&header, sizeof(header), 1, f);
  fwrite(out, width * height * 4, 1, f);

  fclose(f);
  free(out);
  if (ao_data)
    stbi_image_free(ao_data);
  if (roughness_data)
    stbi_image_free(roughness_data);
  if (metallic_data)
    stbi_image_free(metallic_data);

  printf("  [ORM] Packed → %s (%dx%d)\n", dst_path.c_str(), width, height);
  return true;
}

// ============================================================================
// Mesh Cooking
// ============================================================================

struct CookTexRef
{
  std::string cooked_name; // relative path under assets_cooked/
  U32 index;               // index in the texture name table
};

static bool CookFBX(const fs::path &src_path, const fs::path &dst_dir,
                    const std::string &base_name, bool force)
{
  fs::path mesh_path = dst_dir / (base_name + ".mesh");
  if (!force && !NeedsRecook(src_path, mesh_path))
  {
    printf("  [SKIP] %s (up to date)\n", base_name.c_str());
    return true;
  }

  printf("  [COOK] %s → %s\n", src_path.filename().c_str(),
         mesh_path.filename().c_str());

  ufbx_load_opts opts = {};
  opts.target_axes = ufbx_axes_right_handed_y_up;
  opts.target_unit_meters = 1.0f;

  ufbx_error error;
  ufbx_scene *scene = ufbx_load_file(src_path.c_str(), &opts, &error);
  if (!scene)
  {
    printf("  [ERROR] Failed to load FBX: %s\n", error.description.data);
    return false;
  }

  // --- Collect all referenced textures ---
  std::unordered_map<const void *, CookTexRef> tex_cache;
  std::vector<CookedTexName> tex_names;

  auto register_texture = [&](ufbx_texture *tex, const std::string &suffix,
                              bool invert = false) -> U32
  {
    if (!tex || tex->type != UFBX_TEXTURE_FILE || !tex->content.data)
      return 0xFFFFFFFF;

    auto it = tex_cache.find(tex->content.data);
    if (it != tex_cache.end())
      return it->second.index;

    // Decode and write this texture as a .tex file
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *pixels = stbi_load_from_memory(
        (const stbi_uc *)tex->content.data, (int)tex->content.size, &width,
        &height, &channels, 4);
    if (!pixels)
      return 0xFFFFFFFF;

    if (invert)
    {
      for (int i = 0; i < width * height * 4; i += 4)
      {
        pixels[i] = 255 - pixels[i];
        pixels[i + 1] = 255 - pixels[i + 1];
        pixels[i + 2] = 255 - pixels[i + 2];
      }
    }

    fs::path orig_path = tex->filename.data;
    std::string orig_stem = orig_path.stem().string();
    if (orig_stem.empty())
      orig_stem = std::to_string(tex_cache.size());

    std::string tex_name = base_name + "_" + orig_stem + suffix + ".tex";
    fs::path tex_path = dst_dir / tex_name;

    EnsureDir(tex_path.parent_path());
    FILE *tf = fopen(tex_path.c_str(), "wb");
    if (tf)
    {
      CookedTexFileHeader th = {};
      th.magic = TEX_MAGIC;
      th.version = TEX_VERSION;
      th.width = (U32)width;
      th.height = (U32)height;
      th.channels = 4;
      th.num_mips = 1;
      th.is_orm = 0;

      fwrite(&th, sizeof(th), 1, tf);
      fwrite(pixels, width * height * 4, 1, tf);
      fclose(tf);
      printf("    [TEX] %s (%dx%d)\n", tex_name.c_str(), width, height);
    }
    stbi_image_free(pixels);

    U32 idx = (U32)tex_names.size();
    CookedTexName tn = {};
    strncpy(tn.name, tex_name.c_str(), MAX_TEX_NAME_LEN - 1);
    tex_names.push_back(tn);
    tex_cache[tex->content.data] = {tex_name, idx};
    return idx;
  };

  // --- Process Meshes ---
  struct SubMeshData
  {
    CookedSubMeshHeader header;
    std::vector<CookedVertex> vertices;
    std::vector<CookedBoneInfo> bones;
  };
  std::vector<SubMeshData> submeshes;

  for (size_t mesh_i = 0; mesh_i < scene->meshes.count; mesh_i++)
  {
    ufbx_mesh *ufbx_m = scene->meshes.data[mesh_i];
    if (!ufbx_m)
      continue;

    SubMeshData sm = {};
    sm.header.tex_albedo_index = 0xFFFFFFFF;
    sm.header.tex_normal_index = 0xFFFFFFFF;
    sm.header.tex_orm_index = 0xFFFFFFFF;

    // --- Extract material textures ---
    // For embedded FBX textures, we extract and cook them individually.
    // For ORM packing of separate on-disk maps, we handle that in the
    // standalone texture pass below.
    U32 metallic_idx = 0xFFFFFFFF;
    U32 roughness_idx = 0xFFFFFFFF;
    U32 ao_idx = 0xFFFFFFFF;

    if (ufbx_m->materials.count > 0)
    {
      ufbx_material *mat = ufbx_m->materials.data[0];
      for (size_t tex_i = 0; tex_i < mat->textures.count; tex_i++)
      {
        ufbx_material_texture mat_tex = mat->textures.data[tex_i];
        const char *prop = mat_tex.material_prop.data;

        bool is_albedo = strcmp(prop, "DiffuseColor") == 0 ||
                         strcmp(prop, "base_color") == 0;
        bool is_normal = strcmp(prop, "NormalMap") == 0 ||
                         strcmp(prop, "normal_map") == 0 ||
                         strcmp(prop, "Bump") == 0;
        bool is_metallic =
            strcmp(prop, "metalness") == 0 || strcmp(prop, "metallic") == 0;
        bool is_roughness = strcmp(prop, "Shininess") == 0 ||
                            strcmp(prop, "ShininessExponent") == 0 ||
                            strcmp(prop, "roughness") == 0;
        bool is_ao = strcmp(prop, "AmbientColor") == 0 ||
                     strcmp(prop, "ambient_occlusion") == 0;

        bool invert = is_roughness && (strcmp(prop, "ShininessExponent") == 0);

        if (is_albedo)
        {
          sm.header.tex_albedo_index =
              register_texture(mat_tex.texture, "_albedo");
        }
        else if (is_normal)
        {
          sm.header.tex_normal_index =
              register_texture(mat_tex.texture, "_normal");
        }
        else if (is_metallic)
        {
          metallic_idx = register_texture(mat_tex.texture, "_metallic");
        }
        else if (is_roughness)
        {
          roughness_idx =
              register_texture(mat_tex.texture, "_roughness", invert);
        }
        else if (is_ao)
        {
          ao_idx = register_texture(mat_tex.texture, "_ao");
        }
      }

      // If we have separate metallic/roughness/ao from embedded textures,
      // we'll just keep them as separate .tex files for now and reference
      // the first available as the ORM slot. True ORM packing of separate
      // embedded textures would require reading them back, which is complex.
      // We'll handle ORM packing for standalone texture sets below.
      // For now, store the metallic index in the ORM slot as a fallback.
      if (metallic_idx != 0xFFFFFFFF || roughness_idx != 0xFFFFFFFF ||
          ao_idx != 0xFFFFFFFF)
      {
        // Use the first available map as a placeholder for the ORM slot.
        // True ORM packing for embedded FBX textures isn't implemented yet,
        // but this allows the engine to at least bind the map and sample it.
        if (metallic_idx != 0xFFFFFFFF)
          sm.header.tex_orm_index = metallic_idx;
        else if (roughness_idx != 0xFFFFFFFF)
          sm.header.tex_orm_index = roughness_idx;
        else
          sm.header.tex_orm_index = ao_idx;
      }
    }

    // --- Extract Geometry ---
    size_t max_tris = 0;
    for (size_t i = 0; i < ufbx_m->faces.count; i++)
      max_tris += ufbx_m->max_face_triangles;

    sm.vertices.reserve(max_tris * 3);

    // Bounds tracking
    float bmin[3] = {1e30f, 1e30f, 1e30f};
    float bmax[3] = {-1e30f, -1e30f, -1e30f};

    // Skinning
    ufbx_skin_deformer *skin = nullptr;
    if (ufbx_m->skin_deformers.count > 0)
    {
      skin = ufbx_m->skin_deformers.data[0];
      for (size_t c = 0;
           c < skin->clusters.count && sm.bones.size() < MAX_BONES; c++)
      {
        ufbx_skin_cluster *cluster = skin->clusters.data[c];
        CookedBoneInfo bone = {};

        // Copy bone name
        if (cluster->bone_node)
        {
          strncpy(bone.name, cluster->bone_node->name.data,
                  MAX_BONE_NAME_LEN - 1);
        }

        // Inverse bind matrix (column-major)
        bone.inverse_bind_matrix[0] = (float)cluster->geometry_to_bone.m00;
        bone.inverse_bind_matrix[1] = (float)cluster->geometry_to_bone.m10;
        bone.inverse_bind_matrix[2] = (float)cluster->geometry_to_bone.m20;
        bone.inverse_bind_matrix[3] = 0.0f;
        bone.inverse_bind_matrix[4] = (float)cluster->geometry_to_bone.m01;
        bone.inverse_bind_matrix[5] = (float)cluster->geometry_to_bone.m11;
        bone.inverse_bind_matrix[6] = (float)cluster->geometry_to_bone.m21;
        bone.inverse_bind_matrix[7] = 0.0f;
        bone.inverse_bind_matrix[8] = (float)cluster->geometry_to_bone.m02;
        bone.inverse_bind_matrix[9] = (float)cluster->geometry_to_bone.m12;
        bone.inverse_bind_matrix[10] = (float)cluster->geometry_to_bone.m22;
        bone.inverse_bind_matrix[11] = 0.0f;
        bone.inverse_bind_matrix[12] = (float)cluster->geometry_to_bone.m03;
        bone.inverse_bind_matrix[13] = (float)cluster->geometry_to_bone.m13;
        bone.inverse_bind_matrix[14] = (float)cluster->geometry_to_bone.m23;
        bone.inverse_bind_matrix[15] = 1.0f;

        sm.bones.push_back(bone);
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

        // Compute tangent from triangle UVs
        ufbx_vec3 p0 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i0);
        ufbx_vec3 p1 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i1);
        ufbx_vec3 p2 = ufbx_get_vertex_vec3(&ufbx_m->vertex_position, i2);

        ufbx_vec2 uv0 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i0);
        ufbx_vec2 uv1 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i1);
        ufbx_vec2 uv2 = ufbx_get_vertex_vec2(&ufbx_m->vertex_uv, i2);

        float dx1 = (float)(p1.x - p0.x);
        float dy1 = (float)(p1.y - p0.y);
        float dz1 = (float)(p1.z - p0.z);
        float dx2 = (float)(p2.x - p0.x);
        float dy2 = (float)(p2.y - p0.y);
        float dz2 = (float)(p2.z - p0.z);
        float du1 = (float)(uv1.x - uv0.x);
        float dv1 = (float)(uv1.y - uv0.y);
        float du2 = (float)(uv2.x - uv0.x);
        float dv2 = (float)(uv2.y - uv0.y);

        float r = 1.0f / (du1 * dv2 - dv1 * du2);
        float tx = (dv2 * dx1 - dv1 * dx2) * r;
        float ty = (dv2 * dy1 - dv1 * dy2) * r;
        float tz = (dv2 * dz1 - dv1 * dz2) * r;

        if (std::isnan(tx) || std::isinf(tx))
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

          // Gram-Schmidt orthogonalize tangent
          float dot_nt = (float)(norm.x * tx + norm.y * ty + norm.z * tz);
          float t_x = tx - dot_nt * (float)norm.x;
          float t_y = ty - dot_nt * (float)norm.y;
          float t_z = tz - dot_nt * (float)norm.z;
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

          CookedVertex cv = {};
          cv.position[0] = (float)pos.x;
          cv.position[1] = (float)pos.y;
          cv.position[2] = (float)pos.z;
          cv.normal[0] = (float)norm.x;
          cv.normal[1] = (float)norm.y;
          cv.normal[2] = (float)norm.z;
          cv.tangent[0] = t_x;
          cv.tangent[1] = t_y;
          cv.tangent[2] = t_z;
          cv.tex_coord[0] = (float)uv.x;
          cv.tex_coord[1] = (float)uv.y;

          // Update bounds
          for (int a = 0; a < 3; a++)
          {
            if (cv.position[a] < bmin[a])
              bmin[a] = cv.position[a];
            if (cv.position[a] > bmax[a])
              bmax[a] = cv.position[a];
          }

          // Skinning
          if (skin && v_idx < ufbx_m->vertex_indices.count)
          {
            uint32_t geom_v_idx = ufbx_m->vertex_indices.data[v_idx];
            if (geom_v_idx < skin->vertices.count)
            {
              ufbx_skin_vertex sv = skin->vertices.data[geom_v_idx];
              size_t num_w = sv.num_weights;
              if (num_w > 4)
                num_w = 4;

              float total_weight = 0.0f;
              for (size_t w = 0; w < num_w; w++)
              {
                if ((sv.weight_begin + w) < skin->weights.count)
                {
                  ufbx_skin_weight sw = skin->weights.data[sv.weight_begin + w];
                  if (sw.cluster_index < MAX_BONES)
                  {
                    cv.bone_indices[w] = (U32)sw.cluster_index;
                    cv.bone_weights[w] = (float)sw.weight;
                    total_weight += cv.bone_weights[w];
                  }
                }
              }
              if (total_weight > 0.0001f)
              {
                for (int w = 0; w < 4; w++)
                  cv.bone_weights[w] /= total_weight;
              }
            }
          }

          sm.vertices.push_back(cv);
        }
      }
    }
    free(tri_indices);

    sm.header.vertex_count = (U32)sm.vertices.size();
    sm.header.bone_count = (U32)sm.bones.size();
    memcpy(sm.header.bounds.min, bmin, sizeof(bmin));
    memcpy(sm.header.bounds.max, bmax, sizeof(bmax));

    submeshes.push_back(sm);
    printf("    [MESH] submesh %zu: %u verts, %u bones\n", submeshes.size() - 1,
           sm.header.vertex_count, sm.header.bone_count);
  }

  // --- Write .mesh file ---
  EnsureDir(dst_dir);
  FILE *f = fopen(mesh_path.c_str(), "wb");
  if (!f)
  {
    printf("  [ERROR] Failed to write: %s\n", mesh_path.c_str());
    ufbx_free_scene(scene);
    return false;
  }

  CookedMeshFileHeader file_header = {};
  file_header.magic = MESH_MAGIC;
  file_header.version = MESH_VERSION;
  file_header.num_submeshes = (U32)submeshes.size();
  file_header.num_texture_names = (U32)tex_names.size();
  file_header.has_animation = (scene->anim_stacks.count > 0) ? 1 : 0;

  fwrite(&file_header, sizeof(file_header), 1, f);

  // Write texture name table
  for (auto &tn : tex_names)
    fwrite(&tn, sizeof(CookedTexName), 1, f);

  // Write submeshes
  for (auto &sm : submeshes)
  {
    fwrite(&sm.header, sizeof(CookedSubMeshHeader), 1, f);
    fwrite(sm.vertices.data(), sizeof(CookedVertex), sm.vertices.size(), f);
    fwrite(sm.bones.data(), sizeof(CookedBoneInfo), sm.bones.size(), f);
  }

  fclose(f);
  ufbx_free_scene(scene);

  printf("  [OK] %s (%u submeshes, %u textures)\n",
         mesh_path.filename().c_str(), file_header.num_submeshes,
         file_header.num_texture_names);
  return true;
}

// ============================================================================
// Texture Set Discovery & ORM Packing
// ============================================================================

// Detects PBR texture sets by naming convention:
//   *_albedo.png, *_normal-ogl.png, *_metallic.png, *_roughness.png, *_ao.png
static void CookTextureSet(const fs::path &src_dir, const fs::path &dst_dir,
                           const std::string &set_name, bool force)
{
  auto str_ends_with = [](const std::string &s,
                          const std::string &suffix) -> bool
  {
    if (suffix.size() > s.size())
      return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  };

  auto find_tex = [&](const std::string &suffix) -> fs::path
  {
    for (auto &entry : fs::directory_iterator(src_dir))
    {
      std::string name = entry.path().filename().string();
      if (name.find(suffix) != std::string::npos &&
          (str_ends_with(name, ".png") || str_ends_with(name, ".tga") ||
           str_ends_with(name, ".jpg")))
        return entry.path();
    }
    return {};
  };

  fs::path albedo_src = find_tex("_albedo");
  fs::path normal_src = find_tex("_normal");
  fs::path metallic_src = find_tex("_metallic");
  fs::path roughness_src = find_tex("_roughness");
  fs::path ao_src = find_tex("_ao");

  EnsureDir(dst_dir);

  // Cook individual textures
  if (!albedo_src.empty())
  {
    fs::path dst = dst_dir / (set_name + "_albedo.tex");
    if (force || NeedsRecook(albedo_src, dst))
    {
      printf("  [TEX] %s\n", albedo_src.filename().c_str());
      CookTexture(albedo_src, dst, false);
    }
  }

  if (!normal_src.empty())
  {
    fs::path dst = dst_dir / (set_name + "_normal.tex");
    if (force || NeedsRecook(normal_src, dst))
    {
      printf("  [TEX] %s\n", normal_src.filename().c_str());
      CookTexture(normal_src, dst, false);
    }
  }

  // ORM Channel Pack
  if (!metallic_src.empty() || !roughness_src.empty() || !ao_src.empty())
  {
    fs::path orm_dst = dst_dir / (set_name + "_orm.tex");
    bool any_newer = force;
    if (!any_newer && !metallic_src.empty())
      any_newer = NeedsRecook(metallic_src, orm_dst);
    if (!any_newer && !roughness_src.empty())
      any_newer = NeedsRecook(roughness_src, orm_dst);
    if (!any_newer && !ao_src.empty())
      any_newer = NeedsRecook(ao_src, orm_dst);

    if (any_newer)
    {
      CookORMTexture(ao_src, roughness_src, metallic_src, orm_dst, false);
    }
  }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[])
{
  bool force = false;
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--force") == 0)
      force = true;
  }

  fs::path root = fs::current_path();
  fs::path src_dir = root / "assets_src";
  fs::path dst_dir = root / "assets_cooked";

  if (!fs::exists(src_dir))
  {
    printf("ERROR: assets_src/ directory not found at %s\n", src_dir.c_str());
    printf("Please create it and move your raw assets there.\n");
    return 1;
  }

  EnsureDir(dst_dir);

  printf("========================================\n");
  printf("  AssetBuilder v1.0\n");
  printf("  Source: %s\n", src_dir.c_str());
  printf("  Output: %s\n", dst_dir.c_str());
  if (force)
    printf("  Mode: FORCE (re-cooking all)\n");
  printf("========================================\n\n");

  int meshes_cooked = 0;
  int tex_sets_cooked = 0;
  int standalone_tex_cooked = 0;

  // --- Pass 1: Cook FBX meshes ---
  printf("--- Pass 1: Mesh Cooking ---\n");
  for (auto &entry : fs::recursive_directory_iterator(src_dir))
  {
    if (!entry.is_regular_file())
      continue;
    if (entry.path().extension() != ".fbx")
      continue;

    std::string rel = fs::relative(entry.path(), src_dir).string();
    std::string base_name = entry.path().stem().string();

    // Preserve subdirectory structure
    fs::path rel_dir = fs::relative(entry.path().parent_path(), src_dir);
    fs::path out_dir = dst_dir / rel_dir;

    if (CookFBX(entry.path(), out_dir, base_name, force))
      meshes_cooked++;
  }
  printf("\n");

  // --- Pass 2: Cook standalone texture sets (PBR material folders) ---
  printf("--- Pass 2: Texture Set Cooking (ORM Packing) ---\n");
  for (auto &entry : fs::directory_iterator(src_dir))
  {
    if (!entry.is_directory())
      continue;

    // Check if this directory contains PBR textures
    std::string set_name = entry.path().filename().string();
    bool has_pbr = false;
    for (auto &file : fs::directory_iterator(entry.path()))
    {
      std::string fname = file.path().filename().string();
      if (fname.find("_albedo") != std::string::npos ||
          fname.find("_metallic") != std::string::npos ||
          fname.find("_roughness") != std::string::npos)
      {
        has_pbr = true;
        break;
      }
    }

    if (has_pbr)
    {
      printf("[SET] %s\n", set_name.c_str());
      fs::path tex_out_dir = dst_dir / set_name;
      CookTextureSet(entry.path(), tex_out_dir, set_name, force);
      tex_sets_cooked++;
    }
  }
  printf("\n");

  // --- Pass 3: Cook standalone textures (loose .png/.jpg files) ---
  printf("--- Pass 3: Standalone Textures ---\n");
  for (auto &entry : fs::directory_iterator(src_dir))
  {
    if (!entry.is_regular_file())
      continue;
    std::string ext = entry.path().extension().string();
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga")
      continue;

    std::string base_name = entry.path().stem().string();
    fs::path dst = dst_dir / (base_name + ".tex");

    if (force || NeedsRecook(entry.path(), dst))
    {
      printf("  [TEX] %s\n", entry.path().filename().c_str());
      CookTexture(entry.path(), dst, false);
      standalone_tex_cooked++;
    }
  }
  printf("\n");

  // --- Pass 4: Copy animation files as-is (ozz format is already cooked) ---
  printf("--- Pass 4: Animations (copy .ozz files) ---\n");
  fs::path anim_src = src_dir / "animations";
  fs::path anim_dst = dst_dir / "animations";
  if (fs::exists(anim_src))
  {
    EnsureDir(anim_dst);
    int copied = 0;
    for (auto &entry : fs::directory_iterator(anim_src))
    {
      if (entry.path().extension() != ".ozz")
        continue;
      fs::path dst = anim_dst / entry.path().filename();
      if (force || NeedsRecook(entry.path(), dst))
      {
        fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
        copied++;
      }
    }
    printf("  Copied %d .ozz files\n", copied);
  }
  printf("\n");

  printf("========================================\n");
  printf("  Done! Cooked %d meshes, %d texture sets, %d standalone textures\n",
         meshes_cooked, tex_sets_cooked, standalone_tex_cooked);
  printf("========================================\n");

  return 0;
}
