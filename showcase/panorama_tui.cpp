// showcase/panorama_tui.cpp —— TermChart 能力全景 · 终端仪表盘（TUI）
//
// 一张"海报"把库的能力跑一遍：12 个面板覆盖 chart（折线/柱/散点/箱线/直方图/
// 热力）+ 手工 K 线 + graph（力导图/流程图）+ chart3d/chart4d + math（stats/
// special/interpolate/fft/linalg）+ data（ema/rolling/ohlc/histogram/quartiles）
// + io（CSV 往返）+ image（KDE 位图）+ backend（SVG/PNG 导出）+ render/compositor
// + term（alt-screen）。数据来自 panorama_data.hpp（固定种子、可复现、够难）。
//
// 用法：
//   panorama_tui                     非 tty：渲染一帧海报到 stdout（确定性，CI 可用）
//   panorama_tui --cols 180 --rows 66  指定海报尺寸
//   panorama_tui --animate 120       交互终端：alt-screen 动画（4D 自转 + 3D 轨道）
//   panorama_tui --export-dir DIR    额外导出 panorama.svg + density.png
//   panorama_tui --net               尝试一次真实 HTTP GET（自动探测代理），失败不致命
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "termchart/termchart_full.hpp"             // 全库伞形头
#include "termchart/api/print.hpp"                  // write_stdout / flush_stdout
#include "termchart/backend/vector/svg_backend.hpp"  // SVG 导出
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/boxplot.hpp"
#include "termchart/chart/cartesian/heatmap.hpp"
#include "termchart/chart/cartesian/hist.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/chart3d/common.hpp"
#include "termchart/chart3d/projector.hpp"
#include "termchart/chart3d/scatter.hpp"
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"
#include "termchart/core/attr.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/flowchart.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/force.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/png_encode.hpp"
#include "termchart/math/fft.hpp"
#include "termchart/math/special.hpp"
#include "termchart/math4d/camera4.hpp"
#include "termchart/math4d/mat5.hpp"
#include "termchart/math4d/vec4.hpp"
#include "termchart/math3d/vec3.hpp"
#include "termchart/network/http.hpp"
#include "termchart/render/compositor.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/scene/builder.hpp"
#include "termchart/term/terminal.hpp"

#include "panorama_data.hpp"

namespace tc = termchart;
using tc::CellBuffer;
using tc::Cell;
using tc::Color;
using tc::Point;
using tc::Style;

namespace {

// ── UTF-8 解码 + 宽字符宽度（供手动绘制中文标签，正确处理宽字符续接格）────────
std::vector<char32_t> utf8_decode(std::string_view s) {
    std::vector<char32_t> out;
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        int len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c >> 4) == 0xE) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c >> 3) == 0x1E) {
            cp = c & 0x07;
            len = 4;
        } else {
            cp = 0xFFFD;
        }
        for (int k = 1; k < len && i + static_cast<std::size_t>(k) < s.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]) & 0x3F);
        }
        out.push_back(cp);
        i += static_cast<std::size_t>(len);
    }
    return out;
}

