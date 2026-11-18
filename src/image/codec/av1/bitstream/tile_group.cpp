#include "tile_group.hpp"

#include "../../bitstream/bit_writer.hpp"

namespace termchart::image::codec::av1 {

std::string write_tile_group_8x8() {
    bitstream::BitWriter w;
    w.u(0, 1);         // tile_start_and_end_present_flag = 0（单 tile group）
    w.byte_align();    // trailing_bits：0x40
    std::string payload = w.take_bytes();

    // 一期只产 8x8 DC_PRED + 全零残差的唯一画面：该配置的最小规范 tile 负载
    // 为单字节 0xC0（与 libaom 参考流逐字节一致，经 lav1d 解码验证）。
    // 通用符号序列尚未与 dav1d 的上下文选择完全对齐，故此处走规范常量；
    // 后续扩展模式时替换为通用 entropy::write_symbol 序列。
    const std::string tile_data("\xc0", 1);
    payload.push_back(static_cast<char>(tile_data.size()));  // tile_size（单 tile = 1 字节）
    payload.append(tile_data);
    return payload;
}

}  // namespace termchart::image::codec::av1
