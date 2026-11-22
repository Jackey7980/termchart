// av1_min_probe.cpp —— dump 最小 AV1 IVF 与本地重建画面，供 ffmpeg/libdav1d 真解码对拍
//
// 不带模式参数：8x8 平坦帧（一期最小纵切）。
// 带 mode/angle_delta：8x16 双块帧——块 A 带残差提供非平坦上边界，块 B skip=1
// 纯预测，于是块 B 的重建就是帧内预测的原始输出，可以把全部模式（含 angle_delta）
// 与真实解码器逐像素比对。
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "termchart/image/bitmap.hpp"
#include "av1_encode.hpp"
#include "codec/av1/bitstream/headers.hpp"
#include "codec/av1/bitstream/ivf_muxer.hpp"
#include "codec/av1/bitstream/obu_header.hpp"
#include "codec/av1/bitstream/tile_group.hpp"
#include "codec/av1/entropy/coef_cdf_data.hpp"

namespace av1 = termchart::image::codec::av1;

namespace {

int write_file(const char* path, const void* data, std::size_t size) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return 1;
    std::fwrite(data, 1, size, f);
    std::fclose(f);
    return 0;
}

// 8x16 4:2:0：亮度 128 字节 + 两条色度各 32 字节（色度走 DC 无邻居 = 128）。
constexpr std::size_t kYuvBytes8x16 = 192;

void fill_neutral_chroma(std::uint8_t* dst) {
    std::memset(dst + 128, 128, kYuvBytes8x16 - 128);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: av1_min_probe out.ivf expected.yuv [mode angle_delta]\n");
        return 2;
    }

    std::string ivf;
    std::array<std::uint8_t, 96> flat_yuv{};

    if (argc < 5) {
        termchart::image::Bitmap bmp(8, 8, 4);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const auto v = static_cast<std::uint8_t>(128);
                bmp.set(static_cast<std::uint32_t>(x),
                        static_cast<std::uint32_t>(y), 0, v);
                bmp.set(static_cast<std::uint32_t>(x),
                        static_cast<std::uint32_t>(y), 1, v);
                bmp.set(static_cast<std::uint32_t>(x),
                        static_cast<std::uint32_t>(y), 2, v);
            }
        termchart::image::Av1MinimalYuv expected;
        ivf = termchart::image::encode_av1_minimal_ivf(bmp, &expected);
        std::memcpy(flat_yuv.data(), expected.y.data(), expected.y.size());
        std::memcpy(flat_yuv.data() + expected.y.size(), expected.u.data(),
                    expected.u.size());
        std::memcpy(flat_yuv.data() + expected.y.size() + expected.u.size(),
                    expected.v.data(), expected.v.size());
        if (write_file(argv[2], flat_yuv.data(), flat_yuv.size())) return 4;
        std::fprintf(stderr, "ivf bytes=%zu\n", ivf.size());
    } else {
        const int mode = std::atoi(argv[3]);
        const int angle = std::atoi(argv[4]);
        if (mode < 0 || mode > 12 || angle < -3 || angle > 3) {
            std::fprintf(stderr, "mode 0..12, angle_delta -3..3\n");
            return 5;
        }

        av1::SequenceHeaderConfig seq;
        seq.width = 8;
        seq.height = 16;
        av1::FrameHeaderConfig fh;
        fh.base_q_idx = 100;

        av1::Residual8x8 a;
        a.coeff[0] = 6;
        a.coeff[av1::kScan8x8[1]] = 40;
        a.coeff[av1::kScan8x8[2]] = -25;
        a.coeff[av1::kScan8x8[3]] = 18;

        std::string stream = av1::make_temporal_delimiter();
        stream += av1::make_obu(av1::ObuType::SequenceHeader,
                                av1::write_sequence_header(seq));
        stream += av1::make_temporal_delimiter();
        stream += av1::make_obu(av1::ObuType::FrameHeader,
                                av1::write_frame_header(fh));
        stream += av1::make_obu(av1::ObuType::TileGroup,
                                av1::write_tile_group_8x16(a, mode, angle));
        ivf = av1::write_ivf_header(8, 16, 30, 1);
        ivf += av1::write_ivf_frame(stream, 0);

        std::array<std::uint8_t, kYuvBytes8x16> yuv{};
        const auto y = av1::reconstruct_luma_8x16(a, mode, angle);
        std::memcpy(yuv.data(), y.data(), y.size());
        fill_neutral_chroma(yuv.data());
        if (write_file(argv[2], yuv.data(), yuv.size())) return 4;
        std::fprintf(stderr, "ivf bytes=%zu mode=%d angle_delta=%d\n", ivf.size(),
                     mode, angle);
    }

    if (ivf.empty()) {
        std::fprintf(stderr, "encoder refused the frame\n");
        return 3;
    }
    if (write_file(argv[1], ivf.data(), ivf.size())) return 4;
    return 0;
}
