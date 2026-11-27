#include "deblock.hpp"

#include <algorithm>
#include <cstdlib>

namespace termchart::image::codec::av1 {
namespace {

int clip_pixel(int v) { return std::clamp(v, 0, 255); }
int clip_diff(int v) { return std::clamp(v, -128, 127); }

void filter_segment(std::uint8_t* dst, int stridea, int strideb, int wd,
                    int E, int I, int H) {
    for (int i = 0; i < 4; ++i, dst += stridea) {
        int p1 = dst[-2 * strideb], p0 = dst[-strideb];
        int q0 = dst[0], q1 = dst[strideb];
        int p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0;
        int q2 = 0, q3 = 0, q4 = 0, q5 = 0, q6 = 0;

        bool fm = std::abs(p1 - p0) <= I && std::abs(q1 - q0) <= I &&
                  std::abs(p0 - q0) * 2 + (std::abs(p1 - q1) >> 1) <= E;
        if (wd > 4) {
            p2 = dst[-3 * strideb];
            q2 = dst[2 * strideb];
            fm &= std::abs(p2 - p1) <= I && std::abs(q2 - q1) <= I;
            if (wd > 6) {
                p3 = dst[-4 * strideb];
                q3 = dst[3 * strideb];
                fm &= std::abs(p3 - p2) <= I && std::abs(q3 - q2) <= I;
            }
        }
        if (!fm) continue;

        bool flat8out = false, flat8in = false;
        if (wd >= 16) {
            p6 = dst[-7 * strideb]; p5 = dst[-6 * strideb]; p4 = dst[-5 * strideb];
            q4 = dst[4 * strideb]; q5 = dst[5 * strideb]; q6 = dst[6 * strideb];
            flat8out = std::abs(p6 - p0) <= 1 && std::abs(p5 - p0) <= 1 &&
                       std::abs(p4 - p0) <= 1 && std::abs(q4 - q0) <= 1 &&
                       std::abs(q5 - q0) <= 1 && std::abs(q6 - q0) <= 1;
        }
        if (wd >= 6)
            flat8in = std::abs(p2 - p0) <= 1 && std::abs(p1 - p0) <= 1 &&
                      std::abs(q1 - q0) <= 1 && std::abs(q2 - q0) <= 1;
        if (wd >= 8)
            flat8in &= std::abs(p3 - p0) <= 1 && std::abs(q3 - q0) <= 1;

        if (wd >= 16 && flat8out && flat8in) {
            dst[-6 * strideb] = (p6*7 + p5*2 + p4*2 + p3 + p2 + p1 + p0 + q0 + 8) >> 4;
            dst[-5 * strideb] = (p6*5 + p5*2 + p4*2 + p3*2 + p2 + p1 + p0 + q0 + q1 + 8) >> 4;
            dst[-4 * strideb] = (p6*4 + p5 + p4*2 + p3*2 + p2*2 + p1 + p0 + q0 + q1 + q2 + 8) >> 4;
            dst[-3 * strideb] = (p6*3 + p5 + p4 + p3*2 + p2*2 + p1*2 + p0 + q0 + q1 + q2 + q3 + 8) >> 4;
            dst[-2 * strideb] = (p6*2 + p5 + p4 + p3 + p2*2 + p1*2 + p0*2 + q0 + q1 + q2 + q3 + q4 + 8) >> 4;
            dst[-1 * strideb] = (p6 + p5 + p4 + p3 + p2 + p1*2 + p0*2 + q0*2 + q1 + q2 + q3 + q4 + q5 + 8) >> 4;
            dst[0]              = (p5 + p4 + p3 + p2 + p1 + p0*2 + q0*2 + q1*2 + q2 + q3 + q4 + q5 + q6 + 8) >> 4;
            dst[1 * strideb]  = (p4 + p3 + p2 + p1 + p0 + q0*2 + q1*2 + q2*2 + q3 + q4 + q5 + q6*2 + 8) >> 4;
            dst[2 * strideb]  = (p3 + p2 + p1 + p0 + q0 + q1*2 + q2*2 + q3*2 + q4 + q5 + q6*3 + 8) >> 4;
            dst[3 * strideb]  = (p2 + p1 + p0 + q0 + q1 + q2*2 + q3*2 + q4*2 + q5 + q6*4 + 8) >> 4;
            dst[4 * strideb]  = (p1 + p0 + q0 + q1 + q2 + q3*2 + q4*2 + q5*2 + q6*5 + 8) >> 4;
            dst[5 * strideb]  = (p0 + q0 + q1 + q2 + q3 + q4*2 + q5*2 + q6*6 + 8) >> 4;
        } else if (wd >= 8 && flat8in) {
            dst[-3 * strideb] = (p3*3 + p2*2 + p1 + p0 + q0 + 4) >> 3;
            dst[-2 * strideb] = (p3*2 + p2 + p1*2 + p0 + q0 + q1 + 4) >> 3;
            dst[-1 * strideb] = (p3 + p2 + p1 + p0*2 + q0 + q1 + q2 + 4) >> 3;
            dst[0]              = (p2 + p1 + p0 + q0*2 + q1 + q2 + q3 + 4) >> 3;
            dst[1 * strideb]  = (p1 + p0 + q0 + q1*2 + q2 + q3*2 + 4) >> 3;
            dst[2 * strideb]  = (p0 + q0 + q1 + q2*2 + q3*3 + 4) >> 3;
        } else if (wd == 6 && flat8in) {
            dst[-2 * strideb] = (p2*3 + p1*2 + p0*2 + q0 + 4) >> 3;
            dst[-1 * strideb] = (p2*2 + p1*2 + p0*2 + q0 + q1 + 4) >> 3;
            dst[0]              = (p1*2 + p0*2 + q0*2 + q1 + q2 + 4) >> 3;
            dst[1 * strideb]  = (p0*2 + q0*2 + q1*2 + q2*2 + 4) >> 3;
        } else {
            const bool hev = std::abs(p1 - p0) > H || std::abs(q1 - q0) > H;
            if (hev) {
                int f = clip_diff(p1 - q1);
                f = clip_diff(3 * (q0 - p0) + f);
                const int f1 = std::min(f + 4, 127) >> 3;
                const int f2 = std::min(f + 3, 127) >> 3;
                dst[-strideb] = clip_pixel(p0 + f2);
                dst[0] = clip_pixel(q0 - f1);
            } else {
                int f = clip_diff(3 * (q0 - p0));
                const int f1 = std::min(f + 4, 127) >> 3;
                const int f2 = std::min(f + 3, 127) >> 3;
                dst[-strideb] = clip_pixel(p0 + f2);
                dst[0] = clip_pixel(q0 - f1);
                f = (f1 + 1) >> 1;
                dst[-2 * strideb] = clip_pixel(p1 + f);
                dst[strideb] = clip_pixel(q1 - f);
            }
        }
    }
}

}  // namespace

void av1_deblock_edge(std::uint8_t* buf, int stride, int x, int y,
                      bool vertical, int length, int width,
                      const Av1DeblockParams& params) {
    const int level = std::clamp(params.level, 0, 63);
    if (level == 0) return;
    const int sharp = std::clamp(params.sharpness, 0, 7);
    int limit = level;
    if (sharp > 0) {
        limit >>= (sharp + 3) >> 2;
        limit = std::min(limit, 9 - sharp);
    }
    limit = std::max(limit, 1);
    const int E = 2 * (level + 2) + limit;
    const int I = limit;
    const int H = level >> 4;
    const int wd = width <= 4 ? 4 : width <= 6 ? 6 : width <= 8 ? 8 : 16;
    const int count = length / 4;
    for (int i = 0; i < count; ++i) {
        std::uint8_t* p = vertical ? buf + y * stride + x + i * 4
                                   : buf + y * stride + x + i;
        filter_segment(p, vertical ? stride : 1, vertical ? 1 : stride,
                       wd, E, I, H);
    }
}

void av1_deblock_luma_grid(std::uint8_t* y, int stride, int w, int h,
                           const Av1DeblockParams& params) {
    for (int x = 8; x < w; x += 8)
        for (int sy = 0; sy + 4 <= h; sy += 4)
            av1_deblock_edge(y, stride, x, sy, true, 4, 4, params);
    for (int yy = 8; yy < h; yy += 8)
        for (int sx = 0; sx + 4 <= w; sx += 4)
            av1_deblock_edge(y, stride, sx, yy, false, 4, 4, params);
}

void av1_deblock_chroma_grid(std::uint8_t* plane, int stride, int w, int h,
                             const Av1DeblockParams& params) {
    for (int x = 4; x < w; x += 4)
        for (int sy = 0; sy + 4 <= h; sy += 4)
            av1_deblock_edge(plane, stride, x, sy, true, 4, 4, params);
    for (int yy = 4; yy < h; yy += 4)
        for (int sx = 0; sx + 4 <= w; sx += 4)
            av1_deblock_edge(plane, stride, sx, yy, false, 4, 4, params);
}

}  // namespace termchart::image::codec::av1
