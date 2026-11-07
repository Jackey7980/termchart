// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "codec/bitstream/bit_writer.hpp"
#include "codec/hevc/nal_unit.hpp"
#include "codec/hevc/parameter_sets.hpp"
#include "codec/hevc/slice_header.hpp"
#include "codec/hevc/cabac_engine.hpp"
#include "codec/hevc/cabac_syntax.hpp"
#include "codec/hevc/cabac_arith.hpp"
#include "codec/hevc/cabac_context.hpp"
#include "codec/hevc/cu_coding.hpp"
#include "codec/hevc/ctb_tree.hpp"

namespace termchart::image {
namespace {

// ── 通用比特流基建（阶段 9 · A1）─────────────────────────────────────────
// 位写入器 / 防竞争 / Annex-B / NAL 头已下沉到 image/codec/bitstream 与
// image/codec/hevc/nal_unit。I_PCM 的编码逻辑仍自包含，只替换底层实现；
// 门禁 G0（diag_hevc.py 6/6 + 20430B/sha）保证码流字节级不变。
using codec::bitstream::BitWriter;
using codec::hevc::make_nal;

int clamp8(int v) { return codec::bitstream::clamp_u8(v); }

struct Yuv420 {
    int cw = 0, ch = 0;
    std::vector<uint8_t> y, u, v;
};

// RGB(A) → YUV420（BT.601），按 64 对齐补边（HEVC CTB = 64）。
Yuv420 to_yuv(const Bitmap& bmp) {
    const int W = static_cast<int>(bmp.width());
    const int H = static_cast<int>(bmp.height());
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    const size_t cw_s  = static_cast<size_t>(cw);
    const size_t ch_s  = static_cast<size_t>(ch);
    const size_t hcw_s = cw_s / 2;            // 色度行宽（半宽）
    const size_t hch_s = ch_s / 2;            // 色度列高（半高）
    const size_t chroma_n = hcw_s * hch_s;    // U/V 平面元素数，只算一次

    Yuv420 out;
    out.cw = cw;
    out.ch = ch;
    out.y.assign(cw_s * ch_s, 0);
    out.u.assign(chroma_n, 128);
    out.v.assign(chroma_n, 128);
    for (int yy = 0; yy < H; ++yy) {
        for (int xx = 0; xx < W; ++xx) {
            const int r = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 0);
            const int g = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 1);
            const int bb = bmp.at(static_cast<uint32_t>(xx), static_cast<uint32_t>(yy), 2);
            const int ly = clamp8(((66 * r + 129 * g + 25 * bb + 128) >> 8) + 16);
            const size_t y_idx = static_cast<size_t>(yy) * cw_s + static_cast<size_t>(xx);
            out.y[y_idx] = static_cast<uint8_t>(ly);
            if ((xx & 1) == 0 && (yy & 1) == 0) {
                const int lu = clamp8(((-38 * r - 74 * g + 112 * bb + 128) >> 8) + 128);
                const int lv = clamp8(((112 * r - 94 * g - 18 * bb + 128) >> 8) + 128);
                const size_t idx = static_cast<size_t>(yy / 2) * hcw_s + static_cast<size_t>(xx / 2);
                out.u[idx] = static_cast<uint8_t>(lu);
                out.v[idx] = static_cast<uint8_t>(lv);
            }
        }
    }
    return out;
}

// ── 参数集已下沉（阶段 9 · A2）───────────────────────────────────────────
// VPS/SPS/PPS 的位序列在 codec/hevc/parameter_sets.{hpp,cpp}，
// slice_segment_header() 在 codec/hevc/slice_header.{hpp,cpp}。
// 本文件保留 I_PCM 的 CABAC / 编码树 / PCM 样本逻辑，门禁 G0 保证字节级不变。

// ── CABAC 上下文与引擎已下沉（阶段 9 · A3-1/A3-2）─────────────────────
// Table 9-37~9-39 与 init_values 在 codec/hevc/cabac_context.{hpp,cpp}，
// 区间反解引擎在 codec/hevc/cabac_engine.{hpp,cpp}。
// 本文件保留 I_PCM 的编码树 / PCM 样本逻辑，门禁 G0 保证字节级不变。
using codec::hevc::CabacEncoder;

