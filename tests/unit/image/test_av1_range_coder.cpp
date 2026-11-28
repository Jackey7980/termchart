// test_av1_range_coder.cpp —— AV1 区间编解码器的对称回环
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "src/media/encoder/av1/entropy/range_coder.hpp"

namespace av1 = termchart::image::codec::av1;

namespace {

std::uint32_t next_rand(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

bool roundtrip(const std::vector<int>& bits, const std::vector<unsigned>& probs) {
    av1::RangeEncoder enc;
    for (std::size_t i = 0; i < bits.size(); ++i) enc.write_bool(bits[i], probs[i]);
    const std::string bytes = enc.finish();

    av1::RangeDecoder dec;
    dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    for (std::size_t i = 0; i < bits.size(); ++i) {
        if (dec.decode_bool(probs[i]) != bits[i]) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("av1: 区间编解码单比特与短序列回环") {
    for (unsigned prob : {4u, 64u, 16384u, 20000u, 32764u}) {
        for (int bit = 0; bit < 2; ++bit) {
            CHECK(roundtrip(std::vector<int>(1, bit), std::vector<unsigned>(1, prob)));
            CHECK(roundtrip(std::vector<int>(8, bit), std::vector<unsigned>(8, prob)));
            CHECK(roundtrip(std::vector<int>(64, bit), std::vector<unsigned>(64, prob)));
        }
    }
}

TEST_CASE("av1: 区间编解码百万符号随机概率回环") {
    std::uint32_t seed = 0x12345678u;
    constexpr std::size_t kN = 1000000;
    std::vector<int> bits(kN);
    std::vector<unsigned> probs(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        bits[i] = static_cast<int>(next_rand(seed) & 1u);
        probs[i] = 4u + next_rand(seed) % 32761u;
    }
    CHECK(roundtrip(bits, probs));
}
