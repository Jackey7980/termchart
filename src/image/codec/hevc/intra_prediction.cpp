// intra_prediction.cpp —— HEVC 4x4 帧内预测（Planar / DC）
//
// 逐式对齐 HEVC 规范 8.4.4.2.4（Planar）与 8.4.4.2.5（DC），保证编码端预测
// 与解码端（ffmpeg）完全一致。
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "intra_prediction.hpp"
#include "transquant.hpp"  // 模式决策模拟重建走与真实编码同一条 encode_tu_4x4

namespace termchart::image::codec::hevc {
namespace {

int log2_of(int n) {
    int k = 0;
    while ((1 << k) < n) ++k;
    return k;
}

void predict_planar(int n, const std::uint8_t* top, const std::uint8_t* left,
                    std::uint8_t* dst) {
    const int log2n = log2_of(n);
    const int shift = log2n + 1;  // log2(2n)
    const int topRight = top[n];   // p[n][-1]
    const int botLeft = left[n];   // p[-1][n]
    for (int y = 0; y < n; ++y) {
        const int ly = left[y];
        for (int x = 0; x < n; ++x) {
            const int v = (n - 1 - x) * ly + (x + 1) * topRight +
                          (n - 1 - y) * top[x] + (y + 1) * botLeft + n;
            dst[y * n + x] = static_cast<std::uint8_t>(v >> shift);
        }
    }
}

void predict_dc(int n, const std::uint8_t* top, const std::uint8_t* left,
                bool has_top, bool has_left, std::uint8_t* dst) {
    // HEVC 8.4.4.2.2 参考样本替代：某一侧不可用时，用另一侧第一个可用样本
    // 填充该侧；两侧都不可用时用 1 << (bitDepth-1) = 128。
    // 替代后的样本【同样参与 dcVal 的双侧求和】，不能退化成单侧平均
    // （那样 dcVal 会偏，且重建误差会沿预测链传播）。
    std::uint8_t t[32] = {}, l[32] = {};
    if (!has_top && !has_left) {
        for (int i = 0; i < n; ++i) t[i] = l[i] = 128;
    } else if (!has_top) {
        for (int i = 0; i < n; ++i) { t[i] = left[0]; l[i] = left[i]; }
    } else if (!has_left) {
        for (int i = 0; i < n; ++i) { t[i] = top[i]; l[i] = top[0]; }
    } else {
        for (int i = 0; i < n; ++i) { t[i] = top[i]; l[i] = left[i]; }
    }

    // HEVC 8.4.4.2.5：dcVal = (Σ p[x][-1] + Σ p[-1][y] + nTbS) >> (k+1)，k=log2(nTbS)。
    const int log2n = log2_of(n);
    int sum = 0;
    for (int i = 0; i < n; ++i) sum += t[i] + l[i];
    const int dc = (sum + n) >> (log2n + 1);

    // cIdx==0 && nTbS<32 时的 DC 首行/首列边界滤波。常数参考下它是恒等变换
    // （flat128 逐位一致），非平坦参考下必须与解码端一致，否则编码端重建
    // 与 ffmpeg 解码发散，并沿帧内预测参考链逐 CU 累积。
    // FFmpeg pred_template.c:429-457 只有一条公式：替填补齐之后两侧一律视为可用，
    // 首行/首列无条件用 3*dc+2 式（不存在"单侧可用则 (x+dc+1)>>1"的分支）。
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) dst[y * n + x] = static_cast<std::uint8_t>(dc);
    if (n >= 32) return;
    dst[0] = static_cast<std::uint8_t>((l[0] + 2 * dc + t[0] + 2) >> 2);
    for (int x = 1; x < n; ++x)
        dst[x] = static_cast<std::uint8_t>((t[x] + 3 * dc + 2) >> 2);
    for (int y = 1; y < n; ++y)
        dst[y * n] = static_cast<std::uint8_t>((l[y] + 3 * dc + 2) >> 2);
}

}  // namespace

