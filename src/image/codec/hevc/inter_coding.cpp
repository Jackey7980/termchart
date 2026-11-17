#include "inter_coding.hpp"

#include "interp.hpp"

#include <algorithm>

namespace termchart::image::codec::hevc {
namespace {

// H.265 6.4.1 的 Z 序可用性：先比 CTB 地址，同一 CTB 内再按 4x4 最小块的
// Z-scan 地址比较（ffmpeg z_scan_block_avail 用 pps->min_tb_addr_zs 表）。
// 注意「yN>y0 就一定不可用」是错的：CU 按 Z 序递归遍历，例如 16x16 分组里
// (8,8) 会先于 (16,0) 解码，此时 (16,0) 的左下方 A0=(15,8) 是可用的。
static int zscan_tb_addr(int bx, int by, int bits) {
    int z = 0;
    for (int i = 0; i < bits; ++i) {
        z |= ((by >> i) & 1) << (2 * i + 1);
        z |= ((bx >> i) & 1) << (2 * i);
    }
    return z;
}

bool z_scan_available(int x0, int y0, int x_n, int y_n, int log2_ctb) {
    const int x_ctb0 = x0 >> log2_ctb;
    const int y_ctb0 = y0 >> log2_ctb;
    const int x_ctb_n = x_n >> log2_ctb;
    const int y_ctb_n = y_n >> log2_ctb;
    if (y_ctb_n < y_ctb0 || x_ctb_n < x_ctb0) return true;
    if (y_ctb_n > y_ctb0 || x_ctb_n > x_ctb0) return false;
    const int bits = log2_ctb - 2;  // 4x4 最小块：CTB 内每维 2^bits 个
    const int addr_cur = zscan_tb_addr((x0 >> 2) & ((1 << bits) - 1),
                                       (y0 >> 2) & ((1 << bits) - 1), bits);
    const int addr_n = zscan_tb_addr((x_n >> 2) & ((1 << bits) - 1),
                                     (y_n >> 2) & ((1 << bits) - 1), bits);
    return addr_n <= addr_cur;
}

// PPS 的 log2_parallel_merge_level_minus2 固定写 0 → 级别 2。
inline constexpr int kParallelMergeLog2 = 2;

bool is_diff_mer(int x_n, int y_n, int x_p, int y_p) {
    return (x_n >> kParallelMergeLog2) == (x_p >> kParallelMergeLog2) &&
           (y_n >> kParallelMergeLog2) == (y_p >> kParallelMergeLog2);
}

}  // namespace

void MvFieldStore::reset(int pic_width, int pic_height) {
    columns_ = (pic_width + 3) >> 2;
    rows_ = (pic_height + 3) >> 2;
    field_.assign(static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_), PuField{});
}

PuField MvFieldStore::at(int x, int y) const {
    if (columns_ <= 0 || rows_ <= 0) return PuField{};
    const int cx = std::clamp(x >> 2, 0, columns_ - 1);
    const int cy = std::clamp(y >> 2, 0, rows_ - 1);
    return field_[static_cast<std::size_t>(cy) * static_cast<std::size_t>(columns_) +
                  static_cast<std::size_t>(cx)];
}

void MvFieldStore::set_block(int x, int y, int w, int h, PuField field) {
    const int cx0 = std::max(0, x >> 2);
    const int cy0 = std::max(0, y >> 2);
    const int cx1 = std::min(columns_ - 1, (x + w - 1) >> 2);
    const int cy1 = std::min(rows_ - 1, (y + h - 1) >> 2);
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx)
            field_[static_cast<std::size_t>(cy) * static_cast<std::size_t>(columns_) +
                   static_cast<std::size_t>(cx)] = field;
}

