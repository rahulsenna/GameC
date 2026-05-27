#include "game.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

static Arena *global_arena = nullptr;
static id<MTLDevice> global_device = nil;
static id<MTLCommandQueue> global_command_queue = nil;
static id<MTLRenderPipelineState> global_pipeline_state = nil;
static CAMetalLayer *global_metal_layer = nil;
static bool global_running = true;

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
@end

static void RenderFrame()
{
  @autoreleasepool
  {
    id<CAMetalDrawable> drawable = [global_metal_layer nextDrawable];
    if (!drawable)
      return;

    // We will allocate a temporary buffer for the render group
    // In a real engine this would be a specialized temporary arena that resets
    // every frame.
    U8 render_buffer[1024 * 64];
    GameOutput output = {};
    output.render_group.base = render_buffer;
    output.render_group.size = 0;
    output.render_group.max_size = sizeof(render_buffer);

    // Run game logic
    GameUpdateAndRender(global_arena, &output);

    // --- Parse Render Group ---

    // Default clear color in case no clear command was pushed
    MTLClearColor clear_color = MTLClearColorMake(0, 0, 0, 1);

    U32 offset = 0;

    // First pass: Find clear command (Metal needs it for the render pass
    // descriptor)
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
      else if (header->type == RenderGroupEntryType_DrawTriangle)
      {
        offset += sizeof(RenderGroupEntry_DrawTriangle);
      }
    }

    // Setup Render Pass
    MTLRenderPassDescriptor *passDescriptor =
        [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = drawable.texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor = clear_color;
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLCommandBuffer> commandBuffer = [global_command_queue commandBuffer];
    id<MTLRenderCommandEncoder> commandEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];

    [commandEncoder setRenderPipelineState:global_pipeline_state];

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
      else if (header->type == RenderGroupEntryType_DrawTriangle)
      {
        RenderGroupEntry_DrawTriangle *entry =
            (RenderGroupEntry_DrawTriangle *)(output.render_group.base +
                                              offset);

        [commandEncoder setVertexBytes:entry->vertices
                                length:sizeof(entry->vertices)
                               atIndex:0];
        [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                           vertexStart:0
                           vertexCount:3];

        offset += sizeof(RenderGroupEntry_DrawTriangle);
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
    global_arena = ArenaAlloc();
    global_device = MTLCreateSystemDefaultDevice();
    global_command_queue = [global_device newCommandQueue];

    // Load Shaders
    NSString *libPath = [NSString
        stringWithFormat:@"%@/build/shaders.metallib",
                         [[NSFileManager defaultManager] currentDirectoryPath]];
    NSError *error = nil;
    id<MTLLibrary> defaultLibrary =
        [global_device newLibraryWithURL:[NSURL fileURLWithPath:libPath]
                                   error:&error];
    if (!defaultLibrary)
    {
      NSLog(@"Failed to load metallib: %@", error);
      return 1;
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

    global_pipeline_state = [global_device
        newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
                                       error:&error];
    if (!global_pipeline_state)
    {
      NSLog(@"Failed to create pipeline state: %@", error);
      return 1;
    }

    // App Initialization
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect frame = NSMakeRect(0, 0, 800, 800);
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
    [NSApp activateIgnoringOtherApps:YES];

    while (global_running)
    {
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
