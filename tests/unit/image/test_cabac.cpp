// test_cabac.cpp —— HEVC CABAC 算术编码器/解码器往返回归测试
//
// 防止「编码器实现被改写后不再可逆」这类静默回归（曾有一次把可用的
// FFmpeg 风格 queue_/outstanding_ 实现误重写成 bitsToFollow 导致 decision
// 流在长 bypass 后错约 50%）。
//
// 验证策略（不重写同一套算法，避免同错同过）：
//   1) 用仓库自带的解码器 CabacArithDec（标准 16-bit 窗口，bypass 阈值 range<<7）
//      作为对偶 oracle，做 bin -> bits -> bin 完整往返；
//   2) 覆盖 decision / bypass / terminate 混合流，含极长流（残差场景）；
//   3) 编码端和解码端用同一份预生成随机序列回放，逐位比对。
#include <doctest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "src/media/encoder/hevc/cabac_arith.hpp"
#include "src/media/encoder/hevc/cabac_arith_dec.hpp"
#include "src/media/encoder/hevc/cabac_context.hpp"

namespace {

using namespace termchart::image::codec::hevc;

enum class Kind { Decision, Bypass };

// 跑一个往返用例：返回 mismatches 数量（含 terminate 判定）。
int run_roundtrip(std::mt19937& rng, int n_bins) {
    std::vector<Kind> kinds(n_bins);
    std::vector<int> bins(n_bins);
    std::vector<int> ctx_idx(n_bins);
    std::vector<CtxModel> ctx_enc(183), ctx_dec(183);

    CabacContext cc;
    cc.init_hevc(26, 0);
    for (int i = 0; i < 183; ++i) ctx_enc[i] = ctx_dec[i] = cc.ctx[i];

    std::uniform_int_distribution<int> pick(0, 1);
    std::uniform_int_distribution<int> ctxsel(0, 182);
    for (int i = 0; i < n_bins; ++i) {
        kinds[i] = (i % 3 == 0) ? Kind::Bypass : Kind::Decision;
        bins[i] = pick(rng);
        ctx_idx[i] = ctxsel(rng);
    }

    CabacArith enc;
    enc.init();
    for (int i = 0; i < n_bins; ++i) {
        if (kinds[i] == Kind::Bypass)
            enc.encode_bypass(bins[i]);
        else
            enc.encode_decision(ctx_enc[ctx_idx[i]], bins[i]);
    }
    enc.encode_terminate(1);
    std::string bytes = enc.finish();

    CabacArithDec dec;
    dec.init(bytes.data(), bytes.size());
    int failures = 0;
    for (int i = 0; i < n_bins; ++i) {
        int got = (kinds[i] == Kind::Bypass)
                      ? dec.decode_bypass()
                      : dec.decode_decision(ctx_dec[ctx_idx[i]]);
        if (got != bins[i]) ++failures;
    }
    if (dec.decode_terminate() != 1) ++failures;
    return failures;
}

// 纯 bypass 长流（coeff_abs_level_remaining / sign 场景）。
int run_bypass_only(std::mt19937& rng, int n_bins) {
    std::vector<int> bins(n_bins);
    std::uniform_int_distribution<int> pick(0, 1);
    for (int i = 0; i < n_bins; ++i) bins[i] = pick(rng);

    CabacArith enc;
    enc.init();
    for (int i = 0; i < n_bins; ++i) enc.encode_bypass(bins[i]);
    enc.encode_terminate(1);
    std::string bytes = enc.finish();

    CabacArithDec dec;
    dec.init(bytes.data(), bytes.size());
    int failures = 0;
    for (int i = 0; i < n_bins; ++i)
        if (dec.decode_bypass() != bins[i]) ++failures;
    if (dec.decode_terminate() != 1) ++failures;
    return failures;
}

}  // namespace

TEST_CASE("cabac arithmetic coder round-trips decision+bypass+terminate") {
    const int seeds[] = {1, 2, 3, 7, 42, 100, 777, 2024, 99999, 123456};
    for (int s : seeds) {
        std::mt19937 rng(s);
        for (int n : {1, 2, 10, 100, 1000, 5000, 20000}) {
            CHECK(run_roundtrip(rng, n) == 0);
        }
    }
    for (int s : seeds) {
        std::mt19937 rng(s + 1);
        CHECK(run_bypass_only(rng, 20000) == 0);
    }
}
