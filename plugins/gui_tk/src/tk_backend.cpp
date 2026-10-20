// tk_backend.cpp —— Tk 后端实现（Tcl/Tk C API；PIMPL 隔离 Tk 类型）
#include "termchart_gui_tk/tk_backend.hpp"

#include <deque>
#include <mutex>
#include <string>

#include <tcl.h>
#include <tk.h>

namespace termchart_gui_tk {
namespace {

// Tcl 命令名（bind 脚本引用）：小写防撞车。
constexpr const char* kCmdName = "termchart_tk_event";

// Tk 事件状态位 → EventModifier：Shift=0x1 Control=0x4 Alt=0x8/0x20000
// （Alt 的位在 X11/Win32 后端不一致，两处都查）。
std::uint32_t state_to_modifiers(long state) noexcept {
    std::uint32_t bits = 0;
    if ((state & 0x1) != 0) {
        bits |= termchart_gui::kModShift;
    }
    if ((state & 0x4) != 0) {
        bits |= termchart_gui::kModCtrl;
    }
    if ((state & 0x8) != 0 || (state & 0x20000) != 0) {
        bits |= termchart_gui::kModAlt;
    }
    return bits;
}

// Tcl 数字文本 → double（空/坏值给 fallback，不炸消息泵）。
double num(const char* s, double fallback) noexcept {
    if (s == nullptr) {
        return fallback;
    }
    try {
        return std::stod(std::string{s});
    } catch (...) {
        return fallback;
    }
}

}  // namespace

struct TkBackend::Impl {
    Tcl_Interp* interp = nullptr;
    Tk_Window mainwin = nullptr;    // 主窗（Tk_MainWindow）
    Tk_PhotoHandle photo = nullptr; // photo image 句柄（上屏目标）
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool running = false;

    std::unique_ptr<termchart_gui::BitmapCanvas> canvas;
    std::function<void(std::uint32_t, std::uint32_t)> render_hook;

    std::mutex queue_mutex;
    std::deque<termchart_gui::Event> queue;

    // canvas 控件路径（主窗路径 + ".chartcanvas"）。mainwin 名字一般是 "."。
    [[nodiscard]] std::string canvas_path() const {
        const char* main_name = Tk_Name(mainwin);
        return std::string{main_name != nullptr ? main_name : "."} + ".chartcanvas";
    }

