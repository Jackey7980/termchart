// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "codec/bitstream/bit_writer.hpp"
#include "codec/hevc/nal_unit.hpp"
#include "codec/hevc/parameter_sets.hpp"
#include "codec/hevc/slice_header.hpp"
#include "codec/hevc/cabac_engine.hpp"

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

// 编码树：log2size > 3（>8x8）写 split_cu_flag=1 并四等分；到 8x8 叶写 PCM 块。
// split_cu_flag 的上下文按 HEVC spec 9.3.4.2 取：CTB 根（log2size==6）恒为 ctxIdx 0
// （ctxInc=0）；其余深度 ctxInc = condL + condA。本码流是均匀四分树，每个非根 CU 的
// 左/上邻居必为同 CTB 内已切分的 CU，故 condL = condA = 1 → ctxIdx = 2（含 CTB 左/上
// 边界，规范规定 x0==xCtb 即 condL=1，不能因 px==0 算成 0）。
void code_cu(CabacEncoder& cabac, const Yuv420& f, int x0, int y0, int log2size,
             int ctb_x, int ctb_y) {
    const int size = 1 << log2size;
    if (log2size > 3) {  // MinCbLog2SizeY = 3（8x8）
        // split_cu_flag 的 ctxInc 严格按 HEVC 9.3.4.2.2 推导：
        //   · 仅当本 CU 是 CTB 根（64×64，log2size==6）时 ctxInc = condL + condA
        //     = (ctb_x>0) + (ctb_y>0)；左上角 CTB 即为 0。
        //   · 非根 CU 位于 CTB 上/左边界（x0==ctb_x && y0==ctb_y，即 CTB 内左上对角子块）
        //     时 ctxInc = 1（此时 condL=condA=0 → FFMIN(2,1+0+0)=1）。
        //   · 其余内部子块 ctxInc = 2（FFMIN(2,1+condL+condA)，condL/condA 至少一个为 1）。
        //   之前把「CTB 内左上对角子块（如 32×32/16×16 的 (0,0)）」也错当成根写成 ctx=0，
        //   导致自第一个子块起 CABAC 上下文状态与解码器（FFmpeg）错位，叶片 pcm_flag 被误读 → 全帧当 intra。
        // FFmpeg ff_hevc_split_coding_unit_flag_decode():
        //   int x0b = av_zero_extend(x0, log2_ctb_size);   // CTB 内偏移
        //   if (lc->ctb_left_flag || x0b) depth_left = tab_ct_depth[...x_cb-1];
        //   if (lc->ctb_up_flag   || y0b) depth_top  = tab_ct_depth[...y_cb-1];
        //   inc = (depth_left > ct_depth) + (depth_top > ct_depth)
        // 均匀四分树全拆到 8x8 叶子时，已编码叶子的 ct_depth 恒为 3，
        // 而待判 split 的 ct_depth 为 0/1/2，故只要「左侧有已编码 CU」即 inc+1。
        // 「左侧有已编码 CU」= 同 CTB 内 x0b>0 或存在左侧 CTB，两者合起来正是绝对坐标 x0>0；
        // 用 CTB 内坐标会在第 2 个及以后的 CTB 左/上边界漏掉邻居，导致右半帧全错。
        const int ctx = (x0 > 0 ? 1 : 0) + (y0 > 0 ? 1 : 0);
        cabac.encode_bin(ctx, 1);  // split_cu_flag
        const int half = size / 2;
        for (int i = 0; i < 4; ++i) {
            const int x1 = x0 + (i & 1) * half;
            const int y1 = y0 + (i >> 1) * half;
            code_cu(cabac, f, x1, y1, log2size - 1, ctb_x, ctb_y);
        }
        return;
    }
    // 8x8 叶（== MinCbLog2SizeY）：HEVC 7.3.8.5 coding_unit() 语法顺序要求先写 part_mode，
    // 当且仅当 part_mode == PART_2Nx2N 且满足 PCM 条件时才读 pcm_flag。8x8 恰好等于 MinCbLog2SizeY，
    // 故 part_mode 必须显式写出（不可省略）；只有 log2CbSize > MinCbLog2SizeY 时 PART_2Nx2N 才是
    // 唯一合法值、可由 split_cu_flag 隐式决定而省略。
    // part_mode 首 bin：FFmpeg ff_hevc_part_mode_decode() 的
    //   if (GET_CABAC(PART_MODE_OFFSET)) return PART_2Nx2N;
    // 即 bin "1" → PART_2Nx2N，只读 1 个 bin。PART_MODE_OFFSET = 13。
    cabac.encode_bin(13, 1);
    // pcm_flag 是【终止 bin】：FFmpeg ff_hevc_pcm_flag_decode() 直接返回
    // get_cabac_terminate(&lc->cc)，不占用任何上下文（PCM_FLAG 的 NUM_BINS=0）。
    cabac.encode_terminating(1);
    // 字节对齐（pcm_alignment_zero_bit）后写裸样本，落盘到 final_out。
    std::string raw; raw.reserve(96);
    const int cstride = f.cw / 2;
    for (int yy = 0; yy < 8; ++yy)  // luma 8x8
        for (int xx = 0; xx < 8; ++xx)
            raw.push_back(f.y[static_cast<size_t>((y0 + yy) * f.cw + (x0 + xx))]);
    for (int yy = 0; yy < 4; ++yy)  // Cb 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(f.u[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]);
    for (int yy = 0; yy < 4; ++yy)  // Cr 4x4
        for (int xx = 0; xx < 4; ++xx)
            raw.push_back(f.v[static_cast<size_t>(((y0 / 2) + yy) * cstride + ((x0 / 2) + xx))]);
    cabac.flush_cu(std::move(raw));  // 字节对齐 + 拼 [cabac][raw] 到 final_out
}

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
            if (!first_ctb) cabac.encode_terminating(0);
            first_ctb = false;
            code_cu(cabac, f, ctb_x, ctb_y, 6, ctb_x, ctb_y);
        }
    }
    cabac.encode_terminating(1);  // 最后一个 CTB 的 end_of_slice_segment_flag = 1
    rbsp += cabac.finish();
    return rbsp;
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
    out += make_nal(32, codec::hevc::write_vps(codec::hevc::Vps{}));  // VPS
    codec::hevc::Sps sps;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
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
        case VideoEncodeMode::Inter:
            // 阶段一/二才落地；显式返回空串，不静默降级成 PCM。
            return std::string{};
    }
    return std::string{};
}

}  // namespace termchart::image
