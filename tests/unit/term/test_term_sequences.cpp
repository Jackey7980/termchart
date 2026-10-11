// test_term_sequences.cpp —— 鼠标开关、OSC 8 超链接、OSC 52 剪贴板、DA 探测、颜色降级
//
// 这些模块都只"生成序列 / 解析回复"，不碰 stdin/stdout，所以可以逐字节断言。
#include <doctest.h>

#include <string>

#include "termchart/core/color_depth.hpp"
#include "termchart/core/style.hpp"
#include "termchart/term/capability_query.hpp"
#include "termchart/term/caps.hpp"
#include "termchart/term/clipboard.hpp"
#include "termchart/term/color_fallback.hpp"
#include "termchart/term/hyperlink.hpp"
#include "termchart/term/mouse_mode.hpp"
#include "termchart/util/base64.hpp"

namespace {

using namespace termchart;
using namespace termchart::term;

}  // namespace

TEST_CASE("term/mouse: 模式与编码是两组独立开关") {
    CHECK(enable_mouse(MouseMode::Normal, MouseEncoding::Sgr)
          == "\x1b[?1006h\x1b[?1000h");
    CHECK(enable_mouse(MouseMode::AnyMotion, MouseEncoding::Default) == "\x1b[?1003h");
    CHECK(set_mouse_mode(MouseMode::Off).empty());          // Off 不产生模式序列
    CHECK(set_mouse_encoding(MouseEncoding::Default).empty());

    // 关闭时先关模式再关编码
    CHECK(disable_mouse(MouseMode::Normal, MouseEncoding::Sgr)
          == "\x1b[?1000l\x1b[?1006l");

    // 切换模式前要清场：终端的鼠标模式是叠加的，只关当前那个会留下上一个
    const std::string all_off = disable_all_mouse(MouseEncoding::Sgr);
    for (const char* seq : {"\x1b[?9l", "\x1b[?1000l", "\x1b[?1002l", "\x1b[?1003l",
                            "\x1b[?1006l"}) {
        CHECK(all_off.find(seq) != std::string::npos);
    }
}

TEST_CASE("term/hyperlink: OSC 8 精确字节与转义") {
    CHECK(hyperlink_open("https://example.com") ==
          "\x1b]8;;https://example.com\x1b\\");
    CHECK(hyperlink_open("https://example.com", "a1") ==
          "\x1b]8;id=a1;https://example.com\x1b\\");
    CHECK(hyperlink_close() == "\x1b]8;;\x1b\\");

    CHECK(hyperlink("点这里", "https://example.com") ==
          "\x1b]8;;https://example.com\x1b\\点这里\x1b]8;;\x1b\\");

    // ID 里的 ';' 会破坏字段边界，必须剔除
    CHECK(hyperlink_open("https://e.com", "a;b").find("id=ab;") != std::string::npos);
    // URI 里的 ';' 是合法字符（矩阵参数），必须保留
    CHECK(hyperlink_open("https://e.com/a;b").find("a;b") != std::string::npos);
    // 控制字符一律剔除：否则会截断序列或提前结束
    const std::string dirty = hyperlink_open("https://e.com\x1b\x07x");
    CHECK(dirty.find('\x07') == std::string::npos);
    CHECK(dirty.find("x") != std::string::npos);
    CHECK(dirty.size() == std::string("\x1b]8;;").size() + std::string("https://e.comx").size() +
                             std::string("\x1b\\").size());
}

TEST_CASE("term/clipboard: OSC 52 写入、请求与回包解析") {
    const std::string set = clipboard_set("hi");
    CHECK(set == "\x1b]52;c;aGk=\x1b\\");

    const std::string primary = clipboard_set("hi", ClipboardTarget::Primary);
    CHECK(primary == "\x1b]52;p;aGk=\x1b\\");

    // 请求：载荷为空
    CHECK(clipboard_request() == "\x1b]52;c;\x1b\\");

    // 回包解析
    std::string out;
    REQUIRE(parse_clipboard_reply("c;aGk=", out));
    CHECK(out == "hi");

    out = "unchanged";
    CHECK_FALSE(parse_clipboard_reply("c;", out));      // 终端拒绝读取
    CHECK(out == "unchanged");
    CHECK_FALSE(parse_clipboard_reply("", out));
    CHECK_FALSE(parse_clipboard_reply("cc;aGk=", out));  // 缺分隔符
    CHECK_FALSE(parse_clipboard_reply("c;!!!!", out));   // 非法 base64
    CHECK(out == "unchanged");
}

