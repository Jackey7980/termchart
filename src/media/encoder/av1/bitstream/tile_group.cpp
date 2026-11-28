#include "tile_group.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "../../bitstream/bit_writer.hpp"
#include "../entropy/cdf_manager.hpp"
#include "../entropy/coef_cdf_data.hpp"
#include "../entropy/default_cdf.hpp"
#include "../entropy/range_coder.hpp"
#include "../predict/inter_predict.hpp"
#include "../predict/intra_predict.hpp"
#include "../predict/wedge_mask_8x8.hpp"
#include "../transform/quant.hpp"
#include "../transform/transform_2d.hpp"
#include "partition_tree.hpp"

#include "termchart/media/encoder/gpa/bit_ops.hpp"

namespace termchart::image::codec::av1 {
namespace {

constexpr int kEquiprobable = 16384;

int clip_pixel(int v) { return termchart::media::encoder::gpa::clip_pixel(v); }

// 与解码端 read_golomb 互逆：value = x - 1，先写 (位数-1) 个 0 再写 1，
// 随后写 x 的低 (位数-1) 位。
void write_golomb(RangeEncoder& enc, int value) {
    const unsigned d = static_cast<unsigned>(value) + 1u;
    int len = 0;
    while ((1u << len) <= d) ++len;
    for (int i = 1; i < len; ++i) enc.write_bool(0, kEquiprobable);
    enc.write_bool(1, kEquiprobable);
    for (int i = len - 2; i >= 0; --i)
        enc.write_bool((d >> i) & 1u, kEquiprobable);
}

struct CoefficientEncoder {
    RangeEncoder& enc;
    // libaom 的 levels 带 4 列 / 2 行padding；这里用 12x12 的独立 padded 缓冲，
    // 使下/右邻居的越界访问落在恒零边带上。
    std::array<unsigned char, 12 * 12> levels{};

    static std::size_t at(int x, int y) {
        return static_cast<std::size_t>((y + 2) * 12 + (x + 2));
    }
    unsigned char level(int x, int y) const { return levels[at(x, y)]; }
    void set_level(int rc, int magnitude) {
        levels[at(rc >> 3, rc & 7)] =
            static_cast<unsigned char>(std::min(magnitude, 255));
    }

    unsigned low_ctx(int rc) const {
        // libaom get_nz_map_ctx_from_stats / get_lower_levels_ctx 在 TX_CLASS_2D
        // 的 DC 上直接返回 0；AC 邻居幅值截到 3 后每两档折一个上下文。
        if (rc == 0) return 0u;
        const int x = rc >> 3, y = rc & 7;
        const int mag = std::min<int>(level(x, y + 1), 3) +
                        std::min<int>(level(x + 1, y), 3) +
                        std::min<int>(level(x + 1, y + 1), 3) +
                        std::min<int>(level(x, y + 2), 3) +
                        std::min<int>(level(x + 2, y), 3);
        const unsigned ctx = static_cast<unsigned>(std::min((mag + 1) >> 1, 4));
        return kLoCtxOffsetSquare[std::min(y, 4)][std::min(x, 4)] + ctx;
    }

    unsigned high_ctx(int rc) const {
        const int x = rc >> 3, y = rc & 7;
        const int mag = level(x + 1, y) + level(x, y + 1) + level(x + 1, y + 1);
        const unsigned ctx = static_cast<unsigned>(std::min((mag + 1) >> 1, 6));
        if (rc == 0) return ctx;
        return ctx + (((x | y) < 2) ? 7u : 14u);
    }

    static unsigned eob_high_ctx(int rc) {
        if (rc == 0) return 0;
        const int x = rc >> 3, y = rc & 7;
        return ((x | y) < 2) ? 7u : 14u;
    }

    void write_high(unsigned ctx, int magnitude) {
        const std::uint16_t* cdf = kBrTokLuma[ctx];
        for (int shift = 3; shift <= 12; shift += 3) {
            const int part = std::max(std::min(magnitude - shift, 3), 0);
            write_symbol(enc, cdf, 4, part);
            if (part < 3) return;
        }
    }

    void write_token(unsigned ctx, int rc, int magnitude) {
        const int token = std::min(magnitude, 3);
        write_symbol(enc, kBaseTokLuma[ctx], 4, token);
        if (token == 3) write_high(high_ctx(rc), magnitude);
        set_level(rc, magnitude);
    }