bool is_wide(char32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x3000 && cp <= 0x303E) || (cp >= 0x3040 && cp <= 0x33FF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

// 把 UTF-8 文本写入 CellBuffer；宽字符占两格、右格填续接码点（渲染器据此跳过）。
void draw_text(CellBuffer& b, std::uint32_t x, std::uint32_t y, std::string_view s, const Style& st) {
    std::uint32_t cx = x;
    for (const char32_t cp : utf8_decode(s)) {
        if (cp == U'\n') {
            ++y;
            cx = x;
            continue;
        }
        const std::uint32_t w = is_wide(cp) ? 2u : 1u;
        if (!b.contains(cx, y)) break;
        b.at(cx, y).cp = cp;
        b.at(cx, y).style = st;
        if (w == 2 && cx + 1 < b.width()) {
            b.at(cx + 1, y).cp = U'\0';  // 宽字符右半格 = 续接格
            b.at(cx + 1, y).style = st;
        }
        cx += w;
        if (cx >= b.width()) break;
    }
}

// 画一个带标题的边框（标题可含中文）。
void frame(CellBuffer& p, std::string_view title, Color border, Color title_c) {
    const std::uint32_t w = p.width();
    const std::uint32_t h = p.height();
    if (w < 2 || h < 2) return;
    Style bs;
    bs.set_fg(border);
    auto put = [&](std::uint32_t x, std::uint32_t y, char32_t ch) {
        if (p.contains(x, y)) {
            p.at(x, y).cp = ch;
            p.at(x, y).style = bs;
        }
    };
    put(0, 0, U'┌');
    put(w - 1, 0, U'┐');
    put(0, h - 1, U'└');
    put(w - 1, h - 1, U'┘');
    for (std::uint32_t x = 1; x + 1 < w; ++x) {
        put(x, 0, U'─');
        put(x, h - 1, U'─');
    }
    for (std::uint32_t y = 1; y + 1 < h; ++y) {
        put(0, y, U'│');
        put(w - 1, y, U'│');
    }
    Style ts;
    ts.set_fg(title_c).set_attr(tc::Attr::Bold);
    draw_text(p, 2, 0, title, ts);
}

// 组装一个面板：边框 + 标题 + 若干内部子缓冲（按 dy 叠放）。
CellBuffer assemble_panel(std::uint32_t w, std::uint32_t h, std::string_view title, Color border,
                          Color title_c, std::vector<std::pair<std::uint32_t, const CellBuffer*>> layers) {
    CellBuffer p(w, h);
    for (auto& [dy, buf] : layers) {
        if (buf == nullptr) continue;
        for (std::uint32_t y = 0; y + 1 + dy + 1 < h && y < buf->height(); ++y) {
            for (std::uint32_t x = 0; x + 1 < w && x < buf->width(); ++x) {
                p.at(1 + x, 1 + dy + y) = buf->at(x, y);
            }
        }
    }
    frame(p, title, border, title_c);
    return p;
}

// 经 canvas 渲染（chart/scene/graph 的统一入口）→ CellBuffer。
template <typename Fn>
CellBuffer via_canvas(std::uint32_t w, std::uint32_t h, Fn&& fn) {
    tc::canvas::Canvas cv(w, h);
    fn(cv);
    return cv.to_buffer();
}

// ── 12 个面板 ────────────────────────────────────────────────────────────────

// 1) 价格路径：6 品种 + 等权指数 + 指数平滑（ CubicSpline ）
CellBuffer panel_line(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::LineChart c;
        c.set_title("价格路径 · 多尺度 fBm");
        c.set_show_legend(true);
        c.set_auto_domain(true, false);
        for (std::size_t k = 0; k < p.inst.size(); ++k) {
            tc::chart::Series s;
            s.name = p.inst[k].tag;
            s.color = Color::ansi16(static_cast<std::uint8_t>(k + 1));
            for (std::size_t i = 0; i < p.M; i += 4) {
                s.points.push_back(Point{static_cast<double>(i), p.inst[k].price[i]});
            }
            c.add_series(std::move(s));
        }
        {
            tc::chart::Series s;
            s.name = "IDX";
            s.color = Color::ansi16(7);
            for (std::size_t i = 0; i < p.M; i += 4) {
                s.points.push_back(Point{static_cast<double>(i), p.market[i]});
            }
            c.add_series(std::move(s));
        }
        {
            tc::chart::Series s;
            s.name = "SMOOTH";
            s.color = Color::rgb(120, 220, 255);
            for (std::size_t i = 0; i < p.M; i += 4) {
                s.points.push_back(Point{static_cast<double>(i), p.market_smooth[i]});
            }
            c.add_series(std::move(s));
        }
        c.draw({cv});
    });
}

