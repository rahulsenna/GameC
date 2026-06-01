#include "game.h"
#include "renderer.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <dlfcn.h>
#include <mach/mach_time.h>
#include <sys/stat.h>

@protocol MTLDevice;

static Arena *global_arena = nullptr;
static id<MTLDevice> global_device = nil;
static CAMetalLayer *global_metal_layer = nil;
static bool global_running = true;
static GameInput global_input = {};

// --- Game Code Loading ---
typedef void (*GameUpdateAndRenderFunc)(Arena *, GameInput *, float,
                                        GameOutput *);

struct GameCode
{
  void *dylib;
  time_t last_write_time;
  GameUpdateAndRenderFunc UpdateAndRender;
};

static GameCode global_game_code = {};

static time_t GetFileWriteTime(const char *filename)
{
  struct stat file_stat;
  if (stat(filename, &file_stat) == 0)
  {
    return file_stat.st_mtimespec.tv_sec;
  }
  return 0;
}

static void MacUnloadGameCode()
{
  if (global_game_code.dylib)
  {
    dlclose(global_game_code.dylib);
    global_game_code.dylib = nullptr;
  }
  global_game_code.UpdateAndRender = nullptr;
}

static void MacLoadGameCode()
{
  MacUnloadGameCode();
  const char *source_path = "build/game.dylib";
  const char *temp_path = "build/game_load.dylib";

  // Copy the dylib and its dSYM bundle so LLDB can find the debug symbols
  system("cp build/game.dylib build/game_load.dylib");
  system("rm -rf build/game_load.dylib.dSYM");
  system("cp -R build/game.dylib.dSYM build/game_load.dylib.dSYM >/dev/null "
         "2>&1 || true");

  global_game_code.dylib = dlopen(temp_path, RTLD_LAZY | RTLD_GLOBAL);
  if (global_game_code.dylib)
  {
    global_game_code.UpdateAndRender = (GameUpdateAndRenderFunc)dlsym(
        global_game_code.dylib, "GameUpdateAndRender");
    global_game_code.last_write_time = GetFileWriteTime(source_path);
    NSLog(@"Game code loaded successfully!");
  }
  else
  {
    NSLog(@"Failed to load game code: %s", dlerror());
  }
}

static time_t global_shader_write_time = 0;

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
  if (key == 35)
    global_input.key_p = 1;
  if (key == 49)
    global_input.key_space = 1;
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
  if (key == 35)
    global_input.key_p = 0;
  if (key == 49)
    global_input.key_space = 0;
}
- (void)flagsChanged:(NSEvent *)event
{
  if ([event modifierFlags] & NSEventModifierFlagShift)
  {
    global_input.key_shift = 1;
  }
  else
  {
    global_input.key_shift = 0;
  }

  if (([event modifierFlags] & NSEventModifierFlagControl) ||
      ([event modifierFlags] & NSEventModifierFlagOption))
  {
    global_input.key_ctrl = 1;
  }
  else
  {
    global_input.key_ctrl = 0;
  }
}
@end

int main(int argc, const char *argv[])
{
  @autoreleasepool
  {
    MacLoadGameCode();

    ArenaParams arena_params = {0};
    arena_params.reserve_size = GB(16);
    arena_params.commit_size = MB(32);
    global_arena = ArenaAlloc(&arena_params);
    global_device = (__bridge id<MTLDevice>)Renderer_CreateDevice();

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
    [window makeFirstResponder:view];
    [NSApp activateIgnoringOtherApps:YES];

    // Initialize the C++ Renderer
    Renderer_Init((__bridge void *)global_device,
                  (__bridge void *)global_metal_layer);
    global_shader_write_time = GetFileWriteTime("build/shaders.metallib");

    ArenaParams render_arena_params = {0};
    render_arena_params.reserve_size = GB(1);
    render_arena_params.commit_size = MB(32);
    static Arena *render_arena = ArenaAlloc(&render_arena_params);
    static U8 *render_buffer = PushArrayNoZero(render_arena, U8, GB(1));

    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    uint64_t previous_time = mach_absolute_time();

    while (global_running)
    {
      @autoreleasepool
      {
        uint64_t current_time = mach_absolute_time();
        float dt = (float)(current_time - previous_time) *
                   (float)timebase_info.numer / (float)timebase_info.denom /
                   1e9f;
        previous_time = current_time;
        if (dt > 0.1f)
          dt = 0.1f; // Clamp to avoid physics explosion on breakpoint/lag

        time_t new_game_time = GetFileWriteTime("build/game.dylib");
        if (new_game_time != 0 &&
            new_game_time != global_game_code.last_write_time)
        {
          MacLoadGameCode();
        }

        time_t new_shader_time = GetFileWriteTime("build/shaders.metallib");
        if (new_shader_time != 0 && new_shader_time != global_shader_write_time)
        {
          Renderer_LoadShaders();
          global_shader_write_time = new_shader_time;
        }

        NSEvent *event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]))
        {
          [NSApp sendEvent:event];
        }
        [NSApp updateWindows];

        GameOutput output = {};
        output.render_group.base = render_buffer;
        output.render_group.size = 0;
        output.render_group.max_size = 1024 * 1024 * 1024;

        if (global_game_code.UpdateAndRender)
        {
          global_game_code.UpdateAndRender(global_arena, &global_input, dt,
                                           &output);
        }

        Renderer_RenderFrame(&output);
      }
    }
  }
  return 0;
}
