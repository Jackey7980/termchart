// parameter_sets.hpp —— HEVC VPS / SPS / PPS 的 POD 结构与写入接口
//
// 头文件只暴露接口与 POD 结构体，不暴露位序列细节。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP

#include <string>

namespace termchart::image::codec::hevc {

// 当前配置固定：Main / 4:2:0 8bit / CTB=64 / I_PCM。只有图像尺寸随输入变化。
// 阶段二打开帧间、SAO、deblocking 等开关时在此追加字段并同步 write_*.cpp。
struct Vps {
    // VPS / SPS 的 profile_tier_level 必须写同一个 level；4K 需 Level 5.1(153)。
    int general_level_idc = 30;
    // 必须与 SPS 一致：VPS 的 PTL 与 SPS 不匹配时部分解码器直接拒流。
    int general_profile_idc = 1;
    // vps_max_dec_pic_buffering_minus1[0]。帧间档至少要 1（能留住 1 帧参考）；
    // 0 是既有 I_PCM 基线口径（单帧、无参考），改动会移位后续 VPS 比特。
    int max_dec_pic_buffering_minus1 = 0;
    // vps_max_num_reorder_pics[0]：B 片档与 SPS 同步置 1。
    int max_num_reorder_pics = 0;
};

struct Sps {
    int general_level_idc = 30;
    // general_profile_idc：1 = Main（默认，字节级 G0 口径）；4 = Range Extensions。
    // 4 是 transform_skip 超出 4x4 的必要条件——FFmpeg 只在 profile_idc >= 4 时
    // 才解析 PPS range extension（ps.c:2431），否则 log2_max_transform_skip_block_size
    // 被当成默认 2，大 TU 的 transform_skip_flag 根本不会被读。
    int general_profile_idc = 1;
    // sps_range_extension_flag：置 1 时追加 sps_range_extension()（9 个开关全 0）。
    // 与 profile_idc=4 配套打开，让解码端确认这是 RExt 语义的码流。
    bool range_extension = false;
    int pic_width_in_luma_samples = 0;
    int pic_height_in_luma_samples = 0;
    // conformance window 裁剪（单位 CropUnitY：4:2:0 下 = 2 个亮度行）。
    // CTB 对齐产生的底部 padding 靠这个字段裁掉，让显示尺寸回到真实 W/H。
    int conf_win_bottom = 0;
    // pcm_enabled_flag。true = 沿用既有 I_PCM 路径（字节级与 G0 基准一致）；
    // false = 关 PCM，进入 CABAC 编码树（真压缩档），需配合 max_thd_intra>=1。
    bool pcm_enabled = true;
    // max_transform_hierarchy_depth_intra：PCM 路径固定为 0；非 PCM 路径需 >=1
    // 才能让 8x8 CU 的 transform_tree 做 8x8 -> 4x4 这一级分裂。
    int max_transform_hierarchy_depth_intra = 0;
    // log2_diff_max_min_luma_transform_block_size：默认 3（MaxTb=32）是 I_PCM
    // 黄金基线的字节级口径，PCM 档不得改；真压缩档只有 4x4 TU，必须显式设 0，
    // 让解码端在 8x8 根节点隐式分裂、不读 split_transform_flag。
    int log2_diff_max_min_transform_block_size = 3;
    // sample_adaptive_offset_enabled_flag（HEVC 7.3.4.2.1）。false = 沿用既有
    // 字节级基线（SPS 该位写 0）；true = 声明 SAO，slice header 追加
    // slice_sao_luma/chroma_flag，每个 CTB 前追加 sao() 语法（见 sao.hpp）。
    bool sao_enabled = false;
    // log2_max_pic_order_cnt_lsb_minus4。0 = 既有单帧基线（不写 POC，IDR 也不需要）；
    // 帧间档必须 >0：非 IDR 切片的 slice_pic_order_cnt_lsb 位宽 = 该值 + 4。
    int log2_max_pic_order_cnt_lsb_minus4 = 0;
    // sps_max_dec_pic_buffering_minus1（层 0）。参考帧 + 当前帧至少 2 帧 → 写 1。
    int sps_max_dec_pic_buffering_minus1 = 0;
    // sps_max_num_reorder_pics[0]：B 片档须 >=1，否则解码器按解码序直接输出。
    int max_num_reorder_pics = 0;
};

struct Pps {
    // false（默认）= deblocking_filter_control_present_flag 写 0，PPS 位序列与 G0 黄金
    // 基线逐字节一致——PCM 档依赖这一点，任何改动都不许影响它。
    //
    // true = 打开 deblocking_filter_control_present_flag 并令
    // pps_deblocking_filter_disabled_flag=1，使解码端不做去块滤波，从而让
    // 「编码端重建 == 解码端输出」这个逐像素判据真正成立。
    //
    // deblocking_filter_override_enabled_flag 恒写 0：按 HEVC 7.3.6.1，
    // deblocking_filter_override_flag 随之被推断为 0，slice header 不必写
    // slice_deblocking_filter_disabled_flag / beta / tc——**本开关不增加任何一个
    // slice 比特**，只改 PPS。
    bool disable_deblocking = false;

    // transform_skip_enabled_flag（HEVC 7.3.4.3.1）。默认 false ⇒ PPS 位序列与
    // G0 黄金基线逐字节一致；真压缩档可显式置 true，让每个 cbf=1 的 TU
    // 在残差语法最前面多读/写一个 transform_skip_flag bin（ctx 47/48）。
    bool transform_skip_enabled = false;

    // log2_max_transform_skip_block_size（HEVC 7.3.2.3.2，写在 pps_range_extension）。
    // 2 = 沿用 FFmpeg 默认（ps.c:2243），只覆盖 4x4 TU，**不写** pps_extension，
    // 故 PPS 位序列仍与既有基线兼容。>=3 时写 pps_extension_present_flag +
    // pps_range_extension_flag 并声明 log2_max_transform_skip_block_size_minus2，
    // 使 8/16/32 TU 也能带 transform_skip_flag；此时 SPS 必须同时声明 RExt profile
    // （general_profile_idc=4），否则 FFmpeg 根本不解析这段 extension。
    int log2_max_transform_skip_block_size = 2;
};

std::string write_vps(const Vps& vps);
std::string write_sps(const Sps& sps);
std::string write_pps(const Pps& pps);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
