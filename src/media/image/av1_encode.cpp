#include "av1_encode.hpp"

#include "../encoder/av1/bitstream/headers.hpp"
#include "../encoder/av1/bitstream/ivf_muxer.hpp"
#include "../encoder/av1/bitstream/obu_header.hpp"
#include "../encoder/av1/bitstream/tile_group.hpp"
#include "../encoder/av1/encoder/intra_encoder.hpp"

namespace termchart::image {

std::string encode_av1_minimal_ivf(const Bitmap& frame, Av1MinimalYuv* expected) {
    if (frame.width() != 8 || frame.height() != 8) return {};
    codec::av1::SequenceHeaderConfig seq;
    seq.width = 8;
    seq.height = 8;
    codec::av1::FrameHeaderConfig fh;
    fh.base_q_idx = 100;

    std::string stream = codec::av1::make_temporal_delimiter();
    stream += codec::av1::make_obu(codec::av1::ObuType::SequenceHeader,
                                   codec::av1::write_sequence_header(seq));
    stream += codec::av1::make_temporal_delimiter();
    stream += codec::av1::make_obu(codec::av1::ObuType::FrameHeader,
                                   codec::av1::write_frame_header(fh));
    stream += codec::av1::make_obu(codec::av1::ObuType::TileGroup,
                                   codec::av1::write_tile_group_8x8());

    if (expected) {
        // 亮度走与编码器相同的 SMOOTH 预测路径（无邻居时边界填 above=127 /
        // left=129，因此块内是 127/128/129 的渐变，不是纯 128）；色度走 DC，
        // 无邻居时恒为 128。
        expected->y = codec::av1::reconstruct_luma_8x8(codec::av1::Residual8x8{});
        expected->u.fill(128);
        expected->v.fill(128);
    }

    std::string out = codec::av1::write_ivf_header(8, 8, 30, 1);
    out += codec::av1::write_ivf_frame(stream, 0);
    return out;
}

namespace {

bool av1_frame_size_ok(int w, int h) noexcept {
    // 当前仅支持至少有一维为 64 的整倍数帧；两维都大于 64 时
    // 现有帧头/划分组合尚未与 libdav1d 对齐，明确拒绝而非输出坏码流。
    return w >= 64 && h >= 64 && (w % 64) == 0 && (h % 64) == 0 &&
           (w == 64 || h == 64);
}

// 一帧的 OBU 序列（TD + [seq] + TD + frame header + tile group）。
// with_sequence_header 仅首帧为真：AV1 的序列头只需出现一次。
std::string make_av1_frame_stream(const Bitmap& frame,
                                  const Av1FrameEncodeOptions& options,
                                  bool with_sequence_header,
                                  Av1FrameYuv* expected) {
    const int w = static_cast<int>(frame.width());
    const int h = static_cast<int>(frame.height());
    if (!av1_frame_size_ok(w, h)) return {};

    codec::av1::IntraEncodeOptions intra;
    intra.base_q_idx = options.base_q_idx;
    intra.lambda_div = options.lambda_div;
    intra.rdo_mode = options.rdo_mode;
    intra.rdo_partition = options.rdo_partition;
    intra.prediction_only = options.prediction_only;
    const codec::av1::IntraEncodeResult encoded =
        codec::av1::encode_intra_frame(codec::av1::bitmap_to_yuv420(frame), intra);
    if (encoded.tile_data.empty()) return {};

    codec::av1::SequenceHeaderConfig seq;
    seq.width = w;
    seq.height = h;
    seq.inter_frames = true;
    codec::av1::FrameHeaderConfig fh;
    fh.base_q_idx = options.base_q_idx;
    fh.width = w;
    fh.height = h;

    std::string stream = codec::av1::make_temporal_delimiter();
    if (with_sequence_header) {
        stream += codec::av1::make_obu(codec::av1::ObuType::SequenceHeader,
                                       codec::av1::write_sequence_header_inter(seq));
        stream += codec::av1::make_temporal_delimiter();
    }
    stream += codec::av1::make_obu(codec::av1::ObuType::FrameHeader,
                                   codec::av1::write_frame_header_inter(fh));
    stream += codec::av1::make_obu(codec::av1::ObuType::TileGroup,
                                   encoded.tile_data);

    if (expected) {
        const int cw = (w + 1) >> 1, ch = (h + 1) >> 1;
        expected->y.resize(static_cast<std::size_t>(w) * h);
        expected->u.resize(static_cast<std::size_t>(cw) * ch);
        expected->v.resize(expected->u.size());
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                (*expected).y[static_cast<std::size_t>(y) * w + x] =
                    encoded.recon.y[static_cast<std::size_t>(y) * w + x];
            }
        }
        for (int y = 0; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * cw + x;
                (*expected).u[i] = encoded.recon.u[i];
                (*expected).v[i] = encoded.recon.v[i];
            }
        }
    }

    return stream;
}

}  // namespace

std::string encode_av1_ivf(const Bitmap& frame,
                           const Av1FrameEncodeOptions& options,
                           Av1FrameYuv* expected) {
    const int w = static_cast<int>(frame.width());
    const int h = static_cast<int>(frame.height());
    if (!av1_frame_size_ok(w, h)) return {};
    const std::string stream = make_av1_frame_stream(frame, options, true, expected);
    if (stream.empty()) return {};
    std::string out = codec::av1::write_ivf_header(w, h, 30, 1);
    out += codec::av1::write_ivf_frame(stream, 0);
    return out;
}

std::string encode_av1_ivf(std::span<const Bitmap> frames,
                           const Av1FrameEncodeOptions& options) {
    if (frames.empty()) return {};
    const int w = static_cast<int>(frames[0].width());
    const int h = static_cast<int>(frames[0].height());
    if (!av1_frame_size_ok(w, h)) return {};

    std::string out = codec::av1::write_ivf_header(w, h, 30,
                                                    static_cast<std::uint32_t>(frames.size()));
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const Bitmap& frame = frames[i];
        if (static_cast<int>(frame.width()) != w || static_cast<int>(frame.height()) != h)
            return {};
        const std::string stream = make_av1_frame_stream(frame, options, i == 0, nullptr);
        if (stream.empty()) return {};
        out += codec::av1::write_ivf_frame(stream, static_cast<std::uint64_t>(i));
    }
    return out;
}

}  // namespace termchart::image
