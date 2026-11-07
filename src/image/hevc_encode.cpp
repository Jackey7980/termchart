// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
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
#include "codec/hevc/intra_prediction.hpp"
#include "codec/hevc/residual_coding.hpp"
#include "codec/hevc/transquant.hpp"

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

// ── 非 PCM（CABAC 编码树）档：真实帧内编码管线 ─────────────────────────────
// 8x8 帧内 CU：DC 预测 -> 前向变换（亮度 4x4 用 DST-VII）-> 量化 -> 用量化后的
// level 重建（与解码端 ffmpeg 走同一套逆变换/逆量化），重建像素既是本 CU 后续
// TU 的参考，也是后续 CU 的参考。语法层（cu_coding + cabac_arith）此前已按
// FFmpeg 语义逐 bin 核对过，这里只把合成系数换成真实系数。
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

    // 重建帧：帧内预测的参考必须是「重建像素」而非源像素，否则编码端与解码端
    // 的预测基准发散，误差会沿着 CU 扫描顺序一路累积。
    Yuv420 rec;
    rec.cw = f.cw;
    rec.ch = f.ch;
    rec.y.assign(static_cast<std::size_t>(f.cw) * static_cast<std::size_t>(f.ch), 0);
    const std::size_t cn = static_cast<std::size_t>(f.cw / 2) * static_cast<std::size_t>(f.ch / 2);
    rec.u.assign(cn, 128);
    rec.v.assign(cn, 128);

    const int cw = f.cw;
    const int ctb_w = cw / 64, ctb_h = f.ch / 64;
    const int total_ctb = ctb_w * ctb_h;
    int ctb_idx = 0;

    // 全部 PU 都用 DC(1)：MPM 列表只由左/上 PU 模式决定，而它们也都是 DC
    // （边界外推时规范把不可用邻居定为 DC），故 candModeList 恒为 {Planar, DC, 26}，
    // DC 的索引恒为 1。
    constexpr int kDcMode = 1;
    constexpr int kDcMpmIdx = 1;

    auto emit_cu = [&](int x0, int y0) {
        // 4 个 z 序 4x4 TU：顺序必须与解码端 hls_transform_unit 一致——每个 TU 的
        // 左/上参考要么来自已完成的邻块，要么来自本 CU 内已经重建的前序 TU。
        std::int16_t scan[4][16] = {};
        for (int i = 0; i < 4; ++i) {
            const int tx = x0 + ((i & 1) ? 4 : 0);
            const int ty = y0 + ((i & 2) ? 4 : 0);
            const bool has_top = ty > 0;
            const bool has_left = tx > 0;
            std::uint8_t top[5] = {0, 0, 0, 0, 0};
            std::uint8_t left[5] = {0, 0, 0, 0, 0};
            for (int k = 0; k < 4; ++k) {
                if (has_top) top[k] = rec.y[static_cast<std::size_t>(ty - 1) * cw + tx + k];
                if (has_left) left[k] = rec.y[static_cast<std::size_t>(ty + k) * cw + (tx - 1)];
            }
            std::uint8_t pred[16];
            codec::hevc::predict_intra(kDcMode, 4, top, left, has_top, has_left, pred);
            std::int16_t raster[16];
            codec::hevc::encode_tu_4x4(
                &f.y[static_cast<std::size_t>(ty) * cw + tx], cw, pred,
                &rec.y[static_cast<std::size_t>(ty) * cw + tx], cw, raster, slice_qp,
                /*use_dst=*/true);
            // 变换输出是行主序，残差语法要的是对角扫描序。
            for (int n = 0; n < 16; ++n)
                scan[i][n] = raster[codec::hevc::kDiagScan4x4Y[n] * 4 + codec::hevc::kDiagScan4x4X[n]];
        }

        CuIntra8x8 cu;
        cu.part_2nx2n = true;
        cu.prev_intra_luma_pred_flag = true;  // 走 mpm_idx
        cu.mpm_idx = kDcMpmIdx;
        cu.intra_chroma_pred_mode = 4;        // DM（色度跟随亮度 DC）
        cu.cbf_cb = 0;
        cu.cbf_cr = 0;
        for (int i = 0; i < 4; ++i) cu.luma4x4[i] = scan[i];
        const auto bins = codec::hevc::encode_cu_intra_8x8(cu);
        for (const auto& b : bins) {
            if (b.ctx == kBypassCtx) cabac.encode_bypass(b.val);
            else cabac.encode_decision(ctx.ctx[b.ctx], b.val);
        }
    };

    // coding_tree：从 64x64 一路分裂到 8x8（= MinCbLog2SizeY，见 SPS 的
    // log2_min_luma_coding_block_size_minus3=0），故 log2_cb_size==3 的叶子
    // 【不写】split_cu_flag（FFmpeg: log2_cb_size > log2_min_cb_size 才解码）。
    //
    // split_cu_flag 的上下文不是固定的：FFmpeg 用
    //   inc = (depth_left > ct_depth) + (depth_top > ct_depth)
    // 从 min-CB 粒度的 ct_depth 表取左/上邻居的编码深度。用错上下文会让解码端的
    // 算术解码器立刻跑偏（表现：整帧像素全错但 ffmpeg 不报错）。
    const int cw8 = cw / 8;
    std::vector<std::uint8_t> ct_depth(
        static_cast<std::size_t>(cw8) * static_cast<std::size_t>(f.ch / 8), 0);

    std::function<void(int, int, int)> code_tree = [&](int x0, int y0, int depth) {
        const int log2_cb = 6 - depth;
        if (log2_cb > 3) {
            int inc = 0;
            if (x0 > 0 && ct_depth[static_cast<std::size_t>(y0 >> 3) * cw8 + (x0 >> 3) - 1] > depth)
                ++inc;
            if (y0 > 0 && ct_depth[static_cast<std::size_t>((y0 >> 3) - 1) * cw8 + (x0 >> 3)] > depth)
                ++inc;
            cabac.encode_decision(ctx.ctx[kCtxSplitCuFlag + inc], 1);
            const int half = (1 << log2_cb) / 2;
            for (int i = 0; i < 4; ++i)
                code_tree(x0 + ((i & 1) * half), y0 + ((i >> 1) * half), depth + 1);
        } else {
            emit_cu(x0, y0);
            ct_depth[static_cast<std::size_t>(y0 >> 3) * cw8 + (x0 >> 3)] =
                static_cast<std::uint8_t>(depth);
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
#ifdef TERMCHART_HEVC_RECON_DUMP
    {
        std::FILE* fp = std::fopen("tc_enc_recon.yuv", "ab");
        if (fp) {
            std::fwrite(rec.y.data(), 1, rec.y.size(), fp);
            std::fclose(fp);
        }
    }
#endif
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
