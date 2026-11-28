// test_hevc_rdo.cpp —— 阶段三 P3-3：帧内模式率失真决策的语法 bin 口径
//
// 期望值按 cabac_syntax.cpp 的 put_* 实拼出来，不靠被测函数自证：
// mode_signal_bins 必须与真正写进码流的模式语法 bin 数逐项相等，否则
// RDO 的 λ·bins 项就是凭空估值。另锁住「默认 rd_lambda=0 逐位退化为
// 纯 SSD」这一字节级基线契约。
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "src/media/encoder/hevc/cabac_syntax.hpp"
#include "src/media/encoder/hevc/intra_prediction.hpp"

namespace {

using namespace termchart::image::codec::hevc;

constexpr int kPic = 32;

struct Plane {
    // 用 = 初始化：写成 buf(size, value) 会被 most-vexing-parse 解析成成员函数。
    std::vector<std::uint8_t> buf = std::vector<std::uint8_t>(
        static_cast<std::size_t>(kPic) * kPic, 128);
    std::uint8_t* data() { return buf.data(); }
};

// 与 cu_coding.cpp / cabac_syntax.cpp 的发射顺序一致，拼出「模式语法」那一段。
int real_mode_bins(const IntraModeSignal& sig) {
    std::vector<Bin> bins;
    put_prev_intra_luma_pred_flag(bins, sig.prev_intra_luma_pred_flag ? 1 : 0);
    if (sig.prev_intra_luma_pred_flag) put_mpm_idx(bins, sig.mpm_idx);
    else put_rem_intra_luma_pred_mode(bins, sig.rem_intra_luma_pred_mode);
    return static_cast<int>(bins.size());
}

}  // namespace

TEST_CASE("hevc rdo: mode_signal_bins 与真实 put_* 语法逐项等长") {
    // 多组左/上模式组合，覆盖 MPM 候选表去重 / 排序求逆的各条分支。
    for (auto [left, up] : {std::pair{1, 1}, std::pair{0, 26}, std::pair{18, 18},
                            std::pair{34, 2}, std::pair{10, 11}}) {
        const MpmCandidates cands = derive_mpm_candidates(left, up);
        for (int mode = 0; mode < 35; ++mode) {
            const IntraModeSignal sig = signal_intra_mode(mode, cands);
            CHECK(mode_signal_bins(mode, cands) == real_mode_bins(sig));
        }
    }
}

TEST_CASE("hevc rdo: mode_signal_bins 的 MPM / 非 MPM 具体值") {
    const MpmCandidates c{{0, 1, 26}};
    CHECK(mode_signal_bins(0, c) == 2);   // prev(1) + mpm_idx=0 写 1 bin
    CHECK(mode_signal_bins(1, c) == 3);   // prev(1) + mpm_idx=1 写 2 bin
    CHECK(mode_signal_bins(26, c) == 3);  // prev(1) + mpm_idx=2 写 2 bin
    CHECK(mode_signal_bins(10, c) == 6);  // prev(0) + rem 的 5 个 bypass bin
    CHECK(mode_signal_bins(34, c) == 6);
}

TEST_CASE("hevc rdo: 失真余量默认值与环境变量注入点") {
    // 不设 TERMCHART_HEVC_RDO_SLACK 时为 0.05；值在进程内缓存，故只校验区间。
    const double s = rdo_distortion_slack();
    CHECK(s >= 0.0);
    CHECK(s <= 1.0);
}

TEST_CASE("hevc rdo: rd_lambda=0 与默认调用逐位一致（字节级基线）") {
    Plane src, rec;
    // 造一点方向性纹理，让 35 个模式的 SSD 不全相等（否则比较无意义）。
    for (int y = 0; y < kPic; ++y)
        for (int x = 0; x < kPic; ++x)
            src.buf[static_cast<std::size_t>(y) * kPic + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(100 + (x * 5 + y * 2) % 60);
    for (int lg : {2, 3, 4}) {
        for (int qp : {10, 26, 40}) {
            const int a = choose_intra_luma_mode(
                src.data(), kPic, rec.data(), kPic, 8, 8, kPic, kPic, qp, lg);
            const int b = choose_intra_luma_mode(
                src.data(), kPic, rec.data(), kPic, 8, 8, kPic, kPic, qp, lg, 0.0,
                nullptr);
            CHECK(a == b);
            CHECK(a >= 0);
            CHECK(a < 35);
        }
    }
}

TEST_CASE("hevc rdo: 任意 λ / slack 下都只返回合法模式") {
    Plane src, rec;
    for (int y = 0; y < kPic; ++y)
        for (int x = 0; x < kPic; ++x)
            src.buf[static_cast<std::size_t>(y) * kPic + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>((x * 7 + y * 13) % 256);
    const MpmCandidates cands = derive_mpm_candidates(1, 1);
    const double lambdas[] = {0.0, 1.0, 100.0, 1e6};
    for (int lg : {2, 3, 4}) {
        for (double lambda : lambdas) {
            const int m = choose_intra_luma_mode(
                src.data(), kPic, rec.data(), kPic, 8, 8, kPic, kPic, 26, lg,
                lambda, &cands);
            CHECK(m >= 0);
            CHECK(m < 35);
            // 信号化必须能还原成同一个模式（否则解码端会挑错模式）。
            const IntraModeSignal sig = signal_intra_mode(m, cands);
            if (sig.prev_intra_luma_pred_flag) {
                CHECK(cands.mode[sig.mpm_idx] == m);
            } else {
                int sorted[3] = {cands.mode[0], cands.mode[1], cands.mode[2]};
                std::sort(sorted, sorted + 3);
                int mode = sig.rem_intra_luma_pred_mode;
                for (int i = 0; i < 3; ++i)
                    if (mode >= sorted[i]) ++mode;
                CHECK(mode == m);
            }
        }
    }
}

TEST_CASE("hevc rdo: 8x8 转发包装与 tu_log2=2 等价") {
    Plane src, rec;
    for (int y = 0; y < kPic; ++y)
        for (int x = 0; x < kPic; ++x)
            src.buf[static_cast<std::size_t>(y) * kPic + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(120 + (x % 9) * 4);
    CHECK(choose_intra_luma_mode_8x8(src.data(), kPic, rec.data(), kPic,
                                     8, 8, kPic, kPic, 26) ==
          choose_intra_luma_mode(src.data(), kPic, rec.data(), kPic,
                                 8, 8, kPic, kPic, 26, 2));
}
