// png.cpp —— PNG 解码实现（含自带 inflate）
//
// 结构自下而上：
//   BitReader → Huffman 构建/解码 → inflate（zlib 包装 + 三种 deflate 块）
//   → chunk 解析（校验 CRC）→ 反滤波（None/Sub/Up/Average/Paeth）
//   → 采样展开与颜色类型转换 → RGBA 位图
//
// 所有失败路径都返回 ErrorCode，不抛异常、不产出"部分正确"的图。
#include "termchart/image/png.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "termchart/image/png_encode.hpp"  // 复用 crc32 / adler32

namespace termchart::image {
namespace {

// ============================== inflate ==============================

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    // 取 need 位（deflate 的位序是 LSB 优先）；数据不足返回 -1。
    int bits(int need) noexcept {
        while (count_ < need) {
            if (pos_ >= size_) {
                return -1;
            }
            buffer_ |= static_cast<std::uint32_t>(data_[pos_++]) << count_;
            count_ += 8;
        }
        const int value = static_cast<int>(buffer_ & ((1u << need) - 1u));
        buffer_ >>= need;
        count_ -= need;
        return value;
    }

    // 对齐到字节边界：**只丢弃不足 8 位的部分**。整个缓冲清零是错的——缓冲里
    // 可能还压着完整字节（stored 块紧跟在前一块之后时就会这样）。
    void align_to_byte() noexcept {
        const int drop = count_ & 7;
        buffer_ >>= drop;
        count_ -= drop;
    }

