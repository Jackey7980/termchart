// widget_tree/candlestick_widget.hpp —— K 线（蜡烛图）控件（MSN 财经风格）
//
// 复刻金融 K 线：每根蜡烛 = 影线（high→low 细竖线）+ 实体（open↔close 方块）。
// 布局/配色参考 MSN 财经"恒生指数"页：**浅色底 + 绿涨红跌**（涨=绿、跌=红，MSN 西方惯例）；
// 顶部**最新价大字 + 涨跌额/涨跌幅（带色）**，中间 K 线，底部**统计条（前收/高/低）**。
// 数据 → Scene → SceneRenderer 光栅化缓存 → blit 上屏（与 ChartWidget 同套路）；
// 悬停命中某根蜡烛 → 画 OHLC 信息框。alpha/渐变不做（Scene IR 不带，见 chart_widget）。
#ifndef TERMCHART_GUI_WIDGET_TREE_CANDLESTICK_WIDGET_HPP
#define TERMCHART_GUI_WIDGET_TREE_CANDLESTICK_WIDGET_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart/gui/scene_renderer.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/scene/scene.hpp"
#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/widget.hpp"

namespace termchart_gui {

class CandlestickWidget : public Widget {
public:
    using Widget::Widget;  // 继承 (id, style) 构造

    struct Candle {
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
    };

    // 设入数据：labels 为每根蜡烛的标签（如日期 "2026-09-11"），candles 为 OHLC。
    void set_data(std::vector<std::string> labels, std::vector<Candle> candles) {
        labels_ = std::move(labels);
        candles_ = std::move(candles);
        dirty_ = true;
    }

    [[nodiscard]] int hovered_index() const noexcept { return hovered_; }

    void paint(TkCanvas& c) const override {
        const Rect g = global_bounds();
        const int W = static_cast<int>(g.w);
        const int H = static_cast<int>(g.h);
        const int N = static_cast<int>(candles_.size());
        if (W < 48 || H < 32 || N == 0 ||
            labels_.size() != candles_.size()) {  // 防呆
            return;
        }
        if (dirty_ || !base_ || base_w_ != W || base_h_ != H) {
            base_ = std::make_unique<Bitmap>(build_scene(W, H));
            base_w_ = W;
            base_h_ = H;
            dirty_ = false;
        }
        c.blit(*base_, g.x, g.y);
        if (hovered_ >= 0 && hovered_ < N) {
            draw_tooltip(c, g);
        }
    }

