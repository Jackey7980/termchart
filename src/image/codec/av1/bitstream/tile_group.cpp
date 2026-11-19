#include "tile_group.hpp"

#include "../../bitstream/bit_writer.hpp"
#include "../entropy/cdf_manager.hpp"
#include "../entropy/default_cdf.hpp"
#include "../entropy/range_coder.hpp"

namespace termchart::image::codec::av1 {

std::string write_tile_group_8x8() {
    bitstream::BitWriter w;
    w.u(0, 1);         // tile_start_and_end_present_flag = 0（单 tile group）
    w.byte_align();    // trailing_bits：0x40
    std::string payload = w.take_bytes();

    // 一期只产 8x8 DC 附近 + 全零残差的唯一画面。帧尺寸小于最大超级块时
    // partition 被规范强制为 PARTITION_NONE，因此块内语法只剩四个符号：
    //   skip=0 → Y 模式 → use_filter_intra → UV 模式
    // 与 libaom 参考流的单字节 tile 0xC0 解出同一序列（仅收尾补位不同）。
    const CdfDefaultContext& cdf = kDefaultCdf;
    RangeEncoder enc;
    enc.write_bool(0, cdf.m.skip[0][0]);  // skip_txfm = 0
    write_symbol(enc, cdf.kfym[0][0], N_INTRA_PRED_MODES, SMOOTH_PRED);
    enc.write_bool(1, cdf.m.use_filter_intra[0][0]);  // use_filter_intra = 1
    write_symbol(enc, cdf.m.uv_mode[1][SMOOTH_PRED], N_UV_INTRA_PRED_MODES, DC_PRED);

    const std::string tile_data = enc.finish();
    payload.push_back(static_cast<char>(tile_data.size()));  // tile_size（单 tile）
    payload.append(tile_data);
    return payload;
}

}  // namespace termchart::image::codec::av1
