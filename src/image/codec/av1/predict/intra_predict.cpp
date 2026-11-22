#include "intra_predict.hpp"

#include <cstring>

namespace termchart::image::codec::av1 {
namespace {

int clip_pixel(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

int round_pow2(int v, int n) { return (v + (1 << (n - 1))) >> n; }

// 非方向模式的基角度无意义，方向模式的基角度固定。
constexpr int kBaseAngle[13] = {0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0};

// smooth 权重表：按块尺寸 4/8/16/32 顺序排布，log2 缩放 8。
constexpr int kSmoothLog2 = 8;
constexpr int kSmoothScale = 1 << kSmoothLog2;
constexpr std::uint8_t kSmoothWeights[] = {
    // bs = 4
    255, 149, 85, 64,
    // bs = 8
    255, 197, 146, 105, 73, 50, 37, 32,
    // bs = 16
    255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
    // bs = 32
    255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92, 83, 74,
    66, 59, 52, 45, 39, 34, 29, 25, 21, 17, 14, 12, 10, 9, 8, 8,
};

const std::uint8_t* smooth_weights_for(int bs) {
    int offset = 0;
    for (int size = 4; size < bs; size <<= 1) offset += size;
    return kSmoothWeights + offset;
}

// 把外部边界样本整理成“稠密且已复制补齐”的形式。
// 与解码端一致：缺 above 用 left[0] 补，缺 left 用 above[0] 补，都缺用 127/129。
void fill_edges(const IntraSamples& s, int w, int h, bool need_above,
                bool need_left, int num_above_needed, int num_left_needed,
                IntraEdgeRows* rows) {
    std::uint8_t* above = rows->above();
    std::uint8_t* left = rows->left();

    if (need_left) {
        if (s.num_left > 0) {
            const int n_copy =
                s.num_left + (s.num_below_left > 0 ? s.num_below_left : 0);
            for (int i = 0; i < n_copy; ++i) left[i] = s.left[i * s.left_stride];
            for (int i = n_copy; i < num_left_needed; ++i) left[i] = left[n_copy - 1];
        } else {
            const int val = s.num_above > 0 ? s.above[0] : 129;
            std::memset(left, val, static_cast<std::size_t>(num_left_needed));
        }
    }

    if (need_above) {
        if (s.num_above > 0) {
            const int n_copy =
                s.num_above + (s.num_above_right > 0 ? s.num_above_right : 0);
            for (int i = 0; i < n_copy; ++i) above[i] = s.above[i];
            for (int i = n_copy; i < num_above_needed; ++i)
                above[i] = above[n_copy - 1];
        } else {
            const int val = s.num_left > 0 ? s.left[0] : 127;
            std::memset(above, val, static_cast<std::size_t>(num_above_needed));
        }
    }

    // 角像素
    int corner = 128;
    if (s.num_above > 0 && s.num_left > 0) {
        corner = s.corner;
    } else if (s.num_above > 0) {
        corner = s.above[0];
    } else if (s.num_left > 0) {
        corner = s.left[0];
    }
    above[-1] = static_cast<std::uint8_t>(corner);
    left[-1] = static_cast<std::uint8_t>(corner);
}

// 方向预测：zone 1 (0 < angle < 90)，只用上边界。
void dr_zone1(std::uint8_t* dst, int stride, int w, int h,
              const std::uint8_t* above, int upsample_above, int dx) {
    const int max_base_x = ((w + h) - 1) << upsample_above;
    const int frac_bits = 6 - upsample_above;
    const int base_inc = 1 << upsample_above;
    int x = dx;
    for (int r = 0; r < h; ++r, dst += stride, x += dx) {
        int base = x >> frac_bits;
        const int shift = ((x << upsample_above) & 0x3F) >> 1;
        if (base >= max_base_x) {
            for (int i = r; i < h; ++i) {
                std::memset(dst, above[max_base_x], static_cast<std::size_t>(w));
                dst += stride;
            }
            return;
        }
        for (int c = 0; c < w; ++c, base += base_inc) {
            if (base < max_base_x) {
                const int val =
                    above[base] * (32 - shift) + above[base + 1] * shift;
                dst[c] = static_cast<std::uint8_t>(round_pow2(val, 5));
            } else {
                dst[c] = above[max_base_x];
            }
        }
    }
}

// 方向预测：zone 2 (90 < angle < 180)，上/左边界都用。
void dr_zone2(std::uint8_t* dst, int stride, int w, int h,
              const std::uint8_t* above, const std::uint8_t* left,
              int upsample_above, int upsample_left, int dx, int dy) {
    const int min_base_x = -(1 << upsample_above);
    const int frac_bits_x = 6 - upsample_above;
    const int frac_bits_y = 6 - upsample_left;

    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int val;
            int y = r + 1;
            int x = (c << 6) - y * dx;
            const int base_x = x >> frac_bits_x;
            if (base_x >= min_base_x) {
                const int shift = ((x * (1 << upsample_above)) & 0x3F) >> 1;
                val = above[base_x] * (32 - shift) + above[base_x + 1] * shift;
                val = round_pow2(val, 5);
            } else {
                x = c + 1;
                y = (r << 6) - x * dy;
                const int base_y = y >> frac_bits_y;
                const int shift = ((y * (1 << upsample_left)) & 0x3F) >> 1;
                val = left[base_y] * (32 - shift) + left[base_y + 1] * shift;
                val = round_pow2(val, 5);
            }
            dst[c] = static_cast<std::uint8_t>(val);
        }
        dst += stride;
    }
}

// 方向预测：zone 3 (180 < angle < 270)，只用左边界。
void dr_zone3(std::uint8_t* dst, int stride, int w, int h,
              const std::uint8_t* left, int upsample_left, int dy) {
    const int max_base_y = ((w + h) - 1) << upsample_left;
    const int frac_bits = 6 - upsample_left;
    const int base_inc = 1 << upsample_left;
    int y = dy;
    for (int c = 0; c < w; ++c, y += dy) {
        int base = y >> frac_bits;
        const int shift = ((y << upsample_left) & 0x3F) >> 1;
        for (int r = 0; r < h; ++r, base += base_inc) {
            if (base < max_base_y) {
                const int val =
                    left[base] * (32 - shift) + left[base + 1] * shift;
                dst[r * stride + c] = static_cast<std::uint8_t>(round_pow2(val, 5));
            } else {
                for (; r < h; ++r) dst[r * stride + c] = left[max_base_y];
                break;
            }
        }
    }
}

}  // namespace

