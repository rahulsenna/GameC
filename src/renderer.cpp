#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "renderer.h"

// Undefine macros that conflict with metal-cpp methods
#undef Min
#undef Max
#include <Metal/Metal.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <unordered_map>

static MTL::Device *global_device = nullptr;
static CA::MetalLayer *global_metal_layer = nullptr;
static MTL::CommandQueue *global_command_queue = nullptr;
static MTL::RenderPipelineState *global_pipeline_state = nullptr;
static MTL::RenderPipelineState *global_grid_pipeline_state = nullptr;
static MTL::DepthStencilState *global_depth_state = nullptr;
static MTL::Texture *global_depth_texture = nullptr;

static std::unordered_map<U32, MTL::Texture *> global_textures;

static MTL::Buffer *global_gpu_heap = nullptr;
static MTL::Buffer *global_root_buffer = nullptr;
static MTL::ArgumentEncoder *global_texture_argument_encoder = nullptr;
static MTL::Buffer *global_texture_argument_buffer = nullptr;

// Triple-buffered frame arenas — the first GPU_FRAME_ARENA_TOTAL bytes of
// global_gpu_heap are reserved for these.  Each frame we bump-allocate dynamic
// data (currently: bone matrices) into the current sub-arena and reset at the
// end of the frame.  Three arenas ensure the GPU has finished reading the
// previous frame's data before the CPU overwrites it again.
static U32 global_frame_index = 0;
static U32 global_frame_arena_bump = 0; // bytes used in the current frame arena

static U32 FrameArenaBase()
{
  return (global_frame_index % GPU_FRAME_ARENA_COUNT) * GPU_FRAME_ARENA_SIZE;
}

// Allocate `size` bytes from the current frame arena (16-byte aligned).
// Returns the byte offset within global_gpu_heap.
static U32 FrameArenaAlloc(U32 size)
{
  global_frame_arena_bump = (global_frame_arena_bump + 15u) & ~15u;
  U32 offset = FrameArenaBase() + global_frame_arena_bump;
  global_frame_arena_bump += size;
  // Caller must ensure allocations stay within GPU_FRAME_ARENA_SIZE.
  return offset;
}

extern "C" void Renderer_LoadShaders()
{
  std::string fullPath =
      std::filesystem::current_path().string() + "/build/shaders.metallib";
  NS::String *libPath =
      NS::String::string(fullPath.c_str(), NS::UTF8StringEncoding);
  NS::Error *error = nullptr;

  MTL::Library *defaultLibrary = global_device->newLibrary(libPath, &error);
  if (!defaultLibrary)
  {
    std::cerr << "Failed to load metallib: "
              << (error ? error->localizedDescription()->utf8String()
                        : "unknown error")
              << std::endl;
    return;
  }

  MTL::Function *vertexFunction = defaultLibrary->newFunction(
      NS::String::string("vertex_main", NS::UTF8StringEncoding));
  MTL::Function *fragmentFunction = defaultLibrary->newFunction(
      NS::String::string("fragment_main", NS::UTF8StringEncoding));

  MTL::RenderPipelineDescriptor *pipelineStateDescriptor =
      MTL::RenderPipelineDescriptor::alloc()->init();
  pipelineStateDescriptor->setLabel(
      NS::String::string("Simple Pipeline", NS::UTF8StringEncoding));
  pipelineStateDescriptor->setVertexFunction(vertexFunction);
  pipelineStateDescriptor->setFragmentFunction(fragmentFunction);
  pipelineStateDescriptor->colorAttachments()->object(0)->setPixelFormat(
      MTL::PixelFormatBGRA8Unorm);
  pipelineStateDescriptor->setDepthAttachmentPixelFormat(
      MTL::PixelFormatDepth32Float);

  // Blending
  auto colorAttach = pipelineStateDescriptor->colorAttachments()->object(0);
  colorAttach->setBlendingEnabled(true);
  colorAttach->setRgbBlendOperation(MTL::BlendOperationAdd);
  colorAttach->setAlphaBlendOperation(MTL::BlendOperationAdd);
  colorAttach->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
  colorAttach->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
  colorAttach->setDestinationRGBBlendFactor(
      MTL::BlendFactorOneMinusSourceAlpha);
  colorAttach->setDestinationAlphaBlendFactor(
      MTL::BlendFactorOneMinusSourceAlpha);

  if (global_pipeline_state)
  {
    global_pipeline_state->release();
  }
  global_pipeline_state =
      global_device->newRenderPipelineState(pipelineStateDescriptor, &error);
  if (!global_pipeline_state)
  {
    std::cerr << "Failed to create pipeline state\n";
  }

  // Grid Pipeline
  MTL::Function *gridFragmentFunction = defaultLibrary->newFunction(
      NS::String::string("grid_fragment_main", NS::UTF8StringEncoding));
  pipelineStateDescriptor->setLabel(
      NS::String::string("Grid Pipeline", NS::UTF8StringEncoding));
  pipelineStateDescriptor->setFragmentFunction(gridFragmentFunction);

  if (global_grid_pipeline_state)
  {
    global_grid_pipeline_state->release();
  }
  global_grid_pipeline_state =
      global_device->newRenderPipelineState(pipelineStateDescriptor, &error);
  if (!global_grid_pipeline_state)
  {
    std::cerr << "Failed to create grid pipeline state\n";
  }

  if (!global_texture_argument_encoder)
  {
    global_texture_argument_encoder = fragmentFunction->newArgumentEncoder(2);
    global_texture_argument_buffer = global_device->newBuffer(
        global_texture_argument_encoder->encodedLength(),
        MTL::ResourceStorageModeShared);
    global_texture_argument_encoder->setArgumentBuffer(
        global_texture_argument_buffer, 0);
  }

  gridFragmentFunction->release();
  pipelineStateDescriptor->release();
  fragmentFunction->release();
  vertexFunction->release();
  defaultLibrary->release();
}

