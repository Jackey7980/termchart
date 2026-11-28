// test_hevc_sao.cpp —— HEVC 样本自适应偏移（7.3.9.2 sao() + 8.7.3 滤波）
//
// 期望值按 HEVC 规范与 FFmpeg hevcdec.c / h2656_sao_template.c 手工展开，
// 不调用被测实现推导。重点锁住三处真实事故：
//   ① EDGE 的 abs[0]/abs[1] 取正、abs[2]/abs[3] 取负（曾把 cat1 做成负号）；
//   ② eo_class 2 = 135°（左上↔右下）、3 = 45°（左下↔右上）（曾整体对调）；
//   ③ dst_x0/dst_y0 局部原点写回（EDGE 分支曾按绝对坐标写，越界踩堆）。
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "src/media/encoder/hevc/sao.hpp"

namespace {

using namespace termchart::image::codec::hevc;

// 8×8 单 CTB 画面：行主序，stride = 8。
struct Plane8 {
    std::vector<std::uint8_t> src;
    std::vector<std::uint8_t> dst;
    Plane8() : src(64, 0), dst(64, 0) {}
    void rows(const std::vector<std::uint8_t>& row) {
        for (int y = 0; y < 8; ++y)
            std::copy(row.begin(), row.end(), src.begin() + y * 8);
    }
    void filter(const SaoCtbParams& p) {
        sao_filter_ctb(p, 0, 0, 0, 8, 8, src.data(), 8, dst.data(), 8, 8, 8);
    }
};

SaoCtbParams edge_params(const int abs[4], int cls = 0) {
    SaoCtbParams p;
    p.type_idx[0] = kSaoEdge;
    p.eo_class[0] = cls;
    for (int i = 0; i < 4; ++i) p.offset_abs[0][i] = abs[i];
    sao_derive_offsets(p);
    return p;
}

}  // namespace

TEST_CASE("hevc sao: BO offset_val 推导与 log2 scale") {
    SaoCtbParams p;
    p.type_idx[0] = kSaoBand;
    p.offset_abs[0][0] = 3;
    p.offset_abs[0][1] = 0;
    p.offset_abs[0][2] = 5;
    p.offset_sign[0][2] = 1;  // 只有这一项取负
    p.offset_abs[0][3] = 2;
    sao_derive_offsets(p, 1, 0);  // 亮度缩放 ×2
    CHECK(p.offset_val[0][0] == 0);
    CHECK(p.offset_val[0][1] == 6);
    CHECK(p.offset_val[0][2] == 0);
    CHECK(p.offset_val[0][3] == -10);
    CHECK(p.offset_val[0][4] == 4);
}

TEST_CASE("hevc sao: EO 符号由类别固定 [+, +, 0, -, -]") {
    const int abs[4] = {7, 5, 3, 1};
    SaoCtbParams p = edge_params(abs);
    // abs[0]→edgeIdx0 正、abs[1]→edgeIdx1 正、abs[2]→edgeIdx3 负、
    // abs[3]→edgeIdx4 负；edgeIdx2 恒 0，并且 offset_sign 在 EO 下被忽略。
    CHECK(p.offset_val[0][0] == 0);
    CHECK(p.offset_val[0][1] == 7);
    CHECK(p.offset_val[0][2] == 5);
    CHECK(p.offset_val[0][3] == -3);
    CHECK(p.offset_val[0][4] == -1);
    p.offset_sign[0][0] = 1;
    p.offset_sign[0][2] = 1;
    sao_derive_offsets(p);
    CHECK(p.offset_val[0][1] == 7);
    CHECK(p.offset_val[0][3] == -3);
}