int intra_base_angle(int mode) {
    if (mode < 0 || mode > 12) return 0;
    return kBaseAngle[mode];
}

bool is_directional_mode(int mode) {
    return mode >= 1 && mode <= 8;
}

void predict_dc(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, true, w, h, &rows);

    int sum = 0;
    int count = 0;
    if (s.num_above > 0) {
        for (int c = 0; c < w; ++c) sum += rows.above()[c];
        count += w;
    }
    if (s.num_left > 0) {
        for (int r = 0; r < h; ++r) sum += rows.left()[r];
        count += h;
    }
    const int avg = count > 0 ? ((sum + (count >> 1)) / count) : 128;
    for (int r = 0; r < h; ++r) {
        std::memset(dst + r * stride, avg, static_cast<std::size_t>(w));
    }
}

void predict_v(const IntraSamples& s, int w, int h, std::uint8_t* dst,
               int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, false, w, 0, &rows);
    for (int r = 0; r < h; ++r)
        std::memcpy(dst + r * stride, rows.above(), static_cast<std::size_t>(w));
}

void predict_h(const IntraSamples& s, int w, int h, std::uint8_t* dst,
               int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, false, true, 0, h, &rows);
    for (int r = 0; r < h; ++r)
        std::memset(dst + r * stride, rows.left()[r], static_cast<std::size_t>(w));
}

void predict_paeth(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                   int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, true, w, h, &rows);
    const std::uint8_t* above = rows.above();
    const std::uint8_t* left = rows.left();
    const int top_left = above[-1];
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            const int base = above[c] + left[r] - top_left;
            const int p_left = base > left[r] ? base - left[r] : left[r] - base;
            const int p_top = base > above[c] ? base - above[c] : above[c] - base;
            const int p_tl =
                base > top_left ? base - top_left : top_left - base;
            int out;
            if (p_left <= p_top && p_left <= p_tl) {
                out = left[r];
            } else if (p_top <= p_tl) {
                out = above[c];
            } else {
                out = top_left;
            }
            dst[r * stride + c] = static_cast<std::uint8_t>(out);
        }
    }
}

