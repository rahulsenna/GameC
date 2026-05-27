#include "game.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <dlfcn.h>
#include <sys/stat.h>

static Arena *global_arena = nullptr;
static id<MTLDevice> global_device = nil;
static id<MTLCommandQueue> global_command_queue = nil;
static id<MTLRenderPipelineState> global_pipeline_state = nil;
static id<MTLRenderPipelineState> global_grid_pipeline_state = nil;
static id<MTLDepthStencilState> global_depth_state = nil;
static id<MTLTexture> global_depth_texture = nil;
static CAMetalLayer *global_metal_layer = nil;
static NSMutableDictionary<NSNumber *, id<MTLTexture>> *global_textures = nil;
static bool global_running = true;
static GameInput global_input = {};

// --- Game Code Loading ---
typedef void (*GameUpdateAndRenderFunc)(Arena*, GameInput*, GameOutput*);

struct GameCode {
  void *dylib;
  time_t last_write_time;
  GameUpdateAndRenderFunc UpdateAndRender;
};

static GameCode global_game_code = {};

static time_t GetFileWriteTime(const char *filename) {
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0) {
        return file_stat.st_mtimespec.tv_sec;
    }
    return 0;
}

static void MacUnloadGameCode() {
    if (global_game_code.dylib) {
        dlclose(global_game_code.dylib);
        global_game_code.dylib = nullptr;
    }
    global_game_code.UpdateAndRender = nullptr;
}

static void MacLoadGameCode() {
    MacUnloadGameCode();
    
    // Copy the dylib to a new path to avoid file locks
    const char *source_path = "build/game.dylib";
    const char *temp_path = "build/game_load.dylib";
    
    // Using a system command for copying is simple and handmade style
    system("cp build/game.dylib build/game_load.dylib");
    
    global_game_code.dylib = dlopen(temp_path, RTLD_LAZY | RTLD_GLOBAL);
    if (global_game_code.dylib) {
        global_game_code.UpdateAndRender = (GameUpdateAndRenderFunc)dlsym(global_game_code.dylib, "GameUpdateAndRender");
        global_game_code.last_write_time = GetFileWriteTime(source_path);
        NSLog(@"Game code loaded successfully!");
    } else {
        NSLog(@"Failed to load game code: %s", dlerror());
    }
}

static time_t global_shader_write_time = 0;

