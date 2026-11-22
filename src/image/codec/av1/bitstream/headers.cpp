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
    w.u(0, 1);   // enable_interintra_compound
    w.u(0, 1);   // enable_masked_compound
    w.u(1, 1);   // enable_warped_motion
    w.u(0, 1);   // enable_dual_filter
    w.u(0, 1);   // enable_order_hint（无需 POC 缩放）
    w.u(1, 1);   // allow_screen_content_tools = ADAPTIVE
    w.u(1, 1);   // force_integer_mv = ADAPTIVE
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
        w.u(0, 1);   // have_render_size（read_frame_size 的必备字段）
        // allow_screen_content_tools = 0 ⇒ 不读 allow_intrabc
    } else {
        w.u(1, 2);   // frame_type = INTER_FRAME
        w.u(1, 1);   // show_frame
        w.u(1, 1);   // error_resilient_mode：跳过 primary_ref/ref_mvs/warp 位
        w.u(1, 1);   // disable_cdf_update
        w.u(1, 1);   // allow_screen_content_tools
        w.u(0, 1);   // force_integer_mv = 0（允许亚像素）
        w.u(0, 1);   // frame_size_override = 0
        // enable_order_hint = 0 ⇒ 无 frame_offset / short signaling
        // error_resilient_mode ⇒ 无 primary_ref_frame
        w.u(1, 8);   // refresh_frame_flags：刷新参考槽 0
        for (int i = 0; i < 7; ++i) w.u(0, 3);  // ref_frame_idx[0..6] 全指向槽 0
        // frame_size_override = 0 / superres = 0 ⇒ read_frame_size 只读 1 bit
        w.u(0, 1);   // have_render_size
        w.u(1, 1);   // high_precision_mv
        w.u(0, 1);   // subpel_filter_mode 非 switchable
        w.u(0, 2);   // FILTER_8TAP_REGULAR
        w.u(0, 1);   // switchable_motion_mode
        // order_hint = 0 ⇒ skip_mode 不可用；error_resilient ⇒ 无 warp_motion 位
    }
    // disable_cdf_update = 1 ⇒ 无 refresh_frame_context
    w.u(1, 1);   // uniform_tile_spacing_flag（单 tile）
    w.u(static_cast<unsigned>(cfg.base_q_idx), 8);
    w.u(0, 1);   // delta_q_y_dc
    w.u(0, 1);   // delta_q_u_dc
    w.u(0, 1);   // delta_q_u_ac
    w.u(0, 1);   // using_qmatrix
    w.u(0, 1);   // segmentation_enabled
    w.u(0, 1);   // delta_q_present
    w.u(0, 6);   // loop_filter_level[0]
    w.u(0, 6);   // loop_filter_level[1]（均为 0 ⇒ 不写 uv 两级）
    w.u(0, 3);   // loop_filter_sharpness
    w.u(1, 1);   // loop_filter_delta_enabled
    w.u(0, 1);   // loop_filter_delta_update
    // enable_cdef / enable_restoration = 0 ⇒ 不写对应参数
    w.u(0, 1);   // tx_mode_select = 0（TX_MODE_LARGEST）
    if (cfg.frame_type != 0) w.u(0, 1);  // switchable_comp_refs = 0（单向参考）
    w.u(1, 1);   // reduced_tx_set
    if (cfg.frame_type != 0) {
        // 7 个参考槽的全局运动全部为 IDENTITY；逐块 MV 走 NEWMV 残差语法。
        for (int i = 0; i < 7; ++i) w.u(0, 1);
    }
    return w.finish();
}

}  // namespace termchart::image::codec::av1