void predict_smooth(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                    int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, true, w, h, &rows);
    const std::uint8_t* above = rows.above();
    const std::uint8_t* left = rows.left();
    const std::uint8_t below = left[h - 1];
    const std::uint8_t right = above[w - 1];
    const std::uint8_t* wt_w = smooth_weights_for(w);
    const std::uint8_t* wt_h = smooth_weights_for(h);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            const int pred = above[c] * wt_h[r] + below * (kSmoothScale - wt_h[r]) +
                             left[r] * wt_w[c] + right * (kSmoothScale - wt_w[c]);
            dst[r * stride + c] = static_cast<std::uint8_t>(round_pow2(pred, kSmoothLog2 + 1));
        }
    }
}

void predict_smooth_v(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                      int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, true, w, h, &rows);
    const std::uint8_t* above = rows.above();
    const std::uint8_t below = rows.left()[h - 1];
    const std::uint8_t* wt = smooth_weights_for(h);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            const int pred = above[c] * wt[r] + below * (kSmoothScale - wt[r]);
            dst[r * stride + c] = static_cast<std::uint8_t>(round_pow2(pred, kSmoothLog2));
        }
    }
}

void predict_smooth_h(const IntraSamples& s, int w, int h, std::uint8_t* dst,
                      int stride) {
    IntraEdgeRows rows;
    fill_edges(s, w, h, true, true, w, h, &rows);
    const std::uint8_t* left = rows.left();
    const std::uint8_t right = rows.above()[w - 1];
    const std::uint8_t* wt = smooth_weights_for(w);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            const int pred = left[r] * wt[c] + right * (kSmoothScale - wt[c]);
            dst[r * stride + c] = static_cast<std::uint8_t>(round_pow2(pred, kSmoothLog2));
        }
    }
}

// 规范表 7.11.2.3：5 种滤波器，每种 8 组抽头，最后一组恒为 0。
constexpr std::int8_t kFilterIntraTaps[5][8][8] = {
    {{-6, 10, 0, 0, 0, 12, 0, 0}, {-5, 2, 10, 0, 0, 9, 0, 0},
     {-3, 1, 1, 10, 0, 7, 0, 0},  {-3, 1, 1, 2, 10, 5, 0, 0},
     {-4, 6, 0, 0, 0, 2, 12, 0},  {-3, 2, 6, 0, 0, 2, 9, 0},
     {-3, 2, 2, 6, 0, 2, 7, 0},   {-3, 1, 2, 2, 6, 3, 5, 0}},
    {{-10, 16, 0, 0, 0, 10, 0, 0}, {-6, 0, 16, 0, 0, 6, 0, 0},
     {-4, 0, 0, 16, 0, 4, 0, 0},   {-2, 0, 0, 0, 16, 2, 0, 0},
     {-10, 16, 0, 0, 0, 0, 10, 0}, {-6, 0, 16, 0, 0, 0, 6, 0},
     {-4, 0, 0, 16, 0, 0, 4, 0},   {-2, 0, 0, 0, 16, 0, 2, 0}},
    {{-8, 8, 0, 0, 0, 16, 0, 0}, {-8, 0, 8, 0, 0, 16, 0, 0},
     {-8, 0, 0, 8, 0, 16, 0, 0}, {-8, 0, 0, 0, 8, 16, 0, 0},
     {-4, 4, 0, 0, 0, 0, 16, 0}, {-4, 0, 4, 0, 0, 0, 16, 0},
     {-4, 0, 0, 4, 0, 0, 16, 0}, {-4, 0, 0, 0, 4, 0, 16, 0}},
    {{-2, 8, 0, 0, 0, 10, 0, 0}, {-1, 3, 8, 0, 0, 6, 0, 0},
     {-1, 2, 3, 8, 0, 4, 0, 0},  {0, 1, 2, 3, 8, 2, 0, 0},
     {-1, 4, 0, 0, 0, 3, 10, 0}, {-1, 3, 4, 0, 0, 4, 6, 0},
     {-1, 2, 3, 4, 0, 4, 4, 0},  {-1, 2, 2, 3, 4, 3, 3, 0}},
    {{-12, 14, 0, 0, 0, 14, 0, 0}, {-10, 0, 14, 0, 0, 12, 0, 0},
     {-9, 0, 0, 14, 0, 11, 0, 0},  {-8, 0, 0, 0, 14, 10, 0, 0},
     {-10, 12, 0, 0, 0, 0, 14, 0}, {-9, 1, 12, 0, 0, 0, 12, 0},
     {-8, 0, 0, 12, 0, 1, 11, 0},  {-7, 0, 0, 1, 12, 1, 9, 0}},
};

