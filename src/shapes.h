#pragma once
#include "base_arena.h"
#include "game.h"

// All functions allocate the returned vertices from the provided arena.
FBXModel CreateCube(Arena *arena, float size = 1.0f);
FBXModel CreateCuboid(Arena *arena, float width, float height, float depth);
FBXModel CreateSphere(Arena *arena, float radius = 0.5f, int sectors = 36,
                      int stacks = 18);
FBXModel CreateTorus(Arena *arena, float main_radius = 0.5f,
                     float tube_radius = 0.2f, int main_segments = 36,
                     int tube_segments = 18);
FBXModel CreateCylinder(Arena *arena, float radius = 0.5f, float height = 1.0f,
                        int sectors = 36);
FBXModel CreateCone(Arena *arena, float radius = 0.5f, float height = 1.0f,
                    int sectors = 36);
FBXModel CreateTriangularPyramid(Arena *arena, float size = 1.0f);
FBXModel CreateSquarePyramid(Arena *arena, float base_size = 1.0f,
                             float height = 1.0f);
FBXModel CreateTriangularPrism(Arena *arena, float width = 1.0f,
                               float height = 1.0f, float depth = 1.0f);
FBXModel CreatePlane(Arena *arena, float size = 1000.0f);
FBXModel LoadFBX(Arena *arena, const char *filepath, RenderGroup *render_group,
                 U32 *next_texture_handle, MaterialTextures default_textures);