NeighborAvail neighbor_avail(const CuGeom& cu, const PicGeom& pic) {
    const int ctb = 1 << pic.log2_ctb;
    NeighborAvail na;
    na.left = cu.x > 0;
    na.up = cu.y > 0;
    na.up_left = na.left && na.up;

    const int x_in_ctb = cu.x & (ctb - 1);
    const int y_in_ctb = cu.y & (ctb - 1);
    if (x_in_ctb + cu.w == ctb) {
        // 块右边界正好落在 CTB 右边界：右上邻居在右侧 CTB 列，
        // 只有「该 CTB 存在」且「当前 CTB 不是首行」时可用。
        const int ctb_col = cu.x >> pic.log2_ctb;
        const int ctb_row = cu.y >> pic.log2_ctb;
        const bool up_right_ctb = (ctb_col + 1) * ctb < pic.width && ctb_row > 0;
        na.up_right_sap = up_right_ctb && y_in_ctb != 0;
    } else {
        na.up_right_sap = na.up;
    }
    na.bottom_left = (cu.y + cu.h >= pic.height) ? false : na.left;
    return na;
}

int derive_merge_candidates(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                           int max_cand, Mv out[kMaxMergeCand]) {
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_p = cu.x;
    const int y_p = cu.y;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    auto usable = [&](bool cand, int x, int y) {
        return cand && fields.at(x, y).pred != kPredIntra;
    };

    const bool ok_a1 = usable(na.left, x_a1, y_a1) && !is_diff_mer(x_a1, y_a1, x_p, y_p);
    const bool ok_b1 = usable(na.up, x_b1, y_b1) && !is_diff_mer(x_b1, y_b1, x_p, y_p);
    const bool ok_b0 = usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(x_p, y_p, x_b0, y_b0, pic.log2_ctb) &&
                       !is_diff_mer(x_b0, y_b0, x_p, y_p);
    const bool ok_a0 = usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(x_p, y_p, x_a0, y_a0, pic.log2_ctb) &&
                       !is_diff_mer(x_a0, y_a0, x_p, y_p);
    const bool ok_b2 = usable(na.up_left, x_b2, y_b2) && !is_diff_mer(x_b2, y_b2, x_p, y_p);

    // 规范 8.5.3.1.2 / ffmpeg derive_spatial_merge_candidates 的【成对】去重：
    //   B1 与 A1、B0 与 B1、A0 与 A1、B2 与 A1&B1 的运动信息（MV + ref_idx）相同时跳过。
    // 注意 is_available_* 用「原始可用性」参与判定，即使前一个候选已被去重仍照常生效。
    // 单参考下 ref_idx 恒 0，比较 MV 即等价于比较运动信息。
    auto same_mv_ref = [&](int xa, int ya, int xb, int yb) {
        const PuField fa = fields.at(xa, ya);
        const PuField fb = fields.at(xb, yb);
        return fa.ref_idx == fb.ref_idx && fa.mv == fb.mv;
    };
    auto mv_at = [&](int x, int y) { return fields.at(x, y).mv; };

    int n = 0;
    if (ok_a1) out[n++] = mv_at(x_a1, y_a1);
    if (ok_b1 && !(ok_a1 && same_mv_ref(x_b1, y_b1, x_a1, y_a1)))
        out[n++] = mv_at(x_b1, y_b1);
    if (ok_b0 && !(ok_b1 && same_mv_ref(x_b0, y_b0, x_b1, y_b1)))
        out[n++] = mv_at(x_b0, y_b0);
    if (ok_a0 && !(ok_a1 && same_mv_ref(x_a0, y_a0, x_a1, y_a1)))
        out[n++] = mv_at(x_a0, y_a0);
    if (ok_b2 && !(ok_a1 && same_mv_ref(x_b2, y_b2, x_a1, y_a1)) &&
        !(ok_b1 && same_mv_ref(x_b2, y_b2, x_b1, y_b1)) && n != 4)
        out[n++] = mv_at(x_b2, y_b2);

    const int limit = std::min(max_cand, kMaxMergeCand);
    while (n < limit) out[n++] = Mv{};  // 零 MV 补齐（解码端 pred_flag = PF_L0）
    return n;
}

