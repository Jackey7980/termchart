// test_hevc_interp.cpp —— HEVC 1/4 像素插值滤波器单测
//
// 注：与 FFmpeg hevcdsp 的逐位一致性由 tools/verify_hevc_subpel.py 的外部解码门禁锁定
// （force 分数 MV → recon == ffmpeg dec）。本单测锁住纯函数的结构不变量：
// 整数直通、边界按边缘复制、确定性、线性参考下的单调性。
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "src/media/encoder/hevc/interp.hpp"

namespace {

using namespace termchart::image::codec::hevc;

// 造一张 w×h 参考：ref(x,y) = 16 + x + 2*y（线性梯度，便于验证插值位置）。
std::vector<std::uint8_t> ramp(int w, int h) {
    const auto uw = static_cast<std::size_t>(w);
    std::vector<std::uint8_t> r(uw * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            r[static_cast<std::size_t>(y) * uw + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(16 + x + 2 * y);
    return r;
}

}  // namespace

TEST_CASE("hevc/interp: 整数位置直通 luma/chroma") {
    const auto ref = ramp(32, 32);
    std::uint8_t out[64] = {};
    interp_luma(ref.data(), 32, 32, 4, 4, 0, 0, 8, 8, out, 8);
    for (int j = 0; j < 8; ++j)
        for (int i = 0; i < 8; ++i)
            CHECK(out[j * 8 + i] == ref[static_cast<std::size_t>(4 + j) * 32 +
                                             static_cast<std::size_t>(4 + i)]);

    std::uint8_t cout[16] = {};
    interp_chroma(ref.data(), 32, 32, 4, 4, 0, 0, 4, 4, cout, 4);
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i)
            CHECK(cout[j * 4 + i] ==
                  ref[static_cast<std::size_t>(4 + j) * 32 + static_cast<std::size_t>(4 + i)]);
}

TEST_CASE("hevc/interp: 分数 MV 输出落在参考值域内且确定") {
    const auto ref = ramp(32, 32);
    std::uint8_t a[64] = {}, b[64] = {};
    for (int fy = 0; fy <= 3; ++fy)
        for (int fx = 0; fx <= 3; ++fx) {
            interp_luma(ref.data(), 32, 32, 8, 8, fx, fy, 8, 8, a, 8);
            interp_luma(ref.data(), 32, 32, 8, 8, fx, fy, 8, 8, b, 8);
            for (int k = 0; k < 64; ++k) {
                CHECK(a[k] == b[k]);  // 确定性
                CHECK(a[k] >= 16);    // 线性参考值域下界
                CHECK(a[k] <= 70);    // 上界（16+15+30=61，留插值超调余量）
            }
        }
}

TEST_CASE("hevc/interp: 边界越界抽头按边缘复制，不越界") {
    // 全 100 的常数参考：任意分数位、任意边界锚点都应输出 100（常数保持）。
    std::vector<std::uint8_t> flat(16 * 16, 100);
    for (int y : {0, 15})
        for (int x : {0, 15})
            for (int fy = 0; fy <= 3; ++fy)
                for (int fx = 0; fx <= 3; ++fx) {
                    std::uint8_t out[16] = {};
                    interp_luma(flat.data(), 16, 16, x, y, fx, fy, 4, 4, out, 4);
                    for (int k = 0; k < 16; ++k) CHECK(out[k] == 100);
                }
}

TEST_CASE("hevc/interp: 线性参考下 1/2 像素水平插值近似相邻两点均值") {
    const auto ref = ramp(64, 16);
    std::uint8_t out[8 * 4] = {};  // dst_stride=8 × height=4
    // frac=2（1/2 像素），luma 系数对称 {-1,4,-11,40,40,-11,4,-1}；
    // 线性输入下输出应接近锚点 x 与 x+1 的均值（允许滤波 shape 带来的系统性偏差）。
    interp_luma(ref.data(), 64, 16, 10, 4, 2, 0, 8, 4, out, 8);
    for (int j = 0; j < 4; ++j) {
        const int expect = (ref[static_cast<std::size_t>(4 + j) * 64 + 10] +
                            ref[static_cast<std::size_t>(4 + j) * 64 + 11]) / 2;
        for (int i = 0; i < 8; ++i)
            CHECK(out[j * 8 + i] == doctest::Approx(expect + i).epsilon(0.15));
    }
}
