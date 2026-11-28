// inter_coding.hpp —— P 片帧间预测：MV 场、merge / AMVP 候选推导与运动补偿
//
// 语法真值来自 FFmpeg libavcodec/hevc/mvs.c：
//   · merge 候选顺序 A1 → B1 → B0 → A0 → B2 → 零 MV 补齐（8.5.3.2.3）
//   · AMVP 候选左（A0→A1）→ 上（B0→B1→B2）→ 前两项（8.5.3.2.5）
// 本项目只做 P 片、单参考（ref_idx 恒 0）、SPS temporal_mvp=0，因此
// 时间域候选（TMVP）与 L1 分支不参与。
//
// MV 单位是 1/4 亮度采样（HEVC 约定）。本实现只产生 8 的倍数，即 2 亮度采样
// 粒度：亮度落在整数位置、4:2:0 色度也落在整数位置，运动补偿退化为纯拷贝
// （等价 FFmpeg put_uni_pixels / epel[0][0]），不需要任何插值滤波器。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_INTER_CODING_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_INTER_CODING_HPP

#include <cstdint>
#include <vector>

namespace termchart::image::codec::hevc {

inline constexpr int kMvStep = 8;  // MV 步长：2 亮度采样
inline constexpr int kMaxMergeCand = 5;

struct Mv {
    int x = 0;
    int y = 0;
};

inline bool operator==(const Mv& a, const Mv& b) { return a.x == b.x && a.y == b.y; }

inline constexpr std::uint8_t kPredIntra = 0;
inline constexpr std::uint8_t kPredL0 = 1;
inline constexpr std::uint8_t kPredL1 = 2;
inline constexpr std::uint8_t kPredBi = 3;

// 一个 4x4 最小 PU 的预测场（对齐 FFmpeg tab_mvf 的 pred_flag / ref_idx / mv）。
// B 片才有 L1 分量；P/单向预测时 ref_idx_l1 恒 0、mv_l1 恒 {0,0}，
// 与 FFmpeg tab_mvf 里"未用列表填 0"的约定一致（combined bi-pred 直接取用）。
struct PuField {
    std::uint8_t pred = kPredIntra;
    std::int8_t ref_idx = 0;
    Mv mv;
    std::int8_t ref_idx_l1 = 0;
    Mv mv_l1;
};

// 整帧的 4x4 预测场：列宽 = ceil(宽 / 4)。
class MvFieldStore {
public:
    void reset(int pic_width, int pic_height);

    // 亮度像素坐标 → 所属 4x4 格；越界按边界钳制（调用方仍应先判可用性）。
    PuField at(int x, int y) const;
    void set_block(int x, int y, int w, int h, PuField field);

    int columns() const { return columns_; }
    int rows() const { return rows_; }

private:
    int columns_ = 0;
    int rows_ = 0;
    std::vector<PuField> field_;
};

struct PicGeom {
    int width = 0;
    int height = 0;
    int log2_ctb = 6;
};

// CU 的像素几何（本实现恒为 8x8 对齐）。
struct CuGeom {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// 邻居可用性：等价 FFmpeg ff_hevc_set_neighbour_available（单 slice、单 tile）。
struct NeighborAvail {
    bool left = false;
    bool up = false;
    bool up_left = false;
    bool up_right_sap = false;  // 右上：块右边界正好落在 CTB 右边界时改看「上右 CTB」
    bool bottom_left = false;   // 左下：块下边界越出画面则不可用
};

NeighborAvail neighbor_avail(const CuGeom& cu, const PicGeom& pic);

// merge 候选推导（含零 MV 补齐）。返回候选个数（单参考下恒为 max_cand）。
// 单参考版（out 只收 MV，ref_idx 恒 0）——保既有字节级基线。
int derive_merge_candidates(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                            int max_cand, Mv out[kMaxMergeCand]);

// 多参考版：候选携带 (pred, ref_idx, mv)。merge 模式下解码端直接复制运动信息，
// 不缩放 MV；候选顺序与成对去重同单参考版，但 ref_idx 参与"运动信息相同"判定。
int derive_merge_candidates_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                               int max_cand, PuField out[kMaxMergeCand]);

// AMVP 候选推导（L0）。返回 0..2；索引越界时解码端取 (0,0)。
// 单参考版（只要 ref_idx==0 的邻居，无 POC 缩放）。
int derive_amvp_candidates(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                           Mv out[2]);

// 多参考版：只取 ref_idx == target_ref 的邻居（规避跨参考帧的 POC 缩放，
// 是合法但略保守的预测选择）。返回 0..2。
int derive_amvp_candidates_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                              int target_ref, Mv out[2]);

