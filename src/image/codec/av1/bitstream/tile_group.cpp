#include "tile_group.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../bitstream/bit_writer.hpp"
#include "../entropy/cdf_manager.hpp"
#include "../entropy/coef_cdf_data.hpp"
#include "../entropy/default_cdf.hpp"
#include "../entropy/range_coder.hpp"
#include "../predict/inter_predict.hpp"
#include "../predict/intra_predict.hpp"

namespace termchart::image::codec::av1 {
namespace {

constexpr int kEquiprobable = 16384;

int clip_pixel(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

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
        // TX_CLASS_2D：DC（coeff_idx 0）的 base 上下文恒为 0（libaom
        // get_nz_map_ctx_from_stats / get_lower_levels_ctx 都直接返回 0），
        // 与 AC 邻居无关。
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

void write_residual(RangeEncoder& enc, const Residual8x8& residual) {
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

    // reduced_tx_set=1 → intra 8x8 属于 EXT_TX_SET_DTT4_IDTX（eset=2），
    // av1_num_ext_tx_set 为 5 符号；DCT_DCT 的符号索引 av1_ext_tx_ind[2][0] = 1。
    write_symbol(enc, kTxTypeIntra8x8Smooth, 5, 1);

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
                       i == 0 ? kDcSignLuma[0] : kEquiprobable);
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

void write_zero_residual(RangeEncoder& enc, const Residual8x8& residual) {
    bool all_zero = true;
    for (auto v : residual.coeff) all_zero = all_zero && (v == 0);
    enc.write_bool(all_zero ? 1 : 0, kTxBypassLuma8x8[0]);
    if (!all_zero) write_residual(enc, residual);
    // U/V 各一个 4x4 变换块，恒为全零。
    enc.write_bool(1, kTxBypassChroma4x4[0]);
    enc.write_bool(1, kTxBypassChroma4x4[0]);
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

std::array<std::uint8_t, 64> reconstruct_luma_8x8(const Residual8x8& residual) {
    std::int32_t coef[64], tmp[64];
    // AV1 系数布局是转置的（index = col * 8 + row），IDCT 前还原成行主序。
    for (int rc = 0; rc < 64; ++rc) {
        const int col = rc >> 3, row = rc & 7;
        const int dq = kDequant8[rc == 0 ? 0 : 1];
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
                                                 ((tmp[i] + 8) >> 4)));
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
    const int cls = magnitude >> 3;
    write_symbol(enc, mv.classes, 10, cls);
    if (cls == 0) {
        const int class0 = (magnitude >> 2) & 1;
        enc.write_bool(class0, mv.class0[0]);
        write_symbol(enc, mv.class0_fp[class0], 4, (magnitude >> 1) & 3);
        enc.write_bool(magnitude & 1, mv.class0_hp[0]);
    } else {
        for (int n = 0; n < cls; ++n)
            enc.write_bool((magnitude >> n) & 1, mv.classN[n][0]);
        write_symbol(enc, mv.classN_fp, 4, (magnitude >> 1) & 3);
        enc.write_bool(magnitude & 1, mv.classN_hp[0]);
    }
}

std::string write_tile_group_inter_8x8(int mv_x, int mv_y, int joint_override) {
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    write_partition_none(enc, cdf);
    // skip=1；intra=0；参考槽 0；newmv_mode=0（NEWMV）+ 逐分量 MV 残差。
    enc.write_bool(1, cdf.m.skip[0][0]);
    enc.write_bool(1, cdf.m.intra[0][0]);
    enc.write_bool(0, cdf.m.ref[0][0][0]);
    enc.write_bool(0, cdf.m.ref[2][0][0]);
    enc.write_bool(0, cdf.m.ref[3][0][0]);
    if (joint_override < 0) {
        // 稳定主链：GLOBALMV + 全 IDENTITY 全局运动 → 零 MV 拷贝。
        enc.write_bool(1, cdf.m.newmv_mode[0][0]);
        enc.write_bool(0, cdf.m.globalmv_mode[0][0]);
    } else {
        // 实验路径：NEWMV 残差语法（mv_joint + X/Y 分量）。
        enc.write_bool(0, cdf.m.newmv_mode[0][0]);
        write_symbol(enc, cdf.mv.joint, 3, joint_override);
        write_mv_component(enc, cdf, 0, mv_y);
        write_mv_component(enc, cdf, 1, mv_x);
    }
    return wrap_tile_data(enc.finish());
}

RefFrame8x8 reconstruct_inter_8x8(const RefFrame8x8& ref, int mv_x, int mv_y) {
    RefFrame8x8 out;
    inter_predict_block(ref.y.data(), 8, 8, 8, out.y.data(), 8, 8, 8,
                        mv_x, mv_y, InterpFilter::Regular);
    inter_predict_block(ref.u.data(), 4, 4, 4, out.u.data(), 4, 4, 4,
                        mv_x / 2, mv_y / 2, InterpFilter::Bilinear);
    inter_predict_block(ref.v.data(), 4, 4, 4, out.v.data(), 4, 4, 4,
                        mv_x / 2, mv_y / 2, InterpFilter::Bilinear);
    return out;
}

}  // namespace termchart::image::codec::av1
