// panda_demo.cpp —— 用 termchart 的渲染管线画一只熊猫，显示在 Win32 窗口里
//
// 链路是库里真实的三段，示例内不重写任何光栅化：
//   Scene（矢量 IR）─SceneBuilder→ gui::SceneRenderer（超采样抗锯齿）→ Bitmap
//        ─TkCanvas::blit→ Win32 窗口
// Scene 只对矩形有填充语义，所以圆/椭圆/多边形都拆成相邻水平线段叠加：每行线宽
// 1 像素、行距 0.7 像素，叠加成实心区域，边缘交给 SceneRenderer 的超采样。
//
// 用法：
//   panda_demo                 开窗口（ESC / 关闭按钮退出）
//   panda_demo --png out.png   无头渲染一帧存 PNG（不需要窗口）
//   panda_demo --shot out.png  开窗口、首帧存快照后退出
//   panda_demo --size 900x900  初始窗口尺寸
//   panda_demo --ss 3          超采样倍数（1–4，默认 2）
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/widget.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

namespace {

using termchart::Color;
using termchart::Point;
using termchart::Rect;
using termchart::Rgb;
using termchart::image::Bitmap;
namespace sc = termchart::scene;

constexpr double kPi = 3.14159265358979323846;

Rgb mix(Rgb a, Rgb b, double t) {
    const auto f = [t](int x, int y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return Rgb{f(a.r, b.r), f(a.g, b.g), f(a.b, b.b)};
}

// 设计稿坐标系 1000×1000：按窗口短边等比缩放并居中，窗口怎么拉都不变形。
class Sketch {
public:
    Sketch(sc::Scene& scene, double width, double height)
        : b_(scene), width_(width), height_(height) {
        const double side = std::min(width, height);
        k_ = side / 1000.0;
        ox_ = (width - side) * 0.5;
        oy_ = (height - side) * 0.5;
    }

    void draw();

private:
    double X(double v) const { return ox_ + v * k_; }
    double Y(double v) const { return oy_ + v * k_; }

    void seg(double x0, double y0, double x1, double y1, double scene_width, Rgb c);
    void line(double x0, double y0, double x1, double y1, double design_width, Rgb c);
    void disc(double cx, double cy, double r, Rgb c);
    void ring(double cx, double cy, double r_out, double r_in, Rgb c);
    void ellipse(double cx, double cy, double rx, double ry, double deg, Rgb c);
    void poly(const std::vector<Point>& pts, Rgb c);
    void arc(double cx, double cy, double r, double a0, double a1, double design_width, Rgb c);
    void pole(double x, double top, double bottom, double w, Rgb c);
    void round_rect(double x0, double y0, double x1, double y1, double r, Rgb c);

    void backdrop();
    void moon_glow();
    void stars();
    void panel();
    void bamboo();
    void ground();
    void panda();
    void caption();

    sc::SceneBuilder b_;
    double width_ = 0.0;
    double height_ = 0.0;
    double k_ = 1.0;
    double ox_ = 0.0;
    double oy_ = 0.0;
};

void Sketch::seg(double x0, double y0, double x1, double y1, double scene_width, Rgb c) {
    b_.stroke(Color::rgb(c.r, c.g, c.b), scene_width);
    b_.move_to(x0, y0).line_to(x1, y1);
}

void Sketch::line(double x0, double y0, double x1, double y1, double design_width, Rgb c) {
    seg(X(x0), Y(y0), X(x1), Y(y1), design_width * k_, c);
}

void Sketch::disc(double cx, double cy, double r, Rgb c) { ellipse(cx, cy, r, r, 0.0, c); }

// 圆环：只画两条边，避免"每层同心圆都整块重涂"（光晕有 14 层，整块涂是 10 倍开销）。
void Sketch::ring(double cx, double cy, double r_out, double r_in, Rgb c) {
    const double RO = r_out * k_;
    const double RI = r_in * k_;
    if (RO < 0.5) return;
    const double gx = X(cx);
    const double gy = Y(cy);
    constexpr double kStep = 0.7;
    for (double t = -RO + kStep * 0.5; t <= RO - kStep * 0.5; t += kStep) {
        const double wo = std::sqrt(std::max(0.0, RO * RO - t * t));
        const double wi =
            std::abs(t) < RI ? std::sqrt(std::max(0.0, RI * RI - t * t)) : 0.0;
        if (wi <= 0.0) {
            seg(gx - wo, gy + t, gx + wo, gy + t, 1.0, c);
        } else {
            seg(gx - wo, gy + t, gx - wi, gy + t, 1.0, c);
            seg(gx + wi, gy + t, gx + wo, gy + t, 1.0, c);
        }
    }
}

void Sketch::ellipse(double cx, double cy, double rx, double ry, double deg, Rgb c) {
    const double RX = rx * k_;
    const double RY = ry * k_;
    if (RX < 0.4 || RY < 0.4) return;
    const double rad = deg * kPi / 180.0;
    const double ca = std::cos(rad);
    const double sa = std::sin(rad);
    const double gx = X(cx);
    const double gy = Y(cy);
    constexpr double kStep = 0.7;  // 行距；线宽 1 像素，相邻行重叠 0.3 像素
    for (double t = -RY + kStep * 0.5; t <= RY - kStep * 0.5; t += kStep) {
        const double u = t / RY;
        const double hw = RX * std::sqrt(std::max(0.0, 1.0 - u * u));
        seg(gx + hw * ca - t * sa, gy + hw * sa + t * ca,  //
            gx - hw * ca - t * sa, gy - hw * sa + t * ca,  //
            1.0, c);
    }
}

void Sketch::poly(const std::vector<Point>& pts, Rgb c) {
    if (pts.size() < 3) return;
    double lo = pts[0].y;
    double hi = pts[0].y;
    for (const Point& p : pts) {
        lo = std::min(lo, p.y);
        hi = std::max(hi, p.y);
    }
    const double step = 0.7 / k_;
    std::vector<double> xs;
    for (double y = lo; y <= hi; y += step) {
        xs.clear();
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const Point& a = pts[i];
            const Point& d = pts[(i + 1) % pts.size()];
            if ((a.y <= y && d.y > y) || (d.y <= y && a.y > y)) {
                xs.push_back(a.x + (y - a.y) * (d.x - a.x) / (d.y - a.y));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            line(xs[i], y, xs[i + 1], y, 0.95, c);
        }
    }
}

void Sketch::arc(double cx, double cy, double r, double a0, double a1, double design_width, Rgb c) {
    const int steps = std::max(8, static_cast<int>(90.0 * std::abs(a1 - a0) / kPi));
    double px = cx + r * std::cos(a0);
    double py = cy + r * std::sin(a0);
    for (int i = 1; i <= steps; ++i) {
        const double a = a0 + (a1 - a0) * static_cast<double>(i) / steps;
        const double nx = cx + r * std::cos(a);
        const double ny = cy + r * std::sin(a);
        line(px, py, nx, ny, design_width, c);
        px = nx;
        py = ny;
    }
}

void Sketch::pole(double x, double top, double bottom, double w, Rgb c) {
    b_.fill(Color::rgb(c.r, c.g, c.b));
    b_.rect(Rect{X(x), Y(top), w * k_, (bottom - top) * k_});
    disc(x + w * 0.5, top + w * 0.5, w * 0.5, c);
    disc(x + w * 0.5, bottom - w * 0.5, w * 0.5, c);
}

// 圆角矩形 = 中央矩形 + 四条边矩形 + 四角圆。面板/卡片都靠它。
void Sketch::round_rect(double x0, double y0, double x1, double y1, double r, Rgb c) {
    b_.fill(Color::rgb(c.r, c.g, c.b));
    b_.rect(Rect{X(x0 + r), Y(y0), (x1 - x0 - 2 * r) * k_, (y1 - y0) * k_});
    b_.rect(Rect{X(x0), Y(y0 + r), (x1 - x0) * k_, (y1 - y0 - 2 * r) * k_});
    disc(x0 + r, y0 + r, r, c);
    disc(x1 - r, y0 + r, r, c);
    disc(x0 + r, y1 - r, r, c);
    disc(x1 - r, y1 - r, r, c);
}

// ───────────────────────── 场景内容 ─────────────────────────

void Sketch::backdrop() {
    const Rgb top{18, 25, 39};
    const Rgb mid{11, 15, 24};
    const Rgb bottom{5, 7, 12};
    for (double y = 0.0; y < height_; y += 1.0) {
        const double t = y / std::max(1.0, height_);
        const Rgb c = t < 0.55 ? mix(top, mid, t / 0.55) : mix(mid, bottom, (t - 0.55) / 0.45);
        b_.fill(Color::rgb(c.r, c.g, c.b));
        b_.rect(Rect{0.0, y, width_, 1.0});
    }
}

void Sketch::moon_glow() {
    // 展板背后一圈冷光：同心环逐级提亮（没有 alpha，只能靠色阶堆）。
    constexpr int kRings = 12;
    double outer = 566.0;
    for (int i = 0; i < kRings; ++i) {
        const double t = static_cast<double>(i) / (kRings - 1);
        const double inner = 566.0 - 9.0 * (i + 1);
        ring(500.0, 500.0, outer, std::max(0.0, inner),
             mix(Rgb{16, 22, 35}, Rgb{34, 46, 70}, t * t));
        outer = inner;
    }
}

void Sketch::stars() {
    struct Star {
        double x, y, r;
        int lum;
    };
    static constexpr Star kStars[] = {
        {96, 92, 2.4, 190},   {172, 156, 1.7, 150}, {58, 238, 2.0, 170},
        {128, 330, 1.5, 130}, {206, 268, 1.8, 150}, {74, 430, 2.2, 165},
        {168, 486, 1.6, 140}, {916, 86, 2.5, 195},  {842, 148, 1.7, 150},
        {948, 246, 2.1, 172}, {866, 332, 1.5, 132}, {932, 430, 2.2, 168},
        {836, 468, 1.6, 138}, {300, 78, 1.6, 140},  {722, 66, 2.0, 160},
        {590, 96, 1.4, 125},  {410, 62, 1.8, 150},  {258, 148, 1.4, 120},
    };
    for (const Star& s : kStars) {
        const auto b = static_cast<std::uint8_t>(s.lum);
        disc(s.x, s.y, s.r, Rgb{b, static_cast<std::uint8_t>(std::min(255, s.lum + 8)),
                                static_cast<std::uint8_t>(std::min(255, s.lum + 30))});
    }
}

void Sketch::bamboo() {
    const Rgb stalk{74, 122, 78};
    const Rgb node{103, 152, 104};
    const Rgb leaf_dark{62, 106, 68};
    const Rgb leaf_lit{96, 142, 94};

    // 竹子长在浅色展板里，左右各一丛
    pole(178.0, 190.0, 916.0, 30.0, stalk);
    pole(806.0, 250.0, 916.0, 26.0, stalk);
    pole(838.0, 470.0, 916.0, 18.0, stalk);
    for (double y = 300.0; y < 916.0; y += 146.0) {
        b_.fill(Color::rgb(node.r, node.g, node.b));
        b_.rect(Rect{X(176.0), Y(y), 34.0 * k_, 6.0 * k_});
    }
    for (double y = 340.0; y < 916.0; y += 158.0) {
        b_.fill(Color::rgb(node.r, node.g, node.b));
        b_.rect(Rect{X(805.0), Y(y), 28.0 * k_, 5.0 * k_});
    }

    // 竹叶：细长椭圆按角度散开
    const auto leaf = [this](double x, double y, double len, double deg, Rgb c) {
        ellipse(x, y, len, 12.0, deg, c);
    };
    leaf(232.0, 208.0, 76.0, -28.0, leaf_lit);
    leaf(258.0, 258.0, 66.0, 30.0, leaf_dark);
    leaf(212.0, 300.0, 58.0, 64.0, leaf_lit);
    leaf(770.0, 268.0, 72.0, 30.0, leaf_lit);
    leaf(744.0, 320.0, 60.0, -26.0, leaf_dark);
    leaf(800.0, 352.0, 54.0, 56.0, leaf_dark);
}

void Sketch::panel() {
    round_rect(140.0, 100.0, 860.0, 920.0, 76.0, Rgb{206, 219, 237});
    round_rect(150.0, 110.0, 850.0, 910.0, 68.0, Rgb{224, 234, 246});
}

void Sketch::ground() { ellipse(500.0, 812.0, 214.0, 28.0, 0.0, Rgb{188, 202, 222}); }

void Sketch::panda() {
    const Rgb fur{253, 251, 246};
    const Rgb fur_body{245, 242, 234};
    const Rgb fur_dim{228, 224, 215};
    const Rgb ink{28, 28, 34};
    const Rgb blush{243, 186, 197};

    // 熊猫按 0.74 缩到画稿中心，四周给竹子留出呼吸位。
    constexpr double kS = 0.74;
    constexpr double kCx = 500.0;
    constexpr double kCy = 515.0;
    const auto PX = [](double x) { return kCx + (x - kCx) * kS; };
    const auto PY = [](double y) { return kCy + (y - kCy) * kS; };
    const auto E = [&](double x, double y, double rx, double ry, double deg, Rgb c) {
        ellipse(PX(x), PY(y), rx * kS, ry * kS, deg, c);
    };
    const auto D = [&](double x, double y, double r, Rgb c) { disc(PX(x), PY(y), r * kS, c); };
    const auto L = [&](double x0, double y0, double x1, double y1, double w, Rgb c) {
        line(PX(x0), PY(y0), PX(x1), PY(y1), w * kS, c);
    };
    const auto A = [&](double x, double y, double r, double a0, double a1, double w, Rgb c) {
        arc(PX(x), PY(y), r * kS, a0, a1, w * kS, c);
    };

    // 身体 + 肚子阴影（先画，腿和手臂再压上来才看得见）
    E(500.0, 692.0, 232.0, 200.0, 0.0, fur_body);
    E(500.0, 790.0, 146.0, 86.0, 0.0, fur_dim);

    // 后腿：坐姿，黑脚掌要越过身体下缘才读得出来
    E(408.0, 862.0, 86.0, 56.0, -8.0, ink);
    E(592.0, 862.0, 86.0, 56.0, 8.0, ink);
    E(408.0, 876.0, 40.0, 24.0, -8.0, fur_dim);
    E(592.0, 876.0, 40.0, 24.0, 8.0, fur_dim);

    // 手臂：贴着身体两侧，白肚子上的两道黑弧
    E(304.0, 694.0, 62.0, 128.0, 24.0, ink);
    E(696.0, 694.0, 62.0, 128.0, -24.0, ink);

    // 耳朵在头后面
    E(346.0, 206.0, 90.0, 84.0, -18.0, ink);
    E(654.0, 206.0, 90.0, 84.0, 18.0, ink);

    // 头
    D(500.0, 396.0, 228.0, fur);

    // 眼斑：外高内低的斜椭圆，熊猫的招牌
    E(426.0, 374.0, 58.0, 80.0, 20.0, ink);
    E(574.0, 374.0, 58.0, 80.0, -20.0, ink);

    // 眼白 / 瞳孔 / 高光
    D(418.0, 364.0, 19.0, fur);
    D(582.0, 364.0, 19.0, fur);
    D(420.0, 369.0, 10.0, Rgb{16, 16, 20});
    D(580.0, 369.0, 10.0, Rgb{16, 16, 20});
    D(413.0, 360.0, 3.4, Rgb{255, 255, 255});
    D(577.0, 360.0, 3.4, Rgb{255, 255, 255});

    // 鼻子：三角填充 + 同色粗描边把尖角磨圆
    poly({{PX(473.0), PY(458.0)}, {PX(527.0), PY(458.0)}, {PX(500.0), PY(500.0)}}, ink);
    L(473.0, 458.0, 527.0, 458.0, 17.0, ink);
    L(473.0, 458.0, 500.0, 500.0, 15.0, ink);
    L(527.0, 458.0, 500.0, 500.0, 15.0, ink);

    // 嘴：竖线 + 两个下弧（"w"）
    L(500.0, 498.0, 500.0, 520.0, 7.0, ink);
    A(476.0, 520.0, 25.0, 0.0, kPi, 7.0, ink);
    A(524.0, 520.0, 25.0, 0.0, kPi, 7.0, ink);

    // 腮红
    E(336.0, 458.0, 36.0, 21.0, -10.0, blush);
    E(664.0, 458.0, 36.0, 21.0, 10.0, blush);
}

void Sketch::caption() {
    termchart::Style style;
    style.set_fg(Color::rgb(88, 106, 136));
    b_.text(Point{24.0, height_ - 30.0}, "TERMCHART_GUI // PANDA", style);
    style.set_fg(Color::rgb(58, 72, 96));
    b_.text(Point{24.0, 20.0}, "ESC TO QUIT", style);
}

void Sketch::draw() {
    backdrop();
    stars();
    moon_glow();
    panel();
    bamboo();
    ground();
    panda();
    caption();
}

// 把一帧画成位图：Scene 是"画什么"，SceneRenderer 是"怎么落成像素"。
Bitmap render_frame(double width, double height, int supersample) {
    sc::Scene scene;
    Sketch sketch(scene, width, height);
    sketch.draw();

    termchart::gui::RenderOptions options;
    options.width = static_cast<std::uint32_t>(std::lround(width));
    options.height = static_cast<std::uint32_t>(std::lround(height));
    options.autofit = false;   // 坐标即像素
    options.scale = 1.0;
    options.supersample = supersample;
    options.transparent = false;
    options.background = Rgb{5, 7, 12};
    options.text_cell_w = 6.0;
    options.text_cell_h = 8.0;

    termchart::gui::SceneRenderer renderer(options);
    return renderer.render(scene);
}

bool save_png(const Bitmap& bitmap, const std::string& path) {
    const std::string bytes = termchart::image::encode_png(bitmap);
    if (bytes.empty()) return false;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

// 窗口里的熊猫：首次绘制/尺寸变化时整帧重渲染，其余帧直接 blit 缓存。
class PandaWidget final : public termchart_gui::Widget {
public:
    explicit PandaWidget(std::string id) : Widget(std::move(id)) {}

    void set_supersample(int ss) {
        if (ss != ss_) {
            ss_ = ss;
            ready_ = false;
        }
    }

    void paint(termchart_gui::TkCanvas& canvas) const override {
        const Rect g = global_bounds();
        if (g.w < 1.0 || g.h < 1.0) return;
        if (!ready_ || fw_ != g.w || fh_ != g.h) {
            frame_ = render_frame(g.w, g.h, ss_);
            fw_ = g.w;
            fh_ = g.h;
            ready_ = true;
        }
        canvas.blit(frame_, g.x, g.y);
    }

private:
    mutable Bitmap frame_{};
    mutable double fw_ = 0.0;
    mutable double fh_ = 0.0;
    mutable bool ready_ = false;
    int ss_ = 2;
};

struct Options {
    std::uint32_t width = 860;
    std::uint32_t height = 860;
    int supersample = 2;
    std::string png;
    std::string shot;
};

void usage() {
    std::fprintf(stderr,
                 "usage: panda_demo [--png out.png] [--shot out.png] [--size WxH] [--ss N]\n");
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: 缺少参数值\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--png") {
            opt.png = value();
        } else if (arg == "--shot") {
            opt.shot = value();
        } else if (arg == "--size") {
            const std::string v = value();
            unsigned w = 0;
            unsigned h = 0;
            if (std::sscanf(v.c_str(), "%ux%u", &w, &h) != 2 || w < 64 || h < 64) {
                std::fprintf(stderr, "--size 需要 WxH（>=64），收到 %s\n", v.c_str());
                return 2;
            }
            opt.width = w;
            opt.height = h;
        } else if (arg == "--ss") {
            opt.supersample = std::clamp(std::atoi(value().c_str()), 1, 4);
        } else if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else {
            std::fprintf(stderr, "未知参数 %s\n", arg.c_str());
            usage();
            return 2;
        }
    }

    // 无头：不需要窗口，直接把一帧存成 PNG（用于肉眼比对 / 回归）。
    if (!opt.png.empty()) {
        const auto t0 = std::chrono::steady_clock::now();
        const Bitmap frame = render_frame(opt.width, opt.height, opt.supersample);
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        if (!save_png(frame, opt.png)) {
            std::fprintf(stderr, "写 PNG 失败：%s\n", opt.png.c_str());
            return 1;
        }
        std::fprintf(stderr, "panda: %ux%u ss=%d, %.1f ms -> %s\n", opt.width, opt.height,
                     opt.supersample, ms, opt.png.c_str());
        return 0;
    }

    using termchart_gui::Event;
    using termchart_gui::EventType;

    termchart_gui_win32::Win32Backend backend(opt.width, opt.height,
                                             "termchart_gui - panda");
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）\n");
        return 2;
    }

