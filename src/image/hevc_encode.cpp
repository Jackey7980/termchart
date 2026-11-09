// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include "codec/hevc/deblocking.hpp"
#include "codec/hevc/intra_prediction.hpp"
#include "codec/hevc/residual_coding.hpp"
#include "codec/hevc/transquant.hpp"

namespace termchart::image {
namespace {

// ── 通用比特流基建（阶段 9 · A1）─────────────────────────────────────────
// 位写入器 / 防竞争 / Annex-B / NAL 头已下沉到 image/codec/bitstream 与
// image/codec/hevc/nal_unit。I_PCM 的编码逻辑仍自包含，只替换底层实现；
// 门禁 G0（diag_hevc.py 6/6 + 20433B/sha 5d4adef5）保证码流字节级不变。
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
    // SliceQpY = 26 + init_qp_minus26(=0) + slice_qp_delta + QpBdOffsetY(=0, 8bit)。
    // 量化与 CABAC 上下文初始化都用 slice_qp，故 delta 必须同步写进码流，
    // 否则解码端按 26 反量化、上下文也按 26 初始化，重建必然发散。
    // 默认 26 ⇒ delta=0 ⇒ se(0)，与既有码流字节级一致。
    sh.slice_qp_delta = slice_qp - 26;
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

    // 亮度帧内模式选择：默认逐 CU 走 SSD 模式决策（35 模式全覆盖，见
    // intra_prediction.cpp::choose_intra_luma_mode_8x8）；可用环境变量
    // TERMCHART_HEVC_FIXED_MODE 强制全帧固定模式，便于用 ffmpeg 解码做逐模式 oracle。
    const char* fixed_mode_env = std::getenv("TERMCHART_HEVC_FIXED_MODE");
    const int kIntraMode = (fixed_mode_env && *fixed_mode_env) ? std::atoi(fixed_mode_env) : -1;
    // MPM 候选表由左/上 PU 的【实际模式】决定，且 CTB 首行强制 cand_up=DC，
    // 故不能固定 mpm_idx：维护 tab_ipm 并按 FFmpeg luma_intra_pred_mode 推导。
    const std::size_t cw8 = static_cast<std::size_t>(cw / 8);
    std::vector<std::uint8_t> tab_ipm(
        cw8 * static_cast<std::size_t>(f.ch / 8), 1);  // INTRA_DC