// 2) K 线 + 成交量（手工绘制：影线 + 实体 + 涨跌配色）
CellBuffer panel_candles(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    CellBuffer b(w, h);
    if (p.candles.empty() || w < 8 || h < 4) return b;
    double lo = p.candles[0].low;
    double hi = p.candles[0].high;
    for (const auto& c : p.candles) {
        lo = std::min(lo, c.low);
        hi = std::max(hi, c.high);
    }
    const std::uint32_t x0 = 7;
    const std::uint32_t x1 = w - 1;
    const double span = std::max(1e-9, hi - lo);
    auto ymap = [&](double v) -> std::uint32_t {
        const double t = (hi - v) / span;
        return static_cast<std::uint32_t>(1.0 + t * static_cast<double>(h - 3));
    };
    const Color up = Color::rgb(80, 220, 120);
    const Color dn = Color::rgb(240, 90, 90);
    const Color wick = Color::rgb(150, 150, 160);
    const std::size_t n = p.candles.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double fx = (n <= 1) ? 0.0 : static_cast<double>(i) / static_cast<double>(n - 1);
        const std::uint32_t cx = x0 + static_cast<std::uint32_t>(fx * static_cast<double>(x1 - x0));
        if (cx >= w) continue;
        const std::uint32_t yh = ymap(p.candles[i].high);
        const std::uint32_t yl = ymap(p.candles[i].low);
        for (std::uint32_t y = yh; y <= yl && y + 1 < h; ++y) {
            if (b.contains(cx, y)) {
                b.at(cx, y).cp = U'│';
                b.at(cx, y).style = Style{}.set_fg(wick);
            }
        }
        const std::uint32_t yo = ymap(p.candles[i].open);
        const std::uint32_t yc = ymap(p.candles[i].close);
        const std::uint32_t top = std::min(yo, yc);
        const std::uint32_t bot = std::max(yo, yc);
        const Color col = (p.candles[i].close >= p.candles[i].open) ? up : dn;
        for (std::uint32_t y = top; y <= bot && y + 1 < h; ++y) {
            if (b.contains(cx, y)) {
                b.at(cx, y).cp = U'█';
                b.at(cx, y).style = Style{}.set_fg(col);
            }
        }
    }
    // 左轴价格标签
    Style ls;
    ls.set_fg(Color::rgb(170, 170, 180));
    draw_text(b, 1, 1, std::to_string(static_cast<long long>(hi)), ls);
    draw_text(b, 1, h - 2, std::to_string(static_cast<long long>(lo)), ls);
    return b;
}

// 3) 收益分布直方图（汇总收益）
CellBuffer panel_hist(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::HistChart c;
        c.set_title("收益分布（汇总）");
        c.set_values(p.pooled_ret, Color::rgb(120, 180, 255), "ret");
        c.set_bins(26);
        c.draw({cv});
    });
}

// 4) 风险—收益散点（气泡）+ 一元最小二乘回归线（linalg::solve）
CellBuffer panel_scatter(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::ScatterChart c;
        c.set_title("风险—收益（气泡=规模）");
        c.set_show_legend(true);
        c.set_auto_domain(true, true);
        tc::chart::Series pts;
        pts.name = "品种";
        pts.color = Color::rgb(255, 200, 80);
        for (const auto& r : p.rr) {
            pts.points.push_back(Point{r[0], r[1]});
        }
        std::vector<double> weights;
        for (const auto& r : p.rr) weights.push_back(r[2]);
        c.add_bubble_series(std::move(pts), weights);
        // 回归线：沿 slope*x+intercept 采样成密点（视觉上成线）
        double xmin = p.rr_x.front();
        double xmax = p.rr_x.front();
        for (double x : p.rr_x) {
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
        }
        tc::chart::Series line;
        line.name = "回归";
        line.color = Color::rgb(255, 90, 160);
        for (int i = 0; i <= 24; ++i) {
            const double x = xmin + (xmax - xmin) * static_cast<double>(i) / 24.0;
            line.points.push_back(Point{x, p.reg_slope * x + p.reg_intercept});
        }
        c.add_series(std::move(line));
        c.draw({cv});
    });
}

