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
};

struct Pps {};

std::string write_vps(const Vps& vps);
std::string write_sps(const Sps& sps);
std::string write_pps(const Pps& pps);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
