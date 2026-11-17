// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "codec/hevc/inter_coding.hpp"
#include "codec/hevc/residual_coding.hpp"
#include "codec/hevc/sao.hpp"
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

// P 片的跨 CU 状态（切片级）。三者都必须与解码端同步：
//   · ref     —— 上一帧的【重建帧】（= 解码端 DPB 里的参考帧，无 DBF/SAO 时逐像素相等）
//   · mvf     —— PU 级 MV 场，等价 FFmpeg tab_mvf；merge/AMVP 候选全部从它推导，
//                写错一格就会让后续 CU 的候选列表分叉（整片像素错但不报错）
//   · skip8   —— 8x8 粒度的 skip_flag 表，只用于 skip_flag 的 ctxInc
//                （cabac.c:587-600：inc = skip(left) + skip(up)）
struct PFrameState {
    const Yuv420* ref = nullptr;   // L0[0]：前一帧（POC-1）
    const Yuv420* ref1 = nullptr;  // P 档：L0[1]（POC-2）；B 档：L1[0]（后向参考）
    int num_refs = 1;              // L0 参考帧数（1 或 2）
    // B 片档专用（默认 0 ⇒ 所有既有 P 片行为与码流字节不变）
    bool b_slice = false;          // true ⇒ 本片是 B 片（slice_type=0）
    bool b_merge_only = true;      // B 片只用 merge（默认，逐像素门禁全绿）
    int l0_delta = -1;             // L0[0].POC - 当前 POC（P 档 -1；B 档 -4）
    int l1_delta = 2;              // L1[0].POC - 当前 POC（B 档 +2）
    int poc = 0;                   // 当前片 POC（跨列表 AMVP 缩放用）
    codec::hevc::MvFieldStore mvf;
    std::vector<std::uint8_t> skip8;
    int cw8 = 0;
    int max_merge_cand = codec::hevc::kMaxMergeCand;
    bool motion_search = true;
    bool subpel_me = false;  // 整数最优 MV 之后是否做 1/4 像素精搜
    bool trace = false;
    // 调试用：强制 CU 形态（0=正常，1=全 skip，2=全 AMVP(给定 MV)，3=全帧内）
    int force_mode = 0;
    codec::hevc::Mv force_mv{0, 0};
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

// HEVC 8.6.1 / FFmpeg cabac.c:1122-1163：4:2:0 色度 QP 由亮度 QP 经 qPi→qPc
// 映射得到（本编码器 PPS/slice 的 cb/cr_qp_offset 与 cu_qp_offset 均为 0，
// QpBdOffset=0，故 qp_i == qp_y）。亮度与色度反量化步长不同，不能共用 QP。
static int chroma_qp_from_luma(int qp_y) {
    if (qp_y < 30) return qp_y;
    if (qp_y > 43) return qp_y - 6;
    static const int kQpC[] = {29, 30, 31, 32, 33, 33, 34, 34,
                               35, 35, 36, 36, 37, 37};  // qp_i 30..43
    return kQpC[qp_y - 30];
}

// ── 自动 TU 档的内容自适应判据 ────────────────────────────────────────
// 逐 CU 节点比较「整块单 TU」与「分裂成 4 个子块」的真实代价
//   J = SSD(重建, 源) + λ · (残差 bin 数)
// bin 数由 encode_residual 实算，故周期纹理用大变换省 bits、分裂多出的 4 套
// 语法开销都能被正确权衡；λ 随量化噪声方差缩放，高 QP 天然更倾向大块。
// 这是逐节点贪心（非全树 RDO 搜索），但决策只影响语法划分，任意结果都是合法码流。
static double quant_noise_var(int qp) {
    const double qstep = std::pow(2.0, static_cast<double>(qp - 4) / 6.0);
    return qstep * qstep / 12.0;
}

static double auto_lambda(int qp) {
    // 低 QP 时量化噪声趋近 0，若 λ 也趋近 0，决策就只剩失真项、会一路分裂到
    // 最小块，码率反而暴涨（QP10 实测比最佳固定档差 47%）。故给 λ 一个下限，
    // 让「多写 4 套语法」的代价在任何 QP 下都被计入。
    // 验证注入点：环境变量可扫描系数/下限，便于用外部 oracle 标定默认值。
    static const double kScale = [] {
        const char* e = std::getenv("TERMCHART_HEVC_AUTO_LAMBDA");
        return (e && *e) ? std::atof(e) : 4.0;
    }();
    static const double kMin = [] {
        const char* e = std::getenv("TERMCHART_HEVC_AUTO_LMIN");
        return (e && *e) ? std::atof(e) : 12.0;
    }();
    return std::max(kMin, quant_noise_var(qp) * kScale);
}

// 模式 RDO 的 λ：不能直接复用 auto_lambda。后者是为「CU 切分 / transform_skip」
// 标定的，代价里已经含整块残差 bin；模式决策还要再叠一层模式语法 bin，且用
// bin 数近似真实 bits 会系统性高估码率（高概率 bin 远小于 1 bit）。实测直接
// 用 auto_lambda 会把 QP40 的 one8 压掉 9.5 dB。故单独给一个折减系数，
// 由外部 oracle 在「码率收益」与「最坏 PSNR 退化」之间标定。
static double mode_rdo_lambda(int qp) {
    static const double kScale = [] {
        const char* e = std::getenv("TERMCHART_HEVC_RDO_MODE_LAMBDA");
        return (e && *e) ? std::atof(e) : 0.15;
    }();
    return auto_lambda(qp) * kScale;
}

// 单个 n×n TU 的真实 RD 代价：预测 → 变换量化 → 重建 SSD + λ·(残差 bin 数)。
// bin 数由 encode_residual 真算（不是估值），故周期纹理用大变换压缩的收益、
// 以及分裂带来的多套语法开销都能被正确权衡。预测参考取当前已重建平面，
// 重建写局部缓冲，不污染真实重建帧。
static double eval_tu_cost(const std::uint8_t* src, int src_stride,
                           const std::uint8_t* rec, int rec_stride,
                           int x0, int y0, int pw, int ph,
                           int qp, int log2_size, double lambda,
                           int max_skip_log2) {
    const int n = 1 << log2_size;
    std::vector<std::uint8_t> pred(static_cast<std::size_t>(n * n));
    std::vector<std::uint8_t> local(static_cast<std::size_t>(n * n));
    std::vector<std::int16_t> level(static_cast<std::size_t>(n * n));
    // 模式必须用真实决策结果：只用少数代表模式会低估「小块挑到贴合纹理的
    // 角度模式」的收益，低 QP 下把渐变图误判成大块（实测码率多 55%）。
    const int mode = codec::hevc::choose_intra_luma_mode(
        src, src_stride, rec, rec_stride, x0, y0, pw, ph, qp, log2_size);
    codec::hevc::predict_intra_luma(mode, rec, rec_stride, x0, y0, log2_size, pw, ph,
                                    pred.data());
    // 残差 bin 数必须用发射层同一套扫描表：log2<4 时按帧内模式切 VERT/HORIZ，
    // 否则 dots 这类角度纹理上决策层会系统性估错 bin 数（旧代码恒 DIAG）。
    const auto scan_idx = (log2_size >= 4)
        ? codec::hevc::kScanDiag
        : codec::hevc::scan_idx_for_intra_mode(mode);
    const std::uint8_t* srcb = &src[static_cast<std::size_t>(y0) * src_stride + x0];
    // 决策必须与真正写码流时的选择一致：PPS 允许 transform_skip 时，emit_cu
    // 会在「常规变换」与「跳过变换」之间按重建 SSE 选优；这里也两路都算，
    // 并把 flag 的 1 个 bin 计入代价（cbf=1 时才会写，此处按保守估计）。
    auto eval_one = [&](bool skip) -> double {
        codec::hevc::encode_tu(srcb, src_stride, pred.data(), local.data(), n, level.data(),
                               qp, log2_size, /*use_dst=*/false, skip);
        long long ssd = 0;
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const int d = static_cast<int>(
                                  src[static_cast<std::size_t>(y0 + y) * src_stride + x0 + x]) -
                              static_cast<int>(local[static_cast<std::size_t>(y) * n + x]);
                ssd += static_cast<long long>(d) * d;
            }
        bool ok = true;
        const auto bins = codec::hevc::encode_residual(
            level.data(), log2_size, &ok, scan_idx, 0);
        if (!ok) return 1e18;
        return static_cast<double>(ssd) +
               lambda * (static_cast<double>(bins.size()) + (skip ? 1.0 : 0.0));
    };
    double best = eval_one(false);
    if (max_skip_log2 >= 2 && log2_size <= max_skip_log2)
        best = std::min(best, eval_one(true));
    return best;
}

// 返回值：log2_cb（停下用单 TU）、2（仅 8x8 CU，分裂成 4 个 4x4 TU）、
// -1（继续四叉分裂）。比较「整块单 TU」与「4 个子块各单 TU」的真实 RD 代价，
// 分裂侧另加 4 套 CU 头的近似开销（每套约 6 个 bin）。
// 分裂成 4 个子 CU 要多写 4 套 CU 头（part_mode / prev / mpm / chroma / cbf 等），
// 按约 6 bin/套折算；验证注入点便于用外部 oracle 标定。
static const double kHeadBins = [] {
    const char* e = std::getenv("TERMCHART_HEVC_AUTO_HEAD");
    return (e && *e) ? std::atof(e) : 24.0;
}();
// 贪心只比较一层：分裂侧的 4 个子块各自「单 TU」代价高估了它们继续细分
// 后的最优代价，会让决策停在大块（低 QP 渐变图实测差 55%）。折扣系数
// 近似子树的进一步优化空间。
static const double kSplitDiscount = [] {
    const char* e = std::getenv("TERMCHART_HEVC_AUTO_SPLIT");
    return (e && *e) ? std::atof(e) : 0.80;
}();

// 8x8 CU 分裂成 4 个 4x4 TU 的 RD 代价（与 emit_cu 的 4x4 路径同构：
// 统一 mode4 + DST + 可选 transform_skip）。抽出供贪心与两层 RDO 共用。
static double eval_split4_cost(const std::uint8_t* src, int src_stride,
                               const std::uint8_t* rec, int rec_stride,
                               int pw, int ph, int x0, int y0, int qp,
                               double lambda, int max_skip_log2) {
    const int mode4 = codec::hevc::choose_intra_luma_mode(
        src, src_stride, rec, rec_stride, x0, y0, pw, ph, qp, 2);
    const auto scan4 = codec::hevc::scan_idx_for_intra_mode(mode4);
    double body = 0.0;
    for (int i = 0; i < 4; ++i) {
        const int tx = x0 + ((i & 1) ? 4 : 0);
        const int ty = y0 + ((i & 2) ? 4 : 0);
        std::uint8_t pred[16];
        codec::hevc::predict_intra_luma_4x4(mode4, rec, rec_stride, tx, ty, pw, ph, pred);
        auto eval4 = [&](bool skip) -> double {
            std::uint8_t local[16];
            std::int16_t lvl[16];
            codec::hevc::encode_tu(
                &src[static_cast<std::size_t>(ty) * src_stride + tx], src_stride, pred,
                local, 4, lvl, qp, 2, /*use_dst=*/true, skip);
            long long ssd = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    const int d = static_cast<int>(
                                      src[static_cast<std::size_t>(ty + y) * src_stride +
                                          tx + x]) -
                                  static_cast<int>(local[y * 4 + x]);
                    ssd += static_cast<long long>(d) * d;
                }
            bool ok = true;
            const auto bins = codec::hevc::encode_residual(lvl, 2, &ok, scan4, 0);
            if (!ok) return 1e18;
            return static_cast<double>(ssd) +
                   lambda * (static_cast<double>(bins.size()) + (skip ? 1.0 : 0.0));
        };
        double best4 = eval4(false);
        if (!(best4 < 1e18)) return 1e18;
        if (max_skip_log2 >= 2) best4 = std::min(best4, eval4(true));
        body += best4;
    }
    return body;
}

static int auto_decide(const std::uint8_t* src, int src_stride,
                       const std::uint8_t* rec, int rec_stride,
                       int pw, int ph, int x0, int y0, int log2_cb, int qp,
                       int max_skip_log2) {
    // 64x64 是 CTB 而非可选 TU：residual_coding / transform 最大只支持 log2=5，
    // 根节点必须分裂，否则会把 TU 尺寸设成 6 直接越界。
    if (log2_cb > 5) return -1;
    const double lambda = auto_lambda(qp);
    const double stop = eval_tu_cost(src, src_stride, rec, rec_stride, x0, y0, pw, ph,
                                     qp, log2_cb, lambda, max_skip_log2);
    // 两层 RDO（分裂侧改用子节点自身最优代价）已实测并回退：它在 skip 档
    // 让 QP0/10/40 反超固定档，却把 6 形态总码率抬高 2.1%（15717→16049 B），
    // 且 QP26 毫无改善。QP26 剩下的 18 B 差距不是决策退化，而是自动档 SPS
    // 的固有开销：MaxTb=32 + depth=1 让每个 8x8 CU 都要写 split_transform_flag，
    // 一帧 128 个 CU ≈ 128 bin ≈ 16 B；固定 4x4 档隐式分裂、不写这些 bin。
    // 故保留既有贪心 + 折扣，并修正了决策层的扫描表口径（见 eval_tu_cost）。
    double body = 0.0;
    if (log2_cb == 3) {
        body = eval_split4_cost(src, src_stride, rec, rec_stride, pw, ph,
                                x0, y0, qp, lambda, max_skip_log2);
    } else {
        const int half = 1 << (log2_cb - 1);
        for (int i = 0; i < 4; ++i) {
            const int cx = x0 + ((i & 1) ? half : 0);
            const int cy = y0 + ((i >> 1) ? half : 0);
            body += eval_tu_cost(src, src_stride, rec, rec_stride, cx, cy, pw, ph,
                                 qp, log2_cb - 1, lambda, max_skip_log2);
        }
    }
    const double split = lambda * kHeadBins + kSplitDiscount * body;
    if (stop <= split) return log2_cb;
    return (log2_cb == 3) ? 2 : -1;
}