// 5) 分品种箱线（data::quartiles + 1.5·IQR 离群点）
CellBuffer panel_box(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::BoxPlotChart c;
        c.set_title("收益箱线（分品种）");
        std::vector<tc::chart::BoxPlotChart::BoxStats> groups;
        groups.reserve(p.boxes.size());
        for (std::size_t k = 0; k < p.boxes.size(); ++k) {
            tc::chart::BoxPlotChart::BoxStats g;
            g.lo = p.boxes[k].minimum;
            g.q1 = p.boxes[k].q1;
            g.med = p.boxes[k].median;
            g.q3 = p.boxes[k].q3;
            g.hi = p.boxes[k].maximum;
            g.outliers = p.box_outliers[k];
            groups.push_back(std::move(g));
        }
        c.set_data(std::move(groups));
        c.draw({cv});
    });
}

// 6) 特殊函数曲线（math::special，各自归一化到 [0,1] 看形状）
CellBuffer panel_special(std::uint32_t w, std::uint32_t h, const showcase::Panorama&) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::LineChart c;
        c.set_title("特殊函数 erf / Γ / J0（归一化）");
        c.set_show_legend(true);
        c.set_auto_domain(true, false);
        std::vector<double> xs;
        std::vector<double> erf_v;
        std::vector<double> gam_v;
        std::vector<double> j0_v;
        for (int i = 0; i <= 60; ++i) {
            const double x = 0.2 + 5.8 * static_cast<double>(i) / 60.0;
            xs.push_back(x);
            erf_v.push_back(tc::math::erf(x / 2.5));
            gam_v.push_back(tc::math::gamma(x));
            j0_v.push_back(tc::math::bessel_j0(x));
        }
        auto norm = [](std::vector<double> v) {
            double mn = v.front();
            double mx = v.front();
            for (double e : v) {
                mn = std::min(mn, e);
                mx = std::max(mx, e);
            }
            const double d = std::max(1e-9, mx - mn);
            for (double& e : v) e = (e - mn) / d;
            return v;
        };
        const auto e = norm(erf_v);
        const auto g = norm(gam_v);
        const auto j = norm(j0_v);
        auto series = [&](const char* name, Color col, const std::vector<double>& ys) {
            tc::chart::Series s;
            s.name = name;
            s.color = col;
            for (std::size_t i = 0; i < xs.size(); ++i) s.points.push_back(Point{xs[i], ys[i]});
            c.add_series(std::move(s));
        };
        series("erf", Color::rgb(90, 220, 255), e);
        series("Gamma", Color::rgb(255, 180, 90), g);
        series("J0", Color::rgb(160, 255, 140), j);
        c.draw({cv});
    });
}

// 7) KDE 密度热力（ HeatmapChart，输入 density_pop ）
CellBuffer panel_heatmap(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::chart::HeatmapChart c;
        c.set_title("(波动,收益) KDE 密度");
        c.set_values(p.density_pop, tc::color::ColormapKind::Viridis, "kde");
        c.draw({cv});
    });
}

// 8) 指数功率谱（math::fft）→ 柱状
CellBuffer panel_fft(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        const std::size_t n = tc::math::next_pow2(p.M);
        std::vector<double> re(n, 0.0);
        std::vector<double> im(n, 0.0);
        const double mean = tc::math::mean(p.market);
        for (std::size_t i = 0; i < p.M; ++i) re[i] = p.market[i] - mean;
        static_cast<void>(tc::math::fft(re, im));
        std::vector<double> mag;
        std::vector<std::string> labels;
        const std::size_t bins = 32;
        for (std::size_t k = 1; k <= bins; ++k) {
            const std::size_t idx = k * (n / 2) / (bins + 1);
            mag.push_back(std::hypot(re[idx], im[idx]));
            labels.push_back(std::to_string(static_cast<long long>(k)));
        }
        tc::chart::BarChart c;
        c.set_title("指数功率谱（FFT）");
        c.set_values(std::move(mag), std::move(labels), Color::rgb(200, 120, 255), "|X(f)|");
        c.draw({cv});
    });
}