TEST_CASE("hevc sao: put/get 对偶，c_idx==2 不写 type bin 且继承 U") {
    SaoCtbParams p;
    for (int c = 0; c < 3; ++c) {
        p.type_idx[c] = kSaoEdge;
        for (int i = 0; i < 4; ++i) p.offset_abs[c][i] = i;
    }
    p.eo_class[0] = 3;
    p.eo_class[1] = 2;
    p.eo_class[2] = 2;  // 编码器保证 U/V 共用 eo_class
    sao_derive_offsets(p);

    std::vector<Bin> bins;
    put_sao_params(bins, p, true, true, false, false, false, false);
    // c_idx 0/1 各 2 bin（type_idx）；3 平面 × 4 × 截断一元；EO 的 eo_class
    // 只在 c_idx!=2 写 2 bin。offset_abs={0,1,2,3} ⇒ 每个 1/2/3/4 bin，共 30。
    CHECK(bins.size() == 2u * 2u + 30u + 2u * 2u);
    int type_bins = 0;
    for (const Bin& b : bins)
        if (b.ctx == kCtxSaoTypeIdx) ++type_bins;
    CHECK(type_bins == 2);  // 若给 c_idx==2 也写了 type，这里会变 3

    BinCursor cur(bins.data(), bins.size());
    SaoCtbParams q;
    REQUIRE(get_sao_params(cur, true, true, false, false, nullptr, nullptr, &q));
    CHECK(cur.consumed() == bins.size());
    for (int c = 0; c < 3; ++c) {
        CHECK(q.type_idx[c] == kSaoEdge);
        for (int i = 0; i < 4; ++i) CHECK(q.offset_abs[c][i] == i);
        for (int i = 0; i < 5; ++i) CHECK(q.offset_val[c][i] == p.offset_val[c][i]);
    }
    CHECK(q.eo_class[0] == 3);
    CHECK(q.eo_class[1] == 2);
    CHECK(q.eo_class[2] == 2);
}

TEST_CASE("hevc sao: merge_left 只写一个 flag 并整体继承邻居") {
    SaoCtbParams left;
    left.type_idx[0] = kSaoBand;
    left.offset_abs[0][0] = 4;
    left.band_position[0] = 11;
    sao_derive_offsets(left);
    SaoCtbParams up;
    up.type_idx[0] = kSaoEdge;
    sao_derive_offsets(up);

    std::vector<Bin> bins;
    put_sao_params(bins, up, true, true, true, true, true, false);
    CHECK(bins.size() == 1u);  // merge_left=1 时 merge_up 也不写
    CHECK(bins[0].ctx == kCtxSaoMergeFlag);
    CHECK(bins[0].val == 1);

    BinCursor cur(bins.data(), bins.size());
    SaoCtbParams q;
    bool ml = false, mu = false;
    REQUIRE(get_sao_params(cur, true, true, true, true, &left, &up, &q, &ml, &mu));
    CHECK(ml);
    CHECK_FALSE(mu);
    CHECK(cur.consumed() == bins.size());
    CHECK(q.type_idx[0] == kSaoBand);
    CHECK(q.offset_val[0][1] == 4);
    CHECK(q.band_position[0] == 11);
    CHECK(q.offset_val[0][2] == 0);
}

TEST_CASE("hevc sao: BO 只改 4 个连续 band") {
    SaoCtbParams p;
    p.type_idx[0] = kSaoBand;
    p.band_position[0] = 12;  // band 12..15 → 值 96..127
    p.offset_abs[0][0] = 4;
    p.offset_abs[0][1] = 3;
    p.offset_sign[0][1] = 1;  // -3
    p.offset_abs[0][2] = 2;
    p.offset_abs[0][3] = 1;
    p.offset_sign[0][3] = 1;  // -1
    sao_derive_offsets(p);

    Plane8 f;
    f.rows({100, 108, 116, 124, 8, 0, 255, 96});
    f.filter(p);
    const int want[8] = {104, 105, 118, 123, 8, 0, 255, 100};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            CHECK(f.dst[static_cast<std::size_t>(y) * 8 + x] == want[x]);
}

TEST_CASE("hevc sao: EO 水平方向逐类别核对 edgeIdx→offset 映射") {
    const int abs[4] = {3, 2, 1, 4};  // offset_val = {0, +3, +2, -1, -4}
    Plane8 f;
    f.rows({100, 110, 100, 90, 100, 100, 120, 60});
    f.filter(edge_params(abs));
    // x=1 cat4 / x=2 cat2 / x=3 cat0 / x=4 cat3 / x=5 cat1 / x=6 cat4；
    // x=0/7 缺邻居，原样输出。
    const int want[8] = {100, 106, 100, 93, 99, 102, 116, 60};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            CHECK(f.dst[static_cast<std::size_t>(y) * 8 + x] == want[x]);
}

