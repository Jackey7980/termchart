// widget_tree/hsi_kline_renderer.hpp —— 恒生指数 K 线（蜡烛图）渲染（MSN 财经风格）
//
// "骨肉分离"后的"肉"之二：从原 CandlestickWidget 抽出的 HSI K 线绘制逻辑，作为可插拔
// 的 IChartRenderer。复用 ChartWidget 的动画/缓存/降分辨率/淡入/悬停十字轴——即把同一套
// "插座"同时驱动 combo 与 K 线。
//
// 本版新增：① 全色走 current_theme()（一键 Dark/Light，默认深色，与 GitHubDark 预设配套）；
// ② ViewWindow 支持——ChartWidget 滚轮缩放 / 拖拽平移时限制可见区间与 y 量纲；
// ③ 底部成交量副图（与 K 线联动同一视图窗口）。
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
#include "termchart_gui/theme.hpp"
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

    // 成交量（可选）：长度需等于 candles 长度；缺失则不画成交量副图。
    void set_volumes(std::vector<double> vols) { volumes_ = std::move(vols); }

    [[nodiscard]] Rgb background() const override { return current_theme().background; }
    [[nodiscard]] Rgb foreground() const override { return current_theme().foreground; }

    [[nodiscard]] bool empty() const override { return candles_.empty(); }

    // 悬停点的 y（收盘价）位置 + 读数标签，供 ChartWidget 画水平十字线与实时价格。
    [[nodiscard]] double hover_y(const ChartGeom& g, int hovered) const override {
        if (hovered < 0 || hovered >= static_cast<int>(candles_.size()))
            return std::nan("");
        const double d = (g.ytop - ymin_) > 0.0 ? (g.ytop - ymin_) : 1.0;
        return g.pb - (g.pb - g.pt) *
                          (candles_[static_cast<std::size_t>(hovered)].close - ymin_) / d;
    }
    [[nodiscard]] std::string hover_label(const ChartGeom& g, int hovered) const override {
        (void)g;
        if (hovered < 0 || hovered >= static_cast<int>(candles_.size()))
            return std::string{};
        return fmt_price(candles_[static_cast<std::size_t>(hovered)].close);
    }

    // 由数据 + 控件尺寸算目标几何：价格范围（ymin/ymax）+ 蜡烛中心 x + plot 矩形。
    // 受 view_（Pan/Zoom）约束：只算可见下标区间 [lo,hi) 的几何；ymin/ymax 覆盖量纲。
    [[nodiscard]] ChartGeom compute_geom(int W, int H) const override {
        ChartGeom g;
        const int N = static_cast<int>(candles_.size());
        g.n = N;
        if (N == 0) return g;

        const ViewWindow v = view_;
        int hi = (v.hi < 0 || v.hi > N) ? N : v.hi;
        int lo = v.lo;
        if (lo < 0) lo = 0;
        if (lo > hi) lo = hi;
        const int M = hi - lo;  // 可见根数
        if (M <= 0) return g;

        const bool has_vol = volumes_.size() == candles_.size();
        const double header = 50.0;
        const double bottom = 34.0;  // 统计条
        const double vol_reserve =
            has_vol ? (static_cast<double>(H) - header - bottom) * 0.26 + 8.0 : 0.0;

        double lo_p = candles_[static_cast<std::size_t>(lo)].low;
        double hi_p = candles_[static_cast<std::size_t>(lo)].high;
        for (int i = lo + 1; i < hi; ++i) {
            const Candle& cd = candles_[static_cast<std::size_t>(i)];
            lo_p = std::min(lo_p, cd.low);
            hi_p = std::max(hi_p, cd.high);
        }
        if (!view_is_nan(v.ymin)) lo_p = v.ymin;
        if (!view_is_nan(v.ymax)) hi_p = v.ymax;
        double pad = (hi_p - lo_p) * 0.06;
        if (!(pad > 0.0)) pad = std::max(1.0, std::fabs(hi_p) * 0.01);
        const double step = nice_step((hi_p - lo_p + 2.0 * pad) / 4.0);
        const double y0 = std::floor((lo_p - pad) / step) * step;
        const double y1 = std::ceil((hi_p + pad) / step) * step;

        g.pl = 58.0;
        g.pt = header;
        g.pr = static_cast<double>(W) - 14.0;
        g.pb = static_cast<double>(H) - bottom - vol_reserve;  // 价格区底部
        g.ytop = (y1 > y0) ? y1 : y0 + step;
        g.step = step;
        g.intervals = std::max(1, static_cast<int>(std::ceil((g.ytop - y0) / step)));

        g.xs.resize(static_cast<std::size_t>(N));
        for (int i = lo; i < hi; ++i) {
            g.xs[static_cast<std::size_t>(i)] =
                g.pl + (g.pr - g.pl) * (static_cast<double>(i - lo) + 0.5) /
                           static_cast<double>(M);
        }
        // 不可见蜡烛：停到绘图区外（hit/绘制都不会落到它们）。
        for (int i = 0; i < lo; ++i) g.xs[static_cast<std::size_t>(i)] = g.pl - 1.0;
        for (int i = hi; i < N; ++i) g.xs[static_cast<std::size_t>(i)] = g.pr + 1.0;

        ymin_ = y0;  // 供 draw_hover/draw_tooltip 做价格坐标换算
        if (has_vol) {
            vol_top_ = g.pb + 10.0;
            vol_bottom_ = static_cast<double>(H) - bottom;
        } else {
            vol_top_ = 0.0;
            vol_bottom_ = 0.0;
        }
        return g;
    }

    // ① 顶部价格头（MSN）② 网格+刻度 ③ 蜡烛 ④ x 日期标签 ⑤ 成交量副图 ⑥ 底部统计条。
    void build_base_scene(termchart::scene::SceneBuilder& b, const ChartGeom& g, double /*glow*/,
                          bool /*dragging*/) const override {
        const int N = g.n;
        if (N == 0) return;
        const auto to_y = [&](double v) {
            return g.pb - (g.pb - g.pt) * (v - ymin_) / (g.ytop - ymin_);
        };
        const auto cx = [&](int i) {
            return g.pl + (g.pr - g.pl) * (static_cast<double>(i - view_.lo) + 0.5) /
                               static_cast<double>(visible_count());
        };
        const auto col = [](Rgb c) { return termchart::Color::rgb(c.r, c.g, c.b); };
        const GuiTheme th = current_theme();

        Style ink;
        Style muted;
        ink.set_fg(col(th.foreground));
        muted.set_fg(col(th.text_dim));

        // ① 顶部价格头：标题 + 日期范围；最新价 + 涨跌额/幅（带色）。
        const Candle& last = candles_[static_cast<std::size_t>(N - 1)];
        const Candle& prev = candles_[static_cast<std::size_t>(N - 2)];
        const double chg = last.close - prev.close;
        const double pct = (prev.close != 0.0) ? chg / prev.close * 100.0 : 0.0;
        const Rgb chg_col = (chg >= 0.0) ? th.up : th.down;
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
        b.stroke(col(th.grid), 1.0);
        for (double v = ymin_; v <= g.ytop + 1e-6; v += g.step) {
            const double y = to_y(v);
            b.move_to(g.pl, y);
            b.line_to(g.pr, y);
            const std::string lab = fmt_int(v);
            b.text(Point{g.pl - 8.0 - static_cast<double>(lab.size()) * 9.0, y - 7.0}, lab,
                   muted);
        }
        b.stroke(col(th.axis), 1.0);
        b.rect(Rect{g.pl, g.pt, g.pr - g.pl, g.pb - g.pt});

        // ③ 蜡烛：影线 + 实体，绿涨红跌（实心）。
        const double bw = (g.pr - g.pl) / static_cast<double>(visible_count()) * 0.6;
        const double wick = std::max(1.0, bw * 0.12);
        for (int i = view_.lo; i < view_.lo + visible_count(); ++i) {
            if (i < 0 || i >= N) continue;
            const Candle& cd = candles_[static_cast<std::size_t>(i)];
            const double x = cx(i);
            const double yh = to_y(cd.high);
            const double yl = to_y(cd.low);
            const double yo = to_y(cd.open);
            const double yc = to_y(cd.close);
            const bool up = cd.close >= cd.open;
            const Rgb cc = up ? th.up : th.down;
            b.fill(col(cc));
            b.rect(Rect{x - wick / 2.0, yh, wick, yl - yh});  // 影线
            const double top = std::min(yo, yc);
            const double bot = std::max(yo, yc);
            b.rect(Rect{x - bw / 2.0, top, bw, std::max(1.0, bot - top)});  // 实体（实心）
        }

        // ④ x 日期标签（按可用宽度抽稀）。
        const double slot = (g.pr - g.pl) / static_cast<double>(visible_count());
        const int lstep =
            std::max(1, static_cast<int>(std::ceil(44.0 / (slot > 1.0 ? slot : 1.0))));
        label_step_ = lstep;
        for (int i = view_.lo; i < view_.lo + visible_count(); ++i) {
            if (i < 0 || i >= N) continue;
            if (i % lstep != 0 && i != N - 1) continue;
            const std::string lab = short_date(labels_[static_cast<std::size_t>(i)]);
            const double w = static_cast<double>(lab.size()) * 9.0;
            b.text(Point{cx(i) - w / 2.0, g.pb + 6.0}, lab, muted);
        }

        // ⑤ 成交量副图（与 K 线联动同一视图窗口，绿涨红跌）。
        if (volumes_.size() == candles_.size() && vol_bottom_ > vol_top_) {
            double vmax = 0.0;
            for (int i = view_.lo; i < view_.lo + visible_count(); ++i) {
                if (i < 0 || i >= N) continue;
                vmax = std::max(vmax, volumes_[static_cast<std::size_t>(i)]);
            }
            if (vmax <= 0.0) vmax = 1.0;
            const double vh = vol_bottom_ - vol_top_;
            b.stroke(col(th.axis), 1.0);
            b.move_to(g.pl, vol_top_ - 2.0);
            b.line_to(g.pr, vol_top_ - 2.0);
            for (int i = view_.lo; i < view_.lo + visible_count(); ++i) {
                if (i < 0 || i >= N) continue;
                const Candle& cd = candles_[static_cast<std::size_t>(i)];
                const double x = cx(i);
                const double bh = vh * volumes_[static_cast<std::size_t>(i)] / vmax;
                const Rgb vc = (cd.close >= cd.open) ? th.up : th.down;
                b.fill(col(vc));
                b.rect(Rect{x - wick / 2.0, vol_bottom_ - bh, wick, bh});
            }
            muted.set_fg(col(th.text_dim));
            b.text(Point{g.pl - 4.0, vol_top_ - 14.0}, "Volume", muted);
        }

        // ⑥ 底部统计条（MSN）：前收 / 当日高 / 当日低。
        {
            const double sy = (vol_bottom_ > vol_top_) ? vol_bottom_ + 6.0 : g.pb + 20.0;
            const std::string s = "Prev " + fmt_price(prev.close) + "    High " +
                                  fmt_price(last.high) + "    Low " + fmt_price(last.low);
            b.text(Point{g.pl - 4.0, sy}, s, muted);
        }
    }

    // K 线无面积/折线层。
    void build_area_scene(termchart::scene::SceneBuilder& /*b*/, const ChartGeom& /*g*/,
                          double /*glow*/) const override {}
    void build_line_scene(termchart::scene::SceneBuilder& /*b*/, const ChartGeom& /*g*/,
                          double /*glow*/) const override {}

    // 命中：鼠标落在可见区某根蜡烛的 x 列 → 返回其（实际）下标，否则 -1。
    [[nodiscard]] int hit_index(const ChartGeom& g, double lx, double ly) const override {
        const int N = g.n;
        if (N == 0 || lx < g.pl || lx > g.pr || ly < g.pt || ly > g.pb) return -1;
        const int M = visible_count();
        if (M <= 0) return -1;
        const double slot = (g.pr - g.pl) / static_cast<double>(M);
        int k = static_cast<int>((lx - g.pl) / slot);
        if (k < 0) k = 0;
        if (k >= M) k = M - 1;
        const int idx = view_.lo + k;
        if (idx < 0 || idx >= N) return -1;
        return idx;
    }

    [[nodiscard]] int label_step() const override { return label_step_; }

    // 悬停强调：高亮悬停蜡烛的外框（琥珀色）+ 横线（十字光标）。
    void draw_hover(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered,
                   double /*glow*/) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        if (h >= candles_.size()) return;
        const double x = geom.xs[h];
        const double bw = (geom.pr - geom.pl) / static_cast<double>(visible_count()) * 0.6;
        const double yh =
            geom.pb - (geom.pb - geom.pt) * (candles_[h].high - ymin_) / (geom.ytop - ymin_);
        const double yl =
            geom.pb - (geom.pb - geom.pt) * (candles_[h].low - ymin_) / (geom.ytop - ymin_);
        const GuiTheme th = current_theme();
        c.stroke_rect(Rect{g.x + x - bw / 2.0 - 2.0, g.y + yh - 2.0, bw + 4.0, (yl - yh) + 4.0},
                      Rgb{255, 179, 0}, 1.0);  // 蜡烛高亮框
        // 水平十字线（价格轴）贯穿绘图区，便于读数。
        c.fill_rect(Rect{g.x + geom.pl, g.y + yh, geom.pr - geom.pl, 1.0},
                    th.crosshair);
    }

    void draw_tooltip(TkCanvas& c, const Rect& g, const ChartGeom& geom, int hovered) const override {
        const std::size_t h = static_cast<std::size_t>(hovered);
        if (h >= candles_.size()) return;
        const Candle& cd = candles_[h];
        const bool up = cd.close >= cd.open;
        const GuiTheme th = current_theme();
        const Rgb accent = up ? th.up : th.down;
        const double bw = 132.0;
        const double bh = 66.0;
        const double cxh = geom.xs[h];
        double tx = g.x + cxh + 12.0;
        if (tx + bw > g.x + g.w) tx = g.x + cxh - bw - 12.0;  // 贴右边则翻到左侧
        if (tx < g.x + 4.0) tx = g.x + 4.0;
        const double ty = g.y + std::max(4.0, geom.pt + 4.0);

        c.fill_rect(Rect{tx + 2.0, ty + 2.0, bw, bh}, th.surface_alt);    // 投影
        c.fill_rect(Rect{tx, ty, bw, bh}, th.tooltip_bg);                 // 主体
        c.stroke_rect(Rect{tx, ty, bw, bh}, th.tooltip_border, 1.0);      // 描边
        c.fill_rect(Rect{tx, ty, 3.0, bh}, accent);                      // 左强调条
        c.draw_text(tx + 9.0, ty + 5.0, labels_[h], th.tooltip_fg);       // 日期
        c.draw_text(tx + 9.0, ty + 22.0, "O " + fmt_price(cd.open), th.text_dim);
        c.draw_text(tx + 9.0, ty + 35.0, "H " + fmt_price(cd.high), accent);
        c.draw_text(tx + 66.0, ty + 22.0, "L " + fmt_price(cd.low), accent);
        c.draw_text(tx + 66.0, ty + 35.0, "C " + fmt_price(cd.close),
                    up ? th.up : th.down);
    }

private:
    int visible_count() const noexcept {
        const int N = static_cast<int>(candles_.size());
        int hi = (view_.hi < 0 || view_.hi > N) ? N : view_.hi;
        int lo = view_.lo < 0 ? 0 : view_.lo;
        int m = hi - lo;
        if (m < 0) m = 0;
        return m;
    }

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

    std::vector<std::string> labels_;
    std::vector<Candle> candles_;
    std::vector<double> volumes_;  // 成交量（可选，长度==candles 才画）
    mutable double ymin_ = 0.0;     // 价格下界（compute 回填；draw_hover/tooltip 用）
    mutable double vol_top_ = 0.0;  // 成交量副图顶（compute 回填）
    mutable double vol_bottom_ = 0.0;  // 成交量副图底
    mutable int label_step_ = 1;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_HSI_KLINE_RENDERER_HPP
