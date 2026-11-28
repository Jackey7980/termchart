// intra_encoder.cpp —— 真实帧内编码主循环。
//
// 每个 16x16 划分单元做两轮决策：
//   ① SPLIT（四个 8x8 子块）：逐块在 13 个基础帧内模式里做 RDO ——
//      预测 → 残差 → 正向 DCT8 → 量化 → 反量化 + 反 DCT8 → 重建；
//      代价 = 真实重建像素的 SSD + λ × 区间编码器实测比特数（非幅值近似）。
//   ② NONE（单个 16x16 块、skip=1、纯预测）：与四个子块代价之和比较。
// 邻居上下文（上/左的模式与 skip）按 4x4 粒度记录，编码顺序与
// write_tile_group_intra_frame 的树遍历完全一致（超块内按 z 序走 16x16 单元）。
#include "intra_encoder.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "../bitstream/tile_group.hpp"
#include "../predict/intra_predict.hpp"
#include "../transform/quant.hpp"
#include "../transform/transform_2d.hpp"

namespace termchart::image::codec::av1 {

namespace {

constexpr int kMi = 4;      // 上下文粒度（4x4）
constexpr int kAlign = 64;  // 帧对齐到完整超块
constexpr int kBlock = 8;   // 亮度变换块边长
constexpr int kUnit = 16;   // 划分单元边长

constexpr int kModeDc = 0;
constexpr int kModeSmooth = 9;  // SMOOTH_PRED..SMOOTH_H_PRED = 9..11

inline int round_div(int num, int den) {
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

inline std::uint8_t clip_u8(int v) {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// libaom get_intra_edge_filter_type：上/左邻居属于 SMOOTH 族时边界滤波类型取 1。
inline bool is_smooth_mode(int mode) {
    return mode == kModeSmooth || mode == kModeSmooth + 1 || mode == kModeSmooth + 2;
}

// ---------------------------------------------------------------- 邻居上下文
struct SideCtx {
    int mi_rows = 1;
    int mi_cols = 1;
    std::vector<std::uint8_t> mode;  // 覆盖该 4x4 的块 y_mode
    std::vector<std::uint8_t> skip;  // 覆盖该 4x4 的块 skip 标志

    void init(int rows, int cols) {
        mi_rows = std::max(rows, 1);
        mi_cols = std::max(cols, 1);
        const std::size_t n = static_cast<std::size_t>(mi_rows) * mi_cols;
        mode.assign(n, kModeDc);
        skip.assign(n, 0);
    }

    std::size_t idx(int col, int row) const {
        return static_cast<std::size_t>(row) * mi_cols + static_cast<std::size_t>(col);
    }

    int mode_above(int col, int row) const {
        return row > 0 ? static_cast<int>(mode[idx(col, row - 1)]) : kModeDc;
    }
    int mode_left(int col, int row) const {
        return col > 0 ? static_cast<int>(mode[idx(col - 1, row)]) : kModeDc;
    }
    int skip_above(int col, int row) const {
        return row > 0 ? static_cast<int>(skip[idx(col, row - 1)]) : 0;
    }
    int skip_left(int col, int row) const {
        return col > 0 ? static_cast<int>(skip[idx(col - 1, row)]) : 0;
    }

    void commit(int x, int y, int w, int h, int y_mode, int skip_flag) {
        const int c0 = x / kMi, r0 = y / kMi;
        for (int r = r0; r < r0 + h / kMi; ++r) {
            for (int c = c0; c < c0 + w / kMi; ++c) {
                if (c >= mi_cols || r >= mi_rows) continue;
                mode[idx(c, r)] = static_cast<std::uint8_t>(y_mode);
                skip[idx(c, r)] = static_cast<std::uint8_t>(skip_flag);
            }
        }
    }
};

// 单元 4x4 区域的上下文快照（在 16x16 NONE 与 SPLIT 之间来回切换用）。
struct CellSnapshot {
    int c0 = 0;
    int r0 = 0;
    std::array<std::uint8_t, 16> mode{};
    std::array<std::uint8_t, 16> skip{};

    static CellSnapshot take(const SideCtx& ctx, int x, int y) {
        CellSnapshot s;
        s.c0 = x / kMi;
        s.r0 = y / kMi;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                const std::size_t i = static_cast<std::size_t>(r * 4 + c);
                s.mode[i] = ctx.mode[ctx.idx(s.c0 + c, s.r0 + r)];
                s.skip[i] = ctx.skip[ctx.idx(s.c0 + c, s.r0 + r)];
            }
        }
        return s;
    }

    void restore(SideCtx& ctx) const {
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                const std::size_t i = static_cast<std::size_t>(r * 4 + c);
                ctx.mode[ctx.idx(c0 + c, r0 + r)] = mode[i];
                ctx.skip[ctx.idx(c0 + c, r0 + r)] = skip[i];
            }
        }
    }
};