std::string build_slice_rbsp(const Yuv420& f, int slice_qp, int max_skip_log2,
                             int log2_tu, bool sao_enabled, bool rdo, bool deblock,
                             PFrameState* pf, int poc_lsb, int log2_poc,
                             Yuv420* rec_out = nullptr) {
    // max_skip_log2 = PPS 的 log2_max_transform_skip_block_size（0 = 关闭）。
    // <=2 只有 4x4 TU 带 flag；>=3 时更大 TU 也能带，但那是参数集替我们声明的，
    // 这里只需保证「不超过声明值」，否则解码端根本不读该 flag、bin 顺序全错。
    const bool transform_skip_enabled = (max_skip_log2 >= 2);
    codec::hevc::SliceHeader sh;
    // SPS 声明 SAO 后（7.3.6.1）slice header 必须紧跟 slice_type 写两个开关。
    sh.sample_adaptive_offset_enabled_flag = sao_enabled;
    sh.slice_sao_luma_flag = sao_enabled;
    sh.slice_sao_chroma_flag = sao_enabled;
    if (pf != nullptr) {
        // 非 IDR 的 P 片（nal_unit_type=1）。RPS 用 inline 形式（SPS 的
        // num_short_term_ref_pic_sets=0 → 解码端会读 slice 内联的那一套）。
        // 只参考前一帧：deltaPocS0 = -1 ⇒ delta_poc_s0_minus1 = 0。
        sh.nal_unit_type = 1;
        sh.slice_type = pf->b_slice ? 0 : 1;  // 0=B / 1=P
        sh.log2_max_pic_order_cnt_lsb = log2_poc;
        sh.slice_pic_order_cnt_lsb = poc_lsb;
        sh.short_term_ref_pic_set_inline = true;
        if (pf->b_slice) {
            // B 档 GOP=3：当前 POC=2k，L0=前一个 P/IDR（POC-4），L1=后一个 P（POC+2）。
            // 两个列表都只有 1 个活动参考，故 prediction_unit 不发 ref_idx_lx。
            auto& rps = sh.short_term_ref_pic_set;
            rps.num_negative_pics = 1;
            rps.delta_poc_s0_minus1[0] = -pf->l0_delta - 1;
            rps.used_by_curr_pic_s0_flag[0] = true;
            rps.num_positive_pics = 1;
            rps.delta_poc_s1_minus1[0] = pf->l1_delta - 1;
            rps.used_by_curr_pic_s1_flag[0] = true;
            sh.num_ref_idx_l0_default_active_minus1 = 0;
            // override=1 ⇒ l0/l1 计数都在 slice header 里显式写，PPS 默认值不参与。
            sh.num_ref_idx_active_override_flag = true;
            sh.num_ref_idx_l0_active_minus1 = 0;
            sh.num_ref_idx_l1_active_minus1 = 0;
            sh.mvd_l1_zero_flag = false;  // PRED_BI 时照常发 mvd_coding(L1)
        } else {
            const int nrefs = pf->num_refs;
            sh.short_term_ref_pic_set.num_negative_pics = nrefs;
            sh.short_term_ref_pic_set.num_positive_pics = 0;
            // 负参考按 (deltaPocS0Minus1 + 1) 递增排序：ref_idx=0 → POC-1（minus1=0），
            // ref_idx=1 → POC-2（minus1=1）。默认 l0_delta=-1 ⇒ minus1=0，字节不变。
            sh.short_term_ref_pic_set.delta_poc_s0_minus1[0] = -pf->l0_delta - 1;
            sh.short_term_ref_pic_set.used_by_curr_pic_s0_flag[0] = true;
            if (nrefs >= 2) {
                sh.short_term_ref_pic_set.delta_poc_s0_minus1[1] = 1;
                sh.short_term_ref_pic_set.used_by_curr_pic_s0_flag[1] = true;
            }
            sh.num_ref_idx_l0_default_active_minus1 = nrefs - 1;
            sh.num_ref_idx_active_override_flag = false;
        }
        sh.five_minus_max_num_merge_cand = 0;  // max_num_merge_cand = 5
    }
    // SliceQpY = 26 + init_qp_minus26(=0) + slice_qp_delta + QpBdOffsetY(=0, 8bit)。
    // 量化与 CABAC 上下文初始化都用 slice_qp，故 delta 必须同步写进码流，
    // 否则解码端按 26 反量化、上下文也按 26 初始化，重建必然发散。
    // 默认 26 ⇒ delta=0 ⇒ se(0)，与既有码流字节级一致。
    sh.slice_qp_delta = slice_qp - 26;
    std::string rbsp = codec::hevc::write_slice_header(sh);
    CabacArith cabac;
    cabac.init();
    CabacContext ctx;
    ctx.init_hevc(slice_qp, pf == nullptr ? codec::hevc::kInitTypeI
                   : (pf->b_slice ? codec::hevc::kInitTypeB : codec::hevc::kInitTypeP));

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

    // SAO 档的语法交织顺序（hevcdec.c:2795-2802）：每个 CTB 先读 sao()
    // （7.3.9.2）、再读 coding_quadtree。而 offset 的 RD 决策必须等整帧重建 + DBF
    // 完成（SAO 的滤波输入是 DBF 输出），故 SAO 档把每个 CTB 的编码 bin 先缓冲，
    // 决策结束后再按 CTB 交织（sao bins → 该 CTB 的编码 bins → terminate bin）。
    std::vector<std::vector<codec::hevc::Bin>> ctb_bins(
        sao_enabled ? static_cast<std::size_t>(total_ctb) : 0u);
    std::vector<codec::hevc::Bin> cur_bins;
    // 调试用：逐 bin 打印（ctx 为 kBypassCtx 时表示 bypass bin）。
    const bool dump_bins = std::getenv("TERMCHART_HEVC_DUMP_BINS") != nullptr;
    auto emit = [&](int ctx_id, int val) {
        if (dump_bins) std::fprintf(stderr, "B %d %d\n", ctx_id, val);
        if (sao_enabled) {
            cur_bins.push_back(codec::hevc::Bin{ctx_id, val});
        } else if (ctx_id == codec::hevc::kBypassCtx) {
            cabac.encode_bypass(val);
        } else {
            cabac.encode_decision(ctx.ctx[ctx_id], val);
        }
    };

    // 亮度帧内模式选择：默认逐 CU 走 SSD 模式决策（35 模式全覆盖，见
    // intra_prediction.cpp::choose_intra_luma_mode_8x8）；可用环境变量
    // TERMCHART_HEVC_FIXED_MODE 强制全帧固定模式，便于用 ffmpeg 解码做逐模式 oracle。
    const char* fixed_mode_env = std::getenv("TERMCHART_HEVC_FIXED_MODE");
    const int kIntraMode = (fixed_mode_env && *fixed_mode_env) ? std::atoi(fixed_mode_env) : -1;
    // 诊断专用：强制所有亮度 TU 选 transform_skip=1，用于端到端门禁验证「100% skip」
    // 码流仍能被 ffmpeg 逐像素正确解码。默认关闭，不影响任何生产码流。
    // 与 TERMCHART_HEVC_FIXED_MODE / TERMCHART_HEVC_NO_DBLOCK 同属验证注入点。
    const bool kForceSkip = std::getenv("TERMCHART_HEVC_FORCE_SKIP") != nullptr;
    // MPM 候选表由左/上 PU 的【实际模式】决定，且 CTB 首行强制 cand_up=DC，
    // 故不能固定 mpm_idx：维护 tab_ipm 并按 FFmpeg luma_intra_pred_mode 推导。
    const std::size_t cw8 = static_cast<std::size_t>(cw / 8);
    std::vector<std::uint8_t> tab_ipm(
        cw8 * static_cast<std::size_t>(f.ch / 8), 1);  // INTRA_DC

    // CU 边长 = 1<<log2_cb；log2_tu<=3 时 CU 恒为 8x8，>=4 时 CU 与 TU 同尺寸。
    const bool auto_tu = (log2_tu == 0);
    // 自动档同一帧混用 4/8/16/32 TU，去块滤波不能用统一边界周期，必须在逐 CU
    // 发射时记下每个 8x8 网格的左/上边界是否真是 TU 边界（8.7.2：只对 TU/PU
    // 边界滤波）。固定档走 DeblockConfig::edge_period，不填这两张表。
    std::vector<std::uint8_t> vert_edge, horz_edge;
    if (auto_tu) {
        const std::size_t cells = cw8 * static_cast<std::size_t>(f.ch / 8);
        vert_edge.assign(cells, 0);
        horz_edge.assign(cells, 0);
    }
    // RDO 档（EncodeOptions::hevc_rdo）：模式决策从纯 SSD 换成 SSD + λ·bins。
    // 默认 rd_lambda=0，代价逐位退化成原来的纯 SSD，默认码流字节级不变。
    const double rd_lambda =
        (rdo && kIntraMode < 0) ? mode_rdo_lambda(slice_qp) : 0.0;
    auto emit_cu = [&](int x0, int y0, int log2_cb, int tu_log2, int split4) {
        // MPM 候选表只依赖左/上 PU 的已选模式与坐标，不依赖本 CU 的模式，
        // 故可以在模式决策【之前】算好并传进去，让 RDO 档把「模式语法要写多少
        // bin」也计入代价（非 MPM 模式要多 3~4 个 bypass bin）。
        const std::size_t pu = static_cast<std::size_t>(y0 >> 3) * cw8 +
                               static_cast<std::size_t>(x0 >> 3);
        // y0 落在 CTB 首行时 (y0-1) 跨垂直 CTB 边界 ⇒ FFmpeg 取 DC；x0==0
        // 时左邻居不可用 ⇒ DC。详见下方 MPM 段的规范引用。
        const int cand_up = (y0 % 64 != 0) ? static_cast<int>(tab_ipm[pu - cw8]) : 1;
        const int cand_left = (x0 > 0) ? static_cast<int>(tab_ipm[pu - 1]) : 1;
        const auto cands = codec::hevc::derive_mpm_candidates(cand_left, cand_up);
        const int cells = 1 << (log2_cb - 3);
        if (auto_tu) {
            const int cb = 1 << log2_cb;
            const int tu = 1 << tu_log2;
            for (int dy = 0; dy < cb; dy += 8)
                for (int dx = 0; dx < cb; dx += 8) {
                    const int gx = (x0 + dx) / 8;
                    const int gy = (y0 + dy) / 8;
                    const std::size_t gi = static_cast<std::size_t>(gy) * cw8 + gx;
                    if ((dx % tu) == 0) vert_edge[gi] = 1;  // 该格左边界是 TU 边界
                    if ((dy % tu) == 0) horz_edge[gi] = 1;  // 该格上边界是 TU 边界
                }
        }
        // ── P 片：skip / AMVP(无残差) / 帧内 三选一 ──────────────────────
        // 合法组合只有三种（hevcdec.c hls_coding_unit:2477-2600）：
        //   a) skip_flag=1 → merge_idx                          （P_Skip）
        //   b) skip_flag=0, pred_mode=0(INTER), part_mode, merge_flag=0,
        //      [ref_idx：单参考不读], mvd_coding, mvp_lx_flag, rqt_root_cbf=0
        //   c) skip_flag=0, pred_mode=1(INTRA) → 接标准帧内语法
        // 注意 pred_mode_flag：0=INTER、1=INTRA（HEVC 8.3.1 / ffmpeg 枚举同）。
        // 不用「merge_flag=1 + 2Nx2N」：该组合解码端【不读】rqt_root_cbf 并推断
        // 为 1，后面必须跟 transform_tree（至少 6 个 cbf bin），比 skip 还贵。
        int p_skip_inc = 0;
        bool p_intra = false;
        if (pf != nullptr) {
            // skip_flag 的 ctxInc（cabac.c:587-600）：左/上 8x8 的 skip_flag 之和。
            p_skip_inc = (x0 > 0 ? static_cast<int>(pf->skip8[pu - 1]) : 0) +
                         (y0 > 0 ? static_cast<int>(pf->skip8[pu - cw8]) : 0);
            if (pf->b_slice) {
                // 先清零本 CU 的 skip_flag 上下文项：帧内分支也必须为 0，否则
                // 沿用上帧遗留值会让后续 CU 的 skip_flag ctxInc 与解码端分叉。
                pf->skip8[pu] = 0;
                // ── B 片：merge（skip）/ L0 AMVP / L1 AMVP / BI AMVP / 帧内 ──
                // 两个列表各只有一个活动参考（ref_idx 恒 0），故不发 ref_idx bins。
                // 运动信息与 FFmpeg tab_mvf 同构：mvf 里同时存 L0/L1 分量（BI 的邻居
                // 才能进 combined bi-pred 与 L1 AMVP）。
                const codec::hevc::CuGeom gb{x0, y0, 8, 8};
                const codec::hevc::PicGeom pb{cw, f.ch, 6};
                const codec::hevc::CuGeom gchb{x0 / 2, y0 / 2, 4, 4};

                std::uint8_t by[64] = {0}, bu[16] = {0}, bv[16] = {0};
                auto load_ref = [&](const Yuv420* rp, codec::hevc::Mv mv) {
                    codec::hevc::mc_luma(rp->y.data(), cw, f.ch, gb, mv, by, 8);
                    codec::hevc::mc_chroma(rp->u.data(), cw / 2, f.ch / 2, gchb, mv, bu, 4);
                    codec::hevc::mc_chroma(rp->v.data(), cw / 2, f.ch / 2, gchb, mv, bv, 4);
                };
                auto load_bi = [&](codec::hevc::Mv m0, codec::hevc::Mv m1) {
                    codec::hevc::mc_bi(pf->ref->y.data(), pf->ref1->y.data(), cw, f.ch,
                                       gb, m0, m1, 2, by, 8);
                    codec::hevc::mc_bi(pf->ref->u.data(), pf->ref1->u.data(),
                                       cw / 2, f.ch / 2, gchb, m0, m1, 3, bu, 4);
                    codec::hevc::mc_bi(pf->ref->v.data(), pf->ref1->v.data(),
                                       cw / 2, f.ch / 2, gchb, m0, m1, 3, bv, 4);
                };
                auto apply_pred = [&](const codec::hevc::PuField& fld) {
                    if (fld.pred == codec::hevc::kPredBi) load_bi(fld.mv, fld.mv_l1);
                    else if (fld.pred == codec::hevc::kPredL1) load_ref(pf->ref1, fld.mv_l1);
                    else load_ref(pf->ref, fld.mv);
                };
                auto ssd_luma_b = [&]() {
                    std::int64_t s = 0;
                    for (int yy = 0; yy < 8; ++yy) {
                        const std::uint8_t* srow =
                            &f.y[static_cast<std::size_t>(y0 + yy) * cw + x0];
                        for (int xx = 0; xx < 8; ++xx) {
                            const int d = static_cast<int>(srow[xx]) - by[yy * 8 + xx];
                            s += static_cast<std::int64_t>(d) * d;
                        }
                    }
                    return s;
                };
                auto ssd_all_b = [&]() {
                    std::int64_t s = ssd_luma_b();
                    for (int c = 0; c < 2; ++c) {
                        const std::uint8_t* src = (c ? f.v : f.u).data();
                        const std::uint8_t* pr = c ? bv : bu;
                        for (int yy = 0; yy < 4; ++yy) {
                            const std::uint8_t* srow =
                                &src[static_cast<std::size_t>(y0 / 2 + yy) * (cw / 2) + x0 / 2];
                            for (int xx = 0; xx < 4; ++xx) {
                                const int d = static_cast<int>(srow[xx]) - pr[yy * 4 + xx];
                                s += static_cast<std::int64_t>(d) * d;
                            }
                        }
                    }
                    return s;
                };

                codec::hevc::PuField mrg[codec::hevc::kMaxMergeCand] = {};
                const int nb_mrg = codec::hevc::derive_merge_candidates_b(
                    pf->mvf, gb, pb, pf->max_merge_cand, 1, 1, mrg);
                double best_cost = 1e18;
                int best_kind = 4;  // 0=skip, 1=L0, 2=L1, 3=BI, 4=intra
                int best_merge = 0;
                codec::hevc::PuField best_field;
                codec::hevc::Mv mv0{0, 0}, mv1{0, 0}, mvd0{0, 0}, mvd1{0, 0};
                int mvp0 = 0, mvp1 = 0;
                const double lam_b = auto_lambda(slice_qp);
                for (int k = 0; k < nb_mrg; ++k) {
                    apply_pred(mrg[k]);
                    const double c = static_cast<double>(ssd_all_b()) + lam_b * (2.0 + k);
                    if (c < best_cost) {
                        best_cost = c;
                        best_kind = 0;
                        best_merge = k;
                        best_field = mrg[k];
                    }
                }

                auto search_list = [&](const Yuv420* rp, int list, codec::hevc::Mv& omv,
                                       codec::hevc::Mv& omvd, int& omvp) {
                    codec::hevc::Mv cand[2] = {};
                    int nc = pf->b_slice
                                 ? codec::hevc::derive_amvp_candidates_b_ex(
                                       pf->mvf, gb, pb, list, 0, pf->poc,
                                       pf->poc + pf->l0_delta, pf->poc + pf->l1_delta, cand)
                                 : codec::hevc::derive_amvp_candidates_b(
                                       pf->mvf, gb, pb, list, 0, cand);
                    if (nc == 0) { cand[0] = codec::hevc::Mv{0, 0}; nc = 1; }
                    codec::hevc::Mv me{0, 0};
                    std::int64_t best_sad = (static_cast<std::int64_t>(1) << 62);
                    for (int dy = -8; dy <= 8; ++dy) {
                        for (int dx = -8; dx <= 8; ++dx) {
                            const codec::hevc::Mv mv{dx * codec::hevc::kMvStep,
                                                     dy * codec::hevc::kMvStep};
                            const int px = x0 + mv.x / 4, pyy = y0 + mv.y / 4;
                            if (px < 0 || pyy < 0 || px > cw - 8 || pyy > f.ch - 8) continue;
                            load_ref(rp, mv);
                            const std::int64_t sad = ssd_luma_b();
                            if (sad < best_sad) { best_sad = sad; me = mv; }
                        }
                    }
                    int jbest = 0;
                    std::int64_t best_mvd = (static_cast<std::int64_t>(1) << 62);
                    for (int j = 0; j < nc; ++j) {
                        const std::int64_t d =
                            std::llabs(static_cast<long long>(me.x - cand[j].x)) +
                            std::llabs(static_cast<long long>(me.y - cand[j].y));
                        if (d < best_mvd) { best_mvd = d; jbest = j; }
                    }
                    omv = me;
                    omvd = codec::hevc::Mv{me.x - cand[jbest].x, me.y - cand[jbest].y};
                    omvp = jbest;
                };
                search_list(pf->ref, 0, mv0, mvd0, mvp0);
                search_list(pf->ref1, 1, mv1, mvd1, mvp1);
                auto mvd_bits = [](const codec::hevc::Mv& m) {
                    return (std::abs(m.x) + std::abs(m.y)) / 4.0;
                };

                load_ref(pf->ref, mv0);
                double c0 = static_cast<double>(ssd_all_b()) + lam_b * (9.0 + mvd_bits(mvd0));
                if (c0 < best_cost) {
                    best_cost = c0; best_kind = 1; best_field = {};
                    best_field.pred = codec::hevc::kPredL0;
                    best_field.mv = mv0;
                }
                load_ref(pf->ref1, mv1);
                double c1 = static_cast<double>(ssd_all_b()) + lam_b * (9.0 + mvd_bits(mvd1));
                if (c1 < best_cost) {
                    best_cost = c1; best_kind = 2; best_field = {};
                    best_field.pred = codec::hevc::kPredL1;
                    best_field.mv_l1 = mv1;
                }
                load_bi(mv0, mv1);
                double cbi = static_cast<double>(ssd_all_b()) +
                             lam_b * (13.0 + mvd_bits(mvd0) + mvd_bits(mvd1));
                if (cbi < best_cost) {
                    best_cost = cbi; best_kind = 3; best_field = {};
                    best_field.pred = codec::hevc::kPredBi;
                    best_field.mv = mv0;
                    best_field.mv_l1 = mv1;
                }
                {
                    const double qstep = std::pow(2.0, (slice_qp - 4) / 6.0);
                    const double intra_cost = 96.0 * qstep * qstep / 12.0 + lam_b * 48.0;
                    if (!(best_cost < intra_cost)) best_kind = 4;
                }

                // 诊断注入：强制 B 片所有 inter CU 走同一种模式，便于把 bin 级
                // 分叉定位到 merge / L0 / L1 / BI 某一条语法路径（生产默认关闭）。
                if (pf->b_merge_only) {
                    best_kind = 0;
                    best_merge = 0;
                    best_field = mrg[0];
                }
                if (const char* force_b = std::getenv("TERMCHART_HEVC_B_FORCE")) {
                    if (!std::strcmp(force_b, "skip") || !std::strncmp(force_b, "skip", 4)) {
                        int mi = force_b[4] ? std::atoi(force_b + 4) : 0;
                        if (force_b[4] && (mi < 0 || mi >= nb_mrg)) mi = 0;
                        best_kind = 0; best_merge = mi; best_field = mrg[mi];
                    } else if (!std::strcmp(force_b, "l0") || !std::strcmp(force_b, "l1") ||
                               !std::strcmp(force_b, "bi")) {
                        best_kind = !std::strcmp(force_b, "l0") ? 1
                                  : (!std::strcmp(force_b, "l1") ? 2 : 3);
                        best_field = {};
                        best_field.pred = best_kind == 1 ? codec::hevc::kPredL0
                                        : (best_kind == 2 ? codec::hevc::kPredL1
                                                          : codec::hevc::kPredBi);
                        best_field.mv = mv0;
                        best_field.mv_l1 = mv1;
                    } else if (!std::strcmp(force_b, "intra")) {
                        best_kind = 4;
                    }
                }
                if (pf->trace) {
                    std::fprintf(stderr,
                                 "B %3d,%3d kind=%d nbM=%d mrg=%d pred=%d idc=%d "
                                 "mv=(%d,%d) mv1=(%d,%d) mvd=(%d,%d) mvd1=(%d,%d) "
                                 "mvp=%d,%d\n",
                                 x0, y0, best_kind, nb_mrg, best_merge,
                                 static_cast<int>(best_field.pred),
                                 best_kind == 1 ? 0 : (best_kind == 2 ? 1 : (best_kind == 3 ? 2 : -1)),
                                 best_field.mv.x, best_field.mv.y, best_field.mv_l1.x,
                                 best_field.mv_l1.y, mvd0.x, mvd0.y, mvd1.x, mvd1.y,
                                 mvp0, mvp1);
                }
                if (best_kind == 4) {
                    p_intra = true;  // 交回下方标准帧内路径
                } else {
                    codec::hevc::CuInterB8x8 cu;
                    cu.skip_ctx_inc = p_skip_inc;
                    cu.ct_depth = 6 - log2_cb;  // 64→8 的 CU 深度 = 3
                    cu.max_merge_cand = pf->max_merge_cand;
                    if (best_kind == 0) {
                        cu.skip = true;
                        cu.merge_idx = best_merge;
                    } else {
                        cu.skip = false;
                        cu.merge = false;
                        cu.inter_pred_idc = best_kind == 1 ? 0 : (best_kind == 2 ? 1 : 2);
                        cu.mvd_x = mvd0.x;
                        cu.mvd_y = mvd0.y;
                        cu.mvd_x_l1 = mvd1.x;
                        cu.mvd_y_l1 = mvd1.y;
                        cu.mvp_lx_flag = mvp0;
                        cu.mvp_lx_flag_l1 = mvp1;
                        cu.emit_rqt_root_cbf = true;
                        cu.rqt_root_cbf = 0;  // AMVP 无残差，省掉整棵 transform_tree
                    }
                    const auto bins = codec::hevc::encode_cu_inter_b_8x8(cu);
                    if (bins.empty()) return;
                    for (const auto& b : bins) emit(b.ctx, b.val);
                    pf->skip8[pu] = cu.skip ? 1 : 0;
                    pf->mvf.set_block(x0, y0, 8, 8, best_field);
                    apply_pred(best_field);
                    for (int yy = 0; yy < 8; ++yy)
                        for (int xx = 0; xx < 8; ++xx)
                            rec.y[static_cast<std::size_t>(y0 + yy) * cw + x0 + xx] =
                                by[yy * 8 + xx];
                    for (int c = 0; c < 2; ++c) {
                        std::uint8_t* dst = (c ? rec.v : rec.u).data();
                        const std::uint8_t* pr = c ? bv : bu;
                        for (int yy = 0; yy < 4; ++yy)
                            for (int xx = 0; xx < 4; ++xx)
                                dst[static_cast<std::size_t>(y0 / 2 + yy) * (cw / 2) + x0 / 2 + xx] =
                                    pr[yy * 4 + xx];
                    }
                    return;
                }
                // best_kind==4 时落到下方帧内路径，p_intra 保证前缀是 B 片的 skip=0
                // + pred_mode=1（B 片与 P 片这两个 bin 的 ctx 完全相同）。
            }
            const codec::hevc::CuGeom g{x0, y0, 8, 8};
            const codec::hevc::PicGeom pg{cw, f.ch, 6};
            // 多参考档用带 ref_idx 的候选；单参考档沿用旧版（保字节级基线）。
            codec::hevc::PuField merge_field[codec::hevc::kMaxMergeCand];
            codec::hevc::Mv merge_cand[codec::hevc::kMaxMergeCand];
            int nb_merge = 0;
            if (pf->num_refs >= 2) {
                nb_merge = codec::hevc::derive_merge_candidates_ex(
                    pf->mvf, g, pg, pf->max_merge_cand, merge_field);
                for (int k = 0; k < nb_merge; ++k) merge_cand[k] = merge_field[k].mv;
            } else {
                nb_merge = codec::hevc::derive_merge_candidates(
                    pf->mvf, g, pg, pf->max_merge_cand, merge_cand);
            }
            codec::hevc::Mv amvp[2];
            int nb_amvp = codec::hevc::derive_amvp_candidates(pf->mvf, g, pg, amvp);
            const double lam = auto_lambda(slice_qp);
            const int cs = cw / 2;
            const codec::hevc::CuGeom gch{x0 / 2, y0 / 2, 4, 4};

            std::uint8_t py[64] = {0}, pu8[16] = {0}, pv8[16] = {0};
            // load 按 ref_idx 选参考帧：0 → 前一帧，1 → 前两帧（多参考档）。
            auto load = [&](codec::hevc::Mv mv, int ref_idx) {
                const Yuv420* rp = (ref_idx == 1 && pf->ref1) ? pf->ref1 : pf->ref;
                codec::hevc::mc_luma(rp->y.data(), cw, f.ch, g, mv, py, 8);
                codec::hevc::mc_chroma(rp->u.data(), cw / 2, f.ch / 2, gch, mv, pu8, 4);
                codec::hevc::mc_chroma(rp->v.data(), cw / 2, f.ch / 2, gch, mv, pv8, 4);
            };
            auto ssd_luma = [&]() {
                std::int64_t s = 0;
                for (int yy = 0; yy < 8; ++yy) {
                    const std::uint8_t* srow =
                        &f.y[static_cast<std::size_t>(y0 + yy) * cw + x0];
                    for (int xx = 0; xx < 8; ++xx) {
                        const int d = static_cast<int>(srow[xx]) -
                                      static_cast<int>(py[yy * 8 + xx]);
                        s += static_cast<std::int64_t>(d) * d;
                    }
                }
                return s;
            };
            auto ssd_all = [&]() {
                std::int64_t s = ssd_luma();
                for (int c = 0; c < 2; ++c) {
                    const std::uint8_t* src = (c ? f.v : f.u).data();
                    const std::uint8_t* pr = c ? pv8 : pu8;
                    for (int yy = 0; yy < 4; ++yy) {
                        const std::uint8_t* srow =
                            &src[static_cast<std::size_t>(y0 / 2 + yy) * cs + x0 / 2];
                        for (int xx = 0; xx < 4; ++xx) {
                            const int d = static_cast<int>(srow[xx]) -
                                          static_cast<int>(pr[yy * 4 + xx]);
                            s += static_cast<std::int64_t>(d) * d;
                        }
                    }
                }
                return s;
            };

            // 候选 a：P_Skip（MV 必须取自 merge 表）。代价 = SSD + λ·(skip+idx)。
            double best_cost = 1e18;
            int best_kind = 0;  // 0 = skip，1 = AMVP，2 = 帧内
            int best_merge = 0;
            int best_mvp_flag = 0;
            int best_ref = 0;
            codec::hevc::Mv best_mv{0, 0};
            codec::hevc::Mv best_mvd{0, 0};
            for (int k = 0; k < nb_merge; ++k) {
                const int r = (pf->num_refs >= 2) ? merge_field[k].ref_idx : 0;
                load(merge_cand[k], r);
                const double c = static_cast<double>(ssd_all()) + lam * (2.0 + k);
                if (c < best_cost) {
                    best_cost = c;
                    best_kind = 0;
                    best_merge = k;
                    best_mv = merge_cand[k];
                    best_ref = r;
                }
            }

            // 候选 b：整数位置运动搜索（kMvStep=2 亮度采样/步，±16 像素），再经
            // AMVP 拆成 mvp_lx_flag + MVD 送出。多参考档对 L0 的每个参考帧各搜一遍，
            // 取全局最优 (MV, ref_idx)；AMVP 候选只取同 ref_idx 的邻居（无 POC 缩放）。
            if (pf->motion_search) {
                const int span = 8;
                for (int ref = 0; ref < pf->num_refs; ++ref) {
                    codec::hevc::Mv amvp_r[2];
                    int nb_amvp_r = 0;
                    if (pf->num_refs >= 2) {
                        nb_amvp_r = codec::hevc::derive_amvp_candidates_ex(
                            pf->mvf, g, pg, ref, amvp_r);
                    } else {
                        nb_amvp_r = nb_amvp;
                        for (int j = 0; j < nb_amvp; ++j) amvp_r[j] = amvp[j];
                    }
                    // AMVP 列表为空（例如该 ref 还没有同参考邻居）时，解码端候选
                    // 默认为 (0,0)，编码端必须照样搜索——否则 ref=1 永不参与，
                    // 退化成单参考空壳。
                    if (nb_amvp_r == 0) {
                        amvp_r[0] = codec::hevc::Mv{0, 0};
                        nb_amvp_r = 1;
                    }

                    codec::hevc::Mv me{0, 0};
                    std::int64_t best_sad = (static_cast<std::int64_t>(1) << 62);
                    for (int dy = -span; dy <= span; ++dy) {
                        for (int dx = -span; dx <= span; ++dx) {
                            const codec::hevc::Mv mv{dx * codec::hevc::kMvStep,
                                                     dy * codec::hevc::kMvStep};
                            const int px = x0 + mv.x / 4, pyy = y0 + mv.y / 4;
                            if (px < 0 || pyy < 0 || px > cw - 8 || pyy > f.ch - 8) continue;
                            load(mv, ref);
                            const std::int64_t sad = ssd_luma();
                            if (sad < best_sad) {
                                best_sad = sad;
                                me = mv;
                            }
                        }
                    }
                    // 亚像素精搜（可选）：在整数最优 me 附近 ±4 个 1/4 单位做全搜索。
                    if (pf->subpel_me) {
                        codec::hevc::Mv sub = me;
                        std::int64_t sub_sad = best_sad;
                        for (int dy = -4; dy <= 4; ++dy) {
                            for (int dx = -4; dx <= 4; ++dx) {
                                const codec::hevc::Mv mv{me.x + dx, me.y + dy};
                                load(mv, ref);
                                const std::int64_t sad = ssd_all();
                                if (sad < sub_sad) {
                                    sub_sad = sad;
                                    sub = mv;
                                }
                            }
                        }
                        me = sub;
                        best_sad = sub_sad;
                    }
                    int j_best = 0;
                    std::int64_t mvd_best = (static_cast<std::int64_t>(1) << 62);
                    for (int j = 0; j < nb_amvp_r; ++j) {
                        const std::int64_t ax =
                            std::llabs(static_cast<long long>(me.x - amvp_r[j].x));
                        const std::int64_t ay =
                            std::llabs(static_cast<long long>(me.y - amvp_r[j].y));
                        if (ax + ay < mvd_best) {
                            mvd_best = ax + ay;
                            j_best = j;
                        }
                    }
                    const codec::hevc::Mv mvd{me.x - amvp_r[j_best].x, me.y - amvp_r[j_best].y};
                    load(me, ref);
                    // 粗估 bin 数：8 个固定元素 + MVD 分量码价 + 多参考时 ref_idx 1 bin。
                    const double bits = 8.0 + (pf->num_refs >= 2 ? 1.0 : 0.0) +
                                        static_cast<double>(std::abs(mvd.x) +
                                                            std::abs(mvd.y)) / 4.0;
                    const double c = static_cast<double>(ssd_all()) + lam * bits;
                    if (c < best_cost) {
                        best_cost = c;
                        best_kind = 1;
                        best_mv = me;
                        best_mvd = mvd;
                        best_mvp_flag = j_best;
                        best_ref = ref;
                    }
                }
            }

            // 候选 c：帧内。用「量化失真下界 + 语法开销」作基准，inter 拼不过就转帧内。
            {
                const double qstep = std::pow(2.0, (slice_qp - 4) / 6.0);
                const double intra_cost = 96.0 * qstep * qstep / 12.0 + lam * 48.0;
                if (!(best_cost < intra_cost)) best_kind = 2;
            }

            pf->skip8[pu] = 0;
            if (pf->force_mode == 1) {
                best_kind = 0;
                best_merge = 0;
                best_mv = merge_cand[0];
            } else if (pf->force_mode == 2) {
                const codec::hevc::Mv pred = nb_amvp > 0 ? amvp[0] : codec::hevc::Mv{0, 0};
                best_kind = 1;
                best_mvp_flag = 0;
                best_mv = pf->force_mv;
                best_mvd = codec::hevc::Mv{best_mv.x - pred.x, best_mv.y - pred.y};
            } else if (pf->force_mode == 3) {
                best_kind = 2;
            } else if (pf->force_mode == 4) {
                // 调试：AMVP 全用首选预测子、MVD 恒 0 —— 只验证 AMVP 列表本身
                // 是否与解码端一致（此时解码端的 MV 就等于它自己算出的候选 0）。
                best_kind = 1;
                best_mvp_flag = 0;
                best_mv = nb_amvp > 0 ? amvp[0] : codec::hevc::Mv{0, 0};
                best_mvd = codec::hevc::Mv{0, 0};
            }
            if (pf->trace) {
                std::fprintf(stderr,
                             "P %3d,%3d kind=%d nbM=%d nbA=%d mrg=%d ref=%d mv=(%d,%d) "
                             "mvd=(%d,%d) mvp=%d amvp0=(%d,%d) amvp1=(%d,%d)\n",
                             x0, y0, best_kind, nb_merge, nb_amvp, best_merge,
                             (best_kind == 0 && pf->num_refs >= 2) ? merge_field[best_merge].ref_idx
                                                                   : best_ref,
                             best_mv.x, best_mv.y, best_mvd.x, best_mvd.y, best_mvp_flag,
                             amvp[0].x, amvp[0].y, nb_amvp > 1 ? amvp[1].x : 0,
                             nb_amvp > 1 ? amvp[1].y : 0);
            }
            if (best_kind == 2) {
                // 帧内：前缀 skip_flag=0 + pred_mode_flag=1(INTRA) 由下方
                // encode_cu_intra_in_p_8x8 统一发射（此处只置标志，避免重复写 bin）。
                p_intra = true;
            } else {
                // skip 模式的 ref_idx 来自 merge 候选（不单独发）；AMVP 模式用 best_ref。
                const int emit_ref = (best_kind == 0 && pf->num_refs >= 2)
                                         ? merge_field[best_merge].ref_idx
                                         : best_ref;
                pf->mvf.set_block(x0, y0, 8, 8,
                                  codec::hevc::PuField{codec::hevc::kPredL0,
                                                       static_cast<std::int8_t>(emit_ref), best_mv});
                if (best_kind == 0) {
                    pf->skip8[pu] = 1;
                    emit(codec::hevc::kCtxSkipFlag + p_skip_inc, 1);
                    // skip_flag=1 ⇒ 直接进 prediction_unit：只剩 merge_idx（ref_idx 随候选）。
                    for (const auto& b : codec::hevc::merge_idx_bins(best_merge, pf->max_merge_cand))
                        emit(b.ctx, b.val);
                } else {
                    emit(codec::hevc::kCtxSkipFlag + p_skip_inc, 0);
                    emit(codec::hevc::kCtxPredMode, 0);           // pred_mode_flag=0 ⇒ MODE_INTER（=1 是 INTRA）
                    emit(codec::hevc::kCtxPartMode, 1);           // PART_2Nx2N
                    emit(codec::hevc::kCtxMergeFlag, 0);
                    // 多参考：ref_idx_l0 在 mvd 之前（HEVC prediction_unit 顺序）。
                    // nb_refs=2 ⇒ put_ref_idx_l0 只发 1 个 ctx bin：idx0→0, idx1→1。
                    if (pf->num_refs >= 2)
                        emit(codec::hevc::kCtxRefIdxL0, best_ref > 0 ? 1 : 0);
                    for (const auto& b : codec::hevc::mvd_coding_bins(best_mvd.x, best_mvd.y))
                        emit(b.ctx, b.val);
                    emit(codec::hevc::kCtxMvpLxFlag, best_mvp_flag);
                    emit(codec::hevc::kCtxNoResidualData, 0);     // rqt_root_cbf = 0
                }
                // 重建 = 参考块拷贝/插值（与解码端 put_uni_pixels 一致）。
                load(best_mv, emit_ref);
                for (int yy = 0; yy < 8; ++yy)
                    for (int xx = 0; xx < 8; ++xx)
                        rec.y[static_cast<std::size_t>(y0 + yy) * cw + x0 + xx] =
                            py[yy * 8 + xx];
                for (int c = 0; c < 2; ++c) {
                    std::uint8_t* dst = (c ? rec.v : rec.u).data();
                    const std::uint8_t* pr = c ? pv8 : pu8;
                    for (int yy = 0; yy < 4; ++yy)
                        for (int xx = 0; xx < 4; ++xx)
                            dst[static_cast<std::size_t>(y0 / 2 + yy) * cs + x0 / 2 + xx] =
                                pr[yy * 4 + xx];
                }
                return;
            }
        }

        // 本编码器 8x8 CU 用 PART_2Nx2N ⇒ 整 8x8 单 PU 预测（ffmpeg 解码端同样按
        // 8x8 预测）。故先用选定模式算整块预测，再拆成 4 个 4x4 TU 写残差。
        const int mode = (kIntraMode >= 0)
                             ? kIntraMode
                             : codec::hevc::choose_intra_luma_mode(
                                   f.y.data(), cw, rec.y.data(), cw,
                                   x0, y0, cw, f.ch, slice_qp, tu_log2,
                                   rd_lambda, &cands);
        // 4 个 z 序 4x4 TU：顺序必须与解码端 hls_transform_unit 一致。HEVC 帧内预测块
        // 尺寸等于 TU 尺寸，故每个 4x4 TU 用本 CU 单一模式、以其自身 (tx,ty) 定位参考样本。
        // 系数扫描方式由本 CU 的亮度模式隐式决定（HEVC 8.5.3.1 / FFmpeg
        // hevcdec.c:1396-1403）：模式 6..14 用 SCAN_VERT、22..30 用 SCAN_HORIZ，
        // 其余用 SCAN_DIAG。三张扫描表与 sig_coeff ctx 表必须成套切换，否则
        // 解码端会把系放到错误的 (x,y)，整帧重建分叉。
        // log2_trafo_size>=4 时 FFmpeg 不再按帧内模式推导扫描，恒为 DIAG。
        const auto scan_idx = (tu_log2 >= 4) ? codec::hevc::kScanDiag
                                             : codec::hevc::scan_idx_for_intra_mode(mode);
        const auto& st = codec::hevc::scan4x4_tables(scan_idx);
        // CBF 的约束式 RD：发射层按「量化 level 是否全零」自动写 cbf=0/1
        // （cu_coding.cpp），故判定胜出时只需清零 level 并把编码端重建回滚成
        // 预测值。回滚必须紧跟本 TU —— 后续 TU/CU 的帧内预测会读这些样本。
        const double rdo_slack = codec::hevc::rdo_distortion_slack();
        // CBF 对着整块残差语法，与 TU 层 / transform_skip 同一口径，用未折减的
        // auto_lambda；模式 RD 因额外叠了模式语法 bin 才用 mode_rdo_lambda 折减。
        const double cbf_lambda = auto_lambda(slice_qp);
        auto try_zero_cbf = [&](const std::uint8_t* sbuf, int sstride,
                                std::uint8_t* rbuf, int rstride, int bx, int by,
                                int n, const std::uint8_t* pred, std::int16_t* lvl,
                                int lg, bool scan_ordered, int cidx,
                                bool has_skip_bin) -> bool {
            std::int64_t ssd0 = 0, ssd1 = 0;
            for (int yy = 0; yy < n; ++yy) {
                const std::size_t sy = static_cast<std::size_t>(by + yy) *
                                       static_cast<std::size_t>(sstride);
                const std::size_t ry = static_cast<std::size_t>(by + yy) *
                                       static_cast<std::size_t>(rstride);
                const std::size_t bxu = static_cast<std::size_t>(bx);
                for (int xx = 0; xx < n; ++xx) {
                    const std::size_t idx = bxu + static_cast<std::size_t>(xx);
                    const int s = sbuf[sy + idx];
                    const int d0 = s - pred[yy * n + xx];
                    const int d1 = s - rbuf[ry + idx];
                    ssd0 += static_cast<std::int64_t>(d0) * d0;
                    ssd1 += static_cast<std::int64_t>(d1) * d1;
                }
            }
            // 与模式 RD 共用的画质护栏：只允许失真余量内的候选。
            if (static_cast<double>(ssd0) > static_cast<double>(ssd1) * (1.0 + rdo_slack))
                return false;
            bool ok = true;
            const std::size_t rbins =
                scan_ordered
                    ? codec::hevc::encode_residual_4x4(lvl, &ok, scan_idx, cidx).size()
                    : codec::hevc::encode_residual(lvl, lg, &ok, scan_idx, cidx).size();
            if (!ok || rbins == 0) return false;  // 本来就是 cbf=0
            const double j0 = static_cast<double>(ssd0) + cbf_lambda;  // 只写 cbf bin
            const double j1 = static_cast<double>(ssd1) +
                              cbf_lambda * (1.0 + static_cast<double>(rbins) +
                                           (has_skip_bin ? 1.0 : 0.0));
            if (j0 >= j1) return false;
            for (int k = 0; k < n * n; ++k) lvl[k] = 0;
            for (int yy = 0; yy < n; ++yy) {
                const std::size_t ry = static_cast<std::size_t>(by + yy) *
                                       static_cast<std::size_t>(rstride);
                for (int xx = 0; xx < n; ++xx)
                    rbuf[ry + static_cast<std::size_t>(bx) +
                         static_cast<std::size_t>(xx)] = pred[yy * n + xx];
            }
            return true;
        };
        std::int16_t scan[4][16] = {};
        std::int16_t large_level[1024] = {};
        int ts_luma[4] = {0, 0, 0, 0};
        // transform_skip 的取舍必须按 RD（SSD + λ·bin）而不是纯 SSE。skip 不做变换，
        // 残差在像素域直接量化：平滑块上 SSE 可能更低，但系数完全不紧凑、bin 数暴涨。
        // 实测纯 SSE 判据会让 16x16 档在 QP10 码率翻倍（2635→6284 B）。
        // level 传行主序即可（encode_residual 内部按 scan_idx 重排；lg>3 恒 DIAG）。
        const double ts_lambda = auto_lambda(slice_qp);
        // λ 必须按【该平面自己的 QP】取：色度走 qPi→qPc 映射后 QP 更高，
        // 用亮度 λ 会低估色度残差的 bin 代价，在 QP40 误选 skip、把 U/V PSNR
        // 压到 23.7 dB（实测）。故亮度传 ts_lambda，色度传 auto_lambda(cqp)。
        auto tu_rd = [&](const std::uint8_t* base, int stride, int px, int py,
                        const std::uint8_t* rec_buf, int rec_stride,
                        const std::int16_t* lvl, int lg, bool skip, int cidx,
                        double rd_lambda) -> double {
            const int n = 1 << lg;
            std::int64_t ssd = 0;
            for (int yy = 0; yy < n; ++yy)
                for (int xx = 0; xx < n; ++xx) {
                    const std::int64_t d =
                        static_cast<std::int64_t>(
                            base[static_cast<std::size_t>(py + yy) * stride + px + xx]) -
                        static_cast<std::int64_t>(
                            rec_buf[static_cast<std::size_t>(yy) * rec_stride + xx]);
                    ssd += d * d;
                }
            bool ok = true;
            const auto bins = codec::hevc::encode_residual(lvl, lg, &ok, scan_idx, cidx);
            if (!ok) return 1e18;
            return static_cast<double>(ssd) +
                   rd_lambda * (static_cast<double>(bins.size()) + (skip ? 1.0 : 0.0));
        };
        if (tu_log2 >= 3) {
            // 8/16/32 TU：整块一次 n×n 帧内预测（8x8 含 1-2-1 参考平滑）+ 变换，
            // 系数【行主序】交给多 CG 残差语法。PPS 声明 log2_max_transform_skip
            // >= tu_log2 时，两路（常规变换 / 跳过变换）都试，按重建 SSE 选优；
            // 两路都用紧凑缓冲试算，绝不污染 rec（后续 TU 的预测参考它）。
            const int n = 1 << tu_log2;
            std::uint8_t pred[1024];
            codec::hevc::predict_intra_luma(mode, rec.y.data(), cw, x0, y0, tu_log2,
                                            cw, f.ch, pred);
            const std::uint8_t* srcb = &f.y[static_cast<std::size_t>(y0) * cw + x0];
            std::uint8_t* recb = &rec.y[static_cast<std::size_t>(y0) * cw + x0];
            const bool skip_ok = transform_skip_enabled && tu_log2 <= max_skip_log2;
            if (!skip_ok) {
                // 默认路径逐字保留：关闭/未声明时码流与既有基线字节级一致。
                codec::hevc::encode_tu(srcb, cw, pred, recb, cw, large_level, slice_qp,
                                       tu_log2, /*use_dst=*/false, /*transform_skip=*/false);
            } else {
                std::uint8_t r0[1024], r1[1024];
                std::int16_t l0[1024], l1[1024];
                codec::hevc::encode_tu(srcb, cw, pred, r0, n, l0, slice_qp, tu_log2,
                                       /*use_dst=*/false, /*transform_skip=*/false);
                codec::hevc::encode_tu(srcb, cw, pred, r1, n, l1, slice_qp, tu_log2,
                                       /*use_dst=*/false, /*transform_skip=*/true);
                const bool use_skip = kForceSkip ||
                    (tu_rd(srcb, cw, 0, 0, r1, n, l1, tu_log2, /*skip=*/true, 0,
                           ts_lambda) <
                     tu_rd(srcb, cw, 0, 0, r0, n, l0, tu_log2, /*skip=*/false, 0,
                           ts_lambda));
                const std::uint8_t* chosen_rec = use_skip ? r1 : r0;
                const std::int16_t* chosen_lvl = use_skip ? l1 : l0;
                ts_luma[0] = use_skip ? 1 : 0;
                for (int yy = 0; yy < n; ++yy)
                    for (int xx = 0; xx < n; ++xx)
                        recb[static_cast<std::size_t>(yy) * cw + xx] =
                            chosen_rec[yy * n + xx];
                for (int k = 0; k < n * n; ++k) large_level[k] = chosen_lvl[k];
            }
        } else for (int i = 0; i < 4; ++i) {
            const int tx = x0 + ((i & 1) ? 4 : 0);
            const int ty = y0 + ((i & 2) ? 4 : 0);
            std::uint8_t pred[16];
            codec::hevc::predict_intra_luma_4x4(mode, rec.y.data(), cw, tx, ty, cw, f.ch, pred);
            std::int16_t raster[16];
            if (!transform_skip_enabled) {
                // 默认路径逐字保留：码流与既有基线字节级一致。
                codec::hevc::encode_tu_4x4(
                    &f.y[static_cast<std::size_t>(ty) * cw + tx], cw, pred,
                    &rec.y[static_cast<std::size_t>(ty) * cw + tx], cw, raster, slice_qp,
                    /*use_dst=*/true);
            } else {
                // transform_skip：逐 TU 在「DST + 常规量化」与「跳过变换 + 两段式
                // 逆量化」之间按 RD 选优。两路都必须用紧凑 4x4 临时块试算，
                // 不能污染 rec（后续 TU 的帧内预测参考它）。
                std::uint8_t src4[16], r0[16], r1[16];
                for (int yy = 0; yy < 4; ++yy)
                    for (int xx = 0; xx < 4; ++xx)
                        src4[yy * 4 + xx] = f.y[static_cast<std::size_t>(ty + yy) * cw + tx + xx];
                std::int16_t l0[16], l1[16];
                codec::hevc::encode_tu(src4, 4, pred, r0, 4, l0, slice_qp, 2,
                                       /*use_dst=*/true, /*transform_skip=*/false);
                codec::hevc::encode_tu(src4, 4, pred, r1, 4, l1, slice_qp, 2,
                                       /*use_dst=*/true, /*transform_skip=*/true);
                // tu_rd 的 SSD 用 src4（连续 4x4），故传 stride=4、偏移 0。
                const bool use_skip = kForceSkip ||
                    (tu_rd(src4, 4, 0, 0, r1, 4, l1, 2, /*skip=*/true, 0, ts_lambda) <
                     tu_rd(src4, 4, 0, 0, r0, 4, l0, 2, /*skip=*/false, 0, ts_lambda));
                const std::uint8_t* chosen_rec = use_skip ? r1 : r0;
                const std::int16_t* chosen_lvl = use_skip ? l1 : l0;
                ts_luma[i] = use_skip ? 1 : 0;
                for (int yy = 0; yy < 4; ++yy)
                    for (int xx = 0; xx < 4; ++xx)
                        rec.y[static_cast<std::size_t>(ty + yy) * cw + tx + xx] =
                            chosen_rec[yy * 4 + xx];
                for (int k = 0; k < 16; ++k) raster[k] = chosen_lvl[k];
            }
            // 变换输出是行主序（idx = 行*4 + 列），残差语法要的是所选扫描序。
            for (int n = 0; n < 16; ++n)
                scan[i][n] = raster[st.y[n] * 4 + st.x[n]];
            if (rdo) {
                // 必须在【本 TU 写完、下一个 TU 预测之前】决策。若清成 cbf=0，
                // 下一个 TU 的左/上邻居读到的是 pred 而不是残差重建；放在 CU
                // 末尾统一清零会让块内参考分叉（实测 slack=0.25 时 neqY>0）。
                try_zero_cbf(f.y.data(), cw, rec.y.data(), cw, tx, ty, 4, pred,
                             scan[i], 2, true, 0, transform_skip_enabled);
            }
        }

        // 色度：8x8 亮度 CU → 4x4 U + 4x4 V（4:2:0）。DM 模式下色度预测模式
        // 等于亮度模式；FFmpeg hevcdec.c:1484-1506 在 blk_idx==3 处仍传【亮度】
        // 坐标 xBase/yBase 做邻居可用性判定，POS 则按 >>1 采样色度重建平面。
        // 色度变换固定 DCT-II（use_dst=false）、QP 走 qPi→qPc、c_idx>0 不做
        // DC 首行/首列滤波 —— 三处差异由 predict_intra_luma_4x4 的 chroma 参数处理。
        const int cstride = cw / 2;
        const int cqp = chroma_qp_from_luma(slice_qp);
        const double chroma_lambda = auto_lambda(cqp);
        const int cx = x0 / 2, cy = y0 / 2;
        const std::size_t coff = static_cast<std::size_t>(cy) * cstride + cx;
        std::int16_t cscan[2][16] = {};
        std::int16_t chroma_large[2][256] = {};
        int chroma_cbf[2] = {0, 0};
        int chroma_ts[2] = {0, 0};
        for (int c = 0; c < 2; ++c) {
            const std::uint8_t* csrc = (c ? f.v : f.u).data();
            std::uint8_t* crec = (c ? rec.v : rec.u).data();
            if (tu_log2 >= 4) {
                // 色度 TU = 亮度的一半：8x8/16x16，预测/变换/残差都按大 TU 走。
                // 与亮度同构地按 RD 选 skip，但必须用【本块坐标 (cx,cy)】算 SSD、
                // 用【色度 QP】取 λ：早期版本把 SSD 的 (px,py) 写成 (0,0)，
                // 读的是整个色度平面左上角，决策失真使 QP40 的 U/V PSNR 掉到
                // 23.7 dB（当时误判成「色度不该 skip」而整体回退）。
                const int clg = tu_log2 - 1;
                const int cn = 1 << clg;
                std::uint8_t pred[256];
                codec::hevc::predict_intra_luma(
                    mode, rec.y.data(), cw, x0, y0, clg, cw, f.ch, pred,
                    codec::hevc::ChromaPredPlane{crec, cstride});
                if (transform_skip_enabled && clg <= max_skip_log2) {
                    std::uint8_t r0[256], r1[256];
                    std::int16_t l0[256], l1[256];
                    codec::hevc::encode_tu(csrc + coff, cstride, pred, r0, cn, l0, cqp, clg,
                                           /*use_dst=*/false, /*transform_skip=*/false);
                    codec::hevc::encode_tu(csrc + coff, cstride, pred, r1, cn, l1, cqp, clg,
                                           /*use_dst=*/false, /*transform_skip=*/true);
                    const bool use_skip = kForceSkip ||
                        (tu_rd(csrc, cstride, cx, cy, r1, cn, l1, clg, /*skip=*/true,
                               c + 1, chroma_lambda) <
                         tu_rd(csrc, cstride, cx, cy, r0, cn, l0, clg, /*skip=*/false,
                               c + 1, chroma_lambda));
                    const std::uint8_t* chosen_rec = use_skip ? r1 : r0;
                    const std::int16_t* chosen_lvl = use_skip ? l1 : l0;
                    chroma_ts[c] = use_skip ? 1 : 0;
                    for (int yy = 0; yy < cn; ++yy)
                        for (int xx = 0; xx < cn; ++xx)
                            crec[static_cast<std::size_t>(cy + yy) * cstride + cx + xx] =
                                chosen_rec[yy * cn + xx];
                    for (int k = 0; k < cn * cn; ++k)
                        chroma_large[c][k] = chosen_lvl[k];
                } else {
                    codec::hevc::encode_tu(csrc + coff, cstride, pred, crec + coff,
                                           cstride, chroma_large[c], cqp, clg,
                                           /*use_dst=*/false, /*transform_skip=*/false);
                }
                for (int k = 0; k < cn * cn; ++k)
                    if (chroma_large[c][k] != 0) chroma_cbf[c] = 1;
            } else {
                std::uint8_t pred[16];
                codec::hevc::predict_intra_luma_4x4(
                    mode, rec.y.data(), cw, x0, y0, cw, f.ch, pred,
                    codec::hevc::ChromaPredPlane{crec, cstride});
                std::int16_t raster[16];
                if (transform_skip_enabled) {
                    // 4x4 色度同样给 skip 一个候选；紧凑 4x4 源用于 SSD 对拍。
                    std::uint8_t src4[16], r0[16], r1[16];
                    std::int16_t l0[16], l1[16];
                    for (int yy = 0; yy < 4; ++yy)
                        for (int xx = 0; xx < 4; ++xx)
                            src4[yy * 4 + xx] =
                                csrc[static_cast<std::size_t>(cy + yy) * cstride + cx + xx];
                    codec::hevc::encode_tu(src4, 4, pred, r0, 4, l0, cqp, 2,
                                           /*use_dst=*/false, /*transform_skip=*/false);
                    codec::hevc::encode_tu(src4, 4, pred, r1, 4, l1, cqp, 2,
                                           /*use_dst=*/false, /*transform_skip=*/true);
                    const bool use_skip = kForceSkip ||
                        (tu_rd(src4, 4, 0, 0, r1, 4, l1, 2, /*skip=*/true, c + 1,
                               chroma_lambda) <
                         tu_rd(src4, 4, 0, 0, r0, 4, l0, 2, /*skip=*/false, c + 1,
                               chroma_lambda));
                    const std::uint8_t* chosen_rec = use_skip ? r1 : r0;
                    const std::int16_t* chosen_lvl = use_skip ? l1 : l0;
                    chroma_ts[c] = use_skip ? 1 : 0;
                    for (int yy = 0; yy < 4; ++yy)
                        for (int xx = 0; xx < 4; ++xx)
                            crec[static_cast<std::size_t>(cy + yy) * cstride + cx + xx] =
                                chosen_rec[yy * 4 + xx];
                    for (int k = 0; k < 16; ++k) raster[k] = chosen_lvl[k];
                } else {
                    codec::hevc::encode_tu_4x4(csrc + coff, cstride, pred, crec + coff,
                                                cstride, raster, cqp, /*use_dst=*/false);
                }
                for (int n = 0; n < 16; ++n) {
                    cscan[c][n] = raster[st.y[n] * 4 + st.x[n]];
                    if (cscan[c][n] != 0) chroma_cbf[c] = 1;
                }
            }
        }

        // ── CBF 的约束式 RD（阶段三 P3-3）：大 TU 亮度 + 色度 ─────────────
        // 4x4 亮度已在逐 TU 循环里就地决策（块内预测参考会随 cbf 变化，必须
        // 紧跟本 TU），这里只处理单 TU 路径：本 CU 内没有后续 TU 依赖，回滚
        // 重建后跨 CU 的预测自然读到回滚值。单 TU 预测只读块外邻居，故在
        // 此处重算与原编码时逐位相同。
        if (rdo) {
            std::uint8_t pred_large[1024] = {};
            std::uint8_t cpred4[2][16] = {};
            std::uint8_t cpred_large[2][256] = {};
            if (tu_log2 >= 3)
                codec::hevc::predict_intra_luma(mode, rec.y.data(), cw, x0, y0, tu_log2,
                                                cw, f.ch, pred_large);
            for (int c = 0; c < 2; ++c) {
                std::uint8_t* crec = (c ? rec.v : rec.u).data();
                const auto chroma = codec::hevc::ChromaPredPlane{crec, cstride};
                if (tu_log2 >= 4) {
                    codec::hevc::predict_intra_luma(mode, rec.y.data(), cw, x0, y0,
                                                    tu_log2 - 1, cw, f.ch,
                                                    cpred_large[c], chroma);
                } else {
                    codec::hevc::predict_intra_luma_4x4(mode, rec.y.data(), cw, x0, y0,
                                                        cw, f.ch, cpred4[c], chroma);
                }
            }
            if (tu_log2 >= 3) {
                const int n = 1 << tu_log2;
                try_zero_cbf(f.y.data(), cw, rec.y.data(), cw, x0, y0, n, pred_large,
                             large_level, tu_log2, false, 0, transform_skip_enabled);
            }
            for (int c = 0; c < 2; ++c) {
                const std::uint8_t* csrc = (c ? f.v : f.u).data();
                std::uint8_t* crec = (c ? rec.v : rec.u).data();
                const bool large_chroma = (tu_log2 >= 4);
                const int cn = large_chroma ? (1 << (tu_log2 - 1)) : 4;
                std::int16_t* lvl = large_chroma ? chroma_large[c] : cscan[c];
                if (try_zero_cbf(csrc, cstride, crec, cstride, cx, cy, cn,
                                 large_chroma ? cpred_large[c] : cpred4[c], lvl,
                                 large_chroma ? tu_log2 - 1 : 2, !large_chroma, c + 1,
                                 transform_skip_enabled))
                    chroma_cbf[c] = 0;
            }
        }

        // pu / cand_up / cand_left / cands / cells 已在 emit_cu 开头算好：MPM 推导
        // 逐式对齐 FFmpeg luma_intra_pred_mode（CTB 首行取 DC、x0>0 左邻居可用，
        // 见 hevcdec.c:2264-2267 / mvs.c:49-50），且必须早于模式决策才能进 RD 代价。
        const auto sig = codec::hevc::signal_intra_mode(mode, cands);
        // 大 CU 覆盖多个 8x8 PU 格，后续 CU 的 MPM 左/上邻居可能落在其中任意一格。
        for (int dy = 0; dy < cells; ++dy)
            for (int dx = 0; dx < cells; ++dx)
                tab_ipm[(pu + static_cast<std::size_t>(dy) * cw8) + dx] =
                    static_cast<std::uint8_t>(mode);

        CuIntra8x8 cu;
        cu.part_2nx2n = true;
        cu.scan_idx = scan_idx;
        cu.prev_intra_luma_pred_flag = sig.prev_intra_luma_pred_flag;
        cu.mpm_idx = sig.mpm_idx;
        cu.rem_intra_luma_pred_mode = sig.rem_intra_luma_pred_mode;
        cu.intra_chroma_pred_mode = 4;        // DM（色度跟随亮度模式）
        cu.cbf_cb = chroma_cbf[0];
        cu.cbf_cr = chroma_cbf[1];
        for (int i = 0; i < 4; ++i) cu.luma4x4[i] = scan[i];
        cu.chroma4x4[0] = cscan[0];
        cu.chroma4x4[1] = cscan[1];
        // PPS 打开 transform_skip 后，每个 cbf=1 的 TU 都要写 flag（解码端按
        // pps.enabled && log2_trafo_size <= log2_max_transform_skip_block_size 朗读）。
        // 亮度与色度都按各自平面 QP 的 RD 决策（色度在 4x4 与大 TU 两条路径
        // 都参与；flag=1 的语法由 --force-skip 端到端门禁逐像素验证）。
        cu.transform_skip_enabled = transform_skip_enabled;
        for (int i = 0; i < 4; ++i) cu.transform_skip_luma[i] = ts_luma[i];
        cu.transform_skip_chroma[0] = chroma_ts[0];
        cu.transform_skip_chroma[1] = chroma_ts[1];
        cu.log2_tu_size = tu_log2;
        cu.luma_large = large_level;
        cu.chroma_large[0] = chroma_large[0];
        cu.chroma_large[1] = chroma_large[1];
        cu.emit_part_mode = (log2_cb == 3);
        cu.emit_split_transform_flag = auto_tu;
        cu.split_transform_flag = (split4 != 0);
        cu.log2_root = log2_cb;
        // P 片里的帧内 CU 需要先写 skip_flag=0 + pred_mode_flag=1(INTRA)，故走带前缀的
        // 变体；I 片走原路径。两者后续 bin 完全一致。
        const auto bins = p_intra
                              ? codec::hevc::encode_cu_intra_in_p_8x8(cu, p_skip_inc)
                              : codec::hevc::encode_cu_intra_8x8(cu);
        for (const auto& b : bins) emit(b.ctx, b.val);
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

    // TU<=8 时叶子仍是 8x8 CU；TU=16/32 时叶子与 TU 同尺寸，树上少分裂 1/2 层。
    const int leaf_log2_cb = auto_tu ? 3 : ((log2_tu <= 3) ? 3 : log2_tu);
    std::function<void(int, int, int)> code_tree = [&](int x0, int y0, int depth) {
        const int log2_cb = 6 - depth;
        bool stop = (log2_cb <= leaf_log2_cb);
        int chosen_tu = 0;   // 该 CU 实际使用的 TU 尺寸 log2
        int split4 = 0;      // 8x8 CU 是否分裂成 4 个 4x4 TU
        if (auto_tu) {
            // 自动档：逐节点比较「整块单 TU」与「4 个子块」的真实 SSD + λ·bins。
            const int d = auto_decide(f.y.data(), cw, rec.y.data(), cw, cw, f.ch,
                                      x0, y0, log2_cb, slice_qp, max_skip_log2);
            stop = (d >= 2);
            if (stop) {
                chosen_tu = d;
                split4 = (d == 2) ? 1 : 0;
            }
        } else if (stop) {
            chosen_tu = (log2_cb == 3 && log2_tu == 2) ? 2 : log2_cb;
        }
        // 解码端只在 log2_cb_size > MinCbLog2SizeY(3) 时读 split_cu_flag；
        // 因此 16/32 叶子仍要显式写 0，否则后续 CU 的语法全部错位。
        if (log2_cb > 3) {
            int inc = 0;
            if (x0 > 0 && ct_depth[static_cast<std::size_t>(y0 >> 3) * cw8 + (x0 >> 3) - 1] > depth)
                ++inc;
            if (y0 > 0 && ct_depth[static_cast<std::size_t>((y0 >> 3) - 1) * cw8 + (x0 >> 3)] > depth)
                ++inc;
            emit(kCtxSplitCuFlag + inc, stop ? 0 : 1);
            if (!stop) {
                const int half = (1 << log2_cb) / 2;
                for (int i = 0; i < 4; ++i)
                    code_tree(x0 + ((i & 1) * half), y0 + ((i >> 1) * half), depth + 1);
                return;
            }
        }
        emit_cu(x0, y0, log2_cb, chosen_tu, split4);
        const int cells = 1 << (log2_cb - 3);
        for (int dy = 0; dy < cells; ++dy)
            for (int dx = 0; dx < cells; ++dx)
                ct_depth[(static_cast<std::size_t>(y0 >> 3) + dy) * cw8 + (x0 >> 3) + dx] =
                    static_cast<std::uint8_t>(depth);
    };

    for (int ctb_y = 0; ctb_y < f.ch; ctb_y += 64) {
        for (int ctb_x = 0; ctb_x < f.cw; ctb_x += 64) {
            code_tree(ctb_x, ctb_y, 0);
            if (sao_enabled) {
                // terminate bin 也推迟：它必须排在该 CTB 的 sao bins 之后。
                ctb_bins[static_cast<std::size_t>(ctb_idx)].swap(cur_bins);
                cur_bins.clear();
            } else {
                cabac.encode_terminate(ctb_idx == total_ctb - 1 ? 1 : 0);
            }
            ++ctb_idx;
        }
    }
    // 重建出口同步去块滤波（8.7.2）。必须放在整帧编码之后：帧内预测参考的是
    // **滤波前**重建，逐 CU 滤波会污染后续 CU 的参考样本。
    // 2026-11-13 精确化：亮度强/弱滤波与 tc 双段早已逐式对齐 FFmpeg（4x4/8x8 TU
    // 档 Y 平面 maxdiff=0）；补齐的是 ① 色度 8.7.2.5 弱滤波 ② 固定大 TU 档的
    // 边界周期（16/32 TU 内部不存在 TU 边界，不该逐条 8 对齐边都滤）。
    // bS 恒 2 对全帧内码流是规范结论，不是近似。自动档混合 TU 尺寸仍需逐 CU 边界图。
    // 两端都关 DBF 的门禁路径（TERMCHART_HEVC_NO_DBLOCK=1 + -skip_loop_filter all）
    // 保持不变；开 DBF 的逐像素一致性由新增 --dblock-on 门禁覆盖。
    // P 片的 bS 推导（8.7.2.4）依赖每个边界的 intra/inter 类型与残差、MV 差异，
    // 与本文件「bS 恒 2」的帧内假设不同；inter 档在 PPS 里置
    // pps_deblocking_filter_disabled_flag=1，编码端同步跳过 DBF，保证两端一致。
    if (deblock) {
        const int edge_period = auto_tu ? 8 : std::max(8, 1 << log2_tu);
        const codec::hevc::DeblockConfig dbf{
            .slice_qp = slice_qp,
            .beta_offset_div2 = 0,
            .tc_offset_div2 = 0,
            .chroma_qp = chroma_qp_from_luma(slice_qp),
            .edge_period = edge_period,
            .vert_edge = auto_tu ? vert_edge.data() : nullptr,
            .horz_edge = auto_tu ? horz_edge.data() : nullptr,
            .grid_w = static_cast<int>(cw8),
        };
        codec::hevc::deblock_frame_luma(rec.y.data(), cw, f.ch, cw, dbf);
        codec::hevc::deblock_frame_chroma(rec.u.data(), rec.v.data(),
                                          cw / 2, f.ch / 2, cw / 2, dbf);
    }

    // SAO（8.7.3）：in-loop 顺序是 DBF → SAO，故 SAO 的输入必须是上面滤波后的
    // 重建；每个 CTB 的 offset 用率失真（真实滤波 SSE + λ·bin）决策，相邻 CTB
    // 参数一致时改走 merge（只写 1 个 flag）。
    codec::hevc::SaoTable sao;
    codec::hevc::SaoFrameInput sao_in;
    if (sao_enabled) {
        sao_in.width = cw;
        sao_in.height = f.ch;
        sao_in.src[0] = f.y.data();
        sao_in.src[1] = f.u.data();
        sao_in.src[2] = f.v.data();
        sao_in.rec[0] = rec.y.data();
        sao_in.rec[1] = rec.u.data();
        sao_in.rec[2] = rec.v.data();
        sao_in.stride[0] = cw;
        sao_in.stride[1] = cw / 2;
        sao_in.stride[2] = cw / 2;
        codec::hevc::compute_sao_table(sao_in, auto_lambda(slice_qp),
                                       auto_lambda(chroma_qp_from_luma(slice_qp)), &sao);
    }

    // 按 CTB 交织写回：sao() 在前、coding_quadtree 在后，最后是 terminate bin。
    rbsp += [&] {
        if (!sao_enabled) return cabac.finish();
        std::vector<codec::hevc::Bin> sao_bins;
        for (int k = 0; k < total_ctb; ++k) {
            const int cx = k % ctb_w;
            const int cy = k / ctb_w;
            sao_bins.clear();
            codec::hevc::put_sao_params(sao_bins, sao.params[static_cast<std::size_t>(k)],
                                        sao.slice_luma, sao.slice_chroma, cx > 0, cy > 0,
                                        sao.merge_left[static_cast<std::size_t>(k)] != 0,
                                        sao.merge_up[static_cast<std::size_t>(k)] != 0);
            for (const auto& b : sao_bins) {
                if (b.ctx == codec::hevc::kBypassCtx) cabac.encode_bypass(b.val);
                else cabac.encode_decision(ctx.ctx[b.ctx], b.val);
            }
            for (const auto& b : ctb_bins[static_cast<std::size_t>(k)]) {
                if (b.ctx == codec::hevc::kBypassCtx) cabac.encode_bypass(b.val);
                else cabac.encode_decision(ctx.ctx[b.ctx], b.val);
            }
            cabac.encode_terminate(k == total_ctb - 1 ? 1 : 0);
        }
        ctb_bins.clear();
        return cabac.finish();
    }();

    if (sao_enabled) {
        // 滤波输入与输出必须是两块内存（同块会被已滤像素污染），故先拷一份输入。
        Yuv420 sao_src = rec;
        std::uint8_t* dst[3] = {rec.y.data(), rec.u.data(), rec.v.data()};
        const int dst_stride[3] = {cw, cw / 2, cw / 2};
        codec::hevc::SaoFrameInput fin = sao_in;
        fin.rec[0] = sao_src.y.data();
        fin.rec[1] = sao_src.u.data();
        fin.rec[2] = sao_src.v.data();
        codec::hevc::apply_sao_table(fin, sao, dst, dst_stride);
    }
#if defined(TERMCHART_HEVC_RECON_DUMP)  // 诊断专用：重建帧追加到指定前缀（或当前目录）
    {
        // TERMCHART_HEVC_RECON_PATH=<前缀> 时写 <前缀>.yuv + <前缀>.chroma.yuv，
        // 让多码流并行的验证脚本各自落盘；不设置则保持既有固定文件名
        //（verify_modes_recon.py 依赖它）。
        const char* prefix = std::getenv("TERMCHART_HEVC_RECON_PATH");
        const std::string yname = prefix ? std::string(prefix) + ".yuv"
                                         : std::string("tc_enc_recon.yuv");
        const std::string cname = prefix ? std::string(prefix) + ".chroma.yuv"
                                         : std::string("tc_enc_chroma.yuv");
        std::FILE* fp = std::fopen(yname.c_str(), "ab");
        if (fp) {
            std::fwrite(rec.y.data(), 1, rec.y.size(), fp);
            std::fclose(fp);
        }
        // 色度另写一份，避免改变 Y 文件「末 N 字节 = Y 平面」口径。Y/U/V 都是
        // DBF（+ 开启 SAO 时的 SAO）之后的最终重建，与解码器输出逐像素可比。
        std::FILE* cp = std::fopen(cname.c_str(), "ab");
        if (cp) {
            std::fwrite(rec.u.data(), 1, rec.u.size(), cp);
            std::fwrite(rec.v.data(), 1, rec.v.size(), cp);
            std::fclose(cp);
        }
    }
#endif
    // 帧间档要把本帧重建当作下一帧的参考，与解码端 DPB 里的帧逐像素一致。
    if (rec_out != nullptr) *rec_out = rec;
    return rbsp;
}

std::string encode_hevc_intra_annexb(std::span<const Bitmap> frames, int qp,
                                     bool transform_skip, int log2_tu, bool sao,
                                     bool rdo) {
    if (log2_tu < 0 || log2_tu == 1 || log2_tu > 5) return std::string{};
    if (frames.empty()) return std::string{};
    const bool auto_tu = (log2_tu == 0);
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H)
            return std::string{};
    }
    // 非 64 倍数的尺寸（如标准 4K 的 2160 高）由 to_yuv 补 CTB padding，
    // 编码树按对齐后的 cw/ch 遍历整块，再由 SPS conformance window 裁掉底部
    // padding（与 PCM 档同一套口径）。早期版本在此直接返回空，导致标准 4K
    // 只能走 I_PCM 档、码率爆到 2.5 Gb/s 被硬解按 Level 预算拒绝。
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;

    std::string out;
    const int level_idc = (cw >= 3840 || ch >= 2160) ? 153 : 30;
    // transform_skip 的尺寸跨度必须在参数集里声明：
    //   max_skip_log2 = 0      关闭（默认）
    //   max_skip_log2 = 2      只覆盖 4x4，按 FFmpeg 默认值，不写 pps_extension
    //   max_skip_log2 = 3/4/5  覆盖到对应 TU（固定档）或全部（自动档取 5），
    //                          需要 RExt profile + pps_range_extension
    // 直接传 transform_skip=true 且大 TU 档时，这是唯一会改动 SPS profile 的路径，
    // 故默认（transform_skip=false）码流仍与既有 Main profile 基线字节级一致。
    const int max_skip_log2 = transform_skip ? (auto_tu ? 5 : log2_tu) : 0;
    const bool rext = (max_skip_log2 > 2);
    codec::hevc::Vps vps;
    vps.general_level_idc = level_idc;
    vps.general_profile_idc = rext ? 4 : 1;
    out += make_nal(32, codec::hevc::write_vps(vps));  // VPS
    codec::hevc::Sps sps;
    sps.general_level_idc = level_idc;
    sps.general_profile_idc = rext ? 4 : 1;
    sps.range_extension = rext;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
    sps.conf_win_bottom = (ch - H) / 2;
    sps.pcm_enabled = false;                          // 关 PCM，走 CABAC 编码树
    // SAO 声明位默认 0（与既有字节级基线一致）；开启时才写 1，slice header 随之
    // 追加 slice_sao_luma/chroma_flag，每个 CTB 前多一段 sao() 语法。
    sps.sao_enabled = sao;
    // 固定档：MaxTb = 1<<log2_tu、变换层次深度 0 ⇒ 每个叶子 CU 的
    // transform_tree 不读 split_transform_flag，整块就是单个 TU。
    // 自动档：MaxTb=32、深度 1 ⇒ 每个 CU 根都要读 split_transform_flag，
    // 8x8 CU 可写 1 分裂成 4 个 4x4 TU（子层 log2=2 不再读）。
    sps.log2_diff_max_min_transform_block_size = auto_tu ? 3 : log2_tu - 2;
    sps.max_transform_hierarchy_depth_intra =
        auto_tu ? 1 : ((log2_tu >= 3) ? 0 : 1);
    // PCM 档用默认 3，SPS 位序列与 G0 黄金基线逐字节一致；两档参数集互不影响。
    out += make_nal(33, codec::hevc::write_sps(sps));  // SPS
    // 真压缩档开启去块滤波：传默认 Pps{}（deblocking_filter_control_present_flag = 0
    // ⇒ pps_deblocking_filter_disabled_flag 推断为 0，滤波生效，且 slice header 不新增
    // 任何比特）。编码端在重建出口同步实现 DBF（见 build_intra_idr_slice 末尾），
    // 因此 "enc 重建 == dec 输出" 的逐像素判据继续成立。PCM 档同样传 Pps{}，位序列不变。
    codec::hevc::Pps pps;
    pps.transform_skip_enabled = (max_skip_log2 >= 2);
    pps.log2_max_transform_skip_block_size = (max_skip_log2 >= 2) ? max_skip_log2 : 2;
    out += make_nal(34, codec::hevc::write_pps(pps));
    for (const auto& frame : frames) {
        const Yuv420 f = to_yuv(frame);
        out += make_nal(19, build_slice_rbsp(f, qp, max_skip_log2, log2_tu, sao, rdo,
                                             std::getenv("TERMCHART_HEVC_NO_DBLOCK") == nullptr,
                                             nullptr, 0, 0));
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

// 帧间档（阶段二）：首帧 IDR，其后为 P 片。为了让「编码端重建 == 解码端输出」
// 逐像素成立，本档刻意把可变因素压到最少：
//   · 单参考（恒指前一帧）、单切片、无 SAO；PPS 声明 disable_deblocking，
//     因为 inter 边界的 bS 推导（8.7.2.4）与本档「bS 恒 2」的帧内假设不同轨；
//   · MV 限定在 kMvStep（2 亮度采样）的整数网格 ⇒ 亮度/色度 MC 全部退化为拷贝；
//   · 每个 8x8 CU 三选一：P_Skip（MV 取自 merge 表，只写 2~5 个 bin）、
//     AMVP + rqt_root_cbf=0（mvd/mvp_lx_flag，不携带残差）、帧内（复用帧内档
//     的 4x4 TU 路径，bin 序列与 I 片完全一致，只多 skip/pred_mode 两个前缀）。
std::string encode_hevc_inter_annexb(std::span<const Bitmap> frames, int qp,
                                     bool motion_search, bool subpel_me, bool multi_ref) {
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H)
            return std::string{};
    }
    if (frames.size() > 65535u) return std::string{};  // POC 位宽 16
    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;
    const int level_idc = (cw >= 3840 || ch >= 2160) ? 153 : 30;

    std::string out;
    codec::hevc::Vps vps;
    vps.general_level_idc = level_idc;
    vps.general_profile_idc = 1;
    vps.max_dec_pic_buffering_minus1 = multi_ref ? 2 : 1;  // 多参考档声明 2 个参考帧
    out += make_nal(32, codec::hevc::write_vps(vps));

    codec::hevc::Sps sps;
    sps.general_level_idc = level_idc;
    sps.general_profile_idc = 1;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
    sps.conf_win_bottom = (ch - H) / 2;
    sps.pcm_enabled = false;
    sps.sao_enabled = false;                          // P 片不写 sao() 语法
    sps.log2_diff_max_min_transform_block_size = 0;   // MaxTb = 4x4
    sps.max_transform_hierarchy_depth_intra = 1;      // 8x8 CU 可拆 4 个 4x4 TU
    sps.log2_max_pic_order_cnt_lsb_minus4 = 12;       // POC 位宽 16
    sps.sps_max_dec_pic_buffering_minus1 = multi_ref ? 2 : 1;
    out += make_nal(33, codec::hevc::write_sps(sps));

    codec::hevc::Pps pps;
    pps.disable_deblocking = true;  // 必须与下面每片的 deblock=false 成对出现
    out += make_nal(34, codec::hevc::write_pps(pps));

    const int log2_poc = sps.log2_max_pic_order_cnt_lsb_minus4 + 4;
    // DPB：ref_list[0] = POC-1（前一帧重建），ref_list[1] = POC-2（前两帧）。
    // 多参考档从 i>=2 起 ref_list[1] 才有有效帧（i==1 只有一个参考）。
    Yuv420 ref_list[2];
    for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
        const Yuv420 f = to_yuv(frames[i]);
        Yuv420 reco;
        std::string rbsp;
        if (i == 0) {
            // 首帧 IDR：与帧内档同一条 4x4 TU 路径，只是不做 DBF（PPS 已禁用）。
            rbsp = build_slice_rbsp(f, qp, 0, 2, false, false, false, nullptr, 0,
                                    log2_poc, &reco);
            if (rbsp.empty()) return std::string{};
            out += make_nal(19, rbsp);
        } else {
            PFrameState pf;
            pf.ref = &ref_list[0];
            pf.ref1 = &ref_list[1];
            pf.num_refs = (multi_ref && i >= 2) ? 2 : 1;
            pf.mvf.reset(cw, ch);
            pf.cw8 = cw / 8;
            pf.skip8.assign(static_cast<std::size_t>(pf.cw8) * (ch / 8), 0);
            pf.max_merge_cand = codec::hevc::kMaxMergeCand;
            pf.motion_search = motion_search;
            pf.subpel_me = subpel_me;
            pf.trace = std::getenv("TERMCHART_HEVC_INTER_TRACE") != nullptr;
            if (const char* force = std::getenv("TERMCHART_HEVC_INTER_FORCE")) {
                if (!std::strcmp(force, "skip")) {
                    pf.force_mode = 1;
                } else if (!std::strcmp(force, "intra")) {
                    pf.force_mode = 3;
                } else if (!std::strcmp(force, "mvp0")) {
                    pf.force_mode = 4;
                } else if (!std::strncmp(force, "mvf:", 4)) {
                    // 亚像素调试档：dx/dy 直接是 1/4 亮度采样单位（不乘 kMvStep），
                    // 用于强制分数 MV 验证插值滤波器与 ffmpeg 解码逐位一致。
                    int dx = 0, dy = 0;
                    std::sscanf(force + 4, "%d,%d", &dx, &dy);
                    pf.force_mode = 2;
                    pf.force_mv = codec::hevc::Mv{dx, dy};
                } else if (!std::strncmp(force, "mv:", 3)) {
                    int dx = 0, dy = 0;
                    std::sscanf(force + 3, "%d,%d", &dx, &dy);
                    pf.force_mode = 2;
                    pf.force_mv = codec::hevc::Mv{dx * codec::hevc::kMvStep,
                                                  dy * codec::hevc::kMvStep};
                }
            }
            rbsp = build_slice_rbsp(f, qp, 0, 2, false, false, false, &pf, i,
                                    log2_poc, &reco);
            if (rbsp.empty()) return std::string{};
            out += make_nal(1, rbsp);  // 非 IDR（P 片）
        }
        // 调试：把编码端重建帧落盘（TERMCHART_HEVC_RECO_YUV=路径），供与 ffmpeg
        // 解码输出逐像素对拍，验证「编码端重建 == 解码端输出」的一致性。
        if (const char* reco_path = std::getenv("TERMCHART_HEVC_RECO_YUV")) {
            static std::FILE* reco_fp = nullptr;
            if (!reco_fp) reco_fp = std::fopen(reco_path, "wb");
            if (reco_fp) {
                if (!reco.y.empty()) std::fwrite(reco.y.data(), 1, reco.y.size(), reco_fp);
                if (!reco.u.empty()) std::fwrite(reco.u.data(), 1, reco.u.size(), reco_fp);
                if (!reco.v.empty()) std::fwrite(reco.v.data(), 1, reco.v.size(), reco_fp);
                if (i + 1 == static_cast<int>(frames.size())) {
                    std::fclose(reco_fp);
                    reco_fp = nullptr;
                }
            }
        }
        // DPB 滚动：当前重建成为新的 POC-1，旧 POC-1 降为 POC-2。
        ref_list[1] = std::move(ref_list[0]);
        ref_list[0] = std::move(reco);
    }
    return out;
}

