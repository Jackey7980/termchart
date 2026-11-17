// test_hevc_bslice.cpp —— B 片（双向预测）基础设施单测

//
// 覆盖三块（都是纯函数 / 纯表，与编码主循环解耦）：
//   1. CABAC 的 B 片初始化行（init_values[2]）确实被选中，且与 P 行有差异；
//   2. inter_pred_idc 的 bin 结构（FFmpeg ff_hevc_inter_pred_idc_decode 同构）；
//   3. B 片 merge 的 combined bi-pred 补齐、zero_idx 零 MV 补齐、AMVP 按列表取 MV，
//      以及双向 MC 的 (a+b+1)>>1 合成。
// 与 ffmpeg 解码端的逐位一致性由外部解码门禁锁（tools/verify_hevc_*.py）。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include <span>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/hevc_encode.hpp"

#include "src/image/codec/hevc/cabac_context.hpp"
#include "src/image/codec/hevc/cu_coding.hpp"
#include "src/image/codec/hevc/cabac_syntax.hpp"
#include "src/image/codec/hevc/inter_coding.hpp"

using namespace termchart::image::codec::hevc;

TEST_CASE("hevc B slice: CABAC 初始化走 init_values[2]") {
    CabacContext b;
    b.init_hevc(26, kInitTypeB);
    CabacContext p;
    p.init_hevc(26, kInitTypeP);
    CabacContext i;
    i.init_hevc(26, kInitTypeI);

    // pred_mode_flag（ctx 12）：P=149 / B=134 → 初始状态必须不同。
    CHECK(b.state(kCtxPredMode) != p.state(kCtxPredMode));
    // split_coding_unit_flag（ctx 2）：P 与 B 表值同为 107 → 状态相同。
    CHECK(p.state(kCtxSplitCuFlag) == b.state(kCtxSplitCuFlag));
    CHECK(i.state(kCtxSplitCuFlag) != b.state(kCtxSplitCuFlag));
    // 整表核对：B 行与 P 行在若干上下文上确实不同（否则等于没选到 B 行）。
    int diff_b_vs_p = 0;
    for (int k = 0; k < kContextCount; ++k)
        if (b.state(k) != p.state(k)) ++diff_b_vs_p;
    CHECK(diff_b_vs_p > 10);
    // 初始化后所有上下文都必须是合法 CABAC 状态（state < 128）。
    for (int k = 0; k < kContextCount; ++k) CHECK(b.state(k) < 128);
}

TEST_CASE("hevc B slice: inter_pred_idc bin 结构与回读") {
    // 8x8 PU（nPbW+nPbH=16）→ 走 bin0@22+ct_depth，再 bin1@22+4。
    const int cd = 3;  // CTB 64 → 8x8 叶，ct_depth = 3
    std::vector<Bin> bi;
    put_inter_pred_idc(bi, cd, 2);
    REQUIRE(bi.size() == 1);
    CHECK(bi[0].ctx == kCtxInterPredIdc + cd);
    CHECK(bi[0].val == 1);

    std::vector<Bin> l0;
    put_inter_pred_idc(l0, cd, 0);
    REQUIRE(l0.size() == 2);
    CHECK(l0[0].ctx == kCtxInterPredIdc + cd);
    CHECK(l0[0].val == 0);
    CHECK(l0[1].ctx == kCtxInterPredIdc + 4);
    CHECK(l0[1].val == 0);

    std::vector<Bin> l1;
    put_inter_pred_idc(l1, cd, 1);
    REQUIRE(l1.size() == 2);
    CHECK(l1[0].val == 0);
    CHECK(l1[1].ctx == kCtxInterPredIdc + 4);
    CHECK(l1[1].val == 1);

    for (int idc = 0; idc < 3; ++idc) {
        const std::vector<Bin> bins = [&] {
            std::vector<Bin> o;
            put_inter_pred_idc(o, cd, idc);
            return o;
        }();
        BinCursor cur(bins.data(), bins.size());
        int back = -1;
        REQUIRE(get_inter_pred_idc(cur, cd, false, back));
        CHECK(back == idc);
        CHECK(cur.consumed() == bins.size());
    }

    // 4x8 / 8x4（nPbW+nPbH==12）：单 bin @ 22+4，1=BI / 0=L0。
    std::vector<Bin> small;
    put_inter_pred_idc(small, cd, 2, true);
    REQUIRE(small.size() == 1);
    CHECK(small[0].ctx == kCtxInterPredIdc + 4);
    CHECK(small[0].val == 1);
}

