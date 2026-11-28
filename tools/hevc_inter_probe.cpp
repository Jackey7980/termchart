// hevc_inter_probe.cpp —— 帧间（P 片）探针（供 tools/verify_hevc_inter.py 编译）
//
// argv = <qp> <W> <H> <形态> <输出.265> [帧数] [motion_search]
//   形态：still / shift2 / mixed / round / fade / noise
//   帧数：默认 5（首帧 IDR + 其后 P 片）
//   motion_search：1=开整数运动搜索（默认），0=只用 merge 候选（验证 skip 语法）
// 生成多帧码流（首帧 IDR + 其后 P 片），并落盘完整 yuv420p 源（同名 .yuv）：
// 每帧 W*H 的 Y 后接 (W/2)*(H/2) 的 U、V，供 ffmpeg 解码对拍与 PSNR 统计。
// W/H 必须是 64 的倍数（编码树按 CTB 遍历，且不引入 conformance window 位移）。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/hevc_encode.hpp"

using namespace termchart::image;

static int cl(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// 静止纹理：与 tools/hevc_tu_probe.cpp 的 grad 同构（渐变 + 8x8 棋盘细节），
// seed 只参与棋盘相位，便于制造「同内容不同相位」的帧间差异。
static void tex_pixel(int u, int v, int W, int H, int seed, int* r, int* g, int* b) {
    const int grad = (u * 200) / (W - 1) + 20;
    const int t = (((u / 8) + (v / 8) + seed) & 1) ? 14 : -14;
    *r = grad + t;
    *g = grad / 2 + t / 2 + 40;
    *b = (v * 120) / (H - 1) + grad / 3;
}

int main(int argc, char** argv) {
    const int qp = argc > 1 ? std::atoi(argv[1]) : 26;
    const int W = argc > 2 ? std::atoi(argv[2]) : 128;
    const int H = argc > 3 ? std::atoi(argv[3]) : 64;
    const char* shape = argc > 4 ? argv[4] : "shift2";
    const char* out265 = argc > 5 ? argv[5] : "tc_inter.265";
    const int nframes = argc > 6 ? std::atoi(argv[6]) : 5;
    const bool motion_search = argc > 7 ? (std::atoi(argv[7]) != 0) : true;
    if (W % 64 || H % 64 || W <= 0 || H <= 0 || nframes <= 0) {
        std::fprintf(stderr, "size must be 64-multiple, frames>0\n");
        return 2;
    }

    const int cw = W / 2, chh = H / 2;
    std::vector<std::uint8_t> yuv;
    yuv.reserve(static_cast<std::size_t>(W) * H * 3 / 2 * nframes);
    std::vector<Bitmap> frames;
    frames.reserve(static_cast<std::size_t>(nframes));
    for (int f = 0; f < nframes; ++f) {
        Bitmap bmp(W, H, 4);
        std::vector<std::uint8_t> yplane(static_cast<std::size_t>(W) * H);
        std::vector<std::uint8_t> uplane(static_cast<std::size_t>(cw) * chh);
        std::vector<std::uint8_t> vplane(static_cast<std::size_t>(cw) * chh);
        for (int yy = 0; yy < H; ++yy) {
            for (int xx = 0; xx < W; ++xx) {
                int u = xx, v = yy, seed = 0, boost = 0;
                if (!std::strcmp(shape, "flat")) {
                    // 全平：帧内 DC 预测与源完全一致 ⇒ 残差恒为 0，只有极少 bin，
                    // 用于把「CU 前缀语法」与「残差语法」两类问题分开定位。
                    const int R = 128, G = 128, B = 128;
                    bmp.set(xx, yy, 0, static_cast<std::uint8_t>(R));
                    bmp.set(xx, yy, 1, static_cast<std::uint8_t>(G));
                    bmp.set(xx, yy, 2, static_cast<std::uint8_t>(B));
                    yplane[static_cast<std::size_t>(yy) * W + xx] =
                        static_cast<std::uint8_t>(cl(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16));
                    if ((xx & 1) == 0 && (yy & 1) == 0) {
                        const std::size_t ci = static_cast<std::size_t>(yy / 2) * cw + (xx / 2);
                        uplane[ci] = static_cast<std::uint8_t>(
                            cl(((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128));
                        vplane[ci] = static_cast<std::uint8_t>(
                            cl(((112 * R - 94 * G - 18 * B + 128) >> 8) + 128));
                    }
                    continue;
                } else if (!std::strcmp(shape, "still")) {
                    // 静止：理想情况全部 skip
                } else if (!std::strcmp(shape, "shift2")) {
                    // 整帧每帧右移 2 亮度采样（MV 步长 kMvStep=8 正好覆盖）
                    u = xx - 2 * f;
                    if (u < 0) u = 0;
                } else if (!std::strcmp(shape, "mixed")) {
                    // 左半静止（应大量 skip）+ 右半每帧右移 2 采样（应走 AMVP）
                    if (xx >= W / 2) {
                        u = xx - 2 * f;
                        if (u < W / 2) u = W / 2;
                    }
                } else if (!std::strcmp(shape, "round")) {
                    // 双向移动：亮度/色度的 x、y 位移都要参与
                    u = xx - 2 * f;
                    v = yy - 2 * f;
                    if (u < 0) u = 0;
                    if (v < 0) v = 0;
                } else if (!std::strcmp(shape, "cycle2")) {
                    // 两帧亮度周期：偶数帧 boost=0、奇数帧 boost=80。参考前一帧必然
                    // 亮度叉开 80（任何 MV 都匹配不上），参考 POC-2（同相）MV=0 完美，
                    // 强制 ref_idx=1 胜出——实证多参考列表真被用上（非空壳语法）。
                    boost = (f % 2 == 1) ? 80 : 0;
                } else if (!std::strcmp(shape, "fade")) {
                    boost = 6 * f;  // 整体亮度台阶：inter 拼不过时须退化成帧内
                } else if (!std::strcmp(shape, "noise")) {
                    // 每帧独立噪声：帧间基本无解，最大化帧内退化路径覆盖面
                    std::uint32_t s =
                        0x9e3779b9u ^ (static_cast<std::uint32_t>(xx) * 2654435761u) ^
                        (static_cast<std::uint32_t>(yy) * 40503u) ^
                        (static_cast<std::uint32_t>(f) * 2246822519u);
                    s ^= s >> 13;
                    s *= 1274126177u;
                    s ^= s >> 16;
                    const int n = static_cast<int>(s % 64u) - 32;
                    int r, g, b;
                    tex_pixel(xx, yy, W, H, 0, &r, &g, &b);
                    r += n;
                    g += n;
                    b += n;
                    const int R = cl(r), G = cl(g), B = cl(b);
                    bmp.set(xx, yy, 0, static_cast<std::uint8_t>(R));
                    bmp.set(xx, yy, 1, static_cast<std::uint8_t>(G));
                    bmp.set(xx, yy, 2, static_cast<std::uint8_t>(B));
                    yplane[static_cast<std::size_t>(yy) * W + xx] =
                        static_cast<std::uint8_t>(cl(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16));
                    if ((xx & 1) == 0 && (yy & 1) == 0) {
                        const std::size_t ci = static_cast<std::size_t>(yy / 2) * cw + (xx / 2);
                        uplane[ci] = static_cast<std::uint8_t>(
                            cl(((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128));
                        vplane[ci] = static_cast<std::uint8_t>(
                            cl(((112 * R - 94 * G - 18 * B + 128) >> 8) + 128));
                    }
                    continue;
                } else if (!std::strcmp(shape, "code")) {
                    // 8x8 块常量"编码图"：每块一个唯一灰度值。任何错位拷贝都会
                    // 落到另一块的常量值上，帧内预测则会给出非块常量的平滑值，
                    // 因此可以直接从解码值反推"解帧端在这个 CU 到底做了什么"。
                    const int bw = W / 8;
                    const int idx = (yy >> 3) * bw + (xx >> 3);
                    const int val = 4 * (idx & 63);  // 0,4,...,252 块内常量
                    bmp.set(xx, yy, 0, static_cast<std::uint8_t>(val));
                    bmp.set(xx, yy, 1, static_cast<std::uint8_t>(val));
                    bmp.set(xx, yy, 2, static_cast<std::uint8_t>(val));
                    yplane[static_cast<std::size_t>(yy) * W + xx] = static_cast<std::uint8_t>(
                        cl(((66 * val + 129 * val + 25 * val + 128) >> 8) + 16));
                    if ((xx & 1) == 0 && (yy & 1) == 0) {
                        uplane[static_cast<std::size_t>(yy / 2) * cw + (xx / 2)] = 128;
                        vplane[static_cast<std::size_t>(yy / 2) * cw + (xx / 2)] = 128;
                    }
                    continue;
                }

                int r, g, b;
                tex_pixel(u, v, W, H, seed, &r, &g, &b);
                r += boost;
                g += boost;
                b += boost;
                const int R = cl(r), G = cl(g), B = cl(b);
                bmp.set(xx, yy, 0, static_cast<std::uint8_t>(R));
                bmp.set(xx, yy, 1, static_cast<std::uint8_t>(G));
                bmp.set(xx, yy, 2, static_cast<std::uint8_t>(B));
                // 与 src/media/image/hevc_encode.cpp::to_yuv 同一套 BT.601 系数
                yplane[static_cast<std::size_t>(yy) * W + xx] =
                    static_cast<std::uint8_t>(cl(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16));
                if ((xx & 1) == 0 && (yy & 1) == 0) {
                    const std::size_t ci = static_cast<std::size_t>(yy / 2) * cw + (xx / 2);
                    uplane[ci] = static_cast<std::uint8_t>(
                        cl(((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128));
                    vplane[ci] = static_cast<std::uint8_t>(
                        cl(((112 * R - 94 * G - 18 * B + 128) >> 8) + 128));
                }
            }
        }
        yuv.insert(yuv.end(), yplane.begin(), yplane.end());
        yuv.insert(yuv.end(), uplane.begin(), uplane.end());
        yuv.insert(yuv.end(), vplane.begin(), vplane.end());
        frames.push_back(std::move(bmp));
    }

    std::string yuv_path = out265;
    yuv_path += ".yuv";
    std::FILE* fp = std::fopen(yuv_path.c_str(), "wb");
    if (!fp) {
        std::fprintf(stderr, "cannot write %s\n", yuv_path.c_str());
        return 2;
    }
    std::fwrite(yuv.data(), 1, yuv.size(), fp);
    std::fclose(fp);

    EncodeOptions opts;
    opts.video_mode = VideoEncodeMode::Inter;
    opts.video_qp = qp;
    (void)motion_search;  // 运动搜索由 encode_hevc_annexb 的 Inter 分支固定开启
    // 亚像素精搜开关（默认关）；verify_hevc_subpel.py 之外的端到端验证可用它。
    opts.hevc_subpel_me = std::getenv("TERMCHART_HEVC_INTER_SUBPEL") != nullptr;
    // 多参考开关（默认关）；verify_hevc_multiref.py 用它验证 RPS/ref_idx 一致性。
    opts.hevc_multi_ref = std::getenv("TERMCHART_HEVC_INTER_MULTIREF") != nullptr;
    // 双向 B 片（I0 P2 B1 P4 B3 ...）门禁开关，默认关以保既有探针字节级基线。
    opts.hevc_b_slice = std::getenv("TERMCHART_HEVC_INTER_BSLICE") != nullptr;
    opts.hevc_b_amvp = std::getenv("TERMCHART_HEVC_B_AMVP") != nullptr;
    const auto bs = encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
    if (bs.empty()) {
        std::fprintf(stderr, "encode failed qp=%d %dx%d %s frames=%d\n", qp, W, H, shape,
                     nframes);
        return 1;
    }
    fp = std::fopen(out265, "wb");
    if (!fp) {
        std::fprintf(stderr, "cannot write %s\n", out265);
        return 2;
    }
    std::fwrite(bs.data(), 1, bs.size(), fp);
    std::fclose(fp);
    std::printf("%zu\n", bs.size());
    return 0;
}