// ---------------------------------------------------------------- 预测样本
// 直接指向重建平面：above 沿行连续，left 按平面 stride 逐行下行（与已验证的
// 8x16 帧内路径完全一致的取法）。
IntraSamples make_samples(const std::uint8_t* plane, int stride, int x, int y, int w, int h,
                          bool have_above, bool have_left, bool have_above_right) {
    IntraSamples s;
    if (have_above) {
        s.above = plane + static_cast<std::size_t>(y - 1) * stride + x;
        s.num_above = w;
        s.num_above_right = have_above_right ? w : -1;
    }
    if (have_left) {
        s.left = plane + static_cast<std::size_t>(y) * stride + (x - 1);
        s.left_stride = stride;
        s.num_left = h;
        s.num_below_left = -1;  // 左下块在解码顺序上更晚，当前块预测时尚未重建
    }
    if (have_above && have_left)
        s.corner = plane[static_cast<std::size_t>(y - 1) * stride + x - 1];
    return s;
}

// 右上是否已重建：既要在帧内，也不能落在「本单元之后才解码」的区域
// （16 宽单元按 z 序遍历时，by>0 的块右上属于右侧单元）。
bool above_right_ready(int x, int w, int frame_w, bool sub_top_row, bool sub_left_col) {
    return x + 2 * w <= frame_w && (sub_top_row || sub_left_col);
}

// ---------------------------------------------------------------- 量化
Residual8x8 quantize(const std::int16_t* fwd, QuantSteps q) {
    Residual8x8 res;
    res.qm_y = 15;  // 中性量化矩阵（帧头 using_qmatrix 的平铺项）
    for (int rc = 0; rc < 64; ++rc) {
        const int col = rc >> 3, row = rc & 7;
        const int step = rc == 0 ? q.dc : q.ac;
        const int c = fwd[row * 8 + col];
        const int mag = std::min((std::abs(c) + step / 2) / step, 32767);
        res.coeff[static_cast<std::size_t>(rc)] = static_cast<std::int16_t>(c < 0 ? -mag : mag);
    }
    return res;
}

bool residual_is_zero(const Residual8x8& r) {
    for (const auto c : r.coeff) {
        if (c != 0) return false;
    }
    return true;
}

// ---------------------------------------------------------------- 公共上下文
struct EncCommon {
    const PlaneFrame420* src = nullptr;
    const PlaneFrame420* rec = nullptr;
    int stride = 0;
    int frame_w = 0;
    QuantSteps q{};
    int lambda = 1;
    int base_q_idx = 100;
    bool prediction_only = false;
};

// ---------------------------------------------------------------- 8x8 决策
struct LumaChoice {
    std::int64_t cost = std::numeric_limits<std::int64_t>::max();
    std::int64_t sse = 0;
    BlockMode8x8 mode;
    Residual8x8 residual;
    int skip = 1;
    std::array<std::uint8_t, 64> recon{};
};