TEST_CASE("hevc B slice: merge 空间候选 + combined bi-pred + zero_idx 补齐") {
    MvFieldStore fields;
    fields.reset(64, 64);
    PicGeom pic;
    pic.width = 64;
    pic.height = 64;
    pic.log2_ctb = 6;

    PuField l0_only;
    l0_only.pred = kPredL0;
    l0_only.ref_idx = 0;
    l0_only.mv = Mv{8, 0};
    PuField l1_only;
    l1_only.pred = kPredL1;
    l1_only.mv_l1 = Mv{0, 8};
    l1_only.ref_idx_l1 = 0;

    fields.set_block(0, 8, 8, 8, l0_only);    // A1（左）
    fields.set_block(8, 0, 8, 8, l1_only);    // B1（上）

    const CuGeom cu{8, 8, 8, 8};
    PuField out[kMaxMergeCand] = {};
    const int n = derive_merge_candidates_b(fields, cu, pic, 5, 1, 1, out);
    REQUIRE(n == 5);

    // 0: A1（L0 单向）；1: B1（L1 单向）
    CHECK(out[0].pred == kPredL0);
    CHECK(out[0].mv == Mv{8, 0});
    CHECK(out[1].pred == kPredL1);
    CHECK(out[1].mv_l1 == Mv{0, 8});

    // 2: combined bi-pred = A1 的 L0 运动信息 + B1 的 L1 运动信息
    CHECK(out[2].pred == kPredBi);
    CHECK(out[2].mv == Mv{8, 0});
    CHECK(out[2].mv_l1 == Mv{0, 8});
    CHECK(out[2].ref_idx == 0);
    CHECK(out[2].ref_idx_l1 == 0);

    // 3/4: 零 MV 补齐，双向（nb_refs_l0=nb_refs_l1=1 → zero_idx>=nb_refs 取 ref 0）
    CHECK(out[3].pred == kPredBi);
    CHECK(out[3].mv == Mv{0, 0});
    CHECK(out[3].mv_l1 == Mv{0, 0});
    CHECK(out[4].pred == kPredBi);
}

TEST_CASE("hevc B slice: merge 零候选时的 zero_idx 选择") {
    MvFieldStore fields;  // 全 intra → 无空间候选
    fields.reset(64, 64);
    PicGeom pic;
    pic.width = 64;
    pic.height = 64;
    pic.log2_ctb = 6;
    const CuGeom cu{0, 0, 8, 8};

    PuField out[kMaxMergeCand] = {};
    const int n = derive_merge_candidates_b(fields, cu, pic, 5, 2, 2, out);
    REQUIRE(n == 5);
    // zero_idx 遍历 0,1,0,0,0（< nb_refs=min(2,2)=2 的前两个取自身）
    CHECK(out[0].ref_idx == 0);
    CHECK(out[1].ref_idx == 1);
    CHECK(out[2].ref_idx == 0);
    for (int k = 0; k < n; ++k) {
        CHECK(out[k].pred == kPredBi);
        CHECK(out[k].mv == Mv{0, 0});
        CHECK(out[k].ref_idx_l1 == out[k].ref_idx);
    }
}

