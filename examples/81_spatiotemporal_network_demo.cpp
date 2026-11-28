// 81_spatiotemporal_network_demo.cpp —— 同时验证 TUI 与 GUI 渲染 3D 时空网络图 + 缩放交互
//
// 数据：硬编码 6 节点 / 10 条有向边（提取自 06:00 切片）。
//   · flow  → 连线粗细（线宽）
//   · weather → 连线颜色（clear=绿 / cloudy=黄 / rain=蓝）
//   · 节点用带背景色的实心方块 + 标签（N01..N06）表示
//
// 架构分层（与用户要求一致）：
//   L21 API/示例（本文件）= 调度者：组织数据、建 Scene、驱动交互、选后端
//   L12 chart3d/math3d  = 生成 Scene（3D 点经 Camera 投影到 2D，边=Stroke+Move/Line，节点=Fill+Rect）
//   L10 canvas/gui::SceneRenderer = 光栅化（终端 braille/halfblock  或  GUI Bitmap）
//   L14 render = 差分重绘（TUI：render::Renderer 只发变化格，绝不 system("cls")）
//
// 已知框架缺口（按任务要求以 TODO 标注）：
//   · TUI 端 Canvas 仅支持 1 格宽线，StrokeOp.width 被 scene_renderer 忽略——
//     用"平行偏移的若干条线"模拟线宽（flow 越大线越粗）；GUI 端 gui::SceneRenderer
//     真正尊重 StrokeOp.width，故 GUI 能看到真实粗细。
//   · 框架无原生"箭头 / 箭头部"图元（graph/arrow 是 2D 示意图子系统，不可用于此）；
//     方向性暂以"源→目标"的连线顺序表达，未画箭头——TODO: 缺箭头 API。
//
// 跑法：
//   默认（无参）进入 TUI 终端交互：+ / - 缩放，r 复位，q 退出（Windows 直接读控制台句柄
//   WaitForSingleObject+ReadFile 非阻塞，不依赖 conio.h；缩放走 interaction::Viewport）。
//   --gui     进入 GUI：顶部固定高度工具栏放 [+] / [-] 按钮，下方画布满剩余空间；点击按钮
//             缩放并重绘（Win32 真窗口 或 零依赖 TuiBackend 终端 halfblock）。
//   --png FILE  用 HeadlessBackend 离屏渲染 TUI 同款“平行偏移线”场景并导出 PNG，
//             肉眼查看 flow→变线宽（平行线间距）效果。

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include <fstream>
#include <atomic>

// 非阻塞读键：Windows 直接读控制台输入句柄（与 term::Terminal::enter() 的 raw 模式
// 同一个 STD_INPUT_HANDLE）：WaitForSingleObject(h,0) 探活 + ReadFile —— 不依赖 conio.h，
// 且真非阻塞（Windows 控制台句柄不支持 O_NONBLOCK，裸 ReadFile/_read 无输入时会阻塞等键，
// 那正是当初 fread 卡死的根因）。其余平台用 fcntl 把 stdin 设为 O_NONBLOCK 单字节读。
#ifdef _WIN32
#include <windows.h>
// windows.h 把 near/far 定义成宏，会破坏 termchart 头里 cam.near/cam.far 的成员访问。
#undef near
#undef far
#else
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <csignal>
#endif

// ── L0–L14：核心 / 数学3D / 图表3D / 画布 / 渲染 / 终端 / 交互 ──
#include "termchart/core/buffer.hpp"
#include "termchart/core/style.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/math3d/camera.hpp"
#include "termchart/chart3d/common.hpp"        // chart3d::make_camera / shade
#include "termchart/scene/scene.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/canvas/scene_renderer.hpp" // canvas::render_scene
#include "termchart/render/renderer.hpp"
#include "termchart/term/terminal.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/key.hpp"
#include "termchart/interaction/zoom.hpp"  // interaction::Viewport（缩放/平移/夹取）

// ── 平行 GUI 栈（header-only，链接 termchart_gui）──
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/backend_tk/tui_backend.hpp"
#include "termchart_gui/backend_tk/backend.hpp"   // HeadlessBackend（离屏 PNG 导出用）
#include "termchart/media/image/png_encode.hpp"          // image::encode_png
// 原生 Win32 真窗口后端（可选插件，默认关）：仅在该插件启用时才存在。
#ifdef TERMCHART_BUILD_GUI_WIN32
#include "termchart_gui_win32/win32_backend.hpp"
#endif