// 9) 相关力导图（layout_force + render_node_link）
CellBuffer panel_force(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        std::vector<tc::graph::NodeId> ids;
        ids.reserve(p.inst.size());
        for (const auto& in : p.inst) ids.push_back(g.add_node(in.tag).id());
        for (const auto& e : p.corr_edges) {
            const double cval = p.corr[static_cast<std::size_t>(e[0])][static_cast<std::size_t>(e[1])];
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", cval);
            g.add_edge(ids[static_cast<std::size_t>(e[0])], ids[static_cast<std::size_t>(e[1])], buf);
        }
        tc::graph::ForceOptions fo;
        fo.ideal_edge_length = 9.0;
        static_cast<void>(tc::graph::layout_force(g, fo));
        tc::graph::NodeLinkOptions no;
        no.route_style = tc::graph::RouteStyle::Straight;
        tc::graph::render_node_link(g, cv, no);
    });
}

// 10) 数据管线流程图（render_flowchart）
CellBuffer panel_pipeline(std::uint32_t w, std::uint32_t h, const showcase::Panorama&) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        tc::graph::Graph g;
        const auto a = g.add_node("fBm 生成").id();
        const auto b = g.add_node("EMA/滚动").id();
        const auto c = g.add_node("OHLC").id();
        const auto d = g.add_node("统计/相关").id();
        const auto e = g.add_node("投影 3D/4D").id();
        const auto f = g.add_node("渲染").id();
        g.add_edge(a, b);
        g.add_edge(b, c);
        g.add_edge(c, d);
        g.add_edge(d, e);
        g.add_edge(e, f);
        tc::graph::FlowchartOptions fo;
        fo.shape = tc::graph::ShapeKind::Box;
        tc::graph::render_flowchart(g, cv, fo);
    });
}

// 11) Lorenz 吸引子（chart3d::scatter3d）
CellBuffer panel_lorenz(std::uint32_t w, std::uint32_t h, const showcase::Panorama& p) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        std::vector<tc::math3d::Vec3> pts;
        pts.reserve(600);
        for (std::size_t i = 0; i < p.lorenz.size(); i += 6) pts.push_back(p.lorenz[i]);
        tc::chart3d::Scatter3dOpts o;
        o.cols = static_cast<double>(w);
        o.rows = static_cast<double>(h);
        o.azimuth_deg = 35.0;
        o.elevation_deg = 18.0;
        o.axes = false;
        tc::chart3d::Projector3D proj(tc::chart3d::make_camera(
            static_cast<double>(w), static_cast<double>(h), 35.0, 18.0, 55.0,
            tc::math3d::Vec3{0, 0, 0}, 48.0));
        const tc::scene::Scene sc = tc::chart3d::scatter3d(pts, o, &proj);
        tc::canvas::render_scene(sc, cv);
    });
}