    void push(termchart_gui::Event e) {
        // Resize/Quit 在入队时同步生效（Win32/Qt 同款"就地回调"语义）：
        // Tcl 回调本就在 UI 线程，安全。
        if (e.type == termchart_gui::EventType::Resize && render_hook) {
            render_hook(e.width, e.height);
        }
        if (e.type == termchart_gui::EventType::Quit) {
            running = false;
        }
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.push_back(std::move(e));
    }
};

namespace {

// bind 脚本 → C 命令的回调。ClientData = Impl*；子命令与参数约定见 setup_bindings。
int TkEventCmd(ClientData clientData, Tcl_Interp* interp, int objc,
               Tcl_Obj* const* objv) {
    if (objc < 2 || clientData == nullptr) {
        return TCL_OK;
    }
    auto* impl = static_cast<TkBackend::Impl*>(clientData);
    const std::string kind = Tcl_GetString(objv[1]);
    const auto arg = [&](int i) { return Tcl_GetString(objv[i]); };
    termchart_gui::Event e{};

    if (kind == "press" && objc >= 6) {
        e.type = termchart_gui::EventType::MousePress;
        e.x = num(arg(2), 0);
        e.y = num(arg(3), 0);
        e.button = static_cast<int>(num(arg(4), 1));
        e.modifiers = state_to_modifiers(static_cast<long>(num(arg(5), 0)));
    } else if (kind == "release" && objc >= 6) {
        e.type = termchart_gui::EventType::MouseRelease;
        e.x = num(arg(2), 0);
        e.y = num(arg(3), 0);
        e.button = static_cast<int>(num(arg(4), 1));
        e.modifiers = state_to_modifiers(static_cast<long>(num(arg(5), 0)));
    } else if (kind == "motion" && objc >= 4) {
        e.type = termchart_gui::EventType::MouseMove;
        e.x = num(arg(2), 0);
        e.y = num(arg(3), 0);
    } else if (kind == "wheel" && objc >= 6) {
        e.type = termchart_gui::EventType::Wheel;
        e.x = num(arg(2), 0);
        e.y = num(arg(3), 0);
        e.wheel_delta = num(arg(4), 0);
        e.modifiers = state_to_modifiers(static_cast<long>(num(arg(5), 0)));
    } else if (kind == "key" && objc >= 4) {
        e.type = termchart_gui::EventType::KeyPress;
        e.text = arg(2);  // %A：可打印键的字符
        e.modifiers = state_to_modifiers(static_cast<long>(num(arg(3), 0)));
    } else if (kind == "configure" && objc >= 4) {
        e.type = termchart_gui::EventType::Resize;
        e.width = static_cast<std::uint32_t>(num(arg(2), 0));
        e.height = static_cast<std::uint32_t>(num(arg(3), 0));
    } else if (kind == "quit") {
        e.type = termchart_gui::EventType::Quit;
    } else {
        return TCL_OK;  // 未知子命令：忽略（不炸消息泵）
    }
    impl->push(std::move(e));
    (void)interp;
    return TCL_OK;
}

// Tcl 层事件绑定（可移植：不吃 XEvent 结构差异）。%x/%y/… 由 Tk 填，
// %S 是十进制 state 位掩码文本（回调转 EventModifier）。
bool setup_bindings(TkBackend::Impl& impl) {
    Tcl_Interp* interp = impl.interp;
    const std::string c = impl.canvas_path();
    const char* scripts[] = {
        "bind CH <Button-1> {EVT press %x %y %b %S}",
        "bind CH <ButtonRelease-1> {EVT release %x %y %b %S}",
        "bind CH <B1-Motion> {EVT motion %x %y}",
        "bind CH <Button-4> {EVT wheel %x %y -1 %S}",  // Linux 滚轮：上 -1
        "bind CH <Button-5> {EVT wheel %x %y 1 %S}",   // Linux 滚轮：下 +1
        "bind CH <MouseWheel> {EVT wheel %x %y [expr {%D / 120}] %S}",  // Win/macOS
        "bind CH <Key> {EVT key %A %S}",
        "bind CH <Configure> {EVT configure %w %h}",
        "bind CH <Destroy> {EVT quit}",
        "wm protocol . WM_DELETE_WINDOW {EVT quit}",
        nullptr,
    };
    for (int i = 0; scripts[i] != nullptr; ++i) {
        std::string script = scripts[i];
        for (const std::size_t at = script.find("CH"); at != std::string::npos;) {
            script.replace(at, 2, c);
            break;  // 每条脚本只有一个 CH 占位
        }
        for (const std::size_t at = script.find("EVT"); at != std::string::npos;) {
            script.replace(at, 3, kCmdName);
            break;
        }
        if (Tcl_EvalEx(interp, script.c_str(), -1, TCL_EVAL_GLOBAL) != TCL_OK) {
            return false;
        }
    }
    return true;
}

// 构造中途失败的统一清理：删解释器（连带窗口/photo）+ 标记无效。
void fail(TkBackend::Impl& impl) {
    if (impl.interp != nullptr) {
        Tcl_DeleteInterp(impl.interp);
        impl.interp = nullptr;
    }
    impl.photo = nullptr;
}

}  // namespace

TkBackend::TkBackend(std::uint32_t width, std::uint32_t height, std::string title) {
    auto impl = std::make_unique<Impl>();
    impl->width = width;
    impl->height = height;

    impl->interp = Tcl_CreateInterp();
    if (impl->interp == nullptr || Tcl_Init(impl->interp) != TCL_OK ||
        Tk_Init(impl->interp) != TCL_OK) {
        fail(*impl);
        impl_ = std::move(impl);
        return;  // valid() == false（无显示环境/Tk 未装）
    }
    impl->mainwin = Tk_MainWindow(impl->interp);
    if (impl->mainwin == nullptr) {
        fail(*impl);
        impl_ = std::move(impl);
        return;
    }

    // 主窗标题 + 初始尺寸（用 Tcl 字符串插值，不做 shell 转义猜测）。
    Tcl_SetVar(impl->interp, "tc_title", title.c_str(), TCL_GLOBAL_ONLY);
    Tcl_SetVar(impl->interp, "tc_geometry",
               (std::to_string(width) + "x" + std::to_string(height)).c_str(),
               TCL_GLOBAL_ONLY);
    if (Tcl_EvalEx(impl->interp, "wm title . $tc_title\nwm geometry . $tc_geometry",
                   -1, TCL_EVAL_GLOBAL) != TCL_OK) {
        fail(*impl);
        impl_ = std::move(impl);
        return;
    }

    // canvas 控件 + photo image（上屏目标）。
    const std::string c = impl->canvas_path();
    if (Tcl_EvalEx(impl->interp,
                   ("canvas " + c + " -highlightthickness 0 -borderwidth 0\n"
                    "pack " + c + " -fill both -expand 1")
                       .c_str(),
                   -1, TCL_EVAL_GLOBAL) != TCL_OK) {
        fail(*impl);
        impl_ = std::move(impl);
        return;
    }
    if (Tcl_EvalEx(impl->interp,
                   ("image create photo tc_img -width " + std::to_string(width) +
                    " -height " + std::to_string(height) + "\n" + c +
                    " create image 0 0 -image tc_img -anchor nw")
                       .c_str(),
                   -1, TCL_EVAL_GLOBAL) != TCL_OK) {
        fail(*impl);
        impl_ = std::move(impl);
        return;
    }
    impl->photo = Tk_FindPhoto(impl->interp, "tc_img");
    if (impl->photo == nullptr) {
        fail(*impl);
        impl_ = std::move(impl);
        return;
    }

    // 事件桥：Tcl command（ClientData = Impl*）+ 可移植 bindings。
    Tcl_CreateObjCommand(impl->interp, kCmdName, TkEventCmd, impl.get(), nullptr);
    impl->canvas = std::make_unique<termchart_gui::BitmapCanvas>(width, height);
    if (!setup_bindings(*impl)) {
        fail(*impl);  // setup 失败：刷掉整个 interp（bindings 可能半套）
        impl_ = std::move(impl);
        return;
    }
    impl_ = std::move(impl);
}

TkBackend::~TkBackend() {
    if (impl_ != nullptr && impl_->interp != nullptr) {
        Tcl_DeleteInterp(impl_->interp);  // 连带销毁窗口/photo/interp
        impl_->interp = nullptr;
    }
}

bool TkBackend::valid() const noexcept {
    return impl_ != nullptr && impl_->interp != nullptr && impl_->photo != nullptr;
}

std::unique_ptr<termchart_gui::TkWindow> TkBackend::create_window(std::uint32_t w,
                                                                  std::uint32_t h,
                                                                  std::string title) {
    // Tk 单主窗模型：窗口管理归 Tcl 脚本层，本后端只持主窗。多窗口场景
    // 返回空（与无头后端同口径：窗口非关注点，画布才是）。
    (void)w;
    (void)h;
    (void)title;
    return nullptr;
}

termchart_gui::TkCanvas& TkBackend::canvas() { return *impl_->canvas; }

bool TkBackend::poll_event(termchart_gui::Event& out) {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    if (impl_->queue.empty()) {
        return false;
    }
    out = impl_->queue.front();
    impl_->queue.pop_front();
    return true;
}

void TkBackend::present() {
    if (!valid()) {
        return;
    }
    // RGBA 行块直贴 photo image（offset[] 是通道下标：R,G,B,A）。
    const termchart_gui::Bitmap& px = impl_->canvas->pixels();
    Tk_PhotoImageBlock block{};
    block.pixelPtr =
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(px.data()));
    block.width = static_cast<int>(px.width());
    block.height = static_cast<int>(px.height());
    block.pitch = static_cast<int>(px.row_bytes());
    block.pixelSize = 4;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;
    Tk_PhotoPutBlock(impl_->photo, &block, 0, 0, block.width, block.height,
                     TK_PHOTO_COMPOSITE_OVERLAY);
}

void TkBackend::run_loop(termchart_gui::EventLoop& loop) {
    (void)loop;  // Tcl 的消息泵即循环；loop 的 post/定时器由应用自行桥接
    if (!valid()) {
        return;
    }
    impl_->running = true;
    while (impl_->running) {
        Tcl_DoOneEvent(TCL_ALL_EVENTS);  // 派发窗口/idle/timer 事件
        Tcl_Sleep(1);                    // 无事件时让出 CPU（ms）
    }
}

void TkBackend::request_close() {
    if (valid()) {
        Tcl_EvalEx(impl_->interp, "destroy .", -1, TCL_EVAL_GLOBAL);
    }
    if (impl_ != nullptr) {
        impl_->running = false;
    }
}

void TkBackend::set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook) {
    impl_->render_hook = std::move(hook);
}

void TkBackend::process_events() {
    if (valid()) {
        Tcl_DoOneEvent(TCL_ALL_EVENTS);
    }
}

}  // namespace termchart_gui_tk
