#include "obu_header.hpp"

#include <limits>

namespace termchart::image::codec::av1 {

std::string write_leb128(std::uint64_t value) {
    std::string out;
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fu);
        value >>= 7;
        if (value != 0) byte |= 0x80u;
        out.push_back(static_cast<char>(byte));
    } while (value != 0);
    return out;
}

std::string make_obu(ObuType type, std::string_view payload) {
    const auto t = static_cast<unsigned>(type) & 0x0fu;
    // forbidden=0, extension=0, has_size=1, reserved=0。
    const std::uint8_t header = static_cast<std::uint8_t>((t << 3) | 0x02u);
    std::string out;
    out.reserve(1 + 8 + payload.size());
    out.push_back(static_cast<char>(header));
    const auto size = write_leb128(payload.size());
    out.append(size);
    out.append(payload);
    return out;
}

std::string make_temporal_delimiter() {
    std::string out;
    out.push_back(static_cast<char>(0x12));  // type=2, has_size=1
    out.push_back('\0');                     // obu_size=0
    return out;
}

bool read_obu(std::string_view bytes, std::size_t pos, ObuType& type,
              std::string_view& payload, std::size_t& consumed) {
    if (pos >= bytes.size()) return false;
    const std::uint8_t h = static_cast<unsigned char>(bytes[pos]);
    if ((h & 0x80u) != 0) return false;  // forbidden_bit 必须为 0
    std::size_t cur = pos + 1;
    if ((h & 0x04u) != 0) {  // extension header
        if (cur >= bytes.size()) return false;
        ++cur;
    }
    std::uint64_t size = 0;
    if ((h & 0x02u) != 0) {
        unsigned shift = 0;
        for (;;) {
            if (cur >= bytes.size() || shift > 63) return false;
            const std::uint8_t b = static_cast<unsigned char>(bytes[cur++]);
            if (shift == 63 && (b & 0x7fu) > 1u) return false;
            size |= static_cast<std::uint64_t>(b & 0x7fu) << shift;
            if ((b & 0x80u) == 0) break;
            shift += 7;
        }
    } else {
        size = bytes.size() - cur;  // 末 OBU：由剩余字节推出
    }
    if (size > static_cast<std::uint64_t>(bytes.size() - cur)) return false;
    type = static_cast<ObuType>((h >> 3) & 0x0fu);
    payload = bytes.substr(cur, static_cast<std::size_t>(size));
    consumed = cur + static_cast<std::size_t>(size) - pos;
    return true;
}

}  // namespace termchart::image::codec::av1