int derive_merge_candidates_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                               int max_cand, PuField out[kMaxMergeCand]) {
    // 与 derive_merge_candidates 同构，但候选整体是 PuField（MV + ref_idx）。
    // 解码端 merge 模式按候选的 ref_idx 取参考帧、MV 不缩放（HEVC 8.5.3.1.3）。
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_p = cu.x, y_p = cu.y;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    auto usable = [&](bool cand, int x, int y) {
        return cand && fields.at(x, y).pred != kPredIntra;
    };
    const bool ok_a1 = usable(na.left, x_a1, y_a1) && !is_diff_mer(x_a1, y_a1, x_p, y_p);
    const bool ok_b1 = usable(na.up, x_b1, y_b1) && !is_diff_mer(x_b1, y_b1, x_p, y_p);
    const bool ok_b0 = usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(x_p, y_p, x_b0, y_b0, pic.log2_ctb) &&
                       !is_diff_mer(x_b0, y_b0, x_p, y_p);
    const bool ok_a0 = usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(x_p, y_p, x_a0, y_a0, pic.log2_ctb) &&
                       !is_diff_mer(x_a0, y_a0, x_p, y_p);
    const bool ok_b2 = usable(na.up_left, x_b2, y_b2) && !is_diff_mer(x_b2, y_b2, x_p, y_p);

    auto same_info = [&](int xa, int ya, int xb, int yb) {
        const PuField fa = fields.at(xa, ya), fb = fields.at(xb, yb);
        return fa.ref_idx == fb.ref_idx && fa.mv == fb.mv;
    };
    int n = 0;
    if (ok_a1) out[n++] = fields.at(x_a1, y_a1);
    if (ok_b1 && !(ok_a1 && same_info(x_b1, y_b1, x_a1, y_a1))) out[n++] = fields.at(x_b1, y_b1);
    if (ok_b0 && !(ok_b1 && same_info(x_b0, y_b0, x_b1, y_b1))) out[n++] = fields.at(x_b0, y_b0);
    if (ok_a0 && !(ok_a1 && same_info(x_a0, y_a0, x_a1, y_a1))) out[n++] = fields.at(x_a0, y_a0);
    if (ok_b2 && !(ok_a1 && same_info(x_b2, y_b2, x_a1, y_a1)) &&
        !(ok_b1 && same_info(x_b2, y_b2, x_b1, y_b1)) && n != 4)
        out[n++] = fields.at(x_b2, y_b2);

    const int limit = std::min(max_cand, kMaxMergeCand);
    while (n < limit) out[n++] = PuField{kPredL0, 0, Mv{}};  // 零 MV / ref 0 补齐
    return n;
}

int derive_amvp_candidates(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                           Mv out[2]) {
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    // 单参考 + POC 间距恒 1（每帧只参考前一帧）时，MP_MX 与 MP_MX_LT 等价：
    // 邻居的参考图就是当前参考图，无需 POC 缩放。
    auto same_ref_usable = [&](bool cand, int x, int y) {
        if (!cand) return false;
        const PuField f = fields.at(x, y);
        return f.pred == kPredL0 && f.ref_idx == 0;
    };

    const bool ok_a0 = same_ref_usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(cu.x, cu.y, x_a0, y_a0, pic.log2_ctb);
    const bool ok_a1 = same_ref_usable(na.left, x_a1, y_a1);
    const bool ok_b0 = same_ref_usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(cu.x, cu.y, x_b0, y_b0, pic.log2_ctb);
    const bool ok_b1 = same_ref_usable(na.up, x_b1, y_b1);
    const bool ok_b2 = same_ref_usable(na.up_left, x_b2, y_b2);

    bool have_a = false;
    bool have_b = false;
    Mv mv_a;
    Mv mv_b;
    if (ok_a0) {
        mv_a = fields.at(x_a0, y_a0).mv;
        have_a = true;
    } else if (ok_a1) {
        mv_a = fields.at(x_a1, y_a1).mv;
        have_a = true;
    }
    if (ok_b0) {
        mv_b = fields.at(x_b0, y_b0).mv;
        have_b = true;
    } else if (ok_b1) {
        mv_b = fields.at(x_b1, y_b1).mv;
        have_b = true;
    } else if (ok_b2) {
        mv_b = fields.at(x_b2, y_b2).mv;
        have_b = true;
    }

    // ffmpeg scalef 分支：左侧无候选时把上侧候选提为第 0 项。
    if (!(ok_a0 || ok_a1) && have_b) {
        mv_a = mv_b;
        have_a = true;
    }

    int n = 0;
    if (have_a) out[n++] = mv_a;
    if (have_b && (!have_a || !(mv_a == mv_b))) out[n++] = mv_b;
    return n;
}

