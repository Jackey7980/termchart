// cocoa_demo.mm —— Take 2 句柄注入（Cocoa）：用户拥有 NSWindow/NSView
//
// 宿主自建 NSWindow + 自定义 NSView；drawRect: 里调 backend->draw_in_current_context()
// 把画布画进 CGContext，鼠标事件剥壳后 inject_event。等价于 win32 的 embed_demo。
//
// 跑法：cocoa_demo            → 开窗口交互
//       cocoa_demo --smoke    → 开窗口跑短时后退出
//       cocoa_demo --offscreen→ 不开窗，仅验证渲染管线（CI 无 WindowServer 用）
#include <AppKit/AppKit.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_cocoa/cocoa_backend.hpp"

using namespace termchart_gui;

@interface TCHostView : NSView
@property(nonatomic, assign) termchart_gui_cocoa::CocoaBackend* backend;
@end

@implementation TCHostView
- (BOOL)isFlipped {
    return YES;  // 让坐标自上而下，和 Bitmap / 事件坐标一致
}
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (self.backend != nullptr) {
        self.backend->draw_in_current_context();
    }
}
- (void)mouseDown:(NSEvent*)event {
    if (self.backend == nullptr) {
        return;
    }
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    self.backend->inject_event(
        termchart_gui::tk_event_map::mouse_press(p.x, p.y, 0));
}
- (void)mouseUp:(NSEvent*)event {
    if (self.backend == nullptr) {
        return;
    }
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    self.backend->inject_event(
        termchart_gui::tk_event_map::mouse_release(p.x, p.y, 0));
}
- (void)mouseDragged:(NSEvent*)event {
    if (self.backend == nullptr) {
        return;
    }
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    self.backend->inject_event(termchart_gui::tk_event_map::mouse_move(p.x, p.y));
}
- (void)keyDown:(NSEvent*)event {
    if (self.backend == nullptr) {
        return;
    }
    const unsigned short kc = [event keyCode];
    if (kc == 53) {  // ESC
        self.backend->inject_event(termchart_gui::tk_event_map::quit());
        return;
    }
    NSString* s = [event characters];
    self.backend->inject_event(termchart_gui::tk_event_map::key_press(
        static_cast<int>(kc), s != nil ? [s UTF8String] : ""));
}
@end

namespace {

bool surface_has_content(const Bitmap& b) {
    const std::uint8_t* p = b.data();
    const std::size_t n = static_cast<std::size_t>(b.width()) * b.height() * 4;
    for (std::size_t i = 0; i + 3 < n; i += 4) {
        if (p[i] != 0 || p[i + 1] != 0 || p[i + 2] != 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");
    const bool offscreen = (argc > 1 && std::string(argv[1]) == "--offscreen");
    const std::uint32_t w = 640;
    const std::uint32_t h = 420;
    int rc = 0;

    @autoreleasepool {
        NSWindow* window = nil;
        TCHostView* view = nil;
        if (!offscreen) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            NSRect frame = NSMakeRect(0, 0, w, h);
            window = [[NSWindow alloc]
                initWithContentRect:frame
                          styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                     NSWindowStyleMaskResizable)
                            backing:NSBackingStoreBuffered
                              defer:NO];
            [window setTitle:@"User-owned host (Take 2 Cocoa)"];
            view = [[TCHostView alloc] initWithFrame:frame];
            [window setContentView:view];
            [window makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];
        }

        NativeSurface surface{};
        surface.surface = (__bridge void*)view;
        surface.width = w;
        surface.height = h;

        {
            termchart_gui_cocoa::CocoaBackend backend(surface);
            if (!offscreen && !backend.valid()) {
                std::fprintf(stderr, "cocoa_demo: injected backend invalid\n");
                rc = 3;
            } else {
                if (view != nil) {
                    view.backend = &backend;
                }
                Container root("root");
                root.set_bounds(Rect{0, 0, static_cast<double>(w), static_cast<double>(h)});
                root.emplace_child<Label>("title", "Take 2 - Cocoa user-owned view",
                                          Rgb{240, 240, 245}, Rgb{28, 32, 40});
                auto& slider = root.emplace_child<Slider>("slider");
                auto& bar = root.emplace_child<ProgressBar>("bar");
                slider.set_on_change([&](double v) { bar.set_value(v); });

                Mount mount(root, backend);
                mount.paint_all();
                if (!surface_has_content(backend.canvas().snapshot())) {
                    std::fprintf(stderr, "cocoa_demo: canvas stayed empty\n");
                    rc = 4;
                }

                if (!offscreen) {
                    if (smoke) {
                        NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:0.5];
                        while ([deadline timeIntervalSinceNow] > 0) {
                            @autoreleasepool {
                                NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                                 untilDate:deadline
                                                                    inMode:NSDefaultRunLoopMode
                                                                   dequeue:YES];
                                if (ev != nil) {
                                    [NSApp sendEvent:ev];
                                }
                                [view setNeedsDisplay:YES];
                                [view displayIfNeeded];
                            }
                        }
                    } else {
                        [NSApp run];
                    }
                }
                if (view != nil) {
                    view.backend = nullptr;  // 后端先于窗口析构，断开悬挂指针
                }
            }
        }
        if (window != nil) {
            [window close];
        }
    }
    std::fprintf(stderr, "cocoa_demo: %s\n",
                 offscreen ? "offscreen render verified" : "closed by host");
    return rc;
}