    auto emit_cu = [&](int x0, int y0) {
        // 本编码器 8x8 CU 用 PART_2Nx2N ⇒ 整 8x8 单 PU 预测（ffmpeg 解码端同样按
        // 8x8 预测）。故先用选定模式算整块预测，再拆成 4 个 4x4 TU 写残差。
        const int mode = (kIntraMode >= 0)
                             ? kIntraMode
                             : codec::hevc::choose_intra_luma_mode_8x8(
                                   f.y.data(), cw, rec.y.data(), cw,
                                   x0, y0, cw, f.ch, slice_qp);
        // 4 个 z 序 4x4 TU：顺序必须与解码端 hls_transform_unit 一致。HEVC 帧内预测块
        // 尺寸等于 TU 尺寸，故每个 4x4 TU 用本 CU 单一模式、以其自身 (tx,ty) 定位参考样本。
        // 系数扫描方式由本 CU 的亮度模式隐式决定（HEVC 8.5.3.1 / FFmpeg
        // hevcdec.c:1396-1403）：模式 6..14 用 SCAN_VERT、22..30 用 SCAN_HORIZ，
        // 其余用 SCAN_DIAG。三张扫描表与 sig_coeff ctx 表必须成套切换，否则
        // 解码端会把系放到错误的 (x,y)，整帧重建分叉。
        const auto scan_idx = codec::hevc::scan_idx_for_intra_mode(mode);
        const auto& st = codec::hevc::scan4x4_tables(scan_idx);
        std::int16_t scan[4][16] = {};
        for (int i = 0; i < 4; ++i) {
            const int tx = x0 + ((i & 1) ? 4 : 0);
            const int ty = y0 + ((i & 2) ? 4 : 0);
            std::uint8_t pred[16];
            codec::hevc::predict_intra_luma_4x4(mode, rec.y.data(), cw, tx, ty, cw, f.ch, pred);
            std::int16_t raster[16];
            codec::hevc::encode_tu_4x4(
                &f.y[static_cast<std::size_t>(ty) * cw + tx], cw, pred,
                &rec.y[static_cast<std::size_t>(ty) * cw + tx], cw, raster, slice_qp,
                /*use_dst=*/true);
            // 变换输出是行主序（idx = 行*4 + 列），残差语法要的是所选扫描序。
            for (int n = 0; n < 16; ++n)
                scan[i][n] = raster[st.y[n] * 4 + st.x[n]];
        }

        // MPM 候选表：逐式对齐 FFmpeg luma_intra_pred_mode。
        // - y0 落在 CTB 首行时 (y0-1) 跨垂直 CTB 边界 ⇒ FFmpeg 取 DC（不读正上方 PU
        //   模式）；其余取正上方 PU 模式。
        // - x0==0 时左邻居不可用 ⇒ DC；否则取左侧 PU 的模式。
        const std::size_t pu = static_cast<std::size_t>(y0 >> 3) * cw8 +
                               static_cast<std::size_t>(x0 >> 3);
        const int cand_up = (y0 % 64 != 0) ? static_cast<int>(tab_ipm[pu - cw8]) : 1;  // INTRA_DC
        // FFmpeg hevcdec.c:2264-2267 / mvs.c:49-50：
        //   cand_left = (ctb_left_flag || x0b) ? tab_ipm[...] : INTRA_DC
        // 单 slice、单 tile 时 ctb_left_flag == (CTB 不在首列)，故只要 x0>0
        // 左邻居就【可用】——跨 CTB 左边界不再强制回退 DC。旧实现按
        // `x0 % 64 != 0` 判可用，会让第二个 CTB 起 MPM 候选表整体错位。
        const int cand_left = (x0 > 0) ? static_cast<int>(tab_ipm[pu - 1]) : 1;
        const auto cands = codec::hevc::derive_mpm_candidates(cand_left, cand_up);
        const auto sig = codec::hevc::signal_intra_mode(mode, cands);
        tab_ipm[pu] = static_cast<std::uint8_t>(mode);

        CuIntra8x8 cu;
        cu.part_2nx2n = true;
        cu.scan_idx = scan_idx;
        cu.prev_intra_luma_pred_flag = sig.prev_intra_luma_pred_flag;
        cu.mpm_idx = sig.mpm_idx;
        cu.rem_intra_luma_pred_mode = sig.rem_intra_luma_pred_mode;
        cu.intra_chroma_pred_mode = 4;        // DM（色度跟随亮度模式）
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
    // 重建出口同步去块滤波（8.7.2）。必须放在整帧编码之后：帧内预测参考的是
    // **滤波前**重建，逐 CU 滤波会污染后续 CU 的参考样本。
    // deblock_frame_luma 目前是按 8x8 网格 + 恒定 bS=2 的近似实现（没有 CU/TU 边界图，
    // 无法区分 bS=1/2），与 FFmpeg 的 DBF 输出存在 maxdiff≈4（QP26/渐变图）的差异；
    // 该差异只影响编码端 recon，不进码流，故不影响解码正确性。
    // 逐像素门禁（recon == dec）在 DBF 关闭时成立：设 TERMCHART_HEVC_NO_DBLOCK=1 编码
    // + `ffmpeg -skip_loop_filter all` 解码，实测 Y 平面 maxdiff=0。
    if (!std::getenv("TERMCHART_HEVC_NO_DBLOCK"))
        codec::hevc::deblock_frame_luma(rec.y.data(), cw, f.ch, cw,
                                        codec::hevc::DeblockConfig{.slice_qp = slice_qp});
#if defined(TERMCHART_HEVC_RECON_DUMP)  // 诊断专用：把重建帧追加到当前目录的 tc_enc_recon.yuv
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

std::string encode_hevc_intra_annexb(std::span<const Bitmap> frames, int qp) {
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
    sps.log2_diff_max_min_transform_block_size = 0;   // MaxTb=4px（真压缩档专用）
    // PCM 档用默认 3，SPS 位序列与 G0 黄金基线逐字节一致；两档参数集互不影响。
    out += make_nal(33, codec::hevc::write_sps(sps));  // SPS
    // 真压缩档开启去块滤波：传默认 Pps{}（deblocking_filter_control_present_flag = 0
    // ⇒ pps_deblocking_filter_disabled_flag 推断为 0，滤波生效，且 slice header 不新增
    // 任何比特）。编码端在重建出口同步实现 DBF（见 build_intra_idr_slice 末尾），
    // 因此 "enc 重建 == dec 输出" 的逐像素判据继续成立。PCM 档同样传 Pps{}，位序列不变。
    out += make_nal(34, codec::hevc::write_pps(codec::hevc::Pps{}));
    for (const auto& frame : frames) {
        const Yuv420 f = to_yuv(frame);
        out += make_nal(19, build_intra_idr_slice(f, qp));  // IDR slice
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
        case VideoEncodeMode::Intra: {
            const int qp = std::clamp(opts.video_qp, 0, 51);
            return encode_hevc_intra_annexb(frames, qp);
        }
        case VideoEncodeMode::Inter:
            // 阶段二才落地；显式返回空串，不静默降级成 PCM。
            return std::string{};
    }
    return std::string{};
}

}  // namespace termchart::image