LumaChoice eval_luma_mode(const EncCommon& enc, const SideCtx& ctx, int px, int py, int bx,
                          int by, int mode) {
    const int x = px + bx, y = py + by;
    const bool have_above = y > 0;
    const bool have_left = x > 0;
    const bool have_tr =
        have_above && above_right_ready(x, kBlock, enc.frame_w, by == 0, bx == 0);

    const int above_raw = ctx.mode_above(x / kMi, y / kMi);
    const int left_raw = ctx.mode_left(x / kMi, y / kMi);

    IntraPredictParams p;
    p.mode = mode;
    p.width = kBlock;
    p.height = kBlock;
    p.plane = 0;
    p.neighbor_smooth = (have_above && is_smooth_mode(above_raw)) ||
                        (have_left && is_smooth_mode(left_raw));

    const IntraSamples s = make_samples(enc.rec->y.data(), enc.stride, x, y, kBlock, kBlock,
                                        have_above, have_left, have_tr);

    std::array<std::uint8_t, 64> pred{};
    predict_intra(s, p, pred.data(), kBlock);

    std::array<std::int32_t, 64> src_buf{};
    std::array<std::int32_t, 64> residual{};
    for (int r = 0; r < kBlock; ++r) {
        for (int c = 0; c < kBlock; ++c) {
            const std::size_t i = static_cast<std::size_t>(r * kBlock + c);
            src_buf[i] = enc.src->y[static_cast<std::size_t>(y + r) * enc.stride + x + c];
            residual[i] = src_buf[i] - pred[i];
        }
    }

    std::int16_t fwd[64];
    av1_forward_transform_2d(residual.data(), fwd, kBlock, kBlock, Av1Transform1d::Dct,
                             Av1Transform1d::Dct);

    LumaChoice choice;
    choice.mode.y_mode = mode;
    choice.mode.uv_mode = kModeDc;
    choice.residual = quantize(fwd, enc.q);
    choice.skip = residual_is_zero(choice.residual) ? 1 : 0;
    if (enc.prediction_only) {
        choice.residual = Residual8x8{};
        choice.skip = 1;
    }

    const auto signal = av1_intra_residual_signal_8x8(choice.residual, enc.base_q_idx);
    std::int64_t sse = 0;
    for (int i = 0; i < 64; ++i) {
        const int rec_val = choice.skip
                                ? static_cast<int>(pred[static_cast<std::size_t>(i)])
                                : clip_u8(static_cast<int>(pred[static_cast<std::size_t>(i)]) +
                                          signal[static_cast<std::size_t>(i)]);
        choice.recon[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(rec_val);
        const std::int64_t d = static_cast<std::int64_t>(
            src_buf[static_cast<std::size_t>(i)] - rec_val);
        sse += d * d;
    }
    choice.sse = sse;

    const int skip_ctx = ctx.skip_above(x / kMi, y / kMi) + ctx.skip_left(x / kMi, y / kMi);
    const std::size_t bytes = av1_measure_block_bytes(
        choice.mode, av1_intra_mode_context(above_raw), av1_intra_mode_context(left_raw), skip_ctx,
        choice.skip, choice.residual);
    choice.cost = sse + enc.lambda * static_cast<std::int64_t>(bytes) * 8;
    return choice;
}

// 13 个基础帧内模式（angle_delta=0 全部有解码端逐像素对齐验证）。

constexpr std::array<int, 13> kLumaModes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// 16x16 大块候选：跳过 DC_PRED（规范不允许 16x16 用 DC 而无 filter intra 标记），
// 同时天然避开「上边界未重建时向帧外取样本」的边角情况。
constexpr std::array<int, 12> kUnit16Modes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

// ---------------------------------------------------------------- 16x16 决策
struct Unit16Choice {
    std::int64_t cost = std::numeric_limits<std::int64_t>::max();
    int y_mode = kUnit16Modes[0];
};

Unit16Choice eval_unit16(const EncCommon& enc, const SideCtx& ctx, int px, int py) {
    const bool have_above = py > 0;
    const bool have_left = px > 0;
    const bool have_tr = have_above && above_right_ready(px, kUnit, enc.frame_w, true, true);

    const int above_raw = ctx.mode_above(px / kMi, py / kMi);
    const int left_raw = ctx.mode_left(px / kMi, py / kMi);
    const int skip_ctx = ctx.skip_above(px / kMi, py / kMi) + ctx.skip_left(px / kMi, py / kMi);

    const IntraSamples s = make_samples(enc.rec->y.data(), enc.stride, px, py, kUnit, kUnit,
                                        have_above, have_left, have_tr);

    Unit16Choice best;
    for (const int mode : kUnit16Modes) {
        IntraPredictParams p;
        p.mode = mode;
        p.width = kUnit;
        p.height = kUnit;
        p.plane = 0;
        p.neighbor_smooth = (have_above && is_smooth_mode(above_raw)) ||
                            (have_left && is_smooth_mode(left_raw));

        std::array<std::uint8_t, 256> pred{};
        predict_intra(s, p, pred.data(), kUnit);

        std::int64_t sse = 0;
        for (int r = 0; r < kUnit; ++r) {
            for (int c = 0; c < kUnit; ++c) {
                const std::size_t i = static_cast<std::size_t>(r * kUnit + c);
                const std::int64_t d = static_cast<std::int64_t>(
                    enc.src->y[static_cast<std::size_t>(py + r) * enc.stride + px + c] - pred[i]);
                sse += d * d;
            }
        }

        BlockMode8x8 m;
        m.y_mode = mode;
        m.uv_mode = kModeDc;
        const std::size_t bytes = av1_measure_block_bytes(
            m, av1_intra_mode_context(above_raw), av1_intra_mode_context(left_raw), skip_ctx, 1,
            Residual8x8{});
        const std::int64_t cost = sse + enc.lambda * static_cast<std::int64_t>(bytes) * 8;
        if (cost < best.cost) {
            best.cost = cost;
            best.y_mode = mode;
        }
    }
    return best;
}

// ---------------------------------------------------------------- 重建回写
void write_plane(std::uint8_t* plane, int stride, int x, int y, int w, int h,
                 const std::uint8_t* src) {
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            plane[static_cast<std::size_t>(y + r) * stride + x + c] =
                src[static_cast<std::size_t>(r * w + c)];
        }
    }
}