void predict_intra(int mode, int n, const std::uint8_t* top,
                   const std::uint8_t* left, bool has_top, bool has_left,
                   std::uint8_t* dst) {
    if (mode == 0) {
        predict_planar(n, top, left, dst);
    } else {
        predict_dc(n, top, left, has_top, has_left, dst);
    }
}

// ── MPM 候选表与模式信号（逐式对齐 FFmpeg hevcdec.c:2251-2330）────────────
// 逐式对齐 HEVC 8.4.2 的 MPM 候选表推导（与 FFmpeg hevcdec.c 的 cand_tab 一致）。
// 关键约束：候选表【推导顺序】必须严格等于解码端——FFmpeg 用未排序的
// candidate[0..2] 做 prev=1 的 candidate[mpm_idx] 命中；排序只发生在 prev=0 的
// rem 还原（hevcdec.c:2305-2310）。因此 mpm_idx 基于未排序下标，rem 求逆基于排序后表。
MpmCandidates derive_mpm_candidates(int cand_left, int cand_up) {
    constexpr int kPlanar = 0;
    constexpr int kDc = 1;
    constexpr int kAngular26 = 26;
    MpmCandidates c;
    if (cand_left == cand_up) {
        if (cand_left < 2) {
            c.mode[0] = kPlanar;
            c.mode[1] = kDc;
            c.mode[2] = kAngular26;
        } else {
            // HEVC 8.4.2 / HM getIntraDirLumaPredictor：两侧相等且 >= 2 时，
            // 三候选为 A、2+((A+29)%32)、2+((A-1)%32)（结果 0/1 仍为合法 Planar/DC）。
            c.mode[0] = cand_left;
            c.mode[1] = 2 + ((cand_left + 29) % 32);
            c.mode[2] = 2 + ((cand_left - 1) % 32);
        }
    } else {
        c.mode[0] = cand_left;
        c.mode[1] = cand_up;
        if (c.mode[0] != kPlanar && c.mode[1] != kPlanar) c.mode[2] = kPlanar;
        else if (c.mode[0] != kDc && c.mode[1] != kDc) c.mode[2] = kDc;
        else c.mode[2] = kAngular26;
    }
    return c;
}

// 给定目标模式，求能对偶 FFmpeg 解码端的语法信号。
// - 目标在候选表内（prev=1）：FFmpeg hevcdec.c:2302-2303 用【未排序】的
//   candidate[mpm_idx] 直接命中，故 mpm_idx 必须取未排序下标。
// - 否则（prev=0）：FFmpeg hevcdec.c:2305-2315 先把三个候选升序排序，再
//   d = rem; for i: if (d >= c[i]) ++d（比较的是含前次增量的 d）。
//   求逆从大到小：rem = target; for i = 2..0: if (rem > c[i]) --rem。
IntraModeSignal signal_intra_mode(int target_mode, const MpmCandidates& cands) {
    IntraModeSignal s;
    for (int i = 0; i < 3; ++i) {
        if (cands.mode[i] == target_mode) {
            s.prev_intra_luma_pred_flag = true;
            s.mpm_idx = i;
            s.rem_intra_luma_pred_mode = 0;
            return s;
        }
    }
    int sorted[3] = {cands.mode[0], cands.mode[1], cands.mode[2]};
    std::sort(sorted, sorted + 3);
    int rem = target_mode;
    for (int i = 2; i >= 0; --i)
        if (rem > sorted[i]) --rem;
    if (rem < 0 || rem > 31) {  // 目标模式不在 0..34：退回 MPM 首项，绝不产生非法语法
        s.prev_intra_luma_pred_flag = true;
        s.mpm_idx = 0;
        return s;
    }
    s.prev_intra_luma_pred_flag = false;
    s.mpm_idx = 0;
    s.rem_intra_luma_pred_mode = rem;
    return s;
}

