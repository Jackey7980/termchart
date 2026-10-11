// test_base64.cpp —— base64（RFC 4648）
#include <doctest.h>

#include <string>
#include <string_view>

#include "termchart/util/base64.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

}  // namespace

TEST_CASE("util/base64: RFC 4648 标准测试向量") {
    CHECK(base64_encode(std::string_view{}) == "");
    CHECK(base64_encode(std::string_view{"f"}) == "Zg==");
    CHECK(base64_encode(std::string_view{"fo"}) == "Zm8=");
    CHECK(base64_encode(std::string_view{"foo"}) == "Zm9v");
    CHECK(base64_encode(std::string_view{"foob"}) == "Zm9vYg==");
    CHECK(base64_encode(std::string_view{"fooba"}) == "Zm9vYmE=");
    CHECK(base64_encode(std::string_view{"foobar"}) == "Zm9vYmFy");
}

TEST_CASE("util/base64: 解码还原（含高位字节与非 ASCII）") {
    const std::string binary{"\x00\x01\x7F\x80\xFF\xFE", 6};
    const std::string encoded = base64_encode(binary);
    std::string decoded;
    REQUIRE(base64_decode(encoded, decoded));
    CHECK(decoded == binary);

    const std::string utf8 = "终端图表";
    std::string round;
    REQUIRE(base64_decode(base64_encode(utf8), round));
    CHECK(round == utf8);
}

TEST_CASE("util/base64: 解码忽略空白，遇到 '=' 结束") {
    std::string decoded;
    REQUIRE(base64_decode("Zm9v\nYmFy", decoded));
    CHECK(decoded == "foobar");

    REQUIRE(base64_decode("Zm9vYg==", decoded));
    CHECK(decoded == "foob");

    // 尾随垃圾在 '=' 之后：按规范忽略
    REQUIRE(base64_decode("Zm9vYg==xx", decoded));
    CHECK(decoded == "foob");
}

TEST_CASE("util/base64: 非法字符返回 false 且不改动输出") {
    std::string decoded = "unchanged";
    CHECK_FALSE(base64_decode("Zm9v!bar", decoded));
    CHECK(decoded == "unchanged");

    // 长度不完整的尾巴按"能解多少解多少"处理（6 个字符 = 36 位 → 4 字节），
    // 这是各家实现的通行做法。
    std::string partial;
    REQUIRE(base64_decode("Zm9vYm", partial));
    CHECK(partial == "foob");
}
