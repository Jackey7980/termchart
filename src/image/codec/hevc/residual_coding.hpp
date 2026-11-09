// residual_coding.hpp —— 阶段一 B3：4x4 TU 残差语法的 bin 序列
//
// 只负责「量化系数 -> CABAC bin 序列」；bin -> bits 由 cabac_arith 完成。
// bin 序列与算术层解耦，便于在接真码流前逐字段核对。
#ifndef TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP
#define TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cabac_syntax.hpp"  // Bin / kBypassCtx 由语法层统一提供

namespace termchart::image::codec::hevc {

// 帧内 4x4 TU 的系数扫描方式由亮度模式隐式决定（HEVC 8.5.3.1 /
// FFmpeg hevcdec.c:1396-1403）。扫描表、significant_coeff_flag 的 ctx 映射、
// last_significant_coeff_x/y_prefix 是否交换都随之变化，三者必须成套切换。
enum ScanIdx {
    kScanDiag = 0,   // 模式 0..5 / 15..21 / 31..34
    kScanHoriz = 1,  // 模式 22..30
    kScanVert = 2,   // 模式 6..14
};

ScanIdx scan_idx_for_intra_mode(int mode);

struct Scan4x4Tables {
    const std::uint8_t* x;        // scan position -> 列 x
    const std::uint8_t* y;        // scan position -> 行 y
    const std::uint8_t* ctx_map;  // significant_coeff_flag 的 ctxIdxMap（log2=2 行）
    const std::uint8_t (*inv)[4]; // [行 y][列 x] -> scan position
    bool swap_last_xy;            // 写/读 last_significant_coeff_x/y_prefix 时交换
};

const Scan4x4Tables& scan4x4_tables(ScanIdx idx);

// 4x4 TU 的 HEVC 对角扫描表（scan position -> (x,y)）及反查表。
// 保留导出供调用方把行主序量化系数重排成 kScanDiag 扫描序。
extern const std::uint8_t kDiagScan4x4X[16];
extern const std::uint8_t kDiagScan4x4Y[16];
extern const std::uint8_t kDiagScan4x4Inv[4][4];
// log2_trafo_size==2 / SCAN_DIAG 的 significant_coeff_flag ctx 映射。
extern const std::uint8_t kSigCoeffCtxMap4x4[16];

// coeff[16] 按【scan_idx 对应的扫描序】存放量化 level（含符号）。
// 全零时返回空序列（调用方应写 cbf=0，不进入 residual_coding）。
//
// 当前子集：log2_trafo_size=2、单 CG、sign_data_hiding_flag=0、
// 无 transform_skip / scaling_list；c_idx=0 为亮度、1/2 为 U/V 色度。
// Rice 的 escape 分支（prefix>=3）已实现（对齐 FFmpeg coeff_abs_level_remaining_decode）。
// ok 为 false 表示量化 level 超出 Rice 可表达范围（返回空序列）。
//
// c_idx>0 时 FFmpeg cabac.c 给 4x4 色度 TU 加固定上下文偏移：
// last_significant_coeff_x/y_prefix +15、significant_coeff_flag +27、
// coeff_abs_level_greater1_flag +16、greater2_flag +4；其余语法（扫描表、
// Rice、符号位）与亮度同构。全部落在 kContextCount=183 的已分配范围内。
std::vector<Bin> encode_residual_4x4(const std::int16_t* coeff, bool* ok = nullptr,
                                     ScanIdx scan_idx = kScanDiag, int c_idx = 0);

// 按 FFmpeg 解码语义独立回读，用于自检与回归；成功时写回 coeff 与消耗 bin 数。
bool decode_residual_4x4(const Bin* bins, std::size_t count, std::int16_t* coeff,
                         std::size_t* consumed, ScanIdx scan_idx = kScanDiag,
                         int c_idx = 0);

}  // namespace termchart::image::codec::hevc

#endif  // TERMCHART_SRC_IMAGE_CODEC_HEVC_RESIDUAL_CODING_HPP
