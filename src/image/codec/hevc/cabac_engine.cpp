// cabac_engine.cpp —— HEVC CABAC 算术编码引擎（A3-2 从 hevc_encode.cpp 原样迁入）
//
// 三张概率/状态表取自 H.265 标准（Table 9-37 / 9-38 / 9-39），属规范数据；
// 上下文初始化用 init_values + 标准公式（init_type = 2 - slice_type，I 帧 = 0）。
// 码流构造不移植任何解码器源码：它把每个 bin 的判定式写成对「段首 16 位
// CABAC 字节 B」的线性不等式，用仿射区间 [lo,hi) 求解析解，再用独立实现的
// 解码判定复算校验。行为与合规解码器逐位一致（本机 FFmpeg 9.0.2 全帧验证）。
//
// low：算术低位寄存器，有效位在 [9:0]（bit9=进位位，bit8=下一个待出比特）；
// range：9-bit (∈[256,510])。每做一次 renorm：输出 (low>>8)&1 作为比特、
// 用 (low>>9)&1 处理进位，再 low=(low<<1)&0x3FF、range<<=1。
// 注意：HEVC CABAC 启动时【不做】首比特反转（与 H.264 不同），输出比特原样写入码流。
//
// FFmpeg CABAC 解码器的精确对偶求解器：
// 背景：I_PCM 块之后 HEVC 要求重置算术引擎（9.3.4.4），FFmpeg 由
// hls_pcm_sample() 调 skip_bytes(&lc->cc, 96) 完成：它按 low 的低位回退
// bytestream 得到 PCM 起点，并从 ptr+96 重新 ff_init_cabac_decoder()。
// 解码端是 16-bit 优化模型（CABAC_BITS=16, low 为 26-bit 定点，range 为 9-bit），
// 与编码端 10-bit codILow 的写法不天然对齐；早期用 HM 式 FlushD 直接拼位流，
// 生成的 low 初值落在错误子区间，pcm_flag 恒被判 0（全帧退化成 intra 128）。
//
// 这里改为「按解码端模型反解」：每段固定输出 2 字节 CABAC（16 位 B），
// 解码端 ff_init_cabac_decoder 得到 low = B<<10 | (1<<9)（bytestream 偶地址支）
// 或 B<<10 | (b2<<2)+2（奇地址支）。把 low 视为 B 的仿射式 a*B + c，
// 则每个 bin 的 DecodeDecision / DecodeTerminate 判定都收缩出 B 的一个区间 [lo,hi)。
// 取区间下界即得正确 B；再用精确解码模拟（含两个地址分支）校验一遍。
// 这样编码端无需模拟进位/bitsToFollow，且与 FFmpeg 逐位一致（实测全帧精确）。
#include "cabac_engine.hpp"

#include <utility>