std::string build_idr_slice(const Yuv420& f, int slice_qp) {
    codec::hevc::SliceHeader sh;
    std::string rbsp = codec::hevc::write_slice_header(sh);
    CabacEncoder cabac;
    cabac.init_contexts(slice_qp);
    bool first_ctb = true;
    for (int ctb_y = 0; ctb_y < f.ch; ctb_y += 64) {
        for (int ctb_x = 0; ctb_x < f.cw; ctb_x += 64) {
            // HEVC 7.3.8.1：每个 CTB 的 coding_tree_unit() 之后都读一次
            // end_of_slice_segment_flag。除最后一个 CTB 外必须为 0，否则解码器
            // 会提前结束 slice，剩余 CTB 全部丢失（表现为右半/下半帧全错）。
            // 该 bin 属于【重置后】的新 CABAC 引擎，即下一个 CTB 首个 CU 段的段首。
            if (!first_ctb) codec::hevc::write_end_of_slice_segment_flag(cabac, 0);
            first_ctb = false;
            const codec::hevc::CtbFrameView view{f.y.data(), f.u.data(), f.v.data(),
                                                  f.cw, f.ch};
            codec::hevc::code_ctb(cabac, view, ctb_x, ctb_y);
        }
    }
    codec::hevc::write_end_of_slice_segment_flag(cabac, 1);  // 最后一个 CTB 的 end_of_slice_segment_flag = 1
    rbsp += cabac.finish();
    return rbsp;
}

// ── 非 PCM（CABAC 编码树）档：阶段一 B3-4 ───────────────────────────────────
// 用已经 bin 化、且对偶 FFmpeg 语义验证过的 8x8 帧内 CU 语法（cu_coding +
// cabac_arith），把帧编码成真正的 CABAC 码流。当前只做「语法/码流级真压缩档」：
// 不做前向变换/量化/帧内预测（系数是合成的小值），仅验证「我们产出的 bin 序列经
// CABAC 写出后，能被标准解码器 ffmpeg 解出来且不报错」。变换/量化/预测是后续步骤。
using codec::hevc::CabacArith;
using codec::hevc::CabacContext;
using codec::hevc::CuIntra8x8;
using codec::hevc::kBypassCtx;
using codec::hevc::kCtxSplitCuFlag;

std::string build_intra_idr_slice(const Yuv420& f, int slice_qp) {
    codec::hevc::SliceHeader sh;
    std::string rbsp = codec::hevc::write_slice_header(sh);
    CabacArith cabac;
    cabac.init();
    CabacContext ctx;
    ctx.init_hevc(slice_qp);  // I slice -> initType 0

    const int cw8 = f.cw / 8;
    const int ctb_w = f.cw / 64, ctb_h = f.ch / 64;
    const int total_ctb = ctb_w * ctb_h;
    int ctb_idx = 0;

    auto emit_cu = [&](int x0, int y0, int global_cu) {
        CuIntra8x8 cu;
        cu.part_2nx2n = true;
        cu.prev_intra_luma_pred_flag = true;  // 走 mpm_idx
        cu.mpm_idx = global_cu % 3;
        cu.intra_chroma_pred_mode = 4;        // DM
        cu.cbf_cb = 0;
        cu.cbf_cr = 0;
        std::int16_t blocks[4][16] = {};
        // 放少量 1..3 的小 DC，强制走 residual 路径又不触发 escape（escape 未实现）。
        if (global_cu % 13 == 0) blocks[0][0] = static_cast<std::int16_t>(1 + (global_cu % 3));
        cu.luma4x4[0] = blocks[0];
        cu.luma4x4[1] = nullptr;
        cu.luma4x4[2] = nullptr;
        cu.luma4x4[3] = nullptr;
        const auto bins = codec::hevc::encode_cu_intra_8x8(cu);
        for (const auto& b : bins) {
            if (b.ctx == kBypassCtx) cabac.encode_bypass(b.val);
            else cabac.encode_decision(ctx.ctx[b.ctx], b.val);
        }
    };

    // coding_tree：从 64x64 一路分裂到 8x8。均质 8x8 网格下 split_cu_flag 的相邻
    // 深度恒等于当前深度 -> FFmpeg 的 inc=(depth_left>ct_depth)+(depth_top>ct_depth)=0
    // -> 所有 split_cu_flag 都用 ctx == kCtxSplitCuFlag(=2)。
    std::function<void(int, int, int)> code_tree = [&](int x0, int y0, int depth) {
        if (depth < 3) {
            cabac.encode_decision(ctx.ctx[kCtxSplitCuFlag], 1);
            const int half = (1 << (6 - depth)) / 2;
            for (int i = 0; i < 4; ++i)
                code_tree(x0 + ((i & 1) * half), y0 + ((i >> 1) * half), depth + 1);
        } else {
            cabac.encode_decision(ctx.ctx[kCtxSplitCuFlag], 0);
            emit_cu(x0, y0, (y0 / 8) * cw8 + (x0 / 8));
        }
    };

    for (int ctb_y = 0; ctb_y < f.ch; ctb_y += 64) {
        for (int ctb_x = 0; ctb_x < f.cw; ctb_x += 64) {
            code_tree(ctb_x, ctb_y, 0);
            cabac.encode_terminate(ctb_idx == total_ctb - 1 ? 1 : 0);
            ++ctb_idx;
        }
    }
    rbsp += cabac.finish();
    return rbsp;
}

