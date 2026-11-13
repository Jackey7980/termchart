// win32_backend.cpp —— 原生 Win32 后端实现（平台胶水，全部关在本文件）
//
// 模型：离屏 image::Bitmap 当画布（复用 termchart_gui::BitmapCanvas），控件照常画进去；
// present() 触发 WM_PAINT，由 StretchDIBits 把位图（RGBA→BGRA 重排）贴进窗口 DC。
// 事件：OS 鼠标/键盘消息在窗口过程里经 tk_event_map 翻成 Event 入队，poll_event 非阻塞
// 抽取。零第三方依赖，仅链系统 user32/gdi32。
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

namespace termchart_gui_win32 {
namespace {

constexpr wchar_t kClassName[] = L"termchart_gui_win32_wnd";
constexpr int kVkEscape = 0x1B;

std::wstring to_wide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                      nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    }
    return w;
}

double px_x(LPARAM lp) {
    return static_cast<double>(static_cast<int>(static_cast<short>(LOWORD(lp))));
}
double px_y(LPARAM lp) {
    return static_cast<double>(static_cast<int>(static_cast<short>(HIWORD(lp))));
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);  // 定义见后（需 Impl 完整类型）

}  // namespace

struct Win32Backend::Impl {
    HINSTANCE hinst = nullptr;
    HWND hwnd = nullptr;
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    std::unique_ptr<termchart_gui::BitmapCanvas> canvas;
    std::deque<termchart_gui::Event> queue;
    bool running = false;
    bool class_ok = false;
    std::vector<unsigned char> dib_;  // 复用的 BGRA 上屏缓冲（尺寸变化才 resize，免每帧分配）
    termchart_gui::Bitmap last_frame_;  // 上一帧（resize 时拉伸旧图、消灭黑洞的源）
    bool canvas_fresh_ = false;         // 画布是否已在当前尺寸渲染完成
    std::function<void(std::uint32_t, std::uint32_t)> render_hook_;  // WM_SIZE 同步重渲染钩子
    bool external = false;  // Take 2：用户拥有窗口（不销毁 hwnd、不跑 OS 消息泵）

    Impl(HINSTANCE inst, std::uint32_t width, std::uint32_t height)
        : hinst(inst), w(width), h(height),
          canvas(std::make_unique<termchart_gui::BitmapCanvas>(width, height)) {}

    bool ensure_class() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &WndProc;
        wc.hInstance = hinst;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW (wide)
        ATOM atom = RegisterClassExW(&wc);
        if (atom == 0) {
            return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        return true;
    }