// 色度恒为 DC_PRED：4:2:0 下 4x4（SPLIT 子块）或 8x8（16x16 块）。
void write_chroma_dc(std::uint8_t* plane, int stride, int cx, int cy, int w, int h) {
    const IntraSamples s = make_samples(plane, stride, cx, cy, w, h, cy > 0, cx > 0, false);
    IntraPredictParams p;
    p.mode = kModeDc;
    p.width = w;
    p.height = h;
    p.plane = 1;
    std::array<std::uint8_t, 64> tmp{};
    predict_intra(s, p, tmp.data(), w);
    write_plane(plane, stride, cx, cy, w, h, tmp.data());
}

}  // namespace

// ---------------------------------------------------------------- 位图 → 4:2:0
PlaneFrame420 bitmap_to_yuv420(const Bitmap& bitmap) {
    PlaneFrame420 frame;
    frame.width = static_cast<int>(bitmap.width());
    frame.height = static_cast<int>(bitmap.height());
    if (frame.width <= 0 || frame.height <= 0) return frame;

    const int w = frame.width, h = frame.height;
    const int cw = frame.chroma_width(), ch = frame.chroma_height();
    frame.y.assign(static_cast<std::size_t>(w) * h, 0);
    frame.u.assign(static_cast<std::size_t>(cw) * ch, 128);
    frame.v.assign(frame.u.size(), 128);

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const std::uint32_t x = static_cast<std::uint32_t>(col);
            const std::uint32_t y = static_cast<std::uint32_t>(row);
            const int r = static_cast<int>(bitmap.at(x, y, 0));
            const int g = static_cast<int>(bitmap.at(x, y, 1));
            const int b = static_cast<int>(bitmap.at(x, y, 2));
            frame.y[static_cast<std::size_t>(row) * w + col] =
                clip_u8(round_div(77 * r + 150 * g + 29 * b, 256));
        }
    }

    for (int cy = 0; cy < ch; ++cy) {
        for (int cx = 0; cx < cw; ++cx) {
            int sr = 0, sg = 0, sb = 0, n = 0;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int x = cx * 2 + dx, y = cy * 2 + dy;
                    if (x >= w || y >= h) continue;
                    const std::uint32_t ux = static_cast<std::uint32_t>(x);
                    const std::uint32_t uy = static_cast<std::uint32_t>(y);
                    sr += static_cast<int>(bitmap.at(ux, uy, 0));
                    sg += static_cast<int>(bitmap.at(ux, uy, 1));
                    sb += static_cast<int>(bitmap.at(ux, uy, 2));
                    ++n;
                }
            }
            if (n == 0) continue;
            const int r = round_div(sr, n), g = round_div(sg, n), b = round_div(sb, n);
            frame.u[static_cast<std::size_t>(cy) * cw + cx] =
                clip_u8(128 + round_div(-43 * r - 85 * g + 128 * b, 256));
            frame.v[static_cast<std::size_t>(cy) * cw + cx] =
                clip_u8(128 + round_div(128 * r - 107 * g - 21 * b, 256));
        }
    }
    return frame;
}

