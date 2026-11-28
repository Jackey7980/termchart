// cabac_engine.hpp —— HEVC CABAC 算术编码引擎接口
//
// 只暴露 bin 登记与段提交接口；区间反解求解器、上下文表、规范表全部留在 .cpp。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ENGINE_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ENGINE_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "cabac_context.hpp"

namespace termchart::image::codec::hevc {

class CabacEncoder {
public:
    void init_contexts(int slice_qp);

    // 登记一个 bin；段的实际码流在 flush_cu()/finish() 时统一反解。
    void encode_bin(int ctx_idx, int bin);
    // 终止 bin：pcm_flag / end_of_slice_segment_flag，不占上下文。
    void encode_terminating(int bit);

    // 提交一个 PCM CU 段：[2 字节 CABAC][96 字节裸样本]，随后重置算术引擎。
    void flush_cu(std::string raw);
    // 切片末尾：末段 CABAC + rbsp_stop_one_bit + 补零。
    std::string finish();

private:
    using acc_t = std::int64_t;
    using Bin = std::pair<int, int>;

    static acc_t ceil_div(acc_t p, acc_t q);
    bool solve_bounds(acc_t& lo, acc_t& hi, std::uint8_t* st) const;
    bool verify(std::uint32_t B, bool odd_addr, std::uint8_t raw0,
                const std::uint8_t* st0, bool check_ptr) const;
    std::uint32_t solve(std::uint8_t raw0, bool check_ptr);

    std::string final_out_;   // [CU0 cabac 2B][CU0 raw 96B]...[end cabac][rbsp stop]
    CabacContext contexts_;   // 154 个上下文，跨段保持
    std::vector<Bin> seg_;    // 当前段 bin 序列：first = ctxIdx（-1 为终止 bin）
};

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_CABAC_ENGINE_HPP
