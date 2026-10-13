// chart.cpp —— 一行出图的实现（见 include/termchart/api/chart.hpp）
//
// 刻意**不**探测终端尺寸：同一个 Chart 在 CI（无 tty）与真终端里应产出同样的字节，
// 否则 golden 与文档示例都无法复现。要自适应，调用方自己传 opts.cols/rows。
#include "termchart/api/chart.hpp"

#include <cstdint>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/render/renderer.hpp"

namespace termchart::api {
namespace {

// 默认画布：80×24 是终端的经典下限，也是 examples/03 用的尺寸。
constexpr std::uint32_t kDefaultCols = 80;
constexpr std::uint32_t kDefaultRows = 24;

}  // namespace

Result<std::string> render_chart(chart::Chart& chart, const Opts& opts) noexcept {
    // 显式给 0 是参数错误，不猜也不钳位——与 quick::render_line 的口径一致。
    if (opts.width() == 0 || opts.height() == 0) {
        return Result<std::string>(Unexpected<ErrorCode>(ErrorCode::InvalidArgument));
    }
    const std::uint32_t cols = static_cast<std::uint32_t>(opts.width().value_or(kDefaultCols));
    const std::uint32_t rows = static_cast<std::uint32_t>(opts.height().value_or(kDefaultRows));

    canvas::Canvas canvas(cols, rows);
    chart::DrawContext ctx{canvas};
    chart.draw(ctx);

    // 每次都新建 Renderer：一行出图是一次性动作，跨调用保留 prev 只会让第二张图
    // 被"差分"成空白（第一张已经画过同样的格子）。
    render::Renderer renderer(opts.color_depth().value_or(ColorDepth::TrueColor));
    return renderer.render_full(canvas.to_buffer());
}

Status print_chart(chart::Chart& chart, const Opts& opts) noexcept {
    const auto bytes = render_chart(chart, opts);
    if (!bytes.has_value()) {
        return bytes.error();
    }
    const Status st = write_stdout(*bytes);
    flush_stdout();
    return st;
}

}  // namespace termchart::api