static void MacLoadShaders() {
    NSString *libPath = [NSString
        stringWithFormat:@"%@/build/shaders.metallib",
                         [[NSFileManager defaultManager] currentDirectoryPath]];
    NSError *error = nil;
    NSData *data = [NSData dataWithContentsOfFile:libPath];
    if (!data) {
        NSLog(@"Failed to read metallib file at %@", libPath);
        return;
    }
    dispatch_data_t dispatchData = dispatch_data_create(data.bytes, data.length, dispatch_get_main_queue(), ^{});
    id<MTLLibrary> defaultLibrary = [global_device newLibraryWithData:dispatchData error:&error];
    if (!defaultLibrary)
    {
      NSLog(@"Failed to load metallib: %@", error);
      return;
    }

    id<MTLFunction> vertexFunction =
        [defaultLibrary newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction =
        [defaultLibrary newFunctionWithName:@"fragment_main"];

    MTLRenderPipelineDescriptor *pipelineStateDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    pipelineStateDescriptor.label = @"Simple Pipeline";
    pipelineStateDescriptor.vertexFunction = vertexFunction;
    pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    pipelineStateDescriptor.colorAttachments[0].pixelFormat =
        MTLPixelFormatBGRA8Unorm;
    pipelineStateDescriptor.depthAttachmentPixelFormat =
        MTLPixelFormatDepth32Float;

    global_pipeline_state = [global_device
        newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                       error:&error];
    if (!global_pipeline_state)
    {
      NSLog(@"Failed to create pipeline state: %@", error);
    }
    
    // Grid Pipeline
    id<MTLFunction> gridFragmentFunction =
        [defaultLibrary newFunctionWithName:@"grid_fragment_main"];
    pipelineStateDescriptor.label = @"Grid Pipeline";
    pipelineStateDescriptor.fragmentFunction = gridFragmentFunction;
    
    // Enable Alpha Blending for the Grid
    pipelineStateDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineStateDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineStateDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineStateDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineStateDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    global_grid_pipeline_state = [global_device
        newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                       error:&error];
    if (!global_grid_pipeline_state)
    {
      NSLog(@"Failed to create grid pipeline state: %@", error);
    }

    if (global_pipeline_state && global_grid_pipeline_state) {
      NSLog(@"Shaders loaded successfully!");
      global_shader_write_time = GetFileWriteTime("build/shaders.metallib");
    }
}

// Window Delegate
@interface MainWindowDelegate : NSObject <NSWindowDelegate>
@end
@implementation MainWindowDelegate
- (void)windowWillClose:(NSNotification *)notification
{
  global_running = false;
  [NSApp terminate:nil];
}
@end

// View that hosts the Metal Layer
@interface MainView : NSView
@end
@implementation MainView
- (CALayer *)makeBackingLayer
{
  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = global_device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  global_metal_layer = layer;
  return layer;
}
- (BOOL)acceptsFirstResponder
{
  return YES;
}
- (void)keyDown:(NSEvent *)event
{
  unsigned short key = [event keyCode];
  if (key == 13)
    global_input.key_w = 1;
  if (key == 0)
    global_input.key_a = 1;
  if (key == 1)
    global_input.key_s = 1;
  if (key == 2)
    global_input.key_d = 1;
  if (key == 126)
    global_input.key_up = 1;
  if (key == 125)
    global_input.key_down = 1;
  if (key == 123)
    global_input.key_left = 1;
  if (key == 124)
    global_input.key_right = 1;
}
- (void)keyUp:(NSEvent *)event
{
  unsigned short key = [event keyCode];
  if (key == 13)
    global_input.key_w = 0;
  if (key == 0)
    global_input.key_a = 0;
  if (key == 1)
    global_input.key_s = 0;
  if (key == 2)
    global_input.key_d = 0;
  if (key == 126)
    global_input.key_up = 0;
  if (key == 125)
    global_input.key_down = 0;
  if (key == 123)
    global_input.key_left = 0;
  if (key == 124)
    global_input.key_right = 0;
}
@end

static void UpdateDepthTexture(CGSize size)
{
  if (global_depth_texture && global_depth_texture.width == size.width &&
      global_depth_texture.height == size.height)
  {
    return;
  }

  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                   width:size.width
                                  height:size.height
                               mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget;
  desc.storageMode = MTLStorageModePrivate;
  global_depth_texture = [global_device newTextureWithDescriptor:desc];
}

static void RenderFrame()
{
  @autoreleasepool
  {
    id<CAMetalDrawable> drawable = [global_metal_layer nextDrawable];
    if (!drawable)
      return;

    UpdateDepthTexture(
        CGSizeMake(drawable.texture.width, drawable.texture.height));

    static U8 *render_buffer = (U8 *)malloc(1024 * 1024 * 256);
    GameOutput output = {};
    output.render_group.base = render_buffer;
    output.render_group.size = 0;
    output.render_group.max_size = 1024 * 1024 * 256;

    // Run game logic with input
    if (global_game_code.UpdateAndRender) {
      global_game_code.UpdateAndRender(global_arena, &global_input, &output);
    }

    // --- Parse Render Group ---
    MTLClearColor clear_color = MTLClearColorMake(0, 0, 0, 1);
    U32 offset = 0;

    // First pass: Pre-Render (Uploads and State)
    while (offset < output.render_group.size)
    {
      RenderGroupEntryHeader *header =
          (RenderGroupEntryHeader *)(output.render_group.base + offset);
      offset += sizeof(RenderGroupEntryHeader);

      if (header->type == RenderGroupEntryType_Clear)
      {
        RenderGroupEntry_Clear *entry =
            (RenderGroupEntry_Clear *)(output.render_group.base + offset);
        clear_color = MTLClearColorMake(entry->color[0], entry->color[1],
                                        entry->color[2], entry->color[3]);
        offset += sizeof(RenderGroupEntry_Clear);
      }
      else if (header->type == RenderGroupEntryType_UploadTexture)
      {
        RenderGroupEntry_UploadTexture *entry =
            (RenderGroupEntry_UploadTexture *)(output.render_group.base +
                                               offset);
        void *pixels = (void *)(entry + 1);

        MTLTextureDescriptor *textureDesc = [[MTLTextureDescriptor alloc] init];
        textureDesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
        textureDesc.width = entry->width;
        textureDesc.height = entry->height;

        id<MTLTexture> texture =
            [global_device newTextureWithDescriptor:textureDesc];
        MTLRegion region = MTLRegionMake2D(0, 0, entry->width, entry->height);
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:pixels
                   bytesPerRow:4 * entry->width];

        global_textures[@(entry->handle)] = texture;

        offset += sizeof(RenderGroupEntry_UploadTexture) +
                  (entry->width * entry->height * 4);
      }
      else if (header->type == RenderGroupEntryType_DrawMesh)
      {
        RenderGroupEntry_DrawMesh *entry =
            (RenderGroupEntry_DrawMesh *)(output.render_group.base + offset);
        offset += sizeof(RenderGroupEntry_DrawMesh) +
                  sizeof(Vertex) * entry->vertex_count;
      }
    }

    // Setup Render Pass
    MTLRenderPassDescriptor *passDescriptor =
        [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = drawable.texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor = clear_color;
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    passDescriptor.depthAttachment.texture = global_depth_texture;
    passDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
    passDescriptor.depthAttachment.clearDepth = 1.0;
    passDescriptor.depthAttachment.storeAction = MTLStoreActionDontCare;

    id<MTLCommandBuffer> commandBuffer = [global_command_queue commandBuffer];
    id<MTLRenderCommandEncoder> commandEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

    [commandEncoder setDepthStencilState:global_depth_state];

    // Second pass: Draw commands
    offset = 0;
    while (offset < output.render_group.size)
    {
      RenderGroupEntryHeader *header =
          (RenderGroupEntryHeader *)(output.render_group.base + offset);
      offset += sizeof(RenderGroupEntryHeader);

      if (header->type == RenderGroupEntryType_Clear)
      {
        offset += sizeof(RenderGroupEntry_Clear);
      }
      else if (header->type == RenderGroupEntryType_UploadTexture)
      {
        RenderGroupEntry_UploadTexture *entry =
            (RenderGroupEntry_UploadTexture *)(output.render_group.base +
                                               offset);
        offset += sizeof(RenderGroupEntry_UploadTexture) +
                  (entry->width * entry->height * 4);
      }
      else if (header->type == RenderGroupEntryType_DrawMesh)
      {
        RenderGroupEntry_DrawMesh *entry =
            (RenderGroupEntry_DrawMesh *)(output.render_group.base + offset);
        Vertex *vertices = (Vertex *)(entry + 1);

        if (entry->shader_type == 1) {
            [commandEncoder setRenderPipelineState:global_grid_pipeline_state];
        } else {
            [commandEncoder setRenderPipelineState:global_pipeline_state];
        }

        id<MTLTexture> texture = global_textures[@(entry->texture_handle)];
        if (texture && entry->shader_type == 0)
        {
          [commandEncoder setFragmentTexture:texture atIndex:0];
        }

        [commandEncoder setVertexBytes:&entry->uniforms
                                length:sizeof(Uniforms)
                               atIndex:1];
        [commandEncoder setFragmentBytes:&entry->uniforms
                                  length:sizeof(Uniforms)
                                 atIndex:1];
        id<MTLBuffer> vertexBuffer = [global_device newBufferWithBytes:vertices
                                                                length:sizeof(Vertex) * entry->vertex_count
                                                               options:MTLResourceStorageModeShared];
        [commandEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
        [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                           vertexStart:0
                           vertexCount:entry->vertex_count];

        offset += sizeof(RenderGroupEntry_DrawMesh) +
                  sizeof(Vertex) * entry->vertex_count;
      }
    }

    [commandEncoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

int main(int argc, const char *argv[])
{
  @autoreleasepool
  {
    MacLoadGameCode();

    global_arena = ArenaAlloc(1ull << 30, 1ull << 25);
    global_device = MTLCreateSystemDefaultDevice();
    global_command_queue = [global_device newCommandQueue];
    global_textures = [[NSMutableDictionary alloc] init];

    // Create Depth Stencil State
    MTLDepthStencilDescriptor *depthDesc =
        [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionLess;
    depthDesc.depthWriteEnabled = YES;
    global_depth_state =
        [global_device newDepthStencilStateWithDescriptor:depthDesc];

    // Load Shaders
    MacLoadShaders();
    if (!global_pipeline_state) {
        return 1;
    }

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect frame = NSMakeRect(0, 0, 800, 600);
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:(NSWindowStyleMaskTitled |
                                               NSWindowStyleMaskClosable |
                                               NSWindowStyleMaskResizable |
                                               NSWindowStyleMaskMiniaturizable)
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [window cascadeTopLeftFromPoint:NSMakePoint(20, 20)];
    window.title = @"Metal Engine (Handmade Style)";

    MainWindowDelegate *windowDelegate = [[MainWindowDelegate alloc] init];
    window.delegate = windowDelegate;

    MainView *view = [[MainView alloc] initWithFrame:frame];
    view.wantsLayer = YES;
    window.contentView = view;

    [window makeKeyAndOrderFront:nil];
    // Required to capture keyboard events immediately
    [window makeFirstResponder:view];
    [NSApp activateIgnoringOtherApps:YES];

    while (global_running)
    {
      // --- Hot Reload Checks ---
      time_t new_game_time = GetFileWriteTime("build/game.dylib");
      if (new_game_time != 0 && new_game_time != global_game_code.last_write_time) {
          MacLoadGameCode();
      }

      time_t new_shader_time = GetFileWriteTime("build/shaders.metallib");
      if (new_shader_time != 0 && new_shader_time != global_shader_write_time) {
          MacLoadShaders();
      }

      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:[NSDate distantPast]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event)
      {
        [NSApp sendEvent:event];
      }
      RenderFrame();
    }
  }
  return 0;
}
