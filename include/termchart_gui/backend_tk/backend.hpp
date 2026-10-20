// backend_tk/backend.hpp —— 后端抽象 + 无头参考实现
//
// Backend 是"窗口 + 画布 + 事件源 + 上屏"的总接口；真实 tk（Qt/Tk/Win32…）提供
// 具体实现。这里同时给出一个 headless 参考实现（BitmapCanvas + HeadlessBackend），
// 用 image::Bitmap 当画布、内部队列当事件源，使整条平行栈能在无窗口系统下编译与
// 测试；将来接真实 tk 时按需替换，widget_tree / layout / native_loop 不动。
#ifndef TERMCHART_GUI_BACKEND_TK_BACKEND_HPP
#define TERMCHART_GUI_BACKEND_TK_BACKEND_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "termchart/gui/font.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/event.hpp"
#include "termchart_gui/native_loop/loop.hpp"

namespace termchart_gui {

class Backend {
public:
    virtual ~Backend() = default;

    [[nodiscard]] virtual std::unique_ptr<TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) = 0;
    [[nodiscard]] virtual TkCanvas& canvas() = 0;
    // 取一个原生事件（映射到 Event）；无事件返回 false。
    [[nodiscard]] virtual bool poll_event(Event& out) = 0;
    virtual void present() = 0;  // 上屏
    virtual void run_loop(EventLoop& loop) = 0;

    // 无头用：注入事件（真实后端由 OS 消息泵产生事件）。
    virtual void inject_event(Event e) { (void)e; }
};

// ── 无头参考实现 ──────────────────────────────────────────────────────────────

class BitmapCanvas final : public TkCanvas {
public:
    explicit BitmapCanvas(std::uint32_t w, std::uint32_t h) : bmp_(w, h, 4) {}

    void clear(Rgb bg) override { bmp_.fill(bg.r, bg.g, bg.b, 255); }

    void fill_rect(const Rect& r, Rgb color) override {
        const int ix = floor_to_int(r.x);
        const int iy = floor_to_int(r.y);
        const int iw = ceil_to_int(r.x + r.w) - ix;
        const int ih = ceil_to_int(r.y + r.h) - iy;
        if (iw <= 0 || ih <= 0) {
            return;
        }
        bmp_.fill_rect(static_cast<std::uint32_t>(ix), static_cast<std::uint32_t>(iy),
                       static_cast<std::uint32_t>(iw), static_cast<std::uint32_t>(ih),
                       color.r, color.g, color.b, 255);
    }

    void stroke_rect(const Rect& r, Rgb color, double width) override {
        const int wd = width < 1.0 ? 1 : static_cast<int>(width);
        const double dwd = static_cast<double>(wd);
        fill_rect(Rect{r.x, r.y, r.w, dwd}, color);
        fill_rect(Rect{r.x, r.y + r.h - dwd, r.w, dwd}, color);
        fill_rect(Rect{r.x, r.y, dwd, r.h}, color);
        fill_rect(Rect{r.x + r.w - dwd, r.y, dwd, r.h}, color);
    }

    void draw_text(double x, double y, std::string_view text, Rgb color) override {
        // 复用已落地的 GUI 位图字体（5×7 点阵，advance=cell_w=6，格高=8）：
        // 平行栈不重复造字体，直接调 termchart::gui 的文本出口。
        (void)termchart::gui::draw_text(bmp_, text, Point{x, y}, color, kCellW, kCellH);
    }

    // 文本度量：复用渲染核心的 measure_text——它和 draw_text 是同一把尺
    // （显示宽度口径，CJK = 2 列），不可在这里另起炉灶，否则排版与绘制会偏。
    [[nodiscard]] Point text_size(std::string_view text) const noexcept override {
        const termchart::gui::TextMetrics m =
            termchart::gui::measure_text(text, kCellW, kCellH);
        return Point{m.width, m.height};
    }

