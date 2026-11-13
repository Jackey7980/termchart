// embed_demo.cpp —— Take 2 句柄注入演示：用户拥有窗口，termchart 只当像素渲染器
//
// 与 win32_demo（Take 1：库自建窗口）相对照：这里窗口由宿主自己 CreateWindow 创建、
// 自己 DestroyWindow 销毁；库侧的 Win32Backend 用 NativeSurface{hwnd} 构造，只把
// 渲染好的 Bitmap 贴进客户区，并把宿主 WndProc 里的鼠标/键盘消息 inject_event 回传。
// 证明「句柄注入 + 适配器」模型可以完全不碰 Take 1 的 present()/run_loop 契约。
//
// 跑法：embed_demo            → 开窗口交互（ESC 或关闭按钮退出）
//       embed_demo --smoke    → 自动跑数帧后退出（返回 0 即通过）
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

using namespace termchart_gui;

namespace {

struct HostCtx {
    termchart_gui_win32::Win32Backend* backend = nullptr;
    Mount* mount = nullptr;
    Container* root = nullptr;
    bool running = true;
};

double msg_x(LPARAM lp) {
    return static_cast<double>(static_cast<int>(static_cast<short>(LOWORD(lp))));
}
double msg_y(LPARAM lp) {
    return static_cast<double>(static_cast<int>(static_cast<short>(HIWORD(lp))));
}

LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HostCtx* ctx = reinterpret_cast<HostCtx*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_PAINT:
            if (ctx != nullptr && ctx->mount != nullptr) {
                ctx->mount->paint_all();  // 内部 present() → GetDC + StretchDIBits
            }
            ValidateRect(hwnd, nullptr);
            return 0;
        case WM_SIZE:
            if (ctx != nullptr && ctx->backend != nullptr && ctx->root != nullptr) {
                const auto w = static_cast<std::uint32_t>(LOWORD(lp));
                const auto h = static_cast<std::uint32_t>(HIWORD(lp));
                ctx->backend->resize(w, h);
                ctx->root->set_bounds(Rect{0.0, 0.0, static_cast<double>(w),
                                            static_cast<double>(h)});
            }
            return 0;
        case WM_MOUSEMOVE:
            if (ctx != nullptr && ctx->backend != nullptr) {
                ctx->backend->inject_event(tk_event_map::mouse_move(msg_x(lp), msg_y(lp)));
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (ctx != nullptr && ctx->backend != nullptr) {
                ctx->backend->inject_event(
                    tk_event_map::mouse_press(msg_x(lp), msg_y(lp), 0));
            }
            return 0;
        case WM_LBUTTONUP:
            if (ctx != nullptr && ctx->backend != nullptr) {
                ctx->backend->inject_event(
                    tk_event_map::mouse_release(msg_x(lp), msg_y(lp), 0));
            }
            return 0;
        case WM_KEYDOWN:
            if (ctx != nullptr && ctx->backend != nullptr) {
                if (static_cast<int>(wp) == 0x1B) {  // ESC 退出
                    ctx->running = false;
                } else {
                    ctx->backend->inject_event(
                        tk_event_map::key_press(static_cast<int>(wp)));
                }
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;  // 画面全由 present() 负责，避免闪烁
        case WM_DESTROY:
            if (ctx != nullptr) {
                ctx->running = false;
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    // ① 用户自己创建窗口——这就是宿主，生命周期完全由用户管。
    constexpr wchar_t kClass[] = L"termchart_embed_host";
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &HostProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    RECT rc{0, 0, 640, 420};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, kClass, L"User-owned host (Take 2 embed)",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr,
                                inst, nullptr);
    if (hwnd == nullptr) {
        std::fprintf(stderr, "embed_demo: host window create failed\n");
        return 2;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    NativeSurface surface{};
    surface.surface = hwnd;
    surface.width = static_cast<std::uint32_t>(client.right > 0 ? client.right : 1);
    surface.height = static_cast<std::uint32_t>(client.bottom > 0 ? client.bottom : 1);

    // ② 句柄注入：后端不建窗，只往宿主客户区贴像素。
    termchart_gui_win32::Win32Backend backend(surface);
    if (!backend.valid() || !backend.is_external()) {
        std::fprintf(stderr, "embed_demo: injected backend invalid\n");
        DestroyWindow(hwnd);
        return 3;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, static_cast<double>(surface.width),
                         static_cast<double>(surface.height)});
    root.emplace_child<Label>("title", "Take 2 - user-owned window",
                              Rgb{240, 240, 245}, Rgb{28, 32, 40});
    auto& slider = root.emplace_child<Slider>("slider");
    auto& bar = root.emplace_child<ProgressBar>("bar");
    slider.set_on_change([&](double v) { bar.set_value(v); });

    Mount mount(root, backend);
    HostCtx ctx{&backend, &mount, &root, true};
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    mount.paint_all();

    int frames = 0;
    while (ctx.running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                ctx.running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!ctx.running) {
            break;
        }
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) {
                ctx.running = false;
                break;
            }
            (void)mount.dispatch_event(e);
            dirty = true;
        }
        if (dirty) {
            mount.paint_all();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(smoke ? 20 : 8));
        if (smoke && ++frames >= 6) {
            ctx.running = false;
        }
    }

    std::fprintf(stderr, "embed_demo: %s\n",
                 smoke ? "smoke rendered, user window intact"
                       : "closed by host");
    // ③ 用户自己销毁窗口；Win32Backend 析构因 external 模式绝不动它。
    DestroyWindow(hwnd);
    return 0;
}