static void UpdateDepthTexture(int width, int height)
{
  if (global_depth_texture && global_depth_texture->width() == width &&
      global_depth_texture->height() == height)
  {
    return;
  }

  MTL::TextureDescriptor *desc = MTL::TextureDescriptor::texture2DDescriptor(
      MTL::PixelFormatDepth32Float, width, height, false);
  desc->setUsage(MTL::TextureUsageRenderTarget);
  desc->setStorageMode(MTL::StorageModePrivate);

  if (global_depth_texture)
  {
    global_depth_texture->release();
  }
  global_depth_texture = global_device->newTexture(desc);
}

extern "C" void *Renderer_CreateDevice()
{
  return MTL::CreateSystemDefaultDevice();
}

extern "C" void Renderer_Init(void *device, void *metal_layer)
{
  global_device = (MTL::Device *)device;
  global_metal_layer = (CA::MetalLayer *)metal_layer;
  global_metal_layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  global_command_queue = global_device->newCommandQueue();

  global_gpu_heap = global_device->newBuffer(512 * 1024 * 1024,
                                             MTL::ResourceStorageModeShared);
  global_root_buffer =
      global_device->newBuffer(8, MTL::ResourceStorageModeShared);
  uint64_t *root_ptr = (uint64_t *)global_root_buffer->contents();
  *root_ptr = global_gpu_heap->gpuAddress();

  MTL::DepthStencilDescriptor *depthDesc =
      MTL::DepthStencilDescriptor::alloc()->init();
  depthDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
  depthDesc->setDepthWriteEnabled(true);
  global_depth_state = global_device->newDepthStencilState(depthDesc);
  depthDesc->release();

  Renderer_LoadShaders();
}

