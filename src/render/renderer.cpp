// renderer.cpp —— 增量渲染实现
//
// 输出策略（决定字节数的三件事）：
//   1. 只在变化段上写；段间用最短跳转序列（term/ansi.hpp 的 append_move）。
//   2. 同段内样式相同的连续单元合并成一次 SGR + 一段文本。
//   3. 帧尾复位：终端状态必须回到"默认"，否则下个程序的输出会被染色。
#include "termchart/render/renderer.hpp"

#include <string>

#include "termchart/detail/ansi_tables.hpp"
#include "termchart/render/diff.hpp"
#include "termchart/render/sgr.hpp"
#include "termchart/term/ansi.hpp"
#include "termchart/util/assert.hpp"
#include "termchart/util/utf8.hpp"

namespace termchart::render {
namespace {

bool non_default_style(const Style& style) noexcept {
    return style.attrs() != 0 || !style.fg().is_none() || !style.bg().is_none();
}

}  // namespace

class Renderer::Impl {
public:
    explicit Impl(ColorDepth initial_depth) : depth(initial_depth) {}

    ColorDepth depth = ColorDepth::TrueColor;
    CellBuffer previous;
    bool has_previous = false;
    DamageTracker damage;
    Style current_style;      // 终端当前所处的样式（增量输出依赖它）
    bool styled = false;      // 是否已经发过非默认样式
    int cursor_row = 1;       // 1-based，与 ANSI 一致
    int cursor_col = 1;

    void emit_cell_text(std::string& out, const CellBuffer& buffer, const Cell& cell,
                        std::size_t index) noexcept {
        if (is_continuation(cell)) {
            return;  // 宽字符续接格：不写字节，光标由宽字形本身推进两列
        }
        util::append(out, cell.cp);
        // 侧面表（组合记号）：主码点之后再写，宽度 0。
        if (const auto* sides = buffer.sides(index)) {
            for (char32_t cp : *sides) {
                if (cp != U'\0') {
                    util::append(out, cp);
                }
            }
        }
    }

    void move_to(std::string& out, int row, int col) noexcept {
        term::ansi::append_move(out, cursor_row, cursor_col, row, col);
        cursor_row = row;
        cursor_col = col;
    }

    std::string render_full(const CellBuffer& buffer) noexcept {
        std::string out;
        damage.resize(buffer.width(), buffer.height());
        damage.clear();
        current_style = Style{};
        styled = false;
        cursor_row = 1;
        cursor_col = 1;
        move_to(out, 1, 1);

        const std::size_t width = buffer.width();
        for (std::size_t row = 0; row < buffer.height(); ++row) {
            const std::size_t base = row * width;
            for (std::size_t col = 0; col < width; ++col) {
                const Cell& cell = buffer.cells()[base + col];
                if (!(cell.style == current_style)) {
                    sgr::append_style_delta(out, current_style, cell.style, depth);
                    current_style = cell.style;
                    // styled 是**粘性**标记：只要本帧发过任何非默认样式，帧尾就必须复位。
                    if (non_default_style(cell.style)) {
                        styled = true;
                    }
                }
                emit_cell_text(out, buffer, cell, base + col);
            }
            cursor_col += static_cast<int>(width);
            damage.mark_run(static_cast<std::uint32_t>(row), 0,
                            static_cast<std::uint32_t>(width));
            if (row + 1 < buffer.height()) {
                out.push_back('\r');
                out.push_back('\n');
                cursor_row = static_cast<int>(row) + 2;
                cursor_col = 1;
            }
        }
        finish(out);
        previous = buffer;
        has_previous = true;
        return out;
    }

    std::string render_incremental(const CellBuffer& buffer) noexcept {
        std::string out;
        damage.resize(buffer.width(), buffer.height());
        damage.clear();
        const std::vector<DiffRegion> regions = diff(previous, buffer);
        if (regions.empty()) {
            previous = buffer;
            return out;
        }
        const std::size_t width = buffer.width();
        for (const DiffRegion& region : regions) {
            for (std::uint32_t col = region.begin; col < region.end;) {
                const std::size_t index = static_cast<std::size_t>(region.row) * width + col;
                const Cell& cell = buffer.cells()[index];
                std::uint32_t run_end = col + 1;
                while (run_end < region.end) {
                    const Cell& next = buffer.cells()[static_cast<std::size_t>(region.row) * width +
                                                       run_end];
                    if (!(next.style == cell.style)) {
                        break;
                    }
                    ++run_end;
                }
                move_to(out, static_cast<int>(region.row) + 1, static_cast<int>(col) + 1);
                if (!(cell.style == current_style)) {
                    sgr::append_style_delta(out, current_style, cell.style, depth);
                    current_style = cell.style;
                    if (non_default_style(cell.style)) {
                        styled = true;
                    }
                }
                for (std::uint32_t i = col; i < run_end; ++i) {
                    const std::size_t cell_index =
                        static_cast<std::size_t>(region.row) * width + i;
                    emit_cell_text(out, buffer, buffer.cells()[cell_index], cell_index);
                }
                cursor_col = static_cast<int>(run_end) + 1;
                damage.mark_run(region.row, col, run_end);
                col = run_end;
            }
        }
        finish(out);
        previous = buffer;
        return out;
    }

    void finish(std::string& out) noexcept {
        if (styled) {
            out += sgr::reset();
            current_style = Style{};
            styled = false;
        }
    }
};

// 析构、移动都必须在 Impl 完整的地方定义：unique_ptr<不完整类型> 的默认析构
// 需要完整类型，否则 static_assert 直接失败。
Renderer::Renderer(ColorDepth depth) noexcept
    : impl_(std::make_unique<Impl>(depth)), depth_(depth) {}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

void Renderer::set_depth(ColorDepth depth) noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    depth_ = depth;
    impl_->depth = depth;
    reset();  // 深度变化后必须全量重绘，否则旧 prev 与新输出不一致
}

std::string Renderer::render(const CellBuffer& current) noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    if (!impl_->has_previous || impl_->previous.width() != current.width() ||
        impl_->previous.height() != current.height()) {
        return render_full(current);
    }
    return impl_->render_incremental(current);
}

std::string Renderer::render_full(const CellBuffer& current) noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    return impl_->render_full(current);
}

void Renderer::reset() noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    impl_->has_previous = false;
    impl_->previous = CellBuffer{};
    impl_->damage.clear();
    impl_->current_style = Style{};
    impl_->styled = false;
}

const CellBuffer* Renderer::previous() const noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    return impl_->has_previous ? &impl_->previous : nullptr;
}

const DamageTracker& Renderer::damage() const noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    return impl_->damage;
}

DamageTracker& Renderer::damage() noexcept {
    TERMCHART_ASSERT_MSG(impl_ != nullptr, "Renderer 已被移出，只剩析构/重赋值可用");
    return impl_->damage;
}

}  // namespace termchart::render
