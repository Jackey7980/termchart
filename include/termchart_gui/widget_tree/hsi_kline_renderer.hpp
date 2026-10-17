// widget_tree/hsi_kline_renderer.hpp —— 恒生指数 K 线（蜡烛图）渲染（MSN 财经风格）
//
// "骨肉分离"后的"肉"之二：从原 CandlestickWidget 抽出的 HSI K 线绘制逻辑，作为可插拔
// 的 IChartRenderer。复用 ChartWidget 的动画/缓存/降分辨率/淡入/悬停十字轴——即把同一套
// "插座"同时驱动 combo 与 K 线。浅色底 + 绿涨红跌（MSN 西方惯例），顶部最新价大字 +
// 涨跌额/幅（带色），底部统计条（前收/高/低）。配置色/布局参考 MSN，不照搬其数据/图种。
#ifndef TERMCHART_GUI_WIDGET_TREE_HSI_KLINE_RENDERER_HPP
#define TERMCHART_GUI_WIDGET_TREE_HSI_KLINE_RENDERER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "termchart/core/style.hpp"
#include "termchart_gui/backend_tk/tk_canvas.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/i_chart_renderer.hpp"

namespace termchart_gui {

class HsiKLineRenderer : public IChartRenderer {
public:
    struct Candle {
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
    };

    // 设入数据：labels 为每根蜡烛标签（如日期 "2026-09-11"），candles 为 OHLC。
    void set_data(std::vector<std::string> labels, std::vector<Candle> candles) {
        labels_ = std::move(labels);
        candles_ = std::move(candles);
    }

    [[nodiscard]] Rgb background() const override { return Rgb{247, 248, 250}; }
    [[nodiscard]] Rgb foreground() const override { return Rgb{40, 44, 52}; }

    [[nodiscard]] bool empty() const override { return candles_.empty(); }

    // 由数据 + 控件尺寸算目标几何：价格范围（ymin/ymax）+ 蜡烛中心 x + plot 矩形。
    [[nodiscard]] ChartGeom compute_geom(int W, int H) const override {
        ChartGeom g;
        const int N = static_cast<int>(candles_.size());
        g.n = N;
        if (N == 0) return g;
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

        g.pl = 58.0;
        g.pt = 50.0;
        g.pr = static_cast<double>(W) - 14.0;
        g.pb = static_cast<double>(H) - 34.0;
        g.ytop = (y1 > y0) ? y1 : y0 + step;
        g.step = step;
        g.intervals = std::max(1, static_cast<int>(std::ceil((g.ytop - y0) / step)));

        g.xs.resize(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i) {
            g.xs[static_cast<std::size_t>(i)] =
                g.pl + (g.pr - g.pl) * (static_cast<double>(i) + 0.5) / static_cast<double>(N);
        }
        ymin_ = y0;  // 数据静态，恒定；供 draw_hover/draw_tooltip 做价格坐标换算
        return g;
    }

    // ① 顶部价格头（MSN）② 网格+刻度 ③ 蜡烛 ④ x 日期标签 ⑤ 底部统计条。
    void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g, double /*glow*/,
                          bool /*dragging*/) const override {
        const int N = g.n;
        if (N == 0) return;
        const auto to_y = [&](double v) {
            return g.pb - (g.pb - g.pt) * (v - ymin_) / (g.ytop - ymin_);
        };
        const auto cx = [&](int i) {
            return g.pl + (g.pr - g.pl) * (static_cast<double>(i) + 0.5) / static_cast<double>(N);
        };
        const auto col = [](Rgb c) { return termchart::Color::rgb(c.r, c.g, c.b); };

        Style ink;    // 标题/价格
        Style muted;  // 次要
        ink.set_fg(col(kInk));
        muted.set_fg(col(kMuted));

        // ① 顶部价格头：标题 + 日期范围；最新价 + 涨跌额/幅（带色）。
        const Candle& last = candles_[static_cast<std::size_t>(N - 1)];
        const Candle& prev = candles_[static_cast<std::size_t>(N - 2)];
        const double chg = last.close - prev.close;
        const double pct = (prev.close != 0.0) ? chg / prev.close * 100.0 : 0.0;
        const Rgb chg_col = (chg >= 0.0) ? kUp : kDown;
        b.text(Point{g.pl - 4.0, 8.0}, "Hang Seng Index (HSI)", ink);
        {
            const std::string rng =
                short_date(labels_.front()) + " ~ " + short_date(labels_.back());
            const double rw = static_cast<double>(rng.size()) * 9.0;
            b.text(Point{g.pr - rw, 8.0}, rng, muted);
        }
        const std::string price = fmt_price(last.close);
        b.text(Point{g.pl - 4.0, 26.0}, price, ink);  // 最新价（大字位）
        {
            const std::string chgs = fmt_chg(chg) + "  " + fmt_pct(pct);
            const double px = g.pl - 4.0 + static_cast<double>(price.size()) * 9.0 + 12.0;
            Style chgstyle;
            chgstyle.set_fg(col(chg_col));
            b.text(Point{px, 26.0}, chgs, chgstyle);
        }

        // ② 横向网格 + 价格刻度（圆整）。
        b.stroke(termchart::Color::rgb(228, 231, 236), 1.0);
        for (double v = ymin_; v <= g.ytop + 1e-6; v += g.step) {
            const double y = to_y(v);
            b.move_to(g.pl, y);
            b.line_to(g.pr, y);
            const std::string lab = fmt_int(v);
            b.text(Point{g.pl - 8.0 - static_cast<double>(lab.size()) * 9.0, y - 7.0}, lab,
                   muted);
        }
        b.stroke(termchart::Color::rgb(208, 213, 221), 1.0);
        b.rect(Rect{g.pl, g.pt, g.pr - g.pl, g.pb - g.pt});

        // ③ 蜡烛：影线 + 实体，绿涨红跌（实心）。
        const double bw = (g.pr - g.pl) / static_cast<double>(N) * 0.6;
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
        const double slot = (g.pr - g.pl) / static_cast<double>(N);
        const int lstep =
            std::max(1, static_cast<int>(std::ceil(44.0 / (slot > 1.0 ? slot : 1.0))));
        label_step_ = lstep;
        for (int i = 0; i < N; ++i) {
            if (i % lstep != 0 && i != N - 1) {
                continue;
            }
            const std::string lab = short_date(labels_[static_cast<std::size_t>(i)]);
            const double w = static_cast<double>(lab.size()) * 9.0;
            b.text(Point{cx(i) - w / 2.0, g.pb + 6.0}, lab, muted);
        }

        // ⑤ 底部统计条（MSN）：前收 / 当日高 / 当日低。
        {
            const std::string s = "Prev " + fmt_price(prev.close) + "    High " +
                                  fmt_price(last.high) + "    Low " + fmt_price(last.low);
            b.text(Point{g.pl - 4.0, g.pb + 20.0}, s, muted);
        }
    }