namespace termchart::image::codec::hevc {

// I_PCM 档专用：上下文编号沿用旧的自定义表（init()）。真压缩档不经本引擎，
// 走 CabacArith + CabacContext::init_hevc()。若这里改用 init_hevc，PCM 码流的
// 算术编码初始状态会变，ffmpeg 解出的 PCM 样本整体错位（G0 立刻破）。
void CabacEncoder::init_contexts(int slice_qp) { contexts_.init(slice_qp); }

void CabacEncoder::encode_bin(int ctx_idx, int bin) { seg_.push_back({ctx_idx, bin}); }

// FFmpeg ff_hevc_pcm_flag_decode() = get_cabac_terminate()，不占上下文
// （CABAC_ELEMS 中 PCM_FLAG 的 NUM_BINS = 0）。
void CabacEncoder::encode_terminating(int bit) { seg_.push_back({-1, bit}); }

CabacEncoder::acc_t CabacEncoder::ceil_div(acc_t p, acc_t q) {
    acc_t d = p / q, r = p % q;             // C++ 向零截断；p<0 不整除时已等于 ceil
    if (r > 0) ++d;                         // p>0 不整除时需上取整
    return d;
}

// 解析区间求解：返回可行 B 的下界（写入 lo/hi）。
// low = a*B + c；ff_init_cabac_decoder 偶地址支给出 a=1024, c=512。
// 每个 bin 的判定 T=(range-RangeLPS)<<17 对 B 产生 lo/hi 约束，renorm 时
// low 与 range 同步左移（a、c 同步左移），故始终是仿射式，无需逐位模拟。
// st 为本段起始的上下文状态快照；函数同时把 st 演化到段末（供下一 CU 段使用）。
bool CabacEncoder::solve_bounds(acc_t& lo, acc_t& hi, std::uint8_t* st) const {
    lo = 0; hi = 65536;
    acc_t a = 1024, c = 512;
    int range = 510;
    for (const auto& e : seg_) {
        const int ci = e.first, want = e.second;
        if (ci < 0) {                                  // DecodeTerminate
            range -= 2;
            const acc_t T = static_cast<acc_t>(range) << 17;
            if (want == 1) {                           // pcm_flag / 末尾 eos = 1
                const acc_t lb = ceil_div(T - c, a);   // 需 low >= T
                if (lb > lo) lo = lb;
                break;                                 // 段结束，引擎随后重置
            }
            // end_of_slice_segment_flag = 0（非末尾 CTB）：需 low < T。
            // get_cabac_terminate 返回 0 时调 renorm_cabac_decoder_once()，
            // 引擎【不重置】，本段后续 bin（下一个 CTB 的 split）继续在同一引擎上。
            const acc_t ub = ceil_div(T - c, a);
            if (ub < hi) hi = ub;
            const int shift = (range < 256) ? 1 : 0;
            range <<= shift; a <<= shift; c <<= shift;
            if (lo >= hi) return false;
            continue;
        }
        const int s = st[ci];
        const int RangeLPS = range_lps(s, range);
        range -= RangeLPS;
        const acc_t T = static_cast<acc_t>(range) << 17;
        const bool need_lps = (want != (s & 1));
        if (need_lps) {
            const acc_t lb = ceil_div(T - c, a);
            if (lb > lo) lo = lb;
            c -= T;
            range = RangeLPS;
        } else {
            const acc_t ub = ceil_div(T - c, a);       // MPS 需 low < T
            if (ub < hi) hi = ub;
        }
        st[ci] = next_state(s, need_lps);
        const int shift = norm_shift(range);
        range <<= shift;
        a <<= shift;
        c <<= shift;
        if (lo >= hi) return false;
    }
    return lo < hi;
}

// 精确复刻 FFmpeg get_cabac_inline / get_cabac_terminate / skip_bytes 的校验。
// odd_addr 覆盖 ff_init_cabac_decoder 按 bytestream 地址奇偶分支读 2/3 字节的两种情形。
bool CabacEncoder::verify(std::uint32_t B, bool odd_addr, std::uint8_t raw0,
                          const std::uint8_t* st0, bool check_ptr) const {
    std::uint8_t st[154];
    for (int i = 0; i < 154; ++i) st[i] = st0[i];
    const std::uint8_t d0 = static_cast<std::uint8_t>(B >> 8), d1 = static_cast<std::uint8_t>(B);
    // 用 uint32_t 承载 low，规避 FFmpeg 原始实现里 int 左移的有符号溢出 UB。
    std::uint32_t low = (static_cast<std::uint32_t>(d0) << 18) +
                        (static_cast<std::uint32_t>(d1) << 10);
    int bytestream;
    if (!odd_addr) { low += (1u << 9); bytestream = 2; }
    else { low += (static_cast<std::uint32_t>(raw0) << 2) + 2u; bytestream = 3; }
    int range = 0x1FE;
    if ((static_cast<std::uint32_t>(range) << 17) < low) return false;  // AVERROR_INVALIDDATA
    for (const auto& e : seg_) {
        const int ci = e.first, want = e.second;
        if (ci < 0) {
            range -= 2;
            const std::uint32_t T = static_cast<std::uint32_t>(range) << 17;
            if (want == 1) {
                if (low < T) return false;             // pcm_flag 会被判 0
                break;
            }
            if (low >= T) return false;                // eos=0 需要 low < T
            const int shift = (range < 256) ? 1 : 0;  // renorm_cabac_decoder_once
            range <<= shift; low <<= shift;
            if (!(low & 0xFFFF)) return false;
            continue;
        }
        const int s = st[ci];
        const int RangeLPS = range_lps(s, range);
        range -= RangeLPS;
        const std::uint32_t T = static_cast<std::uint32_t>(range) << 17;
        const bool lps = (low > T);      // ((T - low) >> 31) != 0 的无符号等价
        const int bit = lps ? (1 - (s & 1)) : (s & 1);
        if (bit != want) return false;
        if (lps) { low -= T; range = RangeLPS; }
        st[ci] = next_state(s, lps);
        const int shift = norm_shift(range);
        range <<= shift;
        low <<= shift;
        // refill2 会多读 2 字节，使 skip_bytes 的 ptr 整体偏 2、PCM 起点错位；
        // 因此本段的合法 B 必须保证段内不触发 refill2。
        if (!(low & 0xFFFF)) return false;
    }
    if (check_ptr) {
        int ptr = bytestream;
        if (low & 0x1) --ptr;
        if (low & 0x1FF) --ptr;                        // CABAC_BITS == 16
        if (ptr != 2) return false;                    // PCM 必须紧跟 2 字节 CABAC
    }
    return true;
}

// 求解本段 2 字节 CABAC：解析区间取下界，精确解码模拟在两个地址分支校验。
// 若区间下界不满足（例如本段 refill2 触发使仿射假设失效），顺延扫描整个区间。
std::uint32_t CabacEncoder::solve(std::uint8_t raw0, bool check_ptr) {
    std::uint8_t st0[154];
    for (int i = 0; i < 154; ++i) st0[i] = contexts_.state(i);
    acc_t lo, hi;
    std::uint8_t evolved[154];
    for (int i = 0; i < 154; ++i) evolved[i] = st0[i];
    const bool ok = solve_bounds(lo, hi, evolved);
    if (ok) {
        for (acc_t v = lo; v < hi && v <= 0xFFFF; ++v) {
            const std::uint32_t B = static_cast<std::uint32_t>(v);
            if (verify(B, false, raw0, st0, check_ptr) &&
                verify(B, true, raw0, st0, check_ptr)) {
                for (int i = 0; i < 154; ++i) contexts_.set_state(i, evolved[i]);
                return B;
            }
        }
    }
    // 兜底：全空间扫描（解析解只在 refill2 未触发时成立）
    for (std::uint32_t B = 0; B <= 0xFFFF; ++B) {
        if (verify(B, false, raw0, st0, check_ptr) &&
            verify(B, true, raw0, st0, check_ptr)) {
            std::uint8_t tmp[154];
            for (int i = 0; i < 154; ++i) tmp[i] = contexts_.state(i);
            for (const auto& e : seg_) {
                if (e.first >= 0)
                    tmp[e.first] = next_state(tmp[e.first],
                                              e.second != (tmp[e.first] & 1));
            }
            for (int i = 0; i < 154; ++i) contexts_.set_state(i, tmp[i]);
            return B;
        }
    }
    return 0;
}

void CabacEncoder::flush_cu(std::string raw) {
    const std::uint32_t B = solve(static_cast<std::uint8_t>(raw[0]), true);
    final_out_.push_back(static_cast<char>(B >> 8));
    final_out_.push_back(static_cast<char>(B & 0xFF));
    final_out_ += raw;                  // pcm_sample_luma/chroma，字节对齐
    // HEVC 9.3.4.4：PCM 后算术引擎重置（每段各自 ff_init_cabac_decoder）；
    // 上下文模型跨段保持，solve() 已把它演化到段末状态。
    seg_.clear();
}

// 切片末尾：末段（end_of_slice_flag 终止 bin）+ rbsp_stop_one_bit(1) + 补零。
// 末段之后没有 PCM 数据，skip_bytes 不参与，故不检查 ptr。
std::string CabacEncoder::finish() {
    const std::uint32_t B = solve(0, false);
    final_out_.push_back(static_cast<char>(B >> 8));
    final_out_.push_back(static_cast<char>(B & 0xFF));
    final_out_.push_back(static_cast<char>(0x80));   // stop bit 1 + 7 个 0
    seg_.clear();
    return std::move(final_out_);
}

}  // namespace termchart::image::codec::hevc
