// test_backend_extra.cpp —— L16 backend 补充后端：latex（TikZ）与 png_screenshot
#include <doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "termchart/backend/snapshot/png_screenshot.hpp"
#include "termchart/backend/vector/latex_backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"
#include "termchart/media/image/png.hpp"

namespace {

using namespace termchart;
namespace backend = ::termchart::backend;

std::size_t count_of(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = text.find(needle);
    while (pos != std::string::npos) {
        ++count;
        pos = text.find(needle, pos + needle.size());
    }
    return count;
}

std::uint32_t be32(const std::string& bytes, std::size_t pos) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[pos])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[pos + 1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[pos + 2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[pos + 3]));
}

// 4×2：第一行 "AB  "（A 红色），第二行 " C█"（█ 一格实心块）。
CellBuffer make_buffer() {
    CellBuffer buf(4, 2);
    buf.at(0, 0) = Cell{U'A', Style{}.set_fg(Color::rgb(255, 0, 0))};
    buf.at(1, 0) = Cell{U'B', Style{}};
    buf.at(2, 0) = Cell{U' ', Style{}};
    buf.at(3, 0) = Cell{U' ', Style{}};
    buf.at(0, 1) = Cell{U' ', Style{}};
    buf.at(1, 1) = Cell{U'C', Style{}};
    buf.at(2, 1) = Cell{U' ', Style{}};
    buf.at(3, 1) = Cell{U'\u2588', Style{}};
    return buf;
}

}  // namespace

TEST_CASE("backend/vector/latex: 节点数与非空格数对应") {
    const CellBuffer buf = make_buffer();
    backend::vector::LatexBackend latex;
    const std::string out = backend::vector::render_latex(latex, buf);

    // A、B、C 三格 → 3 个 \node；空格不产节点（█ 走 \fill）。
    CHECK(count_of(out, "\\node") == 3);
    CHECK(out.find("{A}") != std::string::npos);
    CHECK(out.find("{B}") != std::string::npos);
    CHECK(out.find("{C}") != std::string::npos);
    CHECK(out.find("\\fill") != std::string::npos);  // █ → 实心矩形
}

TEST_CASE("backend/vector/latex: 颜色以内联 xcolor 出现") {
    CellBuffer buf(2, 1);
    buf.at(0, 0) = Cell{U'A', Style{}.set_fg(Color::rgb(255, 0, 0))};
    buf.at(1, 0) = Cell{U'\u2500', Style{}.set_fg(Color::rgb(0, 0, 255))};

    backend::vector::LatexBackend latex;
    const std::string out = backend::vector::render_latex(latex, buf);

    // 格点 → text 键；直线 → draw 键，同一个 {rgb,255} 模型。
    CHECK(out.find("text={rgb,255:red,255;green,0;blue,0}") != std::string::npos);
    CHECK(out.find("draw={rgb,255:red,0;green,0;blue,255}") != std::string::npos);
    CHECK(out.find("\\draw") != std::string::npos);
}

TEST_CASE("backend/vector/latex: 空 buffer 输出最小文档") {
    const CellBuffer buf(0, 0);
    backend::vector::LatexBackend latex;
    const std::string out = backend::vector::render_latex(latex, buf);

    CHECK(out.find("\\begin{tikzpicture}") != std::string::npos);
    CHECK(out.find("\\end{tikzpicture}") != std::string::npos);
    CHECK(out.find("\\begin{document}") != std::string::npos);
    CHECK(out.find("\\end{document}") != std::string::npos);
    CHECK(count_of(out, "\\node") == 0);

    // standalone=false：裸 tikzpicture，不带 documentclass/document。
    backend::vector::LatexOptions opts;
    opts.standalone = false;
    latex.set_options(opts);
    const std::string bare = backend::vector::render_latex(latex, buf);
    CHECK(bare.find("documentclass") == std::string::npos);
    CHECK(bare.find("\\begin{tikzpicture}") != std::string::npos);
}

TEST_CASE("backend/snapshot/png_screenshot: PNG 魔数与 IHDR 尺寸字段") {
    const CellBuffer buf = make_buffer();  // 4×2 格，默认 8×16 → 32×32 像素
    const std::string png = backend::snapshot::render_png_screenshot(buf);

    REQUIRE(png.size() >= 24);
    CHECK(static_cast<unsigned char>(png[0]) == 0x89);
    CHECK(png.compare(1, 3, "PNG") == 0);
    CHECK(static_cast<unsigned char>(png[4]) == 0x0D);  // \r
    CHECK(static_cast<unsigned char>(png[5]) == 0x0A);  // \n
    CHECK(static_cast<unsigned char>(png[6]) == 0x1A);
    CHECK(static_cast<unsigned char>(png[7]) == 0x0A);

    // IHDR 的宽高为 big-endian uint32：宽 = 4*8 = 32，高 = 2*16 = 32。
    CHECK(png.compare(12, 4, "IHDR") == 0);
    CHECK(be32(png, 16) == 32);
    CHECK(be32(png, 20) == 32);

    // 自家解码器能完整读回，尺寸一致。
    const Result<image::Bitmap> decoded = image::decode_png(png);
    REQUIRE(decoded.has_value());
    CHECK(decoded.value().width() == 32);
    CHECK(decoded.value().height() == 32);
}

TEST_CASE("backend/snapshot/png_screenshot: 空 buffer 退化为空串且可重入") {
    const CellBuffer empty(0, 0);
    const std::string png = backend::snapshot::render_png_screenshot(empty);
    CHECK(png.empty());

    // 非 empty buffer 走一遍后，再对空 buffer 重入，仍干净退化。
    backend::snapshot::PngScreenshotBackend backend;
    const CellBuffer buf = make_buffer();
    backend.begin();
    backend.render(buf);
    backend.end();
    CHECK(!backend.str().empty());

    backend.begin();
    backend.render(empty);
    backend.end();
    CHECK(backend.str().empty());

    // set_cell_size 的 0 被钳到 1，不产生零尺寸位图。
    backend.set_cell_size(0, 0);
    CHECK(backend.cell_width() == 1);
    CHECK(backend.cell_height() == 1);
}