    bool on_event(const Event& e) override {
        if (e.type != EventType::MouseMove) {
            return false;
        }
        const Rect g = global_bounds();
        const int h = hit_index(e.x - g.x, e.y - g.y);
        if (h != hovered_) {
            hovered_ = h;
            return true;  // 悬停目标变化 → 请求重绘
        }
        return false;
    }

private:
    // ── 数字格式化（MSN 风格：千分位 + 2 位小数）──
    [[nodiscard]] static std::string thousands(long long v) {
        std::string digits = std::to_string(v < 0 ? -v : v);
        std::string out;
        int cnt = 0;
        for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
            out.push_back(*it);
            if (++cnt % 3 == 0 && (it + 1) != digits.rend()) {
                out.push_back(',');
            }
        }
        std::reverse(out.begin(), out.end());
        return out;
    }
    [[nodiscard]] static std::string fmt_price(double v) {
        const long long cents = static_cast<long long>(std::lround(v * 100.0));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s.%02lld", thousands(cents / 100).c_str(),
                      cents < 0 ? -(cents % 100) : (cents % 100));
        return std::string(buf);
    }
    [[nodiscard]] static std::string fmt_chg(double v) {  // 带符号 2 位小数
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%+.2f", v);
        return std::string(buf);
    }
    [[nodiscard]] static std::string fmt_pct(double v) {  // 带符号百分比
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%+.2f%%", v);
        return std::string(buf);
    }
    // 取"好看"的刻度步长：1/2/2.5/5/10 ×10^k（圆整价格轴）。
    [[nodiscard]] static double nice_step(double rough) {
        if (!(rough > 0.0)) {
            return 1.0;
        }
        const double mag = std::pow(10.0, std::floor(std::log10(rough)));
        const double n = rough / mag;
        double m = 10.0;
        if (n <= 1.0) m = 1.0;
        else if (n <= 2.0) m = 2.0;
        else if (n <= 2.5) m = 2.5;
        else if (n <= 5.0) m = 5.0;
        return m * mag;
    }
    [[nodiscard]] static std::string fmt_int(double v) {
        return std::to_string(static_cast<int>(std::lround(v)));
    }
    [[nodiscard]] static Bitmap raster(const termchart::scene::Scene& sc, int W, int H) {
        termchart::gui::RenderOptions o;
        o.width = static_cast<std::uint32_t>(W);
        o.height = static_cast<std::uint32_t>(H);
        o.autofit = false;
        o.scale = 1.0;
        o.supersample = 2;
        o.background = Rgb{247, 248, 250};  // MSN 浅色底
        o.foreground = Rgb{40, 44, 52};
        o.text_cell_w = 9.0;
        o.text_cell_h = 14.0;
        o.transparent = false;
        termchart::gui::SceneRenderer renderer(o);
        return renderer.render(sc);
    }
    [[nodiscard]] static std::string short_date(const std::string& s) {
        return s.size() >= 5 ? s.substr(s.size() - 5) : s;  // "2026-09-11" → "09-11"
    }

    // MSN 配色（浅色主题）。
    static constexpr Rgb kBg{247, 248, 250};
    static constexpr Rgb kInk{17, 24, 39};      // 标题/价格（近黑）
    static constexpr Rgb kMuted{107, 114, 128};  // 次要文字（灰）
    static constexpr Rgb kUp{22, 163, 74};       // 涨 = 绿（MSN）
    static constexpr Rgb kDown{220, 50, 50};     // 跌 = 红（MSN）

    // 建整幅 K 线 Scene 并光栅化（回填 plot 矩形/价格范围供命中/信息框用）。
    [[nodiscard]] Bitmap build_scene(int W, int H) const {
        const int N = static_cast<int>(candles_.size());
        double lo = candles_[0].low;
        double hi = candles_[0].high;
        for (const Candle& cd : candles_) {
            lo = std::min(lo, cd.low);
            hi = std::max(hi, cd.high);
        }
        double pad = (hi - lo) * 0.06;
        if (!(pad > 0.0)) {
            pad = std::max(1.0, std::fabs(hi) * 0.01);
        }
        const double step = nice_step((hi - lo + 2.0 * pad) / 4.0);
        const double y0 = std::floor((lo - pad) / step) * step;
        const double y1 = std::ceil((hi + pad) / step) * step;

        pl_ = 58.0;
        pr_ = static_cast<double>(W) - 14.0;
        pt_ = 50.0;
        pb_ = static_cast<double>(H) - 34.0;
        ymin_ = y0;
        ymax_ = (y1 > y0) ? y1 : y0 + step;

        const auto to_y = [&](double v) {
            return pb_ - (pb_ - pt_) * (v - ymin_) / (ymax_ - ymin_);
        };
        const auto cx = [&](int i) {
            return pl_ + (pr_ - pl_) * (static_cast<double>(i) + 0.5) / static_cast<double>(N);
        };
        const auto col = [](Rgb c) { return termchart::Color::rgb(c.r, c.g, c.b); };

        termchart::scene::Scene sc;
        termchart::scene::SceneBuilder b(sc);
        Style ink;    // 标题/价格
        Style muted;  // 次要
        ink.set_fg(col(kInk));
        muted.set_fg(col(kMuted));

        // ① 顶部价格头（MSN）：标题 + 日期范围；最新价 + 涨跌额/幅（带色）。
        const Candle& last = candles_[static_cast<std::size_t>(N - 1)];
        const Candle& prev = candles_[static_cast<std::size_t>(N - 2)];
        const double chg = last.close - prev.close;
        const double pct = (prev.close != 0.0) ? chg / prev.close * 100.0 : 0.0;
        const Rgb chg_col = (chg >= 0.0) ? kUp : kDown;
        b.text(Point{pl_ - 4.0, 8.0}, "Hang Seng Index (HSI)", ink);
        {
            const std::string rng =
                short_date(labels_.front()) + " ~ " + short_date(labels_.back());
            const double rw = static_cast<double>(rng.size()) * 9.0;
            b.text(Point{pr_ - rw, 8.0}, rng, muted);
        }
        const std::string price = fmt_price(last.close);
        b.text(Point{pl_ - 4.0, 26.0}, price, ink);  // 最新价（大字位）
        {
            const std::string chgs = fmt_chg(chg) + "  " + fmt_pct(pct);
            const double px = pl_ - 4.0 + static_cast<double>(price.size()) * 9.0 + 12.0;
            Style chgstyle;
            chgstyle.set_fg(col(chg_col));
            b.text(Point{px, 26.0}, chgs, chgstyle);
        }

        // ② 横向网格 + 价格刻度（圆整）。
        b.stroke(termchart::Color::rgb(228, 231, 236), 1.0);
        for (double v = y0; v <= y1 + 1e-6; v += step) {
            const double y = to_y(v);
            b.move_to(pl_, y);
            b.line_to(pr_, y);
            const std::string lab = fmt_int(v);
            b.text(Point{pl_ - 8.0 - static_cast<double>(lab.size()) * 9.0, y - 7.0}, lab,
                   muted);
        }
        b.stroke(termchart::Color::rgb(208, 213, 221), 1.0);
        b.rect(Rect{pl_, pt_, pr_ - pl_, pb_ - pt_});

        // ③ 蜡烛：影线 + 实体，绿涨红跌（实心）。
        const double bw = (pr_ - pl_) / static_cast<double>(N) * 0.6;
        const double wick = std::max(1.0, bw * 0.12);
        for (int i = 0; i < N; ++i) {
            const Candle& cd = candles_[static_cast<std::size_t>(i)];
            const double x = cx(i);
            const double yh = to_y(cd.high);
            const double yl = to_y(cd.low);
            const double yo = to_y(cd.open);
            const double yc = to_y(cd.close);
            const bool up = cd.close >= cd.open;
            const Rgb cc = up ? kUp : kDown;
            b.fill(col(cc));
            b.rect(Rect{x - wick / 2.0, yh, wick, yl - yh});  // 影线
            const double top = std::min(yo, yc);
            const double bot = std::max(yo, yc);
            b.rect(Rect{x - bw / 2.0, top, bw, std::max(1.0, bot - top)});  // 实体（实心）
        }

        // ④ x 日期标签（按可用宽度抽稀）。
        const double slot = (pr_ - pl_) / static_cast<double>(N);
        const int lstep =
            std::max(1, static_cast<int>(std::ceil(44.0 / (slot > 1.0 ? slot : 1.0))));
        for (int i = 0; i < N; ++i) {
            if (i % lstep != 0 && i != N - 1) {
                continue;
            }
            const std::string lab = short_date(labels_[static_cast<std::size_t>(i)]);
            const double w = static_cast<double>(lab.size()) * 9.0;
            b.text(Point{cx(i) - w / 2.0, pb_ + 6.0}, lab, muted);
        }

        // ⑤ 底部统计条（MSN）：前收 / 当日高 / 当日低。
        {
            const std::string s = "Prev " + fmt_price(prev.close) + "    High " +
                                  fmt_price(last.high) + "    Low " + fmt_price(last.low);
            b.text(Point{pl_ - 4.0, pb_ + 20.0}, s, muted);
        }

        return raster(sc, W, H);
    }

    // 命中：鼠标落在某根蜡烛的 x 列（且在图内）→ 返回其下标，否则 -1。
    [[nodiscard]] int hit_index(double x, double y) const {
        const int N = static_cast<int>(candles_.size());
        if (N == 0 || x < pl_ || x > pr_ || y < pt_ || y > pb_) {
            return -1;
        }
        const double slot = (pr_ - pl_) / static_cast<double>(N);
        int i = static_cast<int>((x - pl_) / slot);
        if (i < 0) {
            i = 0;
        }
        if (i >= N) {
            i = N - 1;
        }
        return i;
    }

    void draw_tooltip(TkCanvas& c, const Rect& g) const {
        const std::size_t h = static_cast<std::size_t>(hovered_);  // 调用方保证 >=0
        const Candle& cd = candles_[h];
        const bool up = cd.close >= cd.open;
        const Rgb accent = up ? kUp : kDown;
        const double bw = 132.0;
        const double bh = 66.0;
        const double cxh = pl_ + (pr_ - pl_) * (static_cast<double>(h) + 0.5) /
                                      static_cast<double>(candles_.size());
        double tx = cxh + 12.0;
        if (tx + bw > static_cast<double>(base_w_)) {
            tx = cxh - bw - 12.0;  // 贴右边则翻到左侧
        }
        if (tx < 4.0) {
            tx = 4.0;
        }
        const double ty = std::max(4.0, pt_ + 4.0);

        c.fill_rect(Rect{tx + 2.0, ty + 2.0, bw, bh}, Rgb{214, 219, 226});       // 投影
        c.fill_rect(Rect{tx, ty, bw, bh}, Rgb{255, 255, 255});                   // 主体（白）
        c.stroke_rect(Rect{tx, ty, bw, bh}, Rgb{208, 213, 221}, 1.0);            // 描边
        c.fill_rect(Rect{tx, ty, 3.0, bh}, accent);                              // 左强调条
        c.draw_text(tx + 9.0, ty + 5.0, labels_[h], Rgb{17, 24, 39});            // 日期
        c.draw_text(tx + 9.0, ty + 22.0, "O " + fmt_price(cd.open), Rgb{80, 86, 96});
        c.draw_text(tx + 9.0, ty + 35.0, "H " + fmt_price(cd.high), accent);
        c.draw_text(tx + 66.0, ty + 22.0, "L " + fmt_price(cd.low), accent);
        c.draw_text(tx + 66.0, ty + 35.0, "C " + fmt_price(cd.close),
                    up ? Rgb{22, 120, 60} : Rgb{180, 40, 40});
        (void)g;
    }

    mutable std::vector<std::string> labels_;
    mutable std::vector<Candle> candles_;
    mutable std::unique_ptr<Bitmap> base_;
    mutable int base_w_ = 0;
    mutable int base_h_ = 0;
    mutable bool dirty_ = true;
    mutable int hovered_ = -1;
    mutable double pl_ = 0.0;  // plot 矩形 + 价格范围（build 回填，命中/信息框用）
    mutable double pt_ = 0.0;
    mutable double pr_ = 0.0;
    mutable double pb_ = 0.0;
    mutable double ymin_ = 0.0;
    mutable double ymax_ = 1.0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_CANDLESTICK_WIDGET_HPP
