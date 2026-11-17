// hevc_tu_probe.cpp —— TU 尺寸档位矩阵探针（供 tools/verify_hevc_tu_sizes.py 编译）
//
// argv = <qp> <W> <H> <形态> <输出.265> [log2_tu] [transform_skip]
//   log2_tu: 0=自动档 / 2=4x4 / 3=8x8 / 4=16x16 / 5=32x32（默认 2）
//   transform_skip: 1=开（大 TU 会写 RExt 参数集，见 hevc_encode.cpp）/ 0=关（默认）
//   sao: 1=开 SAO（SPS 声明 + 每 CTB 写 sao() 语法）/ 0=关（默认，保持字节级基线）
//   rdo: 1=开帧内模式 RDO（SSD + λ·bins）/ 0=关（默认，纯 SSD）
// 生成一帧 Intra 码流，并落盘完整 yuv420p 源（同名 .yuv），供 ffmpeg 解码对拍。
// W/H 必须是 64 的倍数（编码树按 CTB 遍历）。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/hevc_encode.hpp"

using namespace termchart::image;

static std::uint8_t cl(int v) { return (std::uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

int main(int argc, char** argv) {
    const int qp = argc > 1 ? std::atoi(argv[1]) : 26;
    const int W = argc > 2 ? std::atoi(argv[2]) : 128;
    const int H = argc > 3 ? std::atoi(argv[3]) : 64;
    const char* mode = argc > 4 ? argv[4] : "grad";
    const char* out265 = argc > 5 ? argv[5] : "tc_probe.265";
    if (W % 64 || H % 64 || W <= 0 || H <= 0) { std::fprintf(stderr, "size must be 64-multiple\n"); return 2; }

    Bitmap bmp(W, H, 4);
    // 写完整 yuv420p（前 W*H 字节仍是 Y 平面，既有门禁脚本不受影响）；
    // 后半段 U/V 供色度残差验证脚本与 ffmpeg 解码输出逐像素对拍。
    std::vector<std::uint8_t> yuv((size_t)W * H * 3 / 2, 128);
    for (int yy = 0; yy < H; ++yy)
        for (int xx = 0; xx < W; ++xx) {
            int r, g, b;
            if (!std::strcmp(mode, "flat")) {
                r = g = b = 131;
            } else if (!std::strcmp(mode, "checker")) {
                r = g = b = (((xx >> 1) ^ (yy >> 1)) & 1) ? 136 : 120;
            } else if (!std::strcmp(mode, "hstep")) {
                r = 30 + (xx / 8) * 14; g = 128; b = 128;
            } else if (!std::strcmp(mode, "one8")) {
                const int on = (xx < 8 && yy < 8);
                r = g = b = on ? 200 : 0;
            } else if (!std::strcmp(mode, "dots")) {
                // 类屏幕内容：平坦底 + 稀疏亮点。像素域残差稀疏、变换域被摊开，
                // 正是 transform_skip 该赢的形态（自适应档应能自己选中它）。
                const bool on = ((xx * 7 + yy * 13) % 29 == 0) || (xx % 32 == 5 && yy % 16 == 3);
                r = g = b = on ? 210 : 126;
            } else if (!std::strcmp(mode, "one4")) {
                const int on = (xx < 4 && yy < 4);
                r = g = b = on ? 200 : 0;
            } else if (!std::strcmp(mode, "ramp4")) {
                const int on = (xx < 4 && yy < 4);
                r = g = b = on ? (40 + xx * 20) : 0;
            } else {  // grad
                const int grad = (xx * 200) / (W - 1) + 20;
                const int tex = (((xx / 8) + (yy / 8)) & 1) ? 14 : -14;
                r = grad + tex; g = grad / 2 + tex / 2 + 40;
                b = (yy * 120) / (H - 1) + grad / 3;
            }
            const int R = cl(r), G = cl(g), B = cl(b);
            bmp.set(xx, yy, 0, (std::uint8_t)R);
            bmp.set(xx, yy, 1, (std::uint8_t)G);
            bmp.set(xx, yy, 2, (std::uint8_t)B);
            yuv[(size_t)yy * W + xx] = cl(((66 * R + 129 * G + 25 * B + 128) >> 8) + 16);
            if ((xx & 1) == 0 && (yy & 1) == 0) {
                const size_t ci = (size_t)(yy / 2) * (W / 2) + (xx / 2);
                const size_t base = (size_t)W * H;
                // 与 src/image/hevc_encode.cpp::to_yuv 完全同一套 BT.601 系数
                yuv[base + ci] = cl(((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128);
                yuv[base + (size_t)W * H / 4 + ci] =
                    cl(((112 * R - 94 * G - 18 * B + 128) >> 8) + 128);
            }
        }
    std::string yuv_path = out265;
    yuv_path += ".yuv";
    std::FILE* fp = std::fopen(yuv_path.c_str(), "wb");
    std::fwrite(yuv.data(), 1, yuv.size(), fp);
    std::fclose(fp);

    const std::vector<Bitmap> frames = {bmp};
    EncodeOptions opts;
    opts.video_mode = VideoEncodeMode::Intra;
    opts.video_qp = qp;
    // 第 6 个可选参数：亮度 TU 尺寸档位（log2，0=自动 / 2=4x4 / 3=8x8 / 4/5）
    if (argc > 6) opts.hevc_log2_tu_size = std::atoi(argv[6]);
    // 第 7 个可选参数：transform_skip（1=开）。大 TU 档开它会切到 RExt profile。
    if (argc > 7) opts.hevc_transform_skip = (std::atoi(argv[7]) != 0);
    // 第 8 个可选参数：SAO（1=开）。开启后 SPS 声明 SAO、slice header 追加
    // slice_sao_luma/chroma_flag、每个 CTB 前写 sao() 语法与 offset 决策。
    if (argc > 8) opts.hevc_sao = (std::atoi(argv[8]) != 0);
    // 第 9 个可选参数：帧内模式 RDO（1=开）。不传时 hevc_rdo=false，
    // 所有既有 7/8 参调用的码流逐字节不变。
    if (argc > 9) opts.hevc_rdo = (std::atoi(argv[9]) != 0);
    const auto bs = encode_hevc_annexb(std::span<const Bitmap>{frames}, opts);
    if (bs.empty()) { std::fprintf(stderr, "encode failed qp=%d %dx%d %s\n", qp, W, H, mode); return 1; }
    fp = std::fopen(out265, "wb");
    std::fwrite(bs.data(), 1, bs.size(), fp);
    std::fclose(fp);
    std::printf("%zu\n", bs.size());
    return 0;
}