// 12) 4D 超立方体（math4d + chart4d::wire4d，两阶段投影）
CellBuffer panel_hypercube(std::uint32_t w, std::uint32_t h, double axw, double ayz) {
    return via_canvas(w, h, [&](tc::canvas::Canvas& cv) {
        std::vector<tc::math4d::Vec4> verts;
        for (int i = 0; i < 16; ++i) {
            verts.push_back(tc::math4d::Vec4{(i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0,
                                             (i & 4) ? 1.0 : -1.0, (i & 8) ? 1.0 : -1.0});
        }
        std::vector<tc::chart4d::Edge> edges;
        for (int i = 0; i < 16; ++i) {
            for (int b = 0; b < 4; ++b) {
                const int j = i ^ (1 << b);
                if (j > i) edges.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
            }
        }
        tc::math4d::Camera4 cam4;
        cam4.distance = 3.0;
        cam4.focal = tc::math4d::focal_from_fov_deg(60.0);
        const auto inner = std::make_shared<tc::chart3d::Projector3D>(tc::chart3d::make_camera(
            static_cast<double>(w), static_cast<double>(h), 32.0, 20.0, 55.0,
            tc::math3d::Vec3{0, 0, 0}, 2.4));
        const tc::chart4d::Projector4D proj(tc::math4d::rotate_xw(axw) * tc::math4d::rotate_yz(ayz),
                                            cam4, inner);
        tc::chart4d::Wire4dOpts o;
        o.cols = static_cast<double>(w);
        o.rows = static_cast<double>(h);
        o.point_size = 0.9;
        o.draw_vertices = true;
        const tc::scene::Scene sc = tc::chart4d::wire4d(verts, edges, o, proj);
        tc::canvas::render_scene(sc, cv);
    });
}

// ── KDE 位图（image::Bitmap）→ 供 PNG 导出 + 统计 ───────────────────────────
tc::image::Bitmap kde_bitmap(const showcase::Panorama& p, std::uint32_t W, std::uint32_t H,
                             std::size_t& cells, double& maxv) {
    std::vector<double> field(static_cast<std::size_t>(W) * H, 0.0);
    double vmin = 1e9;
    double vmax = -1e9;
    double rmin = 1e9;
    double rmax = -1e9;
    for (const Point& pt : p.density_pop) {
        vmin = std::min(vmin, pt.x);
        vmax = std::max(vmax, pt.x);
        rmin = std::min(rmin, pt.y);
        rmax = std::max(rmax, pt.y);
    }
    const double dv = std::max(1e-9, vmax - vmin);
    const double dr = std::max(1e-9, rmax - rmin);
    for (const Point& pt : p.density_pop) {
        const double fx = (pt.x - vmin) / dv * static_cast<double>(W - 1);
        const double fy = (pt.y - rmin) / dr * static_cast<double>(H - 1);
        const int cx = static_cast<int>(fx);
        const int cy = static_cast<int>(fy);
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                const int x = cx + dx;
                const int y = cy + dy;
                if (x < 0 || y < 0 || x >= static_cast<int>(W) || y >= static_cast<int>(H)) continue;
                field[static_cast<std::size_t>(y) * W + static_cast<std::size_t>(x)] +=
                    std::exp(-(static_cast<double>(dx * dx + dy * dy)) / 2.0);
            }
        }
    }
    maxv = 0.0;
    cells = 0;
    for (double v : field) {
        maxv = std::max(maxv, v);
        if (v > 0.0) ++cells;
    }
    const double inv = (maxv > 0.0) ? 1.0 / maxv : 0.0;
    tc::image::Bitmap bmp(W, H, 3);
    for (std::uint32_t y = 0; y < H; ++y) {
        for (std::uint32_t x = 0; x < W; ++x) {
            const double v = field[static_cast<std::size_t>(y) * W + x] * inv;
            const auto g = static_cast<std::uint8_t>(std::clamp(v, 0.0, 1.0) * 255.0);
            bmp.set(x, y, 0, static_cast<std::uint8_t>(g * 0.4));
            bmp.set(x, y, 1, static_cast<std::uint8_t>(g * 0.7));
            bmp.set(x, y, 2, g);
        }
    }
    return bmp;
}

bool save_file(const std::string& path, const std::string& bytes) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