void predict_intra_luma_4x4(int mode, const std::uint8_t* rec, int stride,
                            int x0, int y0,
                            int pic_w, int pic_h, std::uint8_t* dst,
                            ChromaPredPlane chroma) {
    // 逐式对齐 FFmpeg hevc/pred_template.c 的 intra_pred(size=2)。
    // HEVC 的亮度帧内预测在 TU 粒度进行（PU 只携带模式）：8x8 CU 用
    // PART_2Nx2N 时整块单一亮度模式，但预测按 4 个 4x4 TU 各做一次，每个 TU
    // 用自身 (x0,y0) 定位参考样本。ffmpeg 解码端同构，故编码端预测与之逐字节一致
    // 即可保证重建 == ffmpeg 解码。
    const int size = 4;
    const int size2 = 8;  // 2*size：参考样本数组长度
    int top[48] = {0}, left[48] = {0};
    int* T = top + 16;    // T[-1 .. 15]
    int* L = left + 16;   // L[-1 .. 15]

    // ── 邻居可用性 ───────────────────────────────────────────────────────
    // 可用性按【TU 原点】求值：FFmpeg hevcdec.c:hls_transform_unit 调
    // ff_hevc_set_neighbour_available(lc, x0, y0, trafo_size, trafo_size)，
    // 传的是 TU 的位置与边长，不是 CU/PU 原点。mvs.c 里：
    //   cand_up   = ctb_up_flag   || (y0 & (ctb-1))
    //   cand_left = ctb_left_flag || (x0 & (ctb-1))
    // 本编码器单 slice 覆盖全帧，ctb_left/up_flag 等价于“CTB 不在首列/首行”，
    // 合并后 cand_left == (x0 > 0)、cand_up == (y0 > 0)（x0/y0 为图像坐标）。
    // 同一 CU 内先解出的 TU 已写回 rec，正是 ffmpeg 读到的重建样本。
    const bool is_chroma = (chroma.rec != nullptr);
    // 色度 TU：FFmpeg 对色度调用 ff_hevc_set_neighbour_available 时仍传【亮度】
    // 坐标 xBase/yBase 与 nPbW=nPbH=8（4 色度像素 × 2），邻居判定用亮度 CTB 64；
    // 但 POS 采样的是色度平面，坐标 /2、步长用色度 stride。
    const int nb_w = is_chroma ? 2 * size : size;
    const int nb_h = is_chroma ? 2 * size : size;
    const int px = is_chroma ? (x0 >> 1) : x0;
    const int py = is_chroma ? (y0 >> 1) : y0;
    const int pw = is_chroma ? (pic_w >> 1) : pic_w;
    const int ph = is_chroma ? (pic_h >> 1) : pic_h;
    const int x0b = x0 & 63, y0b = y0 & 63;
    const int na_left        = (x0 > 0);
    const int na_up          = (y0 > 0);
    const int na_up_right    = (((x0b + nb_w) == 64) ? (na_up && !y0b) : na_up) &&
                               ((px + size) < pw);
    // end_of_tiles_y 是【当前 CTB 行的底部】，不是整帧高度：
    //   FFmpeg hevcdec.c:2753  lc->end_of_tiles_y = FFMIN(y_ctb + ctb_size, sps->height)
    //   FFmpeg mvs.c:58       cand_bottom_left = ((y0+nPbH) >= end_of_tiles_y) ? 0 : cand_left
    // 即每个 CTB 行最后一行的 TU 都关掉 bottom-left 扩展，不读下一 CTB 行
    // （那里在光栅解码顺序下尚未解码）。旧实现按整帧 pic_h 判定，单 CTB 行
    // 恰好一致，多 CTB 行时编码端会读到 rec 的未编码区，与 FFmpeg 的填充值分叉，
    // 表现为 mode 0/2..9（依赖 L[4..] 的正角度模式）整帧重建错位。
    const int ctb_bottom = std::min(((y0 >> 6) << 6) + 64, pic_h);
    const int na_bottom_left = ((y0 + nb_h) >= ctb_bottom) ? 0 : na_left;

    const int tb_mask = 15;        // (1<<(log2_ctb(6)-log2_min_tb(2)))-1（亮度坐标）
    // FFmpeg pred_template.c:123-126：size_in_tbs = (size << hshift) >> log2_min_tb。
    // 4:2:0 色度 hshift/vshift 均为 1 ⇒ 色度为 2、亮度为 1。该值参与
    // cand_up_right / cand_bottom_left 的 z-scan 可用性比较，色度沿用 1 会让
    // 角度模式的扩展参考在若干 CU 之后与解码端分叉。
    const int size_in_tbs = is_chroma ? 2 : 1;
    auto zscan = [](int x, int y) {
        int z = 0;
        for (int i = 0; i < 4; ++i) {
            z |= ((x >> i) & 1) << (2 * i);
            z |= ((y >> i) & 1) << (2 * i + 1);
        }
        return z;
    };
    const int x_tb = (x0 >> 2) & tb_mask;
    const int y_tb = (y0 >> 2) & tb_mask;
    const int cur_tb_addr = zscan(x_tb, y_tb);
    int cand_up_left     = na_left && na_up;
    int cand_up          = na_up;
    int cand_left        = na_left;
    // 越界索引在 ffmpeg 的 min_tb_addr_zs 表里是插值行/列的 -1（恒可比较为真），
    // 这里显式等价：x_tb==0 / y_tb==0 时该越界一侧视为 -1。
    const int z_up_right   = (y_tb == 0) ? -1
                           : zscan((x_tb + size_in_tbs) & tb_mask, y_tb - 1);
    const int z_bottom_left = (x_tb == 0) ? -1
                            : zscan(x_tb - 1, (y_tb + size_in_tbs) & tb_mask);
    int cand_up_right    = na_up_right && (cur_tb_addr > z_up_right);
    int cand_bottom_left = na_bottom_left && (cur_tb_addr > z_bottom_left);

    const int top_right_size   = std::min(px + size2, pw) - (px + size);
    const int bottom_left_size = std::min(py + size2, ph) - (py + size);

    // POS 按 TU 位置 (x0,y0) 采样重建平面 rec（含负偏移），与 ffmpeg 的
    // src[(x) + stride*(y)] 同义；扩展参考同样用 TU 位置，不跨 CU。
    const int plane_stride = is_chroma ? chroma.stride : stride;
    const std::uint8_t* src = is_chroma
        ? chroma.rec + static_cast<std::size_t>(py) * plane_stride + px
        : rec + static_cast<std::size_t>(y0) * stride + x0;
    const auto POS = [&](int x, int y) -> int { return src[x + y * plane_stride]; };

    if (cand_up_left) { L[-1] = POS(-1, -1); T[-1] = L[-1]; }
    if (cand_up)
        for (int i = 0; i < size; ++i) T[i] = POS(i, -1);
    if (cand_up_right) {
        for (int i = 0; i < size; ++i) T[size + i] = POS(size + i, -1);
        const int last = T[size + top_right_size - 1];
        for (int i = size + top_right_size; i < size2 + 8; ++i) T[i] = last;
    }
    if (cand_left)
        for (int i = 0; i < size; ++i) L[i] = POS(-1, i);
    if (cand_bottom_left) {
        for (int i = 0; i < bottom_left_size; ++i) L[size + i] = POS(-1, size + i);
        const int last = L[size + bottom_left_size - 1];
        for (int i = size + bottom_left_size; i < size2 + 8; ++i) L[i] = last;
    }

    const auto extend = [](int* p, int val, int len) {
        for (int i = 0; i < len; ++i) p[i] = val;
    };
    if (!cand_bottom_left) {
        if (cand_left)
            extend(L + size, L[size - 1], size);
        else if (cand_up_left) { extend(L, L[-1], size2 + 8); cand_left = 1; }
        else if (cand_up) { L[-1] = T[0]; extend(L, L[-1], size2 + 8); cand_up_left = 1; cand_left = 1; }
        else if (cand_up_right) {
            extend(T, T[size], size); L[-1] = T[size];
            extend(L, L[-1], size2 + 8); cand_up = 1; cand_up_left = 1; cand_left = 1;
        } else { L[-1] = 128; extend(T, L[-1], size2 + 8); extend(L, L[-1], size2 + 8); }
    }
    if (!cand_left) extend(L, L[size], size);
    if (!cand_up_left) L[-1] = L[0];
    if (!cand_up) extend(T, L[-1], size);
    if (!cand_up_right) extend(T + size, T[size - 1], size);
    T[-1] = L[-1];

    // 参考样本平滑：luma 4x4 永远跳过（FFmpeg：size!=4 才 3-tap）。
    // ref_filter_3tap 保留供 8x8+ 路径，本函数固定 size=4 不触发。

    const auto put = [&](int x, int y, int v) {
        dst[y * size + x] = static_cast<std::uint8_t>(v);
    };
    if (mode == 0) {  // Planar
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                put(x, y, ((size - 1 - x) * L[y] + (x + 1) * T[size] +
                           (size - 1 - y) * T[x] + (y + 1) * L[size] + size) >>
                              (log2_of(size) + 1));
    } else if (mode == 1) {  // DC
        int s = size;
        for (int i = 0; i < size; ++i) s += T[i] + L[i];
        const int dc = s >> (log2_of(size) + 1);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) put(x, y, dc);
        // 亮度且 nTbS < 32 时无条件做首行/首列边界滤波（FFmpeg pred_template.c:451-457
        // 与 HEVC 8.4.4.2.5 的 both-available 分支同式）；与“是否图像边界”无关。
        if (!is_chroma) {
            put(0, 0, (L[0] + 2 * dc + T[0] + 2) >> 2);
            for (int x = 1; x < size; ++x) put(x, 0, (T[x] + 3 * dc + 2) >> 2);
            for (int y = 1; y < size; ++y) put(0, y, (L[y] + 3 * dc + 2) >> 2);
        }
    } else {  // 角度模式 2..34
        static const int ap[] = {32, 26, 21, 17, 13, 9, 5, 2, 0, -2, -5, -9, -13, -17, -21,
                                 -26, -32, -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13,
                                 17, 21, 26, 32};
        static const int inv[] = {-4096, -1638, -910, -630, -482, -390, -315, -256,
                                  -315, -390, -482, -630, -910, -1638, -4096};
        const int angle = ap[mode - 2];
        std::vector<int> refbuf(3 * size + 4);
        int* ref_tmp = &refbuf[size];
        const int* ref;
        const int last = (size * angle) >> 5;
        if (mode >= 18) {  // 垂直类（参考 top）
            ref = T - 1;
            if (angle < 0 && last < -1) {
                for (int x = 0; x <= size; ++x) ref_tmp[x] = ref[x];
                for (int x = last; x <= -1; ++x)
                    ref_tmp[x] = L[-1 + ((x * inv[mode - 11] + 128) >> 8)];
                ref = ref_tmp;
            }
            for (int y = 0; y < size; ++y) {
                const int idx = ((y + 1) * angle) >> 5;
                const int fact = ((y + 1) * angle) & 31;
                if (fact)
                    for (int x = 0; x < size; ++x)
                        put(x, y, ((32 - fact) * ref[x + idx + 1] +
                                  fact * ref[x + idx + 2] + 16) >> 5);
                else
                    for (int x = 0; x < size; ++x) put(x, y, ref[x + idx + 1]);
            }
            // FFmpeg pred_template.c:515：mode 26 的首列后处理带 c_idx==0，
            // 色度不做（与 DC 首行/首列滤波、mode 10 同属亮度专属）。
            if (mode == 26 && !is_chroma)
                for (int y = 0; y < size; ++y)
                    put(0, y, std::clamp(T[0] + ((L[y] - L[-1]) >> 1), 0, 255));
        } else {  // 水平类（参考 left）
            ref = L - 1;
            if (angle < 0 && last < -1) {
                for (int x = 0; x <= size; ++x) ref_tmp[x] = ref[x];
                for (int x = last; x <= -1; ++x)
                    ref_tmp[x] = T[-1 + ((x * inv[mode - 11] + 128) >> 8)];
                ref = ref_tmp;
            }
            for (int x = 0; x < size; ++x) {
                const int idx = ((x + 1) * angle) >> 5;
                const int fact = ((x + 1) * angle) & 31;
                if (fact)
                    for (int y = 0; y < size; ++y)
                        put(x, y, ((32 - fact) * ref[y + idx + 1] +
                                  fact * ref[y + idx + 2] + 16) >> 5);
                else
                    for (int y = 0; y < size; ++y) put(x, y, ref[y + idx + 1]);
            }
            // FFmpeg pred_template.c:542：mode 10 的首行后处理同样仅 c_idx==0。
            if (mode == 10 && !is_chroma)
                for (int x = 0; x < size; ++x)
                    put(x, 0, std::clamp(L[0] + ((T[x] - T[-1]) >> 1), 0, 255));
        }
    }
}