TEST_CASE("hevc B slice: AMVP 按列表取对应 MV") {
    MvFieldStore fields;
    fields.reset(64, 64);
    PicGeom pic;
    pic.width = 64;
    pic.height = 64;
    pic.log2_ctb = 6;

    PuField nb;
    nb.pred = kPredBi;
    nb.ref_idx = 0;
    nb.mv = Mv{16, 0};
    nb.ref_idx_l1 = 0;
    nb.mv_l1 = Mv{0, 16};
    fields.set_block(0, 8, 8, 8, nb);  // A1

    const CuGeom cu{8, 8, 8, 8};
    Mv out[2] = {};
    int n = derive_amvp_candidates_b(fields, cu, pic, 0, 0, out);
    REQUIRE(n >= 1);
    CHECK(out[0] == Mv{16, 0});

    n = derive_amvp_candidates_b(fields, cu, pic, 1, 0, out);
    REQUIRE(n >= 1);
    CHECK(out[0] == Mv{0, 16});

    // 目标 ref 不匹配 → 不给候选（我们不做跨参考帧 POC 缩放）。
    n = derive_amvp_candidates_b(fields, cu, pic, 0, 1, out);
    CHECK(n == 0);
}

TEST_CASE("hevc B slice: mc_bi 整数位置合成 = (a+b+1)>>1") {
    const int w = 16, h = 16;
    std::vector<std::uint8_t> r0(static_cast<std::size_t>(w) * h, 100);
    std::vector<std::uint8_t> r1(static_cast<std::size_t>(w) * h, 40);
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(8) * 8, 0);
    const CuGeom cu{0, 0, 8, 8};

    mc_bi(r0.data(), r1.data(), w, h, cu, Mv{8, 0}, Mv{0, 8}, 2, dst.data(), 8);
    for (std::size_t k = 0; k < dst.size(); ++k) CHECK(dst[k] == 70);  // (100+40+1)>>1 = 70

    // 边界外采样按边缘复制：mv 指到画面外也不应越界（值仍为两平面常数）
    mc_bi(r0.data(), r1.data(), w, h, CuGeom{8, 8, 8, 8}, Mv{64, 64}, Mv{-64, -64}, 2, dst.data(),
          8);
    for (std::size_t k = 0; k < dst.size(); ++k) CHECK(dst[k] == 70);
}

// ── CU 级：B 片 prediction_unit() 的 bin 序列往返 ─────────────────────────
namespace {

CuInterB8x8 make_amvp_cu(int inter_pred_idc, int ref_l0, int ref_l1) {
    CuInterB8x8 cu;
    cu.skip = false;
    cu.skip_ctx_inc = 0;
    cu.merge = false;
    cu.inter_pred_idc = inter_pred_idc;
    cu.ct_depth = 2;
    cu.nb_refs_l0 = 2;
    cu.nb_refs_l1 = 2;
    cu.ref_idx_l0 = ref_l0;
    cu.ref_idx_l1 = ref_l1;
    cu.mvd_x = 4;
    cu.mvd_y = -3;
    cu.mvd_x_l1 = 3;
    cu.mvd_y_l1 = 0;
    cu.mvp_lx_flag = 1;
    cu.mvp_lx_flag_l1 = 1;
    cu.emit_rqt_root_cbf = false;  // 2Nx2N + merge_flag=0 ⇒ 解码端读该 bin
    cu.rqt_root_cbf = 1;
    return cu;
}

}  // namespace

TEST_CASE("hevc B slice: CU 双向 AMVP 语法可逐 bin 回读") {
    std::int16_t zero[16] = {};
    CuInterB8x8 cu = make_amvp_cu(2, 1, 0);
    cu.luma4x4[0] = zero;
    cu.luma4x4[1] = zero;
    cu.luma4x4[2] = zero;
    cu.luma4x4[3] = zero;
    cu.chroma4x4[0] = zero;
    cu.chroma4x4[1] = zero;

    const std::vector<Bin> bins = encode_cu_inter_b_8x8(cu);
    REQUIRE(!bins.empty());

    BinCursor cur(bins.data(), bins.size());
    int v = -1;
    REQUIRE(get_skip_flag(cur, 0, v));
    CHECK(v == 0);
    REQUIRE(get_pred_mode_flag(cur, v));
    CHECK(v == 0);  // MODE_INTER
    bool part = false;
    REQUIRE(get_part_mode_inter_2nx2n(cur, part));
    CHECK(part);
    REQUIRE(get_merge_flag(cur, v));
    CHECK(v == 0);

    int idc = -1;
    REQUIRE(get_inter_pred_idc(cur, 2, false, idc));
    CHECK(idc == 2);  // PRED_BI

    int idx = -1;
    REQUIRE(get_ref_idx_l0(cur, 2, idx));
    CHECK(idx == 1);
    int mx = 0, my = 0;
    REQUIRE(get_mvd_coding(cur, mx, my));
    CHECK(mx == 4);
    CHECK(my == -3);
    REQUIRE(get_mvp_lx_flag(cur, v));
    CHECK(v == 1);

    REQUIRE(get_ref_idx_l1(cur, 2, idx));
    CHECK(idx == 0);
    REQUIRE(get_mvd_coding(cur, mx, my));
    CHECK(mx == 3);
    CHECK(my == 0);
    REQUIRE(get_mvp_lx_flag(cur, v));
    CHECK(v == 1);
}

