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
    // quantization_params()
    w.u(static_cast<unsigned>(cfg.base_q_idx), 8);
    w.u(0, 1);   // DeltaQYDc delta_coded
    w.u(0, 1);   // DeltaQUDc delta_coded
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

}  // namespace termchart::image::codec::av1
