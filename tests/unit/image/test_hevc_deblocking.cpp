// test_hevc_deblocking.cpp —— HEVC 亮度去块滤波，逐式对拍 FFmpeg
//
// 参考：libavcodec/hevc/filter.c（betatable / tctable / deblocking_filter_CTB）、
// libavcodec/h26x/h2656_deblock_template.c（强/弱滤波）、
// libavcodec/hevc/dsp_template.c:hevc_loop_filter_luma（判据）。
// 期望值全部手工展开 FFmpeg 公式得出，不引用被测实现。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "src/media/encoder/hevc/deblocking.hpp"

namespace {

using namespace termchart::image::codec::hevc;

constexpr int kN = 64;  // 一个 CTB：既能覆盖边界不滤波规则，也不需要多 CTB 拼图

struct Plane {
    std::vector<std::uint8_t> buf;
    Plane() : buf(static_cast<std::size_t>(kN) * kN, 0) {}
    std::uint8_t& at(int x, int y) {
        return buf[static_cast<std::size_t>(y) * kN + static_cast<std::size_t>(x)];
    }
    std::uint8_t get(int x, int y) const {
        return buf[static_cast<std::size_t>(y) * kN + static_cast<std::size_t>(x)];
    }
    // 整帧写同一列剖面：cols[k] 为第 k 列取值，各行一致（水平边界必为平坦 ⇒ 恒等）
    void fill_columns(const std::vector<std::uint8_t>& cols) {
        for (int y = 0; y < kN; ++y)
            for (int x = 0; x < kN; ++x) at(x, y) = cols[static_cast<std::size_t>(x)];
    }
    void run(int qp) { deblock_frame_luma(buf.data(), kN, kN, kN, DeblockConfig{.slice_qp = qp}); }
};

}  // namespace

TEST_CASE("hevc deblocking: beta/tc 表逐项对齐 FFmpeg") {
    // betatable[qp]、tctable[qp + 2*(bs-1)]
    CHECK(deblock_beta(0) == 0);
    CHECK(deblock_beta(17) == 7);
    CHECK(deblock_beta(26) == 16);
    CHECK(deblock_beta(51) == 64);
    CHECK(deblock_tc(26, 2) == 2);   // tctable[28]
    CHECK(deblock_tc(26, 1) == 1);   // tctable[26]
    CHECK(deblock_tc(51, 2) == 24);  // tctable[53]
    CHECK(deblock_tc(10, 2) == 0);   // tctable[12]
    CHECK(deblock_tc(0, 3) == 0);
    CHECK(deblock_tc(51, 3) == 24);  // 越界被 clip 到 53
}

TEST_CASE("hevc deblocking: 平坦画面不被改动（强滤波恒等）") {
    Plane flat;
    flat.fill_columns(std::vector<std::uint8_t>(kN, 120));
    flat.run(51);
    for (int y = 0; y < kN; ++y)
        for (int x = 0; x < kN; ++x) CHECK(flat.get(x, y) == 120);
}

TEST_CASE("hevc deblocking: 强滤波手工展开结果（QP=51）") {
    // cols 4..7 = p3..p0 = 100，cols 8..11 = q0..q3 = 140
    // QP=51 ⇒ beta=64, tc=tctable[53]=24, t=2*tc=48
    //   dp0=dq0=dp3=dq3=0 ⇒ d0+d3=0<64
    //   |p3-p0|+|q3-q0|=0<8，|p0-q0|=40<tc25=60，d<<1=0<16 ⇒ 强滤波
    //   P0=100+clip(15)=115、P1=100+10=110、P2=100+5=105
    //   Q0=140-15=125、Q1=140-10=130、Q2=140-5=135
    Plane p;
    std::vector<std::uint8_t> cols(kN, 140);
    cols[4] = cols[5] = cols[6] = cols[7] = 100;
    p.fill_columns(cols);
    p.run(51);

    CHECK(p.get(4, 3) == 100);  // P3 不被强滤波修改
    CHECK(p.get(5, 3) == 105);  // P2
    CHECK(p.get(6, 3) == 110);  // P1
    CHECK(p.get(7, 3) == 115);  // P0
    CHECK(p.get(8, 3) == 125);  // Q0
    CHECK(p.get(9, 3) == 130);  // Q1
    CHECK(p.get(10, 3) == 135); // Q2
    CHECK(p.get(11, 3) == 140); // Q3 不被强滤波修改
    // 8 行剖面一致 ⇒ 每行结果相同（沿边界方向逐行施加）
    for (int y = 0; y < 8; ++y) CHECK(p.get(7, y) == 115);
}