// ── B 片（双向预测）────────────────────────────────────────────────────────
// merge 候选不足时的 combined bi-pred 组合表（HEVC 8.5.3.2.3，同 ffmpeg
// l0_l1_cand_idx）：把「L0 候选的 L0 运动信息」与「L1 候选的 L1 运动信息」
// 拼成一个双向候选，条件为 l0 候选 pred==L0 且 l1 候选 pred==L1。
inline constexpr int kCombinedPairTable[12][2] = {
    {0, 1}, {1, 0}, {0, 2}, {2, 0}, {1, 2}, {2, 1},
    {0, 3}, {3, 0}, {1, 3}, {3, 1}, {2, 3}, {3, 2},
};

// B 片 merge 候选推导（8.5.3.2.3 完整版）：空间候选 → combined bi-pred → 零 MV 补齐。
// nb_refs_l0 / nb_refs_l1 用于零 MV 补齐：一个参考列表恒 >= 1，故双向可用时
// zero_idx < min(nb_refs_l0, nb_refs_l1) 取 zero_idx、否则取 0；
// 仅单列表可用时（PPS 的 num_ref_idx_l1 为 0）退化为单列表零 MV。
int derive_merge_candidates_b(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                              int max_cand, int nb_refs_l0, int nb_refs_l1,
                              PuField out[kMaxMergeCand]);

// B 片 AMVP（list = 0 或 1）：只收该列表可用且 ref_idx == target_ref 的邻居，
// 因此不需要跨参考帧 POC 缩放（同 P 片多参考版的口径）。返回 0..2。
int derive_amvp_candidates_b(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                             int list, int target_ref, Mv out[2]);

// FFmpeg 完整版：除了同参考帧的邻居，还会用 MP_MX_LT 从「另一列表」的邻居
// 按 POC 距离缩放 MV（B 片 L0/L1 指向不同图像时不可省略）。curr_poc 为当前
// POC，ref0_poc/ref1_poc 为两个活动参考的 POC。
int derive_amvp_candidates_b_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                                int list, int target_ref, int curr_poc, int ref0_poc, int ref1_poc,
                                Mv out[2]);

// 运动补偿：整数位置拷贝，越界采样按边缘复制（等价 FFmpeg emulated_edge_mc）。
// luma 的位移是 mv >> 2，chroma（4:2:0）是 mv >> 3。
void mc_copy(const std::uint8_t* ref, int ref_w, int ref_h, int x, int y, int w, int h,
             int dx, int dy, std::uint8_t* dst, int dst_stride);

void mc_luma(const std::uint8_t* ref, int ref_w, int ref_h, const CuGeom& cu, Mv mv,
             std::uint8_t* dst, int dst_stride);

// chroma 几何由调用方换算到色度平面坐标（cu.x/2、cu.y/2、cu.w/2、cu.h/2）。
void mc_chroma(const std::uint8_t* ref, int ref_w, int ref_h, const CuGeom& cu_chroma, Mv mv,
               std::uint8_t* dst, int dst_stride);

// B 片双向合成：dst = clip((a + b + 1) >> 1)，a/b 分别为 L0/L1 单列表预测。
// 与 FFmpeg put_hevc_pel_bi_pixels / qpel_bi 的 8bit 无加权情形逐位一致：
//   dst = (src1<<6 + src2<<6 + 64) >> 7 == (src1 + src2 + 1) >> 1。
// sub = 2（亮度，1/4 采样）或 3（色度，1/8 采样）；整数位置走纯拷贝、亚像素走插值。
void mc_bi(const std::uint8_t* ref0, const std::uint8_t* ref1, int ref_w, int ref_h,
           const CuGeom& cu, Mv mv0, Mv mv1, int sub, std::uint8_t* dst, int dst_stride);

// MV 是否满足本实现的整数位置约束。
inline bool mv_is_integer_step(Mv mv) {
    return (mv.x % kMvStep) == 0 && (mv.y % kMvStep) == 0;
}

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_INTER_CODING_HPP