// ---------------------------------------------------------------- 编码主循环
IntraEncodeResult encode_intra_frame(const PlaneFrame420& input,
                                     const IntraEncodeOptions& options) {
    IntraEncodeResult result;
    const int in_w = input.width, in_h = input.height;
    if (in_w < kBlock || in_h < kBlock) return result;
    if (input.y.size() < static_cast<std::size_t>(in_w) * in_h) return result;

    // 对齐到完整 64x64 超块：解码端对越界块仍会读划分/模式符号，只有铺满整个
    // 超块网格（所有块都完整落在帧内）才能保证符号序列一一对应。
    const int pw = (in_w + kAlign - 1) / kAlign * kAlign;
    const int ph = (in_h + kAlign - 1) / kAlign * kAlign;
    const int cw = pw / 2, ch = ph / 2;

    PlaneFrame420 src;
    src.width = pw;
    src.height = ph;
    src.y.assign(static_cast<std::size_t>(pw) * ph, 0);
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            const int sx = std::min(x, in_w - 1), sy = std::min(y, in_h - 1);
            src.y[static_cast<std::size_t>(y) * pw + x] =
                input.y[static_cast<std::size_t>(sy) * in_w + sx];
        }
    }

    const int in_cw = input.chroma_width(), in_ch = input.chroma_height();
    src.u.assign(static_cast<std::size_t>(cw) * ch, 128);
    src.v.assign(src.u.size(), 128);
    const bool have_chroma =
        in_cw > 0 && in_ch > 0 &&
        input.u.size() >= static_cast<std::size_t>(in_cw) * in_ch &&
        input.v.size() == input.u.size();
    if (have_chroma) {
        for (int y = 0; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) {
                const int sx = std::min(x, in_cw - 1), sy = std::min(y, in_ch - 1);
                const std::size_t si = static_cast<std::size_t>(sy) * in_cw + sx;
                src.u[static_cast<std::size_t>(y) * cw + x] = input.u[si];
                src.v[static_cast<std::size_t>(y) * cw + x] = input.v[si];
            }
        }
    }

    PlaneFrame420 rec;
    rec.width = pw;
    rec.height = ph;
    rec.y.assign(static_cast<std::size_t>(pw) * ph, 0);
    rec.u.assign(static_cast<std::size_t>(cw) * ch, 128);
    rec.v.assign(rec.u.size(), 128);

    const QuantSteps q = quant_steps_from_qindex(options.base_q_idx);
    EncCommon enc;
    enc.src = &src;
    enc.rec = &rec;
    enc.stride = pw;
    enc.frame_w = pw;
    enc.q = q;
    enc.lambda = std::max(1, (q.ac * q.ac) / std::max(1, options.lambda_div));
    enc.base_q_idx = options.base_q_idx;
    enc.prediction_only = options.prediction_only;

    const int units_x = pw / kUnit;
    const int units_y = ph / kUnit;
    IntraFramePlan plan;
    plan.width = pw;
    plan.height = ph;
    plan.split16.assign(static_cast<std::size_t>(units_x) * units_y, 1);
    plan.mode16.assign(plan.split16.size(), BlockMode8x8{});
    plan.blocks16.assign(plan.split16.size(), std::array<FrameBlock8x8, 4>{});

    SideCtx ctx;
    ctx.init(ph / kMi, pw / kMi);

    // 遍历顺序必须与 write_tile_group_intra_frame 一致：超块光栅 → 32x32 象限
    // → 16x16 单元，各自按 (0,0)/(1,0)/(0,1)/(1,1) 的 z 序。
    for (int sb_y = 0; sb_y < ph; sb_y += 64) {
        for (int sb_x = 0; sb_x < pw; sb_x += 64) {
            for (int qy = 0; qy < 2; ++qy) {
                for (int qx = 0; qx < 2; ++qx) {
                    for (int uy = 0; uy < 2; ++uy) {
                        for (int ux = 0; ux < 2; ++ux) {
                            const int px = sb_x + qx * 32 + ux * kUnit;
                            const int py = sb_y + qy * 32 + uy * kUnit;
                            const std::size_t unit =
                                static_cast<std::size_t>(py / kUnit) * units_x + px / kUnit;

                            const CellSnapshot snap = CellSnapshot::take(ctx, px, py);

                            // ① SPLIT：四个 8x8 子块按 z 序，上下文随实际解码顺序推进
                            std::array<FrameBlock8x8, 4> blocks;
                            std::int64_t split_cost = 0;
                            for (int k = 0; k < 4; ++k) {
                                const int bx = (k & 1) * kBlock, by = (k >> 1) * kBlock;
                                const int x = px + bx, y = py + by;

                                LumaChoice best;
                                if (options.rdo_mode) {
                                    for (const int mode : kLumaModes) {
                                        const LumaChoice c =
                                            eval_luma_mode(enc, ctx, px, py, bx, by, mode);
                                        if (c.cost < best.cost) best = c;
                                    }
                                } else {
                                    best = eval_luma_mode(enc, ctx, px, py, bx, by, kModeSmooth);
                                }

                                blocks[static_cast<std::size_t>(k)].mode = best.mode;
                                blocks[static_cast<std::size_t>(k)].residual = best.residual;
                                blocks[static_cast<std::size_t>(k)].skip = best.skip;
                                split_cost += best.cost;

                                write_plane(rec.y.data(), pw, x, y, kBlock, kBlock,
                                            best.recon.data());
                                write_chroma_dc(rec.u.data(), cw, x / 2, y / 2, 4, 4);
                                write_chroma_dc(rec.v.data(), cw, x / 2, y / 2, 4, 4);
                                ctx.commit(x, y, kBlock, kBlock, best.mode.y_mode, best.skip);
                            }

                            bool use_none = false;
                            if (options.rdo_partition && px + 2 * kUnit <= pw) {
                                // ② NONE：上下文回到单元起点，再评估单个 16x16 块
                                snap.restore(ctx);
                                const Unit16Choice u = eval_unit16(enc, ctx, px, py);
                                if (u.cost < split_cost) {
                                    plan.split16[unit] = 0;
                                    BlockMode8x8 m;
                                    m.y_mode = u.y_mode;
                                    m.uv_mode = kModeDc;
                                    plan.mode16[unit] = m;

                                    const bool have_above = py > 0;
                                    const bool have_left = px > 0;
                                    const bool have_tr =
                                        have_above && (px + 2 * kUnit <= pw);
                                    IntraPredictParams p;
                                    p.mode = u.y_mode;
                                    p.width = kUnit;
                                    p.height = kUnit;
                                    p.plane = 0;
                                    p.neighbor_smooth =
                                        (have_above &&
                                         is_smooth_mode(ctx.mode_above(px / kMi, py / kMi))) ||
                                        (have_left &&
                                         is_smooth_mode(ctx.mode_left(px / kMi, py / kMi)));
                                    const IntraSamples s = make_samples(
                                        rec.y.data(), pw, px, py, kUnit, kUnit, have_above,
                                        have_left, have_tr);
                                    std::array<std::uint8_t, 256> pred{};
                                    predict_intra(s, p, pred.data(), kUnit);
                                    write_plane(rec.y.data(), pw, px, py, kUnit, kUnit,
                                                pred.data());
                                    write_chroma_dc(rec.u.data(), cw, px / 2, py / 2, 8, 8);
                                    write_chroma_dc(rec.v.data(), cw, px / 2, py / 2, 8, 8);
                                    ctx.commit(px, py, kUnit, kUnit, u.y_mode, 1);
                                    use_none = true;
                                }
                            }

                            if (!use_none) {
                                // 保留 SPLIT：把四个子块重新提交回上下文章
                                for (int k = 0; k < 4; ++k) {
                                    const int bx = (k & 1) * kBlock, by = (k >> 1) * kBlock;
                                    ctx.commit(px + bx, py + by, kBlock, kBlock,
                                               blocks[static_cast<std::size_t>(k)].mode.y_mode,
                                               blocks[static_cast<std::size_t>(k)].skip);
                                }
                                plan.split16[unit] = 1;
                                plan.blocks16[unit] = blocks;
                            }
                        }
                    }
                }
            }
        }
    }

    result.recon = std::move(rec);
    result.tile_data = write_tile_group_intra_frame(plan);
    return result;
}

}  // namespace termchart::image::codec::av1
