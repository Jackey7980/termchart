#include "av1_encode.hpp"


#include "codec/av1/bitstream/headers.hpp"
#include "codec/av1/bitstream/ivf_muxer.hpp"
#include "codec/av1/bitstream/obu_header.hpp"
#include "codec/av1/bitstream/tile_group.hpp"

namespace termchart::image {

std::string encode_av1_minimal_ivf(const Bitmap& frame) {
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

    std::string out = codec::av1::write_ivf_header(8, 8, 30, 1);
    out += codec::av1::write_ivf_frame(stream, 0);
    return out;
}

}  // namespace termchart::image
