// cocoa_backend.mm —— 原生 Cocoa 后端实现（Objective-C++，AppKit 只在本文件出现）
//
// Take 2 外部视图模式：NSView 归宿主；本后端只在宿主 drawRect: 时把 BitmapCanvas
// 以 CGImage 画进当前 CGContext，并把宿主剥壳后的鼠标/键盘事件排队。
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui_cocoa/cocoa_backend.hpp"

namespace termchart_gui_cocoa {

struct CocoaBackend::Impl {
    void* view = nullptr;  // NSView*（__bridge，不持有）
    std::uint32_t w = 1;
    std::uint32_t h = 1;
    bool external = true;
    termchart_gui::BitmapCanvas canvas;
    std::deque<termchart_gui::Event> queue;
    std::vector<std::uint8_t> bgra;  // 复用的上屏缓冲

    Impl(void* v, std::uint32_t width, std::uint32_t height)
        : view(v), w(width > 0 ? width : 1), h(height > 0 ? height : 1),
          canvas(width > 0 ? width : 1, height > 0 ? height : 1) {}

    void paint() {
        if (view == nullptr) {
            return;
        }
        NSView* nsview = (__bridge NSView*)view;
        [nsview setNeedsDisplay:YES];
    }

    void draw() {
        if (view == nullptr) {
            return;
        }
        NSGraphicsContext* gc = [NSGraphicsContext currentContext];
        if (gc == nullptr) {
            return;
        }
        CGContextRef ctx = (CGContextRef)[gc CGContext];
        if (ctx == nullptr) {
            return;
        }
        const termchart_gui::Bitmap& b = canvas.pixels();
        const std::uint32_t bw = b.width();
        const std::uint32_t bh = b.height();
        if (bw == 0 || bh == 0) {
            return;
        }
        // CGImage 需要新的缓冲（Bitmap 是 RGBA，CG 用 kCGImageAlphaNoneSkipLast 直读）。
        const std::size_t n = static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh) * 4;
        if (bgra.size() != n) {
            bgra.resize(n);
        }
        const std::uint8_t* src = b.data();
        for (std::size_t i = 0; i < n; i += 4) {
            bgra[i + 0] = src[i + 0];
            bgra[i + 1] = src[i + 1];
            bgra[i + 2] = src[i + 2];
            bgra[i + 3] = 255;
        }
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        if (cs == nullptr) {
            return;
        }
        CGDataProviderRef provider =
            CGDataProviderCreateWithData(nullptr, bgra.data(), n, nullptr);
        CGImageRef image = CGImageCreate(
            static_cast<size_t>(bw), static_cast<size_t>(bh), 8, 32,
            static_cast<size_t>(bw) * 4, cs,
            kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast, provider, nullptr, false,
            kCGRenderingIntentDefault);
        if (image != nullptr) {
            // 位图自上而下；NSView 默认左下原点（未 flipped）时需翻转 Y 才能正立。
            // 宿主若 override isFlipped 返回 YES，CTM 已是 y 向下，不能再翻一次。
            NSView* nsview = (__bridge NSView*)view;
            CGContextSaveGState(ctx);
            if (![nsview isFlipped]) {
                CGContextTranslateCTM(ctx, 0, static_cast<CGFloat>(bh));
                CGContextScaleCTM(ctx, 1, -1);
            }
            CGContextDrawImage(ctx, CGRectMake(0, 0, static_cast<CGFloat>(bw),
                                               static_cast<CGFloat>(bh)),
                               image);
            CGContextRestoreGState(ctx);
            CGImageRelease(image);
        }
        if (provider != nullptr) {
            CGDataProviderRelease(provider);
        }
        CGColorSpaceRelease(cs);
    }
};

CocoaBackend::CocoaBackend(const termchart_gui::NativeSurface& host) {
    impl_ = std::make_unique<Impl>(host.surface, host.width, host.height);
}

CocoaBackend::~CocoaBackend() = default;

bool CocoaBackend::valid() const noexcept { return impl_ != nullptr && impl_->view != nullptr; }

bool CocoaBackend::is_external() const noexcept { return impl_ != nullptr && impl_->external; }

void CocoaBackend::resize(std::uint32_t width, std::uint32_t height) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->w = width > 0 ? width : 1;
    impl_->h = height > 0 ? height : 1;
    impl_->canvas.resize(impl_->w, impl_->h);
    impl_->queue.push_back(termchart_gui::tk_event_map::resize(impl_->w, impl_->h));
}

void CocoaBackend::draw_in_current_context() {
    if (impl_ != nullptr) {
        impl_->draw();
    }
}

std::unique_ptr<termchart_gui::TkWindow> CocoaBackend::create_window(std::uint32_t,
                                                                     std::uint32_t, std::string) {
    return nullptr;  // 外部模式：NSWindow 归宿主
}

termchart_gui::TkCanvas& CocoaBackend::canvas() { return impl_->canvas; }

bool CocoaBackend::poll_event(termchart_gui::Event& out) {
    if (impl_ == nullptr || impl_->queue.empty()) {
        return false;
    }
    out = impl_->queue.front();
    impl_->queue.pop_front();
    return true;
}

void CocoaBackend::present() {
    if (impl_ != nullptr) {
        impl_->paint();
    }
}

void CocoaBackend::run_loop(termchart_gui::EventLoop& loop) { loop.run(); }

void CocoaBackend::inject_event(termchart_gui::Event e) {
    if (impl_ != nullptr) {
        impl_->queue.push_back(std::move(e));
    }
}

}  // namespace termchart_gui_cocoa
