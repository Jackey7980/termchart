// x11_backend.cpp —— 原生 X11 后端实现（平台胶水，全部关在本文件）
//
// Take 2 外部句柄模式：本后端**不**创建窗口，只把画布贴进宿主 Window 并翻译 X 事件。
// 上屏用 XPutImage（逐像素 XPutPixel 保证 16/24/32 位真彩的字节序都正确）；事件用
// XPending/XNextEvent 非阻塞抽取，宿主自己驱动消息泵。
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// 先包含 termchart 头，再包含 Xlib：X.h 把 None / TrueColor / Status / Bool … 定义成宏，
// 会把 termchart 的 Attr::None、ColorDepth::TrueColor、core::Status 等标识符打碎。
// 顺序反过来的话必须在 Xlib 之前清完一整套宏，脆弱且随 Xlib 版本变化。
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui_x11/x11_backend.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// 之后本 TU 还会用到 termchart 的 inline 代码（tk_event_map 等），把已知冲突宏清掉。
#undef None
#undef Bool
#undef Status
#undef Success
#undef TrueColor
#undef DirectColor
#undef PseudoColor
#undef StaticColor
#undef GrayScale
#undef StaticGray
#undef Always
#undef Above
#undef Below
#undef Complex
#undef Convex
#undef CopyFromParent
#undef ParentRelative

namespace termchart_gui_x11 {
namespace {

constexpr int kVkEscape = 0x1B;

}  // namespace

struct X11Backend::Impl {
    Display* dpy = nullptr;
    Window win = 0;
    GC gc = nullptr;
    XImage* img = nullptr;
    int w = 0;
    int h = 0;
    bool external = true;  // 本后端只支持句柄注入
    bool running = false;
    Atom wm_delete = 0;
    int depth_ = 24;              // 屏幕位深（15/16/24/32）
    const Visual* vis_ = nullptr;  // 屏幕默认 Visual
    termchart_gui::BitmapCanvas canvas;
    std::deque<termchart_gui::Event> queue;

    Impl(Display* d, Window window, std::uint32_t width, std::uint32_t height)
        : dpy(d), win(window), w(width > 0 ? static_cast<int>(width) : 1),
          h(height > 0 ? static_cast<int>(height) : 1),
          canvas(width > 0 ? width : 1, height > 0 ? height : 1) {
        if (dpy != nullptr && win != 0) {
            gc = XCreateGC(dpy, win, 0, nullptr);
            wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
            running = true;
        }
    }

    ~Impl() {
        destroy_image();
        if (dpy != nullptr && gc != nullptr) {
            XFreeGC(dpy, gc);
            gc = nullptr;
        }
        // 不销毁 win（用户拥有）；只断开本地引用。
        dpy = nullptr;
        win = 0;
    }

    void destroy_image() {
        if (img != nullptr) {
            XDestroyImage(img);  // 同时释放 XCreateImage 分配的 data
            img = nullptr;
        }
    }

    void resize_to(std::uint32_t nw, std::uint32_t nh) {
        w = nw > 0 ? static_cast<int>(nw) : 1;
        h = nh > 0 ? static_cast<int>(nh) : 1;
        canvas.resize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
        destroy_image();  // 尺寸变了，XImage 必须重建
    }

    bool ensure_image() {
        if (dpy == nullptr) {
            return false;
        }
        if (img != nullptr && img->width == w && img->height == h) {
            return true;
        }
        destroy_image();
        const int screen = DefaultScreen(dpy);
        const int depth = DefaultDepth(dpy, screen);
        // 只支持 15/16/24/32 位真彩：像素打包方式按位深决定。
        if (depth != 15 && depth != 16 && depth != 24 && depth != 32) {
            return false;  // 8 位 PseudoColor 等需要查色表，这里明确不支持
        }
        depth_ = depth;
        vis_ = DefaultVisual(dpy, screen);
        const int bpp = (depth > 16) ? 32 : 16;
        // XCreateImage 的 data 必须自己给：实测这版 libX11 在 data=nullptr 时不分配
        // 缓冲（返回 data=nil），XPutPixel 会直接踩空指针。XDestroyImage 负责 Xfree。
        const int bpl = ((w * bpp + 31) / 32) * 4;
        // XDestroyImage 用 Xfree（即 free）释放 data，所以这里用 malloc/free 配对。
        char* data = static_cast<char*>(std::malloc(static_cast<std::size_t>(bpl) *
                                                   static_cast<std::size_t>(h)));
        if (data == nullptr) {
            return false;
        }
        // DefaultVisual 返回 const Visual*，而 XCreateImage 只收非 const（Xlib 历史签名）。
        img = XCreateImage(dpy, const_cast<Visual*>(vis_), depth, ZPixmap, 0, data,
                           static_cast<unsigned int>(w), static_cast<unsigned int>(h), 32,
                           bpl);
        if (img == nullptr) {
            std::free(data);
            return false;
        }
        return true;
    }

    // RGBA → 屏幕原生像素值（按位深压缩）。
    unsigned long pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) const {
        if (depth_ >= 24) {
            return (static_cast<unsigned long>(r) << 16) |
                   (static_cast<unsigned long>(g) << 8) | static_cast<unsigned long>(b);
        }
        if (depth_ == 16) {
            return (static_cast<unsigned long>(r >> 3) << 11) |
                   (static_cast<unsigned long>(g >> 2) << 5) |
                   static_cast<unsigned long>(b >> 3);
        }
        // depth == 15
        return (static_cast<unsigned long>(r >> 3) << 10) |
               (static_cast<unsigned long>(g >> 3) << 5) |
               static_cast<unsigned long>(b >> 3);
    }

