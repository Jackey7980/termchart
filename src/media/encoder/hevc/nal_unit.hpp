// nal_unit.hpp —— HEVC NAL 单元头与类型分发
//
// 头文件只暴露接口与 POD 常量，不含参数集或 CABAC 内部细节。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_NAL_UNIT_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_NAL_UNIT_HPP

#include <string>

namespace termchart::image::codec::hevc {

enum class NalType : int {
    IdrWRadl = 19,
    Vps = 32,
    Sps = 33,
    Pps = 34,
};

// 2 字节头：forbidden_zero(1) | nal_unit_type(6) | nuh_layer_id(6) |
// nuh_temporal_id_plus1(3)（layer=0、temporal=1），再套用通用 Annex-B 封装。
std::string make_nal(int type, const std::string& rbsp);

inline std::string make_nal(NalType type, const std::string& rbsp) {
    return make_nal(static_cast<int>(type), rbsp);
}

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_NAL_UNIT_HPP