    bool byte(std::uint8_t& out) noexcept {
        if (count_ >= 8) {
            out = static_cast<std::uint8_t>(buffer_ & 0xFFu);
            buffer_ >>= 8;
            count_ -= 8;
            return true;
        }
        if (pos_ >= size_) {
            return false;
        }
        out = data_[pos_++];
        return true;
    }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t pos_ = 0;
    std::uint32_t buffer_ = 0;
    int count_ = 0;
};

struct Huffman {
    std::array<std::uint16_t, 16> count{};
    std::vector<std::uint16_t> symbol;
};

// 规范 Huffman：只存每个码长的个数与按码长排序的符号表。
bool build_huffman(Huffman& huffman, const std::uint8_t* lengths, std::size_t count) {
    huffman.count.fill(0);
    for (std::size_t i = 0; i < count; ++i) {
        ++huffman.count[static_cast<std::size_t>(lengths[i])];
    }
    if (huffman.count[0] == count) {
        huffman.symbol.clear();  // 全零：空表（合法，但解不出任何符号）
        return true;
    }
    // 过完备（Kraft 不等式被打破）说明码表已损坏。
    int left = 1;
    for (std::size_t len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= huffman.count[len];
        if (left < 0) {
            return false;
        }
    }
    std::array<std::uint16_t, 16> offsets{};
    for (std::size_t len = 1; len < 15; ++len) {
        offsets[len + 1] = static_cast<std::uint16_t>(offsets[len] + huffman.count[len]);
    }
    huffman.symbol.assign(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        if (lengths[i] != 0) {
            huffman.symbol[offsets[lengths[i]]++] = static_cast<std::uint16_t>(i);
        }
    }
    return true;
}

// 逐位下降解码。建表会更快，但这段代码要的是"短且不易写错"。
int decode_symbol(BitReader& reader, const Huffman& huffman) {
    int code = 0;
    int first = 0;
    int index = 0;
    for (std::size_t len = 1; len <= 15; ++len) {
        const int bit = reader.bits(1);
        if (bit < 0) {
            return -1;
        }
        code |= bit;
        const int count = huffman.count[len];
        if (code - count < first) {
            const auto at = static_cast<std::size_t>(index + (code - first));
            return at < huffman.symbol.size() ? static_cast<int>(huffman.symbol[at]) : -1;
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

constexpr int kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19, 23, 27,
                                 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr int kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr int kDistanceBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,   25,
                                   33,   49,   65,   97,   129,  193,   257,   385,   513,  769,
                                   1025, 1537, 2049, 3073, 4097, 6145,  8193,  12289, 16385, 24577};
constexpr int kDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflate_block(BitReader& reader, const Huffman& literal, const Huffman& distance,
                   std::vector<std::uint8_t>& out) {
    for (;;) {
        const int symbol = decode_symbol(reader, literal);
        if (symbol < 0) {
            return false;
        }
        if (symbol < 256) {
            out.push_back(static_cast<std::uint8_t>(symbol));
            continue;
        }
        if (symbol == 256) {
            return true;  // 块结束
        }
        const int length_index = symbol - 257;
        if (length_index >= 29) {
            return false;
        }
        const int length_extra = reader.bits(kLengthExtra[length_index]);
        if (length_extra < 0) {
            return false;
        }
        const int length = kLengthBase[length_index] + length_extra;

        const int distance_symbol = decode_symbol(reader, distance);
        if (distance_symbol < 0 || distance_symbol >= 30) {
            return false;
        }
        const int distance_extra = reader.bits(kDistanceExtra[distance_symbol]);
        if (distance_extra < 0) {
            return false;
        }
        const int dist = kDistanceBase[distance_symbol] + distance_extra;
        if (dist <= 0 || static_cast<std::size_t>(dist) > out.size()) {
            return false;  // 回引越界：流已损坏
        }
        const std::size_t from = out.size() - static_cast<std::size_t>(dist);
        for (int i = 0; i < length; ++i) {
            // 先取值再 push_back：push_back 可能重分配，直接传元素引用会读到失效内存。
            const std::uint8_t value = out[from + static_cast<std::size_t>(i)];
            out.push_back(value);
        }
    }
}

Huffman fixed_literal_table() {
    std::array<std::uint8_t, 288> lengths{};
    for (int i = 0; i < 144; ++i) {
        lengths[static_cast<std::size_t>(i)] = 8;
    }
    for (int i = 144; i < 256; ++i) {
        lengths[static_cast<std::size_t>(i)] = 9;
    }
    for (int i = 256; i < 280; ++i) {
        lengths[static_cast<std::size_t>(i)] = 7;
    }
    for (int i = 280; i < 288; ++i) {
        lengths[static_cast<std::size_t>(i)] = 8;
    }
    Huffman table;
    (void)build_huffman(table, lengths.data(), lengths.size());
    return table;
}

Huffman fixed_distance_table() {
    std::array<std::uint8_t, 30> lengths{};
    lengths.fill(5);
    Huffman table;
    (void)build_huffman(table, lengths.data(), lengths.size());
    return table;
}

const Huffman& fixed_literal() {
    static const Huffman table = fixed_literal_table();
    return table;
}

const Huffman& fixed_distance() {
    static const Huffman table = fixed_distance_table();
    return table;
}

bool read_dynamic_tables(BitReader& reader, Huffman& literal, Huffman& distance) {
    const int hlit = reader.bits(5);
    const int hdist = reader.bits(5);
    const int hclen = reader.bits(4);
    if (hlit < 0 || hdist < 0 || hclen < 0) {
        return false;
    }
    const auto lit_count = static_cast<std::size_t>(hlit + 257);
    const auto dist_count = static_cast<std::size_t>(hdist + 1);
    const int clen_count = hclen + 4;

    static constexpr int kOrder[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                       11, 4,  12, 3, 13, 2, 14, 1, 15};
    std::array<std::uint8_t, 19> code_lengths{};
    for (int i = 0; i < clen_count; ++i) {
        const int value = reader.bits(3);
        if (value < 0) {
            return false;
        }
        code_lengths[static_cast<std::size_t>(kOrder[i])] = static_cast<std::uint8_t>(value);
    }
    Huffman code_length_table;
    if (!build_huffman(code_length_table, code_lengths.data(), code_lengths.size())) {
        return false;
    }

    std::vector<std::uint8_t> lengths(lit_count + dist_count, 0);
    std::size_t index = 0;
    while (index < lengths.size()) {
        const int symbol = decode_symbol(reader, code_length_table);
        if (symbol < 0) {
            return false;
        }
        if (symbol < 16) {
            lengths[index++] = static_cast<std::uint8_t>(symbol);
            continue;
        }
        int repeat = 0;
        std::uint8_t value = 0;
        if (symbol == 16) {
            if (index == 0) {
                return false;
            }
            value = lengths[index - 1];
            const int extra = reader.bits(2);
            if (extra < 0) {
                return false;
            }
            repeat = 3 + extra;
        } else if (symbol == 17) {
            const int extra = reader.bits(3);
            if (extra < 0) {
                return false;
            }
            repeat = 3 + extra;
        } else if (symbol == 18) {
            const int extra = reader.bits(7);
            if (extra < 0) {
                return false;
            }
            repeat = 11 + extra;
        } else {
            return false;
        }
        if (index + static_cast<std::size_t>(repeat) > lengths.size()) {
            return false;
        }
        for (int i = 0; i < repeat; ++i) {
            lengths[index++] = value;
        }
    }
    return build_huffman(literal, lengths.data(), lit_count) &&
           build_huffman(distance, lengths.data() + lit_count, dist_count);
}

// zlib 包装：2 字节头 + deflate 块 + 4 字节 Adler-32（大端）。
bool inflate_zlib(const std::uint8_t* data, std::size_t size,
                  std::vector<std::uint8_t>& out) {
    if (size < 6) {
        return false;
    }
    const int cmf = data[0];
    const int flg = data[1];
    if ((cmf & 0x0F) != 8) {
        return false;  // 压缩方法必须是 deflate
    }
    if (((cmf << 8) | flg) % 31 != 0) {
        return false;  // zlib 头部校验
    }
    if ((flg & 0x20) != 0) {
        return false;  // 不支持预置字典
    }

    BitReader reader(data + 2, size - 2);
    int last = 0;
    do {
        last = reader.bits(1);
        if (last < 0) {
            return false;
        }
        const int type = reader.bits(2);
        if (type < 0) {
            return false;
        }
        if (type == 0) {
            reader.align_to_byte();
            std::uint8_t len_lo = 0;
            std::uint8_t len_hi = 0;
            std::uint8_t nlen_lo = 0;
            std::uint8_t nlen_hi = 0;
            if (!reader.byte(len_lo) || !reader.byte(len_hi) || !reader.byte(nlen_lo) ||
                !reader.byte(nlen_hi)) {
                return false;
            }
            const int len = len_lo | (len_hi << 8);
            const int nlen = nlen_lo | (nlen_hi << 8);
            if ((len ^ 0xFFFF) != nlen) {
                return false;
            }
            for (int i = 0; i < len; ++i) {
                std::uint8_t value = 0;
                if (!reader.byte(value)) {
                    return false;
                }
                out.push_back(value);
            }
        } else if (type == 1) {
            if (!inflate_block(reader, fixed_literal(), fixed_distance(), out)) {
                return false;
            }
        } else if (type == 2) {
            Huffman literal;
            Huffman distance;
            if (!read_dynamic_tables(reader, literal, distance)) {
                return false;
            }
            if (!inflate_block(reader, literal, distance, out)) {
                return false;
            }
        } else {
            return false;  // 类型 3 是非法值
        }
    } while (last == 0);

    const std::uint32_t stored = (static_cast<std::uint32_t>(data[size - 4]) << 24) |
                                 (static_cast<std::uint32_t>(data[size - 3]) << 16) |
                                 (static_cast<std::uint32_t>(data[size - 2]) << 8) |
                                 static_cast<std::uint32_t>(data[size - 1]);
    return stored == adler32(out.data(), out.size());
}

// ============================== PNG 结构 ==============================

std::uint32_t read_be32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

constexpr std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

struct PngHeader {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t bit_depth = 0;
    std::uint8_t color_type = 0;
    std::uint8_t interlace = 0;
    std::vector<std::uint8_t> palette;        // RGB 三元组
    std::vector<std::uint8_t> palette_alpha;  // tRNS for color type 3
    bool has_trns = false;
    std::uint16_t trns_gray = 0;
    std::uint16_t trns_r = 0;
    std::uint16_t trns_g = 0;
    std::uint16_t trns_b = 0;
    std::vector<std::uint8_t> idat;
};

int channels_of(std::uint8_t color_type) noexcept {
    switch (color_type) {
        case 0:
            return 1;
        case 2:
            return 3;
        case 3:
            return 1;
        case 4:
            return 2;
        case 6:
            return 4;
        default:
            return 0;
    }
}

bool parse_chunks(const std::uint8_t* data, std::size_t size, PngHeader& header) {
    std::size_t at = 8;  // 跳过签名
    bool seen_ihdr = false;
    bool seen_iend = false;
    while (at + 12 <= size) {
        const std::uint32_t length = read_be32(data + at);
        if (at + 12 + length > size) {
            return false;  // 长度越界
        }
        const std::uint8_t* type = data + at + 4;
        const std::uint8_t* payload = data + at + 8;
        const std::uint32_t stored_crc = read_be32(data + at + 8 + length);
        if (crc32(data + at + 4, 4 + length) != stored_crc) {
            return false;  // CRC 不符：宁可不给图，也不给可疑的图
        }

        const bool is_ihdr = std::memcmp(type, "IHDR", 4) == 0;
        const bool is_idat = std::memcmp(type, "IDAT", 4) == 0;
        const bool is_iend = std::memcmp(type, "IEND", 4) == 0;
        const bool is_plte = std::memcmp(type, "PLTE", 4) == 0;
        const bool is_trns = std::memcmp(type, "tRNS", 4) == 0;

        if (is_ihdr) {
            if (length != 13 || seen_ihdr) {
                return false;
            }
            seen_ihdr = true;
            header.width = read_be32(payload);
            header.height = read_be32(payload + 4);
            header.bit_depth = payload[8];
            header.color_type = payload[9];
            const std::uint8_t compression = payload[10];
            const std::uint8_t filter_method = payload[11];
            header.interlace = payload[12];
            if (header.width == 0 || header.height == 0) {
                return false;
            }
            if (compression != 0 || filter_method != 0) {
                return false;
            }
            const int channels = channels_of(header.color_type);
            if (channels == 0) {
                return false;
            }
            const bool ok_depth = header.bit_depth == 1 || header.bit_depth == 2 ||
                                  header.bit_depth == 4 || header.bit_depth == 8 ||
                                  header.bit_depth == 16;
            if (!ok_depth) {
                return false;
            }
            // 调色板只允许 1/2/4/8 位；灰度与真彩的 16 位是合法的。
            if (header.color_type == 3 && header.bit_depth == 16) {
                return false;
            }
        } else if (is_plte) {
            if (length % 3 != 0 || length > 256 * 3) {
                return false;
            }
            header.palette.assign(payload, payload + length);
        } else if (is_trns) {
            header.has_trns = true;
            if (header.color_type == 3) {
                header.palette_alpha.assign(payload, payload + length);
            } else if (header.color_type == 0 && length >= 2) {
                header.trns_gray = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
            } else if (header.color_type == 2 && length >= 6) {
                header.trns_r = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
                header.trns_g = static_cast<std::uint16_t>((payload[2] << 8) | payload[3]);
                header.trns_b = static_cast<std::uint16_t>((payload[4] << 8) | payload[5]);
            }
        } else if (is_idat) {
            header.idat.insert(header.idat.end(), payload, payload + length);
        } else if (is_iend) {
            seen_iend = true;
            break;
        }
        // 其余 chunk（gAMA/tEXt/iCCP…）一律跳过：它们不影响像素。

        at += 12 + length;
    }
    if (!seen_ihdr || !seen_iend) {
        return false;
    }
    return true;
}

std::uint16_t sample_at(const std::uint8_t* row, std::uint32_t index,
                        std::uint8_t bit_depth) noexcept {
    switch (bit_depth) {
        case 8:
            return row[index];
        case 16:
            return static_cast<std::uint16_t>((row[static_cast<std::size_t>(index) * 2] << 8) |
                                              row[static_cast<std::size_t>(index) * 2 + 1]);
        case 1:
            return static_cast<std::uint16_t>(
                (row[static_cast<std::size_t>(index) / 8] >> (7 - (index % 8))) & 0x01u);
        case 2:
            return static_cast<std::uint16_t>(
                (row[static_cast<std::size_t>(index) / 4] >> (6 - 2 * (index % 4))) & 0x03u);
        case 4:
            return static_cast<std::uint16_t>(
                (row[static_cast<std::size_t>(index) / 2] >> (4 - 4 * (index % 2))) & 0x0Fu);
        default:
            return 0;
    }
}

// 低深度按比例放大到 0..255；16 位取高字节（终端与 GUI 都用不到 16 位深度）。
std::uint8_t to_8bit(std::uint16_t value, std::uint8_t bit_depth) noexcept {
    switch (bit_depth) {
        case 16:
            return static_cast<std::uint8_t>(value >> 8);
        case 8:
            return static_cast<std::uint8_t>(value);
        case 4:
            return static_cast<std::uint8_t>(value * 17u);
        case 2:
            return static_cast<std::uint8_t>(value * 85u);
        case 1:
            return static_cast<std::uint8_t>(value != 0 ? 255u : 0u);
        default:
            return 0;
    }
}

std::uint8_t paeth(int a, int b, int c) noexcept {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) {
        return static_cast<std::uint8_t>(a);
    }
    return static_cast<std::uint8_t>(pb <= pc ? b : c);
}

bool unfilter(std::vector<std::uint8_t>& raw, std::uint32_t height,
              std::size_t row_bytes, std::size_t bpp) {
    const std::size_t stride = row_bytes + 1;
    if (raw.size() < stride * static_cast<std::size_t>(height)) {
        return false;
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(height) * row_bytes, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t filter = raw[static_cast<std::size_t>(y) * stride];
        const std::uint8_t* src = raw.data() + static_cast<std::size_t>(y) * stride + 1;
        std::uint8_t* dst = out.data() + static_cast<std::size_t>(y) * row_bytes;
        const std::uint8_t* prev =
            y == 0 ? nullptr : out.data() + static_cast<std::size_t>(y - 1) * row_bytes;
        for (std::size_t i = 0; i < row_bytes; ++i) {
            const int left = i >= bpp ? dst[i - bpp] : 0;
            const int up = prev != nullptr ? prev[i] : 0;
            const int up_left = (prev != nullptr && i >= bpp) ? prev[i - bpp] : 0;
            switch (filter) {
                case 0:
                    dst[i] = src[i];
                    break;
                case 1:
                    dst[i] = static_cast<std::uint8_t>(src[i] + left);
                    break;
                case 2:
                    dst[i] = static_cast<std::uint8_t>(src[i] + up);
                    break;
                case 3:
                    dst[i] = static_cast<std::uint8_t>(src[i] + ((left + up) / 2));
                    break;
                case 4:
                    dst[i] = static_cast<std::uint8_t>(src[i] + paeth(left, up, up_left));
                    break;
                default:
                    return false;  // 未知滤波类型
            }
        }
    }
    raw.swap(out);
    return true;
}

}  // namespace

Result<Bitmap> decode_png(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 8 || std::memcmp(data, kSignature, 8) != 0) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }

    PngHeader header;
    if (!parse_chunks(data, size, header)) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    if (header.interlace != 0) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);  // 不支持 Adam7
    }
    const int channels = channels_of(header.color_type);
    if (channels == 0) {
        return Unexpected<ErrorCode>(ErrorCode::Unsupported);
    }
    if (header.color_type == 3 && header.palette.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 调色板图缺 PLTE
    }

    const auto bits_per_pixel =
        static_cast<std::uint64_t>(channels) * static_cast<std::uint64_t>(header.bit_depth);
    const auto row_bytes_u64 =
        (static_cast<std::uint64_t>(header.width) * bits_per_pixel + 7) / 8;
    const auto raw_bytes_u64 = (row_bytes_u64 + 1) * static_cast<std::uint64_t>(header.height);
    if (raw_bytes_u64 > kPngMaxRawBytes) {
        return Unexpected<ErrorCode>(ErrorCode::OutOfRange);  // 畸形头：拒绝天量分配
    }
    const auto row_bytes = static_cast<std::size_t>(row_bytes_u64);
    const auto bpp = static_cast<std::size_t>((bits_per_pixel + 7) / 8);

    if (header.idat.empty()) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(raw_bytes_u64));
    if (!inflate_zlib(header.idat.data(), header.idat.size(), raw)) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }
    if (raw.size() != static_cast<std::size_t>(raw_bytes_u64)) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 解压后长度不符
    }
    if (!unfilter(raw, header.height, row_bytes, bpp)) {
        return Unexpected<ErrorCode>(ErrorCode::ParseError);
    }

    Bitmap bitmap(header.width, header.height, 4);
    for (std::uint32_t y = 0; y < header.height; ++y) {
        const std::uint8_t* row = raw.data() + static_cast<std::size_t>(y) * row_bytes;
        for (std::uint32_t x = 0; x < header.width; ++x) {
            std::uint8_t r = 0;
            std::uint8_t g = 0;
            std::uint8_t b = 0;
            std::uint8_t a = 255;
            switch (header.color_type) {
                case 0: {  // 灰度
                    const std::uint16_t gray = sample_at(row, x, header.bit_depth);
                    r = g = b = to_8bit(gray, header.bit_depth);
                    if (header.has_trns && gray == header.trns_gray) {
                        a = 0;
                    }
                    break;
                }
                case 2: {  // 真彩
                    const std::uint16_t sr = sample_at(row, x * 3 + 0, header.bit_depth);
                    const std::uint16_t sg = sample_at(row, x * 3 + 1, header.bit_depth);
                    const std::uint16_t sb = sample_at(row, x * 3 + 2, header.bit_depth);
                    r = to_8bit(sr, header.bit_depth);
                    g = to_8bit(sg, header.bit_depth);
                    b = to_8bit(sb, header.bit_depth);
                    if (header.has_trns && sr == header.trns_r && sg == header.trns_g &&
                        sb == header.trns_b) {
                        a = 0;
                    }
                    break;
                }
                case 3: {  // 调色板
                    const std::uint16_t index = sample_at(row, x, header.bit_depth);
                    const std::size_t at = static_cast<std::size_t>(index) * 3;
                    if (at + 2 >= header.palette.size()) {
                        return Unexpected<ErrorCode>(ErrorCode::ParseError);  // 索引越界
                    }
                    r = header.palette[at];
                    g = header.palette[at + 1];
                    b = header.palette[at + 2];
                    if (index < header.palette_alpha.size()) {
                        a = header.palette_alpha[index];
                    }
                    break;
                }
                case 4: {  // 灰度 + alpha
                    const std::uint16_t gray = sample_at(row, x * 2 + 0, header.bit_depth);
                    const std::uint16_t alpha = sample_at(row, x * 2 + 1, header.bit_depth);
                    r = g = b = to_8bit(gray, header.bit_depth);
                    a = to_8bit(alpha, header.bit_depth);
                    break;
                }
                default: {  // 6：真彩 + alpha
                    const std::uint16_t sr = sample_at(row, x * 4 + 0, header.bit_depth);
                    const std::uint16_t sg = sample_at(row, x * 4 + 1, header.bit_depth);
                    const std::uint16_t sb = sample_at(row, x * 4 + 2, header.bit_depth);
                    const std::uint16_t sa = sample_at(row, x * 4 + 3, header.bit_depth);
                    r = to_8bit(sr, header.bit_depth);
                    g = to_8bit(sg, header.bit_depth);
                    b = to_8bit(sb, header.bit_depth);
                    a = to_8bit(sa, header.bit_depth);
                    break;
                }
            }
            bitmap.set(x, y, 0, r);
            bitmap.set(x, y, 1, g);
            bitmap.set(x, y, 2, b);
            bitmap.set(x, y, 3, a);
        }
    }
    return bitmap;
}

}  // namespace termchart::image
