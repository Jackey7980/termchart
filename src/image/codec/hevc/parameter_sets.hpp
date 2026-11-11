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
};

struct Sps {
    int general_level_idc = 30;
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
    // G0 黄金基线逐字节一致；真压缩档可显式置 true，让每个 cbf=1 的 4x4 TU
    // 在残差语法最前面多读/写一个 transform_skip_flag bin（ctx 47/48）。
    // 不写 pps_extension ⇒ ffmpeg 用 log2_max_transform_skip_block_size 默认 2
    // （ps.c:2243），4x4 TU 满足 log2_trafo_size<=2，无需额外语法位。
    bool transform_skip_enabled = false;
};

std::string write_vps(const Vps& vps);
std::string write_sps(const Sps& sps);
std::string write_pps(const Pps& pps);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