void predict_filter_intra(const IntraSamples& samples, int w, int h, int mode,
                          std::uint8_t* dst, int dst_stride) {
    IntraEdgeRows rows;
    fill_edges(samples, w, h, true, true, w + h, w + h, &rows);
    const std::uint8_t* above = rows.above();
    const std::uint8_t* left = rows.left();
    std::uint8_t buffer[33][33]{};
    for (int r = 0; r < h; ++r) buffer[r + 1][0] = left[r];
    std::memcpy(buffer[0], above - 1, static_cast<std::size_t>(w + 1));

    for (int r = 1; r < h + 1; r += 2) {
        for (int c = 1; c < w + 1; c += 4) {
            const int p0 = buffer[r - 1][c - 1];
            const int p1 = buffer[r - 1][c];
            const int p2 = buffer[r - 1][c + 1];
            const int p3 = buffer[r - 1][c + 2];
            const int p4 = buffer[r - 1][c + 3];
            const int p5 = buffer[r][c - 1];
            const int p6 = buffer[r + 1][c - 1];
            for (int k = 0; k < 8; ++k) {
                const std::int8_t* tap = kFilterIntraTaps[mode][k];
                const int pr = tap[0] * p0 + tap[1] * p1 + tap[2] * p2 +
                              tap[3] * p3 + tap[4] * p4 + tap[5] * p5 +
                              tap[6] * p6;
                buffer[r + (k >> 2)][c + (k & 3)] =
                    static_cast<std::uint8_t>(clip_pixel((pr + 8) >> 4));
            }
        }
    }
    for (int r = 0; r < h; ++r)
        std::memcpy(dst + r * dst_stride, buffer[r + 1] + 1,
                    static_cast<std::size_t>(w));
}

void predict_cfl_chroma(const std::uint8_t* luma, int luma_stride, int w, int h,
                        int alpha_u_q3, int alpha_v_q3,
                        std::uint8_t* u, std::uint8_t* v, int stride) {
    const int n = 4;
    for (int by = 0; by < h; by += n) {
        for (int bx = 0; bx < w; bx += n) {
            int q3[n * n];
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::uint8_t* src =
                        luma + (by * 2 + 2 * y) * luma_stride + bx * 2 + 2 * x;
                    q3[y * n + x] =
                        (src[0] + src[1] + src[luma_stride] + src[luma_stride + 1]) << 1;
                }
            }
            int sum = 8;
            for (int value : q3) sum += value;
            const int avg = sum >> 4;
            for (int plane = 0; plane < 2; ++plane) {
                const int alpha = plane == 0 ? alpha_u_q3 : alpha_v_q3;
                std::uint8_t* dst = (plane == 0 ? u : v) + by * stride + bx;
                for (int y = 0; y < n; ++y) {
                    for (int x = 0; x < n; ++x) {
                        const int ac = q3[y * n + x] - avg;
                        const int product = alpha * ac;
                        const int scaled = product >= 0 ? (product + 32) >> 6
                                                        : -((-product + 32) >> 6);
                        dst[y * stride + x] =
                            static_cast<std::uint8_t>(clip_pixel(128 + scaled));
                    }
                }
            }
        }
    }
}

