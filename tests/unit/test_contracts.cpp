// test_contracts.cpp —— 跨模块不变量（"地基契约"测试）
//
// 这里放的都不是某个函数的行为，而是**多个模块共同承诺的性质**：
//   * 宽度模型：整串宽度 == 各簇宽度之和（wcwidth + grapheme + unicode 表三方一致）；
//   * UTF-8：编码→解码往返、非法输入永不卡住（fuzz 的运行时对应物）；
//   * 颜色管线：降级幂等、档位单调（color/quantize + core/color_depth）；
//   * SGR：形状合法、无冗余分隔符、delta 为空即零字节（render/sgr）；
//   * 确定性：同样的输入两次渲染得到同样的字节（golden 与 GUI 内嵌都依赖）。
//
// 未来接入 GUI 平行栈时，这些性质必须在像素侧有对应物；改任何一侧先跑这里。
#include <doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include "termchart/api/quick.hpp"
#include "termchart/color/quantize.hpp"
#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/render/sgr.hpp"
#include "termchart/util/grapheme.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;
using namespace termchart::util;

const std::vector<std::string>& sample_texts() {
    static const std::vector<std::string> texts = {
        "hello",
        "中文abc",
        "e\u0301x",         // 组合记号
        "a\r\nb",           // CRLF
        "\u4e2d\u6587",     // 东亚宽
        "\U0001F600!",      // emoji
        "",
    };
    return texts;
}

bool has_prefix(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

}  // namespace

TEST_CASE("contract: 整串宽度等于各簇宽度之和") {
    for (const std::string& text : sample_texts()) {
        const std::vector<GraphemeCluster> clusters = split_clusters(text);
        int summed = 0;
        for (const GraphemeCluster& cluster : clusters) {
            summed += cluster_width(cluster);
        }
        CHECK(display_width(text) == summed);
        CHECK(cluster_count(text) == clusters.size());
    }
}

TEST_CASE("contract: 组合记号不占宽度（主码点决定宽度）") {
    CHECK(display_width("e") == display_width("e\u0301"));
    CHECK(display_width("\u4e2d") == display_width("\u4e2d\u0301"));
}

TEST_CASE("contract: UTF-8 编码解码往返") {
    const std::vector<char32_t> codepoints = {U'a', U'\u00E9', U'\u4E2D', U'\U0001F600',
                                              U'\U0010FFFF'};
    for (const char32_t cp : codepoints) {
        const std::string encoded = to_utf8(cp);
        const Utf8Decode decoded = decode(encoded, 0);
        CHECK(decoded.ok);
        CHECK(decoded.cp == cp);
        CHECK(static_cast<std::size_t>(decoded.consumed) == encoded.size());
    }
}

TEST_CASE("contract: 不可编码的码点被拒，不产出字节") {
    char buffer[8] = {};
    CHECK(encode(U'\xD800', buffer) == 0);  // 代理码点
    CHECK(encode(0x110000, buffer) == 0);   // 超出 Unicode 上界
}

TEST_CASE("contract: 非法 UTF-8 永不抛异常且游标一定前进") {
    const std::string bad = std::string("\xFF\xFE") + std::string("a");
    std::size_t pos = 0;
    std::size_t steps = 0;
    while (pos < bad.size()) {
        const std::size_t step = next_boundary(bad, pos);
        REQUIRE(step > 0);
        pos += step;
        ++steps;
    }
    CHECK(steps == 3);
    CHECK_FALSE(is_valid(bad));
}

TEST_CASE("contract: 颜色降级幂等且档位单调") {
    const Color original = Color::rgb(200, 30, 90);
    for (const ColorDepth depth : {ColorDepth::Ansi256, ColorDepth::Ansi16, ColorDepth::None}) {
        const Color once = color::degrade(original, depth);
        const Color twice = color::degrade(once, depth);
        CHECK(once == twice);
    }
    CHECK(color::degrade(original, ColorDepth::None).is_none());
    CHECK(color::degrade(original, ColorDepth::Ansi256).kind() == ColorKind::Indexed);
    CHECK(color::degrade(original, ColorDepth::Ansi16).kind() == ColorKind::Ansi16);
    CHECK(color::degrade(original, ColorDepth::TrueColor) == original);
}