    void write_eob_token(unsigned ctx, int rc, int magnitude) {
        const int token = std::min(magnitude - 1, 2);
        write_symbol(enc, kEobBaseTokLuma[ctx], 3, token);
        if (token == 2) write_high(eob_high_ctx(rc), magnitude);
        set_level(rc, magnitude);
    }
};

// libaom av1_idct8：cos_bit=12 的定点阶段，阶段间按 16 位范围饱和即可。
void idct8_exact(const std::int32_t* in, std::int32_t* out) {
    constexpr int c8 = 4017, c16 = 3784, c24 = 3406, c32 = 2896;
    constexpr int c40 = 2276, c48 = 1567, c56 = 799;
    const auto btf = [](int w0, int x0, int w1, int x1) {
        return static_cast<std::int32_t>(
            (static_cast<std::int64_t>(w0) * x0 +
             static_cast<std::int64_t>(w1) * x1 + 2048) >> 12);
    };
    std::int32_t b[8], s[8];

    b[0] = in[0]; b[1] = in[4]; b[2] = in[2]; b[3] = in[6];
    b[4] = in[1]; b[5] = in[5]; b[6] = in[3]; b[7] = in[7];

    s[0] = b[0]; s[1] = b[1]; s[2] = b[2]; s[3] = b[3];
    s[4] = btf(c56, b[4], -c8, b[7]);
    s[5] = btf(c24, b[5], -c40, b[6]);
    s[6] = btf(c40, b[5], c24, b[6]);
    s[7] = btf(c8, b[4], c56, b[7]);

    b[0] = btf(c32, s[0], c32, s[1]);
    b[1] = btf(c32, s[0], -c32, s[1]);
    b[2] = btf(c48, s[2], -c16, s[3]);
    b[3] = btf(c16, s[2], c48, s[3]);
    b[4] = s[4] + s[5];
    b[5] = s[4] - s[5];
    b[6] = -s[6] + s[7];
    b[7] = s[6] + s[7];

    s[0] = b[0] + b[3];
    s[1] = b[1] + b[2];
    s[2] = b[1] - b[2];
    s[3] = b[0] - b[3];
    s[4] = b[4];
    s[5] = btf(-c32, b[5], c32, b[6]);
    s[6] = btf(c32, b[5], c32, b[6]);
    s[7] = b[7];

    out[0] = s[0] + s[7];
    out[1] = s[1] + s[6];
    out[2] = s[2] + s[5];
    out[3] = s[3] + s[4];
    out[4] = s[3] - s[4];
    out[5] = s[2] - s[5];
    out[6] = s[1] - s[6];
    out[7] = s[0] - s[7];
}

// inter_txtp：帧间块的 8x8 亮度变换类型符号。reduced_txtp_set=1 时解码端走
// dav1d `txtp_inter3[t_dim->min]` 这一条路——一个 2 符号布尔（1 ⇒ DCT_DCT，
// 0 ⇒ IDTX），与帧内 5 符号的 txtp_intra2 完全是两套语法。
void write_residual(RangeEncoder& enc, const Residual8x8& residual,
                    bool inter_txtp = false, int dc_sign_ctx = 0) {
    CoefficientEncoder coef_enc{enc};
    const auto& q = residual.coeff;

    int eob = 0;
    for (int i = 63; i >= 0; --i) {
        if (q[static_cast<std::size_t>(kScan8x8[i])] != 0) {
            eob = i + 1;
            break;
        }
    }

    // eob=0 由调用方写入的 txb_skip=1 表示，不能再写 tx_type/eob 符号。
    if (eob == 0) return;

    if (inter_txtp) {
        enc.write_bool(1, kTxTypeInter8x8[0]);  // 1 ⇒ DCT_DCT
    } else {
        // reduced_tx_set=1 → intra 8x8 属于 EXT_TX_SET_DTT4_IDTX（eset=2），
        // av1_num_ext_tx_set 为 5 符号；DCT_DCT 的符号索引 av1_ext_tx_ind[2][0] = 1。
        write_symbol(enc, kTxTypeIntra8x8Smooth, 5, 1);
    }

    // eob_pt = av1_get_eob_pos_token(eob)：组起点 1,2,3,5,9,17,...
    int eob_pt = 1, eob_extra = 0;
    if (eob > 1) {
        int v = eob - 1;
        eob_pt = 2;
        while (v > 1) {
            v >>= 1;
            ++eob_pt;
        }
        const int group_start =
            eob_pt >= 3 ? (1 << (eob_pt - 2)) + 1 : 2;
        eob_extra = eob - group_start;
    }
    const int offset_bits = eob_pt >= 3 ? eob_pt - 2 : 0;
    write_symbol(enc, kEobBin64Luma, 7, eob_pt - 1);
    if (offset_bits > 0) {
        const int eob_ctx = eob_pt - 3;
        const int shift = offset_bits - 1;
        enc.write_bool((eob_extra >> shift) & 1, kEobHiBitLuma[eob_ctx][0]);
        for (int i = 1; i < offset_bits; ++i)
            enc.write_bool((eob_extra >> (shift - i)) & 1, kEquiprobable);
    }

    // 解码端按反扫描顺序建立 levels，所有下/右邻居在读取当前系数前已就位。
    const auto scan_at = [&](int i) { return kScan8x8[static_cast<std::size_t>(i)]; };
    const auto mag_at = [&](int rc) {
        return std::abs(q[static_cast<std::size_t>(rc)]);
    };
    for (int i = 0; i < 64; ++i) coef_enc.set_level(i, mag_at(i));

    const int eob_rc = scan_at(eob - 1);
    const unsigned eob_ctx = eob <= 1 ? 0u : (eob <= 9 ? 1u : (eob <= 17 ? 2u : 3u));
    coef_enc.write_eob_token(eob_ctx, eob_rc, mag_at(eob_rc));

    for (int i = eob - 2; i >= 0; --i) {
        const int rc = scan_at(i);
        coef_enc.write_token(coef_enc.low_ctx(rc), rc, mag_at(rc));
    }

    // 符号位按扫描顺序写：DC 走 dc_sign_cdf，其余走裸比特；level >= 15 的系数
    // 在符号之后紧跟 golomb（最大到 base+br=15，超出部分进 golomb）。
    for (int i = 0; i < eob; ++i) {
        const int rc = scan_at(i);
        const int value = q[static_cast<std::size_t>(rc)];
        if (value == 0) continue;
        enc.write_bool(value < 0 ? 1 : 0,
                       i == 0 ? kDcSignLuma[std::min(dc_sign_ctx, 2)] : kEquiprobable);
        const int mag = std::abs(value);
        if (mag >= 15) write_golomb(enc, mag - 15);
    }
}

std::string wrap_tile_data(const std::string& tile_data) {
    // 单 tile 时 tile_start_and_end_present_flag / tile_size 都不存在（spec 5.11.1，
    // libaom `av1_get_tile_obu_header_size` 在 tiles_log2 为 0 时返回 0 字节），
    // OBU 载荷就是裸 tile data。
    return tile_data;
}

}  // namespace

// common_data.h 的 intra_mode_context：上/左块模式折叠成 kfym 的上下文。
constexpr int kIntraModeContext[13] = {0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0};

// 单块的帧内模式语法，顺序与 read_intra_frame_mode_info 一致：
// skip → y_mode → angle_delta → uv_mode → cfl_alpha → filter_intra。
void write_block_mode(RangeEncoder& enc, const CdfDefaultContext& cdf,
                      const BlockMode8x8& m, int above_ctx, int left_ctx,
                      int skip_ctx, int skip) {
    enc.write_bool(skip, cdf.m.skip[skip_ctx][0]);
    write_symbol(enc, cdf.kfym[above_ctx][left_ctx], N_INTRA_PRED_MODES, m.y_mode);
    if (m.y_mode >= 1 && m.y_mode <= 8) {
        // angle_delta 的 CDF 以 mode - V_PRED 为索引，符号值 = delta + 3。
        write_symbol(enc, cdf.m.angle_delta[m.y_mode - 1], 7, m.angle_delta + 3);
    }
    write_symbol(enc, cdf.m.uv_mode[1][m.y_mode], N_UV_INTRA_PRED_MODES, m.uv_mode);
    if (m.uv_mode == CFL_PRED) {
        write_symbol(enc, cdf.m.cfl_sign, 8, m.cfl_signs);
        const int sign = m.cfl_signs + 1;
        const int sign_u = (sign * 86) >> 8;
        const int sign_v = sign - 3 * sign_u;
        if (sign_u) {
            const int ctx = (sign_u == 2 ? 3 : 0) + sign_v;
            write_symbol(enc, cdf.m.cfl_alpha[ctx], 16, m.cfl_u_idx);
        }
        if (sign_v) {
            const int ctx = (sign_v == 2 ? 3 : 0) + sign_u;
            write_symbol(enc, cdf.m.cfl_alpha[ctx], 16, m.cfl_v_idx);
        }
    }
    if (m.y_mode == DC_PRED) {
        // BLOCK_8X8 = 3；dav1d 只在 DC_PRED 且尺寸 <=16 时读这个符号。
        enc.write_bool(m.filter_intra >= 0 ? 1 : 0, cdf.m.use_filter_intra[3][0]);
        if (m.filter_intra >= 0)
            write_symbol(enc, cdf.m.filter_intra, 5, m.filter_intra);
    }
}

void write_block_mode(RangeEncoder& enc, const CdfDefaultContext& cdf, int mode,
                      int angle_delta, int above_ctx, int left_ctx, int skip_ctx,
                      int skip) {
    BlockMode8x8 m;
    m.y_mode = mode;
    m.angle_delta = angle_delta;
    write_block_mode(enc, cdf, m, above_ctx, left_ctx, skip_ctx, skip);
}

// 4x4 粒度的残差上下文视图：0x40 表示该 4x4 无残差，低 6 位是 cul_level，
// bit7 是 DC 符号。空视图保持旧探针的「首块」上下文。
struct ResidualCtxView {
    std::uint8_t* data = nullptr;
    int rows = 0;
    int cols = 0;
};

// inter=false 走帧内 txtp_intra2，inter=true 走帧间 txtp_inter3。
void write_residual_block(RangeEncoder& enc, const Residual8x8& residual,
                          bool inter, const ResidualCtxView& ctx = {},
                          int mi_col = 0, int mi_row = 0) {
    bool all_zero = true;
    for (auto v : residual.coeff) all_zero = all_zero && (v == 0);

    int skip_ctx = 0, dc_sign_ctx = 0;
    const auto at = [&](int c, int r) -> int {
        if (!ctx.data || c < 0 || r < 0 || c >= ctx.cols || r >= ctx.rows) return 0x40;
        return ctx.data[static_cast<std::size_t>(r) * ctx.cols + c];
    };
    if (ctx.data) {
        const int above = at(mi_col, mi_row - 1) | at(mi_col + 1, mi_row - 1);
        const int left = at(mi_col - 1, mi_row) | at(mi_col - 1, mi_row + 1);
        skip_ctx = kSkipCtxLuma[std::min(above & 0x3F, 4)][std::min(left & 0x3F, 4)];
        const int sign_bits = ((at(mi_col, mi_row - 1) & 0xC0) != 0) +
                              ((at(mi_col + 1, mi_row - 1) & 0xC0) != 0) +
                              ((at(mi_col - 1, mi_row) & 0xC0) != 0) +
                              ((at(mi_col - 1, mi_row + 1) & 0xC0) != 0);
        dc_sign_ctx = sign_bits == 4 ? 0 : 1;
    }

    enc.write_bool(all_zero ? 1 : 0,
                   ctx.data ? kTxSkipLuma8x8[skip_ctx] : kTxBypassLuma8x8[0]);
    if (!all_zero) write_residual(enc, residual, inter, dc_sign_ctx);
    // U/V 各一个 4x4 变换块，恒为全零。
    enc.write_bool(1, kTxBypassChroma4x4[0]);
    enc.write_bool(1, kTxBypassChroma4x4[0]);

    if (ctx.data) {
        int cul = 0;
        for (auto v : residual.coeff) cul += std::abs(v);
        const int sign_bit = all_zero || residual.coeff[0] == 0
                                 ? 0x40
                                 : (residual.coeff[0] < 0 ? 0 : 0x80);
        const int value = all_zero ? 0x40 : (std::min(cul, 63) | sign_bit);
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                if (mi_col + c >= ctx.cols || mi_row + r >= ctx.rows) continue;
                ctx.data[static_cast<std::size_t>(mi_row + r) * ctx.cols + mi_col + c] =
                    static_cast<std::uint8_t>(value);
            }
        }
    }
}

void write_zero_residual(RangeEncoder& enc, const Residual8x8& residual) {
    write_residual_block(enc, residual, /*inter=*/false);
}

// 8x8 层的划分符号：read_partition 在 bsize >= BLOCK_8X8 且 has_rows/has_cols
// 为真时读一个 4 符号 partition_cdf（仓内 bl 索引 4 即 8x8 层）。帧左上角的
// 上下文是 0（上/左邻居都不在帧内），随后 PARTITION_NONE 会把上下文置回 0，
// 因此本帧所有块的上下文都是 0。
void write_partition_none(RangeEncoder& enc, const CdfDefaultContext& cdf) {
    write_symbol(enc, cdf.m.partition[4][0], 4, 0 /* PARTITION_NONE */);
}

// 帧右边界（has_rows && !has_cols）的 2 符号划分：read_partition 用
// partition_gather_horz_alike 把 partition_cdf 折叠成 {VERT, SPLIT}，
// 符号 1 表示 SPLIT。8x16 帧宽只有 2 个 mi，16x16 那一层必然走到这里。
// 折叠后的 CDF 只有一项：cdf[0] = 被剔除类型的概率和（即 SPLIT 侧的区间）。
void write_partition_split_at_right_edge(RangeEncoder& enc,
                                         const CdfDefaultContext& cdf, int level) {
    const std::uint16_t* in = cdf.m.partition[level][0];
    int sum = 0;
    for (int sym :
         {PARTITION_H, PARTITION_SPLIT, PARTITION_T_TOP_SPLIT, PARTITION_T_BOTTOM_SPLIT,
          PARTITION_T_LEFT_SPLIT, PARTITION_H4})
        sum += (sym > 0 ? in[sym - 1] : 32768) - in[sym];
    std::uint16_t cdf2[2] = {static_cast<std::uint16_t>(sum), 0};
    write_symbol(enc, cdf2, 2, 1 /* PARTITION_SPLIT */);
}

std::string write_tile_group_8x8() {
    // 全零残差也必须有 tx_skip（亮度）与两条色度 all_zero 符号，否则真实解码器
    // 会在 tile data 末尾读不到符号而判 corrupt。因此直接复用残差路径。
    return write_tile_group_8x8(Residual8x8{});
}

std::string write_tile_group_8x8(const Residual8x8& residual) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_none(enc, cdf);
    write_block_mode(enc, cdf, SMOOTH_PRED, 0, 0, 0, 0, 0);
    write_zero_residual(enc, residual);
    return wrap_tile_data(enc.finish());
}

std::string write_tile_group_8x16(const Residual8x8& residual_a, int mode_b,
                                  int angle_delta_b) {
    BlockMode8x8 m;
    m.y_mode = mode_b;
    m.angle_delta = angle_delta_b;
    return write_tile_group_8x16(residual_a, m);
}