TEST_CASE("hevc B slice: PRED_L1 不发 L0 那组语法，PRED_L0 不发 L1") {
    const std::vector<Bin> l1_only = encode_cu_inter_b_8x8(make_amvp_cu(1, 1, 0));
    const std::vector<Bin> l0_only = encode_cu_inter_b_8x8(make_amvp_cu(0, 1, 0));
    const std::vector<Bin> bi = encode_cu_inter_b_8x8(make_amvp_cu(2, 1, 0));

    REQUIRE(!l1_only.empty());
    REQUIRE(!l0_only.empty());
    REQUIRE(!bi.empty());
    // 双向 = 单向 + 另一列表的 (ref_idx + mvd + mvp)，故必然更长。
    CHECK(bi.size() > l1_only.size());
    CHECK(bi.size() > l0_only.size());

    // PRED_L1：inter_pred_idc 之后直接就是 ref_idx_l1（没有任何 L0 语法）。
    BinCursor cur(l1_only.data(), l1_only.size());
    int v = -1, idc = -1, idx = -1, mx = 0, my = 0;
    REQUIRE(get_skip_flag(cur, 0, v));
    REQUIRE(get_pred_mode_flag(cur, v));
    bool part = false;
    REQUIRE(get_part_mode_inter_2nx2n(cur, part));
    REQUIRE(get_merge_flag(cur, v));
    REQUIRE(get_inter_pred_idc(cur, 2, false, idc));
    CHECK(idc == 1);
    REQUIRE(get_ref_idx_l1(cur, 2, idx));
    CHECK(idx == 0);
    REQUIRE(get_mvd_coding(cur, mx, my));
    CHECK(mx == 3);
    CHECK(my == 0);
    REQUIRE(get_mvp_lx_flag(cur, v));
    CHECK(v == 1);
}

TEST_CASE("hevc B slice: GOP=3 码流的 NAL 结构与默认开关") {
    using namespace termchart::image;
    std::vector<Bitmap> frames;
    for (int i = 0; i < 5; ++i) {
        Bitmap b(64, 64, 4);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const auto v = static_cast<std::uint8_t>((x + y + 4 * i) & 0xff);
                b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, v);
                b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, v);
                b.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, v);
            }
        frames.push_back(std::move(b));
    }
    EncodeOptions opts;
    opts.video_mode = VideoEncodeMode::Inter;
    opts.video_qp = 26;
    const auto plain = encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
    opts.hevc_b_slice = true;
    const auto with_b = encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);

    REQUIRE(!plain.empty());
    REQUIRE(!with_b.empty());
    CHECK(plain != with_b);  // 开关必须真正改变码流，而不是空壳

    const auto nal_types = [](const std::string& bs) {
        std::vector<int> types;
        for (std::size_t i = 0; i + 3 < bs.size(); ++i) {
            if (static_cast<unsigned char>(bs[i]) == 0 &&
                static_cast<unsigned char>(bs[i + 1]) == 0 &&
                static_cast<unsigned char>(bs[i + 2]) == 1) {
                types.push_back((static_cast<unsigned char>(bs[i + 3]) >> 1) & 0x3f);
                i += 2;
            }
        }
        return types;
    };
    // VPS, SPS, PPS, IDR + 4 个非 IDR 片，B 档写出顺序是 I0 P2 B1 P4 B3。
    CHECK(nal_types(with_b) == std::vector<int>{32, 33, 34, 19, 1, 1, 1, 1});
}