int derive_amvp_candidates_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                              int target_ref, Mv out[2]) {
    // 与 derive_amvp_candidates 同构，但只收 ref_idx == target_ref 的邻居——
    // 这样 MV 无需跨参考帧的 POC 缩放（合法但略保守的预测选择）。
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    auto usable = [&](bool cand, int x, int y) {
        if (!cand) return false;
        const PuField f = fields.at(x, y);
        return f.pred == kPredL0 && f.ref_idx == target_ref;
    };
    const bool ok_a0 = usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(cu.x, cu.y, x_a0, y_a0, pic.log2_ctb);
    const bool ok_a1 = usable(na.left, x_a1, y_a1);
    const bool ok_b0 = usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(cu.x, cu.y, x_b0, y_b0, pic.log2_ctb);
    const bool ok_b1 = usable(na.up, x_b1, y_b1);
    const bool ok_b2 = usable(na.up_left, x_b2, y_b2);

    bool have_a = false, have_b = false;
    Mv mv_a, mv_b;
    if (ok_a0) { mv_a = fields.at(x_a0, y_a0).mv; have_a = true; }
    else if (ok_a1) { mv_a = fields.at(x_a1, y_a1).mv; have_a = true; }
    if (ok_b0) { mv_b = fields.at(x_b0, y_b0).mv; have_b = true; }
    else if (ok_b1) { mv_b = fields.at(x_b1, y_b1).mv; have_b = true; }
    else if (ok_b2) { mv_b = fields.at(x_b2, y_b2).mv; have_b = true; }
    if (!(ok_a0 || ok_a1) && have_b) { mv_a = mv_b; have_a = true; }

    int n = 0;
    if (have_a) out[n++] = mv_a;
    if (have_b && (!have_a || !(mv_a == mv_b))) out[n++] = mv_b;
    return n;
}

namespace {

// B 片「运动信息相同」判定：与 ffmpeg compare_mv_ref_idx 逐条一致——
// 比较两个列表的 MV 与 ref_idx（pred_flag 不参与比较）。
bool same_mv_ref_b(const PuField& a, const PuField& b) {
    return a.mv == b.mv && a.mv_l1 == b.mv_l1 && a.ref_idx == b.ref_idx &&
           a.ref_idx_l1 == b.ref_idx_l1;
}

}  // namespace

