// test_backend_snapshot.cpp —— 快照物种四后端（ansi/text/html_pre/json）
#include <doctest.h>

#include <string>

#include "termchart/backend/snapshot/ansi_backend.hpp"
#include "termchart/backend/snapshot/html_pre_backend.hpp"
#include "termchart/backend/snapshot/json_backend.hpp"
#include "termchart/backend/snapshot/text_backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/core/style.hpp"

namespace {

using namespace termchart;
using namespace termchart::backend::snapshot;

// 3×2 缓冲：第一行 "AB "（A 红色），第二行 "你"+续接格。
CellBuffer make_buffer() {
    CellBuffer buf(3, 2);
    buf.at(0, 0) = Cell{U'A', Style{}.set_fg(Color::ansi16(1))};
    buf.at(1, 0) = Cell{U'B', Style{}};
    buf.at(2, 0) = Cell{U' ', Style{}};
    buf.at(0, 1) = Cell{U'\u4F60', Style{}};  // 你（宽字符）
    buf.at(1, 1) = Cell{kContinuationCp, Style{}};
    buf.at(2, 1) = Cell{U' ', Style{}};
    return buf;
}

}  // namespace

TEST_CASE("backend/snapshot: text 后端裁尾空白并跳过续接格") {
    const CellBuffer buf = make_buffer();
    TextBackend backend;
    const std::string out = render_snapshot(backend, buf);
    CHECK(out == "AB\n\u4F60");  // 续接格不产字节；行尾空白裁掉
}

TEST_CASE("backend/snapshot: ansi 后端复用 sgr 且行尾复位") {
    const CellBuffer buf = make_buffer();
    AnsiBackend backend;
    backend.set_options(SnapshotOptions{.depth = ColorDepth::Ansi16});
    const std::string out = render_snapshot(backend, buf);

    CHECK(out.find('A') != std::string::npos);
    CHECK(out.find('B') != std::string::npos);
    CHECK(out.find("\x1b[31;49m") != std::string::npos);  // ansi16(1) → 31;49
    CHECK(out.find(std::string("\x1b[0m")) != std::string::npos);  // 复位
    CHECK(out.find("\u4F60") != std::string::npos);
}

TEST_CASE("backend/snapshot: html_pre 包装并转义") {
    CellBuffer buf(1, 1);
    buf.at(0, 0) = Cell{U'<'};
    HtmlPreBackend backend;
    const std::string out = render_snapshot(backend, buf);
    CHECK(out == "<pre>&lt;</pre>");
}

TEST_CASE("backend/snapshot: json 后端结构化序列化") {
    const CellBuffer buf = make_buffer();
    JsonBackend backend;
    const std::string out = render_snapshot(backend, buf);

    CHECK(out.find("\"width\":3") != std::string::npos);
    CHECK(out.find("\"height\":2") != std::string::npos);
    CHECK(out.find("\"cp\":65") != std::string::npos);                    // 'A'
    CHECK(out.find("\"kind\":\"ansi16\",\"index\":1") != std::string::npos);
    CHECK(out.find("null") != std::string::npos);                         // 续接格
}

TEST_CASE("backend/snapshot: begin 可重入") {
    const CellBuffer buf = make_buffer();
    TextBackend backend;
    const std::string first = render_snapshot(backend, buf);
    const std::string second = render_snapshot(backend, buf);
    CHECK(first == second);
}
