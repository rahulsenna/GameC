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
      textureDesc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      textureDesc->setWidth(entry->width);
      textureDesc->setHeight(entry->height);

      NS::UInteger mipLevels =
          std::floor(std::log2(std::max(entry->width, entry->height))) + 1;
      textureDesc->setMipmapLevelCount(mipLevels);

      MTL::Texture *texture = global_device->newTexture(textureDesc);
      MTL::Region region =
          MTL::Region::Make2D(0, 0, entry->width, entry->height);
      texture->replaceRegion(region, 0, pixels, 4 * entry->width);

      MTL::CommandBuffer *blitCommandBuffer =
          global_command_queue->commandBuffer();
      MTL::BlitCommandEncoder *blitEncoder =
          blitCommandBuffer->blitCommandEncoder();
      blitEncoder->generateMipmaps(texture);
      blitEncoder->endEncoding();
      blitCommandBuffer->commit();

      if (global_textures.count(entry->handle))
      {
        global_textures[entry->handle]->release();
      }
      global_textures[entry->handle] = texture;

      textureDesc->release();

      offset += sizeof(RenderGroupEntry_UploadTexture) +
                (entry->width * entry->height * 4);
    }
    else if (header->type == RenderGroupEntryType_DrawMesh)
    {
      RenderGroupEntry_DrawMesh *entry =
          (RenderGroupEntry_DrawMesh *)(output->render_group.base + offset);
      offset += sizeof(RenderGroupEntry_DrawMesh) +
                sizeof(Vertex) * entry->vertex_count;
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
      offset += sizeof(RenderGroupEntry_UploadTexture) +
                (entry->width * entry->height * 4);
    }
    else if (header->type == RenderGroupEntryType_DrawMesh)
    {
      RenderGroupEntry_DrawMesh *entry =
          (RenderGroupEntry_DrawMesh *)(output->render_group.base + offset);
      Vertex *vertices = (Vertex *)(entry + 1);

      if (entry->shader_type == 1)
      {
        commandEncoder->setRenderPipelineState(global_grid_pipeline_state);
      }
      else
      {
        commandEncoder->setRenderPipelineState(global_pipeline_state);
      }

      if (entry->shader_type == 0)
      {
        MTL::Texture *albedo = global_textures.count(entry->textures.albedo)
                                   ? global_textures[entry->textures.albedo]
                                   : nullptr;
        MTL::Texture *normal = global_textures.count(entry->textures.normal)
                                   ? global_textures[entry->textures.normal]
                                   : nullptr;
        MTL::Texture *metallic = global_textures.count(entry->textures.metallic)
                                     ? global_textures[entry->textures.metallic]
                                     : nullptr;
        MTL::Texture *roughness =
            global_textures.count(entry->textures.roughness)
                ? global_textures[entry->textures.roughness]
                : nullptr;
        MTL::Texture *ao = global_textures.count(entry->textures.ao)
                               ? global_textures[entry->textures.ao]
                               : nullptr;

        if (albedo)
          commandEncoder->setFragmentTexture(albedo, 0);
        if (normal)
          commandEncoder->setFragmentTexture(normal, 1);
        if (metallic)
          commandEncoder->setFragmentTexture(metallic, 2);
        if (roughness)
          commandEncoder->setFragmentTexture(roughness, 3);
        if (ao)
          commandEncoder->setFragmentTexture(ao, 4);
      }

      commandEncoder->setVertexBytes(&entry->uniforms, sizeof(Uniforms), 1);
      commandEncoder->setFragmentBytes(&entry->uniforms, sizeof(Uniforms), 1);

      MTL::Buffer *vertexBuffer = global_device->newBuffer(
          vertices, sizeof(Vertex) * entry->vertex_count,
          MTL::ResourceStorageModeShared);
      commandEncoder->setVertexBuffer(vertexBuffer, 0, 0);

      commandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                                     (NS::UInteger)0,
                                     (NS::UInteger)entry->vertex_count);

      vertexBuffer->release();

      offset += sizeof(RenderGroupEntry_DrawMesh) +
                sizeof(Vertex) * entry->vertex_count;
    }
  }

  commandEncoder->endEncoding();
  commandBuffer->presentDrawable(drawable);
  commandBuffer->commit();
}