int derive_merge_candidates_b(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                              int max_cand, int nb_refs_l0, int nb_refs_l1,
                              PuField out[kMaxMergeCand]) {
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_p = cu.x, y_p = cu.y;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    auto usable = [&](bool cand, int x, int y) {
        return cand && fields.at(x, y).pred != kPredIntra;
    };
    const bool ok_a1 = usable(na.left, x_a1, y_a1) && !is_diff_mer(x_a1, y_a1, x_p, y_p);
    const bool ok_b1 = usable(na.up, x_b1, y_b1) && !is_diff_mer(x_b1, y_b1, x_p, y_p);
    const bool ok_b0 = usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(x_p, y_p, x_b0, y_b0, pic.log2_ctb) &&
                       !is_diff_mer(x_b0, y_b0, x_p, y_p);
    const bool ok_a0 = usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(x_p, y_p, x_a0, y_a0, pic.log2_ctb) &&
                       !is_diff_mer(x_a0, y_a0, x_p, y_p);
    const bool ok_b2 = usable(na.up_left, x_b2, y_b2) && !is_diff_mer(x_b2, y_b2, x_p, y_p);

    const int limit = std::min(max_cand, kMaxMergeCand);
    PuField base[kMaxMergeCand] = {};
    int n = 0;
    if (ok_a1) base[n++] = fields.at(x_a1, y_a1);
    const bool b1_dup_a1 =
        ok_a1 && same_mv_ref_b(fields.at(x_b1, y_b1), fields.at(x_a1, y_a1));
    if (ok_b1 && !b1_dup_a1) base[n++] = fields.at(x_b1, y_b1);
    const bool b0_dup_b1 =
        ok_b1 && same_mv_ref_b(fields.at(x_b0, y_b0), fields.at(x_b1, y_b1));
    if (ok_b0 && !b0_dup_b1) base[n++] = fields.at(x_b0, y_b0);
    const bool a0_dup_a1 =
        ok_a1 && same_mv_ref_b(fields.at(x_a0, y_a0), fields.at(x_a1, y_a1));
    if (ok_a0 && !a0_dup_a1) base[n++] = fields.at(x_a0, y_a0);
    const bool b2_dup_a1 =
        ok_a1 && same_mv_ref_b(fields.at(x_b2, y_b2), fields.at(x_a1, y_a1));
    const bool b2_dup_b1 =
        ok_b1 && same_mv_ref_b(fields.at(x_b2, y_b2), fields.at(x_b1, y_b1)) && n != 4;
    if (ok_b2 && !b2_dup_a1 && !b2_dup_b1) base[n++] = fields.at(x_b2, y_b2);

    for (int i = 0; i < n; ++i) out[i] = base[i];

    // combined bi-pred：按 kCombinedPairTable 的枚举顺序补候选（解码端同序）。
    const int nb_orig = n;
    for (int t = 0; t < 12 && n < limit; ++t) {
        const int ia = kCombinedPairTable[t][0];
        const int ib = kCombinedPairTable[t][1];
        if (ia >= nb_orig || ib >= nb_orig) continue;
        const PuField& ca = base[ia];
        const PuField& cb = base[ib];
        // FFmpeg 用位测试 (pred_flag & PF_L0)/(pred_flag & PF_L1)：BI 邻居同时
        // 含有两个列表的运动信息，既能当 L0 源也能当 L1 源。早期只收纯 L0/L1
        // 会让 B3（邻居是 B1 的 BI CU）的候选列表整体错位。
        const bool has_l0 = ca.pred == kPredL0 || ca.pred == kPredBi;
        const bool has_l1 = cb.pred == kPredL1 || cb.pred == kPredBi;
        if (!has_l0 || !has_l1) continue;
        PuField c;
        c.pred = kPredBi;
        c.ref_idx = ca.ref_idx;
        c.mv = ca.mv;
        c.ref_idx_l1 = cb.ref_idx_l1;
        c.mv_l1 = cb.mv_l1;
        out[n++] = c;
    }

    // 零 MV 补齐（zero_idx 规则，ffmpeg 同款）：双向可用时取 BI，否则取可用单列表。
    const bool has_l0 = nb_refs_l0 > 0;
    const bool has_l1 = nb_refs_l1 > 0;
    const int nb_refs = has_l0 && has_l1 ? std::min(nb_refs_l0, nb_refs_l1) : 1;
    while (n < limit) {
        PuField z;
        z.pred = (has_l0 && has_l1) ? kPredBi : (has_l1 ? kPredL1 : kPredL0);
        z.ref_idx = static_cast<std::int8_t>(n < nb_refs ? n : 0);
        z.ref_idx_l1 = z.ref_idx;
        out[n++] = z;
    }
    return n;
}