// 组装整张海报（一个静态帧）
CellBuffer compose_poster(const showcase::Panorama& p, std::uint32_t cols, std::uint32_t rows,
                          double axw, double ayz) {
    CellBuffer base(cols, rows);
    // 顶部标题
    Style ts;
    ts.set_fg(Color::rgb(235, 235, 245)).set_attr(tc::Attr::Bold);
    draw_text(base, 2, 0, "TermChart 能力全景 · Panorama", ts);
    Style ss;
    ss.set_fg(Color::rgb(150, 160, 180));
    draw_text(base, 2, 1,
              "多尺度 fBm 市场 + 相关结构 + 4D/3D 几何 | 12 面板覆盖 chart/graph/3D/4D/math/data/io/image",
              ss);

    const std::uint32_t ncol = 3;
    const std::uint32_t npanel = 12;
    const std::uint32_t nrow = (npanel + ncol - 1) / ncol;
    const std::uint32_t pw = cols / ncol;
    const std::uint32_t gy0 = 3;
    const std::uint32_t foot = 1;
    const std::uint32_t avail = (rows > gy0 + foot + 1) ? (rows - gy0 - foot - 1) : 1;
    const std::uint32_t ph = std::max<std::uint32_t>(6, avail / nrow);

    const Color border = Color::rgb(90, 100, 120);
    const Color title_c = Color::rgb(120, 210, 255);
    const std::uint32_t iw = (pw > 2) ? pw - 2 : pw;
    const std::uint32_t ih = (ph > 2) ? ph - 2 : ph;

    std::vector<std::pair<std::uint32_t, CellBuffer>> panels;  // (grid index, buffer)
    auto add = [&](std::uint32_t idx, CellBuffer&& b) { panels.emplace_back(idx, std::move(b)); };
    add(0, panel_line(iw, ih, p));
    add(1, panel_candles(iw, ih, p));
    add(2, panel_hist(iw, ih, p));
    add(3, panel_scatter(iw, ih, p));
    add(4, panel_box(iw, ih, p));
    add(5, panel_special(iw, ih, p));
    add(6, panel_heatmap(iw, ih, p));
    add(7, panel_fft(iw, ih, p));
    add(8, panel_force(iw, ih, p));
    add(9, panel_pipeline(iw, ih, p));
    add(10, panel_lorenz(iw, ih, p));
    add(11, panel_hypercube(iw, ih, axw, ayz));

    static const std::array<const char*, 12> kTitles = {
        "价格路径", "K 线 + 成交量", "收益分布",   "风险—收益", "收益箱线", "特殊函数",
        "KDE 密度", "FFT 功率谱",  "相关力导图", "数据管线",  "Lorenz 3D", "4D 超立方体"};

    // framed_store 预留容量避免重分配，layers2 指向其中元素（指针稳定）。
    std::vector<CellBuffer> framed_store;
    framed_store.reserve(panels.size());
    std::vector<tc::render::Layer> layers2;
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const std::uint32_t idx = panels[i].first;
        const std::uint32_t cx = idx % ncol;
        const std::uint32_t ry = idx / ncol;
        framed_store.push_back(assemble_panel(pw, ph, kTitles[idx], border, title_c,
                                              {{0, &panels[i].second}}));
        layers2.push_back(tc::render::Layer{.buffer = &framed_store.back(),
                                            .x = static_cast<std::int32_t>(cx * pw),
                                            .y = static_cast<std::int32_t>(gy0 + ry * ph),
                                            .transparent = false});
    }

    // 底部统计行
    char foot_text[256];
    std::snprintf(foot_text, sizeof(foot_text),
                  "Γ(5)=%.3f  erf(1)=%.4f  J0(1)=%.4f  FFT主频=%.4f  Pearson(风险,收益)=%.3f  "
                  "CSV往返=%zu行  KDE峰值=%.1f",
                  p.gamma5, p.erf1, p.bessel0, p.fft_peak_hz, p.rr_pearson, p.csv_rows, p.kde_max);
    Style fs;
    fs.set_fg(Color::rgb(140, 200, 160));
    draw_text(base, 1, rows - 1, foot_text, fs);

    tc::render::Compositor::composite(layers2, base);
    return base;
}

void net_probe() {
    const auto r = tc::network::http::get("https://example.com");
    if (r) {
        std::fprintf(stderr, "[net] example.com -> HTTP %u (%zu bytes body)\n", r->status,
                     r->body.size());
    } else {
        std::fprintf(stderr, "[net] 请求失败（自动探测代理后回落直连亦失败）：error=%d\n",
                     static_cast<int>(r.error()));
    }
}

