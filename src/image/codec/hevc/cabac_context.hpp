// cabac_context.hpp —— HEVC CABAC 上下文模型接口
//
// 头文件只暴露 POD 与纯函数；Table 9-37 / 9-38 / 9-39 的具体数值留在 .cpp，
// 不向语法层泄漏。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP

#include <cstdint>

namespace termchart::image::codec::hevc {

// state = (pStateIdx << 1) | valMPS（HEVC 9.3.4.2 约定）。
struct CtxModel {
    std::uint8_t state = 0;
};

// 154 个上下文，跨 CABAC 段保持（PCM 后只重置算术引擎，不重置上下文表）。
struct CabacContext {
    CtxModel ctx[154];

    void init(int slice_qp);
    std::uint8_t state(int i) const { return ctx[i].state; }
    void set_state(int i, std::uint8_t s) { ctx[i].state = s; }
};

int range_lps(int state, int range);
std::uint8_t next_state(int state, bool lps);
int norm_shift(int x);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_CONTEXT_HPP