TEST_CASE("hevc sao: eo_class 2 是 135°、3 是 45°（方向对调回归）") {
    // src[y][x] = x+y<7 ? 100 : 120：等值线沿 x+y=const，即 45° 方向。
    // 沿 135°（左上↔右下）取样会跨过阶跃，沿 45° 取样两侧同值。
    Plane8 f;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            f.src[static_cast<std::size_t>(y) * 8 + x] = (x + y < 7) ? 100 : 120;

    const int abs[4] = {0, 0, 5, 0};  // 只给 edgeIdx3 一个 -5
    f.filter(edge_params(abs, 3));  // 45°：所有样本 cat2，画面不变
    for (int i = 0; i < 64; ++i) CHECK(f.dst[static_cast<std::size_t>(i)] == f.src[i]);

    f.filter(edge_params(abs, 2));  // 135°：跨阶跃的两条对角线吃 -5
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * 8 + x;
            const bool inside = x >= 1 && x <= 6 && y >= 1 && y <= 6;
            const bool crossed = x + y == 7 || x + y == 8;
            const int want = static_cast<int>(f.src[i]) - ((inside && crossed) ? 5 : 0);
            CHECK(f.dst[i] == want);
        }
    }
}

TEST_CASE("hevc sao: 局部 dst 原点（dst_x0=64）不越界、不按绝对坐标写") {
    constexpr int kPicW = 128, kPicH = 8, kCtbW = 64;
    std::vector<std::uint8_t> src(static_cast<std::size_t>(kPicW) * kPicH, 0);
    for (int y = 0; y < kPicH; ++y)
        for (int x = 0; x < kPicW; ++x)
            src[static_cast<std::size_t>(y) * kPicW + x] = (x % 2 == 0) ? 100 : 110;
    // 尾部 16 B 哨兵：EDGE 若误用 drow[x]（x 到 127）会踩到它们。
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(kCtbW) * kPicH + 16, 0xAA);

    const int abs[4] = {1, 0, 0, 0};  // cat0（局部极小）加 1
    SaoCtbParams p = edge_params(abs, 0);
    sao_filter_ctb(p, 0, kCtbW, 0, kCtbW, kPicH, src.data(), kPicW, dst.data(), kCtbW,
                   kPicW, kPicH, kCtbW, 0);
    for (int y = 0; y < kPicH; ++y) {
        for (int x = 0; x < kCtbW; ++x) {
            const int abs_x = kCtbW + x;
            const int want = (abs_x % 2 == 0 && abs_x >= 1 && abs_x < kPicW - 1)
                                 ? 101
                                 : src[static_cast<std::size_t>(y) * kPicW + abs_x];
            CHECK(dst[static_cast<std::size_t>(y) * kCtbW + x] == want);
        }
    }
    for (std::size_t i = static_cast<std::size_t>(kCtbW) * kPicH; i < dst.size(); ++i)
        CHECK(dst[i] == 0xAA);
}

TEST_CASE("hevc sao: RD 决策必须真的选出滤波（offset 未展开会静默空转）") {
    constexpr int kN = 64;
    std::vector<std::uint8_t> src_y(kN * kN, 100), rec_y(kN * kN, 96);
    const std::size_t cs = static_cast<std::size_t>(kN / 2) * (kN / 2);
    std::vector<std::uint8_t> src_c(cs, 128), rec_c(cs, 128);
    SaoFrameInput in;
    in.width = kN;
    in.height = kN;
    in.src[0] = src_y.data();
    in.src[1] = in.src[2] = src_c.data();
    in.rec[0] = rec_y.data();
    in.rec[1] = in.rec[2] = rec_c.data();
    in.stride[0] = kN;
    in.stride[1] = in.stride[2] = kN / 2;

    SaoTable table;
    compute_sao_table(in, 1.0, 1.0, &table);
    REQUIRE(table.params.size() == 1u);
    CHECK(table.params[0].type_idx[0] != kSaoNotApplied);

    std::vector<std::uint8_t> out_y = rec_y, out_c = rec_c;
    std::uint8_t* dst[3] = {out_y.data(), out_c.data(), out_c.data()};
    const int stride[3] = {kN, kN / 2, kN / 2};
    apply_sao_table(in, table, dst, stride);
    // 均匀 -4 偏差正好落在 BO 的一个 band 内，SAO 应把 Y 完全拉回源值。
    CHECK(std::all_of(out_y.begin(), out_y.end(), [](std::uint8_t v) { return v == 100; }));
}
