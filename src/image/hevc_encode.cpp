// hevc_encode.cpp —— 自写 HEVC（H.265, Main profile）编码器（I_PCM 档）
#include "termchart/image/hevc_encode.hpp"

#include <algorithm>
#include <cmath>
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

std::string build_intra_idr_slice(const Yuv420& f, int slice_qp,
                                  int max_skip_log2, int log2_tu) {
    // max_skip_log2 = PPS 的 log2_max_transform_skip_block_size（0 = 关闭）。
    // <=2 只有 4x4 TU 带 flag；>=3 时更大 TU 也能带，但那是参数集替我们声明的，
    // 这里只需保证「不超过声明值」，否则解码端根本不读该 flag、bin 顺序全错。
    const bool transform_skip_enabled = (max_skip_log2 >= 2);
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
    auto emit_cu = [&](int x0, int y0, int log2_cb, int tu_log2, int split4) {
        // 本编码器 8x8 CU 用 PART_2Nx2N ⇒ 整 8x8 单 PU 预测（ffmpeg 解码端同样按
        // 8x8 预测）。故先用选定模式算整块预测，再拆成 4 个 4x4 TU 写残差。
        const int mode = (kIntraMode >= 0)
                             ? kIntraMode
                             : codec::hevc::choose_intra_luma_mode(
                                   f.y.data(), cw, rec.y.data(), cw,
                                   x0, y0, cw, f.ch, slice_qp, tu_log2);
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
        // 大 CU 覆盖多个 8x8 PU 格，后续 CU 的 MPM 左/上邻居可能落在其中任意一格。
        const int cells = 1 << (log2_cb - 3);
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
            cabac.encode_decision(ctx.ctx[kCtxSplitCuFlag + inc], stop ? 0 : 1);
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
        // 色度另写一份，避免改变 Y 文件「末 N 字节 = Y 平面」口径；DBF 只滤亮度，
        // 故 U/V 始终是滤波前重建。
        std::FILE* cp = std::fopen(cname.c_str(), "ab");
        if (cp) {
            std::fwrite(rec.u.data(), 1, rec.u.size(), cp);
            std::fwrite(rec.v.data(), 1, rec.v.size(), cp);
            std::fclose(cp);
        }
    }
#endif
    return rbsp;
}

std::string encode_hevc_intra_annexb(std::span<const Bitmap> frames, int qp,
                                     bool transform_skip, int log2_tu) {
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
        out += make_nal(19, build_intra_idr_slice(f, qp, max_skip_log2, log2_tu));
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
            return encode_hevc_intra_annexb(frames, qp, opts.hevc_transform_skip,
                                            opts.hevc_log2_tu_size);
        }
        case VideoEncodeMode::Inter:
            // 阶段二才落地；显式返回空串，不静默降级成 PCM。
            return std::string{};
    }
    return std::string{};
}

}  // namespace termchart::image