    // K 线无面积/折线层。
    void build_area_scene(termchart::scene::SceneBuilder& /*b*/, const ChartGeom& /*g*/,
                          double /*glow*/) const override {}
    void build_line_scene(termchart::scene::SceneBuilder& /*b*/, const ChartGeom& /*g*/,
                          double /*glow*/) const override {}

    // 命中：鼠标落在某根蜡烛的 x 列（且在图内）→ 返回其下标，否则 -1。
    [[nodiscard]] int hit_index(const ChartGeom& g, double lx, double ly) const override {
        const int N = g.n;
        if (N == 0 || lx < g.pl || lx > g.pr || ly < g.pt || ly > g.pb) return -1;
        const double slot = (g.pr - g.pl) / static_cast<double>(N);
        int i = static_cast<int>((lx - g.pl) / slot);
        if (i < 0) i = 0;
        if (i >= N) i = N - 1;
        return i;
    }

    [[nodiscard]] int label_step() const override { return label_step_; }

    // 悬停强调：高亮悬停蜡烛的外框（琥珀色）。
    void draw_hover(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered,
                   double /*glow*/) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        if (h >= candles_.size()) return;
        const double x = geom.xs[h];
        const double bw = (geom.pr - geom.pl) / static_cast<double>(geom.n) * 0.6;
        const double yh =
            geom.pb - (geom.pb - geom.pt) * (candles_[h].high - ymin_) / (geom.ytop - ymin_);
        const double yl =
            geom.pb - (geom.pb - geom.pt) * (candles_[h].low - ymin_) / (geom.ytop - ymin_);
        // 窗口画布未按控件平移 → K 线高亮框需加全局偏移（用户指出的"x 没+一个图表宽度"）。
        c.stroke_rect(Rect{g.x + x - bw / 2.0 - 2.0, g.y + yh - 2.0, bw + 4.0, (yl - yh) + 4.0},
                      Rgb{255, 179, 0}, 1.0);
    }

    void draw_tooltip(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        if (h >= candles_.size()) return;
        const Candle& cd = candles_[h];
        const bool up = cd.close >= cd.open;
        const Rgb accent = up ? kUp : kDown;
        const double bw = 132.0;
        const double bh = 66.0;
        const double cxh = geom.xs[h];
        double tx = g.x + cxh + 12.0;
        if (tx + bw > g.x + g.w) tx = g.x + cxh - bw - 12.0;  // 贴右边则翻到左侧
        if (tx < g.x + 4.0) tx = g.x + 4.0;
        const double ty = g.y + std::max(4.0, geom.pt + 4.0);

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
    }

private:
    static std::string thousands(long long v) {
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
    static std::string fmt_price(double v) {
        const long long cents = static_cast<long long>(std::lround(v * 100.0));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s.%02lld", thousands(cents / 100).c_str(),
                      cents < 0 ? -(cents % 100) : (cents % 100));
        return std::string(buf);
    }
    static std::string fmt_chg(double v) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%+.2f", v);
        return std::string(buf);
    }
    static std::string fmt_pct(double v) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%+.2f%%", v);
        return std::string(buf);
    }
    static std::string short_date(const std::string& s) {
        return s.size() >= 5 ? s.substr(s.size() - 5) : s;  // "2026-09-11" → "09-11"
    }

    // MSN 配色（浅色主题）。
    static constexpr Rgb kInk{17, 24, 39};      // 标题/价格（近黑）
    static constexpr Rgb kMuted{107, 114, 128};  // 次要文字（灰）
    static constexpr Rgb kUp{22, 163, 74};       // 涨 = 绿（MSN）
    static constexpr Rgb kDown{220, 50, 50};     // 跌 = 红（MSN）

    std::vector<std::string> labels_;
    std::vector<Candle> candles_;
    mutable double ymin_ = 0.0;  // 价格下界（compute 回填；draw_hover/tooltip 用）
    mutable int label_step_ = 1;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_HSI_KLINE_RENDERER_HPP