TEST_CASE("hevc deblocking: 弱滤波手工展开结果（QP=40）") {
    // cols 4..7 = p3..p0 = 100,110,115,120；cols 8..11 = q0..q3 = 130,135,140,150
    // QP=40 ⇒ beta=betatable[40]=42, tc=tctable[42]=7, tc_2=3
    //   dp0=dq0=0 ⇒ d0+d3=0<42
    //   强滤波条件 |p3-p0|+|q3-q0|=20+20=40 ≥ beta_3=5 ⇒ 走弱滤波
    //   nd_p=nd_q=2（dp0+dp3=0 < (42+21)>>3=7）
    //   delta0=(9*10-3*20+8)>>4=2 → P0=122, Q0=128
    //   deltap1=(((110+120+1)>>1)-115+2)>>1=1 → P1=116
    //   deltaq1=(((140+130+1)>>1)-135-2)>>1=-1 → Q1=134
    Plane p;
    std::vector<std::uint8_t> cols(kN, 150);
    cols[4] = 100;
    cols[5] = 110;
    cols[6] = 115;
    cols[7] = 120;
    cols[8] = 130;
    cols[9] = 135;
    cols[10] = 140;
    cols[11] = 150;
    p.fill_columns(cols);
    p.run(40);

    CHECK(p.get(4, 5) == 100);
    CHECK(p.get(5, 5) == 110);  // P2 弱滤波不修改
    CHECK(p.get(6, 5) == 116);  // P1
    CHECK(p.get(7, 5) == 122);  // P0
    CHECK(p.get(8, 5) == 128);  // Q0
    CHECK(p.get(9, 5) == 134);  // Q1
    CHECK(p.get(10, 5) == 140); // Q2 弱滤波不修改
    CHECK(p.get(11, 5) == 150);
}

TEST_CASE("hevc deblocking: 低 QP 时 tc==0，整帧不动") {
    Plane p;
    std::vector<std::uint8_t> cols(kN, 0);
    for (int x = 4; x < 8; ++x) cols[static_cast<std::size_t>(x)] = 0;
    for (int x = 8; x < kN; ++x) cols[static_cast<std::size_t>(x)] = 255;
    p.fill_columns(cols);
    p.run(10);  // tctable[12]==0
    for (int y = 0; y < kN; ++y) {
        CHECK(p.get(7, y) == 0);
        CHECK(p.get(8, y) == 255);
    }
}

TEST_CASE("hevc deblocking: 左边界 x=0 处不滤波") {
    // 8.7.2.5：左/上 picture 边界不参与滤波（否则会读到图像外的 P3/P2/P1/P0）。
    Plane p;
    std::vector<std::uint8_t> cols(kN, 240);
    for (int x = 0; x < 4; ++x) cols[static_cast<std::size_t>(x)] = 10;
    p.fill_columns(cols);  // x=4 处台阶：若实现对 x=0 越界滤波，第 0 列会被改写
    p.run(51);
    for (int y = 0; y < kN; ++y) CHECK(p.get(0, y) == 10);
}

TEST_CASE("hevc deblocking: 水平方向的强滤波（xstride=stride 通路）") {
    // 行 0..7 = 100，行 8..63 = 140 ⇒ y=8 处水平台阶；QP=51 参数同垂直强滤波用例
    //   P0(行7)=115、P1(行6)=110、P2(行5)=105、Q0(行8)=125、Q1(行9)=130、Q2(行10)=135
    Plane p;
    for (int y = 0; y < kN; ++y)
        for (int x = 0; x < kN; ++x) p.at(x, y) = (y < 8) ? 100 : 140;
    p.run(51);

    CHECK(p.get(30, 0) == 100);   // 上边界之外的 P3 行不被改写
    CHECK(p.get(30, 4) == 100);   // P3
    CHECK(p.get(30, 5) == 105);   // P2
    CHECK(p.get(30, 6) == 110);   // P1
    CHECK(p.get(30, 7) == 115);   // P0
    CHECK(p.get(30, 8) == 125);   // Q0
    CHECK(p.get(30, 9) == 130);   // Q1
    CHECK(p.get(30, 10) == 135);  // Q2
    CHECK(p.get(30, 11) == 140);  // Q3
}