TEST_CASE("term/capability_query: DA 查询序列与回复解析") {
    CHECK(query_da1() == "\x1b[c");
    CHECK(query_da2() == "\x1b[>c");

    DeviceAttributes da;
    REQUIRE(parse_device_attributes("\x1b[?62;4;22c", da));
    CHECK(da.valid);
    CHECK_FALSE(da.secondary);
    CHECK(da.device_class == 62);
    REQUIRE(da.params.size() == 2);
    CHECK(da.params[0] == 4);
    CHECK(da.params[1] == 22);

    // DA2 回复：'>' 前缀
    REQUIRE(parse_device_attributes("\x1b[>1;95;0c", da));
    CHECK(da.secondary);
    CHECK(da.device_class == 1);

    // 容忍不带 ESC、以及大写终止符
    REQUIRE(parse_device_attributes("[?1;2C", da));
    CHECK(da.device_class == 1);
    // 不带 '?' 的裸参数形式也接受
    REQUIRE(parse_device_attributes("\x1b[62c", da));
    CHECK(da.device_class == 62);
    CHECK(da.params.empty());

    // 畸形输入一律拒绝，且不改动输出
    DeviceAttributes keep;
    keep.valid = true;
    keep.device_class = 7;
    CHECK_FALSE(parse_device_attributes("", keep));
    CHECK_FALSE(parse_device_attributes("\x1b[?62;4", keep));   // 缺终止符
    CHECK_FALSE(parse_device_attributes("\x1b[?62;4cxx", keep));  // 终止符后有垃圾
    CHECK_FALSE(parse_device_attributes("\x1b[?;;c", keep));     // 空参数
    CHECK_FALSE(parse_device_attributes("62;4c", keep));         // 缺 '['
    CHECK_FALSE(parse_device_attributes("\x1b[?abcc", keep));    // 非数字
    CHECK(keep.device_class == 7);

    // 只有 device_class、没有扩展参数时，不能凭空"识别"出能力
    DeviceAttributes bare;
    REQUIRE(parse_device_attributes("\x1b[?62c", bare));
    const Da1Features minimal = da1_features(bare);
    CHECK_FALSE(minimal.sixel);
    CHECK_FALSE(minimal.ansi_color);
    CHECK_FALSE(minimal.window_ops);

    // 未解析成功的 DeviceAttributes 一律返回"无能力"
    const Da1Features invalid = da1_features(DeviceAttributes{});
    CHECK_FALSE(invalid.sixel);
    CHECK_FALSE(invalid.ansi_color);

    // 参数里出现 4 / 18 / 22 / 28 才算有对应能力
    DeviceAttributes rich;
    REQUIRE(parse_device_attributes("\x1b[?62;4;18;22;28c", rich));
    const Da1Features full = da1_features(rich);
    CHECK(full.sixel);
    CHECK(full.window_ops);
    CHECK(full.ansi_color);
    CHECK(full.rectangular_ops);

    DeviceAttributes sixel_only;
    REQUIRE(parse_device_attributes("\x1b[?62;4c", sixel_only));
    CHECK(da1_features(sixel_only).sixel);
    CHECK_FALSE(da1_features(sixel_only).rectangular_ops);
}

TEST_CASE("term/color_fallback: 按 TerminalCaps 降级颜色与样式") {
    TerminalCaps caps;
    caps.color_depth = ColorDepth::TrueColor;
    const Color rgb = Color::rgb(91, 143, 249);
    CHECK(depth_of(caps) == ColorDepth::TrueColor);
    CHECK(apply_fallback(rgb, caps) == rgb);

    caps.color_depth = ColorDepth::Ansi256;
    const Color indexed = apply_fallback(rgb, caps);
    CHECK(indexed.kind() == ColorKind::Indexed);
    CHECK(apply_fallback(Color::ansi16(3), caps) == Color::ansi16(3));  // 已可表示

    caps.color_depth = ColorDepth::Ansi16;
    CHECK(apply_fallback(rgb, caps).kind() == ColorKind::Ansi16);
    // 256 色在 16 色终端上要降级
    CHECK(apply_fallback(Color::indexed(200), caps).kind() == ColorKind::Ansi16);

    caps.color_depth = ColorDepth::None;
    CHECK(apply_fallback(rgb, caps).is_none());
    CHECK(apply_fallback(Color::ansi16(1), caps).is_none());

    // 样式：前/背景降级，属性位保持
    caps.color_depth = ColorDepth::Ansi16;
    const Style style = Style{}.set_fg(Color::rgb(10, 20, 30))
                                   .set_bg(Color::rgb(200, 100, 50))
                                   .set_attr(Attr::Bold);
    const Style degraded = apply_fallback(style, caps);
    CHECK(degraded.fg().kind() == ColorKind::Ansi16);
    CHECK(degraded.bg().kind() == ColorKind::Ansi16);
    CHECK(degraded.has_attr(Attr::Bold));
}