TEST_CASE("contract: 已降级的语义色不再被降级改变") {
    const Color ansi = Color::ansi16(5);
    CHECK(color::degrade(ansi, ColorDepth::TrueColor) == ansi);
    CHECK(color::degrade(ansi, ColorDepth::Ansi16) == ansi);
    CHECK(color::degrade(ansi, ColorDepth::Ansi256) == ansi);
}

TEST_CASE("contract: SGR 序列形状合法，无空参数与冗余分隔符") {
    const std::vector<Style> styles = {
        Style{},
        Style{}.set_attr(Attr::Bold),
        Style{}.set_fg(Color::rgb(1, 2, 3)),
        Style{}.set_bg(Color::indexed(200)).set_attr(Attr::Underline),
        Style{}.set_fg(Color::ansi16(9)).set_bg(Color::ansi16(0)),
    };
    for (const Style& style : styles) {
        for (const ColorDepth depth : {ColorDepth::None, ColorDepth::Ansi16, ColorDepth::Ansi256,
                                       ColorDepth::TrueColor}) {
            const std::string seq = render::sgr::style_sequence(style, depth);
            CHECK(has_prefix(seq, "\x1b["));
            CHECK(seq.back() == 'm');
            CHECK(seq.find(";;") == std::string::npos);
            CHECK(seq.find("[;") == std::string::npos);
            CHECK(seq.find(";m") == std::string::npos);
        }
    }
}

TEST_CASE("contract: 样式未变时 delta 一个字节都不发") {
    const Style style = Style{}.set_fg(Color::rgb(1, 2, 3)).set_attr(Attr::Bold);
    for (const ColorDepth depth : {ColorDepth::None, ColorDepth::Ansi16, ColorDepth::Ansi256,
                                   ColorDepth::TrueColor}) {
        std::string out;
        render::sgr::append_style_delta(out, style, style, depth);
        CHECK(out.empty());
    }
    CHECK(render::sgr::reset() == std::string("\x1b[0m"));
}

TEST_CASE("contract: 渲染是确定性的（同输入两次同输出）") {
    const Style style = Style{}.set_fg(Color::rgb(10, 20, 30)).set_attr(Attr::Bold);
    const Result<std::string> first = api::render_line("determinism", style);
    const Result<std::string> second = api::render_line("determinism", style);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == *second);
}

TEST_CASE("contract: fuzz UTF-8 往返与宽度对随机码点成立") {
    // 确定性 LCG（不依赖种子），覆盖代理区 / 超界 / 正常值——GUI 接入时
    // 用户文本千奇百怪，这条性质是"永不崩、宽度永在 [0,2]"的底线。
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    auto next = [&]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    };

    int encoded = 0;
    for (int i = 0; i < 30000; ++i) {
        const char32_t cp = static_cast<char32_t>(next() % 0x110000ULL);
        INFO("fuzz cp = ", static_cast<std::uint64_t>(cp));

        // encode() 返回 0 表示无法编码（代理区 / 越界）。此时库按设计把
        // 它落为 U+FFFD（to_utf8 因此**非空**），不要求往返——这正是稳健行为。
        char buf[kMaxUtf8Bytes]{};
        if (encode(cp, buf) == 0) {
            const bool invalid = (cp >= 0xD800U && cp <= 0xDFFFU) || cp > 0x10FFFFU;
            CHECK(invalid);
            CHECK(to_utf8(cp) == std::string("\xEF\xBF\xBD"));  // 正好是 U+FFFD
            continue;
        }

        const std::string enc = to_utf8(cp);
        const Utf8Decode dec = decode(enc, 0);
        CHECK(dec.ok);
        CHECK(dec.cp == cp);
        CHECK(static_cast<std::size_t>(dec.consumed) == enc.size());

        const int w = codepoint_width(cp);
        CHECK(w >= 0);
        CHECK(w <= 2);
        CHECK(display_width(enc) == w);
        ++encoded;
    }
    CHECK(encoded > 1000);  // 绝大多数码点应当可编码
}