std::string write_tile_group_8x16(const Residual8x8& residual_a,
                                  const BlockMode8x8& mode_b) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    // 16x16 层（level 3）：帧宽 2 个 mi，走右边界折叠 CDF，选 PARTITION_SPLIT，
    // 让下面两个 8x8 块各自带独立的模式信息。
    write_partition_split_at_right_edge(enc, cdf, 3);
    // 块 A（上 8x8）：带残差，用来给块 B 提供非平坦的上边界。
    write_partition_none(enc, cdf);
    write_block_mode(enc, cdf, SMOOTH_PRED, 0, 0, 0, 0, 0);
    write_zero_residual(enc, residual_a);

    // 块 B（下 8x8）：skip=1 → 没有任何残差符号，recon 就等于预测块。
    write_partition_none(enc, cdf);
    // 上方邻居是 SMOOTH_PRED（context 0），左侧不存在（按 DC_PRED → context 0）。
    write_block_mode(enc, cdf, mode_b,
                     kIntraModeContext[SMOOTH_PRED], kIntraModeContext[DC_PRED],
                     0, 1);
    return wrap_tile_data(enc.finish());
}

// 逆量化 + 反 DCT8 + 舍入，返回 (tmp+8)>>4 的残差信号（行主序整数）。
std::array<std::int32_t, 64> residual_signal_8x8(
    const Residual8x8& residual, QuantSteps quant = quant_steps_from_qindex(100)) {
    std::int32_t coef[64], tmp[64];
    // AV1 系数布局是转置的（index = col * 8 + row），IDCT 前还原成行主序。
    // using_qmatrix 时反量化步额外乘 8x8 权重：(dq * qm[rc] + 16) >> 5。
    const auto qm = av1_qmatrix_8x8(residual.qm_y, /*chroma=*/false);
    for (int rc = 0; rc < 64; ++rc) {
        const int col = rc >> 3, row = rc & 7;
        int dq = rc == 0 ? quant.dc : quant.ac;
        if (residual.qm_y < 15)
            dq = (dq * qm[static_cast<std::size_t>(rc)] + 16) >> 5;
        coef[row * 8 + col] = residual.coeff[static_cast<std::size_t>(rc)] * dq;
    }

    for (int y = 0; y < 8; ++y) idct8_exact(&coef[y * 8], &tmp[y * 8]);
    for (int i = 0; i < 64; ++i) tmp[i] = (tmp[i] + 1) >> 1;
    std::int32_t col[8], out_col[8];
    for (int x = 0; x < 8; ++x) {
        for (int y = 0; y < 8; ++y) col[y] = tmp[y * 8 + x];
        idct8_exact(col, out_col);
        for (int y = 0; y < 8; ++y) tmp[y * 8 + x] = out_col[y];
    }

    std::array<std::int32_t, 64> signal{};
    for (int i = 0; i < 64; ++i) signal[static_cast<std::size_t>(i)] = (tmp[i] + 8) >> 4;
    return signal;
}

std::array<std::uint8_t, 64> reconstruct_luma_8x8(const Residual8x8& residual) {
    const auto signal = residual_signal_8x8(residual);

    // 预测：左上角 8x8 没有上/左邻居，填充规则与解码端一致（above 127 / left 129）。
    std::array<std::uint8_t, 64> pred{};
    IntraSamples samples;
    samples.num_above = 0;
    samples.num_left = 0;
    IntraPredictParams params;
    params.mode = 9;  // SMOOTH_PRED
    params.width = 8;
    params.height = 8;
    predict_intra(samples, params, pred.data(), 8);

    std::array<std::uint8_t, 64> out{};
    for (int i = 0; i < 64; ++i)
        out[static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>(clip_pixel(pred[static_cast<std::size_t>(i)] +
                                                 signal[static_cast<std::size_t>(i)]));
    return out;
}

Reconstructed8x16 reconstruct_8x16(const Residual8x8& residual_a,
                                   const BlockMode8x8& mode_b) {
    Reconstructed8x16 out;
    const auto top = reconstruct_luma_8x8(residual_a);
    std::copy(top.begin(), top.end(), out.y.begin());

    // 块 B：上边界 = 块 A 的最后一行，左/右上/左下都不存在。
    IntraSamples samples;
    samples.above = top.data() + 7 * 8;
    samples.num_above = 8;
    samples.num_left = 0;
    IntraPredictParams params;
    params.mode = mode_b.y_mode;
    params.angle_delta = mode_b.angle_delta;
    params.filter_intra_mode = mode_b.filter_intra;
    params.width = 8;
    params.height = 8;
    // 上方邻居是块 A 的 SMOOTH_PRED，边界类型因此走 type 1。
    params.neighbor_smooth = true;
    predict_intra(samples, params, out.y.data() + 64, 8);

    out.u.fill(128);
    out.v.fill(128);
    if (mode_b.uv_mode == CFL_PRED) {
        const int sign = mode_b.cfl_signs + 1;
        const int sign_u = (sign * 86) >> 8;
        const int sign_v = sign - 3 * sign_u;
        const auto alpha = [](int s, int idx) {
            if (s == 0) return 0;
            const int abs_alpha = idx + 1;
            return s == 2 ? abs_alpha : -abs_alpha;
        };
        predict_cfl_chroma(out.y.data() + 64, 8, 4, 4,
                           alpha(sign_u, mode_b.cfl_u_idx),
                           alpha(sign_v, mode_b.cfl_v_idx),
                           out.u.data() + 16, out.v.data() + 16, 4);
    }
    return out;
}

std::array<std::uint8_t, 128> reconstruct_luma_8x16(const Residual8x8& residual_a,
                                                   int mode_b,
                                                   int angle_delta_b) {
    BlockMode8x8 m;
    m.y_mode = mode_b;
    m.angle_delta = angle_delta_b;
    return reconstruct_8x16(residual_a, m).y;
}

// MV 分量残差：diff = ((class<<3)|(fp<<1)|hp)+1，正负号单独编码。
void write_mv_component(RangeEncoder& enc, const CdfDefaultContext& cdf,
                        int comp, int delta) {
    const auto& mv = cdf.mv.comp;  // 默认 CDF 的 X/Y 分量共用一张表
    (void)comp;
    enc.write_bool(delta < 0 ? 1 : 0, mv.sign[0]);
    const int magnitude = std::abs(delta) - 1;
    // diff = ((up << 3) | (fp << 1) | hp) + 1；up<=1 走 class0，
    // up>=2 时 cls = bit_width(up)-1，up 的最高位由解码端按 1<<cls 补出。
    const int up = magnitude >> 3;
    const int cls = up <= 1 ? 0 : static_cast<int>(std::bit_width(static_cast<unsigned>(up))) - 1;
    write_symbol(enc, mv.classes, 11, cls);
    if (cls == 0) {
        enc.write_bool(up, mv.class0[0]);
        write_symbol(enc, mv.class0_fp[up], 4, (magnitude >> 1) & 3);
        enc.write_bool(magnitude & 1, mv.class0_hp[0]);
    } else {
        for (int n = 0; n < cls; ++n)
            enc.write_bool((up >> n) & 1, mv.classN[n][0]);
        write_symbol(enc, mv.classN_fp, 4, (magnitude >> 1) & 3);
        enc.write_bool(magnitude & 1, mv.classN_hp[0]);
    }
}

std::string write_tile_group_inter_8x8(int mv_x, int mv_y, int newmv_probe,
                                       const Residual8x8& residual) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_none(enc, cdf);
    // NEWMV 实验路径需要 skip=0：skip=1 时解码端不消费 MV 残差语法。
    enc.write_bool(newmv_probe < 0 ? 1 : 0, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    // 块 B 的上方邻居是 intra 块，解码端 av1_get_ref*_ctx 推出 ctx=1（不是 0）。
    enc.write_bool(0, cdf.m.ref[0][1][0]);
    enc.write_bool(0, cdf.m.ref[2][1][0]);
    enc.write_bool(0, cdf.m.ref[3][1][0]);
    if (newmv_probe < 0) {
        // 稳定主链：GLOBALMV + 全 IDENTITY 全局运动 → 零 MV 拷贝。
        enc.write_bool(1, cdf.m.newmv_mode[0][0]);
        enc.write_bool(0, cdf.m.globalmv_mode[0][0]);
    } else {
        // NEWMV 残差语法。newmv_mode=0 之后解码端直接进 NEWMV 分支，只有
        // n_mvs>1 时才插 drl_bit；单块 8x8 帧的 mvstack 只有全局零候选，
        // n_mvs==1，故这里不写 DRL 位。
        const int joint = (mv_x != 0 ? 1 : 0) | (mv_y != 0 ? 2 : 0);
        enc.write_bool(0, cdf.m.newmv_mode[0][0]);
        write_symbol(enc, cdf.mv.joint, 4, joint);
        // joint 的 bit1 = V(y)、bit0 = H(x)，与 read_mv_residual 的分量顺序一致。
        if (joint & 2) write_mv_component(enc, cdf, 0, mv_y);
        if (joint & 1) write_mv_component(enc, cdf, 1, mv_x);
        write_residual_block(enc, residual, /*inter=*/true);
    }
    return wrap_tile_data(enc.finish());
}

std::string write_tile_group_inter_8x8(int mv_x, int mv_y, int newmv_probe) {
    return write_tile_group_inter_8x8(mv_x, mv_y, newmv_probe, Residual8x8{});
}

RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                 InterpFilter filter) {
    return reconstruct_inter_8x8(ref, mv_x, mv_y, Residual8x8{}, filter);
}

RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                  const Residual8x8& residual,
                                  InterpFilter filter) {
    RefFrame8x8 out;
    inter_predict_block(ref.y.data(), 8, 8, 8, out.y.data(), 8, 8, 8,
                        mv_x, mv_y, filter);
    // 色度沿用与亮度相同的插值滤波器（规范里 interp_filter 对三个平面同时生效）。
    inter_predict_chroma(ref.u.data(), ref.v.data(), 4, 4, 4,
                         out.u.data(), out.v.data(), 4, 4, 4, mv_x, mv_y,
                         filter);
    // 亮度残差叠加在预测之后；色度残差在本流水线里恒为零。
    const auto signal = residual_signal_8x8(residual);
    for (int i = 0; i < 64; ++i)
        out.y[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
            clip_pixel(static_cast<int>(out.y[static_cast<std::size_t>(i)]) +
                       signal[static_cast<std::size_t>(i)]));
    return out;
}

std::string write_tile_group_obmc_8x16(int mv_a_x, int mv_a_y,
                                       int mv_b_x, int mv_b_y,
                                       bool obmc_enabled) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    // 16x16 层（level 3）走右边界折叠 CDF，垂直切成上下两个 8x8 块。
    write_partition_split_at_right_edge(enc, cdf, 3);
    // 语法写的是「MV 分量相对 PredMv 的残差」：解码端 ref_mv += read_mv_component_diff，
    // 而 ref_mv 是 mvstack[0]（无候选时为 0）。块 B 的唯一候选是 A，故 B 必须写
    // mv_b - mv_a，否则 dav1d 解出 mv_a + mv_b，两块运动重合、OBMC 无从体现。
    const auto write_mv = [&](int delta_x, int delta_y) {
        const int joint = (delta_x != 0 ? 1 : 0) | (delta_y != 0 ? 2 : 0);
        write_symbol(enc, cdf.mv.joint, 4, joint);
        if (joint & 2) write_mv_component(enc, cdf, 0, delta_y);
        if (joint & 1) write_mv_component(enc, cdf, 1, delta_x);
    };

    // 块 A：NEWMV，上方与左侧都没有块 ⇒ 没有可重叠邻居，不读 motion_mode。
    write_partition_none(enc, cdf);
    enc.write_bool(0, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][1][0]);
    enc.write_bool(0, cdf.m.ref[2][1][0]);
    enc.write_bool(0, cdf.m.ref[3][1][0]);
    enc.write_bool(0, cdf.m.newmv_mode[0][0]);
    write_mv(mv_a_x, mv_a_y);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);

    // 块 B：上方邻居 A 是 NEWMV 且参考帧相同 ⇒ ref 上下文 2；newmv 上下文
    // 由 nearest_match=1 与 have_newmv=1 得 3-1=2（不是 3）。skip/intra 取
    // 两邻居都为零的上下文。mvstack 只有 A 这一个候选，n_mvs==1，因此 NEWMV
    // 之后没有 DRL 位，直接跟单符号 obmc 布尔（1 ⇒ MM_OBMC）。
    write_partition_none(enc, cdf);
    enc.write_bool(0, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][2][0]);
    enc.write_bool(0, cdf.m.ref[2][2][0]);
    enc.write_bool(0, cdf.m.ref[3][2][0]);
    enc.write_bool(0, cdf.m.newmv_mode[2][0]);
    write_mv(mv_b_x - mv_a_x, mv_b_y - mv_a_y);
    // obmc 表按 dav1d 的块尺寸顺序存放（首项是 BS_8x8），不是本仓 BlockSize
    // 枚举下标（枚举首项是 BS_128x128）。用 [3] 会取到 BS_16x8 的概率，
    // 两端区间状态错位后后续 chroma skip 被解成 0，触发 msac 越读。
    if (obmc_enabled) enc.write_bool(1, cdf.m.obmc[0][0]);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);
    return wrap_tile_data(enc.finish());
}

std::string write_tile_group_warp_8x16(int mv_x, int mv_y) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_split_at_right_edge(enc, cdf, 3);
    const auto write_mv = [&](int dx, int dy) {
        const int joint = (dx != 0 ? 1 : 0) | (dy != 0 ? 2 : 0);
        write_symbol(enc, cdf.mv.joint, 4, joint);
        if (joint & 2) write_mv_component(enc, cdf, 0, dy);
        if (joint & 1) write_mv_component(enc, cdf, 1, dx);
    };
    // 块 A：NEWMV(mv)，无邻居 ⇒ 不读 motion_mode。
    write_partition_none(enc, cdf);
    enc.write_bool(0, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][1][0]);
    enc.write_bool(0, cdf.m.ref[2][1][0]);
    enc.write_bool(0, cdf.m.ref[3][1][0]);
    enc.write_bool(0, cdf.m.newmv_mode[0][0]);
    write_mv(mv_x, mv_y);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);
    // 块 B：上方 A 同参考且为 inter ⇒ 允许 warp；MM_WARP=2 走三符号 motion_mode。
    // 两邻居 MV 相同使推导出的 affine 退化为 IDENTITY，无额外 warp 参数符号。
    write_partition_none(enc, cdf);
    enc.write_bool(0, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][2][0]);
    enc.write_bool(0, cdf.m.ref[2][2][0]);
    enc.write_bool(0, cdf.m.ref[3][2][0]);
    enc.write_bool(0, cdf.m.newmv_mode[2][0]);
    write_mv(0, 0);
    write_symbol(enc, cdf.m.motion_mode[0], 3, 2);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);
    return wrap_tile_data(enc.finish());
}

Reconstructed8x16 reconstruct_warp_identity_8x16(
    const Reconstructed8x16& ref, int mv_x, int mv_y, InterpFilter filter) {
    Reconstructed8x16 out;
    // A 块走普通 MC。
    inter_predict_block(ref.y.data(), 8, 8, 16, out.y.data(), 8, 8, 8,
                        mv_x, mv_y, filter);
    // B 块 motion_mode=MM_WARP：PARAMETERS 由唯一的上方邻居 A 一个采样点拟合。
    // 单采样点的非对角项无约束，gamma/delta 会撞上剪切阈值 ⇒ 解码端判 IDENTITY，
    // 于是 B 的亮度实际走块 MV 的普通 MC（这里必须跟着退化为常规插值）。
    const WarpSample above{0, 0, 2, 2, 1, -1, mv_x, mv_y};
    const auto wm = derive_warp_motion(&above, 1, 2, 2, mv_x, mv_y, 0, 2);
    if (wm.affine) {
        warp_affine_8x8(ref.y.data(), 8, 8, 16, out.y.data() + 64, 8, 0, 8,
                        wm.matrix, wm.alpha, wm.beta, wm.gamma, wm.delta, 1);
    } else {
        inter_predict_block_phase(ref.y.data(), 8, 8, 16, out.y.data() + 64, 8,
                                  8, 8, mv_x >> 3, 8 + (mv_y >> 3),
                                  (mv_x & 7) * 2, (mv_y & 7) * 2, filter);
    }
    // 4x4 色度不满足 warp 的最小尺寸，解码端改用块 MV 的普通 4:2:0 MC。
    inter_predict_chroma(ref.u.data(), ref.v.data(), 4, 4, 8,
                         out.u.data(), out.v.data(), 4, 4, 8, mv_x, mv_y, filter);
    return out;
}

std::string write_tile_group_warp_split_16x16(const WarpSplit16x16Mvs& mvs) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    // 16x16 层（bl=3）的 10 符号表：PARTITION_SPLIT。
    write_symbol(enc, cdf.m.partition[3][0], 10, 3);
    const auto write_mv = [&](int dx, int dy) {
        const int joint = (dx != 0 ? 1 : 0) | (dy != 0 ? 2 : 0);
        write_symbol(enc, cdf.mv.joint, 4, joint);
        if (joint & 2) write_mv_component(enc, cdf, 0, dy);
        if (joint & 1) write_mv_component(enc, cdf, 1, dx);
    };
    const auto write_block_head = [&](int ref_ctx, int newmv_ctx) {
        write_partition_none(enc, cdf);
        enc.write_bool(0, cdf.m.skip[0][0]);
        enc.write_bool(1, cdf.m.intra[0][0]);
        enc.write_bool(0, cdf.m.ref[0][ref_ctx][0]);
        enc.write_bool(0, cdf.m.ref[2][ref_ctx][0]);
        enc.write_bool(0, cdf.m.ref[3][ref_ctx][0]);
        enc.write_bool(0, cdf.m.newmv_mode[newmv_ctx][0]);
    };
    // 帧头 is_motion_mode_switchable=1 时，凡有可匹配邻居的帧间块都要写
    // 3 符号 motion_mode（CDF2 ⇒ 3 个符号，不是 4 个）。块 0 在帧角无邻居，
    // 只有块 1/2/3 需要写；前两块取 TRANSLATION，右下块取 WARP。
    const auto write_motion_mode = [&](int mode) {
        write_symbol(enc, cdf.m.motion_mode[0], 3, mode);
    };

    // 块 0（左上）：无邻居、n_mvs==1，无 DRL 位。
    write_block_head(1, 0);
    write_mv(mvs.mv_x[0], mvs.mv_y[0]);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);

    // 块 1 在顶行：只有左邻居块 0 ⇒ nearest_match=1、newmv 上下文 2，无 DRL 位。
    // 块 2 在左列：顶边块 0 与右上块 1 是两个不同 MV 候选 ⇒ n_mvs==2，需要
    // DRL 位；两者 MV 相同时合并成一个候选，DRL 位消失。取 0 选中块 0 的 MV。
    const bool distinct_topright =
        mvs.mv_x[1] != mvs.mv_x[0] || mvs.mv_y[1] != mvs.mv_y[0];
    for (int k = 1; k <= 2; ++k) {
        write_block_head(2, 2);
        if (k == 2 && distinct_topright) enc.write_bool(0, cdf.m.drl_bit[0][0]);
        write_mv(mvs.mv_x[k] - mvs.mv_x[0], mvs.mv_y[k] - mvs.mv_y[0]);
        write_motion_mode(0);
        write_residual_block(enc, Residual8x8{}, /*inter=*/true);
    }

    // 块 3（右下）：上=块 1、左=块 2、左上角=块 0。三个邻居 MV 全相同时
    // 候选合并 ⇒ 无 DRL 位；否则写一个取 0 的 DRL 位选上邻居块 1 的 MV。
    // nearest_match 同时命中行列 ⇒ newmv 上下文 5-1=4。
    write_block_head(2, 4);
    if (distinct_topright || mvs.mv_x[2] != mvs.mv_x[0] ||
        mvs.mv_y[2] != mvs.mv_y[0]) {
        enc.write_bool(0, cdf.m.drl_bit[0][0]);
    }
    write_mv(mvs.mv_x[3] - mvs.mv_x[1], mvs.mv_y[3] - mvs.mv_y[1]);
    write_motion_mode(2);
    write_residual_block(enc, Residual8x8{}, /*inter=*/true);
    return wrap_tile_data(enc.finish());
}