void predict_intra(const IntraSamples& samples, const IntraPredictParams& params,
                   std::uint8_t* dst, int dst_stride) {
    const int w = params.width;
    const int h = params.height;
    const int mode = params.mode;

    if (!is_directional_mode(mode) && params.filter_intra_mode < 0) {
        switch (mode) {
            case 0: predict_dc(samples, w, h, dst, dst_stride); return;
            case 1: predict_v(samples, w, h, dst, dst_stride); return;
            case 2: predict_h(samples, w, h, dst, dst_stride); return;
            case 9: predict_smooth(samples, w, h, dst, dst_stride); return;
            case 10: predict_smooth_v(samples, w, h, dst, dst_stride); return;
            case 11: predict_smooth_h(samples, w, h, dst, dst_stride); return;
            case 12: predict_paeth(samples, w, h, dst, dst_stride); return;
            default: break;
        }
    }

    // angle_delta 的每一档是 3 度（AV1 angle_delta 语法范围 -3..3）。
    const int p_angle =
        is_directional_mode(mode)
            ? intra_base_angle(mode) + 3 * params.angle_delta
            : 0;
    const bool use_filter_intra = params.filter_intra_mode >= 0;
    const bool need_above = use_filter_intra || p_angle < 180;
    const bool need_left = use_filter_intra || p_angle > 90;

    // 纯垂直/水平方向不需要角像素与边界滤波，直接走 V/H。
    if (!use_filter_intra && (p_angle == 90 || p_angle == 180)) {
        if (p_angle == 90) {
            predict_v(samples, w, h, dst, dst_stride);
        } else {
            predict_h(samples, w, h, dst, dst_stride);
        }
        return;
    }

    const bool need_right = p_angle < 90;
    const bool need_bottom = p_angle > 180;
    const int num_above_needed = need_above ? w + (need_right || use_filter_intra ? h : 0) : 0;
    const int num_left_needed = need_left ? h + (need_bottom || use_filter_intra ? w : 0) : 0;

    IntraEdgeRows rows;
    fill_edges(samples, w, h, need_above, need_left, num_above_needed,
               num_left_needed, &rows);

    if (use_filter_intra) {
        predict_filter_intra(samples, w, h, params.filter_intra_mode, dst,
                             dst_stride);
        return;
    }

    // 亮度块只有在邻居全是非 SMOOTH 模式时才用 type 0；邻居含 SMOOTH 族
    // （或本身就是色度）一律 type 1（libaom get_intra_edge_filter_type）。
    const int edge_type =
        (params.plane != 0 || params.neighbor_smooth) ? 1 : 0;
    int upsample_above = 0;
    int upsample_left = 0;
    if (params.enable_edge_filter) {
        if (need_above && need_left && (w + h >= 24)) {
            filter_intra_edge_corner(rows.above(), rows.left());
        }
        if (need_above && samples.num_above > 0) {
            const int strength = intra_edge_filter_strength(
                w, h, p_angle - 90, edge_type);
            const int n_px =
                samples.num_above + 1 + (need_right ? h : 0);
            filter_intra_edge(rows.above() - 1, n_px, strength);
        }
        if (need_left && samples.num_left > 0) {
            const int strength = intra_edge_filter_strength(
                h, w, p_angle - 180, edge_type);
            const int n_px =
                samples.num_left + 1 + (need_bottom ? w : 0);
            filter_intra_edge(rows.left() - 1, n_px, strength);
        }
        upsample_above =
            use_intra_edge_upsample(w, h, p_angle - 90, edge_type);
        if (need_above && upsample_above) {
            const int n_px = w + (need_right ? h : 0);
            upsample_intra_edge(rows.above(), n_px);
        }
        upsample_left =
            use_intra_edge_upsample(h, w, p_angle - 180, edge_type);
        if (need_left && upsample_left) {
            const int n_px = h + (need_bottom ? w : 0);
            upsample_intra_edge(rows.left(), n_px);
        }
    }

    if (p_angle < 90) {
        dr_zone1(dst, dst_stride, w, h, rows.above(), upsample_above,
                 intra_dx(p_angle));
    } else if (p_angle < 180) {
        dr_zone2(dst, dst_stride, w, h, rows.above(), rows.left(), upsample_above,
                 upsample_left, intra_dx(p_angle), intra_dy(p_angle));
    } else {
        dr_zone3(dst, dst_stride, w, h, rows.left(), upsample_left,
                 intra_dy(p_angle));
    }
}

}  // namespace termchart::image::codec::av1