int derive_amvp_candidates_b(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                             int list, int target_ref, Mv out[2]) {
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int x_a0 = cu.x - 1, y_a0 = cu.y + cu.h;
    const int x_a1 = cu.x - 1, y_a1 = cu.y + cu.h - 1;
    const int x_b0 = cu.x + cu.w, y_b0 = cu.y - 1;
    const int x_b1 = cu.x + cu.w - 1, y_b1 = cu.y - 1;
    const int x_b2 = cu.x - 1, y_b2 = cu.y - 1;

    auto usable = [&](bool cand, int x, int y) {
        if (!cand) return false;
        const PuField f = fields.at(x, y);
        if (f.pred == kPredIntra) return false;
        if (list == 0)
            return (f.pred == kPredL0 || f.pred == kPredBi) && f.ref_idx == target_ref;
        return (f.pred == kPredL1 || f.pred == kPredBi) && f.ref_idx_l1 == target_ref;
    };
    auto mv_of = [&](int x, int y) {
        const PuField f = fields.at(x, y);
        return list == 0 ? f.mv : f.mv_l1;
    };

    const bool ok_a0 = usable(na.bottom_left, x_a0, y_a0) && y_a0 < pic.height &&
                       z_scan_available(cu.x, cu.y, x_a0, y_a0, pic.log2_ctb);
    const bool ok_a1 = usable(na.left, x_a1, y_a1);
    const bool ok_b0 = usable(na.up_right_sap, x_b0, y_b0) && x_b0 < pic.width &&
                       z_scan_available(cu.x, cu.y, x_b0, y_b0, pic.log2_ctb);
    const bool ok_b1 = usable(na.up, x_b1, y_b1);
    const bool ok_b2 = usable(na.up_left, x_b2, y_b2);

    bool have_a = false, have_b = false;
    Mv mv_a, mv_b;
    if (ok_a0) { mv_a = mv_of(x_a0, y_a0); have_a = true; }
    else if (ok_a1) { mv_a = mv_of(x_a1, y_a1); have_a = true; }
    if (ok_b0) { mv_b = mv_of(x_b0, y_b0); have_b = true; }
    else if (ok_b1) { mv_b = mv_of(x_b1, y_b1); have_b = true; }
    else if (ok_b2) { mv_b = mv_of(x_b2, y_b2); have_b = true; }
    if (!(ok_a0 || ok_a1) && have_b) { mv_a = mv_b; have_a = true; }

    int n = 0;
    if (have_a) out[n++] = mv_a;
    if (have_b && (!have_a || !(mv_a == mv_b))) out[n++] = mv_b;
    return n;
}

namespace {

// FFmpeg mv_scale：num/den 为 POC 距离之比，rounding 与 C 整除（向零截断）一致。
Mv scale_mv_poc(Mv mv, int num, int den) {
    if (den == 0) return mv;
    const long long x = (static_cast<long long>(mv.x) * num + (den >> 1)) / den;
    const long long y = (static_cast<long long>(mv.y) * num + (den >> 1)) / den;
    return Mv{static_cast<int>(x), static_cast<int>(y)};
}

}  // namespace