extern "C" void Renderer_RenderFrame(GameOutput *output)
{
  CA::MetalDrawable *drawable = global_metal_layer->nextDrawable();
  if (!drawable)
    return;

  UpdateDepthTexture(drawable->texture()->width(),
                     drawable->texture()->height());

  MTL::ClearColor clear_color = MTL::ClearColor::Make(0, 0, 0, 1);
  U32 offset = 0;

  // First pass
  while (offset < output->render_group.size)
  {
    RenderGroupEntryHeader *header =
        (RenderGroupEntryHeader *)(output->render_group.base + offset);
    offset += sizeof(RenderGroupEntryHeader);

    if (header->type == RenderGroupEntryType_Clear)
    {
      RenderGroupEntry_Clear *entry =
          (RenderGroupEntry_Clear *)(output->render_group.base + offset);
      clear_color = MTL::ClearColor::Make(entry->color[0], entry->color[1],
                                          entry->color[2], entry->color[3]);
      offset += sizeof(RenderGroupEntry_Clear);
    }
    else if (header->type == RenderGroupEntryType_UploadTexture)
    {
      RenderGroupEntry_UploadTexture *entry =
          (RenderGroupEntry_UploadTexture *)(output->render_group.base +
                                             offset);
      void *pixels = (void *)(entry + 1);

      MTL::TextureDescriptor *textureDesc =
          MTL::TextureDescriptor::alloc()->init();

      MTL::PixelFormat pixelFormat = MTL::PixelFormatRGBA8Unorm;
      if (entry->format == 1)
        pixelFormat = MTL::PixelFormatASTC_4x4_LDR;
      else if (entry->format == 2)
        pixelFormat = MTL::PixelFormatASTC_4x4_LDR;

      textureDesc->setPixelFormat(pixelFormat);
      textureDesc->setWidth(entry->width);
      textureDesc->setHeight(entry->height);

      NS::UInteger mipLevels = entry->num_mips;
      bool generate_mips = false;
      if (entry->format == 0 && entry->num_mips == 1)
      {
        mipLevels =
            std::floor(std::log2(std::max(entry->width, entry->height))) + 1;
        if (mipLevels > 1)
        {
          generate_mips = true;
        }
      }

      textureDesc->setMipmapLevelCount(mipLevels);
      MTL::Texture *texture = global_device->newTexture(textureDesc);

      U8 *current_pixels = (U8 *)pixels;
      for (U32 mip = 0; mip < entry->num_mips; ++mip)
      {
        U32 mip_width = std::max(1u, entry->width >> mip);
        U32 mip_height = std::max(1u, entry->height >> mip);

        MTL::Region region = MTL::Region::Make2D(0, 0, mip_width, mip_height);

        U32 bytes_per_row = 0;
        U32 mip_size = 0;

        if (entry->format == 0)
        { // RGBA8
          bytes_per_row = mip_width * 4;
          mip_size = mip_width * mip_height * 4;
        }
        else
        { // ASTC 4x4
          U32 blocks_x = (mip_width + 3) / 4;
          U32 blocks_y = (mip_height + 3) / 4;
          bytes_per_row = blocks_x * 16;
          mip_size = blocks_x * blocks_y * 16;
        }

        texture->replaceRegion(region, mip, current_pixels, bytes_per_row);
        current_pixels += mip_size;
      }

      MTL::CommandBuffer *blitCommandBuffer =
          global_command_queue->commandBuffer();
      MTL::BlitCommandEncoder *blitEncoder =
          blitCommandBuffer->blitCommandEncoder();

      if (generate_mips)
      {
        blitEncoder->generateMipmaps(texture);
      }

      blitEncoder->endEncoding();
      blitCommandBuffer->commit();

      if (global_textures.count(entry->handle))
      {
        global_textures[entry->handle]->release();
      }
      global_textures[entry->handle] = texture;
      global_texture_argument_encoder->setTexture(texture, entry->handle);

      textureDesc->release();

      offset += sizeof(RenderGroupEntry_UploadTexture) + entry->data_size;
    }
    else if (header->type == RenderGroupEntryType_UploadGeometry)
    {
      RenderGroupEntry_UploadGeometry *entry =
          (RenderGroupEntry_UploadGeometry *)(output->render_group.base +
                                              offset);
      void *src_data = (void *)(entry + 1);

      U8 *dest = (U8 *)global_gpu_heap->contents() + entry->offset;
      memcpy(dest, src_data, entry->size);

      offset += sizeof(RenderGroupEntry_UploadGeometry) + entry->size;
    }
    else if (header->type == RenderGroupEntryType_DrawMesh)
    {
      RenderGroupEntry_DrawMesh *entry =
          (RenderGroupEntry_DrawMesh *)(output->render_group.base + offset);
      // Skip the fixed struct + optional inline bone data.
      offset += sizeof(RenderGroupEntry_DrawMesh);
      if (entry->uniforms.has_bones)
        offset += MAX_BONES * sizeof(Mat4);
    }
  }

  // Setup Render Pass
  MTL::RenderPassDescriptor *passDescriptor =
      MTL::RenderPassDescriptor::renderPassDescriptor();
  passDescriptor->colorAttachments()->object(0)->setTexture(
      drawable->texture());
  passDescriptor->colorAttachments()->object(0)->setLoadAction(
      MTL::LoadActionClear);
  passDescriptor->colorAttachments()->object(0)->setClearColor(clear_color);
  passDescriptor->colorAttachments()->object(0)->setStoreAction(
      MTL::StoreActionStore);

  passDescriptor->depthAttachment()->setTexture(global_depth_texture);
  passDescriptor->depthAttachment()->setLoadAction(MTL::LoadActionClear);
  passDescriptor->depthAttachment()->setClearDepth(1.0);
  passDescriptor->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);

  MTL::CommandBuffer *commandBuffer = global_command_queue->commandBuffer();
  MTL::RenderCommandEncoder *commandEncoder =
      commandBuffer->renderCommandEncoder(passDescriptor);

  commandEncoder->setDepthStencilState(global_depth_state);
  commandEncoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
  commandEncoder->setCullMode(MTL::CullModeBack);

  commandEncoder->useResource(global_gpu_heap, MTL::ResourceUsageRead,
                              MTL::RenderStageVertex);
  for (auto const &pair : global_textures)
  {
    commandEncoder->useResource(pair.second, MTL::ResourceUsageRead,
                                MTL::RenderStageFragment);
  }

  commandEncoder->setVertexBuffer(global_root_buffer, 0, 0);
  commandEncoder->setFragmentBuffer(global_root_buffer, 0, 0);
  commandEncoder->setFragmentBuffer(global_texture_argument_buffer, 0, 2);

  int current_shader_type = -1;

  // Second pass
  offset = 0;
  while (offset < output->render_group.size)
  {
    RenderGroupEntryHeader *header =
        (RenderGroupEntryHeader *)(output->render_group.base + offset);
    offset += sizeof(RenderGroupEntryHeader);

    if (header->type == RenderGroupEntryType_Clear)
    {
      offset += sizeof(RenderGroupEntry_Clear);
    }
    else if (header->type == RenderGroupEntryType_UploadTexture)
    {
      RenderGroupEntry_UploadTexture *entry =
          (RenderGroupEntry_UploadTexture *)(output->render_group.base +
                                             offset);
      offset += sizeof(RenderGroupEntry_UploadTexture) + entry->data_size;
    }
    else if (header->type == RenderGroupEntryType_UploadGeometry)
    {
      RenderGroupEntry_UploadGeometry *entry =
          (RenderGroupEntry_UploadGeometry *)(output->render_group.base +
                                              offset);
      offset += sizeof(RenderGroupEntry_UploadGeometry) + entry->size;
    }
    else if (header->type == RenderGroupEntryType_DrawMesh)
    {
      RenderGroupEntry_DrawMesh *entry =
          (RenderGroupEntry_DrawMesh *)(output->render_group.base + offset);

      if (entry->shader_type != current_shader_type)
      {
        if (entry->shader_type == 1)
        {
          commandEncoder->setRenderPipelineState(global_grid_pipeline_state);
        }
        else
        {
          commandEncoder->setRenderPipelineState(global_pipeline_state);
        }
        current_shader_type = entry->shader_type;
      }

      // Bone matrices are stored inline in the render group right after the
      // DrawMesh struct.  Bump-allocate a slot in the current frame arena,
      // copy the data, and patch bone_matrix_offset before binding uniforms.
      if (entry->uniforms.has_bones)
      {
        Mat4 *src_bones = (Mat4 *)(entry + 1);
        U32 bone_offset = FrameArenaAlloc(MAX_BONES * sizeof(Mat4));
        memcpy((U8 *)global_gpu_heap->contents() + bone_offset, src_bones,
               MAX_BONES * sizeof(Mat4));
        entry->uniforms.bone_matrix_offset = bone_offset;
      }

      commandEncoder->setVertexBytes(&entry->uniforms, sizeof(Uniforms), 1);
      commandEncoder->setFragmentBytes(&entry->uniforms, sizeof(Uniforms), 1);

      commandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                     (NS::UInteger)0,
                                     (NS::UInteger)entry->vertex_count);

      // Advance past the fixed struct and any trailing inline bone data.
      offset += sizeof(RenderGroupEntry_DrawMesh);
      if (entry->uniforms.has_bones)
        offset += MAX_BONES * sizeof(Mat4);
    }
  }

  commandEncoder->endEncoding();
  commandBuffer->presentDrawable(drawable);
  commandBuffer->commit();

  // Advance the frame arena: next frame uses the next sub-arena.
  global_frame_index++;
  global_frame_arena_bump = 0;
}
