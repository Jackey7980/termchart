// test_av1_daala.cpp —— Daala 区间引擎的编解码对称回环
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "src/image/codec/av1/entropy/daala_engine.hpp"

namespace av1 = termchart::image::codec::av1;

namespace {

std::uint32_t next_rand(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

bool roundtrip(const std::vector<int>& bits, const std::vector<int>& probs) {
    av1::DaalaEncoder enc;
    for (std::size_t i = 0; i < bits.size(); ++i) enc.write_bit(bits[i], probs[i]);
    const std::string bytes = enc.finish();

    av1::DaalaDecoder dec;
    if (!dec.init(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size())) return false;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        int got = -1;
        if (!dec.read_bit(probs[i], got) || got != bits[i]) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("av1: Daala 单比特与短序列回环") {
    for (int prob : {1, 2, 64, 127, 128, 200, 253, 254}) {
        for (int bit = 0; bit < 2; ++bit) {
            CHECK(roundtrip(std::vector<int>(1, bit), std::vector<int>(1, prob)));
            CHECK(roundtrip(std::vector<int>(8, bit), std::vector<int>(8, prob)));
        }
    }
}

TEST_CASE("av1: Daala 百万符号随机概率回环") {
    std::uint32_t seed = 0x12345678u;
    constexpr std::size_t kN = 1000000;
    std::vector<int> bits(kN);
    std::vector<int> probs(kN);
    for (std::size_t i = 0; i < kN; ++i) {
        bits[i] = static_cast<int>(next_rand(seed) & 1u);
        probs[i] = 1 + static_cast<int>(next_rand(seed) % 254u);
    }
    CHECK(roundtrip(bits, probs));
}