Frame16x16 reconstruct_warp_split_16x16(const Frame16x16& ref,
                                        const WarpSplit16x16Mvs& mvs,
                                        InterpFilter filter) {
    Frame16x16 out;
    static constexpr int kOx[4] = {0, 8, 0, 8};
    static constexpr int kOy[4] = {0, 0, 8, 8};
    // 前三个子块只有普通 NEWMV：亮度/色度都按各自块 MV 平移插值。
    for (int k = 0; k < 3; ++k) {
        inter_predict_block_phase(ref.y.data(), 16, 16, 16,
                                  out.y.data() + kOy[k] * 16 + kOx[k], 16, 8, 8,
                                  kOx[k] + (mvs.mv_x[k] >> 3),
                                  kOy[k] + (mvs.mv_y[k] >> 3),
                                  (mvs.mv_x[k] & 7) * 2, (mvs.mv_y[k] & 7) * 2,
                                  filter);
        const int cx = mvs.mv_x[k] >> 4, cy = mvs.mv_y[k] >> 4;
        const int ox = kOx[k] / 2, oy = kOy[k] / 2;
        const int px = mvs.mv_x[k] & 15, py = mvs.mv_y[k] & 15;
        inter_predict_block_phase(ref.u.data(), 8, 8, 8, out.u.data() + oy * 8 + ox,
                                  8, 4, 4, ox + cx, oy + cy, px, py, filter, true, true);
        inter_predict_block_phase(ref.v.data(), 8, 8, 8, out.v.data() + oy * 8 + ox,
                                  8, 4, 4, ox + cx, oy + cy, px, py, filter, true, true);
    }

    // 块 3 的 MM_WARP PARAMETERS：三个采样点分别来自上邻居（块 1）、
    // 左邻居（块 2）、左上角（块 0）。dx4/dy4 是沿邻居边界的 4x4 偏移（这里都为 0），
    // 垂直/水平方向由 sx/sy 与邻居尺寸给出。
    const WarpSample samples[3] = {
        {0, 0, 2, 2, 1, -1, mvs.mv_x[1], mvs.mv_y[1]},   // 上
        {0, 0, 2, 2, -1, 1, mvs.mv_x[2], mvs.mv_y[2]},   // 左
        {0, 0, 2, 2, -1, -1, mvs.mv_x[0], mvs.mv_y[0]},  // 左上角
    };
    const auto wm = derive_warp_motion(samples, 3, 2, 2, mvs.mv_x[3], mvs.mv_y[3],
                                       2, 2);
    if (wm.affine) {
        warp_affine_8x8(ref.y.data(), 16, 16, 16, out.y.data() + 8 * 16 + 8, 16,
                        8, 8, wm.matrix, wm.alpha, wm.beta, wm.gamma, wm.delta, 1);
    } else {
        inter_predict_block_phase(ref.y.data(), 16, 16, 16,
                                  out.y.data() + 8 * 16 + 8, 16, 8, 8,
                                  8 + (mvs.mv_x[3] >> 3), 8 + (mvs.mv_y[3] >> 3),
                                  (mvs.mv_x[3] & 7) * 2, (mvs.mv_y[3] & 7) * 2, filter);
    }
    // 4x4 色度不满足 warp 最小尺寸，退化为块 MV 的常规 4:2:0 MC。
    {
        const int cx = mvs.mv_x[3] >> 4, cy = mvs.mv_y[3] >> 4;
        const int px = mvs.mv_x[3] & 15, py = mvs.mv_y[3] & 15;
        inter_predict_block_phase(ref.u.data(), 8, 8, 8, out.u.data() + 4 * 8 + 4,
                                  8, 4, 4, 4 + cx, 4 + cy, px, py, filter, true, true);
        inter_predict_block_phase(ref.v.data(), 8, 8, 8, out.v.data() + 4 * 8 + 4,
                                  8, 4, 4, 4 + cx, 4 + cy, px, py, filter, true, true);
    }
    return out;
}

Reconstructed8x16 reconstruct_obmc_8x16(const Reconstructed8x16& ref,
                                        int mv_a_x, int mv_a_y,
                                        int mv_b_x, int mv_b_y,
                                        InterpFilter filter) {
    Reconstructed8x16 pa, pb, out;
    // 两路整幅预测：A 区取 mv_a，B 区取 mv_b；OBMC 的邻居分量也是「按邻居 MV
    // 对当前块重新求预测」，所以直接用整幅 mv_a 预测里 B 区的那块来混合。
    inter_predict_block(ref.y.data(), 8, 8, 16, pa.y.data(), 8, 8, 16,
                        mv_a_x, mv_a_y, filter);
    inter_predict_block(ref.y.data(), 8, 8, 16, pb.y.data(), 8, 8, 16,
                        mv_b_x, mv_b_y, filter);
    inter_predict_chroma(ref.u.data(), ref.v.data(), 4, 4, 8,
                         pa.u.data(), pa.v.data(), 4, 4, 8, mv_a_x, mv_a_y, filter);
    inter_predict_chroma(ref.u.data(), ref.v.data(), 4, 4, 8,
                         pb.u.data(), pb.v.data(), 4, 4, 8, mv_b_x, mv_b_y, filter);

    // 邻居分量是解码端对 B 块那 4 行区域单独跑的一次 mc：高 <= 4 时 dav1d
    // 的 GET_V_FILTER 会改用窄块（3+FILTER）系数表，与整帧 8x8 预测差 ±1。
    std::array<std::uint8_t, 32> pa_b_y{};
    std::array<std::uint8_t, 16> pa_b_u{}, pa_b_v{};
    inter_predict_block_phase(ref.y.data(), 8, 8, 16, pa_b_y.data(), 8, 8, 4,
                              mv_a_x >> 3, 8 + (mv_a_y >> 3),
                              (mv_a_x & 7) * 2, (mv_a_y & 7) * 2,
                              filter, false, true);
    inter_predict_block_phase(ref.u.data(), 4, 4, 8, pa_b_u.data(), 4, 4, 4,
                              mv_a_x >> 4, 4 + (mv_a_y >> 4),
                              mv_a_x & 15, mv_a_y & 15, filter);
    inter_predict_block_phase(ref.v.data(), 4, 4, 8, pa_b_v.data(), 4, 4, 4,
                              mv_a_x >> 4, 4 + (mv_a_y >> 4),
                              mv_a_x & 15, mv_a_y & 15, filter);

    std::copy(pa.y.begin(), pa.y.begin() + 64, out.y.begin());
    std::copy(pb.y.begin() + 64, pb.y.end(), out.y.begin() + 64);
    // 重叠区是块高的一半：亮度 8/2 = 4 行 ⇒ 混前 3 行，掩码取 4 行段。
    inter_obmc_blend_top(out.y.data() + 64, 8, pa_b_y.data(), 8, 8, 4);

    // 色度块像素高 4 ⇒ 重叠区 2 行 ⇒ 混前 (2*3)>>2 = 1 行，掩码取 2 行段。
    std::copy(pa.u.begin(), pa.u.begin() + 16, out.u.begin());
    std::copy(pb.u.begin() + 16, pb.u.end(), out.u.begin() + 16);
    inter_obmc_blend_top(out.u.data() + 16, 4, pa_b_u.data(), 4, 4, 2);
    std::copy(pa.v.begin(), pa.v.begin() + 16, out.v.begin());
    std::copy(pb.v.begin() + 16, pb.v.end(), out.v.begin() + 16);
    inter_obmc_blend_top(out.v.data() + 16, 4, pa_b_v.data(), 4, 4, 2);
    return out;
}

RefFrame8x8 reconstruct_gm_warp_8x8(const RefFrame8x8& ref, int type,
                                    int alpha, int beta, int gamma, int delta,
                                    int trans_x, int trans_y) {
    std::int32_t matrix[6] = {
        trans_y << 10, trans_x << 10,
        (alpha << 1) + 65536, beta << 1,
        gamma << 1, (delta << 1) + 65536,
    };
    (void)type;  // ROTZOOM 时 gamma=delta=0，矩阵形式相同。
    RefFrame8x8 out;
    // 帧头 subexp 参数是 15 位定点，warp 内部 shear 参数是 16 位 ⇒ 乘 2。
    const int a = alpha * 2, b = beta * 2, g = gamma * 2, d = delta * 2;
    warp_affine_8x8(ref.y.data(), 8, 8, 8, out.y.data(), 8, 0, 0, matrix,
                    a, b, g, d, 1);
    // 8x8 块的 4x4 色度不满足 imin(cbw4,cbh4)>1：解码端对色度改用
    // GLOBALMV 在块中心导出的整数 MV 走普通 4:2:0 MC，而不是 warp。
    const int cx = 4, cy = 4;
    const int mv_y = (a * cy + g * cx + (trans_y << 10)) >> 13;
    const int mv_x = (b * cy + d * cx + (trans_x << 10)) >> 13;
    inter_predict_chroma(ref.u.data(), ref.v.data(), 4, 4, 4,
                         out.u.data(), out.v.data(), 4, 4, 4,
                         mv_x, mv_y, InterpFilter::Regular);
    return out;
}