int choose_intra_luma_mode_8x8(const std::uint8_t* src, int src_stride,
                               const std::uint8_t* rec, int rec_stride,
                               int x0, int y0, int pic_w, int pic_h, int qp) {
    if (x0 < 0 || y0 < 0 || x0 + 8 > pic_w || y0 + 8 > pic_h) return 1;  // INTRA_DC

    // 逐 TU 预测要读左/上/右上/左下邻域，故在重建平面的副本上模拟，保证候选模式
    // 看到的「块外已重建样本」与最终编码一字不差。
    //
    // 性能：4K 下每个 CU 都「分配整帧 + 整帧 memcpy」约 12 MB × 32640 CU ≈ 390 GB，
    // 是模式决策 564 秒/帧的主因。predict_intra_luma_4x4 的 POS() 读取范围严格落在
    // (x0-1..x0+7, y0-1..y0+7) 这个 9x9 窗口内（左上角、上方 4 + top-right 4、
    // 左侧 4 + bottom-left 4），窗口外的值从不参与预测。故改为复用持久缓冲，
    // 只在尺寸变化时分配，每次调用仅同步该窗口；语义与整帧副本逐位相同。
    static thread_local std::vector<std::uint8_t> trial;
    const std::size_t total = static_cast<std::size_t>(pic_w) * static_cast<std::size_t>(pic_h);
    if (trial.size() != total) trial.assign(total, 0);
    for (int y = y0 - 1; y <= y0 + 7; ++y) {
        if (y < 0 || y >= pic_h) continue;
        for (int x = x0 - 1; x <= x0 + 7; ++x) {
            if (x < 0 || x >= pic_w) continue;
            trial[static_cast<std::size_t>(y) * pic_w + x] =
                rec[static_cast<std::size_t>(y) * rec_stride + x];
        }
    }

    int best_mode = 1;
    long long best_ssd = 1LL << 60;
    for (int mode = 0; mode < 35; ++mode) {
        // 每个候选模式都从同一份「本 CU 尚未重建」的基准出发（只回滚本 CU 8x8）。
        for (int y = 0; y < 8; ++y)
            std::memcpy(&trial[static_cast<std::size_t>(y0 + y) * pic_w + x0],
                        &rec[static_cast<std::size_t>(y0 + y) * rec_stride + x0], 8);

        long long ssd = 0;
        for (int i = 0; i < 4; ++i) {
            const int tx = x0 + ((i & 1) ? 4 : 0);
            const int ty = y0 + ((i & 2) ? 4 : 0);
            std::uint8_t pred[16];
            predict_intra_luma_4x4(mode, trial.data(), pic_w, tx, ty, pic_w, pic_h, pred);
            std::int16_t level[16];
            encode_tu_4x4(&src[static_cast<std::size_t>(ty) * src_stride + tx], src_stride,
                          pred, &trial[static_cast<std::size_t>(ty) * pic_w + tx], pic_w,
                          level, qp, /*use_dst=*/true);
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    const int d =
                        static_cast<int>(src[static_cast<std::size_t>(ty + y) * src_stride + tx + x]) -
                        static_cast<int>(trial[static_cast<std::size_t>(ty + y) * pic_w + tx + x]);
                    ssd += static_cast<long long>(d) * d;
                }
        }
        if (ssd < best_ssd) {
            best_ssd = ssd;
            best_mode = mode;
        }
    }
    return best_mode;
}

}  // namespace termchart::image::codec::hevc