    bool create(std::string title) {
        class_ok = ensure_class();
        if (!class_ok) {
            return false;
        }
        RECT rc{0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        hwnd = CreateWindowExW(0, kClassName, to_wide(title).c_str(),
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr,
                               hinst, this);
        if (hwnd == nullptr) {
            return false;
        }
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        running = true;
        return true;
    }

    void resize_to(std::uint32_t nw, std::uint32_t nh) {
        if (canvas && canvas_fresh_) {
            last_frame_ = canvas->pixels();  // 仅在画布是已渲染帧时更新旧图源（免拷 garbage）
        }
        w = nw;
        h = nh;
        canvas = std::make_unique<termchart_gui::BitmapCanvas>(nw, nh);
        canvas_fresh_ = false;  // 新画布未渲染前，WM_PAINT 走拉伸旧图，避免暴露区涂黑
    }

    LRESULT handle(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                HDC hdc = BeginPaint(h, &ps);
                blit(hdc);
                EndPaint(h, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN:
                queue.push_back(termchart_gui::tk_event_map::mouse_press(px_x(lp), px_y(lp), 0));
                return 0;
            case WM_LBUTTONUP:
                queue.push_back(termchart_gui::tk_event_map::mouse_release(px_x(lp), px_y(lp), 0));
                return 0;
            case WM_MOUSEMOVE:
                queue.push_back(termchart_gui::tk_event_map::mouse_move(px_x(lp), px_y(lp)));
                return 0;
            case WM_KEYDOWN:
                if (static_cast<int>(wp) == kVkEscape) {
                    queue.push_back(termchart_gui::tk_event_map::quit());
                } else {
                    queue.push_back(termchart_gui::tk_event_map::key_press(static_cast<int>(wp)));
                }
                return 0;
            case WM_SIZE: {
                const auto cw = static_cast<std::uint32_t>(LOWORD(lp));
                const auto chh = static_cast<std::uint32_t>(HIWORD(lp));
                resize_to(cw, chh);
                queue.push_back(termchart_gui::tk_event_map::resize(cw, chh));
                if (render_hook_) {
                    render_hook_(cw, chh);  // 拖动中就地同步重渲染（不等被阻塞的主循环）
                }
                return 0;
            }
            case WM_GETMINMAXINFO: {  // 最小跟踪尺寸：别让窗口缩到连坐标轴都排不下
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
                mmi->ptMinTrackSize.x = 320;
                mmi->ptMinTrackSize.y = 240;
                return 0;
            }
            case WM_DESTROY:
                running = false;
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(h, msg, wp, lp);
        }
    }

    // 上屏：把源 Bitmap（RGBA）重排成 BGRA 后 StretchDIBits 贴进 DC。
    //   * 画布已在当前尺寸渲染（present 后）→ 源=画布，1:1 贴；
    //   * 刚 resize、画布还没渲染 → 源=上一帧 last_frame_，StretchDIBits 拉伸铺满新客户区
    //     （硬件级微秒拷贝，消灭"暴露区涂黑/残影"，画面死咬鼠标边缘）。平滑"对焦"由
    //     ChartWidget 指数趋近动画负责，故这里不需交叉淡入。
    // 热路径：只读引用源（不整图拷贝）+ 复用 BGRA 缓冲 + 裸指针重排。
    void blit(HDC hdc) {
        const termchart_gui::Bitmap& canv = canvas->pixels();
        blit_to(hdc, static_cast<int>(canv.width()), static_cast<int>(canv.height()));
    }

    // 指定目标尺寸上屏：外部句柄模式下客户区可能大于/小于画布，StretchDIBits 自动缩放。
    void blit_to(HDC hdc, int dw, int dh) {
        const termchart_gui::Bitmap& canv = canvas->pixels();
        if (dw <= 0 || dh <= 0 || hdc == nullptr) {
            return;
        }
        const termchart_gui::Bitmap& src =
            (canvas_fresh_ || last_frame_.width() == 0) ? canv : last_frame_;
        const int sw = static_cast<int>(src.width());
        const int sh = static_cast<int>(src.height());
        if (sw <= 0 || sh <= 0) {
            return;
        }
        const std::size_t need =
            static_cast<std::size_t>(sw) * static_cast<std::size_t>(sh) * 4;
        if (dib_.size() != need) {
            dib_.resize(need);
        }
        const unsigned char* s = src.data();
        unsigned char* d = dib_.data();
        for (int y = 0; y < sh; ++y) {
            const unsigned char* srow =
                s + static_cast<std::size_t>(y) * static_cast<std::size_t>(sw) * 4;
            unsigned char* drow =
                d + static_cast<std::size_t>(y) * static_cast<std::size_t>(sw) * 4;
            for (int x = 0; x < sw; ++x) {
                const unsigned char* sp = srow + static_cast<std::size_t>(x) * 4;
                unsigned char* dp = drow + static_cast<std::size_t>(x) * 4;
                dp[0] = sp[2];  // DIB 32bpp 内存序为 B,G,R,A
                dp[1] = sp[1];
                dp[2] = sp[0];
                dp[3] = 255;
            }
        }
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = sw;
        bi.bmiHeader.biHeight = -sh;  // 自上而下
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        // dest=整个客户区、src=整幅：sw×sh 与 dw×dh 不同时 StretchDIBits 自动拉伸。
        StretchDIBits(hdc, 0, 0, dw, dh, 0, 0, sw, sh, dib_.data(), &bi, DIB_RGB_COLORS,
                      SRCCOPY);
    }
};

namespace {

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32Backend::Impl* impl = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCT*>(lp);
        impl = static_cast<Win32Backend::Impl*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
    } else {
        impl = reinterpret_cast<Win32Backend::Impl*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (impl != nullptr) {
        return impl->handle(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

// ── 原生窗口（TkWindow）───────────────────────────────────────────────────────
namespace {

class Win32Window final : public termchart_gui::TkWindow {
public:
    explicit Win32Window(Win32Backend::Impl* impl) : impl_(impl) {}

    void set_title(std::string title) override {
        if (impl_ != nullptr && impl_->hwnd != nullptr) {
            SetWindowTextW(impl_->hwnd, to_wide(title).c_str());
        }
    }
    void resize(std::uint32_t nw, std::uint32_t nh) override {
        if (impl_ == nullptr) {
            return;
        }
        impl_->resize_to(nw, nh);
        RECT rc{0, 0, static_cast<LONG>(nw), static_cast<LONG>(nh)};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        if (impl_->hwnd != nullptr) {
            SetWindowPos(impl_->hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
    }
    [[nodiscard]] std::uint32_t width() const override {
        return impl_ != nullptr ? impl_->w : 0;
    }
    [[nodiscard]] std::uint32_t height() const override {
        return impl_ != nullptr ? impl_->h : 0;
    }
    void close() override {
        if (impl_ != nullptr && impl_->hwnd != nullptr) {
            DestroyWindow(impl_->hwnd);
        }
    }

private:
    Win32Backend::Impl* impl_;
};

}  // namespace

Win32Backend::Win32Backend(std::uint32_t width, std::uint32_t height, std::string title) {
    impl_ = std::make_unique<Impl>(GetModuleHandleW(nullptr), width, height);
    impl_->create(std::move(title));
}

// Take 2：绑定用户已有的 HWND。不注册窗口类、不 CreateWindow、不跑消息泵；
// present() 直接把画布贴到该窗口客户区（宿主负责把鼠标/键盘消息 inject_event 回来）。
Win32Backend::Win32Backend(const termchart_gui::NativeSurface& host) {
    const std::uint32_t w = host.width != 0 ? host.width : 1;
    const std::uint32_t h = host.height != 0 ? host.height : 1;
    impl_ = std::make_unique<Impl>(GetModuleHandleW(nullptr), w, h);
    impl_->external = true;
    impl_->hwnd = static_cast<HWND>(host.surface);
    impl_->running = impl_->hwnd != nullptr;
}

Win32Backend::~Win32Backend() {
    // 外部模式：窗口归用户所有，绝不在析构里销毁别人的窗口。
    if (impl_ != nullptr && impl_->hwnd != nullptr && !impl_->external) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
}

bool Win32Backend::valid() const noexcept {
    if (impl_ == nullptr || impl_->hwnd == nullptr) {
        return false;
    }
    return impl_->external ? (IsWindow(impl_->hwnd) != 0) : true;
}

bool Win32Backend::is_external() const noexcept {
    return impl_ != nullptr && impl_->external;
}

void Win32Backend::resize(std::uint32_t width, std::uint32_t height) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->resize_to(width, height);
    impl_->queue.push_back(termchart_gui::tk_event_map::resize(width, height));
}

std::unique_ptr<termchart_gui::TkWindow> Win32Backend::create_window(std::uint32_t w,
                                                                     std::uint32_t h,
                                                                     std::string title) {
    if (impl_ == nullptr) {
        return nullptr;
    }
    if (impl_->external) {
        // 外部模式不接受"由后端建窗"；宿主自己管理窗口与尺寸。
        return nullptr;
    }
    impl_->resize_to(w, h);
    (void)title;  // 主窗口标题在构造时设定；此处仅保证尺寸
    return std::make_unique<Win32Window>(impl_.get());
}

termchart_gui::TkCanvas& Win32Backend::canvas() { return *impl_->canvas; }

bool Win32Backend::poll_event(termchart_gui::Event& out) {
    if (impl_ == nullptr || impl_->hwnd == nullptr) {
        return false;
    }
    if (!impl_->external) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                impl_->running = false;
                out = termchart_gui::tk_event_map::quit();
                return true;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (!impl_->queue.empty()) {
        out = impl_->queue.front();
        impl_->queue.pop_front();
        return true;
    }
    return false;
}

void Win32Backend::present() {
    if (impl_ == nullptr || impl_->hwnd == nullptr) {
        return;
    }
    impl_->canvas_fresh_ = true;  // 应用刚渲染完：画布在当前尺寸有效
    if (impl_->external) {
        // 用户窗口：按当前客户区尺寸贴图（客户区与画布不一致时 StretchDIBits 拉伸）。
        RECT rc{};
        int cw = static_cast<int>(impl_->w);
        int ch = static_cast<int>(impl_->h);
        if (GetClientRect(impl_->hwnd, &rc) != 0) {
            if (rc.right > 0 && rc.bottom > 0) {
                cw = rc.right;
                ch = rc.bottom;
            }
        }
        HDC hdc = GetDC(impl_->hwnd);
        if (hdc != nullptr) {
            impl_->blit_to(hdc, cw, ch);
            ReleaseDC(impl_->hwnd, hdc);
        }
        return;
    }
    InvalidateRect(impl_->hwnd, nullptr, FALSE);
    UpdateWindow(impl_->hwnd);  // 同步触发 WM_PAINT → blit
}

void Win32Backend::run_loop(termchart_gui::EventLoop& loop) {
    if (impl_ == nullptr || impl_->hwnd == nullptr) {
        return;
    }
    if (impl_->external) {
        // 外部模式：消息泵在宿主手里，这里只跑库自己的事件循环（被动消费 inject_event）。
        loop.run();
        return;
    }
    MSG msg;
    while (impl_->running && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        termchart_gui::Event e;
        while (poll_event(e)) {
            loop.post(e);
            if (e.type == termchart_gui::EventType::Quit) {
                loop.stop();
                return;
            }
        }
    }
}

void Win32Backend::request_close() {
    if (impl_ == nullptr || impl_->hwnd == nullptr) {
        return;
    }
    if (impl_->external) {
        // 不销毁用户窗口：只让库侧收尾，投递 Quit。
        impl_->running = false;
        impl_->queue.push_back(termchart_gui::tk_event_map::quit());
        return;
    }
    DestroyWindow(impl_->hwnd);
}

void Win32Backend::set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook) {
    if (impl_) {
        impl_->render_hook_ = std::move(hook);
    }
}

}  // namespace termchart_gui_win32
