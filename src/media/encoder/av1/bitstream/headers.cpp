#include "headers.hpp"

#include "../../bitstream/bit_writer.hpp"

namespace termchart::image::codec::av1 {
namespace {

using bitstream::BitWriter;

int bits_for(int value) {
    int n = 0;
    while ((1 << n) <= value) ++n;
    return n ? n : 1;
}

// dav1d_get_uniform 的逆：把 [0, max-1] 内的 value 写成 "l-1 位 + 可能的 1 位"。
// 编码端必须与解码端的区间划分逐位对齐，否则帧头后续字段整体错位。
void write_uniform(BitWriter& w, unsigned max, unsigned value) {
    unsigned l = 1;
    while ((1u << l) <= max) ++l;  // l = ulog2(max) + 1
    const unsigned m = (1u << l) - max;
    if (value < m) {
        w.u(value, static_cast<int>(l - 1));
        return;
    }
    w.u((value + m) >> 1, static_cast<int>(l - 1));
    w.u((value + m) & 1u, 1);
}

// dav1d_get_bits_subexp 的逆（ref = 0 的情形）：把带符号整数 d 编码成
// "P(sym) 分层前缀 + uniform 尾"的 2^(bits+1) 区间值。AV1 的 GMV 各分量都用它。
void write_subexp(BitWriter& w, int d, int bits) {
    const unsigned n = 2u << bits;
    // 解码端返回 inv_recenter(1<<bits, v) - (1<<bits)：偶数 v 正向、奇数 v 反向。
    const unsigned v = d >= 0 ? static_cast<unsigned>(2 * d)
                              : static_cast<unsigned>(-2 * d - 1);
    unsigned acc = 0;
    for (int i = 0;; ++i) {
        const unsigned b = i ? static_cast<unsigned>(3 + i - 1) : 3u;
        if (n < acc + 3u * (1u << b)) {
            write_uniform(w, n - acc + 1, v - acc);
            return;
        }
        if (v < acc + (1u << b)) {
            w.u(0, 1);
            w.u(v - acc, static_cast<int>(b));
            return;
        }
        w.u(1, 1);
        acc += 1u << b;
    }
}

}  // namespace

std::string write_sequence_header(const SequenceHeaderConfig& cfg) {
    BitWriter w;
    const int wbits = bits_for(cfg.width - 1);
    const int hbits = bits_for(cfg.height - 1);
    w.u(0, 3);   // seq_profile = 0 (Main, 8bit 4:2:0)
    w.u(1, 1);   // still_picture
    w.u(1, 1);   // reduced_still_picture_header
    w.u(0, 5);   // seq_level_idx_0 = 0 (level 2.0)
    w.u(static_cast<unsigned>(wbits - 1), 4);
    w.u(static_cast<unsigned>(hbits - 1), 4);
    w.u(static_cast<unsigned>(cfg.width - 1), wbits);
    w.u(static_cast<unsigned>(cfg.height - 1), hbits);
    w.u(0, 1);   // use_128x128_superblock = 0
    w.u(1, 1);   // enable_filter_intra（与 aomenc 参考流一致）
    w.u(1, 1);   // enable_intra_edge_filter
    // reduced ⇒ 不读 interintra/masked/warp/dual/order_hint/screen 工具字段
    w.u(0, 1);   // enable_superres
    w.u(1, 1);   // enable_cdef
    w.u(1, 1);   // enable_restoration
    // color_config()：profile0 4:2:0，8bit，无色彩描述。
    w.u(0, 1);   // high_bitdepth = 0 ⇒ 8bit
    w.u(0, 1);   // mono_chrome = 0
    w.u(0, 1);   // color_description_present_flag
    w.u(0, 1);   // color_range = limited
    // profile0 && !mono ⇒ subsampling_x/y 推断为 1/1，不写
    w.u(0, 2);   // chroma_sample_position
    w.u(0, 1);   // separate_uv_delta_q
    w.u(0, 1);   // film_grain_params_present
    return w.finish();  // trailing_bits
}

std::string write_frame_header(const FrameHeaderConfig& cfg) {
    BitWriter w;
    // reduced_still_picture_header ⇒ frame_type/show_frame/error_resilient 等全推断。
    w.u(1, 1);   // disable_cdf_update = 1 ⇒ 不写 disable_frame_end_update_cdf
    w.u(0, 1);   // allow_screen_content_tools（seq 为 SELECT，reduced 分支仍读）
    // allow_screen_content_tools=0 ⇒ 不读 force_integer_mv 的 bitstream 槽
    // frame_size_override / order_hint / refresh_frame_flags / ref_order_hint 全推断
    // enable_superres=0 ⇒ 不读 use_superres
    w.u(0, 1);   // render_and_frame_size_different
    // allow_intrabc=0（screen content tools off）
    // tile_info()：uniform_tile_spacing_flag 无论帧多大都要读（spec 5.9.15）。
    // 8x8 帧 sbCols=sbRows=1 ⇒ minLog2TileCols/maxLog2TileCols/TileRows 循环
    // 都是 0 位，只此 1 bit；早期版本漏写它，解码端少消费 1 bit，整条帧头位移。
    w.u(1, 1);   // uniform_tile_spacing_flag = 1（单 tile）
    // quantization_params()：base_q_idx 之后，separate_uv_delta_q=0 时只读
    // DeltaQYDc 与 DeltaQUAc 两个 delta_coded 位（DeltaQUDc 属于
    // separate_uv_delta_q=1 的分支，spec 5.9.12 / libaom setup_quantization）。
    // 早期版本多写了一个 DeltaQUDc 位，靠把 base_q_idx 右移一位"凑对齐"，
    // 结果解码端实际拿到的是 qidx>>1 的 dequant 与 Q 上下文，非平坦残差必然偏。
    w.u(static_cast<unsigned>(cfg.base_q_idx), 8);
    w.u(0, 1);   // DeltaQYDc delta_coded
    w.u(0, 1);   // DeltaQUAc delta_coded
    w.u(0, 1);   // using_qmatrix（separate_uv_delta_q=0 ⇒ 不读 qm_v）
    // segmentation_params()
    w.u(0, 1);   // segmentation_enabled
    // 与 libaom 参考流一致：delta_q_present 是显式 bit，不随 base_q_idx 推断。
    w.u(0, 1);   // delta_q_present
    // delta_q_present=0 ⇒ delta_lf_present 不写（对齐 aomenc 参考流）
    // CodedLossless=false ⇒ loop_filter_params()
    w.u(0, 6);   // loop_filter_level[0]
    w.u(0, 6);   // loop_filter_level[1]
    w.u(0, 3);   // loop_filter_sharpness
    w.u(1, 1);   // loop_filter_delta_enabled（与 aomenc 参考流一致）
    w.u(0, 1);   // loop_filter_delta_update
    // enable_cdef=1 ⇒ cdef_params()（与 aomenc 参考流一致）
    w.u(2, 2);   // cdef_damping_minus_3
    w.u(0, 2);   // cdef_bits
    w.u(0, 4);   // cdef_y_pri_strength[0]
    w.u(0, 2);   // cdef_y_sec_strength[0]
    w.u(0, 4);   // cdef_uv_pri_strength[0]
    w.u(0, 2);   // cdef_uv_sec_strength[0]
    // enable_restoration=1 ⇒ lr_params()：3 个平面各 2 bit lr_type
    w.u(0, 2);   // lr_type[0]
    w.u(0, 2);   // lr_type[1]
    w.u(0, 2);   // lr_type[2]
    // enable_cdef=0 / enable_restoration=0 ⇒ 两段全跳过
    w.u(0, 1);   // tx_mode_select=0 ⇒ TX_MODE_LARGEST
    w.u(1, 1);   // reduced_tx_set ⇒ txtp_intra2（4 符号）
    return w.finish();
}

std::string write_sequence_header_inter(const SequenceHeaderConfig& cfg) {
    BitWriter w;
    const int wbits = bits_for(cfg.width - 1);
    const int hbits = bits_for(cfg.height - 1);
    w.u(0, 3);   // seq_profile = 0
    w.u(0, 1);   // still_picture = 0
    w.u(0, 1);   // reduced_still_picture_header = 0
    w.u(0, 1);   // timing_info_present_flag = 0
    w.u(0, 1);   // display_model_info_present_flag = 0
    w.u(0, 5);   // operating_points_cnt_minus_1 = 0
    w.u(0, 12);  // operating_point_idc[0] = 0
    w.u(0, 3);   // seq_level_idx[0] - 2 = 0（level 2.0）
    w.u(0, 2);   // seq_level_idx <= 7 ⇒ 不写 tier
    w.u(static_cast<unsigned>(wbits - 1), 4);
    w.u(static_cast<unsigned>(hbits - 1), 4);
    w.u(static_cast<unsigned>(cfg.width - 1), wbits);
    w.u(static_cast<unsigned>(cfg.height - 1), hbits);
    w.u(0, 1);   // frame_id_numbers_present_flag
    w.u(0, 1);   // use_128x128_superblock
    w.u(1, 1);   // enable_filter_intra
    w.u(1, 1);   // enable_intra_edge_filter
    w.u(cfg.interintra ? 1 : 0, 1);   // enable_interintra_compound
    w.u(cfg.masked_compound ? 1 : 0, 1);   // enable_masked_compound
    w.u(1, 1);   // enable_warped_motion
    w.u(0, 1);   // enable_dual_filter
    w.u(cfg.order_hint ? 1 : 0, 1);   // enable_order_hint
    if (cfg.order_hint) {
        w.u(cfg.jnt_comp ? 1 : 0, 1);  // enable_jnt_comp
        w.u(0, 1);                     // ref_frame_mvs
    }
    w.u(1, 1);   // allow_screen_content_tools = ADAPTIVE
    w.u(1, 1);   // force_integer_mv = ADAPTIVE
    if (cfg.order_hint)
        w.u(static_cast<unsigned>(cfg.order_hint_bits - 1), 3);
    w.u(0, 1);   // enable_superres
    w.u(0, 1);   // enable_cdef
    w.u(0, 1);   // enable_restoration
    w.u(0, 1);   // high_bitdepth
    w.u(0, 1);   // mono_chrome
    w.u(0, 1);   // color_description_present_flag
    w.u(0, 1);   // color_range
    w.u(0, 2);   // chroma_sample_position
    w.u(0, 1);   // separate_uv_delta_q
    w.u(0, 1);   // film_grain_params_present
    return w.finish();
}

std::string write_frame_header_inter(const FrameHeaderConfig& cfg) {
    BitWriter w;
    w.u(0, 1);   // show_existing_frame
    if (cfg.frame_type == 0) {
        // KEY_FRAME：error_resilient_mode 由 KEY+show 推断，refresh_frame_flags=0xff。
        w.u(0, 2);
        w.u(1, 1);   // show_frame
        w.u(1, 1);   // disable_cdf_update
        w.u(0, 1);   // allow_screen_content_tools
        w.u(0, 1);   // frame_size_override
        if (cfg.order_hint)
            w.u(static_cast<unsigned>(cfg.frame_offset), cfg.order_hint_bits);
        w.u(0, 1);   // have_render_size（read_frame_size 的必备字段）
        // allow_screen_content_tools = 0 ⇒ 不读 allow_intrabc
    } else {
        w.u(1, 2);   // frame_type = INTER_FRAME
        w.u(1, 1);   // show_frame
        w.u(cfg.error_resilient ? 1 : 0, 1);   // error_resilient_mode
        w.u(1, 1);   // disable_cdf_update
        w.u(1, 1);   // allow_screen_content_tools
        w.u(0, 1);   // force_integer_mv = 0（允许亚像素）
        w.u(0, 1);   // frame_size_override = 0
        if (cfg.order_hint)
            w.u(static_cast<unsigned>(cfg.frame_offset), cfg.order_hint_bits);
        if (!cfg.error_resilient)
            w.u(static_cast<unsigned>(cfg.primary_ref_frame & 7), 3);
        w.u(1, 8);   // refresh_frame_flags：刷新参考槽 0
        if (cfg.order_hint && cfg.error_resilient) {
            // 仅 error_resilient_mode=1 才写 8 个 ref_order_hint。
            for (int i = 0; i < 8; ++i) w.u(0, cfg.order_hint_bits);
        }
        if (cfg.order_hint)
            w.u(0, 1);   // frame_ref_short_signaling = 0
        for (int i = 0; i < 7; ++i) w.u(0, 3);  // ref_frame_idx[0..6] 全指向槽 0
        // frame_size_override = 0 / superres = 0 ⇒ read_frame_size 只读 1 bit
        w.u(0, 1);   // have_render_size
        w.u(1, 1);   // high_precision_mv
        w.u(0, 1);   // subpel_filter_mode 非 switchable
        w.u(static_cast<unsigned>(cfg.interp_filter & 3), 2);
        // is_motion_mode_switchable：打开后每个有可重叠邻居的帧间块多读一个
        // motion_mode 符号。error_resilient_mode=1 不写 warp 位，解码端只读
        // 单符号 obmc 布尔，因此这里不需要额外的 warp 参数。
        w.u(cfg.switchable_motion_mode ? 1 : 0, 1);
        // order_hint = 0 ⇒ skip_mode 不可用
    }
    // disable_cdf_update = 1 ⇒ 无 refresh_frame_context
    w.u(1, 1);   // uniform_tile_spacing_flag（单 tile）
    w.u(static_cast<unsigned>(cfg.base_q_idx), 8);
    w.u(0, 1);   // delta_q_y_dc
    w.u(0, 1);   // delta_q_u_dc
    w.u(0, 1);   // delta_q_u_ac
    w.u(cfg.using_qmatrix ? 1 : 0, 1);
    if (cfg.using_qmatrix) {
        w.u(static_cast<unsigned>(cfg.qm_y & 15), 4);
        w.u(static_cast<unsigned>(cfg.qm_u & 15), 4);
    }
    w.u(0, 1);   // segmentation_enabled
    w.u(0, 1);   // delta_q_present
    w.u(static_cast<unsigned>(cfg.loop_filter_level_y & 63), 6);
    w.u(static_cast<unsigned>(cfg.loop_filter_level_y & 63), 6);
    // 非单色流中只要亮度/色度级别非零，就还要写 level_u / level_v 两级。
    if (cfg.loop_filter_level_y || cfg.loop_filter_level_uv) {
        w.u(static_cast<unsigned>(cfg.loop_filter_level_uv & 63), 6);
        w.u(static_cast<unsigned>(cfg.loop_filter_level_uv & 63), 6);
    }
    w.u(static_cast<unsigned>(cfg.loop_filter_sharpness & 7), 3);
    // 打开去块时关掉 mode/ref 增量，使每块的实际级别就等于帧头 level；
    // 否则 INTRA/参考槽的默认 delta 会让像素级预期难以对齐。
    const bool lf_delta_enabled =
        !(cfg.loop_filter_level_y || cfg.loop_filter_level_uv);
    w.u(lf_delta_enabled ? 1 : 0, 1);
    if (lf_delta_enabled) w.u(0, 1);  // loop_filter_delta_update
    // enable_cdef / enable_restoration = 0 ⇒ 不写对应参数
    w.u(0, 1);   // tx_mode_select = 0（TX_MODE_LARGEST）
    // switchable_comp_refs=0 ⇒ 每个帧间块都不读 is_compound，只能单向参考。
    if (cfg.frame_type != 0) w.u(cfg.compound_refs ? 1 : 0, 1);
    // warp_motion 位于 ext-skip 之后、reduced_tx_set 之前（不是紧跟 motion_mode 开关）。
    if (!cfg.error_resilient && cfg.frame_type != 0)
        w.u(cfg.warp_motion ? 1 : 0, 1);
    w.u(1, 1);   // reduced_tx_set
    if (cfg.frame_type != 0) {
        // 7 个参考槽的全局运动：槽 0（LAST，本流水线所有块都指向它）承载可配置的
        // 全局平移，其余槽保持 IDENTITY。wmtype 编码为 1/2/3 位：
        // 0 ⇒ IDENTITY、11 ⇒ ROT_ZOOM、101 ⇒ TRANSLATION、100 ⇒ AFFINE。
        for (int i = 0; i < 7; ++i) {
            const int type = i == 0 ? cfg.global_mv_type : 0;
            if (type == 0) {
                w.u(0, 1);  // IDENTITY
                continue;
            }
            w.u(1, 1);  // is_global
            if (type == 1) {
                w.u(0b01, 2);  // is_rot_zoom=0, is_translation=1
                // high_precision_mv=1：TRANSLATION 的参数单位 1/8 像素（9 位）。
                write_subexp(w, cfg.global_mv_y, 9);
                write_subexp(w, cfg.global_mv_x, 9);
                continue;
            }
            // ROTZOOM (2) / AFFINE (3)：alpha,beta[,gamma,delta],y,x 全是 12 位 subexp。
            w.u(type == 2 ? 1 : 0, 1);
            if (type == 3) w.u(0, 1);  // is_translation=0
            write_subexp(w, cfg.gm_alpha, 12);
            write_subexp(w, cfg.gm_beta, 12);
            if (type == 3) {
                write_subexp(w, cfg.gm_gamma, 12);
                write_subexp(w, cfg.gm_delta, 12);
            }
            write_subexp(w, cfg.gm_trans_y, 12);
            write_subexp(w, cfg.gm_trans_x, 12);
        }
    }
    return w.finish();
}

}  // namespace termchart::image::codec::av1