int derive_amvp_candidates_b_ex(const MvFieldStore& fields, const CuGeom& cu, const PicGeom& pic,
                                int list, int target_ref, int curr_poc, int ref0_poc, int ref1_poc,
                                Mv out[2]) {
    const NeighborAvail na = neighbor_avail(cu, pic);
    const int px[5] = {cu.x - 1, cu.x - 1, cu.x + cu.w, cu.x + cu.w - 1, cu.x - 1};
    const int py[5] = {cu.y + cu.h, cu.y + cu.h - 1, cu.y - 1, cu.y - 1, cu.y - 1};
    const bool avail[5] = {
        na.bottom_left && py[0] < pic.height &&
            z_scan_available(cu.x, cu.y, px[0], py[0], pic.log2_ctb),
        na.left,
        na.up_right_sap && px[2] < pic.width &&
            z_scan_available(cu.x, cu.y, px[2], py[2], pic.log2_ctb),
        na.up,
        na.up_left,
    };
    const int curr_ref_poc = list == 0 ? ref0_poc : ref1_poc;
    const int other_ref_poc = list == 0 ? ref1_poc : ref0_poc;
    const int den = curr_poc - curr_ref_poc;
    const int num = curr_poc - other_ref_poc;

    auto mv_from = [&](int pos, bool scaled, Mv& mv) {
        const PuField f = fields.at(px[pos], py[pos]);
        if (f.pred == kPredIntra) return false;
        const bool has_target = list == 0
                                    ? (f.pred == kPredL0 || f.pred == kPredBi)
                                    : (f.pred == kPredL1 || f.pred == kPredBi);
        const bool has_other = list == 0
                                   ? (f.pred == kPredL1 || f.pred == kPredBi)
                                   : (f.pred == kPredL0 || f.pred == kPredBi);
        if (!scaled) {
            if (has_target && (list == 0 ? f.ref_idx == target_ref
                                         : f.ref_idx_l1 == target_ref)) {
                mv = list == 0 ? f.mv : f.mv_l1;
                return true;
            }
            // FFmpeg MP_MX：另一列表只要指向同一张参考图像即可当候选。
            if (has_other && other_ref_poc == curr_ref_poc) {
                mv = list == 0 ? f.mv_l1 : f.mv;
                return true;
            }
            return false;
        }
        // MP_MX_LT：两列表参考图像不同时按 POC 距离缩放另一列表的 MV。
        if (has_other && den != 0) {
            mv = scale_mv_poc(list == 0 ? f.mv_l1 : f.mv, num, den);
            return true;
        }
        return false;
    };
    auto find_group = [&](int from, int to, Mv& mv) {
        for (int i = from; i <= to; ++i)
            if (avail[i] && mv_from(i, false, mv)) return true;
        for (int i = from; i <= to; ++i)
            if (avail[i] && mv_from(i, true, mv)) return true;
        return false;
    };

    Mv ma{}, mb{};
    const bool have_a = find_group(0, 1, ma);  // A0 → A1
    const bool have_b = find_group(2, 4, mb);  // B0 → B1 → B2
    int n = 0;
    if (have_a) out[n++] = ma;
    if (have_b && (!have_a || !(ma == mb))) out[n++] = mb;
    return n;
}

void mc_copy(const std::uint8_t* ref, int ref_w, int ref_h, int x, int y, int w, int h, int dx,
             int dy, std::uint8_t* dst, int dst_stride) {
    if (ref == nullptr || ref_w <= 0 || ref_h <= 0) return;
    for (int j = 0; j < h; ++j) {
        const int sy = std::clamp(y + j + dy, 0, ref_h - 1);
        const std::uint8_t* src_row = ref + static_cast<std::size_t>(sy) * ref_w;
        std::uint8_t* dst_row = dst + static_cast<std::size_t>(j) * dst_stride;
        for (int i = 0; i < w; ++i) {
            const int sx = std::clamp(x + i + dx, 0, ref_w - 1);
            dst_row[i] = src_row[sx];
        }
    }
}

void mc_luma(const std::uint8_t* ref, int ref_w, int ref_h, const CuGeom& cu, Mv mv,
             std::uint8_t* dst, int dst_stride) {
    const int fx = mv.x & 3, fy = mv.y & 3;
    if (fx == 0 && fy == 0) {
        // 整数位置：原样走纯拷贝，保证既有整数 MV 码流逐字节不变。
        mc_copy(ref, ref_w, ref_h, cu.x, cu.y, cu.w, cu.h, mv.x / 4, mv.y / 4, dst, dst_stride);
        return;
    }
    // 亚像素：FFmpeg mv 单位 1/4 亮度采样；mv/4 为整数位移、mv&3 为分数位。
    interp_luma(ref, ref_w, ref_h, cu.x + mv.x / 4, cu.y + mv.y / 4, fx, fy, cu.w, cu.h, dst,
                dst_stride);
}

void mc_chroma(const std::uint8_t* ref, int ref_w, int ref_h, const CuGeom& cu_chroma, Mv mv,
               std::uint8_t* dst, int dst_stride) {
    // 4:2:0：色度采样是亮度的一半 ⇒ 色度 MV 分数位是 1/8 精度（mv & 7），
    // 整数位移是 mv / 8。分数位为 0 时走纯拷贝保字节不变。
    const int fx = mv.x & 7, fy = mv.y & 7;
    if (fx == 0 && fy == 0) {
        mc_copy(ref, ref_w, ref_h, cu_chroma.x, cu_chroma.y, cu_chroma.w, cu_chroma.h, mv.x / 8,
                mv.y / 8, dst, dst_stride);
        return;
    }
    interp_chroma(ref, ref_w, ref_h, cu_chroma.x + mv.x / 8, cu_chroma.y + mv.y / 8, fx, fy,
                  cu_chroma.w, cu_chroma.h, dst, dst_stride);
}

