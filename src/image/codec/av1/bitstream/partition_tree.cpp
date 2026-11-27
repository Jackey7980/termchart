#include "partition_tree.hpp"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "../entropy/cdf_manager.hpp"

namespace termchart::image::codec::av1 {
namespace {

constexpr int kTop = 32768;

int lookup_above(int w_log2, int h_log2) {
    struct Entry { int w, h, above, left; };
    static constexpr Entry kTable[] = {
        {2, 2, 31, 31}, {2, 3, 31, 30}, {3, 2, 30, 31}, {3, 3, 30, 30},
        {3, 4, 30, 28}, {4, 3, 28, 30}, {4, 4, 28, 28}, {4, 5, 28, 24},
        {5, 4, 24, 28}, {5, 5, 24, 24}, {5, 6, 24, 16}, {6, 5, 16, 24},
        {6, 6, 16, 16}, {6, 7, 16, 0},  {7, 6, 0, 16},  {7, 7, 0, 0},
        {2, 4, 31, 28}, {4, 2, 28, 31}, {3, 5, 30, 24}, {5, 3, 24, 30},
        {4, 6, 28, 16}, {6, 4, 16, 28},
    };
    for (const auto& e : kTable)
        if (e.w == w_log2 && e.h == h_log2) return e.above;
    return 31;
}

int lookup_left(int w_log2, int h_log2) {
    struct Entry { int w, h, above, left; };
    static constexpr Entry kTable[] = {
        {2, 2, 31, 31}, {2, 3, 31, 30}, {3, 2, 30, 31}, {3, 3, 30, 30},
        {3, 4, 30, 28}, {4, 3, 28, 30}, {4, 4, 28, 28}, {4, 5, 28, 24},
        {5, 4, 24, 28}, {5, 5, 24, 24}, {5, 6, 24, 16}, {6, 5, 16, 24},
        {6, 6, 16, 16}, {6, 7, 16, 0},  {7, 6, 0, 16},  {7, 7, 0, 0},
        {2, 4, 31, 28}, {4, 2, 28, 31}, {3, 5, 30, 24}, {5, 3, 24, 30},
        {4, 6, 28, 16}, {6, 4, 16, 28},
    };
    for (const auto& e : kTable)
        if (e.w == w_log2 && e.h == h_log2) return e.left;
    return 31;
}

int prob(const std::uint16_t* cdf, int symbol) {
    return (symbol > 0 ? cdf[symbol - 1] : kTop) - cdf[symbol];
}

std::array<std::uint16_t, 2> gather(const std::uint16_t* cdf,
                                    std::initializer_list<int> symbols,
                                    bool include_4) {
    int acc = kTop;
    for (int s : symbols)
        if (include_4 || s < 8) acc -= prob(cdf, s);
    return {static_cast<std::uint16_t>(acc), 0};
}

std::array<std::uint16_t, 2> gather_horz_alike(const std::uint16_t* cdf,
                                               int size_log2) {
    return gather(cdf, {1, 3, 4, 5, 6, 8}, size_log2 != 7);
}

std::array<std::uint16_t, 2> gather_vert_alike(const std::uint16_t* cdf,
                                               int size_log2) {
    return gather(cdf, {2, 3, 4, 6, 7, 9}, size_log2 != 7);
}

int symbol_length(int size_log2) {
    if (size_log2 <= 3) return 4;
    if (size_log2 == 7) return 8;
    return 10;
}

PartitionType clamp_type(PartitionType wanted, int size_log2,
                         bool has_rows, bool has_cols) {
    if (!has_rows && !has_cols) return PartitionType::Split;
    if (size_log2 <= 3) return PartitionType::None;
    if (!has_rows) {
        return (wanted == PartitionType::Horizontal ||
                wanted == PartitionType::Split)
                   ? wanted
                   : PartitionType::Split;
    }
    if (!has_cols) {
        return (wanted == PartitionType::Vertical ||
                wanted == PartitionType::Split)
                   ? wanted
                   : PartitionType::Split;
    }
    const unsigned s = static_cast<unsigned>(wanted);
    if (s >= static_cast<unsigned>(symbol_length(size_log2)))
        return PartitionType::Split;
    return wanted;
}

std::vector<PartitionNode> make_children(const PartitionNode& node) {
    std::vector<PartitionNode> out;
    const int r = node.row, c = node.col, w = node.w_log2, h = node.h_log2;
    const auto add = [&](int rr, int cc, int ww, int hh) {
        out.push_back(PartitionNode{rr, cc, ww, hh, PartitionType::None, {}});
    };
    switch (node.type) {
        case PartitionType::None:
            break;
        case PartitionType::Horizontal:
            add(r, c, w, h - 1);
            add(r + (1 << (h - 3)), c, w, h - 1);
            break;
        case PartitionType::Vertical:
            add(r, c, w - 1, h);
            add(r, c + (1 << (w - 3)), w - 1, h);
            break;
        case PartitionType::Split:
            add(r, c, w - 1, h - 1);
            add(r, c + (1 << (w - 3)), w - 1, h - 1);
            add(r + (1 << (h - 3)), c, w - 1, h - 1);
            add(r + (1 << (h - 3)), c + (1 << (w - 3)), w - 1, h - 1);
            break;
        case PartitionType::TopSplit:
            add(r, c, w - 1, h - 1);
            add(r, c + (1 << (w - 3)), w - 1, h - 1);
            add(r + (1 << (h - 3)), c, w, h - 1);
            break;
        case PartitionType::BottomSplit:
            add(r, c, w, h - 1);
            add(r + (1 << (h - 3)), c, w - 1, h - 1);
            add(r + (1 << (h - 3)), c + (1 << (w - 3)), w - 1, h - 1);
            break;
        case PartitionType::LeftSplit:
            add(r, c, w - 1, h);
            add(r, c + (1 << (w - 3)), w - 1, h - 1);
            add(r + (1 << (h - 3)), c + (1 << (w - 3)), w - 1, h - 1);
            break;
        case PartitionType::RightSplit:
            add(r, c, w - 1, h - 1);
            add(r + (1 << (h - 3)), c, w - 1, h - 1);
            add(r, c + (1 << (w - 3)), w - 1, h);
            break;
        case PartitionType::Horizontal4:
            for (int i = 0; i < 4; ++i)
                add(r + (i << (h - 4)), c, w, h - 2);
            break;
        case PartitionType::Vertical4:
            for (int i = 0; i < 4; ++i)
                add(r, c + (i << (w - 4)), w - 2, h);
            break;
    }
    return out;
}

}  // namespace

int PartitionContext::index(int mi_row, int mi_col, int size_log2) const {
    const int bsl = size_log2 - 3;
    if (mi_row < 0 || mi_col < 0 || mi_col >= frame_mi_cols ||
        mi_row >= frame_mi_rows || above.empty() || left.empty())
        return bsl * 4;
    const int a = (above[static_cast<std::size_t>(mi_col)] >> bsl) & 1;
    const int l = (left[static_cast<std::size_t>(mi_row)] >> bsl) & 1;
    return (l * 2 + a) + bsl * 4;
}

void PartitionContext::reset() {
    const std::size_t rows =
        static_cast<std::size_t>(frame_mi_rows < 1 ? 1 : frame_mi_rows);
    const std::size_t cols =
        static_cast<std::size_t>(frame_mi_cols < 1 ? 1 : frame_mi_cols);
    above.assign(rows > cols ? rows : cols, 0);
    left.assign(above.size(), 0);
}

void PartitionContext::update(int mi_row, int mi_col, int w_log2, int h_log2) {
    if (above.size() < static_cast<std::size_t>(frame_mi_cols) ||
        left.size() < static_cast<std::size_t>(frame_mi_rows))
        reset();
    const int wmi = 1 << (w_log2 - 3);
    const int hmi = 1 << (h_log2 - 3);
    const int a = lookup_above(w_log2, h_log2);
    const int l = lookup_left(w_log2, h_log2);
    for (int i = 0; i < wmi && mi_col + i < frame_mi_cols; ++i)
        above[static_cast<std::size_t>(mi_col + i)] = static_cast<std::uint8_t>(a);
    for (int i = 0; i < hmi && mi_row + i < frame_mi_rows; ++i)
        left[static_cast<std::size_t>(mi_row + i)] = static_cast<std::uint8_t>(l);
}

PartitionNode write_partition_tree(RangeEncoder& enc, PartitionCdf cdf,
                                   PartitionContext& ctx, int mi_row, int mi_col,
                                   int size_log2, const PartitionChooser& chooser,
                                   const PartitionLeafWriter& leaf_writer) {
    PartitionNode node{mi_row, mi_col, size_log2, size_log2,
                       PartitionType::None, {}};
    // 越过帧的块在解码端直接返回、不读任何符号。
    if (mi_row >= ctx.frame_mi_rows || mi_col >= ctx.frame_mi_cols) return node;
    if (size_log2 <= 3) {
        // 8x8 层：dav1d 总会读一次 4 符号 partition（NONE/H/V/SPLIT）。
        if (cdf) {
            const int idx = ctx.index(mi_row, mi_col, 3);
            write_symbol(enc, cdf[4][idx & 3], 4, 0);
        }
        ctx.update(mi_row, mi_col, size_log2, size_log2);
        if (leaf_writer) leaf_writer(node);
        return node;
    }
    const int hmi = 1 << (size_log2 - 3);
    const bool has_rows = mi_row + hmi < ctx.frame_mi_rows;
    const bool has_cols = mi_col + hmi < ctx.frame_mi_cols;
    const int idx = ctx.index(mi_row, mi_col, size_log2);
    // 每层（BL_128X128..BL_8X8）有独立的 partition 表；bl = 7 - size_log2。
    const int bl = std::clamp(7 - size_log2, 0, 4);
    const std::uint16_t* row = cdf[bl][idx & 3];
    const auto wanted = chooser(mi_row, mi_col, size_log2, size_log2,
                                has_rows, has_cols);
    node.type = clamp_type(wanted, size_log2, has_rows, has_cols);

    if (!has_rows && !has_cols) {
        // 块越过帧的两个方向：dav1d 不读符号，且只在同一个原点递归一层
        // （不是生成四个子块）。16x16 帧的 64/32 层就走这里。
        const PartitionNode child =
            write_partition_tree(enc, cdf, ctx, mi_row, mi_col,
                                 size_log2 - 1, chooser, leaf_writer);
        node.children.push_back(child);
        return node;
    } else if (!has_rows) {
        const auto c = gather_horz_alike(row, size_log2);
        const int sym = node.type == PartitionType::Horizontal ? 0 : 1;
        write_symbol(enc, c.data(), 2, sym);
    } else if (!has_cols) {
        const auto c = gather_vert_alike(row, size_log2);
        const int sym = node.type == PartitionType::Vertical ? 0 : 1;
        write_symbol(enc, c.data(), 2, sym);
    } else {
        write_symbol(enc, row, symbol_length(size_log2),
                     static_cast<int>(node.type));
    }

    auto children = make_children(node);
    for (auto& child : children) {
        if (child.w_log2 == child.h_log2 && child.w_log2 >= 3) {
            child = write_partition_tree(enc, cdf, ctx, child.row, child.col,
                                         child.w_log2, chooser, leaf_writer);
        } else {
            ctx.update(child.row, child.col, child.w_log2, child.h_log2);
        }
        node.children.push_back(std::move(child));
    }
    // PARTITION_NONE 且尺寸 >= 16x16：该节点本身就是编码块，不再有子划分符号。
    if (node.children.empty() && node.type == PartitionType::None &&
        size_log2 >= 4) {
        ctx.update(mi_row, mi_col, size_log2, size_log2);
        if (leaf_writer) leaf_writer(node);
    }
    return node;
}

PartitionNode read_partition_tree(RangeDecoder& dec, PartitionCdf cdf,
                                  PartitionContext& ctx, int mi_row, int mi_col,
                                  int size_log2) {
    PartitionNode node{mi_row, mi_col, size_log2, size_log2,
                       PartitionType::None, {}};
    if (size_log2 <= 3) {
        if (cdf) {
            const int idx = ctx.index(mi_row, mi_col, 3);
            node.type = static_cast<PartitionType>(
                read_symbol(dec, cdf[4][idx & 3], 4));
        }
        ctx.update(mi_row, mi_col, size_log2, size_log2);
        return node;
    }
    const int hmi = 1 << (size_log2 - 3);
    const bool has_rows = mi_row + hmi < ctx.frame_mi_rows;
    const bool has_cols = mi_col + hmi < ctx.frame_mi_cols;
    const int idx = ctx.index(mi_row, mi_col, size_log2);
    const int bl = std::clamp(7 - size_log2, 0, 4);
    const std::uint16_t* row = cdf[bl][idx & 3];

    if (!has_rows && !has_cols) {
        node.type = PartitionType::Split;
        const PartitionNode child =
            read_partition_tree(dec, cdf, ctx, mi_row, mi_col, size_log2 - 1);
        node.children.push_back(child);
        return node;
    } else if (!has_rows) {
        const auto c = gather_horz_alike(row, size_log2);
        node.type = read_symbol(dec, c.data(), 2) == 0
                        ? PartitionType::Horizontal
                        : PartitionType::Split;
    } else if (!has_cols) {
        const auto c = gather_vert_alike(row, size_log2);
        node.type = read_symbol(dec, c.data(), 2) == 0
                        ? PartitionType::Vertical
                        : PartitionType::Split;
    } else {
        node.type = static_cast<PartitionType>(
            read_symbol(dec, row, symbol_length(size_log2)));
    }

    if (node.children.empty() && node.type == PartitionType::None &&
        size_log2 >= 4)
        ctx.update(mi_row, mi_col, size_log2, size_log2);
    auto children = make_children(node);
    for (auto& child : children) {
        if (child.w_log2 == child.h_log2 && child.w_log2 >= 3) {
            child = read_partition_tree(dec, cdf, ctx, child.row, child.col,
                                        child.w_log2);
        } else {
            ctx.update(child.row, child.col, child.w_log2, child.h_log2);
        }
        node.children.push_back(std::move(child));
    }
    return node;
}

}  // namespace termchart::image::codec::av1