    termchart_gui::Container root("root");
    root.set_bounds(Rect{0, 0, static_cast<double>(opt.width), static_cast<double>(opt.height)});

    auto& panda = root.emplace_child<PandaWidget>("panda");
    panda.set_supersample(opt.supersample);
    panda.set_bounds(root.bounds());

    termchart_gui::Mount mount(root, backend);
    mount.paint_all();

    // 拖动边框时系统进入模态 resize 循环，主循环被阻塞；这里就地重排+重画，
    // 保证窗口跟着鼠标变（熊猫按短边等比缩放，不会变形）。
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        const Rect r{0, 0, static_cast<double>(w), static_cast<double>(h)};
        root.set_bounds(r);
        panda.set_bounds(r);
        mount.paint_all();
    });

    if (!opt.shot.empty()) {
        if (!save_png(backend.canvas().snapshot(), opt.shot)) {
            std::fprintf(stderr, "写快照失败：%s\n", opt.shot.c_str());
            backend.request_close();
            return 1;
        }
        std::fprintf(stderr, "窗口首帧已存至 %s\n", opt.shot.c_str());
        backend.request_close();
        for (int i = 0; i < 50; ++i) {
            Event e;
            while (backend.poll_event(e)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        return 0;
    }

    bool running = true;
    while (running) {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            switch (e.type) {
                case EventType::Quit:
                    running = false;
                    break;
                case EventType::Resize: {
                    const Rect r{0, 0, static_cast<double>(e.width),
                                 static_cast<double>(e.height)};
                    root.set_bounds(r);
                    panda.set_bounds(r);
                    dirty = true;
                    break;
                }
                default:
                    dirty = mount.dispatch_event(e) || dirty;
                    break;
            }
        }
        if (dirty) mount.paint_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    backend.request_close();
    return 0;
}
