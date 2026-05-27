#include "game.h"
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

static Arena *global_arena = nullptr;
static id<MTLDevice> global_device = nil;
static id<MTLCommandQueue> global_command_queue = nil;
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

    // Ask the cross-platform game layer for what to draw
    GameOutput output = {};
    GameUpdateAndRender(global_arena, &output);

    // Setup the render pass to clear the screen
    MTLRenderPassDescriptor *passDescriptor =
        [MTLRenderPassDescriptor renderPassDescriptor];
    passDescriptor.colorAttachments[0].texture = drawable.texture;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor =
        MTLClearColorMake(output.clear_color[0], output.clear_color[1],
                          output.clear_color[2], output.clear_color[3]);
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLCommandBuffer> commandBuffer = [global_command_queue commandBuffer];
    id<MTLRenderCommandEncoder> commandEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    [commandEncoder
        endEncoding]; // For Phase 1 we only clear the screen, no draw calls

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

int main(int argc, const char *argv[])
{
  @autoreleasepool
  {
    // 1. Initialize our Memory Arena
    global_arena = ArenaAlloc();

    // 2. Initialize Metal
    global_device = MTLCreateSystemDefaultDevice();
    global_command_queue = [global_device newCommandQueue];

    // 3. Initialize Cocoa Application programmatically
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect frame = NSMakeRect(0, 0, 1024, 768);
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
    view.wantsLayer = YES; // Triggers makeBackingLayer
    window.contentView = view;

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // 4. Custom Event Loop
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

      // nextDrawable will block when the GPU queue is full (usually 3 frames),
      // giving us a natural vsync-like pacing without eating 100% CPU.
      RenderFrame();
    }
  }
  return 0;
}
