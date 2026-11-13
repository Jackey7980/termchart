// x11_demo.cpp —— Take 2 句柄注入（X11）：用户拥有窗口，库只贴像素 + 翻事件
//
// 宿主自己 XCreateSimpleWindow / XDestroyWindow；X11Backend 只把 BitmapCanvas 贴进
// 该窗口客户区，并把 X 事件翻成中性 Event 入队。等价于 win32 的 embed_demo。
//
// 跑法：x11_demo            → 开窗口交互（ESC / 关闭按钮退出）
//       x11_demo --smoke    → 自动跑数帧后退出（返回 0 即通过；可配 Xvfb 无头跑）
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

// 同 x11_backend.cpp：termchart 头先解析，Xlib 后入并清掉同名宏，避免 Attr::None /
// ColorDepth::TrueColor / core::Status 被打碎。
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_x11/x11_backend.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

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

using namespace termchart_gui;

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    Display* dpy = XOpenDisplay(nullptr);
    if (dpy == nullptr) {
        std::fprintf(stderr, "x11_demo: XOpenDisplay failed (no DISPLAY?)\n");
        return 2;
    }
    const int screen = DefaultScreen(dpy);
    const std::uint32_t w = 640;
    const std::uint32_t h = 420;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, w, h, 1,
                                     BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "User-owned host (Take 2 X11)");
    XSelectInput(dpy, win,
                 ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | KeyPressMask);
    const Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, const_cast<Atom*>(&wm_delete), 1);
    XMapWindow(dpy, win);
    XFlush(dpy);

    NativeSurface surface{};
    surface.surface = reinterpret_cast<void*>(static_cast<std::uintptr_t>(win));
    surface.display = dpy;
    surface.width = w;
    surface.height = h;

    int rc = 0;
    // 后端 / 控件树 / Mount 全部在作用域内；离开作用域时先析构后端（释放 GC/XImage，
    // 此时 Display 仍开着），再销毁用户窗口、关闭 Display——顺序不能反。
    {
        termchart_gui_x11::X11Backend backend(surface);
        if (!backend.valid() || !backend.is_external()) {
            std::fprintf(stderr, "x11_demo: injected backend invalid\n");
            rc = 3;
        } else {
            Container root("root");
            root.set_bounds(Rect{0, 0, static_cast<double>(w), static_cast<double>(h)});
            root.emplace_child<Label>("title", "Take 2 - X11 user-owned window",
                                      Rgb{240, 240, 245}, Rgb{28, 32, 40});
            auto& slider = root.emplace_child<Slider>("slider");
            auto& bar = root.emplace_child<ProgressBar>("bar");
            slider.set_on_change([&](double v) { bar.set_value(v); });

            Mount mount(root, backend);
            mount.paint_all();
            {
                const Bitmap& b = backend.canvas().snapshot();
                bool has_content = false;
                const std::size_t n = static_cast<std::size_t>(b.width()) * b.height() * 4;
                const std::uint8_t* p = b.data();
                for (std::size_t i = 0; i + 3 < n; i += 4) {
                    if (p[i] != 0 || p[i + 1] != 0 || p[i + 2] != 0) {
                        has_content = true;
                        break;
                    }
                }
                if (!has_content) {
                    std::fprintf(stderr, "x11_demo: canvas stayed empty\n");
                    rc = 4;
                }
            }

            bool running = true;
            int frames = 0;
            while (running) {
                Event e;
                bool got = false;
                while (backend.poll_event(e)) {
                    if (e.type == EventType::Quit) {
                        running = false;
                        break;
                    }
                    if (e.type == EventType::Resize) {
                        root.set_bounds(Rect{0, 0, static_cast<double>(e.width),
                                             static_cast<double>(e.height)});
                    }
                    (void)mount.dispatch_event(e);
                    got = true;
                }
                if (!running) {
                    break;
                }
                if (got) {
                    mount.paint_all();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(smoke ? 20 : 4));
                if (smoke && ++frames >= 6) {
                    running = false;
                }
            }
        }
    }

    std::fprintf(stderr, "x11_demo: %s\n",
                 smoke ? "smoke rendered, user window intact" : "closed by host");
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return rc;
}