    // 文本格尺寸：与 termchart::gui 点阵字体的 advance/格高约定一致（5×7 字形
    // 画在前 5/6 宽、7/8 高，末列/末行留作字距），供控件排版居中/量宽。
    static constexpr double kCellW = 6.0;
    static constexpr double kCellH = 8.0;

    [[nodiscard]] Bitmap snapshot() const override { return bmp_; }

    // 只读引用内部位图（免 snapshot 的整图拷贝；原生后端上屏热路径用）。
    [[nodiscard]] const Bitmap& pixels() const noexcept { return bmp_; }

    // blit：把 src 以 (x,y) 为左上角 alpha 合成进 bmp_（越界裁剪）。opacity 整体缩放源 alpha。
    // blit：src 以 (x,y) 为左上角 alpha 合成进 bmp_（越界裁剪），opacity 缩放源 alpha。
    // 热路径全程走裸指针（data()），**不用**逐像素 at()/set()——那俩跨 TU + 边界检查，
    // 在 1080p 全屏每帧几百万次调用会把帧时间打到几百毫秒（实测过的坑）。
    void blit(const Bitmap& src, double x, double y, double opacity = 1.0) override {
        const int ox = floor_to_int(x);
        const int oy = floor_to_int(y);
        const int sw = static_cast<int>(src.width());
        const int sh = static_cast<int>(src.height());
        const int dw = static_cast<int>(bmp_.width());
        const int dh = static_cast<int>(bmp_.height());
        if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
            return;
        }
        const double op = opacity < 0.0 ? 0.0 : (opacity > 1.0 ? 1.0 : opacity);
        const std::uint8_t* sp = src.data();
        std::uint8_t* dp = bmp_.data();
        for (int sy = 0; sy < sh; ++sy) {
            const int py = oy + sy;
            if (py < 0 || py >= dh) {
                continue;
            }
            const std::uint8_t* srow =
                sp + static_cast<std::size_t>(sy) * static_cast<std::size_t>(sw) * 4;
            std::uint8_t* drow =
                dp + static_cast<std::size_t>(py) * static_cast<std::size_t>(dw) * 4;
            for (int sx = 0; sx < sw; ++sx) {
                const int px = ox + sx;
                if (px < 0 || px >= dw) {
                    continue;
                }
                const std::uint8_t* s = srow + static_cast<std::size_t>(sx) * 4;
                std::uint8_t* d = drow + static_cast<std::size_t>(px) * 4;
                const int a = static_cast<int>(static_cast<double>(s[3]) * op);
                if (a >= 255) {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                } else if (a > 0) {
                    d[0] = static_cast<std::uint8_t>((s[0] * a + d[0] * (255 - a)) / 255);
                    d[1] = static_cast<std::uint8_t>((s[1] * a + d[1] * (255 - a)) / 255);
                    d[2] = static_cast<std::uint8_t>((s[2] * a + d[2] * (255 - a)) / 255);
                }
            }
        }
    }

private:
    static int floor_to_int(double v) { return static_cast<int>(v); }  // 坐标已为像素
    static int ceil_to_int(double v) {
        const int i = static_cast<int>(v);
        return (v > static_cast<double>(i)) ? i + 1 : i;
    }
    Bitmap bmp_;
};

class HeadlessBackend final : public Backend {
public:
    explicit HeadlessBackend(std::uint32_t w, std::uint32_t h) : canvas_(w, h) {}

    [[nodiscard]] std::unique_ptr<TkWindow> create_window(std::uint32_t, std::uint32_t,
                                                         std::string) override {
        return nullptr;  // 无头不需要真实窗口
    }
    [[nodiscard]] TkCanvas& canvas() override { return canvas_; }
    [[nodiscard]] bool poll_event(Event& out) override {
        if (queue_.empty()) {
            return false;
        }
        out = queue_.front();
        queue_.pop_front();
        return true;
    }
    void present() override {}  // 无头无需上屏
    void run_loop(EventLoop& loop) override { loop.run(); }
    void inject_event(Event e) override { queue_.push_back(std::move(e)); }

private:
    BitmapCanvas canvas_;
    std::deque<Event> queue_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_BACKEND_HPP