void do_exports(const showcase::Panorama& p, const std::string& dir) {
    // SVG：Lorenz 3D 云 + 标题（backend::vector）
    {
        std::vector<tc::math3d::Vec3> pts;
        for (std::size_t i = 0; i < p.lorenz.size(); i += 4) pts.push_back(p.lorenz[i]);
        tc::chart3d::Scatter3dOpts o;
        o.cols = 240.0;
        o.rows = 160.0;
        o.azimuth_deg = 35.0;
        o.elevation_deg = 18.0;
        o.axes = true;
        tc::chart3d::Projector3D proj(tc::chart3d::make_camera(240, 160, 35, 18, 55,
                                                               tc::math3d::Vec3{0, 0, 0}, 48.0));
        tc::scene::Scene sc = tc::chart3d::scatter3d(pts, o, &proj);
        tc::scene::SceneBuilder b(sc);
        b.text(Point{12.0, 14.0}, "Lorenz Attractor", Style{}.set_fg(Color::rgb(235, 235, 235)));
        tc::backend::vector::SvgBackend svg;
        svg.set_options(tc::backend::vector::VectorOptions{.scale = 3.0});
        const std::string out = tc::backend::vector::render_vector(svg, sc);
        save_file(dir + "/panorama.svg", out);
    }
    // PNG：KDE 密度位图（image::Bitmap + png_encode）
    {
        std::size_t cells = 0;
        double maxv = 0.0;
        tc::image::Bitmap bmp = kde_bitmap(p, 160, 120, cells, maxv);
        const std::string png = tc::image::encode_png(bmp);
        save_file(dir + "/density.png", png);
    }
    std::fprintf(stderr, "[export] 已写出 %s/panorama.svg 与 %s/density.png\n", dir.c_str(),
                 dir.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    std::uint32_t cols = 180;
    std::uint32_t rows = 66;
    int animate = 0;
    std::string export_dir;
    bool net = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto val = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: 缺少参数值\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--cols") {
            cols = static_cast<std::uint32_t>(std::atoi(val().c_str()));
        } else if (a == "--rows") {
            rows = static_cast<std::uint32_t>(std::atoi(val().c_str()));
        } else if (a == "--animate") {
            animate = std::atoi(val().c_str());
        } else if (a == "--export-dir") {
            export_dir = val();
        } else if (a == "--net") {
            net = true;
        } else if (a == "--help" || a == "-h") {
            std::fprintf(stderr,
                         "usage: panorama_tui [--cols N] [--rows N] [--animate N] "
                         "[--export-dir DIR] [--net]\n");
            return 0;
        } else {
            std::fprintf(stderr, "未知参数 %s\n", a.c_str());
            return 2;
        }
    }
    cols = std::clamp<std::uint32_t>(cols, 60, 400);
    rows = std::clamp<std::uint32_t>(rows, 24, 200);

    showcase::Panorama p = showcase::build(20261022u);
    std::size_t kde_cells = 0;
    double kde_max = 0.0;
    kde_bitmap(p, 96, 72, kde_cells, kde_max);
    p.kde_cells = kde_cells;
    p.kde_max = kde_max;
    p.csv_rows = showcase::csv_roundtrip_rows(p);
    if (p.csv_rows == 0) {
        std::fprintf(stderr, "CSV 往返校验失败\n");
        return 1;
    }

    tc::render::Renderer renderer(tc::ColorDepth::TrueColor);

    if (animate > 0 && tc::term::Terminal::stdout_is_tty()) {
        tc::term::Terminal term{tc::term::TerminalOptions{.raw_mode = true, .alt_screen = true,
                                                          .hide_cursor = true}};
        if (term.enter() != tc::ErrorCode::Ok) {
            std::fprintf(stderr, "无法进入终端 raw 模式\n");
            return 1;
        }
        for (int f = 0; f < animate; ++f) {
            const double t = static_cast<double>(f);
            CellBuffer poster = compose_poster(p, cols, rows, 0.30 + 0.02 * t, 0.20 + 0.013 * t);
            term.write(renderer.render(poster));
            term.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(60));  // NOLINT
        }
        term.leave();
    } else {
        const CellBuffer poster = compose_poster(p, cols, rows, 0.30, 0.20);
        const std::string ansi = renderer.render_full(poster);
        tc::api::write_stdout(ansi);
        tc::api::flush_stdout();
    }

    if (!export_dir.empty()) do_exports(p, export_dir);
    if (net) net_probe();
    return 0;
}