// 双向 B 片档（GOP=3）：显示序 [I0, B1, P2, B3, P4, ...]，码流按解码序
// [I0, P2, B1, P4, B3, ...] 写出。POC = 显示序号 × 2，B 帧的 POC 落在两个 P 之间。
// 每个 B 片 L0=前一个 P/IDR、L1=紧随其后的 P，两个列表各 1 个参考，故不需要
// ref_idx bins；P 片仍单参考，但 RPS 的 deltaPocS0 按实际 POC 差写（通常 -4）。
std::string encode_hevc_inter_b_annexb(std::span<const Bitmap> frames, int qp,
                                       bool motion_search, bool subpel_me, bool b_amvp) {
    (void)motion_search;  // B 档固定做整数运动搜索
    (void)subpel_me;      // B 档 MV 限定 kMvStep 整数网格，插值退化为拷贝
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames)
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H)
            return std::string{};
    if (frames.size() > 32767u) return std::string{};  // POC = 2*i 须落在 16 bit

    const int cw = ((W + 63) / 64) * 64;
    const int ch = ((H + 63) / 64) * 64;
    const int level_idc = (cw >= 3840 || ch >= 2160) ? 153 : 30;

    std::string out;
    codec::hevc::Vps vps;
    vps.general_level_idc = level_idc;
    vps.general_profile_idc = 1;
    vps.max_dec_pic_buffering_minus1 = 3;  // 2 个参考 + 当前 + 重排序余量
    vps.max_num_reorder_pics = 1;          // B 帧必须晚于后向参考输出一帧
    out += make_nal(32, codec::hevc::write_vps(vps));

    codec::hevc::Sps sps;
    sps.general_level_idc = level_idc;
    sps.general_profile_idc = 1;
    sps.pic_width_in_luma_samples = cw;
    sps.pic_height_in_luma_samples = ch;
    sps.conf_win_bottom = (ch - H) / 2;
    sps.pcm_enabled = false;
    sps.sao_enabled = false;
    sps.log2_diff_max_min_transform_block_size = 0;
    sps.max_transform_hierarchy_depth_intra = 1;
    sps.log2_max_pic_order_cnt_lsb_minus4 = 12;
    sps.sps_max_dec_pic_buffering_minus1 = 3;
    // 必须让解码器知道有 1 帧重排序，否则它按解码序直接输出（I0 P2 B1 ...）。
    sps.max_num_reorder_pics = 1;
    out += make_nal(33, codec::hevc::write_sps(sps));

    codec::hevc::Pps pps;
    pps.disable_deblocking = true;  // inter 边界的 bS 与帧内假设不同轨
    out += make_nal(34, codec::hevc::write_pps(pps));
    const int log2_poc = sps.log2_max_pic_order_cnt_lsb_minus4 + 4;

    std::vector<Yuv420> src;
    src.reserve(frames.size());
    for (const auto& b : frames) src.push_back(to_yuv(b));
    std::vector<Yuv420> rec_by_display(frames.size());

    // DPB 中只需保留最近两个 P/IDR 重建：prev = 最近一个 P，older = 它前面的 P。
    Yuv420 prev, older;
    int prev_poc = 0, older_poc = 0;

    auto emit_slice = [&](int idx, int poc, Yuv420& reco, bool b_slice) {
        PFrameState pf;
        pf.b_slice = b_slice;
        pf.b_merge_only = !b_amvp;
        pf.poc = poc;
        pf.num_refs = 1;
        pf.mvf.reset(cw, ch);
        pf.cw8 = cw / 8;
        pf.skip8.assign(static_cast<std::size_t>(pf.cw8) * (ch / 8), 0);
        pf.max_merge_cand = codec::hevc::kMaxMergeCand;
        pf.motion_search = true;
        pf.trace = std::getenv("TERMCHART_HEVC_INTER_TRACE") != nullptr;
        if (b_slice) {
            pf.ref = &older;
            pf.ref1 = &prev;
            pf.l0_delta = older_poc - poc;
            pf.l1_delta = prev_poc - poc;
        } else {
            pf.ref = &prev;
            pf.l0_delta = prev_poc - poc;  // L0.POC - 当前 POC（通常为 -4）
        }
        const std::string rbsp = build_slice_rbsp(src[static_cast<std::size_t>(idx)], qp,
                                                  0, 2, false, false, false, &pf,
                                                  poc, log2_poc, &reco);
        if (rbsp.empty()) return false;
        out += make_nal(1, rbsp);
        rec_by_display[static_cast<std::size_t>(idx)] = reco;
        if (!b_slice) {
            older = std::move(prev);
            older_poc = prev_poc;
            // reco 是 rec_by_display[idx] 的引用：不能 move，否则该帧的 dump 变空。
            prev = reco;
            prev_poc = poc;
        }
        return true;
    };

    // 首帧 IDR（POC=0）
    {
        Yuv420 reco;
        const std::string rbsp = build_slice_rbsp(src[0], qp, 0, 2, false, false, false,
                                                  nullptr, 0, log2_poc, &reco);
        if (rbsp.empty()) return std::string{};
        out += make_nal(19, rbsp);
        rec_by_display[0] = reco;
        prev = reco;
        older = reco;
        prev_poc = 0;
        older_poc = 0;
    }

    const int n = static_cast<int>(frames.size());
    // base, base+1 构成一个 GOP：先编码 P(base+1)，再回头编码 B(base)。
    for (int base = 1; base < n; base += 2) {
        if (base + 1 < n) {
            if (!emit_slice(base + 1, 2 * (base + 1),
                            rec_by_display[static_cast<std::size_t>(base + 1)], false))
                return std::string{};
            if (!emit_slice(base, 2 * base,
                            rec_by_display[static_cast<std::size_t>(base)], true))
                return std::string{};
        } else {
            // 序列末尾落单的一帧：编码为 P，参考前一个 P。
            if (!emit_slice(base, 2 * base,
                            rec_by_display[static_cast<std::size_t>(base)], false))
                return std::string{};
        }
    }

    // recon dump 必须按【显示序】：ffmpeg 解码器按 POC 重排后输出，编码序的
    // I0 P2 B1 ... 与解码输出对不上，逐字节对拍会假失败。
    if (const char* reco_path = std::getenv("TERMCHART_HEVC_RECO_YUV")) {
        std::FILE* fp = std::fopen(reco_path, "wb");
        if (fp) {
            for (const auto& r : rec_by_display) {
                std::fwrite(r.y.data(), 1, r.y.size(), fp);
                std::fwrite(r.u.data(), 1, r.u.size(), fp);
                std::fwrite(r.v.data(), 1, r.v.size(), fp);
            }
            std::fclose(fp);
        }
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
            return encode_hevc_intra_annexb(frames, qp, opts.hevc_transform_skip,
                                            opts.hevc_log2_tu_size, opts.hevc_sao,
                                            opts.hevc_rdo);
        }
        case VideoEncodeMode::Inter:
            if (opts.hevc_b_slice)
                return encode_hevc_inter_b_annexb(frames, std::clamp(opts.video_qp, 0, 51),
                                                  /*motion_search=*/true, opts.hevc_subpel_me,
                                                  opts.hevc_b_amvp);
            return encode_hevc_inter_annexb(frames, std::clamp(opts.video_qp, 0, 51),
                                            /*motion_search=*/true, opts.hevc_subpel_me,
                                            opts.hevc_multi_ref);
    }
    return std::string{};
}

}  // namespace termchart::image
