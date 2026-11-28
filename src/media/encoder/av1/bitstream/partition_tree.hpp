// partition_tree.hpp —— 128x128 超级块十叉树划分的编解码与邻居上下文
#ifndef TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_PARTITION_TREE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_AV1_BITSTREAM_PARTITION_TREE_HPP

#include <cstdint>
#include <functional>
#include <vector>

#include "../entropy/range_coder.hpp"

namespace termchart::image::codec::av1 {

enum class PartitionType : unsigned {
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Split = 3,
    TopSplit = 4,      // HORZ_A
    BottomSplit = 5,   // HORZ_B
    LeftSplit = 6,     // VERT_A
    RightSplit = 7,    // VERT_B
    Horizontal4 = 8,
    Vertical4 = 9,
};

struct PartitionNode {
    int row = 0;
    int col = 0;
    int w_log2 = 3;
    int h_log2 = 3;
    PartitionType type = PartitionType::None;
    std::vector<PartitionNode> children;
};

// 指向 partition[N_BL_LEVELS][4][16] 的首层；递归时按块尺寸取对应层。
using PartitionCdf = const std::uint16_t (*)[4][16];

class PartitionContext {
public:
    int frame_mi_rows = 16;
    int frame_mi_cols = 16;
    // 按 4x4 单位记录上 / 左邻居的划分上下文；尺寸随帧变化。
    std::vector<std::uint8_t> above;
    std::vector<std::uint8_t> left;

    int index(int mi_row, int mi_col, int size_log2) const;
    void update(int mi_row, int mi_col, int w_log2, int h_log2);
    void reset();
};

using PartitionChooser =
    std::function<PartitionType(int row, int col, int w_log2, int h_log2,
                                bool has_rows, bool has_cols)>;

// 叶块回调：在划分符号写完之后调用，供调用方写 mode info / 残差。
// 8x8 节点与「NONE 且尺寸 >= 16x16」的节点都会被当作叶。
using PartitionLeafWriter = std::function<void(const PartitionNode&)>;

PartitionNode write_partition_tree(RangeEncoder& enc, PartitionCdf cdf,
                                   PartitionContext& ctx, int mi_row, int mi_col,
                                   int size_log2, const PartitionChooser& chooser,
                                   const PartitionLeafWriter& leaf_writer = {});

PartitionNode read_partition_tree(RangeDecoder& dec, PartitionCdf cdf,
                                  PartitionContext& ctx, int mi_row, int mi_col,
                                  int size_log2);

}  // namespace termchart::image::codec::av1

#endif
