// chart.cpp —— 一行出图的实现（见 include/termchart/api/chart.hpp）
//
// 刻意**不**探测终端尺寸：同一个 Chart 在 CI（无 tty）与真终端里应产出同样的字节，
// 否则 golden 与文档示例都无法复现。要自适应，调用方自己传 opts.cols/rows。
#include "termchart/api/chart.hpp"

#include <cstdint>

#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/svg_backend.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/context.hpp"
#include "termchart/chart/scene_animation.hpp"
#include "termchart/render/renderer.hpp"

namespace termchart::api {
namespace {

// 默认画布：80×24 是终端的经典下限，也是 examples/03 用的尺寸。
constexpr std::uint32_t kDefaultCols = 80;
constexpr std::uint32_t kDefaultRows = 24;

}  // namespace

Result<std::string> render_chart(chart::Chart& chart, const ApiOptions& opts) noexcept {
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

Status print_chart(chart::Chart& chart, const ApiOptions& opts) noexcept {
    const auto bytes = render_chart(chart, opts);
    if (!bytes.has_value()) {
        return bytes.error();
    }
    const Status st = write_stdout(*bytes);
    flush_stdout();
    return st;
}

Result<scene::Scene> chart_to_scene(const chart::Chart& chart, std::uint32_t width,
                                    std::uint32_t height) noexcept {
    if (width == 0 || height == 0) {
        return Result<scene::Scene>(Unexpected<ErrorCode>(ErrorCode::InvalidArgument));
    }
    scene::Scene out;
    if (!chart.to_scene(out, width, height)) {
        // 不静默回退成 Canvas 转录：调用方要的是几何，给他一份假 Scene 比报错更糟。
        return Result<scene::Scene>(Unexpected<ErrorCode>(ErrorCode::Unsupported));
    }
    // 动画是**后处理**：to_scene 出完整几何，这里按图种给出的组进度统一施加。
    // 没登记动画的图种拿到空表 → apply_animation 直接返回（零开销、零差异）。
    std::vector<chart::GroupProgress> groups;
    chart.animation_groups(groups);
    chart::apply_animation(out, groups);
    return out;
}

Result<std::string> chart_to_svg(const chart::Chart& chart, std::uint32_t width,
                                 std::uint32_t height,
                                 const backend::vector::VectorOptions& options) noexcept {
    const auto sc = chart_to_scene(chart, width, height);
    if (!sc.has_value()) {
        return Result<std::string>(Unexpected<ErrorCode>(sc.error()));
    }
    // 图表 Scene 以字符格为场景单位：字号不设 1.0 会被 scale 放大 12 倍。
    backend::vector::VectorOptions opts = options;
    if (opts.font_size == 0.0) {
        opts.font_size = 1.0;
    }
    backend::vector::SvgBackend svg;
    svg.set_options(opts);
    return backend::vector::render_vector(svg, *sc);
}

}  // namespace termchart::api
