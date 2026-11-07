// parameter_sets.hpp —— HEVC VPS / SPS / PPS 的 POD 结构与写入接口
//
// 头文件只暴露接口与 POD 结构体，不暴露位序列细节。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP

#include <string>

namespace termchart::image::codec::hevc {

// 当前配置固定：Main / 4:2:0 8bit / CTB=64 / I_PCM。只有图像尺寸随输入变化。
// 阶段二打开帧间、SAO、deblocking 等开关时在此追加字段并同步 write_*.cpp。
struct Vps {};

struct Sps {
    int pic_width_in_luma_samples = 0;
    int pic_height_in_luma_samples = 0;
};

struct Pps {};

std::string write_vps(const Vps& vps);
std::string write_sps(const Sps& sps);
std::string write_pps(const Pps& pps);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_PARAMETER_SETS_HPP