// ── 2026-11-13 补：色度通路 + 固定大 TU 边界周期 + 自动档真实 TU 边界图 ──
// 这三项是「开 DBF 时 recon == dec」的前提；期望值同样手工展开公式，
// 端到端逐像素对拍由 tools/verify_hevc_tu_sizes.py --dblock-on 覆盖。

TEST_CASE("hevc deblocking: 色度 bS=2 弱滤波手工展开（QPc=26）") {
    Plane u;
    std::vector<std::uint8_t> cols(kN, 100);
    for (int x = 8; x < 12; ++x) cols[static_cast<std::size_t>(x)] = 110;
    u.fill_columns(cols);
    std::vector<std::uint8_t> v = u.buf;  // V 走同一套公式

    DeblockConfig cfg;
    cfg.slice_qp = 26;
    cfg.chroma_qp = 26;  // tc = kTcTable[26 + 2] = 2
    cfg.edge_period = 8;
    deblock_frame_chroma(u.buf.data(), v.data(), kN, kN, kN, cfg);

    // x=8 垂直边界：p1=100, p0=100, q0=110, q1=110
    // delta = clip3(-2, 2, (((110-100)<<2) + 100 - 110 + 4) >> 3) = clip(4) = 2
    CHECK(u.get(7, 3) == 102);
    CHECK(u.get(8, 3) == 108);
    CHECK(v[3 * kN + 7] == 102);
    CHECK(v[3 * kN + 8] == 108);
    // 色度只改 p0/q0；其它 8 对齐边界两侧同值 ⇒ delta=0
    CHECK(u.get(6, 3) == 100);
    CHECK(u.get(9, 3) == 110);
    CHECK(u.get(16, 3) == 100);
}

TEST_CASE("hevc deblocking: 16/32 TU 档内部 8 对齐边不滤波") {
    auto make_step = [] {
        Plane p;
        std::vector<std::uint8_t> cols(kN, 140);
        for (int x = 0; x < 8; ++x) cols[static_cast<std::size_t>(x)] = 100;
        p.fill_columns(cols);
        return p;
    };
    Plane a = make_step(), b = make_step();
    DeblockConfig period8, period16;
    period8.slice_qp = 51;
    period8.edge_period = 8;   // 4x4/8x8 TU：x=8 是真 TU 边界
    period16.slice_qp = 51;
    period16.edge_period = 16; // 16x16 TU：x=8 是 TU 内部，不该滤
    deblock_frame_luma(a.buf.data(), kN, kN, kN, period8);
    deblock_frame_luma(b.buf.data(), kN, kN, kN, period16);

    CHECK(a.get(7, 3) != 100);  // 周期 8：强滤波改了 P0
    CHECK(b.get(7, 3) == 100);  // 周期 16：整帧不动
    CHECK(b.get(8, 3) == 140);
}

TEST_CASE("hevc deblocking: 自动档按真实 TU 边界图只滤标记的边") {
    auto make_step = [] {
        Plane p;
        std::vector<std::uint8_t> cols(kN, 140);
        for (int x = 0; x < 8; ++x) cols[static_cast<std::size_t>(x)] = 100;
        p.fill_columns(cols);
        return p;
    };
    constexpr int kGrid = kN / 8;
    const std::size_t cells = static_cast<std::size_t>(kGrid) * kGrid;
    std::vector<std::uint8_t> ve(cells, 0), he(cells, 0), zero(cells, 0);
    for (int gy = 0; gy < kGrid; ++gy) ve[gy * kGrid + 1] = 1;  // 只标 x=8 这条边

    Plane mapped = make_step(), unmapped = make_step();
    DeblockConfig cfg;
    cfg.slice_qp = 51;
    cfg.edge_period = 8;
    cfg.vert_edge = ve.data();
    cfg.horz_edge = he.data();
    cfg.grid_w = kGrid;
    deblock_frame_luma(mapped.buf.data(), kN, kN, kN, cfg);

    cfg.vert_edge = zero.data();
    deblock_frame_luma(unmapped.buf.data(), kN, kN, kN, cfg);

    CHECK(mapped.get(7, 3) != 100);   // 标记的边被滤
    CHECK(unmapped.get(7, 3) == 100); // 全 0 图：一条边都不滤
    CHECK(unmapped.get(8, 3) == 140);
}

