// quick.cpp —— 总入口实现
//
// 完整链路（阶段 1 的端到端切片）：
//   text + Style → CellBuffer → Renderer → stdout
// 这里刻意**只做编排**：不含任何 SGR 拼接、不含任何平台判断，
// 那些分别在 render/sgr.hpp 与 term/ 里。
#include "termchart/api/quick.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/bar_chart.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/cartesian/scatter_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/logging.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::api {
namespace {

std::once_flag g_init_flag;
std::atomic<bool> g_initialized{false};

void do_init_core() noexcept {
    util::set_log_level(util::LogLevel::Warn);  // 默认安静：库不该刷屏
    g_initialized.store(true, std::memory_order_release);
}

}  // namespace

void init_core() noexcept {
    std::call_once(g_init_flag, do_init_core);
}

bool core_initialized() noexcept { return g_initialized.load(std::memory_order_acquire); }

Result<std::string> render_line(std::string_view text, const Style& style,
                                const Opts& opts) noexcept {
    init_core();

    const std::uint32_t width =
        opts.width().has_value() ? static_cast<std::uint32_t>(*opts.width()) : 0;
    if (opts.width().has_value() && *opts.width() == 0) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    const std::uint32_t height =
        opts.height().has_value() ? static_cast<std::uint32_t>(*opts.height()) : 1;

    // 宽度未指定时用文本宽度（至少 1），高度固定 1 行——这是"一行出图"。
    const int text_width = util::display_width(text);
    const std::uint32_t columns = width != 0 ? width : static_cast<std::uint32_t>(text_width > 0 ? text_width : 1);

    CellBuffer buffer(columns, height);
    std::size_t column = 0;
    std::size_t pos = 0;
    while (pos < text.size() && column < columns) {
        const std::size_t step = util::next_boundary(text, pos);
        if (step == 0) {
            break;
        }
        const util::Utf8Decode head = util::decode(text, pos);
        if (head.consumed == 0) {
            break;
        }
        const int cluster_width = util::codepoint_width(head.cp);
        if (cluster_width == 0) {
            // 零宽簇（组合记号）挂到前一格的侧面表上。
            if (column > 0) {
                const std::size_t index = buffer.index_of(static_cast<std::uint32_t>(column - 1), 0);
                char32_t sides[termchart::kMaxSideBearings]{};
                std::size_t count = 0;
                std::size_t inner = pos;
                while (inner < pos + step && count < termchart::kMaxSideBearings) {
                    const util::Utf8Decode d = util::decode(text, inner);
                    if (d.consumed == 0) {
                        break;
                    }
                    sides[count++] = d.cp;
                    inner += static_cast<std::size_t>(d.consumed);
                }
                buffer.set_sides(index, sides, count);
            }
            pos += step;
            continue;
        }
        if (static_cast<int>(column) + cluster_width > static_cast<int>(columns)) {
            break;  // 放不下就截断：渲染路径不抛异常
        }
        buffer.at(static_cast<std::uint32_t>(column), 0) = Cell(head.cp, style);
        if (cluster_width == 2 && column + 1 < columns) {
            // 宽字符：右侧一格是续接占位，渲染器不写它（否则会覆盖字形右半边）。
            buffer.at(static_cast<std::uint32_t>(column + 1), 0) = Cell(kContinuationCp, style);
        }
        column += static_cast<std::size_t>(cluster_width);
        pos += step;
    }

    const ColorDepth depth =
        opts.color_depth().has_value() ? *opts.color_depth() : ColorDepth::TrueColor;
    render::Renderer renderer(depth);
    std::string out = renderer.render_full(buffer);
    if (opts.newline()) {
        out.push_back('\n');
    }
    return out;
}

Status print_line(std::string_view text, const Style& style, const Opts& opts) noexcept {
    const Result<std::string> rendered = render_line(text, style, opts);
    if (!rendered.has_value()) {
        return rendered.error();
    }
    return write_stdout(*rendered);
}