std::string write_tile_group_interintra_8x8(int mv_x, int mv_y,
                                             const InterIntra8x8Config& cfg,
                                             InterpFilter filter) {
    (void)filter;  // 帧头统一给出滤波器，块内不再读 interp_filter。
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_none(enc, cdf);
    enc.write_bool(1, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    // switchable_comp_refs=0 ⇒ 不读 is_compound；ref[0]=LAST，GLOBALMV。
    enc.write_bool(0, cdf.m.ref[0][1][0]);
    enc.write_bool(0, cdf.m.ref[2][1][0]);
    enc.write_bool(0, cdf.m.ref[3][1][0]);
    enc.write_bool(1, cdf.m.newmv_mode[0][0]);
    enc.write_bool(0, cdf.m.globalmv_mode[0][0]);
    // 8x8 的 size_group=1，wedge_ctx_lut=0。
    enc.write_bool(1, cdf.m.interintra[1][0]);
    write_symbol(enc, cdf.m.interintra_mode[1], 4, cfg.mode & 3);
    enc.write_bool(cfg.wedge ? 1 : 0, cdf.m.interintra_wedge[0][0]);
    if (cfg.wedge) write_symbol(enc, cdf.m.wedge_idx[0], 16, cfg.wedge_idx);
    return wrap_tile_data(enc.finish());
}

RefFrame8x8 reconstruct_interintra_8x8(const RefFrame8x8& ref, int mv_x, int mv_y,
                                       const InterIntra8x8Config& cfg,
                                       InterpFilter filter) {
    const RefFrame8x8 inter = reconstruct_inter_8x8(ref, mv_x, mv_y, filter);
    // 8x8 块位于帧左上角：prepare_intra_edges 的不可用上边界取 127、
    // 左边界取 129、左上角取 128。SMOOTH 用 dav1d_sm_weights 在两者间插值。
    static constexpr int kIntraValue[4] = {128, 127, 129, 0};
    static constexpr std::uint8_t kSmoothWeights[16] = {
        0, 0, 255, 128, 255, 149, 85, 64,
        255, 197, 146, 105, 73, 50, 37, 32,
    };
    auto intra_at = [&](int plane, int x, int y) {
        const int mode = cfg.mode & 3;
        if (mode != 3) return kIntraValue[mode];
        const int base = plane == 0 ? 8 : 4;
        const int wv = kSmoothWeights[base + y], wh = kSmoothWeights[base + x];
        const int pred = wv * 127 + (256 - wv) * 129 +
                         wh * 129 + (256 - wh) * 127;
        return (pred + 256) >> 9;
    };
    static constexpr int kIiiWeights[32] = {
        60, 52, 45, 39, 34, 30, 26, 22, 19, 17, 15, 13, 11, 10, 8, 7,
        6, 6, 5, 4, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    };
    auto mask_at = [&](int plane, int x, int y) {
        if (cfg.wedge) {
            if (plane == 0) return kAv1WedgeMask8x8[cfg.wedge_idx & 15][0][y * 8 + x];
            const int off = (2 * y) * 8 + 2 * x;
            const auto& m = kAv1WedgeMask8x8[cfg.wedge_idx & 15][0];
            return static_cast<std::uint8_t>((m[off] + m[off + 1] +
                                              m[off + 8] + m[off + 9] + 2) >> 2);
        }
        if (cfg.mode == 0) return static_cast<std::uint8_t>(32);
        const int step = plane == 0 ? 4 : 8;
        const int p = cfg.mode - 1;
        if (p == 0) return static_cast<std::uint8_t>(kIiiWeights[y * step]);
        if (p == 1) return static_cast<std::uint8_t>(kIiiWeights[x * step]);
        return static_cast<std::uint8_t>(kIiiWeights[std::min(x, y) * step]);
    };
    const auto blend = [&](const std::uint8_t* src, std::uint8_t* dst, int plane, int w, int h) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int m = mask_at(plane, x, y);
                const int intra = intra_at(plane, x, y);
                dst[y * w + x] = static_cast<std::uint8_t>(
                    (src[y * w + x] * (64 - m) + intra * m + 32) >> 6);
            }
    };
    RefFrame8x8 out;
    blend(inter.y.data(), out.y.data(), 0, 8, 8);
    blend(inter.u.data(), out.u.data(), 1, 4, 4);
    blend(inter.v.data(), out.v.data(), 1, 4, 4);
    return out;
}

std::string write_tile_group_compound_8x8(const Compound8x8Config& cfg) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_none(enc, cdf);
    // 复合块暂不带残差：skip=1 时解码端读完模式语法就直接进入重建。
    enc.write_bool(1, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    // 单块帧的上/左邻居都不在帧内：get_comp_ctx / av1_get_ref_ctx /
    // av1_get_fwd_ref_1_ctx 取对称上下文 1，get_comp_dir_ctx 取 2。
    enc.write_bool(1, cdf.m.comp[1][0]);            // is_compound
    enc.write_bool(0, cdf.m.comp_dir[2][0]);        // 0 ⇒ 单向复合
    enc.write_bool(0, cdf.m.comp_uni_ref[0][1][0]); // ref[0] = LAST_FRAME
    enc.write_bool(0, cdf.m.comp_uni_ref[1][1][0]); // ref[1] = GOLDEN_FRAME
    // GLOBALMV_GLOBALMV（索引 6）：两路 MV 都由帧头 GMV 矩阵给出，不写 MVD。
    write_symbol(enc, cdf.m.comp_inter_mode[0], N_COMP_INTER_PRED_MODES, 6);
    // 序列头 enable_masked_compound=1 ⇒ 先读 comp_group_idx：0 进入
    // joint/average 组，1 进入 wedge/diffwtd 掩码组。
    const bool masked_group = cfg.mode == CompoundMode8x8::Wedge ||
                              cfg.mode == CompoundMode8x8::DiffWtd;
    enc.write_bool(masked_group ? 1 : 0, cdf.m.mask_comp[0][0]);
    if (masked_group) {
        // MASKED_COMPOUND_TYPES 的两个符号：0 = WEDGE，1 = DIFFWTD。
        // 8x8 的 wedge_ctx_lut 是 0（不是按 32x32 起算的尺寸序号）。
        enc.write_bool(cfg.mode == CompoundMode8x8::Wedge ? 0 : 1,
                       cdf.m.wedge_comp[0][0]);
        if (cfg.mode == CompoundMode8x8::Wedge) {
            // wedge_idx（16 个）与 wedge_sign（原始位）。
            write_symbol(enc, cdf.m.wedge_idx[0], 16, cfg.wedge_idx);
            enc.write_bool(cfg.mask_sign != 0 ? 1 : 0, kEquiprobable);
        } else {
            // DIFFWTD：只有一个原始 mask_type 位（MAX_DIFFWTD_MASK_BITS）。
            enc.write_bool(cfg.mask_sign != 0 ? 1 : 0, kEquiprobable);
        }
    } else if (cfg.jnt_comp) {
        // dav1d: comp_type = COMP_INTER_WEIGHTED_AVG + bit，枚举里
        // WEIGHTED_AVG=1、AVG=2，故 bit=0 才是距离加权，bit=1 是普通 AVG。
        // d0==d1 且无上/左邻居 ⇒ jnt_ctx=3。
        enc.write_bool(cfg.mode == CompoundMode8x8::DistWtd ? 0 : 1,
                       cdf.m.jnt_comp[3][0]);
    }
    return wrap_tile_data(enc.finish());
}