std::string encode_hevc_intra_annexb(std::span<const Bitmap> frames) {
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H)
            return std::string{};
    }
    // 非 PCM 路径的 coding_tree 不做 CTB 边界部分块，要求尺寸为 64 的整数倍；
    // 不满足时显式返回空（fail-loud，不静默降级成 PCM）。
    if (W % 64 != 0 || H % 64 != 0) return std::string{};
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    std::string out;
    const int level_idc = (cw >= 3840 || ch >= 2160) ? 153 : 30;
    codec::hevc::Vps vps;
    vps.general_level_idc = level_idc;
    out += make_nal(32, codec::hevc::write_vps(vps));  // VPS
    codec::hevc::Sps sps;
    sps.general_level_idc = level_idc;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
    sps.conf_win_bottom = (ch - H) / 2;
    sps.pcm_enabled = false;                          // 关 PCM，走 CABAC 编码树
    sps.max_transform_hierarchy_depth_intra = 1;      // 允许 8x8 -> 4x4 一级分裂
    out += make_nal(33, codec::hevc::write_sps(sps));  // SPS
    out += make_nal(34, codec::hevc::write_pps(codec::hevc::Pps{}));  // PPS
    for (const auto& frame : frames) {
        const Yuv420 f = to_yuv(frame);
        out += make_nal(19, build_intra_idr_slice(f, 26));  // IDR slice, QP = 26
    }
    return out;
}

// I_PCM 档：完整自包含实现。后续把 bit_writer / parameter_sets / cabac_* /
// ctb_tree 等下沉到 image/codec/hevc/ 时，只服务真压缩档；这条 PCM 路径保持
// 原样，作为 diag_hevc.py 的字节级黄金回归基线，任何一步都不许改变其码流。
std::string encode_hevc_pcm_annexb(std::span<const Bitmap> frames) {
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H) {
            return std::string{};  // 暂要求统一尺寸
        }
    }
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    std::string out;
    // Level 声明必须覆盖实际图像尺寸/采样率：4K 用 Level 5.1(153)，小尺寸沿用
    // 3.0(30)。把 3.0 挂在 4K 上会让硬解按错误的 DPB/码率预算拒绝码流。
    const int level_idc = (cw >= 3840 || ch >= 2160) ? 153 : 30;
    codec::hevc::Vps vps;
    vps.general_level_idc = level_idc;
    out += make_nal(32, codec::hevc::write_vps(vps));  // VPS
    codec::hevc::Sps sps;
    sps.general_level_idc = level_idc;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
    // 4:2:0 下 CropUnitY = SubHeightC = 2，故裁掉的行数除以 2。
    sps.conf_win_bottom = (ch - H) / 2;
    out += make_nal(33, codec::hevc::write_sps(sps));  // SPS
    out += make_nal(34, codec::hevc::write_pps(codec::hevc::Pps{}));  // PPS
    for (const auto& frame : frames) {
        const Yuv420 f = to_yuv(frame);
        out += make_nal(19, build_idr_slice(f, 26));  // IDR slice, slice QP = 26
    }
    return out;
}

}  // namespace

std::string encode_hevc_annexb(std::span<const Bitmap> frames, const EncodeOptions& opts) {
    switch (opts.video_mode) {
        case VideoEncodeMode::Pcm:
            return encode_hevc_pcm_annexb(frames);
        case VideoEncodeMode::Intra:
            return encode_hevc_intra_annexb(frames);
        case VideoEncodeMode::Inter:
            // 阶段二才落地；显式返回空串，不静默降级成 PCM。
            return std::string{};
    }
    return std::string{};
}

}  // namespace termchart::image