Status hello(const Opts& opts) noexcept {
    init_core();
    Style style;
    style.set_fg(Color::rgb(86, 197, 154)).set_attr(Attr::Bold);
    const Status first = print_line("termchart ready", style, opts);
    if (first != ErrorCode::Ok) {
        return first;
    }
    Style hint;
    hint.set_fg(Color::rgb(143, 165, 190));
    const Status second = print_line("v0.1 — 终端上色闭环已打通", hint, opts);
    return second != ErrorCode::Ok ? second : flush_stdout();
}

namespace {

// (x[i], y[i]) → Series 数据点；长度取两者较小者。
std::vector<Point> to_points(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t n = std::min(x.size(), y.size());
    std::vector<Point> pts;
    pts.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        pts.push_back(Point{x[i], y[i]});
    }
    return pts;
}

// 一行出图的公共编排：Chart → Canvas → Renderer → 字节流（不写 stdout）。
// 只做接线：不含 SGR、不含平台判断（各自在 render/sgr 与 term/）。
Result<std::string> render_chart_bytes(std::unique_ptr<chart::Chart> chart,
                                       const PlotOpts& opts) noexcept {
    init_core();
    if (opts.width == 0 || opts.height == 0) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    if (!opts.title.empty()) {
        chart->set_title(opts.title);
    }
    canvas::Canvas canvas(opts.width, opts.height);
    chart->draw({canvas});
    render::Renderer renderer(opts.depth);
    return renderer.render_full(canvas.to_buffer());
}

Status print_rendered(const Result<std::string>& rendered) noexcept {
    if (!rendered.has_value()) {
        return rendered.error();
    }
    return write_stdout(*rendered);
}

}  // namespace

Result<std::string> render_plot(const std::vector<double>& x, const std::vector<double>& y,
                                const PlotOpts& opts) noexcept {
    auto chart = std::make_unique<chart::LineChart>();
    chart->set_show_legend(opts.show_legend);
    chart::Series s;
    s.name = "y";
    s.color = chart::default_color(chart::DefaultColor::Neutral);
    s.points = to_points(x, y);
    chart->add_series(std::move(s));
    return render_chart_bytes(std::move(chart), opts);
}

Status plot(const std::vector<double>& x, const std::vector<double>& y,
            const PlotOpts& opts) noexcept {
    return print_rendered(render_plot(x, y, opts));
}

Result<std::string> render_scatter(const std::vector<double>& x, const std::vector<double>& y,
                                   const PlotOpts& opts) noexcept {
    auto chart = std::make_unique<chart::ScatterChart>();
    chart->set_show_legend(opts.show_legend);
    chart::Series s;
    s.name = "y";
    s.color = chart::default_color(chart::DefaultColor::Accent);
    s.points = to_points(x, y);
    chart->add_series(std::move(s));
    return render_chart_bytes(std::move(chart), opts);
}

Status scatter(const std::vector<double>& x, const std::vector<double>& y,
               const PlotOpts& opts) noexcept {
    return print_rendered(render_scatter(x, y, opts));
}

Result<std::string> render_bar(const std::vector<double>& values,
                               const std::vector<std::string>& labels,
                               const PlotOpts& opts) noexcept {
    auto chart = std::make_unique<chart::BarChart>();
    chart->set_show_legend(opts.show_legend);
    std::vector<std::string> names = labels;
    if (names.size() < values.size()) {  // 缺省按序号补标签
        names.reserve(values.size());
        for (std::size_t i = names.size(); i < values.size(); ++i) {
            names.push_back(std::to_string(i));
        }
    }
    chart->set_values(values, names, chart::default_color(chart::DefaultColor::Accent),
                      "series");
    return render_chart_bytes(std::move(chart), opts);
}

Status bar(const std::vector<double>& values, const std::vector<std::string>& labels,
           const PlotOpts& opts) noexcept {
    return print_rendered(render_bar(values, labels, opts));
}

}  // namespace termchart::api
