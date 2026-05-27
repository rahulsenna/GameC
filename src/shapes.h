#pragma once
#include "game.h"
#include "base_arena.h"

// All functions allocate the returned vertices from the provided arena.
MeshData CreateCube(Arena *arena, float size = 1.0f);
MeshData CreateCuboid(Arena *arena, float width, float height, float depth);
MeshData CreateSphere(Arena *arena, float radius = 0.5f, int sectors = 36, int stacks = 18);
MeshData CreateTorus(Arena *arena, float main_radius = 0.5f, float tube_radius = 0.2f, int main_segments = 36, int tube_segments = 18);
MeshData CreateCylinder(Arena *arena, float radius = 0.5f, float height = 1.0f, int sectors = 36);
MeshData CreateCone(Arena *arena, float radius = 0.5f, float height = 1.0f, int sectors = 36);
MeshData CreateTriangularPyramid(Arena *arena, float size = 1.0f);
MeshData CreateSquarePyramid(Arena *arena, float base_size = 1.0f, float height = 1.0f);
MeshData CreateTriangularPrism(Arena *arena, float width = 1.0f, float height = 1.0f, float depth = 1.0f);