void mc_bi(const std::uint8_t* ref0, const std::uint8_t* ref1, int ref_w, int ref_h,
           const CuGeom& cu, Mv mv0, Mv mv1, int sub, std::uint8_t* dst, int dst_stride) {
    if (ref0 == nullptr || ref1 == nullptr || dst == nullptr) return;
    if (ref_w <= 0 || ref_h <= 0 || cu.w <= 0 || cu.h <= 0) return;
    const int m = (1 << sub) - 1;
    const int dx0 = mv0.x >> sub, dy0 = mv0.y >> sub;
    const int dx1 = mv1.x >> sub, dy1 = mv1.y >> sub;
    const bool integer = ((mv0.x & m) == 0) && ((mv0.y & m) == 0) && ((mv1.x & m) == 0) &&
                         ((mv1.y & m) == 0);

    if (integer) {
        // 整数位置：两份拷贝后 (a+b+1)>>1，即 FFmpeg pel_bi 的 8bit 无加权算式。
        for (int j = 0; j < cu.h; ++j) {
            const int y0 = std::clamp(cu.y + j + dy0, 0, ref_h - 1);
            const int y1 = std::clamp(cu.y + j + dy1, 0, ref_h - 1);
            const std::uint8_t* r0 = ref0 + static_cast<std::size_t>(y0) * ref_w;
            const std::uint8_t* r1 = ref1 + static_cast<std::size_t>(y1) * ref_w;
            std::uint8_t* d = dst + static_cast<std::size_t>(j) * dst_stride;
            for (int i = 0; i < cu.w; ++i) {
                const int x0 = std::clamp(cu.x + i + dx0, 0, ref_w - 1);
                const int x1 = std::clamp(cu.x + i + dx1, 0, ref_w - 1);
                d[i] = static_cast<std::uint8_t>((r0[x0] + r1[x1] + 1) >> 1);
            }
        }
        return;
    }

    // 亚像素：两列表各自插值到紧凑缓冲（stride = cu.w），再按 (a+b+1)>>1 合成。
    // 注意这里的合成顺序与 FFmpeg qpel_bi 的"两次舍入"不同：本实现只用于
    // 亚像素双向的编码端试算，字节级基线路径不会走到（B 片 MV 现取 kMvStep 整数步）。
    const std::size_t n = static_cast<std::size_t>(cu.w) * static_cast<std::size_t>(cu.h);
    std::vector<std::uint8_t> a(n), b(n);
    if (sub == 2) {
        interp_luma(ref0, ref_w, ref_h, cu.x + dx0, cu.y + dy0, mv0.x & m, mv0.y & m, cu.w, cu.h,
                    a.data(), cu.w);
        interp_luma(ref1, ref_w, ref_h, cu.x + dx1, cu.y + dy1, mv1.x & m, mv1.y & m, cu.w, cu.h,
                    b.data(), cu.w);
    } else {
        interp_chroma(ref0, ref_w, ref_h, cu.x + dx0, cu.y + dy0, mv0.x & m, mv0.y & m, cu.w, cu.h,
                      a.data(), cu.w);
        interp_chroma(ref1, ref_w, ref_h, cu.x + dx1, cu.y + dy1, mv1.x & m, mv1.y & m, cu.w, cu.h,
                      b.data(), cu.w);
    }
    for (int j = 0; j < cu.h; ++j) {
        std::uint8_t* d = dst + static_cast<std::size_t>(j) * dst_stride;
        const std::size_t row = static_cast<std::size_t>(j) * cu.w;
        for (int i = 0; i < cu.w; ++i)
            d[i] = static_cast<std::uint8_t>((a[row + i] + b[row + i] + 1) >> 1);
    }
}

}  // namespace termchart::image::codec::hevc
