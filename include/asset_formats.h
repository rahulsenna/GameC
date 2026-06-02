#pragma once
#include "base_types.h"
#include <math.h>

// ============================================================================
// Cooked Asset Binary Format Definitions
//
// These structs define the on-disk binary format for cooked assets.
// They are shared between the AssetBuilder (offline tool) and the engine
// (runtime loader). All data is tightly packed and can be loaded directly
// into memory with a single fread().
// ============================================================================

// --- Magic numbers for format validation ---
#define MESH_MAGIC 0x4853454D // "MESH" in little-endian
#define TEX_MAGIC 0x00584554  // "TEX\0" in little-endian

#define MESH_VERSION 1
#define TEX_VERSION 1

// --- .tex File Header ---
// Stores pixel data, optionally compressed, optionally with a full mip chain.
enum TexFormat : U32
{
  TexFormat_RGBA8_UNorm = 0,
  TexFormat_ASTC4x4_UNorm = 1,
  TexFormat_ASTC4x4_sRGB = 2
};

struct CookedTexFileHeader
{
  U32 magic;   // TEX_MAGIC
  U32 version; // TEX_VERSION
  U32 width;
  U32 height;
  TexFormat format;
  U32 num_mips; // 1 = base level only
  U32 is_orm;   // 1 = this is a channel-packed ORM texture
  U32 _pad;
  // Followed by: U32 mip_offsets[num_mips] (relative to the start of pixel data
  // block) Followed by: U32 mip_sizes[num_mips] Followed by: raw pixel data
};

// --- Cooked Vertex (matches the runtime Vertex struct layout) ---
struct CookedVertex
{
  float position[3];
  float normal[3];
  float tangent[3];
  float tex_coord[2];
  U32 bone_indices[4];
  float bone_weights[4];
};

// --- Bone Info (for skeleton mapping) ---
#define MAX_BONE_NAME_LEN 64

struct CookedBoneInfo
{
  char name[MAX_BONE_NAME_LEN];
  float inverse_bind_matrix[16]; // column-major 4x4
};

// --- Bounding Box ---
struct CookedBounds
{
  float min[3];
  float max[3];
};

// --- Sub-Mesh within a .mesh file ---
struct CookedSubMeshHeader
{
  U32 vertex_count;
  U32 bone_count;
  // Texture slot indices into the file's texture name table
  U32 tex_albedo_index; // 0xFFFFFFFF = no texture
  U32 tex_normal_index; // 0xFFFFFFFF = no texture
  U32 tex_orm_index;    // 0xFFFFFFFF = no texture (packed ORM)
  CookedBounds bounds;
  // Followed by: CookedVertex[vertex_count]
  // Followed by: CookedBoneInfo[bone_count]
};

// --- Texture Name Entry ---
#define MAX_TEX_NAME_LEN 128

struct CookedTexName
{
  char name[MAX_TEX_NAME_LEN]; // relative path to the cooked .tex file
};

// --- .mesh File Header ---
struct CookedMeshFileHeader
{
  U32 magic;   // MESH_MAGIC
  U32 version; // MESH_VERSION
  U32 num_submeshes;
  U32 num_texture_names;
  B32 has_animation;
  U32 _pad[3]; // Padding for alignment / future use
  // Followed by: CookedTexName[num_texture_names]
  // Followed by: CookedSubMeshHeader + data for each submesh
};