    void paint() {
        if (dpy == nullptr || win == 0 || gc == nullptr || !ensure_image()) {
            return;
        }
        const termchart_gui::Bitmap& b = canvas.pixels();
        const int bw = static_cast<int>(b.width());
        const int bh = static_cast<int>(b.height());
        for (int y = 0; y < std::min(h, bh); ++y) {
            for (int x = 0; x < std::min(w, bw); ++x) {
                const std::uint8_t* p =
                    b.data() + (static_cast<std::size_t>(y) *
                                    static_cast<std::size_t>(bw) +
                                static_cast<std::size_t>(x)) *
                                   4;
                XPutPixel(img, x, y, pack(p[0], p[1], p[2]));
            }
        }
        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, static_cast<unsigned int>(w),
                  static_cast<unsigned int>(h));
        XFlush(dpy);
    }

    void handle(const XEvent& ev) {
        switch (ev.type) {
            case Expose:
                queue.push_back(termchart_gui::tk_event_map::resize(
                    static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)));
                break;
            case ConfigureNotify:
                if (static_cast<int>(ev.xconfigure.width) != w ||
                    static_cast<int>(ev.xconfigure.height) != h) {
                    resize_to(static_cast<std::uint32_t>(ev.xconfigure.width),
                              static_cast<std::uint32_t>(ev.xconfigure.height));
                    queue.push_back(termchart_gui::tk_event_map::resize(
                        static_cast<std::uint32_t>(w),
                        static_cast<std::uint32_t>(h)));
                }
                break;
            case ButtonPress: {
                const int b = ev.xbutton.button;
                if (b == 4 || b == 5) {  // 滚轮
                    termchart_gui::Event e;
                    e.type = termchart_gui::EventType::Wheel;
                    e.x = static_cast<double>(ev.xbutton.x);
                    e.y = static_cast<double>(ev.xbutton.y);
                    e.wheel_delta = (b == 4) ? 1.0 : -1.0;
                    queue.push_back(std::move(e));
                } else {
                    queue.push_back(termchart_gui::tk_event_map::mouse_press(
                        static_cast<double>(ev.xbutton.x),
                        static_cast<double>(ev.xbutton.y), b));
                }
                break;
            }
            case ButtonRelease:
                if (ev.xbutton.button != 4 && ev.xbutton.button != 5) {
                    queue.push_back(termchart_gui::tk_event_map::mouse_release(
                        static_cast<double>(ev.xbutton.x),
                        static_cast<double>(ev.xbutton.y), ev.xbutton.button));
                }
                break;
            case MotionNotify:
                queue.push_back(termchart_gui::tk_event_map::mouse_move(
                    static_cast<double>(ev.xmotion.x), static_cast<double>(ev.xmotion.y)));
                break;
            case KeyPress: {
                if (static_cast<int>(ev.xkey.keycode) == kVkEscape) {
                    queue.push_back(termchart_gui::tk_event_map::quit());
                    running = false;
                    break;
                }
                char buf[8] = {0};
                KeySym ks = 0;
                XLookupString(const_cast<XKeyEvent*>(&ev.xkey), buf, sizeof(buf) - 1, &ks,
                              nullptr);
                queue.push_back(termchart_gui::tk_event_map::key_press(
                    static_cast<int>(ev.xkey.keycode), std::string(buf)));
                break;
            }
            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == wm_delete) {
                    queue.push_back(termchart_gui::tk_event_map::quit());
                    running = false;
                }
                break;
            default:
                break;
        }
    }
};

X11Backend::X11Backend(const termchart_gui::NativeSurface& host) {
    impl_ = std::make_unique<Impl>(static_cast<Display*>(host.display),
                                   static_cast<Window>(
                                       reinterpret_cast<std::uintptr_t>(host.surface)),
                                   host.width, host.height);
}

X11Backend::~X11Backend() = default;

bool X11Backend::valid() const noexcept {
    return impl_ != nullptr && impl_->dpy != nullptr && impl_->win != 0;
}

bool X11Backend::is_external() const noexcept { return impl_ != nullptr && impl_->external; }

void X11Backend::resize(std::uint32_t width, std::uint32_t height) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->resize_to(width, height);
    impl_->queue.push_back(termchart_gui::tk_event_map::resize(width, height));
}

std::unique_ptr<termchart_gui::TkWindow> X11Backend::create_window(std::uint32_t,
                                                                   std::uint32_t, std::string) {
    return nullptr;  // 外部模式：窗口由宿主创建
}

termchart_gui::TkCanvas& X11Backend::canvas() { return impl_->canvas; }

bool X11Backend::poll_event(termchart_gui::Event& out) {
    if (impl_ == nullptr || impl_->dpy == nullptr) {
        return false;
    }
    while (XPending(impl_->dpy) > 0) {
        XEvent ev{};
        XNextEvent(impl_->dpy, &ev);
        if (ev.xany.window != impl_->win && ev.type != ClientMessage) {
            continue;  // 宿主可能还有别的窗口，只处理自己的
        }
        impl_->handle(ev);
    }
    if (!impl_->queue.empty()) {
        out = impl_->queue.front();
        impl_->queue.pop_front();
        return true;
    }
    return false;
}

void X11Backend::present() {
    if (impl_ != nullptr) {
        impl_->paint();
    }
}

void X11Backend::run_loop(termchart_gui::EventLoop& loop) {
    // 消息泵归宿主；库侧只跑自己的事件循环（被动消费 inject_event / poll_event）。
    loop.run();
}

void X11Backend::inject_event(termchart_gui::Event e) {
    if (impl_ != nullptr) {
        impl_->queue.push_back(std::move(e));
    }
}

}  // namespace termchart_gui_x11