RefFrame8x8 reconstruct_compound_8x8(const RefFrame8x8& last, int mv_last_x, int mv_last_y,
                                     const RefFrame8x8& golden, int mv_golden_x, int mv_golden_y,
                                     const Compound8x8Config& cfg,
                                     InterpFilter filter) {
    // 两路预测各自独立做亚像素 MC，停在 4 位小数的中间精度：
    // 解码器的复合混合就发生在这一层，先降到 8 位再平均会在 ±1 码值上偏差。
    std::int16_t mid0[64], mid1[64], cu0[16], cv0[16], cu1[16], cv1[16];
    inter_predict_block_mid(last.y.data(), 8, 8, 8, mid0, 8, 8, 8,
                            mv_last_x >> 3, mv_last_y >> 3, (mv_last_x & 7) * 2,
                            (mv_last_y & 7) * 2, filter);
    inter_predict_block_mid(golden.y.data(), 8, 8, 8, mid1, 8, 8, 8,
                            mv_golden_x >> 3, mv_golden_y >> 3, (mv_golden_x & 7) * 2,
                            (mv_golden_y & 7) * 2, filter);
    // 4:2:0 色度：整数偏移 mv>>4，1/16 相位 mv&15。
    inter_predict_block_mid(last.u.data(), 4, 4, 4, cu0, 4, 4, 4, mv_last_x >> 4,
                            mv_last_y >> 4, mv_last_x & 15, mv_last_y & 15, filter);
    inter_predict_block_mid(last.v.data(), 4, 4, 4, cv0, 4, 4, 4, mv_last_x >> 4,
                            mv_last_y >> 4, mv_last_x & 15, mv_last_y & 15, filter);
    inter_predict_block_mid(golden.u.data(), 4, 4, 4, cu1, 4, 4, 4, mv_golden_x >> 4,
                            mv_golden_y >> 4, mv_golden_x & 15, mv_golden_y & 15, filter);
    inter_predict_block_mid(golden.v.data(), 4, 4, 4, cv1, 4, 4, 4, mv_golden_x >> 4,
                            mv_golden_y >> 4, mv_golden_x & 15, mv_golden_y & 15, filter);

    RefFrame8x8 out;
    if (cfg.mode == CompoundMode8x8::DistWtd) {
        // dav1d w_avg_c 在 4 位小数中间量上做：(a*w + b*(16-w) + 128) >> 8。
        // d0==d1 时 jnt_weights = lookup[0][1] = 7（ref0 权重 7，ref1 权重 9）。
        const auto wavg = [](const std::int16_t* a, const std::int16_t* b,
                             std::uint8_t* dst, int n) {
            for (int i = 0; i < n; ++i)
                dst[i] = static_cast<std::uint8_t>(
                    clip_pixel((a[i] * 7 + b[i] * 9 + 128) >> 8));
        };
        wavg(mid0, mid1, out.y.data(), 64);
        wavg(cu0, cu1, out.u.data(), 16);
        wavg(cv0, cv1, out.v.data(), 16);
    } else if (cfg.mode == CompoundMode8x8::Wedge) {
        const auto& mask = kAv1WedgeMask8x8[cfg.wedge_idx & 15][cfg.mask_sign & 1];
        // 解码器在中间精度上做掩码混合：(m0*m + m1*(64-m) + 512) >> 10。
        const auto blend = [](const std::int16_t* a, const std::int16_t* b,
                              const std::uint8_t* m, std::uint8_t* dst, int n) {
            for (int i = 0; i < n; ++i)
                dst[i] = static_cast<std::uint8_t>(
                    clip_pixel((a[i] * m[i] + b[i] * (64 - m[i]) + 512) >> 10));
        };
        blend(mid0, mid1, mask, out.y.data(), 64);
        // 4:2:0 色度掩码由亮度 2x2 邻域取得（(sum+2)>>2）。
        std::uint8_t cmask[16];
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                const int off = (2 * y) * 8 + 2 * x;
                cmask[y * 4 + x] = static_cast<std::uint8_t>(
                    (mask[off] + mask[off + 1] + mask[off + 8] + mask[off + 9] + 2) >> 2);
            }
        }
        blend(cu0, cu1, cmask, out.u.data(), 16);
        blend(cv0, cv1, cmask, out.v.data(), 16);
    } else if (cfg.mode == CompoundMode8x8::DiffWtd) {
        // dav1d w_mask_c：权重由两路中间预测的差值给出，m = min(38 + (|d|+8)/256, 64)。
        const int sign = cfg.mask_sign & 1;
        const std::int16_t* a = sign ? mid1 : mid0;
        const std::int16_t* b = sign ? mid0 : mid1;
        const std::int16_t* au = sign ? cu1 : cu0;
        const std::int16_t* bu = sign ? cu0 : cu1;
        const std::int16_t* av = sign ? cv1 : cv0;
        const std::int16_t* bv = sign ? cv0 : cv1;
        std::uint8_t mask[64], cmask[16] = {};
        for (int i = 0; i < 64; ++i) {
            const int d = a[i] - b[i];
            const int m = std::min(38 + ((std::abs(d) + 8) >> 8), 64);
            mask[i] = static_cast<std::uint8_t>(m);
            out.y[i] = static_cast<std::uint8_t>(clip_pixel((d * m + b[i] * 64 + 512) >> 10));
        }
        // 4:2:0 色度掩码：亮度 2x2 求和，偶数行暂存、奇数行取 4 项平均。
        for (int y = 0; y < 8; y += 2) {
            for (int x = 0; x < 4; ++x) {
                const int s0 = mask[y * 8 + 2 * x] + mask[y * 8 + 2 * x + 1];
                const int s1 = mask[(y + 1) * 8 + 2 * x] +
                               mask[(y + 1) * 8 + 2 * x + 1];
                cmask[y * 2 + x] = static_cast<std::uint8_t>((s0 + s1 + 2 - sign) >> 2);
            }
        }
        const auto blend_c = [&](const std::int16_t* p0, const std::int16_t* p1,
                                 std::uint8_t* dst) {
            for (int i = 0; i < 16; ++i) {
                const int m = cmask[i];
                dst[i] = static_cast<std::uint8_t>(
                    clip_pixel((p0[i] * m + p1[i] * (64 - m) + 512) >> 10));
            }
        };
        blend_c(au, bu, out.u.data());
        blend_c(av, bv, out.v.data());
    } else {
        inter_compound_average_mid(mid0, mid1, out.y.data(), 64);
        inter_compound_average_mid(cu0, cu1, out.u.data(), 16);
        inter_compound_average_mid(cv0, cv1, out.v.data(), 16);
    }
    return out;
}

// 16x16 帧单块的划分层：8x8 探针用的是 bl=4 的 4 符号表；16x16 块在 dav1d 里
// 是 bl=3，该层 partition_cdf 有 10 个符号（NONE/HORZ/VERT/SPLIT + 4 个 A·B
// + 2 个 4 分）。帧左上角没有上/左邻居，al_part_ctx 折叠出 ctx=0。
std::string write_tile_group_intra_16x16(const BlockMode8x8& mode) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_symbol(enc, cdf.m.partition[3][0], 10, 0 /* PARTITION_NONE */);
    // 单块 skip=1：本路径不承载变换层符号，块重建就是纯帧内预测。
    write_block_mode(enc, cdf, mode, 0, 0, 0, 1);
    return wrap_tile_data(enc.finish());
}

Frame16x16 reconstruct_intra_16x16(const BlockMode8x8& mode) {
    Frame16x16 out{};
    // 边界样本全不可用：解码端按 base=128 回填，predict_intra 内部同构处理。
    IntraSamples samples;
    IntraPredictParams luma;
    luma.mode = mode.y_mode;
    luma.angle_delta = mode.angle_delta;
    luma.filter_intra_mode = mode.filter_intra;
    luma.width = 16;
    luma.height = 16;
    luma.plane = 0;
    predict_intra(samples, luma, out.y.data(), 16);

    // 4:2:0 下 16x16 亮度对应的色度块是 8x8，走色度档边界滤波阈值。
    IntraPredictParams chroma;
    chroma.mode = mode.uv_mode;
    chroma.width = 8;
    chroma.height = 8;
    chroma.plane = 1;
    predict_intra(samples, chroma, out.u.data(), 8);
    predict_intra(samples, chroma, out.v.data(), 8);
    return out;
}

std::string write_tile_group_inter_16x16(int mv_x, int mv_y) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_symbol(enc, cdf.m.partition[3][0], 10, 0 /* PARTITION_NONE */);
    // skip 只跳过变换层符号，MV 语法照读；帧首块无邻居，skip ctx 与 intra ctx
    // 都是 0，av1_get_ref_ctx 折叠出 1。
    enc.write_bool(1, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][1][0]);
    enc.write_bool(0, cdf.m.ref[2][1][0]);
    enc.write_bool(0, cdf.m.ref[3][1][0]);
    // newmv_mode=0 进 NEWMV；单块帧的 mvstack 只有全局零候选，n_mvs==1 ⇒ 无 DRL 位。
    const int joint = (mv_x != 0 ? 1 : 0) | (mv_y != 0 ? 2 : 0);
    enc.write_bool(0, cdf.m.newmv_mode[0][0]);
    write_symbol(enc, cdf.mv.joint, 4, joint);
    if (joint & 2) write_mv_component(enc, cdf, 0, mv_y);
    if (joint & 1) write_mv_component(enc, cdf, 1, mv_x);
    return wrap_tile_data(enc.finish());
}

Frame16x16 reconstruct_inter_16x16(const Frame16x16& ref, int mv_x, int mv_y,
                                   InterpFilter filter) {
    Frame16x16 out{};
    inter_predict_block(ref.y.data(), 16, 16, 16, out.y.data(), 16, 16, 16,
                        mv_x, mv_y, filter);
    inter_predict_chroma(ref.u.data(), ref.v.data(), 8, 8, 8,
                         out.u.data(), out.v.data(), 8, 8, 8, mv_x, mv_y, filter);
    return out;
}

// SMOOTH 族（9/10/11）邻居会把帧内边界滤波类型抬到 1（色度档）。
bool is_smooth_family_mode(int mode) {
    return mode == SMOOTH_PRED || mode == SMOOTH_V_PRED ||
           mode == SMOOTH_H_PRED;
}

std::string write_tile_group_intra_16x16_split(const Blocks16x16& blocks) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    // 16x16 层（dav1d bl=3）的 10 符号表：PARTITION_SPLIT 拆成 2x2 个 8x8 子块。
    write_symbol(enc, cdf.m.partition[3][0], 10, 3 /* PARTITION_SPLIT */);
    for (int k = 0; k < 4; ++k) {
        // 8x8 层（bl=4，4 符号表）。四块的邻居都是 8x8 的 PARTITION_NONE，
        // al_part_ctx 折出 0，因此与单块路径共用 ctx 0 的写法。
        write_partition_none(enc, cdf);
        // 上/左邻居：k>=2 的上方是 k-2；奇数 k 的左侧是 k-1。不存在时按
        // DC_PRED 折叠（ctx 0）。
        const int above_ctx =
            k >= 2 ? kIntraModeContext[blocks.modes[static_cast<std::size_t>(k - 2)].y_mode]
                   : 0;
        const int left_ctx =
            (k % 2) ? kIntraModeContext[blocks.modes[static_cast<std::size_t>(k - 1)].y_mode]
                    : 0;
        // skip ctx = 上邻居的 skip + 左邻居的 skip（dav1d 直接读邻居数组，
        // 不存在的一侧按 0 参与）。
        const int skip_ctx =
            (k >= 2 ? blocks.skip[static_cast<std::size_t>(k - 2)] : 0) +
            ((k % 2) ? blocks.skip[static_cast<std::size_t>(k - 1)] : 0);
        const int skip = blocks.skip[static_cast<std::size_t>(k)];
        write_block_mode(enc, cdf, blocks.modes[static_cast<std::size_t>(k)],
                         above_ctx, left_ctx, skip_ctx, skip);
        // skip=1 时整块没有变换层符号；带残差只会出现在首块（无邻居，txb_skip
        // 上下文恒为 0），邻居块走 skip=1 以免踩到未接线的 txb 上下文表。
        if (!skip)
            write_residual_block(enc, blocks.residuals[static_cast<std::size_t>(k)],
                                 /*inter=*/false);
    }
    return wrap_tile_data(enc.finish());
}

