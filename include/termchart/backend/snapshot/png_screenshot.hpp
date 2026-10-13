// backend/snapshot/png_screenshot.hpp —— CellBuffer → PNG 截图
//
// 复用 image/png_encode 的 encode_png（零依赖 stored deflate），不重复实现
// PNG 编码。每个字符格映射为 cell_w×cell_h 的像素块：整块先填背景色
// （Style 未设色用 ResolveOptions 的默认背景），非空格再整块覆盖前景色。
// 前景是"块状示意"而非字形光栅化——逐字形渲染需要字体栅格化器，超出
// 本后端职责（要带字形请走 protocol/ 的 sixel/kitty 路径）。
//
// 注：继承 SnapshotOptions 里的 depth / trim_trailing_blank 对 PNG 无意义，
// 保留只是为了共用快照物种的生命周期与 render_snapshot 便捷入口。
#ifndef TERMCHART_BACKEND_SNAPSHOT_PNG_SCREENSHOT_HPP
#define TERMCHART_BACKEND_SNAPSHOT_PNG_SCREENSHOT_HPP

#include <cstdint>
#include <string>

#include "termchart/backend/snapshot/backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/export.hpp"

namespace termchart::backend::snapshot {

class PngScreenshotBackend final : public SnapshotBackend {
public:
    // 每格像素尺寸；0 按 1 处理（0×0 只由空 buffer 的退化路径产生）。
    void set_cell_size(std::uint32_t w, std::uint32_t h) noexcept;
    [[nodiscard]] std::uint32_t cell_width() const noexcept { return cell_w_; }
    [[nodiscard]] std::uint32_t cell_height() const noexcept { return cell_h_; }

    void render(const CellBuffer& buffer) override;

private:
    std::uint32_t cell_w_ = 8;
    std::uint32_t cell_h_ = 16;
};

// 便捷入口：默认 8×16 每格，返回 PNG 字节串；空 buffer 返回空串。
[[nodiscard]] TERMCHART_API std::string render_png_screenshot(
    const CellBuffer& buffer, std::uint32_t cell_w = 8, std::uint32_t cell_h = 16);

}  // namespace termchart::backend::snapshot

#endif  // TERMCHART_BACKEND_SNAPSHOT_PNG_SCREENSHOT_HPP