namespace demo {

using namespace termchart;

// ── 数据模型（硬编码，不读文件）─────────────────────────────────────────────
struct Node {
    std::string id;
    math3d::Vec3 pos;
};
struct Edge {
    int from = 0;
    int to = 0;
    double flow = 0.0;    // → 线宽
    double speed = 0.0;   // 仅展示
    std::string weather;  // → 颜色
};
struct Network {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

Network make_network() {
    Network n;
    n.nodes = {
        {"N01", {0.0, 0.0, 0.0}},
        {"N02", {100.0, 50.0, 30.0}},
        {"N03", {-80.0, 60.0, 20.0}},
        {"N04", {20.0, -90.0, 40.0}},
        {"N05", {-30.0, -50.0, 10.0}},
        {"N06", {60.0, 80.0, 50.0}},
    };
    n.edges = {
        {0, 1, 35.0, 110.0, "clear"},
        {0, 2, 28.0, 95.0, "cloudy"},
        {0, 3, 42.0, 130.0, "rain"},
        {1, 5, 20.0, 80.0, "clear"},
        {2, 4, 15.0, 70.0, "cloudy"},
        {3, 4, 33.0, 120.0, "rain"},
        {1, 2, 18.0, 90.0, "clear"},
        {4, 5, 25.0, 100.0, "cloudy"},
        {3, 5, 12.0, 60.0, "clear"},
        {2, 5, 22.0, 105.0, "rain"},
    };
    return n;
}

// weather → 连线颜色（clear=绿 / cloudy=黄 / rain=蓝）
Color weather_color(const std::string& w) {
    if (w == "rain") return Color::rgb(70, 130, 230);
    if (w == "cloudy") return Color::rgb(230, 200, 60);
    return Color::rgb(60, 200, 90);  // clear
}

// flow → 目标线宽（场景单位）：12..42 映射到约 1..5。
double flow_width(double flow) {
    const double w = 1.0 + (flow - 10.0) / 8.0;
    return w < 1.0 ? 1.0 : (w > 5.0 ? 5.0 : w);
}

// 点云包围盒中心 + 半径（用于自动取景）。
void bounds(const Network& net, math3d::Vec3& center, double& radius) {
    math3d::Vec3 mn = net.nodes.front().pos, mx = net.nodes.front().pos;
    for (const auto& nd : net.nodes) {
        mn.x = std::min(mn.x, nd.pos.x); mx.x = std::max(mx.x, nd.pos.x);
        mn.y = std::min(mn.y, nd.pos.y); mx.y = std::max(mx.y, nd.pos.y);
        mn.z = std::min(mn.z, nd.pos.z); mx.z = std::max(mx.z, nd.pos.z);
    }
    center = (mn + mx) * 0.5;
    radius = (mx - mn).length() * 0.5;
    if (!(radius > 0.0)) radius = 1.0;
}

// ── L12：生成 Scene（3D 投影到 2D 图元）。被 TUI 与 GUI 共用。──────────────
// faux_width=true → 用平行偏移的若干条线模拟线宽（TUI：Canvas 忽略 StrokeOp.width）；
// faux_width=false → 直接设 StrokeOp.width（GUI：gui::SceneRenderer 尊重线宽）。
scene::Scene build_scene(const Network& net, int cols, int rows,
                         const interaction::Viewport& vp, bool faux_width) {
    scene::Scene s;
    math3d::Vec3 center;
    double radius = 0.0;
    bounds(net, center, radius);

    // 基准投影（zoom=1，无缩放）：把 3D 节点投影进 cols×rows 屏幕格。真正的缩放/平移交给
    // interaction::Viewport——它在屏幕格空间里维护"可见窗口"，自带夹取（不会飞出屏外 / 平移
    // 越界），zoom_at 的锚点保证缩放中心不动。
    const math3d::Camera cam = chart3d::make_camera(
        static_cast<double>(cols), static_cast<double>(rows),
        35.0, 22.0, 55.0, center, radius);

    // 基准屏幕坐标 p 经 Viewport 映射到最终屏幕坐标（reset 时恒等，缩放时放大/平移）。
    const auto map_pt = [&](const math3d::Projected& p) -> Point {
        return Point{
            vp.normalized_x(p.x) * static_cast<double>(cols),
            vp.normalized_y(p.y) * static_cast<double>(rows),
        };
    };

    // 边：StrokeOp + MoveTo + LineTo。
    for (const auto& e : net.edges) {
        const auto pa = cam.project(net.nodes[static_cast<std::size_t>(e.from)].pos,
                                    static_cast<double>(cols), static_cast<double>(rows));
        const auto pb = cam.project(net.nodes[static_cast<std::size_t>(e.to)].pos,
                                    static_cast<double>(cols), static_cast<double>(rows));
        if (!pa.visible || !pb.visible) continue;
        const Point a = map_pt(pa);
        const Point b = map_pt(pb);
        const Color c = weather_color(e.weather);
        const double w = flow_width(e.flow);
        if (faux_width) {
            // TODO: 缺少可变线宽 API（终端 Canvas 仅 1 格宽，StrokeOp.width 被忽略）
            // —— 用平行偏移的多条线段占位，使 flow 越大连线越粗。
            const int k = static_cast<int>(std::ceil(w));
            const double dx = b.x - a.x, dy = b.y - a.y;
            const double len = std::hypot(dx, dy) < 1e-6 ? 1.0 : std::hypot(dx, dy);
            const double ox = -dy / len, oy = dx / len;  // 单位法向
            for (int t = 0; t < k; ++t) {
                const double off = t - (k - 1) / 2.0;
                s.push(scene::StrokeOp{c, 1.0});
                s.push(scene::MoveTo{Point{a.x + ox * off, a.y + oy * off}});
                s.push(scene::LineTo{Point{b.x + ox * off, b.y + oy * off}});
            }
        } else {
            // GUI 路径：gui::SceneRenderer 真正尊重 StrokeOp.width（场景单位，随 autofit 缩放）。
            s.push(scene::StrokeOp{c, w});
            s.push(scene::MoveTo{Point{a.x, a.y}});
            s.push(scene::LineTo{Point{b.x, b.y}});
        }
    }

    // 节点：FillOp + RectOp 实心小方块（深度着色）+ 标签。
    for (const auto& nd : net.nodes) {
        const auto p = cam.project(nd.pos, static_cast<double>(cols), static_cast<double>(rows));
        if (!p.visible) continue;
        const Point q = map_pt(p);
        s.push(scene::FillOp{chart3d::shade(p.depth)});
        s.push(scene::RectOp{Rect{q.x - 0.45, q.y - 0.45, 0.9, 0.9}});
        s.push(scene::TextOp{Point{q.x + 0.6, q.y - 0.3}, nd.id, Style{}});
    }
    return s;
}

// ── TUI：Scene → Canvas → CellBuffer → 差分重绘 ───────────────────────────
void render_tui_frame(CellBuffer& buf, const Network& net, const interaction::Viewport& vp,
                      const std::string& last_key) {
    const int cols = static_cast<int>(buf.width());
    const int rows = static_cast<int>(buf.height());
    canvas::Canvas canvas(static_cast<std::uint32_t>(cols),
                          static_cast<std::uint32_t>(rows));

    // L12 生成 + L10 光栅化（braille/halfblock）。
    const scene::Scene s = build_scene(net, cols, rows, vp, /*faux_width=*/true);
    canvas::render_scene(s, canvas);

    // 右上角伪按钮 [+] [-]（固定位置）。
    const Style btn = Style{}.set_fg(Color::rgb(255, 220, 120)).set_attr(Attr::Bold);
    const std::string btns = "[+]  [-]";
    const int bx = std::max(0, cols - static_cast<int>(btns.size()) - 1);
    canvas.draw_text(Point{static_cast<double>(bx), 0.0}, btns, btn);

    // 底部状态行（含调试用的 last key 回显，避免写实时终端导致花屏）。
    char status[192];
    std::snprintf(status, sizeof(status),
                  "scale=%.2f  nodes=%zu  edges=%zu   last=%s   [+/- zoom][r reset][q quit]",
                  vp.scale_x(), net.nodes.size(), net.edges.size(), last_key.c_str());
    canvas.draw_text(Point{1.0, static_cast<double>(rows - 1)}, status,
                     Style{}.set_fg(Color::rgb(180, 200, 220)));

    // 把 Canvas 拷回持久 CellBuffer（维持 Renderer 的 prev 状态以实现差分）。
    const CellBuffer tmp = canvas.to_buffer();
    auto dst = buf.cells();
    const auto src = tmp.cells();
    if (dst.size() == src.size()) {
        std::copy(src.begin(), src.end(), dst.begin());
    }
}

// 非阻塞读一个键。Windows 直接读控制台输入句柄（与 term::Terminal::enter() 的 raw 模式
// 同一 HANDLE）：WaitForSingleObject(h,0) 探活 + ReadFile —— 不依赖 conio.h，且真非阻塞
// （Windows 控制台句柄不支持 O_NONBLOCK，裸 ReadFile/_read 无输入时会阻塞等键，那正是
// 当初 fread 卡死的根因）。其它平台用 fcntl 把 stdin 设为 O_NONBLOCK 单字节读。
#ifdef _WIN32
static bool read_key(std::string& out) {
    const HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (WaitForSingleObject(h, 0) != WAIT_OBJECT_0) return false;  // 无输入 → 立即返回
    unsigned char c = 0;
    DWORD n = 0;
    if (!ReadFile(h, &c, 1, &n, nullptr) || n != 1) return false;
    if (c == 0x00 || c == 0xE0) {  // 方向/功能键：丢弃紧随的扫描码字节，本次不产出
        if (WaitForSingleObject(h, 0) == WAIT_OBJECT_0) {
            unsigned char d = 0; DWORD m = 0; ReadFile(h, &d, 1, &m, nullptr);
        }
        return false;
    }
    out.assign(1, static_cast<char>(c));
    return true;
}
#else
static int g_stdin_flags = -1;
static bool read_key(std::string& out) {
    if (g_stdin_flags < 0) {
        g_stdin_flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
        ::fcntl(STDIN_FILENO, F_SETFL, g_stdin_flags | O_NONBLOCK);
    }
    char c = 0;
    const ssize_t n = ::read(STDIN_FILENO, &c, 1);
    if (n <= 0) return false;  // EAGAIN / EOF / 出错
    out.assign(1, c);
    return true;
}
#endif

// 跨平台：把 Ctrl-C / SIGINT 转成"优雅退出"标志，确保循环结束后 term 的 RAII 析构能恢复
// 终端（光标 / alt-screen / raw 模式）。否则某些终端下 Ctrl-C 直接杀进程会残留 raw 模式。
static std::atomic<bool> g_interrupt{false};
#ifdef _WIN32
static BOOL WINAPI ctrl_handler(DWORD) { g_interrupt.store(true); return TRUE; }
#else
static void sigint_handler(int) { g_interrupt.store(true); }
#endif

int run_tui() {
    term::Terminal term{term::TerminalOptions{
        .raw_mode = true, .alt_screen = true, .hide_cursor = true}};
    if (term.enter() != ErrorCode::Ok) {
        std::fprintf(stderr, "cannot enter terminal raw/alt-screen mode (not a tty?)\n");
        return 1;
    }
    const term::TerminalSize size = term.size();
    const Network net = make_network();
    CellBuffer buf(size.cols, size.rows);
    render::Renderer renderer;  // 内部持有 prev，render() 只发变化格

    // 屏幕格空间里的视口：data_bounds = 整屏；reset=全览，缩放受 min_span 夹取（不会飞出屏外）。
    interaction::Viewport vp{interaction::ViewportOptions{
        .data_bounds = Rect{0.0, 0.0, static_cast<double>(size.cols),
                            static_cast<double>(size.rows)},
        .min_span = 4.0}};
    const Point anchor{size.cols / 2.0, size.rows / 2.0};  // 以屏幕中心为缩放锚点

#ifdef _WIN32
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
    struct sigaction sa {};
    sa.sa_handler = sigint_handler;
    ::sigaction(SIGINT, &sa, nullptr);
#endif

    std::string last_key = "(none)";
    const auto redraw = [&]() {
        render_tui_frame(buf, net, vp, last_key);
        term.write(renderer.render(buf));  // 首帧自动全量，之后仅变化格（绝不 cls）
        term.flush();
    };
    redraw();

    interaction::InputParser parser;
    bool running = true;
    while (running && !g_interrupt.load()) {
        std::string key;
        if (read_key(key)) {
            for (const auto& ev : parser.feed(key)) {
                if (const auto* k = std::get_if<interaction::KeyEvent>(&ev)) {
                    // 调试信息：把命中键值回显到状态行（不写实时终端，避免花屏）。
                    char dbg[32];
                    std::snprintf(dbg, sizeof(dbg), "U+%04X",
                                  static_cast<unsigned>(k->ch));
                    last_key = dbg;
                    if (k->key == interaction::Key::Escape || k->ch == U'q' ||
                        k->ch == U'Q') {
                        running = false;
                    } else if (k->ch == U'+') {
                        vp.zoom_at(1.25, anchor); redraw();
                    } else if (k->ch == U'-') {
                        vp.zoom_at(0.8, anchor); redraw();
                    } else if (k->ch == U'r' || k->ch == U'R') {
                        vp.reset(); redraw();
                    }
                } else if (const auto* r = std::get_if<interaction::ResizeEvent>(&ev)) {
                    buf.resize(static_cast<std::uint32_t>(r->cols),
                               static_cast<std::uint32_t>(r->rows));
                    // 终端尺寸变了：重建视口（data_bounds 跟随新尺寸）。
                    vp = interaction::Viewport{interaction::ViewportOptions{
                        .data_bounds = Rect{0.0, 0.0, static_cast<double>(r->cols),
                                            static_cast<double>(r->rows)},
                        .min_span = 4.0}};
                    renderer.reset();  // 尺寸变 → 下一帧强制全量
                    redraw();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // term 析构（RAII）自动 leave：恢复光标 / alt-screen / raw 模式，终端状态零残留。
    std::fprintf(stderr, "[tui] exited | last_key=%s scale=%.2f\n",
                 last_key.c_str(), vp.scale_x());
    return 0;
}

// ── GUI：termchart_gui widget_tree（VBox: 顶部 HBox 按钮 + 下方画布）────────
namespace gui_demo {

using namespace termchart_gui;

// 自定义画布控件：paint() 里复用与 TUI 同一份 build_scene，经 gui::SceneRenderer
// 光栅成 Bitmap 再 blit 上屏（GUI 端真正尊重线宽）。缩放走 interaction::Viewport。
class NetworkCanvas : public Widget {
public:
    static constexpr int kCellsX = 200, kCellsY = 90;  // 离屏「单元格」视口（与 TUI 同口径）

    NetworkCanvas(std::string id, const Network& net)
        : Widget(std::move(id)), net_(&net),
          viewport_(Rect{0.0, 0.0, static_cast<double>(kCellsX),
                         static_cast<double>(kCellsY)},
                    /*min_span=*/4.0) {}

    interaction::Viewport& viewport() noexcept { return viewport_; }
    void set_faux(bool f) noexcept { faux_ = f; }
    bool faux() const noexcept { return faux_; }

    void paint(TkCanvas& c) const override {
        const Rect g = global_bounds();
        c.clear(Rgb{16, 18, 24});
        const int w = static_cast<int>(g.w), h = static_cast<int>(g.h);
        if (w <= 0 || h <= 0) return;
        // 用 interaction::Viewport 做缩放（reset 即全览）；SceneRenderer 的 autofit 再把
        // 单元坐标缩放到 w×h 像素位图。喂像素坐标会让 autofit 把图缩成无穷小→空白。
        const scene::Scene s = build_scene(*net_, kCellsX, kCellsY, viewport_, faux_);
        termchart::gui::SceneRenderer sr{termchart::gui::RenderOptions{
            .width = static_cast<std::uint32_t>(w),
            .height = static_cast<std::uint32_t>(h),
            .autofit = true,
            .background = Rgb{16, 18, 24},
        }};
        const termchart::image::Bitmap bmp = sr.render(s);
        c.blit(bmp, g.x, g.y);  // TODO: 缺箭头 API —— 方向仅由连线顺序表达
        c.draw_text(g.x + 6.0, g.y + 6.0,
                    "scale=" + std::to_string(viewport_.scale_x()),
                    Rgb{220, 220, 226});
    }

private:
    const Network* net_ = nullptr;
    bool faux_ = false;
    interaction::Viewport viewport_;  // data_bounds = 单元格视口；reset=全览
};

}  // namespace gui_demo

int run_gui(int argc, char** argv) {
    using namespace termchart_gui;
    const int W = 480, H = 320;

    // 后端选择：启用 Win32 插件 → 真窗口（鼠标点击）；否则零依赖 TuiBackend（终端 halfblock）。
#ifdef TERMCHART_BUILD_GUI_WIN32
    termchart_gui_win32::Win32Backend backend(W, H, "spatiotemporal network");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32 backend unavailable\n");
        return 2;
    }
#else
    TuiBackend backend(W, H, termchart::ColorDepth::TrueColor);
#endif

    const Network net = make_network();

    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});

    // 顶部工具栏：固定高度（vpolicy=Fixed），不再是 50/50 瓜分；下方画布 Fill 占满剩余空间。
    auto& toolbar = root.emplace_child<Container>("toolbar");
    toolbar.set_vpolicy(SizePolicy::Fixed);
    toolbar.set_size(static_cast<double>(W), 36.0);
    auto fl = std::make_unique<FlexLayout>();
    fl->direction = FlexDirection::Row;
    fl->gap = 6.0;
    fl->cross_align = FlexAlign::Stretch;
    toolbar.set_layout(std::move(fl));
    auto& plus = toolbar.emplace_child<Button>("plus", "+");
    auto& minus = toolbar.emplace_child<Button>("minus", "-");

    // 下方画布区（自定义控件，默认 vpolicy=Fill → 占满工具栏以外的全部空间）。
    auto& canvas = root.emplace_child<gui_demo::NetworkCanvas>("canvas", net);

    Mount mount(root, backend);
    // 点击按钮 → 走 interaction::Viewport 缩放（锚点=画布中心，自带夹取不会飞出）。
    const auto db = canvas.viewport().data_bounds();
    const Point center{db.x + db.w / 2.0, db.y + db.h / 2.0};
    plus.set_on_click([&] { canvas.viewport().zoom_at(1.1, center); mount.paint_all(); });
    minus.set_on_click([&] { canvas.viewport().zoom_at(1.0 / 1.1, center); mount.paint_all(); });

    mount.paint_all();  // 首帧

#ifdef TERMCHART_BUILD_GUI_WIN32
    // 真窗口：OS 消息泵驱动事件循环（ESC / 关闭按钮退出）。
    auto drain = [&](bool& running) -> bool {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) {
                running = false;
                return dirty;
            }
            (void)mount.dispatch_event(e);  // 命中按钮 → on_click → update_zoom → 重绘
            dirty = true;
        }
        return dirty;
    };
    bool running = true;
    while (running) {
        if (drain(running)) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
#else
    // TuiBackend：刷到终端（看得见）。演示一次合成点击 + 可选导出 halfblock 文本。
    backend.present();
    const Event click = tk_event_map::mouse_press(20.0, 10.0);  // 命中 plus 按钮
    if (mount.dispatch_event(click)) mount.paint_all();
    if (argc > 1) {
        const auto bmp = backend.canvas().snapshot();
        const std::string frame =
            bitmap_to_halfblock(bmp, termchart::ColorDepth::TrueColor);
        std::fprintf(stdout, "%s", frame.c_str());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
    return 0;
}

// 离屏 PNG 导出：用 HeadlessBackend 走与 GUI 同款离屏光栅路径，渲染 TUI 同款
// “平行偏移线”场景（faux_width=true），肉眼查看 flow→变线宽（平行线间距）效果。
int export_png(const std::string& path) {
    using namespace termchart_gui;
    const Network net = make_network();
    constexpr int W = 1400, H = 630;
    HeadlessBackend backend(W, H);
    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});
    auto& canvas = root.emplace_child<gui_demo::NetworkCanvas>("canvas", net);
    canvas.set_faux(true);  // 平行线模拟变线宽（TUI 效果）
    canvas.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});
    Mount mount(root, backend);
    mount.paint_all();  // 渲染进无头画布
    const termchart::image::Bitmap bmp = backend.canvas().snapshot();
    const std::string png = termchart::image::encode_png(bmp);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "cannot open %s for writing\n", path.c_str());
        return 1;
    }
    f.write(png.data(), static_cast<std::streamsize>(png.size()));
    std::fprintf(stdout, "wrote %s (%d x %d, %zu bytes)\n", path.c_str(),
                 bmp.width(), bmp.height(), png.size());
    return 0;
}

}  // namespace demo

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--gui") == 0) return demo::run_gui(argc, argv);
    if (argc > 2 && std::strcmp(argv[1], "--png") == 0) return demo::export_png(argv[2]);
    return demo::run_tui();
}
