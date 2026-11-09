// test_hevc_mpm.cpp —— MPM 候选表与帧内模式信号，逐式对拍 FFmpeg
// libavcodec/hevc/hevcdec.c:luma_intra_pred_mode (2251-2330)。
#include <doctest.h>

#include <algorithm>

#include "src/image/codec/hevc/intra_prediction.hpp"

namespace {

using namespace termchart::image::codec::hevc;

// 独立复刻 FFmpeg 解码端（用于穷举求逆的交叉验证）。
int ff_decode_mode(bool prev, int mpm_idx, int rem, int cand_left, int cand_up) {
    int cand[3];
    if (cand_left == cand_up) {
        if (cand_left < 2) { cand[0] = 0; cand[1] = 1; cand[2] = 26; }
        else {
            cand[0] = cand_left;
            cand[1] = 2 + ((cand_left - 2 - 1 + 32) & 31);
            cand[2] = 2 + ((cand_left - 2 + 1) & 31);
        }
    } else {
        cand[0] = cand_left; cand[1] = cand_up;
        if (cand[0] != 0 && cand[1] != 0) cand[2] = 0;
        else if (cand[0] != 1 && cand[1] != 1) cand[2] = 1;
        else cand[2] = 26;
    }
    if (prev) return cand[mpm_idx];
    std::sort(cand, cand + 3);
    int mode = rem;
    for (int i = 0; i < 3; ++i)
        if (mode >= cand[i]) ++mode;
    return mode;
}

}  // namespace

TEST_CASE("mpm: candidate table matches FFmpeg for equal neighbours") {
    // cand_left == cand_up，且 < 2 ⇒ {Planar, DC, 26}
    for (int m : {0, 1}) {
        const auto c = derive_mpm_candidates(m, m);
        CHECK(c.mode[0] == 0);
        CHECK(c.mode[1] == 1);
        CHECK(c.mode[2] == 26);
    }
    // 角度模式：{m, m-1, m+1} 在 2..33 环绕
    for (int m : {2, 10, 26, 33, 34}) {
        const auto c = derive_mpm_candidates(m, m);
        CHECK(c.mode[0] == m);
        CHECK(c.mode[1] == 2 + ((m - 3 + 32) & 31));
        CHECK(c.mode[2] == 2 + ((m - 1) & 31));
    }
}

TEST_CASE("mpm: candidate table matches FFmpeg for unequal neighbours") {
    struct Case { int l, u, e2; };
    const Case cases[] = {
        {1, 0, 26},   // 一个 Planar 一个 DC ⇒ 26
        {0, 1, 26},
        {10, 18, 0},  // 都不是 Planar ⇒ Planar
        {0, 10, 26},  // 有 Planar、都不是 DC? 0!=1 && 10!=1 ⇒ DC
        {1, 10, 26},  // 有 DC、都不是 Planar ⇒ DC
    };
    for (const auto& t : cases) {
        const auto c = derive_mpm_candidates(t.l, t.u);
        CHECK(c.mode[0] == t.l);
        CHECK(c.mode[1] == t.u);
        // 第三项按 FFmpeg 分支计算，与独立复刻交叉验证
        int e[3] = {t.l, t.u, 0};
        if (e[0] != 0 && e[1] != 0) e[2] = 0;
        else if (e[0] != 1 && e[1] != 1) e[2] = 1;
        else e[2] = 26;
        CHECK(c.mode[2] == e[2]);
    }
}

TEST_CASE("mpm: signal_intra_mode round-trips through FFmpeg decoder for all modes") {
    // 覆盖多组邻居组合与全部 35 个模式，穷举求逆必须与 FFmpeg 解码一致。
    const int pairs[][2] = {{1, 1}, {0, 0}, {0, 1}, {1, 0}, {10, 10},
                            {26, 26}, {10, 18}, {2, 34}, {34, 2}};
    for (const auto& p : pairs) {
        const auto c = derive_mpm_candidates(p[0], p[1]);
        for (int target = 0; target < 35; ++target) {
            const auto s = signal_intra_mode(target, c);
            const int decoded = ff_decode_mode(
                s.prev_intra_luma_pred_flag,
                s.prev_intra_luma_pred_flag ? s.mpm_idx : 0,
                s.prev_intra_luma_pred_flag ? 0 : s.rem_intra_luma_pred_mode,
                p[0], p[1]);
            CHECK(decoded == target);
        }
    }
}

TEST_CASE("mpm: modes in candidate table use prev=1 with exact index") {
    const auto c = derive_mpm_candidates(1, 1);  // {0,1,26}
    auto s = signal_intra_mode(0, c);
    CHECK(s.prev_intra_luma_pred_flag);
    CHECK(s.mpm_idx == 0);
    s = signal_intra_mode(1, c);
    CHECK(s.prev_intra_luma_pred_flag);
    CHECK(s.mpm_idx == 1);
    s = signal_intra_mode(26, c);
    CHECK(s.prev_intra_luma_pred_flag);
    CHECK(s.mpm_idx == 2);
}