Frame16x16 reconstruct_intra_16x16_split(const Blocks16x16& blocks) {
    Frame16x16 out{};
    for (int k = 0; k < 4; ++k) {
        const int bx = (k % 2) ? 8 : 0;
        const int by = (k >= 2) ? 8 : 0;
        IntraSamples s;
        if (by > 0) {
            s.above = out.y.data() + (by - 1) * 16 + bx;
            s.num_above = 8;
            // above-right = (bx+8..bx+15, by-1)：bx=0 落在帧内，bx=8 越界。
            s.num_above_right = (bx + 8 < 16) ? 8 : -1;
        }
        if (bx > 0) {
            s.left = out.y.data() + by * 16 + (bx - 1);
            s.left_stride = 16;
            s.num_left = 8;
            // below-left 属于解码顺序更晚的左下块，当前块解码时尚未重建。
            s.num_below_left = -1;
        }
        if (by > 0 && bx > 0)
            s.corner = out.y[static_cast<std::size_t>((by - 1) * 16 + bx - 1)];

        const BlockMode8x8& m = blocks.modes[static_cast<std::size_t>(k)];
        IntraPredictParams p;
        p.mode = m.y_mode;
        p.angle_delta = m.angle_delta;
        p.filter_intra_mode = m.filter_intra;
        p.width = 8;
        p.height = 8;
        p.plane = 0;
        p.neighbor_smooth =
            (by > 0 && is_smooth_family_mode(
                           blocks.modes[static_cast<std::size_t>(k - 2)].y_mode)) ||
            (bx > 0 && is_smooth_family_mode(
                           blocks.modes[static_cast<std::size_t>(k - 1)].y_mode));
        predict_intra(s, p, out.y.data() + by * 16 + bx, 16);

        if (!blocks.skip[static_cast<std::size_t>(k)]) {
            const auto signal = residual_signal_8x8(
                blocks.residuals[static_cast<std::size_t>(k)]);
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    const std::size_t i =
                        static_cast<std::size_t>((by + r) * 16 + bx + c);
                    out.y[i] = static_cast<std::uint8_t>(clip_pixel(
                        static_cast<int>(out.y[i]) +
                        signal[static_cast<std::size_t>(r * 8 + c)]));
                }
            }
        }
    }

    // 色度：4:2:0 下 16x16 帧是 8x8 平面，四个 4x4 象限各用自己子块的 uv_mode。
    for (int k = 0; k < 4; ++k) {
        const int cx = (k % 2) ? 4 : 0;
        const int cy = (k >= 2) ? 4 : 0;
        for (auto* plane : {&out.u, &out.v}) {
            IntraSamples s;
            if (cy > 0) {
                s.above = plane->data() + (cy - 1) * 8 + cx;
                s.num_above = 4;
                s.num_above_right = (cx + 4 < 8) ? 4 : -1;
            }
            if (cx > 0) {
                s.left = plane->data() + cy * 8 + (cx - 1);
                s.left_stride = 8;
                s.num_left = 4;
                s.num_below_left = -1;
            }
            if (cy > 0 && cx > 0)
                s.corner = (*plane)[static_cast<std::size_t>((cy - 1) * 8 + cx - 1)];
            IntraPredictParams p;
            // CFL 不在本路径（需要 4x4 亮度子块的 q3 均值），uv_mode 限定 0..12。
            p.mode = blocks.modes[static_cast<std::size_t>(k)].uv_mode;
            p.width = 4;
            p.height = 4;
            p.plane = 1;
            predict_intra(s, p, plane->data() + cy * 8 + cx, 8);
        }
    }
    return out;
}

std::array<std::int32_t, 64> av1_intra_residual_signal_8x8(
    const Residual8x8& residual, int base_q_idx) {
    return residual_signal_8x8(residual, quant_steps_from_qindex(base_q_idx));
}

int av1_intra_mode_context(int mode) {
    if (mode < 0 || mode > 12) return 0;
    return kIntraModeContext[mode];
}

std::size_t av1_measure_block_bytes(const BlockMode8x8& mode, int above_ctx,
                                    int left_ctx, int skip_ctx, int skip,
                                    const Residual8x8& residual) {
    RangeEncoder enc;
    write_block_mode(enc, kDefaultCdf, mode, above_ctx, left_ctx, skip_ctx, skip);
    if (!skip) write_residual_block(enc, residual, /*inter=*/false);
    return enc.finish().size();
}

namespace {

// 按 4x4 单位记录上/左邻居的模式与 skip，供 kfym / skip CDF 选上下文。
struct FrameModeContexts {
    std::vector<std::uint8_t> above_mode;
    std::vector<std::uint8_t> left_mode;
    std::vector<std::uint8_t> above_skip;
    std::vector<std::uint8_t> left_skip;

    void reset(int mi_rows, int mi_cols) {
        const std::size_t n = static_cast<std::size_t>(
            (mi_rows > mi_cols ? mi_rows : mi_cols) + 1);
        above_mode.assign(n, 0);
        left_mode.assign(n, 0);
        above_skip.assign(n, 0);
        left_skip.assign(n, 0);
    }
};

}  // namespace

std::string write_tile_group_intra_frame(const IntraFramePlan& plan) {
    if (plan.width < 8 || plan.height < 8 || (plan.width & 7) || (plan.height & 7))
        return {};
    const int mi_rows = (plan.height + 3) >> 2;
    const int mi_cols = (plan.width + 3) >> 2;
    const int units_x = (plan.width + 15) >> 4;
    const int units_y = (plan.height + 15) >> 4;
    const std::size_t unit_count =
        static_cast<std::size_t>(units_x) * static_cast<std::size_t>(units_y);
    if (plan.split16.size() != unit_count || plan.mode16.size() != unit_count ||
        plan.blocks16.size() != unit_count)
        return {};

    const CdfDefaultContext& cdf = kDefaultCdf;
    PartitionContext pctx;
    pctx.frame_mi_rows = mi_rows;
    pctx.frame_mi_cols = mi_cols;
    pctx.reset();
    FrameModeContexts modes;
    modes.reset(mi_rows, mi_cols);
    // 解码端 lcoef 数组以 0x40 初始化；每个带残差的 8x8 块按 2x2 个 4x4 回写。
    std::vector<std::uint8_t> res_ctx(
        static_cast<std::size_t>(mi_rows) * mi_cols, 0x40);

    RangeEncoder enc;

    const auto leaf = [&](const PartitionNode& node) {
        const int px = node.col * 4;
        const int py = node.row * 4;
        const int w = 1 << node.w_log2;
        const int h = 1 << node.h_log2;
        const int above_ctx = kIntraModeContext[modes.above_mode[static_cast<std::size_t>(node.col)]];
        const int left_ctx = kIntraModeContext[modes.left_mode[static_cast<std::size_t>(node.row)]];
        const int skip_ctx = modes.above_skip[static_cast<std::size_t>(node.col)] +
                             modes.left_skip[static_cast<std::size_t>(node.row)];
        const std::size_t unit = static_cast<std::size_t>(py >> 4) *
                                     static_cast<std::size_t>(units_x) +
                                 static_cast<std::size_t>(px >> 4);

        BlockMode8x8 mode;
        int skip = 1;
        if (w <= 8 && h <= 8) {
            const int sub = ((py & 15) >= 8 ? 2 : 0) + ((px & 15) >= 8 ? 1 : 0);
            const FrameBlock8x8& blk = plan.blocks16[unit][static_cast<std::size_t>(sub)];
            write_block_mode(enc, cdf, blk.mode, above_ctx, left_ctx, skip_ctx,
                             blk.skip);
            if (!blk.skip)
                write_residual_block(enc, blk.residual, /*inter=*/false,
                                     ResidualCtxView{res_ctx.data(), mi_rows, mi_cols},
                                     node.col, node.row);
            mode = blk.mode;
            skip = blk.skip;
        } else {
            mode = plan.mode16[unit];
            write_block_mode(enc, cdf, mode, above_ctx, left_ctx, skip_ctx, 1);
        }

        const int wmi = w >> 2;
        const int hmi = h >> 2;
        for (int i = 0; i < wmi && node.col + i < mi_cols; ++i) {
            modes.above_mode[static_cast<std::size_t>(node.col + i)] =
                static_cast<std::uint8_t>(mode.y_mode);
            modes.above_skip[static_cast<std::size_t>(node.col + i)] =
                static_cast<std::uint8_t>(skip);
        }
        for (int i = 0; i < hmi && node.row + i < mi_rows; ++i) {
            modes.left_mode[static_cast<std::size_t>(node.row + i)] =
                static_cast<std::uint8_t>(mode.y_mode);
            modes.left_skip[static_cast<std::size_t>(node.row + i)] =
                static_cast<std::uint8_t>(skip);
        }
    };

    const PartitionChooser chooser =
        [&](int row, int col, int w_log2, int /*h_log2*/, bool /*has_rows*/,
            bool /*has_cols*/) {
            if (w_log2 >= 5) return PartitionType::Split;
            const std::size_t unit =
                static_cast<std::size_t>((row * 4) >> 4) *
                    static_cast<std::size_t>(units_x) +
                static_cast<std::size_t>((col * 4) >> 4);
            return plan.split16[unit] ? PartitionType::Split
                                      : PartitionType::None;
        };

    for (int y = 0; y < plan.height; y += 64)
        for (int x = 0; x < plan.width; x += 64)
            write_partition_tree(enc, cdf.m.partition, pctx, y >> 2, x >> 2, 6,
                                 chooser, leaf);

    return enc.finish();
}

}  // namespace termchart::image::codec::av1
