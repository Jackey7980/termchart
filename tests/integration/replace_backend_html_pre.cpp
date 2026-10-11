// replace_backend_html_pre.cpp —— 可扩展性实验 A（ROADMAP 阶段 3 契约验证点）
//
// 命题：**新增一个后端不需要改库任何一行**——只要实现 backend/snapshot 的
// SnapshotBackend 接口即可。这里"替换"内置 html_pre 后端，加一个 CSS class。
#include <doctest.h>

#include <cstdint>
#include <string>

#include "termchart/backend/snapshot/backend.hpp"
#include "termchart/core/buffer.hpp"
#include "termchart/core/cell.hpp"
#include "termchart/util/utf8.hpp"

namespace {

using namespace termchart;

// 自定义后端：实现 SnapshotBackend 接口即可接入，无任何库内改动。
class AttributedHtmlPreBackend final : public backend::snapshot::SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override {
        out_.clear();
        out_ += "<pre class=\"termchart\">";
        for (std::uint32_t y = 0; y < buffer.height(); ++y) {
            const std::uint32_t w = row_width(buffer, y);
            for (std::uint32_t x = 0; x < w; ++x) {
                const Cell& cell = buffer.at(x, y);
                if (is_continuation(cell)) {
                    continue;
                }
                switch (cell.cp) {
                    case U'<':
                        out_ += "&lt;";
                        break;
                    case U'>':
                        out_ += "&gt;";
                        break;
                    case U'&':
                        out_ += "&amp;";
                        break;
                    default:
                        util::append(out_, cell.cp);
                        break;
                }
            }
            if (y + 1 < buffer.height()) {
                out_ += '\n';
            }
        }
        out_ += "</pre>";
    }
};

}  // namespace

TEST_CASE("replace/backend: 自定义快照后端零核心改动接入") {
    CellBuffer buffer(2, 1);
    buffer.at(0, 0) = Cell{U'<'};
    buffer.at(1, 0) = Cell{U'a'};

    AttributedHtmlPreBackend backend;
    const std::string out = backend::snapshot::render_snapshot(backend, buffer);
    CHECK(out == "<pre class=\"termchart\">&lt;a</pre>");
}
