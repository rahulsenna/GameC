#pragma once
#include "game.h"

// Forward declarations to avoid including Metal/Metal.hpp in Objective-C files
// and compiling them as C++
#ifdef __cplusplus
extern "C"
{
#endif

  // Create the Metal device and return as void*
  void *Renderer_CreateDevice();

  // Initialize the Metal C++ renderer.
  // device: id<MTLDevice> mapped to void*
  // metal_layer: CAMetalLayer* mapped to void*
  void Renderer_Init(void *device, void *metal_layer);

  // Load or reload shaders
  void Renderer_LoadShaders();

  // Render a single frame from the game output
  void Renderer_RenderFrame(GameOutput *output);

#ifdef __cplusplus
}
#endif
