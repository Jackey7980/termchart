// single_include/termchart.hpp —— 由 tools/gen_single_header.py 生成，禁手改。
//
// 由 362 个头文件按依赖拓扑序拼接：层号取 cmake/TermchartModules.cmake，
// 组别为 顶层头 → detail 生成表 → 普通模块头（伞形头殿后），同组按路径字母序决胜；
// 内部 #include 已剥除，系统头按名去重（保序取首次出现）。
// 重跑生成器：python tools/gen_single_header.py（--check 校验一致）。
//
// 用法：把本文件所在目录加入 include 路径后 #include "termchart.hpp"，
// 再链接 termchart 静态库（或自行编译 src/ 下同名实现文件）。
#ifndef TERMCHART_SINGLE_HEADER
#define TERMCHART_SINGLE_HEADER
#pragma once

// 单头用法必然链接静态库/自编译源码：未显式定义时默认补上，
// 使 export.hpp 把 TERMCHART_API 展开为空。
#ifndef TERMCHART_STATIC
#define TERMCHART_STATIC
#endif

// ===== include/termchart/deprecate.hpp =====
// deprecate.hpp —— 弃用标记宏
//
// 弃用流程（docs/contrib/deprecation.md）：
//   第一步加 TERMCHART_DEPRECATED("将于 vX.Y 移除，改用 xxx")，至少一个 minor 周期保留；
//   第二步在下个 major 删除。测试用 -Werror=deprecated-declarations 时，
//   内部调用点必须显式加 TERMCHART_DEPRECATED_IGNORE_BEGIN/END。

#if defined(__GNUC__) || defined(__clang__)
#define TERMCHART_DEPRECATED(msg) __attribute__((deprecated(msg)))
#define TERMCHART_DEPRECATED_IGNORE_BEGIN \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define TERMCHART_DEPRECATED_IGNORE_END _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define TERMCHART_DEPRECATED(msg) __declspec(deprecated(msg))
#define TERMCHART_DEPRECATED_IGNORE_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
#define TERMCHART_DEPRECATED_IGNORE_END __pragma(warning(pop))
#else
#define TERMCHART_DEPRECATED(msg)
#define TERMCHART_DEPRECATED_IGNORE_BEGIN
#define TERMCHART_DEPRECATED_IGNORE_END
#endif

// 枚举值级弃用：保留原值，新增替代值。
#define TERMCHART_DEPRECATED_ENUM(msg) TERMCHART_DEPRECATED(msg)

// ===== include/termchart/export.hpp =====
// export.hpp —— 符号可见性宏
//
// 所有需要跨库边界（或被应用程序直接链接）的符号都标注 TERMCHART_API。
// 静态库构建时为空，避免污染。

#if defined(TERMCHART_STATIC)
#define TERMCHART_API
#define TERMCHART_NO_EXPORT
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(TERMCHART_BUILD_SHARED)
#define TERMCHART_API __declspec(dllexport)
#else
#define TERMCHART_API __declspec(dllimport)
#endif
#define TERMCHART_NO_EXPORT
#else
#define TERMCHART_API __attribute__((visibility("default")))
#define TERMCHART_NO_EXPORT __attribute__((visibility("hidden")))
#endif

// 仅用于注释性标注：该 ABI 面承诺稳定（见 ABI_POLICY.md）。
#define TERMCHART_ABI_STABLE

// ===== include/termchart/fwd.hpp =====
// fwd.hpp —— 公开类型前向声明
//
// 用途：让只"持有指针/引用"的头不必拉进完整定义（降低编译耦合）。
// 前向声明的类型必须与真实声明保持一致（namespace / 模板参数 / class vs struct），
// 否则会有 ODR 风险——新增前向声明时先看真实定义。

#include <cstdint>

namespace termchart {

// ── core ───────────────────────────────────────────────────────────────
struct Rgb;
enum class ColorDepth : std::uint8_t;
enum class Attr : std::uint16_t;
enum class ColorKind : std::uint8_t;
class Color;
class Style;
struct Cell;
class GraphemeCluster;
class CellBuffer;
class Arena;

// ── util ───────────────────────────────────────────────────────────────
namespace util {
class Rng;
}  // namespace util

// ── color ──────────────────────────────────────────────────────────────
namespace color {
enum class ColormapKind : std::uint8_t;
enum class PaletteKind : std::uint8_t;
class Gradient;
}  // namespace color

// ── term ───────────────────────────────────────────────────────────────
namespace term {
enum class ImageProtocolKind : std::uint8_t;
struct CapEnvironment;
struct TerminalCaps;
struct TerminalSize;
struct TerminalOptions;
class Terminal;
class RawMode;
class Waker;
}  // namespace term

// ── render ─────────────────────────────────────────────────────────────
namespace render {
class DamageTracker;
class Renderer;
}  // namespace render

// ── api ────────────────────────────────────────────────────────────────
namespace api {
// 全库约定是 `XxxOptions`（graph/layout/diagram 全都如此），api 侧旧名 `Opts`
// 是唯一例外，已收敛为 `ApiOptions`；`Opts` 保留为兼容别名。
class ApiOptions;
using Opts = ApiOptions;
}  // namespace api

}  // namespace termchart

// ===== include/termchart/version.hpp =====
// version.hpp —— 版本宏
//
// 与本轮的库版本同源：CMake 侧通过 target_compile_definitions 注入同值宏，
// 头文件侧的默认值用于"单头文件 / 直接拷 assume 头文件"场景。

#define TERMCHART_VERSION_MAJOR 0
#define TERMCHART_VERSION_MINOR 1
#define TERMCHART_VERSION_PATCH 0

#define TERMCHART_VERSION_STRING "0.1.0"

// ABI 面 strip 到 major.minor：minor 之内不破坏（见 ABI_POLICY.md）。
#define TERMCHART_ABI_VERSION (TERMCHART_VERSION_MAJOR * 100 + TERMCHART_VERSION_MINOR)

// ===== include/termchart/detail/ansi_tables.hpp =====
// ansi_tables.hpp —— ANSI / 控制字符查表（层号 -1 横切数据表）
//
// 数据表职责：C0 控制码 ↔ 脱字符记号（^@、^[…）与助记名，以及 ESC 序列前缀。
// **不负责生成着色序列** —— SGR 的唯一生成点是 render/sgr.hpp（见 ROADMAP 阶段 1）。
// 本文件不依赖 termchart 其余文件；任何层都可 include。

#include <array>
#include <string_view>

namespace termchart::detail {

// C0 控制码数量：0x00–0x1F 外加 0x7F（DEL）。
inline constexpr int kC0Count = 33;

struct C0Entry {
    std::uint8_t code;
    char32_t caret;          // 脱字符记号里的字母，例如 0x01 -> 'A'
    std::string_view name;   // 助记名，例如 "SOH"
};

// clang-format off
inline constexpr std::array<C0Entry, kC0Count> kC0Table{{
    {0x00, U'@', "NUL"}, {0x01, U'A', "SOH"}, {0x02, U'B', "STX"}, {0x03, U'C', "ETX"},
    {0x04, U'D', "EOT"}, {0x05, U'E', "ENQ"}, {0x06, U'F', "ACK"}, {0x07, U'G', "BEL"},
    {0x08, U'H', "BS"},  {0x09, U'I', "HT"},  {0x0A, U'J', "LF"},  {0x0B, U'K', "VT"},
    {0x0C, U'L', "FF"},  {0x0D, U'M', "CR"},  {0x0E, U'N', "SO"},  {0x0F, U'O', "SI"},
    {0x10, U'P', "DLE"}, {0x11, U'Q', "DC1"}, {0x12, U'R', "DC2"}, {0x13, U'S', "DC3"},
    {0x14, U'T', "DC4"}, {0x15, U'U', "NAK"}, {0x16, U'V', "SYN"}, {0x17, U'W', "ETB"},
    {0x18, U'X', "CAN"}, {0x19, U'Y', "EM"},  {0x1A, U'Z', "SUB"}, {0x1B, U'[', "ESC"},
    {0x1C, U'\\', "FS"}, {0x1D, U']', "GS"},  {0x1E, U'^', "RS"},  {0x1F, U'_', "US"},
    {0x7F, U'?', "DEL"},
}};
// clang-format on

// 是否为需要"可视化转义"的控制码。
constexpr bool is_c0(std::uint8_t code) noexcept {
    return code <= 0x1Fu || code == 0x7Fu;
}

// 控制码 → 助记名；非控制码返回空视图。
constexpr std::string_view c0_name(std::uint8_t code) noexcept {
    for (const C0Entry& e : kC0Table) {
        if (e.code == code) {
            return e.name;
        }
    }
    return {};
}

// 控制码 → 脱字符记号里的字母（0x01 -> 'A'）。非控制码返回 0。
constexpr char32_t c0_caret_letter(std::uint8_t code) noexcept {
    for (const C0Entry& e : kC0Table) {
        if (e.code == code) {
            return e.caret;
        }
    }
    return 0;
}

// ESC 与其序列引入符：不是 SGR，不涉及颜色。
inline constexpr char32_t kEscape = 0x1B;
inline constexpr std::string_view kCsiPrefix = "\x1B[";
inline constexpr std::string_view kOscPrefix = "\x1B]";
inline constexpr std::string_view kSt = "\x1B\\";

}  // namespace termchart::detail

// ===== include/termchart/detail/block_chars.hpp =====
// block_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 块元素（U+2580 系）的唯一来源：raster/block、chart/progress、indicator/* 共用。


namespace termchart::detail {

inline constexpr char32_t kSpace = 0x20;
inline constexpr char32_t kUpperHalf = 0x2580;
inline constexpr char32_t kLowerHalf = 0x2584;
inline constexpr char32_t kLeftHalf = 0x258C;
inline constexpr char32_t kRightHalf = 0x2590;
inline constexpr char32_t kFullBlock = 0x2588;
inline constexpr char32_t kLightShade = 0x2591;
inline constexpr char32_t kMediumShade = 0x2592;
inline constexpr char32_t kDarkShade = 0x2593;

// 下沿填充：[0] = 空格，[n] = 高度占 n/8 的下块。
inline constexpr std::array<char32_t, 9> kLowerBlocks{{
    0x0020, 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588,
}};

// 左侧填充：[0] = 空格，[n] = 宽度占 n/8 的左块。
inline constexpr std::array<char32_t, 9> kLeftBlocks{{
    0x0020, 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589, 0x2588,
}};

// 密度阶梯（raster/ascii 用）：由淡到浓。
inline constexpr std::array<char32_t, 10> kDensityChars{{
    0x20, 0x2E, 0x3A, 0x2D, 0x3D, 0x2B, 0x2A, 0x23, 0x25, 0x40,
}};

constexpr char32_t lower_eighth(int n) noexcept {
    const int clamped = n < 0 ? 0 : (n > 8 ? 8 : n);
    return kLowerBlocks[static_cast<std::size_t>(clamped)];
}

constexpr char32_t left_eighth(int n) noexcept {
    const int clamped = n < 0 ? 0 : (n > 8 ? 8 : n);
    return kLeftBlocks[static_cast<std::size_t>(clamped)];
}

constexpr char32_t density_char(double density) noexcept {
    if (!(density > 0.0)) {
        return kSpace;
    }
    const double scaled = density * 9.0 + 0.5;
    const int i = scaled > 9.0 ? 9 : static_cast<int>(scaled);
    return kDensityChars[static_cast<std::size_t>(i)];
}

}  // namespace termchart::detail

// ===== include/termchart/detail/box_chars.hpp =====
// box_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 制表符（U+2500 系）的唯一来源：graph/wiregrid、layout/box、widget/border 都走这里，
// 不允许在别处硬编码 U+2500 序列。
//
// 邻接掩码约定（bitOR）：Up = 1, Right = 2, Down = 4, Left = 8


namespace termchart::detail {

enum class BoxMask : std::uint8_t {
    None = 0,
    Up = 1,
    Right = 2,
    Down = 4,
    Left = 8,
};

constexpr std::uint8_t operator|(BoxMask a, BoxMask b) noexcept {
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

enum class BoxWeight : std::uint8_t {
    Light = 0,
    Heavy = 1,
    Double = 2,
    Round = 3,   // 圆角：仅四个直角掩码有专用码点，其余退化为 Light
};

// clang-format off
// 索引：[weight][mask]
inline constexpr std::array<std::array<char32_t, 16>, 4> kBoxChars{{
    {0x0020, 0x2575, 0x2576, 0x2514, 0x2577, 0x2502, 0x250C, 0x251C,
     0x2574, 0x2518, 0x2500, 0x2534, 0x2510, 0x2524, 0x252C, 0x253C},  // Light
    {0x0020, 0x2579, 0x257A, 0x2517, 0x257B, 0x2503, 0x250F, 0x2523,
     0x2578, 0x251B, 0x2501, 0x253B, 0x2513, 0x252B, 0x2533, 0x254B},  // Heavy
    {0x0020, 0x2551, 0x2550, 0x255A, 0x2551, 0x2551, 0x2554, 0x2560,
     0x2550, 0x255D, 0x2550, 0x2569, 0x2557, 0x2563, 0x2566, 0x256C},  // Double
    {0x0020, 0x2575, 0x2576, 0x2570, 0x2577, 0x2502, 0x256D, 0x251C,
     0x2574, 0x256F, 0x2500, 0x2534, 0x256E, 0x2524, 0x252C, 0x253C},  // Round
}};
// clang-format on

constexpr char32_t box_char(std::uint8_t mask, BoxWeight weight = BoxWeight::Light) noexcept {
    return kBoxChars[static_cast<std::size_t>(weight)][mask & 0x0Fu];
}

inline constexpr char32_t kHorizontalLight = 0x2500;
inline constexpr char32_t kVerticalLight = 0x2502;

}  // namespace termchart::detail

// ===== include/termchart/detail/braille_table.hpp =====
// braille_table.hpp —— 由 tools/gen_braille_table.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
// 盲文点阵 vil：**点 i ↔ bit (i-1)**，码点 = U+2800 | dots，这一约定只在本处定义。


namespace termchart::detail {

// 点 i（1..8）的位掩码。
inline constexpr std::uint8_t kBrailleDot1 = 0x01;
inline constexpr std::uint8_t kBrailleDot2 = 0x02;
inline constexpr std::uint8_t kBrailleDot3 = 0x04;
inline constexpr std::uint8_t kBrailleDot4 = 0x08;
inline constexpr std::uint8_t kBrailleDot5 = 0x10;
inline constexpr std::uint8_t kBrailleDot6 = 0x20;
inline constexpr std::uint8_t kBrailleDot7 = 0x40;
inline constexpr std::uint8_t kBrailleDot8 = 0x80;

// 码点 = U+2800 | dots（dots 为上述位掩码的或）。查表仅为显式化该约定。
// clang-format off
inline constexpr std::array<char32_t, 256> kBrailleChars{
    U'\u2800', U'\u2801', U'\u2802', U'\u2803', U'\u2804', U'\u2805', U'\u2806', U'\u2807',
    U'\u2808', U'\u2809', U'\u280A', U'\u280B', U'\u280C', U'\u280D', U'\u280E', U'\u280F',
    U'\u2810', U'\u2811', U'\u2812', U'\u2813', U'\u2814', U'\u2815', U'\u2816', U'\u2817',
    U'\u2818', U'\u2819', U'\u281A', U'\u281B', U'\u281C', U'\u281D', U'\u281E', U'\u281F',
    U'\u2820', U'\u2821', U'\u2822', U'\u2823', U'\u2824', U'\u2825', U'\u2826', U'\u2827',
    U'\u2828', U'\u2829', U'\u282A', U'\u282B', U'\u282C', U'\u282D', U'\u282E', U'\u282F',
    U'\u2830', U'\u2831', U'\u2832', U'\u2833', U'\u2834', U'\u2835', U'\u2836', U'\u2837',
    U'\u2838', U'\u2839', U'\u283A', U'\u283B', U'\u283C', U'\u283D', U'\u283E', U'\u283F',
    U'\u2840', U'\u2841', U'\u2842', U'\u2843', U'\u2844', U'\u2845', U'\u2846', U'\u2847',
    U'\u2848', U'\u2849', U'\u284A', U'\u284B', U'\u284C', U'\u284D', U'\u284E', U'\u284F',
    U'\u2850', U'\u2851', U'\u2852', U'\u2853', U'\u2854', U'\u2855', U'\u2856', U'\u2857',
    U'\u2858', U'\u2859', U'\u285A', U'\u285B', U'\u285C', U'\u285D', U'\u285E', U'\u285F',
    U'\u2860', U'\u2861', U'\u2862', U'\u2863', U'\u2864', U'\u2865', U'\u2866', U'\u2867',
    U'\u2868', U'\u2869', U'\u286A', U'\u286B', U'\u286C', U'\u286D', U'\u286E', U'\u286F',
    U'\u2870', U'\u2871', U'\u2872', U'\u2873', U'\u2874', U'\u2875', U'\u2876', U'\u2877',
    U'\u2878', U'\u2879', U'\u287A', U'\u287B', U'\u287C', U'\u287D', U'\u287E', U'\u287F',
    U'\u2880', U'\u2881', U'\u2882', U'\u2883', U'\u2884', U'\u2885', U'\u2886', U'\u2887',
    U'\u2888', U'\u2889', U'\u288A', U'\u288B', U'\u288C', U'\u288D', U'\u288E', U'\u288F',
    U'\u2890', U'\u2891', U'\u2892', U'\u2893', U'\u2894', U'\u2895', U'\u2896', U'\u2897',
    U'\u2898', U'\u2899', U'\u289A', U'\u289B', U'\u289C', U'\u289D', U'\u289E', U'\u289F',
    U'\u28A0', U'\u28A1', U'\u28A2', U'\u28A3', U'\u28A4', U'\u28A5', U'\u28A6', U'\u28A7',
    U'\u28A8', U'\u28A9', U'\u28AA', U'\u28AB', U'\u28AC', U'\u28AD', U'\u28AE', U'\u28AF',
    U'\u28B0', U'\u28B1', U'\u28B2', U'\u28B3', U'\u28B4', U'\u28B5', U'\u28B6', U'\u28B7',
    U'\u28B8', U'\u28B9', U'\u28BA', U'\u28BB', U'\u28BC', U'\u28BD', U'\u28BE', U'\u28BF',
    U'\u28C0', U'\u28C1', U'\u28C2', U'\u28C3', U'\u28C4', U'\u28C5', U'\u28C6', U'\u28C7',
    U'\u28C8', U'\u28C9', U'\u28CA', U'\u28CB', U'\u28CC', U'\u28CD', U'\u28CE', U'\u28CF',
    U'\u28D0', U'\u28D1', U'\u28D2', U'\u28D3', U'\u28D4', U'\u28D5', U'\u28D6', U'\u28D7',
    U'\u28D8', U'\u28D9', U'\u28DA', U'\u28DB', U'\u28DC', U'\u28DD', U'\u28DE', U'\u28DF',
    U'\u28E0', U'\u28E1', U'\u28E2', U'\u28E3', U'\u28E4', U'\u28E5', U'\u28E6', U'\u28E7',
    U'\u28E8', U'\u28E9', U'\u28EA', U'\u28EB', U'\u28EC', U'\u28ED', U'\u28EE', U'\u28EF',
    U'\u28F0', U'\u28F1', U'\u28F2', U'\u28F3', U'\u28F4', U'\u28F5', U'\u28F6', U'\u28F7',
    U'\u28F8', U'\u28F9', U'\u28FA', U'\u28FB', U'\u28FC', U'\u28FD', U'\u28FE', U'\u28FF'
};
// clang-format on

constexpr char32_t braille_char(std::uint8_t dots) noexcept {
    return kBrailleChars[dots];
}

// 2×4 子像素网格 → 点掩码。约定几何排布：
//     列 0（左）→ dot1/dot2/dot3/dot7     列 1（右）→ dot4/dot5/dot6/dot8
//     行序自上而下：dot1,dot2,dot3 后接 dot7（左列），dot4,dot5,dot6 后接 dot8（右列）
inline constexpr std::array<std::uint8_t, 8> kBrailleCellDots{
    kBrailleDot1,  // 左上  (row 0, col 0)
    kBrailleDot2,  // 左中上(row 1, col 0)
    kBrailleDot3,  // 左中下(row 2, col 0)
    kBrailleDot7,  // 左下  (row 3, col 0)
    kBrailleDot4,  // 右上  (row 0, col 1)
    kBrailleDot5,  // 右中上(row 1, col 1)
    kBrailleDot6,  // 右中下(row 2, col 1)
    kBrailleDot8,  // 右下  (row 3, col 1)
};

// 子像素线性下标（col * 4 + row，先列后行）→ 点掩码。越界返回 0（不点亮）。
constexpr std::uint8_t braille_dots_for_cell(int col, int row) noexcept {
    if (col < 0 || col > 1 || row < 0 || row > 3) {
        return 0;
    }
    return kBrailleCellDots[static_cast<std::size_t>(col) * 4 + static_cast<std::size_t>(row)];
}

// 清空全部点（空白盲文）：U+2800。
inline constexpr char32_t kBrailleBlank = U'\u2800';

}  // namespace termchart::detail

// ===== include/termchart/detail/grapheme_tables.hpp =====
// grapheme_tables.hpp —— 由 tools/gen_grapheme_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
// 只提供 Grapheme_Cluster_Break 属性（UAX #29 简化版）的区间数据。
// 分割算法在 util/grapheme.hpp（L1）；core/grapheme.hpp 只定义数据结构。

#include <cstddef>

namespace termchart::detail {

// Grapheme_Cluster_Break 属性值。取值与 UAX #29 同名。
enum class Gb : std::uint8_t {
    Any = 0,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RegionalIndicator,
    Prepend,
    SpacingMark,
    L,          // Hangul 初声
    V,          // Hangul 中声
    T,          // Hangul 终声
    LV,         // Hangul LV 音节
    LVT,        // Hangul LVT 音节
};

// 区间表项：[first, last] 闭区间。
struct GbRange {
    std::uint32_t first;
    std::uint32_t last;
    Gb value;
};

// clang-format off
inline constexpr std::size_t kGbRangeCount = 1138;
inline constexpr std::array<GbRange, kGbRangeCount> kGbRanges{{
    {0x0000, 0x0009, Gb::Control},
    {0x000A, 0x000A, Gb::LF},
    {0x000B, 0x000C, Gb::Control},
    {0x000D, 0x000D, Gb::CR},
    {0x000E, 0x001F, Gb::Control},
    {0x007F, 0x009F, Gb::Control},
    {0x00AD, 0x00AD, Gb::Control},
    {0x0300, 0x036F, Gb::Extend},
    {0x0378, 0x0378, Gb::Control},
    {0x0483, 0x0489, Gb::Extend},
    {0x0591, 0x05BD, Gb::Extend},
    {0x05BF, 0x05BF, Gb::Extend},
    {0x05C1, 0x05C2, Gb::Extend},
    {0x0600, 0x0605, Gb::Prepend},
    {0x0610, 0x061A, Gb::Extend},
    {0x061C, 0x061C, Gb::Control},
    {0x064B, 0x065F, Gb::Extend},
    {0x0670, 0x0670, Gb::Extend},
    {0x06D6, 0x06DC, Gb::Extend},
    {0x06DD, 0x06DD, Gb::Prepend},
    {0x06DF, 0x06E4, Gb::Extend},
    {0x06E7, 0x06E8, Gb::Extend},
    {0x070F, 0x070F, Gb::Prepend},
    {0x0711, 0x0711, Gb::Extend},
    {0x0730, 0x074A, Gb::Extend},
    {0x07A6, 0x07B0, Gb::Extend},
    {0x07EB, 0x07F3, Gb::Extend},
    {0x0816, 0x0819, Gb::Extend},
    {0x081B, 0x0823, Gb::Extend},
    {0x0825, 0x0827, Gb::Extend},
    {0x0829, 0x082D, Gb::Extend},
    {0x0859, 0x085B, Gb::Extend},
    {0x0890, 0x0891, Gb::Prepend},
    {0x08B5, 0x08B5, Gb::Control},
    {0x08D4, 0x08E1, Gb::Extend},
    {0x08E2, 0x08E2, Gb::Prepend},
    {0x08E3, 0x0902, Gb::Extend},
    {0x0903, 0x0903, Gb::SpacingMark},
    {0x093A, 0x093A, Gb::Extend},
    {0x093B, 0x093B, Gb::SpacingMark},
    {0x093C, 0x093C, Gb::Extend},
    {0x093E, 0x0940, Gb::SpacingMark},
    {0x0941, 0x0948, Gb::Extend},
    {0x0949, 0x094C, Gb::SpacingMark},
    {0x094D, 0x094D, Gb::Extend},
    {0x094E, 0x094F, Gb::SpacingMark},
    {0x0951, 0x0957, Gb::Extend},
    {0x0962, 0x0963, Gb::Extend},
    {0x0981, 0x0981, Gb::Extend},
    {0x0982, 0x0983, Gb::SpacingMark},
    {0x09BC, 0x09BC, Gb::Extend},
    {0x09BE, 0x09C0, Gb::SpacingMark},
    {0x09C1, 0x09C4, Gb::Extend},
    {0x09C7, 0x09C8, Gb::SpacingMark},
    {0x09CB, 0x09CC, Gb::SpacingMark},
    {0x09CD, 0x09CD, Gb::Extend},
    {0x09D7, 0x09D7, Gb::SpacingMark},
    {0x0A01, 0x0A02, Gb::Extend},
    {0x0A03, 0x0A03, Gb::SpacingMark},
    {0x0A3C, 0x0A3C, Gb::Extend},
    {0x0A3E, 0x0A40, Gb::SpacingMark},
    {0x0A41, 0x0A42, Gb::Extend},
    {0x0A47, 0x0A48, Gb::Extend},
    {0x0A4B, 0x0A4D, Gb::Extend},
    {0x0A70, 0x0A71, Gb::Extend},
    {0x0A81, 0x0A82, Gb::Extend},
    {0x0A83, 0x0A83, Gb::SpacingMark},
    {0x0ABC, 0x0ABC, Gb::Extend},
    {0x0ABE, 0x0AC0, Gb::SpacingMark},
    {0x0AC1, 0x0AC5, Gb::Extend},
    {0x0AC7, 0x0AC8, Gb::Extend},
    {0x0AC9, 0x0AC9, Gb::SpacingMark},
    {0x0ACB, 0x0ACC, Gb::SpacingMark},
    {0x0ACD, 0x0ACD, Gb::Extend},
    {0x0B01, 0x0B01, Gb::Extend},
    {0x0B02, 0x0B03, Gb::SpacingMark},
    {0x0B3C, 0x0B3C, Gb::Extend},
    {0x0B3E, 0x0B3E, Gb::SpacingMark},
    {0x0B3F, 0x0B3F, Gb::Extend},
    {0x0B40, 0x0B40, Gb::SpacingMark},
    {0x0B41, 0x0B44, Gb::Extend},
    {0x0B47, 0x0B48, Gb::SpacingMark},
    {0x0B4B, 0x0B4C, Gb::SpacingMark},
    {0x0B4D, 0x0B4D, Gb::Extend},
    {0x0B82, 0x0B82, Gb::Extend},
    {0x0BBE, 0x0BBF, Gb::SpacingMark},
    {0x0BC0, 0x0BC0, Gb::Extend},
    {0x0BC1, 0x0BC2, Gb::SpacingMark},
    {0x0BC6, 0x0BC8, Gb::SpacingMark},
    {0x0BCA, 0x0BCC, Gb::SpacingMark},
    {0x0BCD, 0x0BCD, Gb::Extend},
    {0x0BD7, 0x0BD7, Gb::SpacingMark},
    {0x0C00, 0x0C00, Gb::Extend},
    {0x0C01, 0x0C03, Gb::SpacingMark},
    {0x0C04, 0x0C04, Gb::Extend},
    {0x0C3E, 0x0C40, Gb::Extend},
    {0x0C41, 0x0C44, Gb::SpacingMark},
    {0x0C46, 0x0C48, Gb::Extend},
    {0x0C4A, 0x0C4D, Gb::Extend},
    {0x0C82, 0x0C83, Gb::SpacingMark},
    {0x0CBE, 0x0CBE, Gb::SpacingMark},
    {0x0CC0, 0x0CC4, Gb::SpacingMark},
    {0x0CC7, 0x0CC8, Gb::SpacingMark},
    {0x0CCA, 0x0CCB, Gb::SpacingMark},
    {0x0CD5, 0x0CD6, Gb::SpacingMark},
    {0x0D01, 0x0D01, Gb::Extend},
    {0x0D02, 0x0D03, Gb::SpacingMark},
    {0x0D3B, 0x0D3C, Gb::Extend},
    {0x0D3E, 0x0D40, Gb::SpacingMark},
    {0x0D41, 0x0D44, Gb::Extend},
    {0x0D46, 0x0D48, Gb::SpacingMark},
    {0x0D4A, 0x0D4C, Gb::SpacingMark},
    {0x0D4D, 0x0D4D, Gb::Extend},
    {0x0D4E, 0x0D4E, Gb::Prepend},
    {0x0D57, 0x0D57, Gb::SpacingMark},
    {0x0D82, 0x0D83, Gb::SpacingMark},
    {0x0DCF, 0x0DD1, Gb::SpacingMark},
    {0x0DD8, 0x0DDF, Gb::SpacingMark},
    {0x0DF2, 0x0DF3, Gb::SpacingMark},
    {0x0E31, 0x0E31, Gb::Extend},
    {0x0E33, 0x0E33, Gb::SpacingMark},
    {0x0E34, 0x0E3A, Gb::Extend},
    {0x0E47, 0x0E4E, Gb::Extend},
    {0x0EB1, 0x0EB1, Gb::Extend},
    {0x0EB3, 0x0EB3, Gb::SpacingMark},
    {0x0EB4, 0x0EBC, Gb::Extend},
    {0x0EC8, 0x0ECD, Gb::Extend},
    {0x0F18, 0x0F19, Gb::Extend},
    {0x0F35, 0x0F35, Gb::Extend},
    {0x0F37, 0x0F37, Gb::Extend},
    {0x0F39, 0x0F39, Gb::Extend},
    {0x0F3E, 0x0F3F, Gb::SpacingMark},
    {0x0F71, 0x0F7E, Gb::Extend},
    {0x0F7F, 0x0F7F, Gb::SpacingMark},
    {0x0F80, 0x0F84, Gb::Extend},
    {0x0F86, 0x0F87, Gb::Extend},
    {0x0FB0, 0x0FB4, Gb::Extend},
    {0x102B, 0x102C, Gb::SpacingMark},
    {0x102D, 0x1030, Gb::Extend},
    {0x1031, 0x1031, Gb::SpacingMark},
    {0x1032, 0x1037, Gb::Extend},
    {0x1038, 0x1038, Gb::SpacingMark},
    {0x1039, 0x103A, Gb::Extend},
    {0x103B, 0x103C, Gb::SpacingMark},
    {0x103D, 0x103E, Gb::Extend},
    {0x1056, 0x1057, Gb::SpacingMark},
    {0x1058, 0x1059, Gb::Extend},
    {0x105E, 0x1060, Gb::Extend},
    {0x1062, 0x1064, Gb::SpacingMark},
    {0x1067, 0x106D, Gb::SpacingMark},
    {0x1071, 0x1074, Gb::Extend},
    {0x1082, 0x1082, Gb::Extend},
    {0x1083, 0x1084, Gb::SpacingMark},
    {0x108F, 0x108F, Gb::SpacingMark},
    {0x109A, 0x109B, Gb::SpacingMark},
    {0x1100, 0x115F, Gb::L},
    {0x1160, 0x11A7, Gb::V},
    {0x11A8, 0x11FF, Gb::T},
    {0x135D, 0x135F, Gb::Extend},
    {0x1712, 0x1714, Gb::Extend},
    {0x1732, 0x1734, Gb::Extend},
    {0x1752, 0x1753, Gb::Extend},
    {0x1772, 0x1773, Gb::Extend},
    {0x17B4, 0x17B5, Gb::Extend},
    {0x17B6, 0x17B6, Gb::SpacingMark},
    {0x17B7, 0x17BD, Gb::Extend},
    {0x17BE, 0x17C5, Gb::SpacingMark},
    {0x17C6, 0x17C6, Gb::Extend},
    {0x17C7, 0x17C8, Gb::SpacingMark},
    {0x17C9, 0x17D3, Gb::Extend},
    {0x17DD, 0x17DD, Gb::Extend},
    {0x180B, 0x180D, Gb::Extend},
    {0x18A9, 0x18A9, Gb::Extend},
    {0x1920, 0x1922, Gb::Extend},
    {0x1923, 0x1926, Gb::SpacingMark},
    {0x1927, 0x1928, Gb::Extend},
    {0x1929, 0x192B, Gb::SpacingMark},
    {0x1930, 0x1931, Gb::SpacingMark},
    {0x1932, 0x1932, Gb::Extend},
    {0x1933, 0x1938, Gb::SpacingMark},
    {0x1939, 0x193B, Gb::Extend},
    {0x19B0, 0x19C0, Gb::SpacingMark},
    {0x19C8, 0x19C9, Gb::SpacingMark},
    {0x1A17, 0x1A18, Gb::Extend},
    {0x1A19, 0x1A1A, Gb::SpacingMark},
    {0x1A1B, 0x1A1B, Gb::Extend},
    {0x1A55, 0x1A55, Gb::SpacingMark},
    {0x1A56, 0x1A56, Gb::Extend},
    {0x1A57, 0x1A57, Gb::SpacingMark},
    {0x1A58, 0x1A5E, Gb::Extend},
    {0x1A60, 0x1A60, Gb::Extend},
    {0x1A61, 0x1A61, Gb::SpacingMark},
    {0x1A62, 0x1A62, Gb::Extend},
    {0x1A63, 0x1A64, Gb::SpacingMark},
    {0x1A65, 0x1A6C, Gb::Extend},
    {0x1A6D, 0x1A72, Gb::SpacingMark},
    {0x1A73, 0x1A7C, Gb::Extend},
    {0x1A7F, 0x1A7F, Gb::Extend},
    {0x1AB0, 0x1ACE, Gb::Extend},
    {0x1B00, 0x1B03, Gb::Extend},
    {0x1B04, 0x1B04, Gb::SpacingMark},
    {0x1B34, 0x1B34, Gb::Extend},
    {0x1B35, 0x1B35, Gb::SpacingMark},
    {0x1B36, 0x1B3A, Gb::Extend},
    {0x1B3B, 0x1B3B, Gb::SpacingMark},
    {0x1B3C, 0x1B3C, Gb::Extend},
    {0x1B3D, 0x1B41, Gb::SpacingMark},
    {0x1B42, 0x1B42, Gb::Extend},
    {0x1B43, 0x1B44, Gb::SpacingMark},
    {0x1B6B, 0x1B73, Gb::Extend},
    {0x1B80, 0x1B81, Gb::Extend},
    {0x1B82, 0x1B82, Gb::SpacingMark},
    {0x1BA1, 0x1BA1, Gb::SpacingMark},
    {0x1BA2, 0x1BA5, Gb::Extend},
    {0x1BA6, 0x1BA7, Gb::SpacingMark},
    {0x1BA8, 0x1BA9, Gb::Extend},
    {0x1BAA, 0x1BAA, Gb::SpacingMark},
    {0x1BAB, 0x1BAD, Gb::Extend},
    {0x1BE6, 0x1BE6, Gb::Extend},
    {0x1BE7, 0x1BE7, Gb::SpacingMark},
    {0x1BE8, 0x1BE9, Gb::Extend},
    {0x1BEA, 0x1BEC, Gb::SpacingMark},
    {0x1BED, 0x1BED, Gb::Extend},
    {0x1BEE, 0x1BEE, Gb::SpacingMark},
    {0x1BEF, 0x1BF1, Gb::Extend},
    {0x1BF2, 0x1BF3, Gb::SpacingMark},
    {0x1C24, 0x1C2B, Gb::SpacingMark},
    {0x1C2C, 0x1C33, Gb::Extend},
    {0x1C34, 0x1C35, Gb::SpacingMark},
    {0x1C36, 0x1C37, Gb::Extend},
    {0x1CD0, 0x1CD2, Gb::Extend},
    {0x1CD4, 0x1CE0, Gb::Extend},
    {0x1CE1, 0x1CE1, Gb::SpacingMark},
    {0x1CE2, 0x1CE8, Gb::Extend},
    {0x1CED, 0x1CED, Gb::Extend},
    {0x1CF2, 0x1CF3, Gb::SpacingMark},
    {0x1CF4, 0x1CF4, Gb::Extend},
    {0x1CF7, 0x1CF7, Gb::SpacingMark},
    {0x1CF8, 0x1CF9, Gb::Extend},
    {0x1DC0, 0x1DF5, Gb::Extend},
    {0x1DFC, 0x1DFF, Gb::Extend},
    {0x200D, 0x200D, Gb::ZWJ},
    {0x20D0, 0x20F0, Gb::Extend},
    {0x2CEF, 0x2CF1, Gb::Extend},
    {0x2D7F, 0x2D7F, Gb::Extend},
    {0x2DE0, 0x2DFF, Gb::Extend},
    {0x302E, 0x302F, Gb::SpacingMark},
    {0x3099, 0x309A, Gb::Extend},
    {0xA66F, 0xA672, Gb::Extend},
    {0xA674, 0xA67D, Gb::Extend},
    {0xA69E, 0xA69F, Gb::Extend},
    {0xA6F0, 0xA6F1, Gb::Extend},
    {0xA802, 0xA802, Gb::Extend},
    {0xA806, 0xA806, Gb::Extend},
    {0xA80B, 0xA80B, Gb::Extend},
    {0xA823, 0xA824, Gb::SpacingMark},
    {0xA825, 0xA826, Gb::Extend},
    {0xA827, 0xA827, Gb::SpacingMark},
    {0xA880, 0xA881, Gb::SpacingMark},
    {0xA8B4, 0xA8C3, Gb::SpacingMark},
    {0xA8C4, 0xA8C5, Gb::Extend},
    {0xA8E0, 0xA8F1, Gb::Extend},
    {0xA926, 0xA92D, Gb::Extend},
    {0xA947, 0xA951, Gb::Extend},
    {0xA952, 0xA953, Gb::SpacingMark},
    {0xA960, 0xA97C, Gb::L},
    {0xA980, 0xA982, Gb::Extend},
    {0xA983, 0xA983, Gb::SpacingMark},
    {0xA9B3, 0xA9B3, Gb::Extend},
    {0xA9B4, 0xA9B5, Gb::SpacingMark},
    {0xA9B6, 0xA9B9, Gb::Extend},
    {0xA9BA, 0xA9BB, Gb::SpacingMark},
    {0xA9BC, 0xA9BC, Gb::Extend},
    {0xA9BD, 0xA9C0, Gb::SpacingMark},
    {0xA9E5, 0xA9E5, Gb::Extend},
    {0xAA29, 0xAA2E, Gb::Extend},
    {0xAA2F, 0xAA30, Gb::SpacingMark},
    {0xAA31, 0xAA32, Gb::Extend},
    {0xAA33, 0xAA34, Gb::SpacingMark},
    {0xAA35, 0xAA36, Gb::Extend},
    {0xAA43, 0xAA43, Gb::Extend},
    {0xAA4C, 0xAA4C, Gb::Extend},
    {0xAA4D, 0xAA4F, Gb::SpacingMark},
    {0xAA7B, 0xAA7B, Gb::SpacingMark},
    {0xAA7C, 0xAA7C, Gb::Extend},
    {0xAA7D, 0xAA7D, Gb::SpacingMark},
    {0xAAB0, 0xAAB0, Gb::Extend},
    {0xAAB1, 0xAAB1, Gb::SpacingMark},
    {0xAAB2, 0xAAB4, Gb::Extend},
    {0xAAB5, 0xAAB6, Gb::SpacingMark},
    {0xAAB7, 0xAAB8, Gb::Extend},
    {0xAAB9, 0xAABD, Gb::SpacingMark},
    {0xAABE, 0xAABF, Gb::Extend},
    {0xAAC0, 0xAAC0, Gb::SpacingMark},
    {0xAAC1, 0xAAC1, Gb::Extend},
    {0xAAC2, 0xAAC2, Gb::SpacingMark},
    {0xAAEC, 0xAAED, Gb::Extend},
    {0xAAF5, 0xAAF5, Gb::SpacingMark},
    {0xAAF6, 0xAAF6, Gb::Extend},
    {0xABE3, 0xABE4, Gb::SpacingMark},
    {0xABE5, 0xABE5, Gb::Extend},
    {0xABE6, 0xABE7, Gb::SpacingMark},
    {0xABE8, 0xABE8, Gb::Extend},
    {0xABE9, 0xABEA, Gb::SpacingMark},
    {0xABEC, 0xABEC, Gb::SpacingMark},
    {0xABED, 0xABED, Gb::Extend},
    {0xAC00, 0xAC00, Gb::LV},
    {0xAC01, 0xAC1B, Gb::LVT},
    {0xAC1C, 0xAC1C, Gb::LV},
    {0xAC1D, 0xAC37, Gb::LVT},
    {0xAC38, 0xAC38, Gb::LV},
    {0xAC39, 0xAC53, Gb::LVT},
    {0xAC54, 0xAC54, Gb::LV},
    {0xAC55, 0xAC6F, Gb::LVT},
    {0xAC70, 0xAC70, Gb::LV},
    {0xAC71, 0xAC8B, Gb::LVT},
    {0xAC8C, 0xAC8C, Gb::LV},
    {0xAC8D, 0xACA7, Gb::LVT},
    {0xACA8, 0xACA8, Gb::LV},
    {0xACA9, 0xACC3, Gb::LVT},
    {0xACC4, 0xACC4, Gb::LV},
    {0xACC5, 0xACDF, Gb::LVT},
    {0xACE0, 0xACE0, Gb::LV},
    {0xACE1, 0xACFB, Gb::LVT},
    {0xACFC, 0xACFC, Gb::LV},
    {0xACFD, 0xAD17, Gb::LVT},
    {0xAD18, 0xAD18, Gb::LV},
    {0xAD19, 0xAD33, Gb::LVT},
    {0xAD34, 0xAD34, Gb::LV},
    {0xAD35, 0xAD4F, Gb::LVT},
    {0xAD50, 0xAD50, Gb::LV},
    {0xAD51, 0xAD6B, Gb::LVT},
    {0xAD6C, 0xAD6C, Gb::LV},
    {0xAD6D, 0xAD87, Gb::LVT},
    {0xAD88, 0xAD88, Gb::LV},
    {0xAD89, 0xADA3, Gb::LVT},
    {0xADA4, 0xADA4, Gb::LV},
    {0xADA5, 0xADBF, Gb::LVT},
    {0xADC0, 0xADC0, Gb::LV},
    {0xADC1, 0xADDB, Gb::LVT},
    {0xADDC, 0xADDC, Gb::LV},
    {0xADDD, 0xADF7, Gb::LVT},
    {0xADF8, 0xADF8, Gb::LV},
    {0xADF9, 0xAE13, Gb::LVT},
    {0xAE14, 0xAE14, Gb::LV},
    {0xAE15, 0xAE2F, Gb::LVT},
    {0xAE30, 0xAE30, Gb::LV},
    {0xAE31, 0xAE4B, Gb::LVT},
    {0xAE4C, 0xAE4C, Gb::LV},
    {0xAE4D, 0xAE67, Gb::LVT},
    {0xAE68, 0xAE68, Gb::LV},
    {0xAE69, 0xAE83, Gb::LVT},
    {0xAE84, 0xAE84, Gb::LV},
    {0xAE85, 0xAE9F, Gb::LVT},
    {0xAEA0, 0xAEA0, Gb::LV},
    {0xAEA1, 0xAEBB, Gb::LVT},
    {0xAEBC, 0xAEBC, Gb::LV},
    {0xAEBD, 0xAED7, Gb::LVT},
    {0xAED8, 0xAED8, Gb::LV},
    {0xAED9, 0xAEF3, Gb::LVT},
    {0xAEF4, 0xAEF4, Gb::LV},
    {0xAEF5, 0xAF0F, Gb::LVT},
    {0xAF10, 0xAF10, Gb::LV},
    {0xAF11, 0xAF2B, Gb::LVT},
    {0xAF2C, 0xAF2C, Gb::LV},
    {0xAF2D, 0xAF47, Gb::LVT},
    {0xAF48, 0xAF48, Gb::LV},
    {0xAF49, 0xAF63, Gb::LVT},
    {0xAF64, 0xAF64, Gb::LV},
    {0xAF65, 0xAF7F, Gb::LVT},
    {0xAF80, 0xAF80, Gb::LV},
    {0xAF81, 0xAF9B, Gb::LVT},
    {0xAF9C, 0xAF9C, Gb::LV},
    {0xAF9D, 0xAFB7, Gb::LVT},
    {0xAFB8, 0xAFB8, Gb::LV},
    {0xAFB9, 0xAFD3, Gb::LVT},
    {0xAFD4, 0xAFD4, Gb::LV},
    {0xAFD5, 0xAFEF, Gb::LVT},
    {0xAFF0, 0xAFF0, Gb::LV},
    {0xAFF1, 0xB00B, Gb::LVT},
    {0xB00C, 0xB00C, Gb::LV},
    {0xB00D, 0xB027, Gb::LVT},
    {0xB028, 0xB028, Gb::LV},
    {0xB029, 0xB043, Gb::LVT},
    {0xB044, 0xB044, Gb::LV},
    {0xB045, 0xB05F, Gb::LVT},
    {0xB060, 0xB060, Gb::LV},
    {0xB061, 0xB07B, Gb::LVT},
    {0xB07C, 0xB07C, Gb::LV},
    {0xB07D, 0xB097, Gb::LVT},
    {0xB098, 0xB098, Gb::LV},
    {0xB099, 0xB0B3, Gb::LVT},
    {0xB0B4, 0xB0B4, Gb::LV},
    {0xB0B5, 0xB0CF, Gb::LVT},
    {0xB0D0, 0xB0D0, Gb::LV},
    {0xB0D1, 0xB0EB, Gb::LVT},
    {0xB0EC, 0xB0EC, Gb::LV},
    {0xB0ED, 0xB107, Gb::LVT},
    {0xB108, 0xB108, Gb::LV},
    {0xB109, 0xB123, Gb::LVT},
    {0xB124, 0xB124, Gb::LV},
    {0xB125, 0xB13F, Gb::LVT},
    {0xB140, 0xB140, Gb::LV},
    {0xB141, 0xB15B, Gb::LVT},
    {0xB15C, 0xB15C, Gb::LV},
    {0xB15D, 0xB177, Gb::LVT},
    {0xB178, 0xB178, Gb::LV},
    {0xB179, 0xB193, Gb::LVT},
    {0xB194, 0xB194, Gb::LV},
    {0xB195, 0xB1AF, Gb::LVT},
    {0xB1B0, 0xB1B0, Gb::LV},
    {0xB1B1, 0xB1CB, Gb::LVT},
    {0xB1CC, 0xB1CC, Gb::LV},
    {0xB1CD, 0xB1E7, Gb::LVT},
    {0xB1E8, 0xB1E8, Gb::LV},
    {0xB1E9, 0xB203, Gb::LVT},
    {0xB204, 0xB204, Gb::LV},
    {0xB205, 0xB21F, Gb::LVT},
    {0xB220, 0xB220, Gb::LV},
    {0xB221, 0xB23B, Gb::LVT},
    {0xB23C, 0xB23C, Gb::LV},
    {0xB23D, 0xB257, Gb::LVT},
    {0xB258, 0xB258, Gb::LV},
    {0xB259, 0xB273, Gb::LVT},
    {0xB274, 0xB274, Gb::LV},
    {0xB275, 0xB28F, Gb::LVT},
    {0xB290, 0xB290, Gb::LV},
    {0xB291, 0xB2AB, Gb::LVT},
    {0xB2AC, 0xB2AC, Gb::LV},
    {0xB2AD, 0xB2C7, Gb::LVT},
    {0xB2C8, 0xB2C8, Gb::LV},
    {0xB2C9, 0xB2E3, Gb::LVT},
    {0xB2E4, 0xB2E4, Gb::LV},
    {0xB2E5, 0xB2FF, Gb::LVT},
    {0xB300, 0xB300, Gb::LV},
    {0xB301, 0xB31B, Gb::LVT},
    {0xB31C, 0xB31C, Gb::LV},
    {0xB31D, 0xB337, Gb::LVT},
    {0xB338, 0xB338, Gb::LV},
    {0xB339, 0xB353, Gb::LVT},
    {0xB354, 0xB354, Gb::LV},
    {0xB355, 0xB36F, Gb::LVT},
    {0xB370, 0xB370, Gb::LV},
    {0xB371, 0xB38B, Gb::LVT},
    {0xB38C, 0xB38C, Gb::LV},
    {0xB38D, 0xB3A7, Gb::LVT},
    {0xB3A8, 0xB3A8, Gb::LV},
    {0xB3A9, 0xB3C3, Gb::LVT},
    {0xB3C4, 0xB3C4, Gb::LV},
    {0xB3C5, 0xB3DF, Gb::LVT},
    {0xB3E0, 0xB3E0, Gb::LV},
    {0xB3E1, 0xB3FB, Gb::LVT},
    {0xB3FC, 0xB3FC, Gb::LV},
    {0xB3FD, 0xB417, Gb::LVT},
    {0xB418, 0xB418, Gb::LV},
    {0xB419, 0xB433, Gb::LVT},
    {0xB434, 0xB434, Gb::LV},
    {0xB435, 0xB44F, Gb::LVT},
    {0xB450, 0xB450, Gb::LV},
    {0xB451, 0xB46B, Gb::LVT},
    {0xB46C, 0xB46C, Gb::LV},
    {0xB46D, 0xB487, Gb::LVT},
    {0xB488, 0xB488, Gb::LV},
    {0xB489, 0xB4A3, Gb::LVT},
    {0xB4A4, 0xB4A4, Gb::LV},
    {0xB4A5, 0xB4BF, Gb::LVT},
    {0xB4C0, 0xB4C0, Gb::LV},
    {0xB4C1, 0xB4DB, Gb::LVT},
    {0xB4DC, 0xB4DC, Gb::LV},
    {0xB4DD, 0xB4F7, Gb::LVT},
    {0xB4F8, 0xB4F8, Gb::LV},
    {0xB4F9, 0xB513, Gb::LVT},
    {0xB514, 0xB514, Gb::LV},
    {0xB515, 0xB52F, Gb::LVT},
    {0xB530, 0xB530, Gb::LV},
    {0xB531, 0xB54B, Gb::LVT},
    {0xB54C, 0xB54C, Gb::LV},
    {0xB54D, 0xB567, Gb::LVT},
    {0xB568, 0xB568, Gb::LV},
    {0xB569, 0xB583, Gb::LVT},
    {0xB584, 0xB584, Gb::LV},
    {0xB585, 0xB59F, Gb::LVT},
    {0xB5A0, 0xB5A0, Gb::LV},
    {0xB5A1, 0xB5BB, Gb::LVT},
    {0xB5BC, 0xB5BC, Gb::LV},
    {0xB5BD, 0xB5D7, Gb::LVT},
    {0xB5D8, 0xB5D8, Gb::LV},
    {0xB5D9, 0xB5F3, Gb::LVT},
    {0xB5F4, 0xB5F4, Gb::LV},
    {0xB5F5, 0xB60F, Gb::LVT},
    {0xB610, 0xB610, Gb::LV},
    {0xB611, 0xB62B, Gb::LVT},
    {0xB62C, 0xB62C, Gb::LV},
    {0xB62D, 0xB647, Gb::LVT},
    {0xB648, 0xB648, Gb::LV},
    {0xB649, 0xB663, Gb::LVT},
    {0xB664, 0xB664, Gb::LV},
    {0xB665, 0xB67F, Gb::LVT},
    {0xB680, 0xB680, Gb::LV},
    {0xB681, 0xB69B, Gb::LVT},
    {0xB69C, 0xB69C, Gb::LV},
    {0xB69D, 0xB6B7, Gb::LVT},
    {0xB6B8, 0xB6B8, Gb::LV},
    {0xB6B9, 0xB6D3, Gb::LVT},
    {0xB6D4, 0xB6D4, Gb::LV},
    {0xB6D5, 0xB6EF, Gb::LVT},
    {0xB6F0, 0xB6F0, Gb::LV},
    {0xB6F1, 0xB70B, Gb::LVT},
    {0xB70C, 0xB70C, Gb::LV},
    {0xB70D, 0xB727, Gb::LVT},
    {0xB728, 0xB728, Gb::LV},
    {0xB729, 0xB743, Gb::LVT},
    {0xB744, 0xB744, Gb::LV},
    {0xB745, 0xB75F, Gb::LVT},
    {0xB760, 0xB760, Gb::LV},
    {0xB761, 0xB77B, Gb::LVT},
    {0xB77C, 0xB77C, Gb::LV},
    {0xB77D, 0xB797, Gb::LVT},
    {0xB798, 0xB798, Gb::LV},
    {0xB799, 0xB7B3, Gb::LVT},
    {0xB7B4, 0xB7B4, Gb::LV},
    {0xB7B5, 0xB7CF, Gb::LVT},
    {0xB7D0, 0xB7D0, Gb::LV},
    {0xB7D1, 0xB7EB, Gb::LVT},
    {0xB7EC, 0xB7EC, Gb::LV},
    {0xB7ED, 0xB807, Gb::LVT},
    {0xB808, 0xB808, Gb::LV},
    {0xB809, 0xB823, Gb::LVT},
    {0xB824, 0xB824, Gb::LV},
    {0xB825, 0xB83F, Gb::LVT},
    {0xB840, 0xB840, Gb::LV},
    {0xB841, 0xB85B, Gb::LVT},
    {0xB85C, 0xB85C, Gb::LV},
    {0xB85D, 0xB877, Gb::LVT},
    {0xB878, 0xB878, Gb::LV},
    {0xB879, 0xB893, Gb::LVT},
    {0xB894, 0xB894, Gb::LV},
    {0xB895, 0xB8AF, Gb::LVT},
    {0xB8B0, 0xB8B0, Gb::LV},
    {0xB8B1, 0xB8CB, Gb::LVT},
    {0xB8CC, 0xB8CC, Gb::LV},
    {0xB8CD, 0xB8E7, Gb::LVT},
    {0xB8E8, 0xB8E8, Gb::LV},
    {0xB8E9, 0xB903, Gb::LVT},
    {0xB904, 0xB904, Gb::LV},
    {0xB905, 0xB91F, Gb::LVT},
    {0xB920, 0xB920, Gb::LV},
    {0xB921, 0xB93B, Gb::LVT},
    {0xB93C, 0xB93C, Gb::LV},
    {0xB93D, 0xB957, Gb::LVT},
    {0xB958, 0xB958, Gb::LV},
    {0xB959, 0xB973, Gb::LVT},
    {0xB974, 0xB974, Gb::LV},
    {0xB975, 0xB98F, Gb::LVT},
    {0xB990, 0xB990, Gb::LV},
    {0xB991, 0xB9AB, Gb::LVT},
    {0xB9AC, 0xB9AC, Gb::LV},
    {0xB9AD, 0xB9C7, Gb::LVT},
    {0xB9C8, 0xB9C8, Gb::LV},
    {0xB9C9, 0xB9E3, Gb::LVT},
    {0xB9E4, 0xB9E4, Gb::LV},
    {0xB9E5, 0xB9FF, Gb::LVT},
    {0xBA00, 0xBA00, Gb::LV},
    {0xBA01, 0xBA1B, Gb::LVT},
    {0xBA1C, 0xBA1C, Gb::LV},
    {0xBA1D, 0xBA37, Gb::LVT},
    {0xBA38, 0xBA38, Gb::LV},
    {0xBA39, 0xBA53, Gb::LVT},
    {0xBA54, 0xBA54, Gb::LV},
    {0xBA55, 0xBA6F, Gb::LVT},
    {0xBA70, 0xBA70, Gb::LV},
    {0xBA71, 0xBA8B, Gb::LVT},
    {0xBA8C, 0xBA8C, Gb::LV},
    {0xBA8D, 0xBAA7, Gb::LVT},
    {0xBAA8, 0xBAA8, Gb::LV},
    {0xBAA9, 0xBAC3, Gb::LVT},
    {0xBAC4, 0xBAC4, Gb::LV},
    {0xBAC5, 0xBADF, Gb::LVT},
    {0xBAE0, 0xBAE0, Gb::LV},
    {0xBAE1, 0xBAFB, Gb::LVT},
    {0xBAFC, 0xBAFC, Gb::LV},
    {0xBAFD, 0xBB17, Gb::LVT},
    {0xBB18, 0xBB18, Gb::LV},
    {0xBB19, 0xBB33, Gb::LVT},
    {0xBB34, 0xBB34, Gb::LV},
    {0xBB35, 0xBB4F, Gb::LVT},
    {0xBB50, 0xBB50, Gb::LV},
    {0xBB51, 0xBB6B, Gb::LVT},
    {0xBB6C, 0xBB6C, Gb::LV},
    {0xBB6D, 0xBB87, Gb::LVT},
    {0xBB88, 0xBB88, Gb::LV},
    {0xBB89, 0xBBA3, Gb::LVT},
    {0xBBA4, 0xBBA4, Gb::LV},
    {0xBBA5, 0xBBBF, Gb::LVT},
    {0xBBC0, 0xBBC0, Gb::LV},
    {0xBBC1, 0xBBDB, Gb::LVT},
    {0xBBDC, 0xBBDC, Gb::LV},
    {0xBBDD, 0xBBF7, Gb::LVT},
    {0xBBF8, 0xBBF8, Gb::LV},
    {0xBBF9, 0xBC13, Gb::LVT},
    {0xBC14, 0xBC14, Gb::LV},
    {0xBC15, 0xBC2F, Gb::LVT},
    {0xBC30, 0xBC30, Gb::LV},
    {0xBC31, 0xBC4B, Gb::LVT},
    {0xBC4C, 0xBC4C, Gb::LV},
    {0xBC4D, 0xBC67, Gb::LVT},
    {0xBC68, 0xBC68, Gb::LV},
    {0xBC69, 0xBC83, Gb::LVT},
    {0xBC84, 0xBC84, Gb::LV},
    {0xBC85, 0xBC9F, Gb::LVT},
    {0xBCA0, 0xBCA0, Gb::LV},
    {0xBCA1, 0xBCBB, Gb::LVT},
    {0xBCBC, 0xBCBC, Gb::LV},
    {0xBCBD, 0xBCD7, Gb::LVT},
    {0xBCD8, 0xBCD8, Gb::LV},
    {0xBCD9, 0xBCF3, Gb::LVT},
    {0xBCF4, 0xBCF4, Gb::LV},
    {0xBCF5, 0xBD0F, Gb::LVT},
    {0xBD10, 0xBD10, Gb::LV},
    {0xBD11, 0xBD2B, Gb::LVT},
    {0xBD2C, 0xBD2C, Gb::LV},
    {0xBD2D, 0xBD47, Gb::LVT},
    {0xBD48, 0xBD48, Gb::LV},
    {0xBD49, 0xBD63, Gb::LVT},
    {0xBD64, 0xBD64, Gb::LV},
    {0xBD65, 0xBD7F, Gb::LVT},
    {0xBD80, 0xBD80, Gb::LV},
    {0xBD81, 0xBD9B, Gb::LVT},
    {0xBD9C, 0xBD9C, Gb::LV},
    {0xBD9D, 0xBDB7, Gb::LVT},
    {0xBDB8, 0xBDB8, Gb::LV},
    {0xBDB9, 0xBDD3, Gb::LVT},
    {0xBDD4, 0xBDD4, Gb::LV},
    {0xBDD5, 0xBDEF, Gb::LVT},
    {0xBDF0, 0xBDF0, Gb::LV},
    {0xBDF1, 0xBE0B, Gb::LVT},
    {0xBE0C, 0xBE0C, Gb::LV},
    {0xBE0D, 0xBE27, Gb::LVT},
    {0xBE28, 0xBE28, Gb::LV},
    {0xBE29, 0xBE43, Gb::LVT},
    {0xBE44, 0xBE44, Gb::LV},
    {0xBE45, 0xBE5F, Gb::LVT},
    {0xBE60, 0xBE60, Gb::LV},
    {0xBE61, 0xBE7B, Gb::LVT},
    {0xBE7C, 0xBE7C, Gb::LV},
    {0xBE7D, 0xBE97, Gb::LVT},
    {0xBE98, 0xBE98, Gb::LV},
    {0xBE99, 0xBEB3, Gb::LVT},
    {0xBEB4, 0xBEB4, Gb::LV},
    {0xBEB5, 0xBECF, Gb::LVT},
    {0xBED0, 0xBED0, Gb::LV},
    {0xBED1, 0xBEEB, Gb::LVT},
    {0xBEEC, 0xBEEC, Gb::LV},
    {0xBEED, 0xBF07, Gb::LVT},
    {0xBF08, 0xBF08, Gb::LV},
    {0xBF09, 0xBF23, Gb::LVT},
    {0xBF24, 0xBF24, Gb::LV},
    {0xBF25, 0xBF3F, Gb::LVT},
    {0xBF40, 0xBF40, Gb::LV},
    {0xBF41, 0xBF5B, Gb::LVT},
    {0xBF5C, 0xBF5C, Gb::LV},
    {0xBF5D, 0xBF77, Gb::LVT},
    {0xBF78, 0xBF78, Gb::LV},
    {0xBF79, 0xBF93, Gb::LVT},
    {0xBF94, 0xBF94, Gb::LV},
    {0xBF95, 0xBFAF, Gb::LVT},
    {0xBFB0, 0xBFB0, Gb::LV},
    {0xBFB1, 0xBFCB, Gb::LVT},
    {0xBFCC, 0xBFCC, Gb::LV},
    {0xBFCD, 0xBFE7, Gb::LVT},
    {0xBFE8, 0xBFE8, Gb::LV},
    {0xBFE9, 0xC003, Gb::LVT},
    {0xC004, 0xC004, Gb::LV},
    {0xC005, 0xC01F, Gb::LVT},
    {0xC020, 0xC020, Gb::LV},
    {0xC021, 0xC03B, Gb::LVT},
    {0xC03C, 0xC03C, Gb::LV},
    {0xC03D, 0xC057, Gb::LVT},
    {0xC058, 0xC058, Gb::LV},
    {0xC059, 0xC073, Gb::LVT},
    {0xC074, 0xC074, Gb::LV},
    {0xC075, 0xC08F, Gb::LVT},
    {0xC090, 0xC090, Gb::LV},
    {0xC091, 0xC0AB, Gb::LVT},
    {0xC0AC, 0xC0AC, Gb::LV},
    {0xC0AD, 0xC0C7, Gb::LVT},
    {0xC0C8, 0xC0C8, Gb::LV},
    {0xC0C9, 0xC0E3, Gb::LVT},
    {0xC0E4, 0xC0E4, Gb::LV},
    {0xC0E5, 0xC0FF, Gb::LVT},
    {0xC100, 0xC100, Gb::LV},
    {0xC101, 0xC11B, Gb::LVT},
    {0xC11C, 0xC11C, Gb::LV},
    {0xC11D, 0xC137, Gb::LVT},
    {0xC138, 0xC138, Gb::LV},
    {0xC139, 0xC153, Gb::LVT},
    {0xC154, 0xC154, Gb::LV},
    {0xC155, 0xC16F, Gb::LVT},
    {0xC170, 0xC170, Gb::LV},
    {0xC171, 0xC18B, Gb::LVT},
    {0xC18C, 0xC18C, Gb::LV},
    {0xC18D, 0xC1A7, Gb::LVT},
    {0xC1A8, 0xC1A8, Gb::LV},
    {0xC1A9, 0xC1C3, Gb::LVT},
    {0xC1C4, 0xC1C4, Gb::LV},
    {0xC1C5, 0xC1DF, Gb::LVT},
    {0xC1E0, 0xC1E0, Gb::LV},
    {0xC1E1, 0xC1FB, Gb::LVT},
    {0xC1FC, 0xC1FC, Gb::LV},
    {0xC1FD, 0xC217, Gb::LVT},
    {0xC218, 0xC218, Gb::LV},
    {0xC219, 0xC233, Gb::LVT},
    {0xC234, 0xC234, Gb::LV},
    {0xC235, 0xC24F, Gb::LVT},
    {0xC250, 0xC250, Gb::LV},
    {0xC251, 0xC26B, Gb::LVT},
    {0xC26C, 0xC26C, Gb::LV},
    {0xC26D, 0xC287, Gb::LVT},
    {0xC288, 0xC288, Gb::LV},
    {0xC289, 0xC2A3, Gb::LVT},
    {0xC2A4, 0xC2A4, Gb::LV},
    {0xC2A5, 0xC2BF, Gb::LVT},
    {0xC2C0, 0xC2C0, Gb::LV},
    {0xC2C1, 0xC2DB, Gb::LVT},
    {0xC2DC, 0xC2DC, Gb::LV},
    {0xC2DD, 0xC2F7, Gb::LVT},
    {0xC2F8, 0xC2F8, Gb::LV},
    {0xC2F9, 0xC313, Gb::LVT},
    {0xC314, 0xC314, Gb::LV},
    {0xC315, 0xC32F, Gb::LVT},
    {0xC330, 0xC330, Gb::LV},
    {0xC331, 0xC34B, Gb::LVT},
    {0xC34C, 0xC34C, Gb::LV},
    {0xC34D, 0xC367, Gb::LVT},
    {0xC368, 0xC368, Gb::LV},
    {0xC369, 0xC383, Gb::LVT},
    {0xC384, 0xC384, Gb::LV},
    {0xC385, 0xC39F, Gb::LVT},
    {0xC3A0, 0xC3A0, Gb::LV},
    {0xC3A1, 0xC3BB, Gb::LVT},
    {0xC3BC, 0xC3BC, Gb::LV},
    {0xC3BD, 0xC3D7, Gb::LVT},
    {0xC3D8, 0xC3D8, Gb::LV},
    {0xC3D9, 0xC3F3, Gb::LVT},
    {0xC3F4, 0xC3F4, Gb::LV},
    {0xC3F5, 0xC40F, Gb::LVT},
    {0xC410, 0xC410, Gb::LV},
    {0xC411, 0xC42B, Gb::LVT},
    {0xC42C, 0xC42C, Gb::LV},
    {0xC42D, 0xC447, Gb::LVT},
    {0xC448, 0xC448, Gb::LV},
    {0xC449, 0xC463, Gb::LVT},
    {0xC464, 0xC464, Gb::LV},
    {0xC465, 0xC47F, Gb::LVT},
    {0xC480, 0xC480, Gb::LV},
    {0xC481, 0xC49B, Gb::LVT},
    {0xC49C, 0xC49C, Gb::LV},
    {0xC49D, 0xC4B7, Gb::LVT},
    {0xC4B8, 0xC4B8, Gb::LV},
    {0xC4B9, 0xC4D3, Gb::LVT},
    {0xC4D4, 0xC4D4, Gb::LV},
    {0xC4D5, 0xC4EF, Gb::LVT},
    {0xC4F0, 0xC4F0, Gb::LV},
    {0xC4F1, 0xC50B, Gb::LVT},
    {0xC50C, 0xC50C, Gb::LV},
    {0xC50D, 0xC527, Gb::LVT},
    {0xC528, 0xC528, Gb::LV},
    {0xC529, 0xC543, Gb::LVT},
    {0xC544, 0xC544, Gb::LV},
    {0xC545, 0xC55F, Gb::LVT},
    {0xC560, 0xC560, Gb::LV},
    {0xC561, 0xC57B, Gb::LVT},
    {0xC57C, 0xC57C, Gb::LV},
    {0xC57D, 0xC597, Gb::LVT},
    {0xC598, 0xC598, Gb::LV},
    {0xC599, 0xC5B3, Gb::LVT},
    {0xC5B4, 0xC5B4, Gb::LV},
    {0xC5B5, 0xC5CF, Gb::LVT},
    {0xC5D0, 0xC5D0, Gb::LV},
    {0xC5D1, 0xC5EB, Gb::LVT},
    {0xC5EC, 0xC5EC, Gb::LV},
    {0xC5ED, 0xC607, Gb::LVT},
    {0xC608, 0xC608, Gb::LV},
    {0xC609, 0xC623, Gb::LVT},
    {0xC624, 0xC624, Gb::LV},
    {0xC625, 0xC63F, Gb::LVT},
    {0xC640, 0xC640, Gb::LV},
    {0xC641, 0xC65B, Gb::LVT},
    {0xC65C, 0xC65C, Gb::LV},
    {0xC65D, 0xC677, Gb::LVT},
    {0xC678, 0xC678, Gb::LV},
    {0xC679, 0xC693, Gb::LVT},
    {0xC694, 0xC694, Gb::LV},
    {0xC695, 0xC6AF, Gb::LVT},
    {0xC6B0, 0xC6B0, Gb::LV},
    {0xC6B1, 0xC6CB, Gb::LVT},
    {0xC6CC, 0xC6CC, Gb::LV},
    {0xC6CD, 0xC6E7, Gb::LVT},
    {0xC6E8, 0xC6E8, Gb::LV},
    {0xC6E9, 0xC703, Gb::LVT},
    {0xC704, 0xC704, Gb::LV},
    {0xC705, 0xC71F, Gb::LVT},
    {0xC720, 0xC720, Gb::LV},
    {0xC721, 0xC73B, Gb::LVT},
    {0xC73C, 0xC73C, Gb::LV},
    {0xC73D, 0xC757, Gb::LVT},
    {0xC758, 0xC758, Gb::LV},
    {0xC759, 0xC773, Gb::LVT},
    {0xC774, 0xC774, Gb::LV},
    {0xC775, 0xC78F, Gb::LVT},
    {0xC790, 0xC790, Gb::LV},
    {0xC791, 0xC7AB, Gb::LVT},
    {0xC7AC, 0xC7AC, Gb::LV},
    {0xC7AD, 0xC7C7, Gb::LVT},
    {0xC7C8, 0xC7C8, Gb::LV},
    {0xC7C9, 0xC7E3, Gb::LVT},
    {0xC7E4, 0xC7E4, Gb::LV},
    {0xC7E5, 0xC7FF, Gb::LVT},
    {0xC800, 0xC800, Gb::LV},
    {0xC801, 0xC81B, Gb::LVT},
    {0xC81C, 0xC81C, Gb::LV},
    {0xC81D, 0xC837, Gb::LVT},
    {0xC838, 0xC838, Gb::LV},
    {0xC839, 0xC853, Gb::LVT},
    {0xC854, 0xC854, Gb::LV},
    {0xC855, 0xC86F, Gb::LVT},
    {0xC870, 0xC870, Gb::LV},
    {0xC871, 0xC88B, Gb::LVT},
    {0xC88C, 0xC88C, Gb::LV},
    {0xC88D, 0xC8A7, Gb::LVT},
    {0xC8A8, 0xC8A8, Gb::LV},
    {0xC8A9, 0xC8C3, Gb::LVT},
    {0xC8C4, 0xC8C4, Gb::LV},
    {0xC8C5, 0xC8DF, Gb::LVT},
    {0xC8E0, 0xC8E0, Gb::LV},
    {0xC8E1, 0xC8FB, Gb::LVT},
    {0xC8FC, 0xC8FC, Gb::LV},
    {0xC8FD, 0xC917, Gb::LVT},
    {0xC918, 0xC918, Gb::LV},
    {0xC919, 0xC933, Gb::LVT},
    {0xC934, 0xC934, Gb::LV},
    {0xC935, 0xC94F, Gb::LVT},
    {0xC950, 0xC950, Gb::LV},
    {0xC951, 0xC96B, Gb::LVT},
    {0xC96C, 0xC96C, Gb::LV},
    {0xC96D, 0xC987, Gb::LVT},
    {0xC988, 0xC988, Gb::LV},
    {0xC989, 0xC9A3, Gb::LVT},
    {0xC9A4, 0xC9A4, Gb::LV},
    {0xC9A5, 0xC9BF, Gb::LVT},
    {0xC9C0, 0xC9C0, Gb::LV},
    {0xC9C1, 0xC9DB, Gb::LVT},
    {0xC9DC, 0xC9DC, Gb::LV},
    {0xC9DD, 0xC9F7, Gb::LVT},
    {0xC9F8, 0xC9F8, Gb::LV},
    {0xC9F9, 0xCA13, Gb::LVT},
    {0xCA14, 0xCA14, Gb::LV},
    {0xCA15, 0xCA2F, Gb::LVT},
    {0xCA30, 0xCA30, Gb::LV},
    {0xCA31, 0xCA4B, Gb::LVT},
    {0xCA4C, 0xCA4C, Gb::LV},
    {0xCA4D, 0xCA67, Gb::LVT},
    {0xCA68, 0xCA68, Gb::LV},
    {0xCA69, 0xCA83, Gb::LVT},
    {0xCA84, 0xCA84, Gb::LV},
    {0xCA85, 0xCA9F, Gb::LVT},
    {0xCAA0, 0xCAA0, Gb::LV},
    {0xCAA1, 0xCABB, Gb::LVT},
    {0xCABC, 0xCABC, Gb::LV},
    {0xCABD, 0xCAD7, Gb::LVT},
    {0xCAD8, 0xCAD8, Gb::LV},
    {0xCAD9, 0xCAF3, Gb::LVT},
    {0xCAF4, 0xCAF4, Gb::LV},
    {0xCAF5, 0xCB0F, Gb::LVT},
    {0xCB10, 0xCB10, Gb::LV},
    {0xCB11, 0xCB2B, Gb::LVT},
    {0xCB2C, 0xCB2C, Gb::LV},
    {0xCB2D, 0xCB47, Gb::LVT},
    {0xCB48, 0xCB48, Gb::LV},
    {0xCB49, 0xCB63, Gb::LVT},
    {0xCB64, 0xCB64, Gb::LV},
    {0xCB65, 0xCB7F, Gb::LVT},
    {0xCB80, 0xCB80, Gb::LV},
    {0xCB81, 0xCB9B, Gb::LVT},
    {0xCB9C, 0xCB9C, Gb::LV},
    {0xCB9D, 0xCBB7, Gb::LVT},
    {0xCBB8, 0xCBB8, Gb::LV},
    {0xCBB9, 0xCBD3, Gb::LVT},
    {0xCBD4, 0xCBD4, Gb::LV},
    {0xCBD5, 0xCBEF, Gb::LVT},
    {0xCBF0, 0xCBF0, Gb::LV},
    {0xCBF1, 0xCC0B, Gb::LVT},
    {0xCC0C, 0xCC0C, Gb::LV},
    {0xCC0D, 0xCC27, Gb::LVT},
    {0xCC28, 0xCC28, Gb::LV},
    {0xCC29, 0xCC43, Gb::LVT},
    {0xCC44, 0xCC44, Gb::LV},
    {0xCC45, 0xCC5F, Gb::LVT},
    {0xCC60, 0xCC60, Gb::LV},
    {0xCC61, 0xCC7B, Gb::LVT},
    {0xCC7C, 0xCC7C, Gb::LV},
    {0xCC7D, 0xCC97, Gb::LVT},
    {0xCC98, 0xCC98, Gb::LV},
    {0xCC99, 0xCCB3, Gb::LVT},
    {0xCCB4, 0xCCB4, Gb::LV},
    {0xCCB5, 0xCCCF, Gb::LVT},
    {0xCCD0, 0xCCD0, Gb::LV},
    {0xCCD1, 0xCCEB, Gb::LVT},
    {0xCCEC, 0xCCEC, Gb::LV},
    {0xCCED, 0xCD07, Gb::LVT},
    {0xCD08, 0xCD08, Gb::LV},
    {0xCD09, 0xCD23, Gb::LVT},
    {0xCD24, 0xCD24, Gb::LV},
    {0xCD25, 0xCD3F, Gb::LVT},
    {0xCD40, 0xCD40, Gb::LV},
    {0xCD41, 0xCD5B, Gb::LVT},
    {0xCD5C, 0xCD5C, Gb::LV},
    {0xCD5D, 0xCD77, Gb::LVT},
    {0xCD78, 0xCD78, Gb::LV},
    {0xCD79, 0xCD93, Gb::LVT},
    {0xCD94, 0xCD94, Gb::LV},
    {0xCD95, 0xCDAF, Gb::LVT},
    {0xCDB0, 0xCDB0, Gb::LV},
    {0xCDB1, 0xCDCB, Gb::LVT},
    {0xCDCC, 0xCDCC, Gb::LV},
    {0xCDCD, 0xCDE7, Gb::LVT},
    {0xCDE8, 0xCDE8, Gb::LV},
    {0xCDE9, 0xCE03, Gb::LVT},
    {0xCE04, 0xCE04, Gb::LV},
    {0xCE05, 0xCE1F, Gb::LVT},
    {0xCE20, 0xCE20, Gb::LV},
    {0xCE21, 0xCE3B, Gb::LVT},
    {0xCE3C, 0xCE3C, Gb::LV},
    {0xCE3D, 0xCE57, Gb::LVT},
    {0xCE58, 0xCE58, Gb::LV},
    {0xCE59, 0xCE73, Gb::LVT},
    {0xCE74, 0xCE74, Gb::LV},
    {0xCE75, 0xCE8F, Gb::LVT},
    {0xCE90, 0xCE90, Gb::LV},
    {0xCE91, 0xCEAB, Gb::LVT},
    {0xCEAC, 0xCEAC, Gb::LV},
    {0xCEAD, 0xCEC7, Gb::LVT},
    {0xCEC8, 0xCEC8, Gb::LV},
    {0xCEC9, 0xCEE3, Gb::LVT},
    {0xCEE4, 0xCEE4, Gb::LV},
    {0xCEE5, 0xCEFF, Gb::LVT},
    {0xCF00, 0xCF00, Gb::LV},
    {0xCF01, 0xCF1B, Gb::LVT},
    {0xCF1C, 0xCF1C, Gb::LV},
    {0xCF1D, 0xCF37, Gb::LVT},
    {0xCF38, 0xCF38, Gb::LV},
    {0xCF39, 0xCF53, Gb::LVT},
    {0xCF54, 0xCF54, Gb::LV},
    {0xCF55, 0xCF6F, Gb::LVT},
    {0xCF70, 0xCF70, Gb::LV},
    {0xCF71, 0xCF8B, Gb::LVT},
    {0xCF8C, 0xCF8C, Gb::LV},
    {0xCF8D, 0xCFA7, Gb::LVT},
    {0xCFA8, 0xCFA8, Gb::LV},
    {0xCFA9, 0xCFC3, Gb::LVT},
    {0xCFC4, 0xCFC4, Gb::LV},
    {0xCFC5, 0xCFDF, Gb::LVT},
    {0xCFE0, 0xCFE0, Gb::LV},
    {0xCFE1, 0xCFFB, Gb::LVT},
    {0xCFFC, 0xCFFC, Gb::LV},
    {0xCFFD, 0xD017, Gb::LVT},
    {0xD018, 0xD018, Gb::LV},
    {0xD019, 0xD033, Gb::LVT},
    {0xD034, 0xD034, Gb::LV},
    {0xD035, 0xD04F, Gb::LVT},
    {0xD050, 0xD050, Gb::LV},
    {0xD051, 0xD06B, Gb::LVT},
    {0xD06C, 0xD06C, Gb::LV},
    {0xD06D, 0xD087, Gb::LVT},
    {0xD088, 0xD088, Gb::LV},
    {0xD089, 0xD0A3, Gb::LVT},
    {0xD0A4, 0xD0A4, Gb::LV},
    {0xD0A5, 0xD0BF, Gb::LVT},
    {0xD0C0, 0xD0C0, Gb::LV},
    {0xD0C1, 0xD0DB, Gb::LVT},
    {0xD0DC, 0xD0DC, Gb::LV},
    {0xD0DD, 0xD0F7, Gb::LVT},
    {0xD0F8, 0xD0F8, Gb::LV},
    {0xD0F9, 0xD113, Gb::LVT},
    {0xD114, 0xD114, Gb::LV},
    {0xD115, 0xD12F, Gb::LVT},
    {0xD130, 0xD130, Gb::LV},
    {0xD131, 0xD14B, Gb::LVT},
    {0xD14C, 0xD14C, Gb::LV},
    {0xD14D, 0xD167, Gb::LVT},
    {0xD168, 0xD168, Gb::LV},
    {0xD169, 0xD183, Gb::LVT},
    {0xD184, 0xD184, Gb::LV},
    {0xD185, 0xD19F, Gb::LVT},
    {0xD1A0, 0xD1A0, Gb::LV},
    {0xD1A1, 0xD1BB, Gb::LVT},
    {0xD1BC, 0xD1BC, Gb::LV},
    {0xD1BD, 0xD1D7, Gb::LVT},
    {0xD1D8, 0xD1D8, Gb::LV},
    {0xD1D9, 0xD1F3, Gb::LVT},
    {0xD1F4, 0xD1F4, Gb::LV},
    {0xD1F5, 0xD20F, Gb::LVT},
    {0xD210, 0xD210, Gb::LV},
    {0xD211, 0xD22B, Gb::LVT},
    {0xD22C, 0xD22C, Gb::LV},
    {0xD22D, 0xD247, Gb::LVT},
    {0xD248, 0xD248, Gb::LV},
    {0xD249, 0xD263, Gb::LVT},
    {0xD264, 0xD264, Gb::LV},
    {0xD265, 0xD27F, Gb::LVT},
    {0xD280, 0xD280, Gb::LV},
    {0xD281, 0xD29B, Gb::LVT},
    {0xD29C, 0xD29C, Gb::LV},
    {0xD29D, 0xD2B7, Gb::LVT},
    {0xD2B8, 0xD2B8, Gb::LV},
    {0xD2B9, 0xD2D3, Gb::LVT},
    {0xD2D4, 0xD2D4, Gb::LV},
    {0xD2D5, 0xD2EF, Gb::LVT},
    {0xD2F0, 0xD2F0, Gb::LV},
    {0xD2F1, 0xD30B, Gb::LVT},
    {0xD30C, 0xD30C, Gb::LV},
    {0xD30D, 0xD327, Gb::LVT},
    {0xD328, 0xD328, Gb::LV},
    {0xD329, 0xD343, Gb::LVT},
    {0xD344, 0xD344, Gb::LV},
    {0xD345, 0xD35F, Gb::LVT},
    {0xD360, 0xD360, Gb::LV},
    {0xD361, 0xD37B, Gb::LVT},
    {0xD37C, 0xD37C, Gb::LV},
    {0xD37D, 0xD397, Gb::LVT},
    {0xD398, 0xD398, Gb::LV},
    {0xD399, 0xD3B3, Gb::LVT},
    {0xD3B4, 0xD3B4, Gb::LV},
    {0xD3B5, 0xD3CF, Gb::LVT},
    {0xD3D0, 0xD3D0, Gb::LV},
    {0xD3D1, 0xD3EB, Gb::LVT},
    {0xD3EC, 0xD3EC, Gb::LV},
    {0xD3ED, 0xD407, Gb::LVT},
    {0xD408, 0xD408, Gb::LV},
    {0xD409, 0xD423, Gb::LVT},
    {0xD424, 0xD424, Gb::LV},
    {0xD425, 0xD43F, Gb::LVT},
    {0xD440, 0xD440, Gb::LV},
    {0xD441, 0xD45B, Gb::LVT},
    {0xD45C, 0xD45C, Gb::LV},
    {0xD45D, 0xD477, Gb::LVT},
    {0xD478, 0xD478, Gb::LV},
    {0xD479, 0xD493, Gb::LVT},
    {0xD494, 0xD494, Gb::LV},
    {0xD495, 0xD4AF, Gb::LVT},
    {0xD4B0, 0xD4B0, Gb::LV},
    {0xD4B1, 0xD4CB, Gb::LVT},
    {0xD4CC, 0xD4CC, Gb::LV},
    {0xD4CD, 0xD4E7, Gb::LVT},
    {0xD4E8, 0xD4E8, Gb::LV},
    {0xD4E9, 0xD503, Gb::LVT},
    {0xD504, 0xD504, Gb::LV},
    {0xD505, 0xD51F, Gb::LVT},
    {0xD520, 0xD520, Gb::LV},
    {0xD521, 0xD53B, Gb::LVT},
    {0xD53C, 0xD53C, Gb::LV},
    {0xD53D, 0xD557, Gb::LVT},
    {0xD558, 0xD558, Gb::LV},
    {0xD559, 0xD573, Gb::LVT},
    {0xD574, 0xD574, Gb::LV},
    {0xD575, 0xD58F, Gb::LVT},
    {0xD590, 0xD590, Gb::LV},
    {0xD591, 0xD5AB, Gb::LVT},
    {0xD5AC, 0xD5AC, Gb::LV},
    {0xD5AD, 0xD5C7, Gb::LVT},
    {0xD5C8, 0xD5C8, Gb::LV},
    {0xD5C9, 0xD5E3, Gb::LVT},
    {0xD5E4, 0xD5E4, Gb::LV},
    {0xD5E5, 0xD5FF, Gb::LVT},
    {0xD600, 0xD600, Gb::LV},
    {0xD601, 0xD61B, Gb::LVT},
    {0xD61C, 0xD61C, Gb::LV},
    {0xD61D, 0xD637, Gb::LVT},
    {0xD638, 0xD638, Gb::LV},
    {0xD639, 0xD653, Gb::LVT},
    {0xD654, 0xD654, Gb::LV},
    {0xD655, 0xD66F, Gb::LVT},
    {0xD670, 0xD670, Gb::LV},
    {0xD671, 0xD68B, Gb::LVT},
    {0xD68C, 0xD68C, Gb::LV},
    {0xD68D, 0xD6A7, Gb::LVT},
    {0xD6A8, 0xD6A8, Gb::LV},
    {0xD6A9, 0xD6C3, Gb::LVT},
    {0xD6C4, 0xD6C4, Gb::LV},
    {0xD6C5, 0xD6DF, Gb::LVT},
    {0xD6E0, 0xD6E0, Gb::LV},
    {0xD6E1, 0xD6FB, Gb::LVT},
    {0xD6FC, 0xD6FC, Gb::LV},
    {0xD6FD, 0xD717, Gb::LVT},
    {0xD718, 0xD718, Gb::LV},
    {0xD719, 0xD733, Gb::LVT},
    {0xD734, 0xD734, Gb::LV},
    {0xD735, 0xD74F, Gb::LVT},
    {0xD750, 0xD750, Gb::LV},
    {0xD751, 0xD76B, Gb::LVT},
    {0xD76C, 0xD76C, Gb::LV},
    {0xD76D, 0xD787, Gb::LVT},
    {0xD788, 0xD788, Gb::LV},
    {0xD789, 0xD7A3, Gb::LVT},
    {0xD7B0, 0xD7C6, Gb::V},
    {0xD7CB, 0xD7FB, Gb::T},
    {0xFB1E, 0xFB1E, Gb::Extend},
    {0xFE00, 0xFE0F, Gb::Extend},
    {0xFE20, 0xFE2F, Gb::Extend},
    {0x110BD, 0x110BD, Gb::Prepend},
    {0x110CD, 0x110CD, Gb::Prepend},
    {0x111C2, 0x111C3, Gb::Prepend},
    {0x11300, 0x11301, Gb::SpacingMark},
    {0x114B0, 0x114C1, Gb::SpacingMark},
    {0x115AF, 0x115B4, Gb::SpacingMark},
    {0x11930, 0x1193B, Gb::SpacingMark},
    {0x1193F, 0x1193F, Gb::Prepend},
    {0x11941, 0x11941, Gb::Prepend},
    {0x11A33, 0x11A38, Gb::SpacingMark},
    {0x11A3A, 0x11A3A, Gb::Prepend},
    {0x11A47, 0x11A51, Gb::SpacingMark},
    {0x11A84, 0x11A89, Gb::Prepend},
    {0x11A99, 0x11A9F, Gb::SpacingMark},
    {0x11D42, 0x11D44, Gb::SpacingMark},
    {0x11D46, 0x11D46, Gb::Prepend},
    {0x11F00, 0x11F01, Gb::SpacingMark},
    {0x11F02, 0x11F02, Gb::Prepend},
    {0x1D159, 0x1D159, Gb::Control},
    {0x1D165, 0x1D166, Gb::SpacingMark},
    {0x1D16D, 0x1D172, Gb::SpacingMark},
    {0x1D173, 0x1D17A, Gb::Control},
    {0x1E2FF, 0x1E2FF, Gb::Prepend},
    {0x1F1E6, 0x1F1FF, Gb::RegionalIndicator},
    {0x1F3FB, 0x1F3FF, Gb::Extend},
    {0xE0001, 0xE0001, Gb::Control},
    {0xE0020, 0xE007F, Gb::Control},
    {0xE0099, 0xE0099, Gb::Control},
    {0xE0100, 0xE01EF, Gb::Extend},
}};
// clang-format on


// 二分查找：返回码点的 Grapheme_Cluster_Break 类别，缺省 Gb::Any。
constexpr Gb grapheme_class(std::uint32_t cp) noexcept {
    std::size_t lo = 0;
    std::size_t hi = kGbRanges.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const GbRange& r = kGbRanges[mid];
        if (cp < r.first) {
            hi = mid;
        } else if (cp > r.last) {
            lo = mid + 1;
        } else {
            return r.value;
        }
    }
    return Gb::Any;
}

}  // namespace termchart::detail

// ===== include/termchart/detail/sixel_table.hpp =====
// sixel_table.hpp —— Sixel 编码字符表（层号 -1 横切数据表）
//
// Sixel 每个 6 位数据值 v ∈ [0,63] 用一个可打印字符 0x3F+v 表示：
//   v=0  -> '?'（空/透明）      v=1  -> '@'     ...  v=63 -> '~'
// 这张表把"值 ↔ 字符"这件事钉死在一处，protocol/sixel.hpp 与本PROTOCOL 旁的
// 解码测试共用同一份定义。


namespace termchart::detail {

inline constexpr char kSixelBase = 0x3F;   // '?'
inline constexpr std::uint8_t kSixelMax = 63;

// clang-format off
inline constexpr std::array<char, 64> kSixelChars{{
    '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^',
    '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',
}};
// clang-format on

// 数据值 → 字符（越界按取低 6 位处理，等价于终端的解包行为）。
constexpr char sixel_char(std::uint8_t value) noexcept {
    return kSixelChars[value & kSixelMax];
}

// 字符 → 数据值；非法字符返回 0（透明），与"未知一律按空处理"的容错约定一致。
constexpr std::uint8_t sixel_value(char c) noexcept {
    for (std::uint8_t i = 0; i <= kSixelMax; ++i) {
        if (kSixelChars[i] == c) {
            return i;
        }
    }
    return 0;
}

// 常用控制/参数字符（不属于数据值，单独成常量便于读代码）。
inline constexpr char kSixelRepeatIntro = '!';    // DECGRA：重复前一个字符 N 次
inline constexpr char kSixelRasterAttr = '"';     // DECGRA 光栅属性引出符
inline constexpr char kSixelColorIntro = '#';     // DECGCI：选色
inline constexpr char kSixelCrlf = '-';           // 换行（回到行首）
inline constexpr char kSixelNewline = '$';        // 不回到行首的换行

}  // namespace termchart::detail

// ===== include/termchart/detail/sort_network.hpp =====
// sort_network.hpp —— 小数组排序网络（层号 -1 横切数据表）
//
// 为什么用排序网络：glibc 的 std::sort 对小 n 也有调用开销，而 layout/label_avoid、
// raster 子像素合成里存在大量"固定长度 2~8 的数组、每帧排序"的热点。
// 排序网络无分支预测失败、无递归，且 constexpr 可在编译期跑完。
//
// 网络本体是"比较器下标对"表：pair {i, j} 表示无条件交换 a[i] 与 a[j]（若逆序）。

#include <utility>

namespace termchart::detail {

struct ComparePair {
    std::uint8_t i;
    std::uint8_t j;
};

// ── n = 2..8 的最优/准最优网络（下标对，按执行顺序排列） ─────────────────
using Network2 = std::array<ComparePair, 1>;
using Network3 = std::array<ComparePair, 3>;
using Network4 = std::array<ComparePair, 5>;
using Network5 = std::array<ComparePair, 9>;
using Network6 = std::array<ComparePair, 12>;
using Network7 = std::array<ComparePair, 16>;
using Network8 = std::array<ComparePair, 19>;

// clang-format off
inline constexpr Network2 kSortNetwork2{{{0, 1}}};
inline constexpr Network3 kSortNetwork3{{{0, 1}, {0, 2}, {1, 2}}};
inline constexpr Network4 kSortNetwork4{{{0, 1}, {2, 3}, {0, 2}, {1, 3}, {1, 2}}};
inline constexpr Network5 kSortNetwork5{{
    {0, 1}, {2, 3}, {1, 3}, {2, 4}, {0, 2}, {1, 4}, {1, 2}, {3, 4}, {2, 3},
}};
inline constexpr Network6 kSortNetwork6{{
    {0, 1}, {2, 3}, {4, 5}, {0, 2}, {1, 4}, {3, 5}, {0, 1}, {2, 3},
    {4, 5}, {1, 2}, {3, 4}, {2, 3},
}};
inline constexpr Network7 kSortNetwork7{{
    {0, 1}, {2, 3}, {4, 5}, {0, 2}, {1, 3}, {4, 6}, {0, 4}, {1, 5},
    {2, 6}, {1, 2}, {3, 5}, {0, 1}, {2, 3}, {4, 5}, {3, 4}, {5, 6},
}};
inline constexpr Network8 kSortNetwork8{{
    {0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7},
    {1, 2}, {5, 6}, {0, 4}, {3, 7}, {1, 5}, {2, 6}, {1, 4}, {3, 6},
    {2, 4}, {3, 5}, {3, 4},
}};
// clang-format on

// 按网络执行 compare-exchange。Comp：严格弱序，返回 true 表示 a 应排在 b 前。
template <class T, std::size_t N, std::size_t M, class Comp>
constexpr void apply_network(std::array<T, N>& a, const std::array<ComparePair, M>& net,
                             Comp comp) {
    static_assert(N >= 2 && N <= 8, "sort_network 只覆盖 n ∈ [2, 8]");
    for (const ComparePair& p : net) {
        T& x = a[p.i];
        T& y = a[p.j];
        if (comp(y, x)) {
            T tmp = std::move(x);
            x = std::move(y);
            y = std::move(tmp);
        }
    }
}

}  // namespace termchart::detail

// ===== include/termchart/detail/turbo_table.hpp =====
// turbo_table.hpp —— 由 tools/gen_turbo_table.py 生成，禁手改。
//
// 数据源：assets/colormaps/turbo.dat（官方 Turbo 色表，Google，Apache-2.0）。
// 生成器只做"数据搬运"，不参与任何数值加工——色值需要改就改 .dat 再重跑。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；它不依赖 termchart 其余文件。


namespace termchart::detail {

struct TurboEntry {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

// 256 级，索引 0 = 最低值端（深紫），255 = 最高值端（暗红）。
// clang-format off
inline constexpr std::array<TurboEntry, 256> kTurboTable{{
    { 48,  18,  59}, { 50,  21,  67}, { 51,  24,  74}, { 52,  27,  81},
    { 53,  30,  88}, { 54,  33,  95}, { 55,  36, 102}, { 56,  39, 109},
    { 57,  42, 115}, { 58,  45, 121}, { 59,  47, 128}, { 60,  50, 134},
    { 61,  53, 139}, { 62,  56, 145}, { 63,  59, 151}, { 63,  62, 156},
    { 64,  64, 162}, { 65,  67, 167}, { 65,  70, 172}, { 66,  73, 177},
    { 66,  75, 181}, { 67,  78, 186}, { 68,  81, 191}, { 68,  84, 195},
    { 68,  86, 199}, { 69,  89, 203}, { 69,  92, 207}, { 69,  94, 211},
    { 70,  97, 214}, { 70, 100, 218}, { 70, 102, 221}, { 70, 105, 224},
    { 70, 107, 227}, { 71, 110, 230}, { 71, 113, 233}, { 71, 115, 235},
    { 71, 118, 238}, { 71, 120, 240}, { 71, 123, 242}, { 70, 125, 244},
    { 70, 128, 246}, { 70, 130, 248}, { 70, 133, 250}, { 70, 135, 251},
    { 69, 138, 252}, { 69, 140, 253}, { 68, 143, 254}, { 67, 145, 254},
    { 66, 148, 255}, { 65, 150, 255}, { 64, 153, 255}, { 62, 155, 254},
    { 61, 158, 254}, { 59, 160, 253}, { 58, 163, 252}, { 56, 165, 251},
    { 55, 168, 250}, { 53, 171, 248}, { 51, 173, 247}, { 49, 175, 245},
    { 47, 178, 244}, { 46, 180, 242}, { 44, 183, 240}, { 42, 185, 238},
    { 40, 188, 235}, { 39, 190, 233}, { 37, 192, 231}, { 35, 195, 228},
    { 34, 197, 226}, { 32, 199, 223}, { 31, 201, 221}, { 30, 203, 218},
    { 28, 205, 216}, { 27, 208, 213}, { 26, 210, 210}, { 26, 212, 208},
    { 25, 213, 205}, { 24, 215, 202}, { 24, 217, 200}, { 24, 219, 197},
    { 24, 221, 194}, { 24, 222, 192}, { 24, 224, 189}, { 25, 226, 187},
    { 25, 227, 185}, { 26, 228, 182}, { 28, 230, 180}, { 29, 231, 178},
    { 31, 233, 175}, { 32, 234, 172}, { 34, 235, 170}, { 37, 236, 167},
    { 39, 238, 164}, { 42, 239, 161}, { 44, 240, 158}, { 47, 241, 155},
    { 50, 242, 152}, { 53, 243, 148}, { 56, 244, 145}, { 60, 245, 142},
    { 63, 246, 138}, { 67, 247, 135}, { 70, 248, 132}, { 74, 248, 128},
    { 78, 249, 125}, { 82, 250, 122}, { 85, 250, 118}, { 89, 251, 115},
    { 93, 252, 111}, { 97, 252, 108}, {101, 253, 105}, {105, 253, 102},
    {109, 254,  98}, {113, 254,  95}, {117, 254,  92}, {121, 254,  89},
    {125, 255,  86}, {128, 255,  83}, {132, 255,  81}, {136, 255,  78},
    {139, 255,  75}, {143, 255,  73}, {146, 255,  71}, {150, 254,  68},
    {153, 254,  66}, {156, 254,  64}, {159, 253,  63}, {161, 253,  61},
    {164, 252,  60}, {167, 252,  58}, {169, 251,  57}, {172, 251,  56},
    {175, 250,  55}, {177, 249,  54}, {180, 248,  54}, {183, 247,  53},
    {185, 246,  53}, {188, 245,  52}, {190, 244,  52}, {193, 243,  52},
    {195, 241,  52}, {198, 240,  52}, {200, 239,  52}, {203, 237,  52},
    {205, 236,  52}, {208, 234,  52}, {210, 233,  53}, {212, 231,  53},
    {215, 229,  53}, {217, 228,  54}, {219, 226,  54}, {221, 224,  55},
    {223, 223,  55}, {225, 221,  55}, {227, 219,  56}, {229, 217,  56},
    {231, 215,  57}, {233, 213,  57}, {235, 211,  57}, {236, 209,  58},
    {238, 207,  58}, {239, 205,  58}, {241, 203,  58}, {242, 201,  58},
    {244, 199,  58}, {245, 197,  58}, {246, 195,  58}, {247, 193,  58},
    {248, 190,  57}, {249, 188,  57}, {250, 186,  57}, {251, 184,  56},
    {251, 182,  55}, {252, 179,  54}, {252, 177,  54}, {253, 174,  53},
    {253, 172,  52}, {254, 169,  51}, {254, 167,  50}, {254, 164,  49},
    {254, 161,  48}, {254, 158,  47}, {254, 155,  45}, {254, 153,  44},
    {254, 150,  43}, {254, 147,  42}, {254, 144,  41}, {253, 141,  39},
    {253, 138,  38}, {252, 135,  37}, {252, 132,  35}, {251, 129,  34},
    {251, 126,  33}, {250, 123,  31}, {249, 120,  30}, {249, 117,  29},
    {248, 114,  28}, {247, 111,  26}, {246, 108,  25}, {245, 105,  24},
    {244, 102,  23}, {243,  99,  21}, {242,  96,  20}, {241,  93,  19},
    {240,  91,  18}, {239,  88,  17}, {237,  85,  16}, {236,  83,  15},
    {235,  80,  14}, {234,  78,  13}, {232,  75,  12}, {231,  73,  12},
    {229,  71,  11}, {228,  69,  10}, {226,  67,  10}, {225,  65,   9},
    {223,  63,   8}, {221,  61,   8}, {220,  59,   7}, {218,  57,   7},
    {216,  55,   6}, {214,  53,   6}, {212,  51,   5}, {210,  49,   5},
    {208,  47,   5}, {206,  45,   4}, {204,  43,   4}, {202,  42,   4},
    {200,  40,   3}, {197,  38,   3}, {195,  37,   3}, {193,  35,   2},
    {190,  33,   2}, {188,  32,   2}, {185,  30,   2}, {183,  29,   2},
    {180,  27,   1}, {178,  26,   1}, {175,  24,   1}, {172,  23,   1},
    {169,  22,   1}, {167,  20,   1}, {164,  19,   1}, {161,  18,   1},
    {158,  16,   1}, {155,  15,   1}, {152,  14,   1}, {149,  13,   1},
    {146,  11,   1}, {142,  10,   1}, {139,   9,   2}, {136,   8,   2},
    {133,   7,   2}, {129,   6,   2}, {126,   5,   2}, {122,   4,   3}
}};
// clang-format on

}  // namespace termchart::detail

// ===== include/termchart/detail/unicode_tables.hpp =====
// unicode_tables.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
// 只回答"码点级朴素宽度"，不含 emoji 序列规则（见 docs/design/emoji_width.md）；
// cluster 宽度由 util/wcwidth.hpp 结合 core/grapheme 数据结构给出。


namespace termchart::detail {

struct CpRange {
    std::uint32_t first;   // 闭区间起点
    std::uint32_t last;    // 闭区间终点
};

constexpr bool in_any_range(const CpRange* table, std::size_t n, std::uint32_t cp) noexcept {
    std::size_t lo = 0;
    std::size_t hi = n;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (cp < table[mid].first) {
            hi = mid;
        } else if (cp > table[mid].last) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

// East Asian Width = Wide / Fullwidth：显示宽度 2。
inline constexpr std::size_t kEastAsianWideCount = 103;
// clang-format off
inline constexpr std::array<CpRange, kEastAsianWideCount> kEastAsianWide{{
    {0x1100, 0x115F},
    {0x231A, 0x231B},
    {0x2329, 0x232A},
    {0x23F0, 0x23F0},
    {0x23F3, 0x23F3},
    {0x25FD, 0x25FE},
    {0x2614, 0x2615},
    {0x2648, 0x2653},
    {0x267F, 0x267F},
    {0x2693, 0x2693},
    {0x26A1, 0x26A1},
    {0x26AA, 0x26AB},
    {0x26BD, 0x26BE},
    {0x26C4, 0x26C5},
    {0x26CE, 0x26CE},
    {0x26D4, 0x26D4},
    {0x26EA, 0x26EA},
    {0x26F2, 0x26F3},
    {0x26F5, 0x26F5},
    {0x26FA, 0x26FA},
    {0x26FD, 0x26FD},
    {0x2705, 0x2705},
    {0x270A, 0x270B},
    {0x2728, 0x2728},
    {0x274C, 0x274C},
    {0x274E, 0x274E},
    {0x2753, 0x2755},
    {0x2757, 0x2757},
    {0x2795, 0x2797},
    {0x27B0, 0x27B0},
    {0x27BF, 0x27BF},
    {0x2B1B, 0x2B1C},
    {0x2B50, 0x2B50},
    {0x2B55, 0x2B55},
    {0x2E80, 0x2E99},
    {0x2E9B, 0x2EF3},
    {0x2F00, 0x2FD5},
    {0x2FF0, 0x2FFB},
    {0x3000, 0x303E},
    {0x3041, 0x3096},
    {0x3099, 0x30FF},
    {0x3105, 0x312F},
    {0x3131, 0x318E},
    {0x3190, 0x31E3},
    {0x31F0, 0x321E},
    {0x3220, 0x3247},
    {0x3250, 0x32FE},
    {0x3300, 0x33FF},
    {0x3400, 0x4DBF},
    {0x4E00, 0x9FFF},
    {0xA000, 0xA48C},
    {0xA490, 0xA4C6},
    {0xA960, 0xA97C},
    {0xAC00, 0xD7A3},
    {0xF900, 0xFAFF},
    {0xFE10, 0xFE19},
    {0xFE30, 0xFE52},
    {0xFE54, 0xFE66},
    {0xFE68, 0xFE6B},
    {0xFF01, 0xFF60},
    {0xFFE0, 0xFFE6},
    {0x16FE0, 0x16FE4},
    {0x17000, 0x187F7},
    {0x18800, 0x18AF2},
    {0x1B000, 0x1B11E},
    {0x1B170, 0x1B2FB},
    {0x1F004, 0x1F004},
    {0x1F0CF, 0x1F0CF},
    {0x1F18E, 0x1F18E},
    {0x1F191, 0x1F19A},
    {0x1F200, 0x1F320},
    {0x1F32D, 0x1F335},
    {0x1F337, 0x1F37C},
    {0x1F37E, 0x1F393},
    {0x1F3A0, 0x1F3CA},
    {0x1F3CF, 0x1F3D3},
    {0x1F3E0, 0x1F3F0},
    {0x1F3F4, 0x1F3F4},
    {0x1F3F8, 0x1F43E},
    {0x1F440, 0x1F440},
    {0x1F442, 0x1F4FC},
    {0x1F4FF, 0x1F53D},
    {0x1F54B, 0x1F54E},
    {0x1F550, 0x1F567},
    {0x1F57A, 0x1F57A},
    {0x1F595, 0x1F596},
    {0x1F5A4, 0x1F5A4},
    {0x1F5FB, 0x1F64F},
    {0x1F680, 0x1F6C5},
    {0x1F6CC, 0x1F6CC},
    {0x1F6D0, 0x1F6D2},
    {0x1F6EB, 0x1F6EC},
    {0x1F6F4, 0x1F6F9},
    {0x1F910, 0x1F93E},
    {0x1F940, 0x1F970},
    {0x1F973, 0x1F976},
    {0x1F97A, 0x1F97A},
    {0x1F97C, 0x1F9A2},
    {0x1F9B0, 0x1F9B9},
    {0x1F9C0, 0x1F9C2},
    {0x1F9D0, 0x1F9FF},
    {0x20000, 0x2FFFD},
    {0x30000, 0x3FFFD},
}};
// clang-format on

// East Asian Width = Ambiguous：策略相关，默认按窄（见 util/wcwidth.hpp）。
inline constexpr std::size_t kEastAsianAmbiguousCount = 151;
// clang-format off
inline constexpr std::array<CpRange, kEastAsianAmbiguousCount> kEastAsianAmbiguous{{
    {0x00A1, 0x00A1},
    {0x00A4, 0x00A4},
    {0x00A7, 0x00A8},
    {0x00AA, 0x00AA},
    {0x00AE, 0x00AE},
    {0x00B0, 0x00B4},
    {0x00B6, 0x00BA},
    {0x00BC, 0x00BF},
    {0x00C6, 0x00C6},
    {0x00D0, 0x00D0},
    {0x00D7, 0x00D8},
    {0x00DE, 0x00E1},
    {0x00E6, 0x00E6},
    {0x00E8, 0x00EA},
    {0x00EC, 0x00ED},
    {0x00F0, 0x00F0},
    {0x00F2, 0x00F3},
    {0x00F7, 0x00FA},
    {0x00FC, 0x00FC},
    {0x00FE, 0x00FE},
    {0x0101, 0x0101},
    {0x0111, 0x0111},
    {0x0113, 0x0113},
    {0x011B, 0x011B},
    {0x0126, 0x0127},
    {0x012B, 0x012B},
    {0x0131, 0x0133},
    {0x0138, 0x0138},
    {0x013F, 0x0142},
    {0x0144, 0x0144},
    {0x0148, 0x014B},
    {0x014D, 0x014D},
    {0x0152, 0x0153},
    {0x0166, 0x0167},
    {0x016B, 0x016B},
    {0x01CE, 0x01CE},
    {0x01D0, 0x01D0},
    {0x01D2, 0x01D2},
    {0x01D4, 0x01D4},
    {0x01D6, 0x01D6},
    {0x01D8, 0x01D8},
    {0x01DA, 0x01DA},
    {0x01DC, 0x01DC},
    {0x0251, 0x0251},
    {0x0261, 0x0261},
    {0x02C4, 0x02C4},
    {0x02C7, 0x02C7},
    {0x02C9, 0x02CB},
    {0x02D8, 0x02DB},
    {0x02DD, 0x02DD},
    {0x0391, 0x03A1},
    {0x03A3, 0x03A9},
    {0x03B1, 0x03C1},
    {0x03C3, 0x03C9},
    {0x0401, 0x0401},
    {0x0410, 0x044F},
    {0x0451, 0x0451},
    {0x2010, 0x2010},
    {0x2013, 0x2016},
    {0x2018, 0x2019},
    {0x201C, 0x201D},
    {0x2020, 0x2022},
    {0x2024, 0x2027},
    {0x2030, 0x2030},
    {0x2032, 0x2033},
    {0x203B, 0x203B},
    {0x203E, 0x203E},
    {0x2074, 0x2074},
    {0x207F, 0x207F},
    {0x2081, 0x2084},
    {0x20AC, 0x20AC},
    {0x2103, 0x2103},
    {0x2105, 0x2105},
    {0x2109, 0x2109},
    {0x2113, 0x2113},
    {0x2116, 0x2116},
    {0x2121, 0x2122},
    {0x2126, 0x2126},
    {0x212B, 0x212B},
    {0x2153, 0x2154},
    {0x215B, 0x215E},
    {0x2160, 0x216B},
    {0x2170, 0x2179},
    {0x2189, 0x2189},
    {0x2190, 0x2199},
    {0x21B8, 0x21B9},
    {0x21D2, 0x21D2},
    {0x21D4, 0x21D4},
    {0x21E7, 0x21E7},
    {0x2200, 0x2200},
    {0x2202, 0x2203},
    {0x2206, 0x2208},
    {0x220B, 0x220B},
    {0x220F, 0x220F},
    {0x2211, 0x2211},
    {0x2215, 0x2215},
    {0x221A, 0x221A},
    {0x221D, 0x2220},
    {0x2223, 0x2223},
    {0x2225, 0x2225},
    {0x2227, 0x222C},
    {0x222E, 0x222E},
    {0x2234, 0x2237},
    {0x223C, 0x223D},
    {0x2248, 0x2248},
    {0x224C, 0x224C},
    {0x2252, 0x2252},
    {0x2260, 0x2261},
    {0x2264, 0x2267},
    {0x226A, 0x226B},
    {0x226E, 0x226F},
    {0x2282, 0x2283},
    {0x2286, 0x2287},
    {0x2295, 0x2295},
    {0x2299, 0x2299},
    {0x22A5, 0x22A5},
    {0x22BF, 0x22BF},
    {0x2312, 0x2312},
    {0x2460, 0x249B},
    {0x24EA, 0x24FF},
    {0x2500, 0x254B},
    {0x2550, 0x2573},
    {0x2580, 0x258F},
    {0x2592, 0x2595},
    {0x25A0, 0x25A1},
    {0x25A3, 0x25A9},
    {0x25B2, 0x25B3},
    {0x25B6, 0x25B7},
    {0x25BC, 0x25BD},
    {0x25C0, 0x25C1},
    {0x25C6, 0x25C8},
    {0x25CB, 0x25CB},
    {0x25CE, 0x25D1},
    {0x25E2, 0x25E5},
    {0x25EF, 0x25EF},
    {0x2605, 0x2606},
    {0x2609, 0x2609},
    {0x260E, 0x260F},
    {0x261C, 0x261C},
    {0x261E, 0x261E},
    {0x2640, 0x2640},
    {0x2642, 0x2642},
    {0x2660, 0x2661},
    {0x2663, 0x2665},
    {0x2667, 0x266A},
    {0x266C, 0x266D},
    {0x266F, 0x266F},
    {0x273D, 0x273D},
    {0x2776, 0x277F},
    {0xE000, 0xF8FF},
    {0xFFFD, 0xFFFD},
}};
// clang-format on

// 组合记号 Mn / Me + 变体选择符 + emoji 肤色修饰符：宽度 0。
inline constexpr std::size_t kCombiningCount = 131;
// clang-format off
inline constexpr std::array<CpRange, kCombiningCount> kCombining{{
    {0x0300, 0x036F},
    {0x0483, 0x0489},
    {0x0591, 0x05BD},
    {0x05BF, 0x05BF},
    {0x05C1, 0x05C2},
    {0x0610, 0x061A},
    {0x064B, 0x065F},
    {0x0670, 0x0670},
    {0x06D6, 0x06DC},
    {0x06DF, 0x06E4},
    {0x06E7, 0x06E8},
    {0x0711, 0x0711},
    {0x0730, 0x074A},
    {0x07A6, 0x07B0},
    {0x07EB, 0x07F3},
    {0x0816, 0x0819},
    {0x081B, 0x0823},
    {0x0825, 0x0827},
    {0x0829, 0x082D},
    {0x0859, 0x085B},
    {0x08D4, 0x0903},
    {0x093A, 0x093C},
    {0x0941, 0x0948},
    {0x094D, 0x094D},
    {0x0951, 0x0957},
    {0x0962, 0x0963},
    {0x0981, 0x0981},
    {0x09BC, 0x09BC},
    {0x09C1, 0x09C4},
    {0x09CD, 0x09CD},
    {0x0E31, 0x0E31},
    {0x0E34, 0x0E3A},
    {0x0E47, 0x0E4E},
    {0x0EB1, 0x0EB1},
    {0x0EB4, 0x0EBC},
    {0x0EC8, 0x0ECD},
    {0x0F18, 0x0F19},
    {0x0F35, 0x0F35},
    {0x0F37, 0x0F37},
    {0x0F39, 0x0F39},
    {0x0F71, 0x0F7E},
    {0x0F80, 0x0F84},
    {0x0F86, 0x0F87},
    {0x135D, 0x135F},
    {0x1712, 0x1714},
    {0x1732, 0x1734},
    {0x1752, 0x1753},
    {0x1772, 0x1773},
    {0x17B4, 0x17B5},
    {0x17B7, 0x17BD},
    {0x17C6, 0x17C6},
    {0x17C9, 0x17D3},
    {0x180B, 0x180E},
    {0x18A9, 0x18A9},
    {0x1920, 0x1922},
    {0x1927, 0x1928},
    {0x1932, 0x1932},
    {0x1939, 0x193B},
    {0x1A17, 0x1A18},
    {0x1A1B, 0x1A1B},
    {0x1A56, 0x1A56},
    {0x1A58, 0x1A5E},
    {0x1A60, 0x1A60},
    {0x1A62, 0x1A62},
    {0x1A65, 0x1A6C},
    {0x1A73, 0x1A7C},
    {0x1A7F, 0x1A7F},
    {0x1AB0, 0x1ACE},
    {0x1B00, 0x1B04},
    {0x1B34, 0x1B44},
    {0x1B6B, 0x1B73},
    {0x1B80, 0x1B82},
    {0x1BA1, 0x1BAD},
    {0x1BE6, 0x1BF3},
    {0x1C2C, 0x1C33},
    {0x1C36, 0x1C37},
    {0x1CD0, 0x1CE8},
    {0x1CED, 0x1CED},
    {0x1CF4, 0x1CF4},
    {0x1CF8, 0x1CF9},
    {0x1DC0, 0x1DF5},
    {0x1DFC, 0x1DFF},
    {0x20D0, 0x20F0},
    {0x2CEF, 0x2CF1},
    {0x2D7F, 0x2D7F},
    {0x2DE0, 0x2DFF},
    {0x3099, 0x309A},
    {0xA66F, 0xA672},
    {0xA674, 0xA67D},
    {0xA69E, 0xA69F},
    {0xA6F0, 0xA6F1},
    {0xA802, 0xA802},
    {0xA806, 0xA806},
    {0xA80B, 0xA80B},
    {0xA825, 0xA826},
    {0xA8C4, 0xA8C5},
    {0xA8E0, 0xA8F1},
    {0xA926, 0xA92D},
    {0xA947, 0xA953},
    {0xA980, 0xA983},
    {0xA9B3, 0xA9C0},
    {0xAA29, 0xAA36},
    {0xAA43, 0xAA43},
    {0xAA4C, 0xAA4D},
    {0xAA7B, 0xAA7D},
    {0xAAB0, 0xAAB0},
    {0xAAB2, 0xAAB4},
    {0xAAB7, 0xAABE},
    {0xAAC0, 0xAAC1},
    {0xAAEB, 0xAAEF},
    {0xAAF5, 0xAAF6},
    {0xABE3, 0xABEA},
    {0xABEC, 0xABED},
    {0xFB1E, 0xFB1E},
    {0xFE00, 0xFE0F},
    {0xFE20, 0xFE2F},
    {0x101FD, 0x101FD},
    {0x102E0, 0x102E0},
    {0x10376, 0x1037A},
    {0x10EAB, 0x10EAC},
    {0x11300, 0x11301},
    {0x1D165, 0x1D169},
    {0x1D16D, 0x1D172},
    {0x1D17B, 0x1D182},
    {0x1D185, 0x1D18B},
    {0x1D1AA, 0x1D1AD},
    {0x1D242, 0x1D244},
    {0x1F3FB, 0x1F3FF},
    {0xE0001, 0xE0001},
    {0xE0020, 0xE007F},
    {0xE0100, 0xE01EF},
}};
// clang-format on

// 格式字符 Cf 与特定零宽控制：宽度 0。
inline constexpr std::size_t kFormatCount = 18;
// clang-format off
inline constexpr std::array<CpRange, kFormatCount> kFormat{{
    {0x00AD, 0x00AD},
    {0x0600, 0x0605},
    {0x061C, 0x061C},
    {0x06DD, 0x06DD},
    {0x070F, 0x070F},
    {0x08E2, 0x08E2},
    {0x200B, 0x200F},
    {0x202A, 0x202E},
    {0x2060, 0x2064},
    {0x2066, 0x206F},
    {0xFEFF, 0xFEFF},
    {0xFFF9, 0xFFFB},
    {0x110BD, 0x110BD},
    {0x1D159, 0x1D159},
    {0x1D173, 0x1D17A},
    {0xE0002, 0xE001F},
    {0xE0080, 0xE00FF},
    {0xE01F0, 0xE0FFF},
}};
// clang-format on

constexpr bool is_east_asian_wide(char32_t cp) noexcept {
    return in_any_range(kEastAsianWide.data(), kEastAsianWideCount,
                        static_cast<std::uint32_t>(cp));
}

constexpr bool is_east_asian_ambiguous(char32_t cp) noexcept {
    return in_any_range(kEastAsianAmbiguous.data(), kEastAsianAmbiguousCount,
                        static_cast<std::uint32_t>(cp));
}

constexpr bool is_combining(char32_t cp) noexcept {
    return in_any_range(kCombining.data(), kCombiningCount, static_cast<std::uint32_t>(cp));
}

constexpr bool is_format(char32_t cp) noexcept {
    return in_any_range(kFormat.data(), kFormatCount, static_cast<std::uint32_t>(cp));
}

// C0 / DEL / C1：控制字符，不可直接绘制。
constexpr bool is_control(char32_t cp) noexcept {
    const auto c = static_cast<std::uint32_t>(cp);
    return c < 0x20u || c == 0x7Fu || (c >= 0x80u && c <= 0x9Fu);
}

constexpr bool is_zero_width(char32_t cp) noexcept {
    return is_control(cp) || is_combining(cp) || is_format(cp);
}

// 朴素宽度：0 / 1 / 2。不含 emoji ZWJ 序列规则。
constexpr int naive_width(char32_t cp) noexcept {
    if (is_zero_width(cp)) {
        return 0;
    }
    if (is_east_asian_wide(cp)) {
        return 2;
    }
    return 1;
}

}  // namespace termchart::detail

// ===== include/termchart/core/arena.hpp =====
// arena.hpp —— 帧分配器
//
// 用途：每帧大量的短命小对象（anvas 路径片段、布局候选矩形）。
// 语义：**只能 reset()，不能单独释放**——一帧结束整块回收，无析构开销。
//
// 硬约束（docs/design/memory_model.md）：
//   * 持有堆资源的类型**禁止**进 arena：create<T>() 用 static_assert 卡住
//     （非 trivially destructible 直接编译失败）。
//   * Arena 自身非拷贝非移动：它是"帧"的同义词。

#include <memory>
#include <new>
#include <type_traits>
#include <vector>


namespace termchart {

class Arena {
public:
    explicit Arena(std::size_t initial_capacity = 64 * 1024);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    // 取原始内存：不做构造。对齐必须是 2 的幂。
    TERMCHART_API void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    // 原地构造。静态断言防止"持堆类型"混入（违反内存模型）。
    template <class T, class... Args>
    T* create(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "Arena 只接纳 trivially destructible 类型：持有堆资源的对象请改用普通容器");
        void* raw = allocate(sizeof(T), alignof(T));
        return ::new (raw) T(std::forward<Args>(args)...);
    }

    // 一帧结束：复用全部内存（不调用析构，因 T 必为 trivially destructible）。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] TERMCHART_API std::size_t used_bytes() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t capacity_bytes() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t block_count() const noexcept;

private:
    struct Block {
        std::unique_ptr<std::byte[]> data;
        std::size_t size = 0;
        std::size_t used = 0;
    };

    std::vector<Block> blocks_;
};

}  // namespace termchart

// ===== include/termchart/core/attr.hpp =====
// attr.hpp —— 字符属性位（header-only）
//
// 属性只有这五个：Bold / Dim / Italic / Underline / Reverse。
// 位运算全部 constexpr，便于编译期组合常量样式。


namespace termchart {

enum class Attr : std::uint16_t {
    None = 0,
    Bold = 1u << 0,
    Dim = 1u << 1,
    Italic = 1u << 2,
    Underline = 1u << 3,
    Reverse = 1u << 4,
};

using AttrMask = std::uint16_t;

constexpr AttrMask mask_of(Attr a) noexcept { return static_cast<AttrMask>(a); }
constexpr bool has_flag(AttrMask m, Attr a) noexcept { return (m & mask_of(a)) != 0; }

constexpr AttrMask operator|(Attr a, Attr b) noexcept {
    return static_cast<AttrMask>(static_cast<AttrMask>(a) | static_cast<AttrMask>(b));
}
constexpr AttrMask operator|(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m | static_cast<AttrMask>(a));
}
constexpr AttrMask operator|(Attr a, AttrMask m) noexcept {
    return static_cast<AttrMask>(m | static_cast<AttrMask>(a));
}
constexpr AttrMask& operator|=(AttrMask& m, Attr a) noexcept {
    m = m | a;
    return m;
}
constexpr AttrMask operator&(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m & static_cast<AttrMask>(a));
}
// 去掉某一位。注：AttrMask 是整型别名，不能重载 operator~（非类类型），
// 因此需要显式命名函数而不是运算符。
constexpr AttrMask without(AttrMask m, Attr a) noexcept {
    return static_cast<AttrMask>(m & static_cast<AttrMask>(~static_cast<unsigned>(a)));
}

inline constexpr AttrMask kNoAttrs = 0;

}  // namespace termchart

// ===== include/termchart/core/error.hpp =====
// error.hpp —— 错误枚举与类别
//
// 错误处理边界见 docs/design/error_handling.md：
//   * **可恢复的、用户可能遇到的**失败 → Expected / Result<T>（本文件 + expected.hpp）；
//   * **契约破坏 / 内部不变量** → util/assert.hpp（Release 下 abort）；
//   * 渲染热路径一律不用异常：改为返回 Result 或钳位。



namespace termchart {

enum class ErrorCode : std::int32_t {
    Ok = 0,

    // 领域 / 参数
    InvalidArgument = 100,
    OutOfRange = 101,
    Unsupported = 102,
    NotImplemented = 103,
    NotInitialized = 104,

    // 解析 / 编码
    MalformedUtf8 = 200,
    ParseError = 201,

    // 平台 / IO
    IoError = 300,
    TerminalUnavailable = 301,
    PlatformError = 302,
};

// 错误类别：给日志与上层分流使用，不与具体码一一对应。
enum class ErrorCategory : std::uint8_t {
    None = 0,
    Logic,       // 调用方契约问题，改代码才能解决
    Domain,      // 数据/参数不合经营范围
    Encoding,    // UTF-8 / 编码相关
    Platform,    // IO / 终端 / OS
};

// constexpr 函数隐含 inline，因此定义必须留在头文件里；to_string/describe 在 .cpp。
constexpr std::string_view category_of(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "none";
        case ErrorCode::InvalidArgument:
        case ErrorCode::OutOfRange:
        case ErrorCode::NotInitialized:
            return "logic";
        case ErrorCode::Unsupported:
        case ErrorCode::NotImplemented:
            return "domain";
        case ErrorCode::MalformedUtf8:
        case ErrorCode::ParseError:
            return "encoding";
        case ErrorCode::IoError:
        case ErrorCode::TerminalUnavailable:
        case ErrorCode::PlatformError:
            return "platform";
    }
    return "none";
}

// 稳定英文标识：不随语言/文案变化，可用于断言与测试。
const char* to_string(ErrorCode code) noexcept;

// 便于打日志的人类可读描述（英文，避免编码问题）。
const char* describe(ErrorCode code) noexcept;

constexpr bool is_ok(ErrorCode code) noexcept { return code == ErrorCode::Ok; }

}  // namespace termchart

// ===== include/termchart/core/expected.hpp =====
// expected.hpp —— Expected<T, E> 与 Result<T> 别名
//
// 为什么不直接用 std::expected：它是 C++23 库设施，本库要求 C++20；
// 而且我们只需要"值或错误码"这个小子集。语义对齐 std::expected：
//   * has_value() / operator bool()；
//   * value() 在无值时**抛 std::logic_error**（调用方已是 bug）；
//   * error() 在有值时属 UB（与 std::expected 一致，此处以异常兜底）。

#include <variant>


namespace termchart {

// 无值访问时的异常来源（实现在 src/core/expected.cpp，避免头文件带 throw 模板膨胀）。
[[noreturn]] TERMCHART_API void throw_bad_expected_access(const char* what) noexcept(false);

template <class E>
class Unexpected {
public:
    explicit constexpr Unexpected(E error) : error_(std::move(error)) {}
    constexpr const E& error() const noexcept { return error_; }

private:
    E error_;
};

template <class E>
Unexpected(E) -> Unexpected<E>;

template <class T, class E>
class Expected {
public:
    using value_type = T;
    using error_type = E;

    constexpr Expected(T value) : store_(std::in_place_index<0>, std::move(value)) {}
    constexpr Expected(Unexpected<E> err) : store_(std::in_place_index<1>, std::move(err.error())) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return store_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr const T& value() const noexcept(false) {
        if (!has_value()) {
            throw_bad_expected_access("termchart::Expected::value() 无值");
        }
        return *std::get_if<0>(&store_);
    }

    [[nodiscard]] constexpr T& value() noexcept(false) {
        if (!has_value()) {
            throw_bad_expected_access("termchart::Expected::value() 无值");
        }
        return *std::get_if<0>(&store_);
    }

    [[nodiscard]] constexpr const E& error() const noexcept(false) {
        if (has_value()) {
            throw_bad_expected_access("termchart::Expected::error() 无错误");
        }
        return *std::get_if<1>(&store_);
    }

    // std::expected 兼容的解引用：无值时抛（与 value() 同）。
    // 非 const 重载必须存在：否则 `*result` 永远是 const 引用，调用方拿到
    // "改不了的结果"——写 `parse(x).value().add_...()` 这类代码时会卡住。
    [[nodiscard]] constexpr const T& operator*() const noexcept(false) { return value(); }
    [[nodiscard]] constexpr T& operator*() noexcept(false) { return value(); }
    [[nodiscard]] constexpr const T* operator->() const noexcept(false) { return &value(); }
    [[nodiscard]] constexpr T* operator->() noexcept(false) { return &value(); }

    [[nodiscard]] constexpr T value_or(T fallback) const noexcept {
        return has_value() ? *std::get_if<0>(&store_) : std::move(fallback);
    }

    // 链式：成功时变换值，失败时原样传递错误。
    template <class Fn>
    [[nodiscard]] constexpr auto and_then(Fn&& fn) const {
        using R = decltype(fn(std::declval<const T&>()));
        return has_value() ? fn(*std::get_if<0>(&store_)) : R(Unexpected<E>(*std::get_if<1>(&store_)));
    }

private:
    std::variant<T, E> store_;
};

// Result<T>：错误类型固定为 ErrorCode，覆盖绝大多数场景。
template <class T>
using Result = Expected<T, ErrorCode>;

// 无返回值：成功与否只用错误码表达。用 Result<std::monostate> 太饶，这里给专用alias。
using Status = ErrorCode;

}  // namespace termchart

// ===== include/termchart/core/grapheme.hpp =====
// grapheme.hpp —— 组合序列（grapheme cluster）的**数据结构**
//
// 分层约定（见 ROADMAP 阶段 1"一旦定错后续全错"）：
//   * 本文件**只定义结构，不做分割**；Splitter 在 util/grapheme.hpp（L1）；
//   * 宽度模型是"主码点 + 侧面列表"（side bearing）：Cell 存主码点，
//     CellBuffer 用稀疏表保存额外的组合记号。
//
// 为什么定长内联数组而非 vector：CellBuffer 一帧内可能有大量侧面项，
// 逐个堆分配会毁掉帧率与局部性。上限来自 UAX #15 的建议：
// 实用文本极少超过 7 个组合记号，超限则丢弃尾部（并在 add 时返回 false）。



namespace termchart {

inline constexpr std::size_t kMaxSideBearings = 7;

class GraphemeCluster {
public:
    constexpr GraphemeCluster() noexcept = default;
    constexpr explicit GraphemeCluster(char32_t base) noexcept : base_(base) {}

    [[nodiscard]] constexpr char32_t base() const noexcept { return base_; }
    constexpr void set_base(char32_t cp) noexcept { base_ = cp; }

    // 追加组合记号（side bearing）。已满返回 false，调用方应放弃尾部而非 "抛异常：渲染路径无异常。
    TERMCHART_API bool add_side_bearing(char32_t cp) noexcept;

    [[nodiscard]] constexpr std::size_t side_count() const noexcept { return count_; }
    [[nodiscard]] constexpr const char32_t* sides() const noexcept { return sides_.data(); }
    [[nodiscard]] constexpr bool has_sides() const noexcept { return count_ != 0; }
    constexpr void clear_sides() noexcept { count_ = 0; }

    TERMCHART_API bool operator==(const GraphemeCluster& other) const noexcept;
    TERMCHART_API bool operator!=(const GraphemeCluster& other) const noexcept;

private:
    char32_t base_ = U' ';
    std::array<char32_t, kMaxSideBearings> sides_{};
    std::uint8_t count_ = 0;
};

}  // namespace termchart

// ===== include/termchart/core/point.hpp =====
// point.hpp —— 浮点坐标点（header-only）
//
// canvas/ 用浮点，"最后一像素/半格"才能对齐；core 层只提供 POD 与必要算子。

namespace termchart {

struct Point {
    double x = 0.0;
    double y = 0.0;

    constexpr Point() noexcept = default;
    constexpr Point(double px, double py) noexcept : x(px), y(py) {}

    constexpr bool operator==(const Point&) const noexcept = default;
    constexpr bool operator!=(const Point&) const noexcept = default;

    constexpr Point operator+(const Point& o) const noexcept { return Point{x + o.x, y + o.y}; }
    constexpr Point operator-(const Point& o) const noexcept { return Point{x - o.x, y - o.y}; }
    constexpr Point operator*(double s) const noexcept { return Point{x * s, y * s}; }
    constexpr Point operator/(double s) const noexcept { return Point{x / s, y / s}; }
    constexpr Point& operator+=(const Point& o) noexcept {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Point& operator-=(const Point& o) noexcept {
        x -= o.x;
        y -= o.y;
        return *this;
    }
};

}  // namespace termchart

// ===== include/termchart/core/rect.hpp =====
// rect.hpp —— 浮点矩形（header-only）
//
// 约定：x/y 为左上角，宽高非负；所有关系谓词为闭-开区间（right/bottom 不含在内），
// 与 canvas 的半开像素约定一致。


namespace termchart {

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    constexpr Rect() noexcept = default;
    constexpr Rect(double px, double py, double pw, double ph) noexcept
        : x(px), y(py), w(pw), h(ph) {}
    constexpr Rect(Point origin, Point size) noexcept
        : x(origin.x), y(origin.y), w(size.x), h(size.y) {}

    constexpr bool operator==(const Rect&) const noexcept = default;
    constexpr bool operator!=(const Rect&) const noexcept = default;

    constexpr double right() const noexcept { return x + w; }
    constexpr double bottom() const noexcept { return y + h; }
    constexpr Point origin() const noexcept { return Point{x, y}; }
    constexpr Point center() const noexcept { return Point{x + w / 2.0, y + h / 2.0}; }
    constexpr bool empty() const noexcept { return !(w > 0.0) || !(h > 0.0); }
    constexpr double area() const noexcept { return w * h; }

    constexpr bool contains(const Point& p) const noexcept {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }
    constexpr bool contains(double px, double py) const noexcept {
        return contains(Point{px, py});
    }

    // 半开区间相交：仅接触边（o.x == right()）不算相交；退化矩形（w/h <= 0）是空集，
    // 与任何矩形（含另一个退化矩形）都不相交。否则会出现 intersects 为真而
    // intersection 为空、contains 对任何点为假的矛盾。加守卫后
    // intersects(o) 与 !intersection(o).empty() 严格等价。
    constexpr bool intersects(const Rect& o) const noexcept {
        return !empty() && !o.empty() && x < o.right() && o.x < right() && y < o.bottom() &&
               o.y < bottom();
    }

    // 相交矩形；无交（含任一矩形退化）返回空矩形（w=h=0）。
    constexpr Rect intersection(const Rect& o) const noexcept {
        if (!intersects(o)) {
            return Rect{};
        }
        const double nx = x > o.x ? x : o.x;
        const double ny = y > o.y ? y : o.y;
        return Rect{nx, ny, (right() < o.right() ? right() : o.right()) - nx,
                    (bottom() < o.bottom() ? bottom() : o.bottom()) - ny};
    }

    // 包围盒；空矩形参与时返回另一方。
    constexpr Rect union_with(const Rect& o) const noexcept {
        if (empty()) {
            return o;
        }
        if (o.empty()) {
            return *this;
        }
        const double nx = x < o.x ? x : o.x;
        const double ny = y < o.y ? y : o.y;
        return Rect{nx, ny, (right() > o.right() ? right() : o.right()) - nx,
                    (bottom() > o.bottom() ? bottom() : o.bottom()) - ny};
    }
};

}  // namespace termchart

// ===== include/termchart/core/rgb.hpp =====
// rgb.hpp —— 颜色 POD（header-only，属 cmake/header_only.txt）
//
// 只表达颜色本身，不含"如何输出"：输出编码在 render/sgr.hpp（唯一 SGR 生成点），
// 降级策略在 color/quantize.hpp 与 term/color_fallback.hpp。


namespace termchart {

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr Rgb() noexcept = default;
    constexpr Rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        : r(red), g(green), b(blue) {}

    // 0xRRGGBB → Rgb；越界高位被忽略（只取低 24 位）。
    static constexpr Rgb from_hex(std::uint32_t hex) noexcept {
        return Rgb{static_cast<std::uint8_t>((hex >> 16) & 0xFFu),
                   static_cast<std::uint8_t>((hex >> 8) & 0xFFu),
                   static_cast<std::uint8_t>(hex & 0xFFu)};
    }

    constexpr std::uint32_t to_hex() const noexcept {
        return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }

    // 感知亮度（Rec.709），用于自动挑选对比色。
    constexpr double luma() const noexcept {
        return 0.2126 * static_cast<double>(r) + 0.7152 * static_cast<double>(g) +
               0.0722 * static_cast<double>(b);
    }

    constexpr bool operator==(const Rgb&) const noexcept = default;
    constexpr bool operator!=(const Rgb&) const noexcept = default;
};

inline constexpr Rgb kBlack{0, 0, 0};
inline constexpr Rgb kWhite{255, 255, 255};

}  // namespace termchart

// ===== include/termchart/core/color_depth.hpp =====
// color_depth.hpp —— 终端颜色能力档位（header-only）
//
// 与具体后端无关：它描述"终端能吃下多少颜色"，由 term/caps.hpp 产出，
// 被 render/sgr.hpp 与 color/quantize.hpp 消费。



namespace termchart {

// 升序：档位越高能力越强，可用关系运算符比较。
enum class ColorDepth : std::uint8_t {
    None = 0,      // 无色（不含 SGR 颜色参数）
    Ansi16 = 1,
    Ansi256 = 2,
    TrueColor = 3,
};

constexpr int color_depth_count(ColorDepth depth) noexcept {
    switch (depth) {
        case ColorDepth::None:
            return 1;
        case ColorDepth::Ansi16:
            return 16;
        case ColorDepth::Ansi256:
            return 256;
        case ColorDepth::TrueColor:
            return 16777216;
    }
    return 1;
}

constexpr bool operator<(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) < static_cast<std::uint8_t>(b);
}
constexpr bool operator<=(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) <= static_cast<std::uint8_t>(b);
}
constexpr bool operator>(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) > static_cast<std::uint8_t>(b);
}
constexpr bool operator>=(ColorDepth a, ColorDepth b) noexcept {
    return static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b);
}

// 目标色在给定档位下是否需要降级。Rgb 色在 TrueColor 下无需降级。
constexpr bool ensures_at_least(ColorDepth have, ColorDepth want) noexcept { return have >= want; }

}  // namespace termchart

// ===== include/termchart/core/span_utils.hpp =====
// span_utils.hpp —— span 安全切片（header-only）
//
// CellBuffer 对外暴露 std::span<Cell>；所有"取子区间"必须走这里，
// 让越界行为在一处定义：**钳位而非抛异常**（渲染热路径不允许异常）。

#include <functional>
#include <optional>
#include <span>

namespace termchart {

// [offset, offset+n) 钳位到 [0, size)；返回安全切片。
template <class T>
constexpr std::span<T> safe_subspan(std::span<T> source, std::size_t offset,
                                    std::size_t count) noexcept {
    if (offset >= source.size()) {
        return source.subspan(source.size(), 0);
    }
    const std::size_t available = source.size() - offset;
    return source.subspan(offset, count < available ? count : available);
}

template <class T>
constexpr std::span<const T> safe_subspan(std::span<const T> source, std::size_t offset,
                                          std::size_t count) noexcept {
    if (offset >= source.size()) {
        return source.subspan(source.size(), 0);
    }
    const std::size_t available = source.size() - offset;
    return source.subspan(offset, count < available ? count : available);
}

// 只读下标访问：越界返回 nullopt。
template <class T>
constexpr std::optional<std::reference_wrapper<const T>> checked_at(std::span<const T> source,
                                                                    std::size_t index) noexcept {
    if (index >= source.size()) {
        return std::nullopt;
    }
    return std::cref(source[index]);
}

// 两段 span 是否逐元素相等（尺寸不同直接 false）。
template <class T, class U>
constexpr bool span_equal(std::span<const T> lhs, std::span<const U> rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace termchart

// ===== include/termchart/core/style.hpp =====
// style.hpp —— 语义样式（属性位 + 前景 + 背景）
//
// Style 与后端无关：它不认识 SGR，不知道 A'tuin 终端能否显示斜体。
// 从 Style 到字节流的唯一转换点在 render/sgr.hpp。
//
// Color 用"带判别标识的值"而非 variant：Cell 是逐帧遍历百万次的热结构，
// sizeof(Color) 必须小而固定（8 字节），不能承担 variant 的分支与额外存储。



namespace termchart {

enum class ColorKind : std::uint8_t {
    None = 0,       // 未指定：沿用终端默认色
    Ansi16 = 1,     // 0..15
    Indexed = 2,    // 0..255（含 16..231 立方格，232..255 灰阶）
    Rgb = 3,        // 真彩
};

class Color {
public:
    constexpr Color() noexcept = default;

    static constexpr Color none() noexcept { return Color{}; }
    static constexpr Color ansi16(std::uint8_t index) noexcept {
        return Color{ColorKind::Ansi16, static_cast<std::uint8_t>(index & 0x0Fu), Rgb{}};
    }
    static constexpr Color indexed(std::uint8_t index) noexcept {
        return Color{ColorKind::Indexed, index, Rgb{}};
    }
    static constexpr Color rgb(Rgb value) noexcept { return Color{ColorKind::Rgb, 0, value}; }
    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
        return Color{ColorKind::Rgb, 0, Rgb{r, g, b}};
    }

    [[nodiscard]] constexpr ColorKind kind() const noexcept { return kind_; }
    [[nodiscard]] constexpr bool is_none() const noexcept { return kind_ == ColorKind::None; }

    // Ansi16 / Indexed 的索引值；其它 kind 返回 0。
    [[nodiscard]] constexpr std::uint8_t index() const noexcept { return index_; }
    [[nodiscard]] constexpr Rgb rgb() const noexcept { return rgb_; }

    TERMCHART_API bool operator==(const Color& other) const noexcept;
    TERMCHART_API bool operator!=(const Color& other) const noexcept;

private:
    constexpr Color(ColorKind kind, std::uint8_t index, Rgb value) noexcept
        : rgb_(value), index_(index), kind_(kind) {}

    Rgb rgb_{};
    std::uint8_t index_ = 0;
    ColorKind kind_ = ColorKind::None;
};

class Style {
public:
    constexpr Style() noexcept = default;

    [[nodiscard]] static constexpr Style plain() noexcept { return Style{}; }

    constexpr Style& set_attr(Attr attr, bool on = true) noexcept {
        attrs_ = on ? (attrs_ | mask_of(attr)) : without(attrs_, attr);
        return *this;
    }
    constexpr Style& set_fg(Color color) noexcept {
        fg_ = color;
        return *this;
    }
    constexpr Style& set_bg(Color color) noexcept {
        bg_ = color;
        return *this;
    }

    [[nodiscard]] constexpr AttrMask attrs() const noexcept { return attrs_; }
    [[nodiscard]] constexpr Color fg() const noexcept { return fg_; }
    [[nodiscard]] constexpr Color bg() const noexcept { return bg_; }
    [[nodiscard]] constexpr bool has_attr(Attr attr) const noexcept {
        return has_flag(attrs_, attr);
    }

    TERMCHART_API bool operator==(const Style& other) const noexcept;
    TERMCHART_API bool operator!=(const Style& other) const noexcept;

private:
    Color fg_{};
    Color bg_{};
    AttrMask attrs_ = kNoAttrs;
};

// 常用语义色：给 ui 组件默认样式用，避免到处魔数。
inline constexpr Rgb kDefaultFg{204, 204, 204};
inline constexpr Rgb kDefaultBg{0, 0, 0};

}  // namespace termchart

// ===== include/termchart/core/cell.hpp =====
// cell.hpp —— 终端字符单元
//
// 关键约定（破坏它等于破坏整个宽度模型）：
//   * Cell 只存**单个主码点**；组合记号绝不进 Cell。
//   * 组合序列（e.g. 'e' + U+0301）由 CellBuffer 的稀疏侧面表承载（side bearing），
//     数据结构定义在 core/grapheme.hpp，分割在 util/grapheme.hpp。
//   * 于是 Cell 是 trivially copyable：差分遍历时可 memcpy/memcmp。



namespace termchart {

struct Cell {
    char32_t cp = U' ';
    Style style;

    constexpr Cell() noexcept = default;
    constexpr explicit Cell(char32_t codepoint) noexcept : cp(codepoint) {}
    constexpr Cell(char32_t codepoint, Style s) noexcept : cp(codepoint), style(s) {}
};

TERMCHART_API bool operator==(const Cell& lhs, const Cell& rhs) noexcept;
TERMCHART_API bool operator!=(const Cell& lhs, const Cell& rhs) noexcept;

// 宽字符（显示宽 2）占据两个网格单元：第一格放主码点，第二格放续接占位。
// 续接格**不产生任何输出字节**——终端绘制宽字形时自动占两列，
// 再补一个空格会覆盖字形右半边。生产者（api/quick、canvas）负责写续接格，
// 渲染器（render/renderer）负责跳过它。
inline constexpr char32_t kContinuationCp = U'\0';

constexpr bool is_continuation(const Cell& cell) noexcept {
    return cell.cp == kContinuationCp;
}

// 是否为"空白且无样式"：差分渲染时用于跳过。
TERMCHART_API bool is_blank(const Cell& cell) noexcept;

}  // namespace termchart

// ===== include/termchart/core/buffer.hpp =====
// buffer.hpp —— CellBuffer：连续网格 + 稀疏侧面表
//
// 全库的**中枢结构**（L0）：
//   * 终端实时路径：canvas to_buffer() → CellBuffer → render/renderer → 字节流；
//   * snapshot 导出：backend/snapshot/* 直接吃 CellBuffer；
//   * 差分开销由 prev/cur 两个 CellBuffer 逐行为单位比较（见 render/diff.hpp）。
//
// 侧面表是**稀疏**的（unordered_map<index, Sides>）：绝大多数单元没有组合记号，
// 逐单元存一个数组会让 CellBuffer 尺寸直接翻倍。
//
// 注意：CellBuffer 持有堆资源，**禁止放进 arena**（见 core/arena.hpp）。

#include <unordered_map>


namespace termchart {

class CellBuffer {
public:
    // 稀疏侧面表的值类型：定长内联数组，避免侧面项逐个堆分配。
    using SideBearingList = std::array<char32_t, kMaxSideBearings>;

    CellBuffer() = default;
    TERMCHART_API CellBuffer(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();  // 全部单元置空白，清空侧面表

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return cells_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return cells_.empty(); }

    [[nodiscard]] constexpr bool contains(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < width_ && y < height_;
    }

    // 越界即 UB（Debug 下断言，Release 下不付代价）。调用方该先问 contains()。
    TERMCHART_API Cell& at(std::uint32_t x, std::uint32_t y) noexcept;
    TERMCHART_API const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept;

    // 可失败的取值：越界返回 nullptr，而不是 UB。热路径上先 contains() 再 at()，
    // 冷路径（导后端的边界扫描等）用这个。
    [[nodiscard]] TERMCHART_API Cell* try_at(std::uint32_t x, std::uint32_t y) noexcept;
    [[nodiscard]] TERMCHART_API const Cell* try_at(std::uint32_t x,
                                                   std::uint32_t y) const noexcept;

    // 线性索引版本：热路径避免乘法。（索引 = y * width + x）
    [[nodiscard]] constexpr std::size_t index_of(std::uint32_t x,
                                                 std::uint32_t y) const noexcept {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    // 对外暴露 span：调用方不得保存跨越 resize 的 span。
    TERMCHART_API std::span<Cell> row(std::uint32_t y) noexcept;
    TERMCHART_API std::span<const Cell> row(std::uint32_t y) const noexcept;
    TERMCHART_API std::span<Cell> cells() noexcept;
    TERMCHART_API std::span<const Cell> cells() const noexcept;

    // ── 稀疏侧面表 ──────────────────────────────────────────────────────
    [[nodiscard]] TERMCHART_API bool has_sides(std::size_t index) const noexcept;
    [[nodiscard]] TERMCHART_API const SideBearingList* sides(std::size_t index) const noexcept;
    // count 为实际有效项数（≤ kMaxSideBearings）；超出部分被截尾。
    // 返回是否写入：index 越界或 count == 0 时不写入并返回 false
    // （旧版静默丢弃，调用方无从分辨"写成功了"与"被丢了"）。
    TERMCHART_API bool set_sides(std::size_t index, const char32_t* data, std::size_t count);
    TERMCHART_API void clear_sides(std::size_t index);
    [[nodiscard]] TERMCHART_API std::size_t side_entry_count() const noexcept;

    TERMCHART_API bool operator==(const CellBuffer& other) const noexcept;
    TERMCHART_API bool operator!=(const CellBuffer& other) const noexcept;

private:
    std::vector<Cell> cells_;
    std::unordered_map<std::size_t, SideBearingList> sides_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

}  // namespace termchart

// ===== include/termchart/util/assert.hpp =====
// assert.hpp —— 断言宏（header-only）
//
// 错误处理的第二档（另一档是 core/error.hpp 的 Result）：
//   * TERMCHART_ASSERT       —— Release 下也生效（契约破坏必须早失败）；
//   * TERMCHART_DEBUG_ASSERT —— 仅 NDEBUG 未定义时生效（昂贵的不变量检查）；
//   * 渲染热路径**不要**用它做常规校验：那里用钳位或 Result。
//
// 失败时直接 abort 而非抛异常：不动 ODR/ABI，也不让调用方以为"能 catch 回来"。

#include <cstdio>
#include <cstdlib>

namespace termchart::util {

// 放在头文件的原因：希望每个断言点都能被内联与常量折叠。
// noexcept(false) 未加：断言失败就是要带走进程。
inline void assertion_failed(const char* expr, const char* file, int line,
                             const char* message) noexcept {
    if (message != nullptr) {
        std::fprintf(stderr, "termchart: assertion failed: %s (%s), file %s, line %d\n", expr,
                     message, file, line);
    } else {
        std::fprintf(stderr, "termchart: assertion failed: %s, file %s, line %d\n", expr, file,
                     line);
    }
    std::fflush(stderr);
    std::abort();
}

}  // namespace termchart::util

#define TERMCHART_ASSERT(expr)                                                    \
    do {                                                                          \
        if (!(expr)) {                                                            \
            ::termchart::util::assertion_failed(#expr, __FILE__, __LINE__, nullptr); \
        }                                                                         \
    } while (false)

#define TERMCHART_ASSERT_MSG(expr, msg)                                            \
    do {                                                                           \
        if (!(expr)) {                                                             \
            ::termchart::util::assertion_failed(#expr, __FILE__, __LINE__, (msg)); \
        }                                                                          \
    } while (false)

#ifdef NDEBUG
#define TERMCHART_DEBUG_ASSERT(expr) \
    do {                             \
    } while (false)
#define TERMCHART_DEBUG_ASSERT_MSG(expr, msg) \
    do {                                      \
    } while (false)
#else
#define TERMCHART_DEBUG_ASSERT(expr) TERMCHART_ASSERT(expr)
#define TERMCHART_DEBUG_ASSERT_MSG(expr, msg) TERMCHART_ASSERT_MSG(expr, msg)
#endif

// 逻辑上不可达，但编译器不知道（例如 switch 全覆盖）。
#define TERMCHART_UNREACHABLE()                                                     \
    do {                                                                            \
        ::termchart::util::assertion_failed("unreachable", __FILE__, __LINE__, nullptr); \
    } while (false)

// ===== include/termchart/util/base64.hpp =====
// base64.hpp —— 标准 base64（RFC 4648，含 '=' 填充）
//
// 本文件是**规格文件清单之外的补充**，理由：OSC 52 剪贴板（term/，L2）与
// kitty / iterm2 图像协议（protocol/，L9）都需要 base64，而 term 在 protocol 之下，
// 不能反向依赖。把唯一实现放在最底层 util/，所有需要方都调这里，避免出现第二份
// 实现后各自漂移（protocol 里原先那份内联实现已改调本文件）。
//
// 纯函数、无状态、无 I/O；列入 header-only 清单。

#include <string>

namespace termchart::util {

// 编码：标准字母表，长度不是 3 的倍数时补 '='。空输入返回空串。
[[nodiscard]] inline std::string base64_encode(const std::uint8_t* data, std::size_t len) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += kTable[v & 0x3F];
    }
    const std::size_t rest = len - i;
    if (rest == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += "==";
    } else if (rest == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

[[nodiscard]] inline std::string base64_encode(std::string_view text) {
    return base64_encode(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

// 解码：忽略空白（终端回传的 OSC 52 常带换行），遇到 '=' 即结束。
// 字母表之外的字符视为非法，返回 false 并保持 out 内容不变。
[[nodiscard]] inline bool base64_decode(std::string_view text, std::string& out) {
    auto value_of = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') {
            return ch - 'A';
        }
        if (ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 26;
        }
        if (ch >= '0' && ch <= '9') {
            return ch - '0' + 52;
        }
        if (ch == '+') {
            return 62;
        }
        if (ch == '/') {
            return 63;
        }
        return -1;
    };

    std::string decoded;
    decoded.reserve(text.size() / 4 * 3);
    std::uint32_t accum = 0;
    int bits = 0;
    for (const char ch : text) {
        if (ch == '=') {
            break;  // 填充只出现在末尾
        }
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
            continue;
        }
        const int value = value_of(ch);
        if (value < 0) {
            return false;
        }
        accum = (accum << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded.push_back(static_cast<char>((accum >> bits) & 0xFFu));
        }
    }
    out = std::move(decoded);
    return true;
}

}  // namespace termchart::util

// ===== include/termchart/util/bits.hpp =====
// bits.hpp —— 位操作（header-only）
//
// 光栅器（raster/*）与制表符绘制大量依赖位掩码：这里是**唯一**的位操作归口，
// 避免各处手写 `1 << (n - 1)` 而错位。

#include <bit>

namespace termchart::util {

// n ∈ [0, 31]，越界返回 0：宁可"不点亮"，也不要 UB。
constexpr std::uint32_t bit32(int n) noexcept { return (n >= 0 && n < 32) ? (1u << n) : 0u; }

template <class T>
constexpr T value_bit(int n) noexcept {
    return static_cast<T>(bit32(n));
}

template <class T>
constexpr int popcount(T v) noexcept {
    using U = std::make_unsigned_t<T>;
    return std::popcount(static_cast<U>(v));
}

template <class T>
constexpr bool test_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return (static_cast<U>(v) & static_cast<U>(value_bit<U>(n))) != static_cast<U>(0);
}

template <class T>
constexpr T set_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return static_cast<T>(static_cast<U>(static_cast<U>(v) | static_cast<U>(value_bit<U>(n))));
}

template <class T>
constexpr T clear_bit(T v, int n) noexcept {
    using U = std::make_unsigned_t<T>;
    return static_cast<T>(static_cast<U>(static_cast<U>(v) & ~static_cast<U>(value_bit<U>(n))));
}

// 宽度 width 的低位掩码，例如 low_mask(3) == 0b111。
constexpr std::uint32_t low_mask(int width) noexcept {
    return width <= 0 ? 0u : (width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u));
}

// 循环左移：六位打包（sixel）与盲文点阵需要。
constexpr std::uint8_t rotl8(std::uint8_t v, int n) noexcept {
    const int shift = n & 7;
    if (shift == 0) {
        return v;
    }
    return static_cast<std::uint8_t>((v << shift) | (v >> (8 - shift)));
}

}  // namespace termchart::util

// ===== include/termchart/util/data/aggregate.hpp =====
// util/data/aggregate.hpp —— 聚合（group by + reducer）
//
// 设计目标：pandas DataFrame.groupby().agg() 的"够用版"，零依赖。
// 数据模型：`DataView` 是对外部 std::vector<double> 的非持有视图（指针+长度），
// 调用方负责生命周期；多列对齐靠 `Columns` —— 一组同长度 DataView，
// "分组键"由第一列决定（或显式指定 key column）。
//
// 用途：等宽直方图、按类目求和、按时间桶聚合、箱线图四分位数（min/25/50/75/max）。
//
// 约束：单线程；不抛异常；空输入返回空结果。




namespace termchart::util::data {

struct DataView {
    const double* data = nullptr;
    std::size_t   size = 0;
};

enum class Reducer : std::uint8_t {
    Sum, Mean, Min, Max, Count, First, Last, Median, Stddev,
};

[[nodiscard]] TERMCHART_API const char* reducer_name(Reducer r) noexcept;

struct Group {
    double                key = 0.0;       // 分组键（NaN 不参与聚合）
    std::vector<DataView> values_per_col;  // 每列在原数组中的"段"
};

// 基础聚合：单列按 key 分组，对 value 列施加 reducer。
// `key_view` 与 `value_view` 必须同长度；NaN key 跳过。
[[nodiscard]] TERMCHART_API std::vector<Group>
group_reduce(DataView key_view, std::vector<DataView> value_views, Reducer reducer);

// 多 reducer：同组输出多个聚合结果。
struct MultiReduceResult {
    std::vector<Group>   groups;   // 同一组只出现一次（按 key 升序）
    std::vector<Reducer> reducers; // 与 groups[i].values_per_col 对应
};
[[nodiscard]] TERMCHART_API MultiReduceResult
group_reduce_multi(DataView key_view,
                   std::vector<DataView> value_views,
                   std::vector<Reducer> reducers);

// 直方图：等宽 bin，bin 数由调用方给，返回每桶计数。
struct HistogramBin {
    double lo = 0.0;     // 包含
    double hi = 0.0;     // 不包含（最后一桶包含 hi）
    std::size_t count = 0;
};
[[nodiscard]] TERMCHART_API std::vector<HistogramBin>
histogram(DataView view, std::size_t num_bins);

// 四分位数（min / Q1 / median / Q3 / max）。NaN 跳过；偶数样本中位数为两均值。
struct Quartiles {
    double minimum = 0.0;
    double q1 = 0.0;
    double median = 0.0;
    double q3 = 0.0;
    double maximum = 0.0;
};
[[nodiscard]] TERMCHART_API Quartiles quartiles(DataView view);

}  // namespace termchart::util::data

// ===== include/termchart/util/data/fill_missing.hpp =====
// util/data/fill_missing.hpp —— 缺失值处理（NaN 视为缺失）
//
// NaN 判定：std::isnan(value) —— IEEE 754 静默 NaN / 信号 NaN 都算缺失。
// 三种策略：
//   ForwardFill   ：沿用前一个非缺失值（适合时间序列）
//   LinearInterp  ：相邻非缺失值线性插值（端点外用前/后端值延拓）
//   Drop          ：返回新 vector 仅含非缺失值
//
// 输出：就地把目标 vector 改写（Fill 类）或返回新 vector（Drop）。



namespace termchart::util::data {

enum class FillStrategy : std::uint8_t {
    ForwardFill,
    LinearInterp,
    Drop,
};

[[nodiscard]] TERMCHART_API const char* fill_strategy_name(FillStrategy s) noexcept;

// 改写入参；全 NaN 全部保持 NaN（ForwardFill/LinearInterp 都这样）。
[[nodiscard]] TERMCHART_API std::size_t
fill_missing(std::vector<double>& values, FillStrategy strategy) noexcept;

// Drop：返回新 vector（仅非缺失值）。
[[nodiscard]] TERMCHART_API std::vector<double>
drop_missing(const std::vector<double>& values);

// 计数：返回 NaN 个数。
[[nodiscard]] TERMCHART_API std::size_t
count_missing(const std::vector<double>& values) noexcept;

}  // namespace termchart::util::data

// ===== include/termchart/util/data/resample.hpp =====
// util/data/resample.hpp —— 重采样（按规则桶聚合）
//
// 用途：把不规则间隔的时间序列按"每 N 个一桶"或"按显式桶边界"降采样，
// 输出每桶的聚合值（mean / sum / min / max / first / last）。
//
// 与 `aggregate::histogram` 的差异：resample 不要求等宽，
// 桶边界由 `BinEdges` 显式给出（等宽也只是其中一种生成方式）。



namespace termchart::util::data {

// 等宽桶：lo..hi 切成 n 桶（最后一桶包含 hi）。
[[nodiscard]] TERMCHART_API std::vector<double>
linspace_edges(double lo, double hi, std::size_t n);

// 重采样：把 values 按 edges 切桶，每桶施加 reducer。
// edges 必须升序且至少 2 个元素；values 越界的不进任何桶。
struct ResampleBin {
    double      lo = 0.0;       // 包含
    double      hi = 0.0;       // 不包含（最后一桶包含 hi）
    double      value = 0.0;    // 该桶聚合结果（NaN 表示空桶）
    std::size_t count = 0;      // 该桶样本数
};
[[nodiscard]] TERMCHART_API std::vector<ResampleBin>
resample(const std::vector<double>& values, const std::vector<double>& edges,
         Reducer reducer);

}  // namespace termchart::util::data

// ===== include/termchart/util/data/rolling.hpp =====
// util/data/rolling.hpp —— 滚动窗口聚合
//
// 滚动统计（pandas Series.rolling(window).mean() 的最小集）。
// 中心约定：
//   * 窗口长度 = window，**右对齐**：第 i 个输出由 [i-window+1, i] 决定；
//     前 i < window-1 个输出按 partial = true 处理（NaN 或 min_periods 触发）。
//   * min_periods：窗口内最少非缺失值，少于则输出 NaN。
//   * 支持：mean / sum / stddev / min / max / median。
//   * 不抛异常；空 vector 返回空 vector。



namespace termchart::util::data {

enum class RollingStat : std::uint8_t {
    Mean, Sum, Stddev, Min, Max, Median,
};

[[nodiscard]] TERMCHART_API const char* rolling_stat_name(RollingStat s) noexcept;

struct RollingOptions {
    std::size_t window = 3;
    std::size_t min_periods = 0;       // 0 = 与 window 同；NaN 跳过
    bool        right_aligned = true;  // false = 中心（左右各半）
};

[[nodiscard]] TERMCHART_API std::vector<double>
rolling(const std::vector<double>& values, RollingStat stat,
        const RollingOptions& options = {}) noexcept;

}  // namespace termchart::util::data

// ===== include/termchart/util/format.hpp =====
// format.hpp —— 轻量整数 / 浮点格式化
//
// 为什么不用 iostream / std::format：
//   * iostream 受 locale 影响，同一份程序在不同机器上输出不同（golden 测试会炸）；
//   * std::format 依赖较新的标准库且体积大；项目只要"固定小数位 + 去尾零"两种。
// 因此这里用 <charconv> 的 to_chars：locale 无关、无分配、最快。



namespace termchart::util {

// 定点表示，precision 位小数；NaN/Inf 输出 "nan"/"inf"。
TERMCHART_API std::string format_double(double value, int precision = 2);

// 同上但去掉尾部多余的 0 与小数点：0.50 → "0.5"，1.000 → "1"。
TERMCHART_API std::string format_double_trimmed(double value, int precision = 2);

// 追加到已有缓冲：渲染热路径避免临时字符串。
TERMCHART_API void append_double(std::string& out, double value, int precision = 2);

// 千分位由调用方自行拼装；这里只提供无 locale 依赖的整数输出。
TERMCHART_API std::string format_int(std::int64_t value);

// 轴标签常用：2000 → "2k"，避免长数字挤占坐标空间。
TERMCHART_API std::string format_compact(double value, int precision = 1);

}  // namespace termchart::util

// ===== include/termchart/util/geo/geojson.hpp =====
// util/geo/geojson.hpp —— GeoJSON 解析（最小集，仅 FeatureCollection）
//
// GeoJSON 规范参考 RFC 7946。本文件只覆盖：
//   * FeatureCollection → 一组 Polygon / MultiPolygon（边界数据）；
//   * 每个 Feature 的 `properties` 全部拷成 std::string → std::string；
//   * `geometry.type == "Polygon" | "MultiPolygon"`（其他类型抛错并返回 false）；
//   * 坐标数组 [lon, lat] → Point（高度忽略）。
//
// 用途：把世界地图/中国省份/主要城市的边界从 assets/maps/*.json 读进 termchart，
// 再走 util::geo::projection 投影到画布坐标系。
//
// 故意不写：
//   * GeometryCollection（用得少，Poly/MultiPoly 已覆盖 ≥99% 边界数据）；
//   * 弧线（RFC 7946 §3.1.6 弧线段，termchart 用直线段足够）；
//   * 跨子午线环（antimeridian cutting）—— caller 自己预处理。
//
// 错误：解析失败返回 false，错误信息通过 out_error 输出。



namespace termchart::util::geo {

struct GeoPolygon {
    std::vector<std::vector<Point>> rings;  // 首环外环，其余为内环（孔）
    std::vector<std::pair<std::string, std::string>> properties;
};

struct GeoFeatureCollection {
    std::vector<GeoPolygon> polygons;
};

// 从字符串解析。空字符串或缺 type/feature 字段返回 false。
[[nodiscard]] TERMCHART_API bool parse_geojson(const std::string& text,
                                                GeoFeatureCollection& out,
                                                std::string& out_error) noexcept;

// 从文件中读 + 解析（薄包装）。
[[nodiscard]] TERMCHART_API bool parse_geojson_file(const std::string& path,
                                                     GeoFeatureCollection& out,
                                                     std::string& out_error) noexcept;

// 经纬度范围（粗略 bbox，用于投影前估算画布 / 选投影）。
struct GeoBounds {
    double min_lat = 0.0, max_lat = 0.0;
    double min_lon = 0.0, max_lon = 0.0;
};
[[nodiscard]] TERMCHART_API GeoBounds
compute_bounds(const GeoFeatureCollection& fc) noexcept;

}  // namespace termchart::util::geo

// ===== include/termchart/util/geo/projection.hpp =====
// util/geo/projection.hpp —— 经纬度 → 平面投影
//
// 极简集：4 种最常用投影，纯函数、零依赖、终端坐标友好（aspect 由 caller 自定）。
// 输入：纬度（-90..90）+ 经度（-180..180）→ 输出：平面 (x, y)（单位无定义，
//       调用方决定 1 单位 = 多少字符格 / 多少像素）。
//
// 约定：y 向上为正（北向上）；调用方需要"y 向下"时自行取负。
//
// 边界：极点附近 Mercator / Equirectangular 输出趋于无穷，由 caller 钳制。


namespace termchart::util::geo {

// 等距矩形（plate carrée）：最简单，lat → y, lon → x 直接线性。
// 适合：低分辨率地图、全局数据预览。
[[nodiscard]] TERMCHART_API Point equirectangular(double lat_deg, double lon_deg) noexcept;

// 墨卡托（Web Mercator）：google/高德/必应地图的标准投影。
// 公式：x = lon, y = ln(tan(π/4 + lat/2))
// 在 ±85.05113° 之外返回极大值（Mercator 极点奇点）。
[[nodiscard]] TERMCHART_API Point web_mercator(double lat_deg, double lon_deg) noexcept;

// 正射投影（orthographic）：从无穷远看地球的视点，圆形地图。
// 半球内有效；lat/lon 超出可见半球则返回 (NaN, NaN) 哨兵（由 caller 判定）。
[[nodiscard]] TERMCHART_API Point orthographic(double lat_deg, double lon_deg,
                                               double center_lat_deg,
                                               double center_lon_deg) noexcept;

// 兰勃特等面积圆柱投影：保持面积相等，世界地图常用。
// y = sin(lat), x = lon * cos(lat_mean)。
[[nodiscard]] TERMCHART_API Point lambert_cylindrical(double lat_deg, double lon_deg,
                                                       double reference_lat_deg = 0.0) noexcept;

}  // namespace termchart::util::geo

// ===== include/termchart/util/geom.hpp =====
// geom.hpp —— 平面几何：点线距离、线段关系、多边形面积/重心/凸包
//
// 只依赖 core（Point），纯函数、无状态、无 I/O。`graph/crossing` 的严格相交判定
// 与这里是同一套规则，图模块直接复用本文件，不再各写一份。



namespace termchart::util {

// 叉积的 z 分量：cross(a, b, c) = (b - a) × (c - a)。
// > 0 表示 a→b→c 左转（逆时针），< 0 右转，== 0 三点共线。
[[nodiscard]] TERMCHART_API double cross(Point a, Point b, Point c) noexcept;

// 点到**无限直线** ab 的带符号距离：> 0 表示 p 在 a→b 的左侧。
// a == b 时返回 p 到该点的距离（恒为非负）。
[[nodiscard]] TERMCHART_API double signed_distance_to_line(Point p, Point a, Point b) noexcept;

// 点到**线段** ab 的最近距离（线段退化为点时即两点距离）。
[[nodiscard]] TERMCHART_API double distance_to_segment(Point p, Point a, Point b) noexcept;

// 线段 ab 与 cd 是否**真相交**：共端点、共线重叠、端点落在另一条线段上都**不算**。
// 这正是"连线交叉数"需要的口径（贴着端点绕过去不算交叉）。
[[nodiscard]] TERMCHART_API bool segments_cross(Point a, Point b, Point c, Point d) noexcept;

// 两条线段所在直线的交点：仅在唯一交点落在两条线段之内（含端点）时返回 true。
// 平行、共线、退化线段一律返回 false。
[[nodiscard]] TERMCHART_API bool segment_intersection(Point a, Point b, Point c, Point d,
                                                      Point& out) noexcept;

// 多边形面积（鞋带公式）。顶点按给定顺序；逆时针为正，顺时针为负。
// count < 3 返回 0。
[[nodiscard]] TERMCHART_API double polygon_area(const Point* points,
                                                std::size_t count) noexcept;

// 多边形重心（面积加权）。面积为 0 时退化为顶点算术平均。
[[nodiscard]] TERMCHART_API Point polygon_centroid(const Point* points,
                                                   std::size_t count) noexcept;

// 点是否落在多边形内部（射线法，奇偶规则）。边界上的点视为**在内部**。
[[nodiscard]] TERMCHART_API bool point_in_polygon(Point p, const Point* points,
                                                  std::size_t count) noexcept;

// 凸包（Andrew 单调链）。返回**逆时针**顶点序列，起点为最左下的点；
// 同一条边上的中间共线点会被剔除。点数 < 3 时原样返回（去重后）。
[[nodiscard]] TERMCHART_API std::vector<Point> convex_hull(std::vector<Point> points);

}  // namespace termchart::util

// ===== include/termchart/util/grapheme.hpp =====
// grapheme.hpp —— grapheme cluster 边界检测（UAX #29 简化版）
//
// 这里是**唯一做分割**的地方（core/grapheme.hpp 只定义数据结构）。
// 属性区间表来自 detail/grapheme_tables.hpp（层号 -1，任何层可用）。
//
// 实现的规则集：GB3（CR×LF）、GB4/GB5（控制符自成簇）、GB6–GB8（Hangul）、
// GB9（×Extend/ZWJ）、GB9a（×SpacingMark）、GB9b（Prepend×）、
// GB11（ZWJ×扩展象形）、GB12/GB13（地区旗标成对连缀）。
// 未实现 emoji 序列的全部细则（UAX #51 的杂项键帽等）——见 ROADMAP 阶段 7。



namespace termchart::util {

// 从 pos 走到下一个簇的边界（返回增量；0 表示已到末尾）。
TERMCHART_API std::size_t next_boundary(std::string_view text, std::size_t pos) noexcept;

// 簇计数：文本 = 多少个"用户感知字符"。
TERMCHART_API std::size_t cluster_count(std::string_view text) noexcept;

// 全量分割。热路径请用 next_boundary 流式遍历，避免中间容器。
TERMCHART_API std::vector<GraphemeCluster> split_clusters(std::string_view text);

// 是否为"扩展象形"（emoji 粗判）：GB11 需要它。
TERMCHART_API bool is_extended_pictographic(char32_t cp) noexcept;

}  // namespace termchart::util

// ===== include/termchart/util/logging.hpp =====
// logging.hpp —— 分级日志，编译期可关
//
// 两级开关：
//   * 运行期 set_log_level()：默认 Info；
//   * 编译期 TERMCHART_MAX_LOG_LEVEL（0..5），高于它的 TERMCHART_LOG_* 直接编译消失，
//     保证 Release 里日志分支零成本。
// sink 可替换：测试用 capture_log() 抓取输出，不写 stderr。



#ifndef TERMCHART_MAX_LOG_LEVEL
#define TERMCHART_MAX_LOG_LEVEL 3  // Info
#endif

namespace termchart::util {

enum class LogLevel : std::uint8_t {
    Off = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
};

TERMCHART_API void set_log_level(LogLevel level) noexcept;
TERMCHART_API LogLevel log_level() noexcept;
TERMCHART_API const char* level_name(LogLevel level) noexcept;

TERMCHART_API void write_log(LogLevel level, std::string_view message) noexcept;

// 替换输出目标（测试/嵌入式）。设为 nullptr 恢复默认（stderr）。
TERMCHART_API void set_log_sink(std::function<void(LogLevel, std::string_view)> sink);

// 测试辅助：接管 sink 并记录全部条目；返回即式 RAII 由调用方手动 flush_log_capture()。
TERMCHART_API void capture_log_begin() noexcept;
TERMCHART_API std::vector<std::string> capture_log_end() noexcept;

}  // namespace termchart::util

#define TERMCHART_LOG_AT(level_enum, msg)                                              \
    do {                                                                               \
        using ::termchart::util::LogLevel;                                             \
        constexpr int kLevelValue = static_cast<int>(level_enum);                      \
        if constexpr (kLevelValue <= TERMCHART_MAX_LOG_LEVEL) {                        \
            if (kLevelValue <= static_cast<int>(::termchart::util::log_level())) {     \
                ::termchart::util::write_log(level_enum, (msg));                       \
            }                                                                          \
        }                                                                              \
    } while (false)

#define TERMCHART_LOG_ERROR(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Error, msg)
#define TERMCHART_LOG_WARN(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Warn, msg)
#define TERMCHART_LOG_INFO(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Info, msg)
#define TERMCHART_LOG_DEBUG(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Debug, msg)
#define TERMCHART_LOG_TRACE(msg) TERMCHART_LOG_AT(::termchart::util::LogLevel::Trace, msg)

// ===== include/termchart/util/math.hpp =====
// math.hpp —— 标量数学（header-only）
//
// 只提供终端绘图真正需要的那几个函数；依赖 <cmath> 但不引入 eigen/boost。
// 所有函数 constexpr / noexcept：布局与缩放是在热路径上跑的。

#include <cmath>
#include <limits>

namespace termchart::util {

template <class T>
constexpr T clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

template <class T>
constexpr T lerp(T a, T b, T t) noexcept {
    return a + (b - a) * t;
}

// t 钳位到 [0,1] 后的 lerp：绘图 APIs 的边界输入极其常见，默认密封。
template <class T>
constexpr T clamp_lerp(T a, T b, T t) noexcept {
    return lerp<T>(a, b, clamp<T>(t, T{0}, T{1}));
}

// 归一：把 v 从 [lo,hi] 映射到 [0,1]；退化的区间（lo == hi）返回 0，不是 NaN。
constexpr double normalize(double v, double lo, double hi) noexcept {
    const double span = hi - lo;
    if (span == 0.0) {
        return 0.0;
    }
    return (v - lo) / span;
}

// 平滑过渡：比线性少一些"折点感"，用于动画缓动与渐变采样。
constexpr double smoothstep(double t) noexcept {
    const double x = clamp<double>(t, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

constexpr double kPi = 3.14159265358979323846;
constexpr double deg_to_rad(double deg) noexcept { return deg * kPi / 180.0; }
constexpr double rad_to_deg(double rad) noexcept { return rad * 180.0 / kPi; }

template <class T>
constexpr int sign(T v) noexcept {
    return (v > T{0}) - (v < T{0});
}

// 浮点比较：布局代码里到处是 " == 0 "，用这个而不是裸比较。
template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr bool nearly_equal(T a, T b, T eps = T{1e-9}) noexcept {
    const T diff = a > b ? a - b : b - a;
    return diff <= eps;
}

// NaN/Inf 防御：数据来自用户 CSV 时，一个 NaN 会让整张图消失。
template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
constexpr bool is_finite(T v) noexcept {
    return v == v && v != std::numeric_limits<T>::infinity() && v != -std::numeric_limits<T>::infinity();
}

}  // namespace termchart::util

// ===== include/termchart/util/random.hpp =====
// random.hpp —— 可复现伪随机（header-only）
//
// 为什么自带：**可复现**是硬需求。散点抖动、力导向初始布局、用例 fixture
// 都要"同一个种子得到同一个结果"，std::random_device/mt19937 的默认初始化做不到。
// 算法：splitmix64（无状态争用、质量足够、实现 20 行）。


namespace termchart::util {

class Rng {
public:
    using result_type = std::uint64_t;

    constexpr explicit Rng(std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept : state_(seed) {}

    constexpr std::uint64_t next() noexcept {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    constexpr double unit() noexcept {  // [0, 1)
        return static_cast<double>(next() >> 11) * (1.0 / (1ull << 53));
    }

    constexpr double range(double lo, double hi) noexcept { return lo + (hi - lo) * unit(); }

    constexpr std::uint64_t integer(std::uint64_t bound) noexcept {
        return bound == 0 ? 0 : next() % bound;
    }

    static constexpr result_type min() noexcept { return 0; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

private:
    std::uint64_t state_;
};

}  // namespace termchart::util

// ===== include/termchart/util/task.hpp =====
// task.hpp —— 可取消任务句柄
//
// 用途：把"这件事还要不要继续做"从执行体里解耦出来。执行体只持有只读的
// `CancellationToken`（随时问一句 cancelled()），发起方持有 `TaskHandle`
// （可 cancel、可在收尾时 finish）。两者共享一份原子状态，因此 cancel 可以在
// 别的线程发生。
//
// 边界说明：这里**不建线程、不排队、不调度**——只提供取消语义。要定时请用
// util/scheduler，要线程请用 app/（L20）。



namespace termchart::util {

// 内部共享状态。实现细节，外部只通过下面两个句柄访问。
class TaskState;

// 只读观察端。
class CancellationToken {
public:
    CancellationToken() = default;

    [[nodiscard]] TERMCHART_API bool cancelled() const noexcept;
    [[nodiscard]] TERMCHART_API bool finished() const noexcept;
    // 是否绑定到一个真实任务。默认构造的空 token 恒为 false，便于把
    // "没有任务" 与 "有任务且未取消" 区分开。
    [[nodiscard]] TERMCHART_API bool valid() const noexcept;

private:
    friend class TaskHandle;
    explicit CancellationToken(std::shared_ptr<const TaskState> state);

    std::shared_ptr<const TaskState> state_;
};

// 控制端。
class TaskHandle {
public:
    // 构造即创建一份独立状态：不同句柄互不影响。
    TERMCHART_API TaskHandle();

    [[nodiscard]] TERMCHART_API CancellationToken token() const noexcept;
    TERMCHART_API void cancel() noexcept;
    [[nodiscard]] TERMCHART_API bool cancelled() const noexcept;
    // 执行体收尾时调用，便于发起方判断"是取消还是跑完了"。
    TERMCHART_API void finish() noexcept;
    [[nodiscard]] TERMCHART_API bool finished() const noexcept;

private:
    std::shared_ptr<TaskState> state_;
};

}  // namespace termchart::util

// ===== include/termchart/util/timer.hpp =====
// timer.hpp —— 单调时钟、秒表、帧计时、超时判断
//
// 全部基于 `std::chrono::steady_clock`：它单调递增，不受系统时间调整影响，
// 适合测耗时；而"墙上时间"应当用 system_clock，本模块不掺和。
//
// 可测试性：帧计时与超时都提供**注入时间点**的接口（tick_at / expired_at），
// 单测因此不依赖真实时间流逝（也不会因为机器慢而抖动）。

#include <chrono>


namespace termchart::util {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

// 进程内单调起点（首次调用时确定），用于打印相对时间。
[[nodiscard]] TERMCHART_API TimePoint monotonic_start() noexcept;

// 当前单调时刻。
[[nodiscard]] TERMCHART_API TimePoint now() noexcept;

// 距进程内单调起点的秒数（可用于日志时间戳）。
[[nodiscard]] TERMCHART_API double seconds_since_start() noexcept;

// 秒表：start / stop / reset，可反复读。
class Stopwatch {
public:
    Stopwatch() noexcept : start_(now()) {}

    void restart() noexcept;
    // 从上次 restart（或构造）到"现在"经过的毫秒 / 秒数。
    [[nodiscard]] double elapsed_ms() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;

private:
    TimePoint start_;
};

// 帧计时：给出相邻两帧的间隔（可选指数平滑）与 FPS。
class FrameTimer {
public:
    // smoothing ∈ [0,1)：0 表示不平滑（用最近一帧），越接近 1 越平滑。
    explicit FrameTimer(double smoothing = 0.9) noexcept;

    // 推进一帧，返回距上一帧的秒数；首帧返回 0。
    double tick() noexcept;
    // 用指定时间推进（测试用，避免依赖真实时钟）。
    double tick_at(TimePoint stamp) noexcept;

    [[nodiscard]] double delta_seconds() const noexcept { return delta_; }
    // delta 为 0 时返回 0（不返回无穷）。
    [[nodiscard]] double fps() const noexcept;
    [[nodiscard]] std::uint64_t frame_count() const noexcept { return frames_; }

    void reset() noexcept;

private:
    double smoothing_ = 0.9;
    double delta_ = 0.0;
    std::uint64_t frames_ = 0;
    TimePoint last_{};
    bool started_ = false;
};

// 超时判断：构造时给定阈值，之后用 expired_at 查询（或用 expired 走真实时钟）。
class Timeout {
public:
    explicit Timeout(Duration limit) noexcept : limit_(limit), start_(now()) {}

    void restart() noexcept;
    void restart_at(TimePoint stamp) noexcept;
    [[nodiscard]] bool expired_at(TimePoint stamp) const noexcept;
    [[nodiscard]] bool expired() const noexcept;
    [[nodiscard]] double remaining_seconds(TimePoint stamp) const noexcept;
    [[nodiscard]] Duration limit() const noexcept { return limit_; }

private:
    Duration limit_{};
    TimePoint start_{};
};

}  // namespace termchart::util

// ===== include/termchart/util/scheduler.hpp =====
// scheduler.hpp —— 单线程定时任务调度器
//
// 不建线程、不读时钟：**调用方每个 tick 传入"现在"**，调度器据此决定谁到期。
// 于是它是纯逻辑，可以确定性地测（配合 util/timer 的注入时间点），也不会出现
// "库内部偷偷起了个线程"这种事。线程与事件循环属于 app/（L20）。
//
// 周期任务采用**固定间隔、不补偿追赶**：每次触发后从当次时刻重排下一次。
// 长时间卡顿（例如调试器断住）之后不会一次补跑几十次——那会让界面雪崩。



namespace termchart::util {

class Scheduler {
public:
    using TaskId = std::uint64_t;
    using Callback = std::function<void()>;

    // 一次性任务：delay 之后触发一次。
    [[nodiscard]] TERMCHART_API TaskId after(Duration delay, Callback callback);
    // 周期任务：每 interval 触发一次（interval <= 0 视为每 tick 一次）。
    [[nodiscard]] TERMCHART_API TaskId every(Duration interval, Callback callback);

    // 显式基准版：以给定时刻算到期，**必须与 tick(stamp) 用同一个时间源**。
    // 测试与自定义时钟走这两个；上面两个便捷版内部用 now()，只该与 tick(now()) 配用。
    // （混用两个时间源会让到期时刻算错——这是本类曾经的设计缺陷，故在此显式区分。）
    [[nodiscard]] TERMCHART_API TaskId after_at(TimePoint stamp, Duration delay,
                                                Callback callback);
    [[nodiscard]] TERMCHART_API TaskId every_at(TimePoint stamp, Duration interval,
                                                Callback callback);

    // 取消任务；任务不存在（已触发过的一次性任务）返回 false。
    TERMCHART_API bool cancel(TaskId id);
    [[nodiscard]] TERMCHART_API bool contains(TaskId id) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t pending() const noexcept;
    // 最近一个到期时刻；没有任务时返回 nullopt。
    [[nodiscard]] TERMCHART_API std::optional<TimePoint> next_due() const;

    // 执行所有"到期时刻 <= stamp"的任务，返回执行次数（周期任务记一次）。
    // 回调中可以安全地登记新任务或取消任务：
    //   * 到期集合在开始前快照，但每次调用前会按 id 重新查表——被取消的不执行；
    //   * 本轮新登记的任务即使也到期，也留到下一次 tick（本轮不递归）。
    TERMCHART_API std::size_t tick(TimePoint stamp);

private:
    struct Task {
        TaskId id = 0;
        TimePoint due{};
        Duration interval{};
        bool periodic = false;
        Callback callback;
    };

    [[nodiscard]] std::vector<Task>::iterator find(TaskId id) noexcept;

    std::vector<Task> tasks_;
    TaskId next_id_ = 1;
};

}  // namespace termchart::util

// ===== include/termchart/util/utf8.hpp =====
// utf8.hpp —— UTF-8 编解码，对非法序列容错
//
// 容错策略（这是本文件的核心决策）：**永不抛异常、永不 UB**。
// 输入可能来自 CSV、来自管道、来自 fuzz（tests/fuzz/fuzz_utf8.cpp）：
//   * 非法序列 → 产出 U+FFFD，**只消耗 1 字节**后继续（保证游标一定前进）；
//   * 截断序列 → 同上；调用方可选地把 "consumed < 期望" 当作"数据还没到"。



namespace termchart::util {

inline constexpr char32_t kReplacementChar = 0xFFFD;
inline constexpr std::size_t kMaxUtf8Bytes = 4;

struct Utf8Decode {
    char32_t cp = kReplacementChar;   // 解码出的码点（失败时为 U+FFFD）
    int consumed = 0;                 // 消耗的字节数；0 表示 pos 已越界
    bool ok = false;                  // 是否为合法且完整的序列
    bool incomplete = false;          // 合法前缀但字节不够（流式场景用）
};

// 从 pos 处解码一个码点。pos 越界返回 consumed == 0。
TERMCHART_API Utf8Decode decode(std::string_view bytes, std::size_t pos) noexcept;

// 编码码点到 UTF-8，返回写入字节数（0 表示不可编码：代理码点或 > U+10FFFF）。
TERMCHART_API std::size_t encode(char32_t cp, char* out) noexcept;

TERMCHART_API void append(std::string& out, char32_t cp);
TERMCHART_API std::string to_utf8(char32_t cp);

// 码点数（非法序列按一个 U+FFFD 计）。
TERMCHART_API std::size_t codepoint_count(std::string_view text) noexcept;

// 严格校验：整个串是否为合法 UTF-8（解析 CSV 头、DSL 输入时先过一遍）。
TERMCHART_API bool is_valid(std::string_view text) noexcept;

// 显式报错版本：给"必须干净"的输入用。与上面的容错版并存，不互相替代。
TERMCHART_API Result<std::string> sanitize(std::string_view text) noexcept;

}  // namespace termchart::util

// ===== include/termchart/util/wcwidth.hpp =====
// wcwidth.hpp —— 显示宽度
//
// 宽度模型：**主码点决定宽度，组合记号 (side bearing) 宽 0**（详情见
// docs/design/utf8_handling.md）。因此宽度查询的输入可以是码点或
// GraphemeCluster，二者语义一致。emoji 宽度策略见 docs/design/emoji_width.md。



namespace termchart::util {

struct WidthOptions {
    // East Asian Ambiguous 是否按宽处理（CJK 区域终端常见需求）。
    bool ambiguous_wide = false;
    // emoji 是否按 2 宽处理；某些终端（含部分 Windows 控制台）只给 1 宽。
    bool emoji_wide = true;
};

// 单个码点宽度：0（组合/格式/控制）、1（窄）、2（东亚宽 / emoji）。
TERMCHART_API int codepoint_width(char32_t cp, const WidthOptions& options = {}) noexcept;

// 一个簇的宽度：等于主码点宽度（侧面宽 0）。
TERMCHART_API int cluster_width(const GraphemeCluster& cluster,
                                const WidthOptions& options = {}) noexcept;

// 整串宽度：以簇为单位累加；非法 UTF-8 的替换字符按宽度 1 计。
TERMCHART_API int display_width(std::string_view text, const WidthOptions& options = {}) noexcept;

// 按显示宽度截断：返回"最多个前缀码点"，使宽度不超过 budget。
// 工具核心：**绝不切在组合记号中间**（否则会出现孤立的组合记号，渲染为乱码）。
TERMCHART_API std::size_t prefix_by_width(std::string_view text, int budget,
                                          const WidthOptions& options = {}) noexcept;

}  // namespace termchart::util

// ===== include/termchart/util/text.hpp =====
// text.hpp —— 文本排版：截断、对齐、换行、填充
//
// 所有操作以**显示宽度**而非码点数为单位——这是终端排版与 Web 的根本差异之一。
// 因此全部依赖 util/wcwidth.hpp 与 util/grapheme.hpp。



namespace termchart::util {

enum class Align : std::uint8_t { Left, Center, Right };

// 重复字符 n 次：进度条、分隔线用。n == 0 返回空串。
TERMCHART_API std::string repeat(char32_t cp, std::size_t n);

// 宽度 ≤ max_width 的对齐填充。超宽时不截断（截断请用 truncate）。
TERMCHART_API std::string pad(std::string_view text, int width, Align align = Align::Left,
                              char32_t pad_char = U' ');

// 截断到 max_width；宽度不足时退化为省略符（默认 "..."）。
TERMCHART_API std::string truncate(std::string_view text, int max_width,
                                   Align align = Align::Left, std::string_view ellipsis = "...");

// 折行：贪心按簇边界断；超长单词（无空格）在 force_break 时硬切。
TERMCHART_API std::vector<std::string> wrap(std::string_view text, int width,
                                            bool force_break = true);

// 取显示宽度≤max_width 的前缀，并补齐到 max_width（标题栏、图例条常用）。
TERMCHART_API std::string fit(std::string_view text, int max_width, Align align = Align::Left);

}  // namespace termchart::util

// ===== include/termchart/color/colormap.hpp =====
// colormap.hpp —— 连续色表：viridis / inferno / magma / plasma / turbo
//
// 为什么用**多项式拟合**而不是数据文件：
//   * 避免把原始数据（CC0）打包进仓库，也避免需要在运行期加载 asset；
//   * 拟合的最大误差约 2/255，终端着色场景不可分辨；
//   * 于是 colormap 是纯函数，无状态、线程安全、无需初始化。
// 拟合系数来自公开的多项式拟合（Google, "Analytic Approximations for Colormaps"），
// 见 docs/reference/module_matrix.md 之外的说明；需要权威数据时用 utile/sample 表替换。



namespace termchart::color {

enum class ColormapKind : std::uint8_t {
    Viridis = 0,
    Inferno = 1,
    Magma = 2,
    Plasma = 3,
    Turbo = 4,
};

inline constexpr int kColormapCount = 5;

// t 会被钳到 [0,1]。真正的可见性/感知均匀性由色表本身保证，这里不插值采样不足。
TERMCHART_API Rgb sample(ColormapKind kind, double t) noexcept;

// 预采样成均匀色带（默认 256 级），缓存于进程内：热力图/色标每秒查几万次，
// 每次跑 7 次多项式太贵。首次调用构建，之后只读——**无锁**：单线程主循环前提成立。
TERMCHART_API std::span<const Rgb> stops(ColormapKind kind, std::size_t count = 256);

TERMCHART_API const char* colormap_name(ColormapKind kind) noexcept;

}  // namespace termchart::color

// ===== include/termchart/color/gradient.hpp =====
// gradient.hpp —— 渐变生成
//
// 线性多段渐变：给色阶条、热度标尺、流式明暗用；真·连续色表走 colormap.hpp。
// 这里是无状态值类型：构造后只读，可被 style/theme 复用。



namespace termchart::color {

class Gradient {
public:
    Gradient() = default;
    explicit Gradient(Rgb from, Rgb to) noexcept;

    // pos ∈ [0,1]；重复 pos 会覆盖；顺序无关（内部按 pos 排序）。
    Gradient& add_stop(double pos, Rgb color);
    Gradient& clear_stops() noexcept;

    [[nodiscard]] TERMCHART_API Rgb sample(double t) const noexcept;
    [[nodiscard]] TERMCHART_API std::vector<Rgb> generate(std::size_t count) const;
    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t stop_count() const noexcept;

private:
    struct Stop {
        double pos;
        Rgb color;
    };
    std::vector<Stop> stops_;
};

// 两色之间线性插值，t 钳位到 [0,1]（sRGB 空间，不做 gamma 校正）。
TERMCHART_API Rgb mix(Rgb from, Rgb to, double t) noexcept;

// 按感知亮度挑黑白之一作为前景，保证对比可读。
TERMCHART_API Rgb contrasting_text_color(Rgb background) noexcept;

}  // namespace termchart::color

// ===== include/termchart/color/named_colors.hpp =====
// named_colors.hpp —— CSS 颜色名查找表（header-only）
//
// 用途：让用户能写 "#2f6f4f" 之外的 "seagreen"，也让示例代码更可读。
// 表按名字升序存放，查询走二分——**因此顺序与内容是契约**：新增必须保持有序
// （重新生成见 tools/gen_colormap.py 的同名策略）。
//
// 当前为 **CSS Color 命名色全集 148 色**（含 gray/grey 两套拼法与 rebeccapurple，
// 即 CSS Color Module Level 4 的完整列表，不含 `transparent`/`currentcolor`
// 这两个关键字——它们不是颜色值，由 Color 侧单独处理）。
// 查不到的名字由 Color 侧回落到 "不是颜色名" 而非魔数。
//
// 契约由 `tests/unit/color/test_color.cpp` 守住：表必须严格升序、无重复、
// 数量等于 kNamedColorCount、抽查若干色值与 CSS 规范一致。



namespace termchart::color {

struct NamedColorEntry {
    std::string_view name;
    Rgb value;
};

// CSS 命名色全集的大小（148）。二分查找的上界与容量都取自它，不要让二者各写一份数字。
inline constexpr std::size_t kNamedColorCount = 148;

// clang-format off
inline constexpr std::array<NamedColorEntry, kNamedColorCount> kNamedColors{{
    {"aliceblue", Rgb{240, 248, 255}},
    {"antiquewhite", Rgb{250, 235, 215}},
    {"aqua", Rgb{0, 255, 255}},
    {"aquamarine", Rgb{127, 255, 212}},
    {"azure", Rgb{240, 255, 255}},
    {"beige", Rgb{245, 245, 220}},
    {"bisque", Rgb{255, 228, 196}},
    {"black", Rgb{0, 0, 0}},
    {"blanchedalmond", Rgb{255, 235, 205}},
    {"blue", Rgb{0, 0, 255}},
    {"blueviolet", Rgb{138, 43, 226}},
    {"brown", Rgb{165, 42, 42}},
    {"burlywood", Rgb{222, 184, 135}},
    {"cadetblue", Rgb{95, 158, 160}},
    {"chartreuse", Rgb{127, 255, 0}},
    {"chocolate", Rgb{210, 105, 30}},
    {"coral", Rgb{255, 127, 80}},
    {"cornflowerblue", Rgb{100, 149, 237}},
    {"cornsilk", Rgb{255, 248, 220}},
    {"crimson", Rgb{220, 20, 60}},
    {"cyan", Rgb{0, 255, 255}},
    {"darkblue", Rgb{0, 0, 139}},
    {"darkcyan", Rgb{0, 139, 139}},
    {"darkgoldenrod", Rgb{184, 134, 11}},
    {"darkgray", Rgb{169, 169, 169}},
    {"darkgreen", Rgb{0, 100, 0}},
    {"darkgrey", Rgb{169, 169, 169}},
    {"darkkhaki", Rgb{189, 183, 107}},
    {"darkmagenta", Rgb{139, 0, 139}},
    {"darkolivegreen", Rgb{85, 107, 47}},
    {"darkorange", Rgb{255, 140, 0}},
    {"darkorchid", Rgb{153, 50, 204}},
    {"darkred", Rgb{139, 0, 0}},
    {"darksalmon", Rgb{233, 150, 122}},
    {"darkseagreen", Rgb{143, 188, 143}},
    {"darkslateblue", Rgb{72, 61, 139}},
    {"darkslategray", Rgb{47, 79, 79}},
    {"darkslategrey", Rgb{47, 79, 79}},
    {"darkturquoise", Rgb{0, 206, 209}},
    {"darkviolet", Rgb{148, 0, 211}},
    {"deeppink", Rgb{255, 20, 147}},
    {"deepskyblue", Rgb{0, 191, 255}},
    {"dimgray", Rgb{105, 105, 105}},
    {"dimgrey", Rgb{105, 105, 105}},
    {"dodgerblue", Rgb{30, 144, 255}},
    {"firebrick", Rgb{178, 34, 34}},
    {"floralwhite", Rgb{255, 250, 240}},
    {"forestgreen", Rgb{34, 139, 34}},
    {"fuchsia", Rgb{255, 0, 255}},
    {"gainsboro", Rgb{220, 220, 220}},
    {"ghostwhite", Rgb{248, 248, 255}},
    {"gold", Rgb{255, 215, 0}},
    {"goldenrod", Rgb{218, 165, 32}},
    {"gray", Rgb{128, 128, 128}},
    {"green", Rgb{0, 128, 0}},
    {"greenyellow", Rgb{173, 255, 47}},
    {"grey", Rgb{128, 128, 128}},
    {"honeydew", Rgb{240, 255, 240}},
    {"hotpink", Rgb{255, 105, 180}},
    {"indianred", Rgb{205, 92, 92}},
    {"indigo", Rgb{75, 0, 130}},
    {"ivory", Rgb{255, 255, 240}},
    {"khaki", Rgb{240, 230, 140}},
    {"lavender", Rgb{230, 230, 250}},
    {"lavenderblush", Rgb{255, 240, 245}},
    {"lawngreen", Rgb{124, 252, 0}},
    {"lemonchiffon", Rgb{255, 250, 205}},
    {"lightblue", Rgb{173, 216, 230}},
    {"lightcoral", Rgb{240, 128, 128}},
    {"lightcyan", Rgb{224, 255, 255}},
    {"lightgoldenrodyellow", Rgb{250, 250, 210}},
    {"lightgray", Rgb{211, 211, 211}},
    {"lightgreen", Rgb{144, 238, 144}},
    {"lightgrey", Rgb{211, 211, 211}},
    {"lightpink", Rgb{255, 182, 193}},
    {"lightsalmon", Rgb{255, 160, 122}},
    {"lightseagreen", Rgb{32, 178, 170}},
    {"lightskyblue", Rgb{135, 206, 250}},
    {"lightslategray", Rgb{119, 136, 153}},
    {"lightslategrey", Rgb{119, 136, 153}},
    {"lightsteelblue", Rgb{176, 196, 222}},
    {"lightyellow", Rgb{255, 255, 224}},
    {"lime", Rgb{0, 255, 0}},
    {"limegreen", Rgb{50, 205, 50}},
    {"linen", Rgb{250, 240, 230}},
    {"magenta", Rgb{255, 0, 255}},
    {"maroon", Rgb{128, 0, 0}},
    {"mediumaquamarine", Rgb{102, 205, 170}},
    {"mediumblue", Rgb{0, 0, 205}},
    {"mediumorchid", Rgb{186, 85, 211}},
    {"mediumpurple", Rgb{147, 112, 219}},
    {"mediumseagreen", Rgb{60, 179, 113}},
    {"mediumslateblue", Rgb{123, 104, 238}},
    {"mediumspringgreen", Rgb{0, 250, 154}},
    {"mediumturquoise", Rgb{72, 209, 204}},
    {"mediumvioletred", Rgb{199, 21, 133}},
    {"midnightblue", Rgb{25, 25, 112}},
    {"mintcream", Rgb{245, 255, 250}},
    {"mistyrose", Rgb{255, 228, 225}},
    {"moccasin", Rgb{255, 228, 181}},
    {"navajowhite", Rgb{255, 222, 173}},
    {"navy", Rgb{0, 0, 128}},
    {"oldlace", Rgb{253, 245, 230}},
    {"olive", Rgb{128, 128, 0}},
    {"olivedrab", Rgb{107, 142, 35}},
    {"orange", Rgb{255, 165, 0}},
    {"orangered", Rgb{255, 69, 0}},
    {"orchid", Rgb{218, 112, 214}},
    {"palegoldenrod", Rgb{238, 232, 170}},
    {"palegreen", Rgb{152, 251, 152}},
    {"paleturquoise", Rgb{175, 238, 238}},
    {"palevioletred", Rgb{219, 112, 147}},
    {"papayawhip", Rgb{255, 239, 213}},
    {"peachpuff", Rgb{255, 218, 185}},
    {"peru", Rgb{205, 133, 63}},
    {"pink", Rgb{255, 192, 203}},
    {"plum", Rgb{221, 160, 221}},
    {"powderblue", Rgb{176, 224, 230}},
    {"purple", Rgb{128, 0, 128}},
    {"rebeccapurple", Rgb{102, 51, 153}},
    {"red", Rgb{255, 0, 0}},
    {"rosybrown", Rgb{188, 143, 143}},
    {"royalblue", Rgb{65, 105, 225}},
    {"saddlebrown", Rgb{139, 69, 19}},
    {"salmon", Rgb{250, 128, 114}},
    {"sandybrown", Rgb{244, 164, 96}},
    {"seagreen", Rgb{46, 139, 87}},
    {"seashell", Rgb{255, 245, 238}},
    {"sienna", Rgb{160, 82, 45}},
    {"silver", Rgb{192, 192, 192}},
    {"skyblue", Rgb{135, 206, 235}},
    {"slateblue", Rgb{106, 90, 205}},
    {"slategray", Rgb{112, 128, 144}},
    {"slategrey", Rgb{112, 128, 144}},
    {"snow", Rgb{255, 250, 250}},
    {"springgreen", Rgb{0, 255, 127}},
    {"steelblue", Rgb{70, 130, 180}},
    {"tan", Rgb{210, 180, 140}},
    {"teal", Rgb{0, 128, 128}},
    {"thistle", Rgb{216, 191, 216}},
    {"tomato", Rgb{255, 99, 71}},
    {"turquoise", Rgb{64, 224, 208}},
    {"violet", Rgb{238, 130, 238}},
    {"wheat", Rgb{245, 222, 179}},
    {"white", Rgb{255, 255, 255}},
    {"whitesmoke", Rgb{245, 245, 245}},
    {"yellow", Rgb{255, 255, 0}},
    {"yellowgreen", Rgb{154, 205, 50}},
}};
// clang-format on

// 查表：名字必须全小写且完全匹配，否则返回 nullopt。
constexpr std::optional<Rgb> lookup_named_color(std::string_view name) noexcept {
    std::size_t lo = 0;
    std::size_t hi = kNamedColors.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (kNamedColors[mid].name < name) {
            lo = mid + 1;
        } else if (name < kNamedColors[mid].name) {
            hi = mid;
        } else {
            return kNamedColors[mid].value;
        }
    }
    return std::nullopt;
}

}  // namespace termchart::color

// ===== include/termchart/color/palette.hpp =====
// palette.hpp —— 命名调色板
//
// 与 colormap 的区别：**离散**。系列图例、多条折线要的是"彼此可区分的一组色"，
// 而不是连续色带。因此 palette 是一份条目顺序重要的数组（第 i 个系列取第 i 色）。



namespace termchart::color {

enum class PaletteKind : std::uint8_t {
    Default = 0,   // 深浅背景都可辨的通用色（默认即可用）
    Ocean = 1,
    Forest = 2,
    Sunset = 3,
    Mono = 4,      // 灰度：给单色/打印/降级场景
    HighContrast = 5,
};

inline constexpr int kPaletteCount = 6;

// 未知名字返回 Default，而不是失败：配色不该让图表画不出来。
TERMCHART_API std::span<const Rgb> palette(PaletteKind kind) noexcept;
TERMCHART_API std::span<const Rgb> palette_by_name(std::string_view name) noexcept;

// 第 index 个系列的取色：**循环取模**。系列数超过色数时不越界。
TERMCHART_API Rgb series_color(PaletteKind kind, std::size_t index) noexcept;
TERMCHART_API std::size_t palette_size(PaletteKind kind) noexcept;
TERMCHART_API const char* palette_name(PaletteKind kind) noexcept;

}  // namespace termchart::color

// ===== include/termchart/color/quantize.hpp =====
// quantize.hpp —— 真彩 → 256 → 16 降级
//
// 颜色管线的一环（docs/design/color_pipeline.md）：
//   真彩 Rgb →（终端能力）→ Ansi256 / Ansi16 / None。
// 能力本身由 term/caps.hpp 给出，本文件只做"退化算法"，不知道终端是否存在。



namespace termchart::color {

// xterm-256 立方格 / 灰阶映射。索引可逆（见 from_index_256）。
TERMCHART_API std::uint8_t to_index_256(Rgb color) noexcept;

// 最近邻映射到标准 16 色（含高亮档 8..15）。
TERMCHART_API std::uint8_t to_index_16(Rgb color) noexcept;

TERMCHART_API Rgb from_index_256(std::uint8_t index) noexcept;
TERMCHART_API Rgb from_index_16(std::uint8_t index) noexcept;

// 语义色 → 在给定能力下能表达的色。None 档返回"未指定"，让终端沿用默认色。
TERMCHART_API Color degrade(Color color, ColorDepth depth) noexcept;
TERMCHART_API Style degrade(Style style, ColorDepth depth) noexcept;

}  // namespace termchart::color

// ===== include/termchart/color/resolve.hpp =====
// resolve.hpp —— Color / Style → 真正的 RGB（GUI 与位图输出的分界点）
//
// 终端侧通常把 `Color` 原样交给 SGR 序列化，由终端去解释（16 色/256 色/真彩）；
// 但 GUI 和位图输出（PNG 截图、sixel 预览）必须拿到**具体像素色**。这个文件就是
// 那道转换，而且是唯一实现：
//   * None  → 用调用方给的默认色（终端默认前景/背景），不是黑色；
//   * Ansi16 / Indexed → 查表；
//   * Rgb   → 先按 ColorDepth 量化再取色，保证"GUI 里看到的颜色"与"终端实际
//     显示的颜色"一致（深度不足时不假装能显示真彩）。


namespace termchart::color {

struct ResolveOptions {
    // 目标终端/画布的颜色深度；Rgb 颜色会先量化到该深度再取色。
    ColorDepth depth = ColorDepth::TrueColor;
    // 默认前景（Color 为 None 时使用）。取常见终端的浅灰前景，而不是纯白。
    Rgb default_fg{229, 229, 229};
    // 默认背景（需要背景色但 Style 未指定时使用）。
    Rgb default_bg{20, 20, 20};
};

struct ResolvedStyle {
    Rgb fg{};
    Rgb bg{};
    // Style 未指定背景色：GUI 侧应当与下层混合，而不是填默认背景。
    bool bg_transparent = false;
};

// Color → RGB；None 返回 default_fg。
[[nodiscard]] TERMCHART_API Rgb resolve_color(const Color& color,
                                              const ResolveOptions& options = {}) noexcept;

// Color → RGB；None 返回 default_bg（背景语义的默认值与前景不同）。
[[nodiscard]] TERMCHART_API Rgb resolve_background(
    const Color& color, const ResolveOptions& options = {}) noexcept;

// Style → {fg, bg, bg_transparent}。属性位（加粗/下划线等）不在本函数职责内，
// 调用方仍从原 Style 读取。
[[nodiscard]] TERMCHART_API ResolvedStyle resolve_style(
    const Style& style, const ResolveOptions& options = {}) noexcept;

}  // namespace termchart::color

// ===== include/termchart/math3d/vec3.hpp =====
// vec3.hpp —— 三维向量（header-only）
//
// 仅放 3D 投影真正需要的那几个运算；依赖 <cmath>，不引入 eigen/glm。
// 约定：行主序矩阵 × 列向量（见 mat4.hpp），本文件只负责向量代数。


namespace termchart::math3d {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() noexcept = default;
    constexpr Vec3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const noexcept { return {x / s, y / s, z / s}; }

    constexpr double dot(const Vec3& o) const noexcept { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const noexcept {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    constexpr double length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
    constexpr Vec3 normalized() const noexcept {
        const double l = length();
        return l > 0.0 ? (*this) / l : Vec3{};
    }
    constexpr bool operator==(const Vec3& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

}  // namespace termchart::math3d

// ===== include/termchart/math3d/mat4.hpp =====
// mat4.hpp —— 4×4 矩阵（header-only，行主序）
//
// 约定（与绝大多数图形教材一致）：矩阵 × 列向量。元素 m[r*4+c] 即第 r 行第 c 列。
// `transform(p)` 计算 M·p（齐次 w=1）。投影链 = perspective · look_at，即
// clip = perspective(look_at(p))。所有函数 constexpr，便于单测断言确定性结果。



namespace termchart::math3d {

struct Mat4 {
    double m[16] = {};  // 行主序，初值全 0

    constexpr Mat4() { identity(); }

    constexpr void identity() noexcept {
        for (int i = 0; i < 16; ++i) {
            m[i] = (i % 5 == 0) ? 1.0 : 0.0;  // 对角线（index 0,5,10,15）
        }
    }

    constexpr double& at(int r, int c) noexcept { return m[r * 4 + c]; }
    constexpr double at(int r, int c) const noexcept { return m[r * 4 + c]; }

    // M · (x,y,z,1)
    constexpr Vec3 transform(const Vec3& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = 1.0;
        return {m[0] * x + m[1] * y + m[2] * z + m[3] * w,
                m[4] * x + m[5] * y + m[6] * z + m[7] * w,
                m[8] * x + m[9] * y + m[10] * z + m[11] * w};
    }

    // 透视除法后的齐次 w（投影矩阵填在 m[11]，clip.w = -(view.z)）。
    constexpr double clip_w(const Vec3& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = 1.0;
        return m[12] * x + m[13] * y + m[14] * z + m[15] * w;
    }
};

constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k) {
                s += a.at(i, k) * b.at(k, j);
            }
            r.at(i, j) = s;
        }
    }
    return r;
}

constexpr Mat4 translate(double tx, double ty, double tz) noexcept {
    Mat4 r;
    r.at(0, 3) = tx;  // 平移量在最后一列（行主序下为 m[3]/m[7]/m[11]）
    r.at(1, 3) = ty;
    r.at(2, 3) = tz;
    return r;
}

constexpr Mat4 scale(double sx, double sy, double sz) noexcept {
    Mat4 r;
    r.at(0, 0) = sx;
    r.at(1, 1) = sy;
    r.at(2, 2) = sz;
    return r;
}

// 绕 X 轴旋转（右手系，正角逆时针看向 +X）。
constexpr Mat4 rotate_x(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(1, 1) = c;
    r.at(1, 2) = -s;
    r.at(2, 1) = s;
    r.at(2, 2) = c;
    return r;
}

constexpr Mat4 rotate_y(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 2) = s;
    r.at(2, 0) = -s;
    r.at(2, 2) = c;
    return r;
}

constexpr Mat4 rotate_z(double a) noexcept {
    Mat4 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 1) = -s;
    r.at(1, 0) = s;
    r.at(1, 1) = c;
    return r;
}

// 透视投影（右手系，相机看向 -Z）。fovy/near/far 单位为同一坐标空间。
inline Mat4 perspective(double fovy_rad, double aspect, double near, double far) noexcept {
    Mat4 r;  // 全 0
    const double f = 1.0 / std::tan(fovy_rad / 2.0);
    r.at(0, 0) = f / aspect;
    r.at(1, 1) = f;
    r.at(2, 2) = (far + near) / (near - far);
    r.at(2, 3) = (2.0 * far * near) / (near - far);
    r.at(3, 2) = -1.0;
    return r;
}

// 视图矩阵：把世界坐标变换到以 eye 为原点、看向 center 的相机空间。
// up 与视线近似平行时退化为 (0,0,1)，避免叉乘退化出零向量。
inline Mat4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
    const Vec3 z = (eye - center).normalized();
    Vec3 up_used = up;
    if (std::fabs(z.dot(up_used)) > 0.999) {
        up_used = Vec3{0.0, 0.0, 1.0};
    }
    const Vec3 x = up_used.cross(z).normalized();
    const Vec3 y = z.cross(x);
    Mat4 r;
    r.at(0, 0) = x.x;
    r.at(0, 1) = x.y;
    r.at(0, 2) = x.z;
    r.at(0, 3) = -x.dot(eye);
    r.at(1, 0) = y.x;
    r.at(1, 1) = y.y;
    r.at(1, 2) = y.z;
    r.at(1, 3) = -y.dot(eye);
    r.at(2, 0) = z.x;
    r.at(2, 1) = z.y;
    r.at(2, 2) = z.z;
    r.at(2, 3) = -z.dot(eye);
    r.at(3, 3) = 1.0;
    return r;
}

}  // namespace termchart::math3d

// ===== include/termchart/math3d/camera.hpp =====
// camera.hpp —— 轨道相机 + 透视投影到画布坐标（header-only）
//
// 设计取舍：3D 渲染**不**自造 z-buffer/终端光栅器，而是把 3D 点投影到 2D 画布坐标
// 后，作为 Scene 图元交给已有的 Scene→Canvas（braille/halfblock）管线。深度只用来
// 决定着色（近亮远暗），得到可辨识的立体感而不重复造轮子。
//
// project() 返回的屏幕坐标即画布格坐标（左上为原点，y 向下），直接喂给 Scene 的
// MoveTo/LineTo/RectOp 即可被 render_scene 消费。


namespace termchart::math3d {

constexpr double kDeg2Rad = 0.017453292519943295769;  // π/180

struct Projected {
    double x = 0.0;   // 画布列坐标
    double y = 0.0;   // 画布行坐标
    double depth = 0.0;  // 归一化深度：0=最近，1=最远，供着色
    bool visible = false;  // 在视锥内且位于相机前方
};

struct Camera {
    Vec3 eye{0.0, 0.0, 5.0};
    Vec3 target{0.0, 0.0, 0.0};
    Vec3 up{0.0, 1.0, 0.0};
    double fovy_deg = 55.0;
    double aspect = 1.0;  // vw / vh
    double near = 0.1;
    double far = 100.0;

    // 以 target 为中心、按方位角/仰角/距离摆放 eye。
    void orbit(double azimuth_deg, double elevation_deg, double distance) noexcept {
        const double az = azimuth_deg * kDeg2Rad;
        const double el = elevation_deg * kDeg2Rad;
        const double ce = std::cos(el);
        eye = target + Vec3{ce * std::sin(az), std::sin(el), ce * std::cos(az)} * distance;
    }

    // 世界坐标 → 画布坐标。vw/vh 为画布尺寸（格）。
    Projected project(const Vec3& world, double vw, double vh) const noexcept {
        const Mat4 view = look_at(eye, target, up);
        const Mat4 proj = perspective(fovy_deg * kDeg2Rad, aspect, near, far);
        const Mat4 vp = proj * view;

        const Vec3 clip = vp.transform(world);
        const double w = vp.clip_w(world);  // 透视矩阵的 clip.w = -z_view
        if (w <= 0.0) {
            return {};  // 相机后方
        }
        const double invw = 1.0 / w;
        const double ndcx = clip.x * invw;
        const double ndcy = clip.y * invw;
        const double ndcz = clip.z * invw;
        if (ndcz < -1.0 || ndcz > 1.0) {
            return {};  // 超出近/远裁剪面
        }

        const double sx = (ndcx * 0.5 + 0.5) * vw;
        const double sy = (1.0 - (ndcy * 0.5 + 0.5)) * vh;

        const Vec3 viewp = view.transform(world);
        const double span = far - near;
        const double t = (span > 0.0) ? clamp_d(((-viewp.z) - near) / span, 0.0, 1.0) : 0.0;
        return {sx, sy, t, true};
    }

private:
    static constexpr double clamp_d(double v, double lo, double hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

}  // namespace termchart::math3d

// ===== include/termchart/math4d/vec4.hpp =====
// vec4.hpp —— 四维向量（header-only）
//
// math3d::Vec3 的 4D 对应物。仅放 4D 投影真正需要的运算；依赖 <cmath>，
// 不引入 eigen/glm。约定与 vec3.hpp 一致：行主序矩阵 × 列向量（见 mat5.hpp），
// 本文件只负责向量代数。4D 没有叉积的直推（那是 7D 才有），故不提供 cross。


namespace termchart::math4d {

struct Vec4 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 0.0;

    constexpr Vec4() noexcept = default;
    constexpr Vec4(double x_, double y_, double z_, double w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}

    constexpr Vec4 operator+(const Vec4& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z, w + o.w};
    }
    constexpr Vec4 operator-(const Vec4& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z, w - o.w};
    }
    constexpr Vec4 operator-() const noexcept { return {-x, -y, -z, -w}; }
    constexpr Vec4 operator*(double s) const noexcept { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec4 operator/(double s) const noexcept { return {x / s, y / s, z / s, w / s}; }

    constexpr double dot(const Vec4& o) const noexcept {
        return x * o.x + y * o.y + z * o.z + w * o.w;
    }
    constexpr double length() const noexcept { return std::sqrt(x * x + y * y + z * z + w * w); }
    constexpr Vec4 normalized() const noexcept {
        const double l = length();
        return l > 0.0 ? (*this) / l : Vec4{};
    }
    constexpr bool operator==(const Vec4& o) const noexcept {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
};

}  // namespace termchart::math4d

// ===== include/termchart/math4d/camera4.hpp =====
// camera4.hpp —— 4D→3D 透视投影（header-only，"除以 W"的齐次除法）
//
// 这是 4D 管线第一阶段的世界→3D 相机：把 4D 相机架在第 4 轴（W）上、看向
// 原点，对一个 Vec4 做透视除法，产出 math3d::Vec3（交给下游 chart3d 的 3D
// 轨道相机做第二阶段 3D→2D）。与 math3d::Camera 的关系：后者吃 Vec3 出画布
// 坐标，本结构吃 Vec4 出 Vec3——两者串起来就是完整的 4D→2D 链。
//
// 之所以返回 math3d::Vec3 而不是 math4d 自有类型：3D 向量的语义归 math3d，
// 且下游（chart4d）本来就要把它喂给 math3d 的相机；math4d 与 math3d 同为 L3，
// 该依赖方向被层号允许。



namespace termchart::math4d {

constexpr double kDeg2Rad = 0.017453292519943295769;  // π/180（与 math3d::kDeg2Rad 同值）

// 4D→3D 投影结果。xyz 即投影到的 3D 点；w_view 是相机空间深度
// （distance - w，越大越远），供着色/调试；visible 表示位于相机前方且
// 不在近裁剪面之内。
struct Projected4 {
    math3d::Vec3 xyz{};
    double w_view = 0.0;
    bool visible = false;
};

struct Camera4 {
    double distance = 3.0;  // 4D 相机置于 +W 轴、距原点 distance，看向 -W（朝原点）
    double focal = 2.0;     // W 向焦距：投影缩放 = focal / w_view（越大视野越窄）
    double near = 0.1;      // 近裁剪：w_view 的下界（含）以外的点不可见

    // 透视除以 W：先把点移到相机前方深度 w_view = distance - p.w，再按
    // focal / w_view 缩放 xyz。w_view <= near（相机处、后方或太近）→ 不可见，
    // 避免除出巨大/负值坐标（与 math3d::Camera 的 w<=0 早退同思路）。
    Projected4 project(const Vec4& p) const noexcept {
        Projected4 r;
        const double w_view = distance - p.w;
        if (!(w_view > near)) {
            return r;  // NaN 也走这里：!(NaN>near) 为真
        }
        const double k = focal / w_view;
        r.xyz = math3d::Vec3{p.x * k, p.y * k, p.z * k};
        r.w_view = w_view;
        r.visible = true;
        return r;
    }
};

// 由 W 向垂直视场角推焦距：f = 1 / tan(fov/2)，与 math3d 透视的 f 同式。
// fov_deg 越视野小、焦距越大（放大）；60° → ≈1.732。
inline double focal_from_fov_deg(double fov_deg) noexcept {
    return 1.0 / std::tan(fov_deg * kDeg2Rad / 2.0);
}

}  // namespace termchart::math4d

// ===== include/termchart/math4d/mat5.hpp =====
// mat5.hpp —— 5×5 矩阵（header-only，行主序，4D 齐次坐标）
//
// 约定与 mat4.hpp 一致：矩阵 × 列向量。用 5×5（而非 4×4）是为了容纳 4D
// 齐次坐标 (x,y,z,w,1)：`transform(p)` 计算 M·(x,y,z,w,1) 并取前四个分量，
// 于是平移也落进同一个矩阵（与 mat4 的 w=1 齐次平移同思路）。所有函数
// constexpr，便于单测断言确定性结果。
//
// 4D 旋转与 3D 的本质区别：3D 只有 3 个坐标轴、旋转发生在**轴正交的平面**里
// （XY/XZ/YZ）；4D 有 4 个轴，任取两轴张成一个旋转平面，共 C(4,2)=6 个：
//   XY(0,1) XZ(0,2) XW(0,3) YZ(1,2) YW(1,3) ZW(2,3)。
// 每个都是在对应二维子平面上嵌一个标准 2D 旋转、其余对角线为 1。



namespace termchart::math4d {

struct Mat5 {
    double m[25] = {};  // 行主序，初值全 0

    constexpr Mat5() { identity(); }

    constexpr void identity() noexcept {
        for (int i = 0; i < 25; ++i) {
            m[i] = (i % 6 == 0) ? 1.0 : 0.0;  // 对角线（index 0,6,12,18,24）
        }
    }

    constexpr double& at(int r, int c) noexcept { return m[r * 5 + c]; }
    constexpr double at(int r, int c) const noexcept { return m[r * 5 + c]; }

    // M · (x,y,z,w,1)，取前 4 行（第 5 行是齐次占位，恒为 (0,0,0,0,1)·= 1）。
    constexpr Vec4 transform(const Vec4& p) const noexcept {
        const double x = p.x, y = p.y, z = p.z, w = p.w, t = 1.0;
        return {m[0] * x + m[1] * y + m[2] * z + m[3] * w + m[4] * t,
                m[5] * x + m[6] * y + m[7] * z + m[8] * w + m[9] * t,
                m[10] * x + m[11] * y + m[12] * z + m[13] * w + m[14] * t,
                m[15] * x + m[16] * y + m[17] * z + m[18] * w + m[19] * t};
    }
};

constexpr Mat5 operator*(const Mat5& a, const Mat5& b) noexcept {
    Mat5 r;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            double s = 0.0;
            for (int k = 0; k < 5; ++k) {
                s += a.at(i, k) * b.at(k, j);
            }
            r.at(i, j) = s;
        }
    }
    return r;
}

// 4D 平移：平移量落在最后一列（行主序下为 m[4]/m[9]/m[14]/m[19]）。
constexpr Mat5 translate(double tx, double ty, double tz, double tw) noexcept {
    Mat5 r;
    r.at(0, 4) = tx;
    r.at(1, 4) = ty;
    r.at(2, 4) = tz;
    r.at(3, 4) = tw;
    return r;
}

constexpr Mat5 scale(double sx, double sy, double sz, double sw) noexcept {
    Mat5 r;
    r.at(0, 0) = sx;
    r.at(1, 1) = sy;
    r.at(2, 2) = sz;
    r.at(3, 3) = sw;
    return r;
}

// 绕 (i,j) 坐标平面的旋转（i,j ∈ {0,1,2,3}，对应 x,y,z,w），符号约定与
// mat4::rotate_x 一致（plane(1,2) 即 rotate_x）：at(i,i)=c at(i,j)=-s
// at(j,i)=s at(j,j)=c。正角把第 i 轴转向第 j 轴。
constexpr Mat5 rotate_plane(int i, int j, double a) noexcept {
    Mat5 r;
    const double c = std::cos(a), s = std::sin(a);
    r.at(i, i) = c;
    r.at(i, j) = -s;
    r.at(j, i) = s;
    r.at(j, j) = c;
    return r;
}

// 4D 的 6 个旋转平面（见文件头注释）。rotate_xy(90°) 把 e_x 送到 e_y，余类推。
constexpr Mat5 rotate_xy(double a) noexcept { return rotate_plane(0, 1, a); }
constexpr Mat5 rotate_xz(double a) noexcept { return rotate_plane(0, 2, a); }
constexpr Mat5 rotate_xw(double a) noexcept { return rotate_plane(0, 3, a); }
constexpr Mat5 rotate_yz(double a) noexcept { return rotate_plane(1, 2, a); }
constexpr Mat5 rotate_yw(double a) noexcept { return rotate_plane(1, 3, a); }
constexpr Mat5 rotate_zw(double a) noexcept { return rotate_plane(2, 3, a); }

}  // namespace termchart::math4d

// ===== include/termchart/scale/auto_domain.hpp =====
// auto_domain.hpp —— 由数据范围反推「好看」的值域
//
// 图表不该把轴正好卡在 min/max 上（看着压抑且易出界）。这里按 5% 留白后，
// 把两端对齐到 nice 步长；可选 include_zero 让坐标轴穿过原点（财务/差值图常用）。


namespace termchart::scale {

struct Domain {
    double min = 0.0;
    double max = 0.0;
    bool ok = false;
};

// data_min/data_max 为数据极值；include_zero 时值域下界不超过 0。
TERMCHART_API Domain nice_domain(double data_min, double data_max,
                                 bool include_zero = false) noexcept;

}  // namespace termchart::scale

// ===== include/termchart/scale/nice.hpp =====
// nice.hpp —— nice number 算法（刻度与值域对齐的核心）
//
// 「好看的数」= 1 / 2 / 5 × 10^k。它让轴标签对人类可读，而不是 0.317、3.142…
// 这是 Heckbert「Graphics Gems」经典算法的最小可用版，被 ticks/auto_domain 复用。



namespace termchart::scale {

// 把正数 x 收成最近的 nice 数。round=true 时向上取整（用于步长），
// round=false 时就近（用于值域对齐）。
TERMCHART_API double nice_number(double x, bool round = false) noexcept;

// 给定跨度与目标刻度个数，返回 nice 步长（span / target 的 nice 上界）。
TERMCHART_API double nice_step(double span, int target_ticks) noexcept;

}  // namespace termchart::scale

// ===== include/termchart/scale/scale.hpp =====
// scale.hpp —— 数据坐标 ↔ 显示坐标的抽象（比例尺基类）
//
// 终端图表和 GUI 控件都要把「数据域」映射到「像素/归一域」。把这件事抽成
// 一个多态基类，是为了让 chart / graph / canvas 在**不知道具体映射**的情况下
// 工作：换线性为对数、换连续为分类，调用方代码一行不动。
//
// 不变量（所有派生类必须遵守）：
//   * forward(data) 单调；inverse(forward(x)) ≈ x（在 valid 域内）；
//   * 越界输入走 NaN 或夹紧，绝不未定义行为（数据来自用户 CSV，含 NaN）。


namespace termchart::scale {

class Scale {
public:
    virtual ~Scale();

    // 数据坐标 → 显示坐标（像素或归一化 [0,1]）。
    [[nodiscard]] virtual double forward(double data) const noexcept = 0;
    // 显示坐标 → 数据坐标。
    [[nodiscard]] virtual double inverse(double pixel) const noexcept = 0;

    // 是否可逆向求解（对称对数等在零附近不可逆，返回 false）。
    [[nodiscard]] virtual bool invertible() const noexcept { return true; }
    // 域是否合法（未退化、对数域为正等）。
    [[nodiscard]] virtual bool valid() const noexcept = 0;

    [[nodiscard]] virtual double domain_min() const noexcept = 0;
    [[nodiscard]] virtual double domain_max() const noexcept = 0;
    [[nodiscard]] virtual double range_min() const noexcept = 0;
    [[nodiscard]] virtual double range_max() const noexcept = 0;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/band.hpp =====
// band.hpp —— 带状比例尺（每个类别占一条等宽带，forward 返回带中心）
//
// 与 CategoryScale 把类别压成"网格点"不同，BandScale 把 [range_min, range_max]
// 均分成 n 条等宽带，第 i 个类别落在第 i 条带的中心。柱状/条形图让柱体落在带
// 内、两侧留 padding 时观感最自然。band_width() 暴露单带宽度供绘制。



namespace termchart::scale {

class BandScale final : public Scale {
public:
    BandScale() = default;
    BandScale(std::size_t categories, double range_min = 0.0, double range_max = 1.0,
              double padding = 0.1) noexcept {
        set_categories(categories);
        set_range(range_min, range_max);
        set_padding(padding);
    }

    void set_categories(std::size_t n) noexcept { n_ = n; }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_padding(double p) noexcept { padding_ = util::clamp(p, 0.0, 0.5); }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (n_ == 0) {
            return rmin_;
        }
        const double band = (rmax_ - rmin_) / static_cast<double>(n_);
        const double inner = band * (1.0 - 2.0 * padding_);
        const double center = rmin_ + (data + 0.5) * band;
        if (clamp_) {
            const double half = inner / 2.0;
            return util::clamp(center, rmin_ + half, rmax_ - half);
        }
        return center;
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        if (n_ == 0) {
            return 0.0;
        }
        const double band = (rmax_ - rmin_) / static_cast<double>(n_);
        double t = (pixel - rmin_) / band - 0.5;
        if (clamp_) {
            t = util::clamp(t, 0.0, static_cast<double>(n_ > 0 ? n_ - 1 : 0));
        }
        return t;
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(rmin_) && util::is_finite(rmax_) && n_ >= 1 &&
               rmax_ > rmin_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return 0.0; }
    [[nodiscard]] double domain_max() const noexcept override {
        return static_cast<double>(n_ > 0 ? n_ - 1 : 0);
    }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }

    [[nodiscard]] double band_width() const noexcept {
        return n_ > 0 ? (rmax_ - rmin_) / static_cast<double>(n_) : 0.0;
    }
    [[nodiscard]] std::size_t categories() const noexcept { return n_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    std::size_t n_ = 0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    double padding_ = 0.1;
    bool clamp_ = true;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/category.hpp =====
// category.hpp —— 类别比例尺（离散类别等距排布）
//
// 把 0..n-1 的类别索引线性映射到 [range_min, range_max]：第 i 个类别落在
// 等距网格点上。柱形/条形/分类型坐标轴常用。forward(类别索引) 取对应网格点，
// inverse(像素) 反算最近类别索引（用于命中判定）。



namespace termchart::scale {

class CategoryScale final : public Scale {
public:
    CategoryScale() = default;
    explicit CategoryScale(std::size_t categories, double range_min = 0.0,
                          double range_max = 1.0) noexcept {
        set_categories(categories);
        set_range(range_min, range_max);
    }

    void set_categories(std::size_t n) noexcept { n_ = n; }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (n_ <= 1) {
            return rmin_;
        }
        double t = data / static_cast<double>(n_ - 1);
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        const double rspan = rmax_ - rmin_;
        double t = rspan == 0.0 ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return t * static_cast<double>(n_ > 1 ? n_ - 1 : 0);
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(rmin_) && util::is_finite(rmax_) && n_ >= 1;
    }

    [[nodiscard]] double domain_min() const noexcept override { return 0.0; }
    [[nodiscard]] double domain_max() const noexcept override {
        return static_cast<double>(n_ > 0 ? n_ - 1 : 0);
    }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }

    [[nodiscard]] std::size_t categories() const noexcept { return n_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    std::size_t n_ = 0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/linear.hpp =====
// linear.hpp —— 线性比例尺（最常用，也是其他比例尺的退化形态）
//
// 默认构造即「单位映射」domain [0,1] → range [0,1]，方便在未知数据范围时
// 先占位、后 set_domain。clamp 默认开启：绘图 API 的越界输入极其常见，
// 默认密封比默认崩溃友好。


namespace termchart::scale {

class LinearScale final : public Scale {
public:
    LinearScale() = default;
    LinearScale(double domain_min, double domain_max, double range_min = 0.0,
                double range_max = 1.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        clamp_ = clamp;
    }

    void set_domain(double domain_min, double domain_max) noexcept {
        dmin_ = domain_min;
        dmax_ = domain_max;
    }
    void set_range(double range_min, double range_max) noexcept {
        rmin_ = range_min;
        rmax_ = range_max;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        const double span = dmax_ - dmin_;
        double t = (span == 0.0) ? 0.0 : (data - dmin_) / span;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        const double rspan = rmax_ - rmin_;
        double t = (rspan == 0.0) ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(dmin_, dmax_, t);
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(dmin_) && util::is_finite(dmax_) &&
               util::is_finite(rmin_) && util::is_finite(rmax_) && dmin_ != dmax_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }
    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    double dmin_ = 0.0;
    double dmax_ = 1.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/log.hpp =====
// log.hpp —— 对数比例尺
//
// 域必须为正（或同时为负，这里只支持正域——负域先取绝对值再映射，调用方处理）。
// 零与负数在 forward 里返回 NaN：对数域里它们没有位置，UI 应在此之前过滤或
// 切到线性子段，而不是让整张图消失。



namespace termchart::scale {

class LogScale final : public Scale {
public:
    LogScale() = default;
    LogScale(double domain_min, double domain_max, double range_min = 0.0,
             double range_max = 1.0, double base = 10.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        base_ = base;
        clamp_ = clamp;
    }

    void set_domain(double domain_min, double domain_max) noexcept {
        dmin_ = domain_min;
        dmax_ = domain_max;
    }
    void set_range(double range_min, double range_max) noexcept {
        rmin_ = range_min;
        rmax_ = range_max;
    }
    void set_base(double base) noexcept { base_ = base; }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        if (!util::is_finite(data) || data <= 0.0 || !valid()) {
            return std::nan("");
        }
        const double lmin = std::log(dmin_) / std::log(base_);
        const double lmax = std::log(dmax_) / std::log(base_);
        const double ldata = std::log(data) / std::log(base_);
        const double lspan = lmax - lmin;
        double t = (lspan == 0.0) ? 0.0 : (ldata - lmin) / lspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        if (!valid()) {
            return std::nan("");
        }
        const double rspan = rmax_ - rmin_;
        double t = (rspan == 0.0) ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        const double lmin = std::log(dmin_) / std::log(base_);
        const double lmax = std::log(dmax_) / std::log(base_);
        return std::pow(base_, util::lerp(lmin, lmax, t));
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(dmin_) && util::is_finite(dmax_) &&
               util::is_finite(base_) && base_ > 0.0 && base_ != 1.0 &&
               dmin_ > 0.0 && dmax_ > 0.0 && dmin_ != dmax_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }
    [[nodiscard]] double base() const noexcept { return base_; }

private:
    double dmin_ = 1.0;
    double dmax_ = 10.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    double base_ = 10.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/registry.hpp =====
// registry.hpp —— 比例尺注册表（与 chart/raster 同一套契约）
//
// instance() 用 function-local static：**构造前注册、构造后只读**（调用方在自身构造
// 时对注册表做浅拷贝快照并冻结，运行期注册属 UB——见 docs/design/registry.md）。
// 内置 `linear` / `log` 直接在 create() 里给出；自定义比例尺用 register_scale()
// 登记，重名以最后一次为准（便于测试里临时替换）。

#include <map>


namespace termchart::scale {

class ScaleRegistry {
public:
    using Factory = std::function<std::unique_ptr<Scale>()>;

    static ScaleRegistry& instance() noexcept;

    ScaleRegistry(const ScaleRegistry&) = delete;
    ScaleRegistry& operator=(const ScaleRegistry&) = delete;

    void register_scale(std::string_view name, Factory factory);

    // 未知名字返回 nullptr（不抛异常）。
    [[nodiscard]] std::unique_ptr<Scale> create(std::string_view name) const;
    [[nodiscard]] bool supports(std::string_view name) const;

    // 全部可用名字（内置 + 自定义），按字典序去重；供快照/诊断/CLI 提示用。
    [[nodiscard]] std::vector<std::string> names() const;

private:
    ScaleRegistry() = default;
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace termchart::scale

// ===== include/termchart/scale/ticks.hpp =====
// ticks.hpp —— 刻度生成
//
// 两种入口：
//   * nice_ticks：纯数据轴，无需比例尺，直接给 [dmin,dmax] 上的 nice 刻度；
//   * ticks_for：已有 Scale（线性/对数都行），在其 range 上均匀取点，但刻度
//     值对应 nice 的数据坐标——所以对数轴上刻度是 1/10/100 而非等距空格。



namespace termchart::scale {

// 落在 [dmin,dmax]（自动归一顺序）内的 nice 刻度值，按升序返回。
TERMCHART_API std::vector<double> nice_ticks(double domain_min, double domain_max,
                                             int target = 5) noexcept;

// 对 Scale 生成刻度：在 range 上均匀取约 target 个点（用 nice_step 定步长），
// 再 inverse 回数据坐标。要求 scale.valid()。
TERMCHART_API std::vector<double> ticks_for(const Scale& scale, int target = 5) noexcept;

}  // namespace termchart::scale

// ===== include/termchart/scale/time.hpp =====
// time.hpp —— 时间比例尺（时间戳数值线性映射，语义化时间轴）
//
// 把时间（以数值表示的时间戳，例如 Unix 秒/毫秒；单位由调用方统一）线性映射到
// 显示域。calendar-aware 的刻度（年/月/日/时）由 ticks 模块按数据范围选取，
// 本类只负责坐标变换。与 LinearScale 同构，单列一类以明确"这是时间轴"语义，
// 便于图表层识别并切换到时间刻度策略。


namespace termchart::scale {

class TimeScale final : public Scale {
public:
    TimeScale() = default;
    TimeScale(double domain_min, double domain_max, double range_min = 0.0,
              double range_max = 1.0, bool clamp = true) noexcept {
        set_domain(domain_min, domain_max);
        set_range(range_min, range_max);
        clamp_ = clamp;
    }

    void set_domain(double dmin, double dmax) noexcept {
        dmin_ = dmin;
        dmax_ = dmax;
    }
    void set_range(double rmin, double rmax) noexcept {
        rmin_ = rmin;
        rmax_ = rmax;
    }
    void set_clamp(bool on) noexcept { clamp_ = on; }

    [[nodiscard]] double forward(double data) const noexcept override {
        const double span = dmax_ - dmin_;
        double t = span == 0.0 ? 0.0 : (data - dmin_) / span;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(rmin_, rmax_, t);
    }

    [[nodiscard]] double inverse(double pixel) const noexcept override {
        const double rspan = rmax_ - rmin_;
        double t = rspan == 0.0 ? 0.0 : (pixel - rmin_) / rspan;
        if (clamp_) {
            t = util::clamp(t, 0.0, 1.0);
        }
        return util::lerp(dmin_, dmax_, t);
    }

    [[nodiscard]] bool valid() const noexcept override {
        return util::is_finite(dmin_) && util::is_finite(dmax_) &&
               util::is_finite(rmin_) && util::is_finite(rmax_) && dmin_ != dmax_;
    }

    [[nodiscard]] double domain_min() const noexcept override { return dmin_; }
    [[nodiscard]] double domain_max() const noexcept override { return dmax_; }
    [[nodiscard]] double range_min() const noexcept override { return rmin_; }
    [[nodiscard]] double range_max() const noexcept override { return rmax_; }

    [[nodiscard]] bool clamped() const noexcept { return clamp_; }

private:
    double dmin_ = 0.0;
    double dmax_ = 1.0;
    double rmin_ = 0.0;
    double rmax_ = 1.0;
    bool clamp_ = true;
};

}  // namespace termchart::scale

// ===== include/termchart/scene/command.hpp =====
// command.hpp —— Scene 的指令集（header-only）
//
// Scene 是**非完备图形 IR**：这里只放 vector 后端（SVG / LaTeX-TikZ）与
// canvas 都能消费的那部分图元。**不要往里塞 canvas 才有的能力**
// （例如渐变填充、裁剪路径、图层合成）——那些属于 canvas/（L10）。
//
// 指令一律用聚合体 + std::variant：
//   * variant 的 index() 就是指令类别，不需要额外的 kind 字段；
//   * 聚合体天然可 constexpr 构造、可 operator==，于是 Scene 可整体比较；
//   * 新增指令 = 往 variant 里加一个聚合体，访问者会**编译期**被逼着补全。
//
// 本文件是 header-only（见 cmake/header_only.txt），不得有同名 src 实现。



namespace termchart::scene {

// 注意：聚合体**不会**隐式获得 operator==（C++20 只在你写 = default 时生成），
// 而 variant 的 == 要求每个候选都可比较——Scene 的整体比较依赖它，故逐个 default。

// ── 路径 ────────────────────────────────────────────────────────────────
struct MoveTo {
    Point to;

    constexpr bool operator==(const MoveTo&) const noexcept = default;
};

struct LineTo {
    Point to;

    constexpr bool operator==(const LineTo&) const noexcept = default;
};

// 轴对齐矩形。命名避开 core::Rect，否则成员声明会自引用。
struct RectOp {
    Rect rect;

    constexpr bool operator==(const RectOp&) const noexcept = default;
};

// 圆弧：角度为**弧度**，从 start 逆时针到 end。整圆 = {0, 2π}。
struct ArcOp {
    Point center;
    double radius = 0.0;
    double start = 0.0;
    double end = 0.0;

    constexpr bool operator==(const ArcOp&) const noexcept = default;
};

// ── 绘制状态 ────────────────────────────────────────────────────────────
// "当前路径"由消费者（canvas / vector 后端）维护，Scene 不保存路径栈——
// 保存它就等于把 canvas 的状态机搬进 IR，那正是本文件要避免的事。
struct FillOp {
    Color color;

    // Color::operator== 在库里实现（TERMCHART_API），因此这里不能是 constexpr。
    bool operator==(const FillOp& other) const noexcept { return color == other.color; }
};

struct StrokeOp {
    Color color;
    double width = 1.0;

    bool operator==(const StrokeOp& other) const noexcept {
        return color == other.color && width == other.width;
    }
};

// ── 内容 ────────────────────────────────────────────────────────────────
struct TextOp {
    Point at;
    std::string text;
    Style style;

    bool operator==(const TextOp& other) const noexcept {
        return at == other.at && text == other.text && style == other.style;
    }
};

// 图像只带**引用**（image_id），不持有像素：矢量 IR 的职责是"描述画什么"，
// 把位图塞进来会让一份 Scene 的体积与内存随图片大小失控，也让它无法被廉价复制。
// image_id 由调用方在导出/渲染时解析成 image/ 里的实际位图。
struct ImageOp {
    Rect rect;
    std::uint32_t image_id = 0;

    constexpr bool operator==(const ImageOp&) const noexcept = default;
};

using Command = std::variant<MoveTo, LineTo, RectOp, ArcOp, FillOp, StrokeOp, TextOp, ImageOp>;

// 顺序与 Command 的候选列表一一对应；值即 variant 的 index()。
enum class CommandKind : std::uint8_t {
    MoveTo = 0,
    LineTo,
    Rect,
    Arc,
    Fill,
    Stroke,
    Text,
    Image,
};

constexpr CommandKind kind_of(const Command& command) noexcept {
    return static_cast<CommandKind>(command.index());
}

}  // namespace termchart::scene

// ===== include/termchart/scene/scene.hpp =====
// scene.hpp —— 绘制指令序列（矢量 IR）
//
// 三条渲染路径在它这里分岔：
//   * 终端实时：Scene → canvas/scene_renderer → Canvas → to_buffer() → CellBuffer → renderer；
//   * vector 导出：Scene → backend/vector/*（svg / latex-TikZ）；
//   * Document：scene 被 document/block.hpp 的 Chart 块持有，走 structured 导出。
//
// 因此 Scene 只有两个承诺：**可顺序回放**与**可整体比较**（golden 与
// roundtrip 测试都依赖后者）。它不理解颜色、不做裁剪、不算布局。



namespace termchart::scene {

class Scene {
public:
    Scene() = default;

    TERMCHART_API void clear() noexcept;
    TERMCHART_API void reserve(std::size_t count);
    TERMCHART_API void push(Command command);

    [[nodiscard]] std::size_t size() const noexcept { return commands_.size(); }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

    [[nodiscard]] std::span<const Command> commands() const noexcept { return {commands_}; }
    [[nodiscard]] std::span<Command> commands() noexcept { return {commands_}; }

    // 几何包围盒：只统计带几何的指令（MoveTo/LineTo 记点，Arc 用外接方框）。
    // Text 不参与——它的宽度要等字体度量，那不是 IR 该知道的事。
    // 空场景返回空矩形（w=h=0）。
    [[nodiscard]] TERMCHART_API Rect bounds() const noexcept;

    TERMCHART_API bool operator==(const Scene& other) const noexcept;
    TERMCHART_API bool operator!=(const Scene& other) const noexcept;

private:
    std::vector<Command> commands_;
};

}  // namespace termchart::scene

// ===== include/termchart/scene/bounds.hpp =====
// bounds.hpp —— Scene 的几何包围盒
//
// 为什么需要它：GUI 侧的 `render/scene_renderer` 要先知道"这份 Scene 有多大"才能
// 决定画布/无效区；终端侧也可以用它预分配画布。放在 scene/（L2）而不是 canvas/，
// 因为消费 Scene 的**每个**前端都需要它——不只终端那一条路径。
//
// 计算口径（刻意保守，宁可略大也不要漏）：
//   * 线段取两端点；
//   * 圆弧取"圆心 ± 半径"的正方形（不做最小包围盒）；
//   * 文本按**显示宽度** × 1 行估算（宽字符算 2 列）；
//   * 图片取它的矩形；
//   * **不计描边宽度**：线宽会让实际墨迹略微外扩，本函数只保证几何上包得住。


namespace termchart::scene {

// 空 Scene 返回零矩形（w == h == 0）。
[[nodiscard]] TERMCHART_API Rect scene_bounds(const Scene& scene) noexcept;

}  // namespace termchart::scene

// ===== include/termchart/scene/builder.hpp =====
// builder.hpp —— 流式构建辅助
//
// 存在的唯一理由：让"往 Scene 里追加图元"这件事读起来像绘图代码。
// 它**不持有** Scene（只持有引用），也不缓存当前路径——路径状态属于消费者。



namespace termchart::scene {

class SceneBuilder {
public:
    explicit SceneBuilder(Scene& scene) noexcept : scene_(&scene) {}

    TERMCHART_API SceneBuilder& move_to(Point to);
    TERMCHART_API SceneBuilder& move_to(double x, double y);

    TERMCHART_API SceneBuilder& line_to(Point to);
    TERMCHART_API SceneBuilder& line_to(double x, double y);

    TERMCHART_API SceneBuilder& rect(Rect value);

    // 角度为弧度；整圆传 {0.0, 2π}。
    TERMCHART_API SceneBuilder& arc(Point center, double radius, double start, double end);

    TERMCHART_API SceneBuilder& fill(Color color);
    TERMCHART_API SceneBuilder& stroke(Color color, double width = 1.0);

    TERMCHART_API SceneBuilder& text(Point at, std::string_view text, Style style = Style{});

    TERMCHART_API SceneBuilder& image(Rect rect, std::uint32_t image_id);

    // 逃生口：直接压一条已构造好的指令（variant 新候选还没加语法糖时用）。
    TERMCHART_API SceneBuilder& push(Command command);

    [[nodiscard]] Scene& scene() const noexcept { return *scene_; }

private:
    Scene* scene_;
};

}  // namespace termchart::scene

// ===== include/termchart/scene/visitor.hpp =====
// visitor.hpp —— 指令访问者接口
//
// 为什么既给 std::visit 又给这套虚函数接口：
//   * std::visit 适合"一次性、无状态"的消费（统计、序列化），零分配；
//   * 虚函数访问者适合"有状态、可复用"的消费者（canvas 的光栅化器、
//     SVG 后端），它们需要把状态挂在对象上而不是塞进 lambda 捕获。
//
// accept() 的实现放在 cpp：variant 的访问模板只实例化一次，不进头文件。


namespace termchart::scene {

class CommandVisitor {
public:
    virtual ~CommandVisitor();

    virtual void visit(const MoveTo& command) = 0;
    virtual void visit(const LineTo& command) = 0;
    virtual void visit(const RectOp& command) = 0;
    virtual void visit(const ArcOp& command) = 0;
    virtual void visit(const FillOp& command) = 0;
    virtual void visit(const StrokeOp& command) = 0;
    virtual void visit(const TextOp& command) = 0;
    virtual void visit(const ImageOp& command) = 0;
};

TERMCHART_API void accept(const Command& command, CommandVisitor& visitor);
TERMCHART_API void accept_all(const Scene& scene, CommandVisitor& visitor);

}  // namespace termchart::scene

// ===== include/termchart/scene/stateful_visitor.hpp =====
// stateful_visitor.hpp —— 有状态 Scene 消费者基类
//
// 把"Fill/Stroke 是状态指令、路径需累积、Rect 有填充/描边分支"这套时序
// 收在一处。派生类只实现"往哪画"，不管"什么时候画"。
//
// 为什么需要它：canvas 的 ScenePainter 与 backend/vector 的 SVG 后端各自
// 写了一遍同一套状态机（MoveTo/LineTo 累积、遇到状态指令先按旧状态收口、
// Rect 按有无填充色分两支）。两处独立演化就会出现"同一份 Scene、两种画法"
// 的偏差；这里是那个偏差的唯一收敛点。
//
// 时序约定（与 svg_backend / scene_renderer 原行为一致）：
//   * MoveTo     —— 更新 current_，不 flush（允许多子路径合成一条 <path>）；
//   * LineTo     —— 首个 LineTo 触发 begin_path，其余 extend_path；
//   * Fill/Stroke—— 先 end_path() 收口旧状态，再更新；重置 path_open_；
//   * Rect/Arc/Text/Image —— 先 end_path() 收口，再 emit，并重置 path_open_；
//   * run() 结束 —— 收尾一次。
//
// 两条容易写错的约束（写错就是静默错图，不是编译错误）：
//   1. end_path() 必须**无条件**调用，不能在基类里加 `if (path_open_)` 守卫：
//      MoveTo 会把 path_open_ 置 false，但派生类累积的几何可能已经 dirty，
//      一守卫就漏掉这次收口。判空交给实现自己（d_.empty() / dirty 标志）。
//   2. end_path() 里读到的 fill_/stroke_ 是**旧值**——因为 FillOp/StrokeOp
//      的处理顺序是「先收口、后赋值」。这正是想要的：几何按它出现时的状态着色。


namespace termchart::scene {

class StatefulVisitor : public CommandVisitor {
public:
    void visit(const MoveTo& command) override;
    void visit(const LineTo& command) override;
    void visit(const RectOp& command) override;
    void visit(const ArcOp& command) override;
    void visit(const FillOp& command) override;
    void visit(const StrokeOp& command) override;
    void visit(const TextOp& command) override;
    void visit(const ImageOp& command) override;

    // 遍历 + 收尾。调用方不应再自己调 end_path。
    void run(const Scene& scene);

protected:
    // ── 派生类实现的"往哪画" ──
    // begin_path/extend_path/end_path 三条是路径的三段式：begin 发起点、
    // extend 追加一段、end 收口。SVG 需要 begin（要发 M），终端不需要
    // （extend_path 自带两端），因此派生类把它实现成 no-op 即可。
    virtual void begin_path(const Point& from) = 0;
    virtual void extend_path(const Point& from, const Point& to) = 0;
    virtual void end_path() = 0;

    virtual void emit_filled_rect(const Rect& rect, const Color& fill) = 0;
    virtual void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) = 0;
    virtual void emit_arc(const ArcOp& arc, const Color& stroke, double width) = 0;
    virtual void emit_text(const TextOp& text) = 0;
    virtual void emit_image(const ImageOp& image) = 0;

    // ── 当前状态只读视图（end_path 里读的是"旧值"，见文件头第 2 条）──
    [[nodiscard]] const Color& current_fill() const noexcept { return fill_; }
    [[nodiscard]] const Color& current_stroke() const noexcept { return stroke_; }
    [[nodiscard]] double current_stroke_width() const noexcept { return stroke_width_; }

private:
    void finish_path();  // end_path + 重置 path_open_

    Color fill_ = Color::none();
    Color stroke_ = Color::none();
    double stroke_width_ = 1.0;
    Point current_{};
    bool path_open_ = false;
};

}  // namespace termchart::scene

// ===== include/termchart/image/bitmap.hpp =====
// bitmap.hpp —— 彩色像素缓冲（宽 / 高 / 通道 / 像素数组 / 行对齐）
//
// 这是"图片"在库里的通用表示：protocol/（sixel/kitty/iterm2）与 image/png 都
// 面向它。通道数为 3（RGB）或 4（RGBA）；行按通道紧致对齐，row_bytes() 即步长。
// 标量强度场是另一种东西，见 image/field.hpp（raster 的输入）。
// 纯值类型，列入 header_only。

#include <algorithm>

namespace termchart::image {

class Bitmap {
public:
    Bitmap() = default;
    Bitmap(std::uint32_t width, std::uint32_t height, std::uint8_t channels = 4) {
        resize(width, height, channels);
    }

    void resize(std::uint32_t width, std::uint32_t height, std::uint8_t channels = 4) {
        w_ = width;
        h_ = height;
        channels_ = (channels == 3 || channels == 4) ? channels : 4;
        pixels_.assign(static_cast<std::size_t>(row_bytes()) * static_cast<std::size_t>(h_), 0);
    }

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return w_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return h_; }
    [[nodiscard]] constexpr std::uint8_t channels() const noexcept { return channels_; }
    // 行步长（字节）：行对齐后的每行字节数。
    [[nodiscard]] constexpr std::uint32_t row_bytes() const noexcept {
        return w_ * static_cast<std::uint32_t>(channels_);
    }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return pixels_.size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return pixels_.empty(); }

    [[nodiscard]] std::uint8_t* data() noexcept { return pixels_.data(); }
    [[nodiscard]] const std::uint8_t* data() const noexcept { return pixels_.data(); }

    // 越界返回 0；写越界静默忽略。
    [[nodiscard]] std::uint8_t at(std::uint32_t x, std::uint32_t y,
                                  std::uint8_t channel = 0) const noexcept {
        if (x >= w_ || y >= h_ || channel >= channels_) {
            return 0;
        }
        return pixels_[static_cast<std::size_t>(y) * row_bytes() +
                       static_cast<std::size_t>(x) * channels_ + channel];
    }

    void set(std::uint32_t x, std::uint32_t y, std::uint8_t channel, std::uint8_t value) noexcept {
        if (x >= w_ || y >= h_ || channel >= channels_) {
            return;
        }
        pixels_[static_cast<std::size_t>(y) * row_bytes() +
                static_cast<std::size_t>(x) * channels_ + channel] = value;
    }

    void fill(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept {
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                set(x, y, 0, r);
                set(x, y, 1, g);
                set(x, y, 2, b);
                if (channels_ == 4) {
                    set(x, y, 3, a);
                }
            }
        }
    }

    [[nodiscard]] bool contains(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < w_ && y < h_;
    }

    // ---- GUI / 合成侧常用的原语（终端侧也用得上：sixel 缩略、PNG 截图拼版）----

    // 局部填充，越界部分裁剪。
    void fill_rect(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                   std::uint8_t r, std::uint8_t g, std::uint8_t b,
                   std::uint8_t a = 255) noexcept {
        for (std::uint32_t row = y; row < y + h && row < h_; ++row) {
            for (std::uint32_t col = x; col < x + w && col < w_; ++col) {
                set(col, row, 0, r);
                set(col, row, 1, g);
                set(col, row, 2, b);
                if (channels_ == 4) {
                    set(col, row, 3, a);
                }
            }
        }
    }

    // 把 src 以左上角 (dx, dy) 叠加到本图，源 alpha 做 source-over 混合。
    // 偏移用**带符号**类型：GUI 里"有一部分在窗口外"是常态，越界裁剪即可。
    void blit_over(const Bitmap& src, int dx, int dy) noexcept {
        if (src.empty() || empty() || src.channels() < 4) {
            return;  // 无 alpha 的源无法混合，请先 with_alpha()
        }
        for (std::uint32_t sy = 0; sy < src.height(); ++sy) {
            const int ty = dy + static_cast<int>(sy);
            if (ty < 0 || ty >= static_cast<int>(h_)) {
                continue;
            }
            for (std::uint32_t sx = 0; sx < src.width(); ++sx) {
                const int tx = dx + static_cast<int>(sx);
                if (tx < 0 || tx >= static_cast<int>(w_)) {
                    continue;
                }
                const double sa = src.at(sx, sy, 3) / 255.0;
                if (sa <= 0.0) {
                    continue;
                }
                const double da = channels_ == 4 ? at(static_cast<std::uint32_t>(tx),
                                                     static_cast<std::uint32_t>(ty), 3) /
                                                      255.0
                                                 : 1.0;
                const double out_a = sa + da * (1.0 - sa);
                for (std::uint32_t ch = 0; ch < 3; ++ch) {
                    const double sc = src.at(sx, sy, static_cast<std::uint8_t>(ch)) / 255.0;
                    const double dc = at(static_cast<std::uint32_t>(tx),
                                         static_cast<std::uint32_t>(ty),
                                         static_cast<std::uint8_t>(ch)) /
                                      255.0;
                    const double blended =
                        out_a > 0.0 ? (sc * sa + dc * da * (1.0 - sa)) / out_a : 0.0;
                    set(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty),
                        static_cast<std::uint8_t>(ch),
                        static_cast<std::uint8_t>(blended * 255.0 + 0.5));
                }
                if (channels_ == 4) {
                    set(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty), 3,
                        static_cast<std::uint8_t>(out_a * 255.0 + 0.5));
                }
            }
        }
    }

    // 最近邻缩放：**不做插值**，避免引入原图没有的颜色（调色板/像素画场景需要）。
    [[nodiscard]] Bitmap scaled_to(std::uint32_t width, std::uint32_t height) const {
        Bitmap out(width, height, channels_);
        if (out.empty() || empty() || w_ == 0 || h_ == 0) {
            return out;
        }
        for (std::uint32_t y = 0; y < height; ++y) {
            const std::uint32_t sy =
                std::min(h_ - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * h_ /
                                                             height));
            for (std::uint32_t x = 0; x < width; ++x) {
                const std::uint32_t sx =
                    std::min(w_ - 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * w_ /
                                                                 width));
                for (std::uint8_t ch = 0; ch < channels_; ++ch) {
                    out.set(x, y, ch, at(sx, sy, ch));
                }
            }
        }
        return out;
    }

    // 通道转换：3 → 4（补 alpha），4 → 3（丢 alpha）。已是目标通道数时原样返回。
    [[nodiscard]] Bitmap with_alpha(std::uint8_t alpha = 255) const {
        if (channels_ == 4) {
            return *this;
        }
        Bitmap out(w_, h_, 4);
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                out.set(x, y, 0, at(x, y, 0));
                out.set(x, y, 1, at(x, y, 1));
                out.set(x, y, 2, at(x, y, 2));
                out.set(x, y, 3, alpha);
            }
        }
        return out;
    }

    [[nodiscard]] Bitmap without_alpha() const {
        if (channels_ == 3) {
            return *this;
        }
        Bitmap out(w_, h_, 3);
        for (std::uint32_t y = 0; y < h_; ++y) {
            for (std::uint32_t x = 0; x < w_; ++x) {
                out.set(x, y, 0, at(x, y, 0));
                out.set(x, y, 1, at(x, y, 1));
                out.set(x, y, 2, at(x, y, 2));
            }
        }
        return out;
    }

    [[nodiscard]] bool operator==(const Bitmap& other) const noexcept {
        return w_ == other.w_ && h_ == other.h_ && channels_ == other.channels_ &&
               pixels_ == other.pixels_;
    }
    [[nodiscard]] bool operator!=(const Bitmap& other) const noexcept {
        return !(*this == other);
    }

private:
    std::uint32_t w_ = 0;
    std::uint32_t h_ = 0;
    std::uint8_t channels_ = 4;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace termchart::image

// ===== include/termchart/image/bmp.hpp =====
// bmp.hpp —— BMP 解码（自主实现，零依赖；输出 RGBA）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 文件头 'BM' + BITMAPINFOHEADER（40 字节）与后续更长的 V4/V5 头；
//   ✅ 位深 1 / 4 / 8（调色板，BI_RGB）、24（BGR）、32（BGRX 或 BITFIELDS）、
//      16（BITFIELDS 或默认 RGB555）；
//   ✅ 压缩 0（BI_RGB）与 3（BI_BITFIELDS，带 3~4 个 DWORD 掩码）；
//   ✅ 行序 bottom-up（height 为正）与 top-down（height 为负）；
//   ✅ 行 4 字节对齐、调色板缺省色表（bpp ≤ 8 且 ciColors 为 0 时按 2^bpp 项）；
//   ⛔ 不支持：RLE4/RLE8 压缩（返回 Unsupported）、OS/2 的 12 字节旧头
//      （返回 Unsupported）、每通道超过 8 位的 16bpp 掩码组合（按能处理的
//      掩码解析，掩码不连续/不覆盖时返回 ParseError）。
// 错误：头结构非法/尺寸越界/掩码不可解析 → ParseError；魔数或压缩不支持 →
//      Unsupported。



namespace termchart::image {

// 解码 BMP 为 RGBA。data 至少覆盖文件头声明的全部字节；输出 4 通道，
// 无 alpha 信息的来源位（24bpp）一律填 255。
[[nodiscard]] TERMCHART_API Result<Bitmap> decode_bmp(const std::uint8_t* data,
                                                      std::size_t size);

[[nodiscard]] TERMCHART_API inline Result<Bitmap> decode_bmp(std::string_view bytes) {
    return decode_bmp(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

[[nodiscard]] TERMCHART_API inline Result<Bitmap> decode_bmp(const std::vector<std::uint8_t>& bytes) {
    return decode_bmp(bytes.data(), bytes.size());
}

}  // namespace termchart::image

// ===== include/termchart/image/decoder.hpp =====
// decoder.hpp —— 图像解码入口（按魔数识别格式后分发）
//
// 为什么先"识别再分发"：调用方拿到的常常是一段来路不明的字节（剪贴板、文件、
// 网络），先嗅探格式再交给对应解码器，比让每个解码器各自试一遍要清楚。
//
// 目前 **PNG（自带 inflate）与 BMP（自带解析）是原生零依赖解码器**；JPEG / GIF /
// WebP 走 vendored `stb_image`（可选依赖 `TERMCHART_ENABLE_STB_IMAGE`，默认关，
// 关时如实 `Unsupported`）。所有"识别得出但无解码器"的情况都返回
// `ErrorCode::Unsupported`，不做"猜着解"。解码器注册表等到第四种格式落地再引入
// （与 raster/chart 同构），现在引入只会是空壳。



namespace termchart::image {

enum class ImageFormat : std::uint8_t {
    Unknown = 0,
    Png,
    Jpeg,
    Gif,
    Bmp,
    WebP,
};

// 只看魔数（不看扩展名）。数据不足或对不上任何魔数返回 Unknown。
[[nodiscard]] TERMCHART_API ImageFormat sniff_image_format(const std::uint8_t* data,
                                                           std::size_t size) noexcept;

[[nodiscard]] TERMCHART_API const char* image_format_name(ImageFormat format) noexcept;

// 识别 + 解码。识别不出返回 `ErrorCode::ParseError`；识别出但无解码器返回
// `ErrorCode::Unsupported`。
[[nodiscard]] TERMCHART_API Result<Bitmap> decode_image(const std::uint8_t* data,
                                                        std::size_t size);

[[nodiscard]] inline Result<Bitmap> decode_image(std::string_view bytes) {
    return decode_image(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace termchart::image

// ===== include/termchart/image/field.hpp =====
// field.hpp —— 光栅器消费的标量强度场（FloatField）
//
// raster/ 的所有光栅器都把"连续标量场"当成输入：场里每格是一个 0..1 的强度
// （密度 / 高度 / 命中次数归一化后的值）。它不属于"位图"（那是 image/bitmap.hpp
// 的彩色像素缓冲），因此单独成文件，避免一个 bitmap.hpp 既当像素又当标量。
// 纯值类型（无虚函数、无堆外状态），列入 header_only。


namespace termchart::image {

// 行主序的 2D 强度场，值约定在 [0,1]。越界访问返回 0（方便光栅器按 subpixel
// 网格采样时忽略边缘外）。
class FloatField {
public:
    FloatField() = default;
    FloatField(std::uint32_t width, std::uint32_t height, float value = 0.0f) {
        resize(width, height, value);
    }

    void resize(std::uint32_t width, std::uint32_t height, float value = 0.0f) {
        w_ = width;
        h_ = height;
        data_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), value);
    }

    [[nodiscard]] constexpr std::uint32_t width() const noexcept { return w_; }
    [[nodiscard]] constexpr std::uint32_t height() const noexcept { return h_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return data_.empty(); }

    // 越界返回 0.0f（const 只读路径，光栅器依赖此行为）。
    [[nodiscard]] float at(std::uint32_t x, std::uint32_t y) const noexcept {
        if (x >= w_ || y >= h_) {
            return 0.0f;
        }
        return data_[static_cast<std::size_t>(y) * w_ + x];
    }

    // 写路径：调用方须保证在界内。
    [[nodiscard]] float& at(std::uint32_t x, std::uint32_t y) noexcept {
        return data_[static_cast<std::size_t>(y) * w_ + x];
    }

    void set(std::uint32_t x, std::uint32_t y, float value) noexcept {
        if (x < w_ && y < h_) {
            data_[static_cast<std::size_t>(y) * w_ + x] = value;
        }
    }

    void fill(float value) noexcept { data_.assign(data_.size(), value); }

    [[nodiscard]] const float* data() const noexcept { return data_.data(); }

private:
    std::uint32_t w_ = 0;
    std::uint32_t h_ = 0;
    std::vector<float> data_;
};

}  // namespace termchart::image

// ===== include/termchart/image/convert.hpp =====
// convert.hpp —— 位图与标量场的互转（GUI 与终端共用）
//
// 本文件是**规格清单之外的补充**，理由：GUI 侧的图片控件要把 RGBA 位图转成标量场
// 再喂给 raster/（盲文/密度预览），终端侧的图像预览走的也是同一条转换。把唯一实现
// 放在 image/ 里，比让每个上层各写一遍亮度公式可靠。
//
// 两点约定（刻意如此，别再"优化"）：
//   * 亮度用 Rec. 601 系数（0.299 / 0.587 / 0.114），**不做 gamma 校正** ——
//     GUI 与终端必须看到同一个亮度，加 gamma 只会让两边不一致且难以解释；
//   * alpha 不参与亮度，而是**乘进结果**：全透明像素在场里就是 0，
//     不会因为"透明黑"而变成一块黑斑。
//
// 纯函数、无状态；列入 header-only 清单。



namespace termchart::image {

namespace detail {
inline constexpr float kLumaR = 0.299f;
inline constexpr float kLumaG = 0.587f;
inline constexpr float kLumaB = 0.114f;
}  // namespace detail

// 位图 → 标量场（尺寸相同）。值域 [0,1]。
[[nodiscard]] inline FloatField luminance_field(const Bitmap& bitmap) {
    FloatField field(bitmap.width(), bitmap.height(), 0.0f);
    for (std::uint32_t y = 0; y < bitmap.height(); ++y) {
        for (std::uint32_t x = 0; x < bitmap.width(); ++x) {
            const float luma = detail::kLumaR * static_cast<float>(bitmap.at(x, y, 0)) +
                               detail::kLumaG * static_cast<float>(bitmap.at(x, y, 1)) +
                               detail::kLumaB * static_cast<float>(bitmap.at(x, y, 2));
            float value = luma / 255.0f;
            if (bitmap.channels() == 4) {
                value *= static_cast<float>(bitmap.at(x, y, 3)) / 255.0f;
            }
            field.set(x, y, std::clamp(value, 0.0f, 1.0f));
        }
    }
    return field;
}

// 位图 → 标量场（先最近邻缩放到目标尺寸，避免上游为了预览而额外做一次缩放）。
[[nodiscard]] inline FloatField luminance_field(const Bitmap& bitmap, std::uint32_t width,
                                               std::uint32_t height) {
    if (bitmap.width() == width && bitmap.height() == height) {
        return luminance_field(bitmap);
    }
    return luminance_field(bitmap.scaled_to(width, height));
}

// 标量场 → 双色位图：按阈值决定取 on 还是 off。
// alpha_from_value 为真时 alpha 取场值（把场当遮罩用），否则 alpha 恒为 255。
[[nodiscard]] inline Bitmap field_to_bitmap(const FloatField& field, Rgb on, Rgb off,
                                            double threshold = 0.5,
                                            bool alpha_from_value = false) {
    Bitmap out(field.width(), field.height(), 4);
    for (std::uint32_t y = 0; y < field.height(); ++y) {
        for (std::uint32_t x = 0; x < field.width(); ++x) {
            const float value = field.at(x, y);
            const bool lit = static_cast<double>(value) >= threshold;
            const Rgb color = lit ? on : off;
            out.set(x, y, 0, color.r);
            out.set(x, y, 1, color.g);
            out.set(x, y, 2, color.b);
            const auto alpha = alpha_from_value
                                   ? static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) *
                                                               255.0f + 0.5f)
                                   : static_cast<std::uint8_t>(255);
            out.set(x, y, 3, alpha);
        }
    }
    return out;
}

}  // namespace termchart::image

// ===== include/termchart/image/png.hpp =====
// png.hpp —— PNG 解码（8/16 位、非隔行；输出 RGBA）
//
// 与 png_encode.hpp 对称：编码器自带（零依赖），解码器也自带——同样是为了不把
// zlib 变成硬依赖。解码器包含一个完整的 **inflate**（stored / 固定 Huffman /
// 动态 Huffman 三种块），因此能读任意常规 PNG，而不只是本库自己写出的那种。
//
// 支持范围与刻意的限制（都在返回值里明确报错，不静默出错图）：
//   * 位深：1 / 2 / 4 / 8 / 16；
//   * 颜色类型：0 灰度、2 真彩、3 调色板、4 灰度+alpha、6 真彩+alpha；
//   * tRNS：调色板透明与"单一透明色"（灰度/真彩）都处理；
//   * **不支持隔行（Adam7）**：返回 `ErrorCode::Unsupported`；
//   * 16 位样本**取高字节**降为 8 位（终端与 GUI 都用不到 16 位深度）；
//   * 每个 chunk 的 CRC 都要对，错一个即返回 `ErrorCode::ParseError`。
// 输出恒为 4 通道 RGBA（调用方要 RGB 用 `Bitmap::without_alpha()`）。



namespace termchart::image {

// 单张 PNG 的原始扫描线解压后不得大于此值（防止畸形头导致天量分配）。
inline constexpr std::uint64_t kPngMaxRawBytes = 512ull * 1024ull * 1024ull;

[[nodiscard]] TERMCHART_API Result<Bitmap> decode_png(const std::uint8_t* data,
                                                      std::size_t size);

[[nodiscard]] TERMCHART_API inline Result<Bitmap> decode_png(std::string_view bytes) {
    return decode_png(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace termchart::image

// ===== include/termchart/image/png_encode.hpp =====
// png_encode.hpp —— 最小 PNG 编码器（8 位 RGB / RGBA，非隔行）
//
// 为什么自带而不是用 libpng/zlib：本仓库的硬约束是"每个外部依赖都必须可缺失"
// （`TERMCHART_ENABLE_PNG` 默认 OFF）。这里只实现 PNG 必需的最小集合：
//
//   * CRC-32（IEEE 802.3）—— 每个 chunk 的校验字段；
//   * Adler-32 —— zlib 流尾校验；
//   * zlib 流里的 **stored（未压缩）deflate 块** —— 不需要字典与霍夫曼编码，
//     体积比压缩版大约多 0.03%，但完全合规，任何解码器都能读。
//
// 将来接入真 deflate（或可选 zlib）时只换内部实现，公开接口不变。
// 代价说清楚：stored 块的输出体积 ≈ 原始像素 + 每 65535 字节 5 字节开销，
// 用于截图/内联图像足够，不适合当压缩格式用。



namespace termchart::image {

// PNG chunk 用的 CRC-32（标准校验值：crc32("123456789") == 0xCBF43926）。
TERMCHART_API std::uint32_t crc32(const std::uint8_t* data, std::size_t len) noexcept;

// zlib 流尾用的 Adler-32（标准校验值：adler32("123456789") == 0x091E01DE）。
TERMCHART_API std::uint32_t adler32(const std::uint8_t* data, std::size_t len) noexcept;

// 编码为完整 PNG 字节流：签名 + IHDR + IDAT + IEND。
// 颜色类型按位图通道数选（3 → truecolor 2，4 → truecolor+alpha 6），位深恒为 8。
// 宽或高为 0 的空位图返回空串（不产出非法文件）。
TERMCHART_API std::string encode_png(const Bitmap& bitmap);

}  // namespace termchart::image

// ===== include/termchart/image/ttf_font.hpp =====
// ttf_font.hpp —— 自主 TTF 解析（sfnt 目录 + cmap + glyf/loca + hmtx + 二次
// 贝塞尔轮廓光栅化 + glyph cache），输出 8-bit 灰度位图
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ sfnt 版本 0x00010000（TrueType）与 'true' 变体；'ttcf' 集合取第一个字体；
//   ✅ cmap format 4（BMP，含代理对段）与 format 12（全 Unicode），平台子表
//      选择序：(3,10) > (3,1) > (0,x) > 任意 format 4/12；
//   ✅ glyf 简单轮廓（on/off 曲线 + REPEAT 旗标 + 隐含中点）与复合轮廓
//      （ARGS_ARE_XY_VALUES / 缩放矩阵 / 分量变换，深度上限 8，循环引用防护）；
//   ✅ loca 短/长格式（head.indexToLocFormat）；hhea 行高度量；hmtx 步进宽；
//   ✅ 光栅化：二次贝塞尔自适应展平 → 扫描线非零环绕 → 4x4 超采样 → alpha；
//   ✅ glyph cache：按（码点, 0.25px 量化后的尺寸）缓存位图。
//   ⛔ 不执行 hinting 字节码（指令区正确跳过但不执行）；不解析可变字体
//      （fvar/gvar）；不解析位图字形表（EBDT/CBDT → 该字形回退 .notdef）；
//      不解析 OpenType CFF（'CFF ' 轮廓，无 'glyf' → load 即 Unsupported）。
// 错误：表缺失、目录/表体越界读取 → ParseError；'CFF ' 无 glyf → Unsupported；
// 复合轮廓过深或表长度声明溢出 → OutOfRange。



namespace termchart::image {

// 行高度量（像素单位，按 pixel_size 从字体单位缩放）。
struct TtfMetrics {
    double ascent = 0.0;
    double descent = 0.0;  // 通常是负值（y 向上坐标系）
    double line_gap = 0.0;
    // 建议行距：ascent - descent + line_gap。
    [[nodiscard]] constexpr double line_height() const noexcept {
        return ascent - descent + line_gap;
    }
};

// 单个字形的位图。bitmap 为 4 通道 RGBA：RGB 恒为 255（白）、**A 通道是
// 覆盖率**（0 透明 → 255 全覆盖）——Bitmap 只支持 3/4 通道，单通道灰度无处
// 安放，用 alpha 承载覆盖率对合成（文字上色）最方便；
// left/top 是位图左上角相对**笔触原点**的像素偏移（y 向下为正）；
// advance_x 是排版步进宽度（像素）。无轮廓的字形 bitmap 为空（0x0）。
struct TtfGlyph {
    Bitmap bitmap;
    std::int32_t left = 0;
    std::int32_t top = 0;
    double advance_x = 0.0;
};

// 已解析的 TrueType 字体。拷贝共享同一份解析结果（表目录 + cmap 映射 +
// cache），拷贝便宜；glyph() 带 cache 且无锁——多线程请每线程独立 load，
// 或只调 advance()。
class TtfFont {
public:
    static Result<TtfFont> load(const std::uint8_t* data, std::size_t size) noexcept;
    static Result<TtfFont> load(std::string_view bytes) noexcept;
    static Result<TtfFont> load(const std::vector<std::uint8_t>& bytes) noexcept {
        return load(bytes.data(), bytes.size());
    }

    TtfFont() = default;
    TtfFont(TtfFont&&) noexcept = default;
    TtfFont& operator=(TtfFont&&) noexcept = default;
    TtfFont(const TtfFont&) = default;
    TtfFont& operator=(const TtfFont&) = default;
    ~TtfFont() = default;

    // 字体在指定像素大小下的行高度量。
    [[nodiscard]] TtfMetrics metrics(double pixel_size) const noexcept;

    // 取字形位图（带 cache；码点缺失回退 .notdef，再不行回退空白位图）。
    // 按值返回（Result 不能承载引用）；pixel_size 按 0.25px 量化后查 cache。
    [[nodiscard]] Result<TtfGlyph> glyph(char32_t codepoint, double pixel_size) noexcept;

    // 步进宽度（像素），不生成位图——文本测量/排版用。
    [[nodiscard]] double advance(char32_t codepoint, double pixel_size) const noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    explicit TtfFont(std::shared_ptr<Impl> impl) noexcept;
};

}  // namespace termchart::image

// ===== include/termchart/raster/rasterizer.hpp =====
// rasterizer.hpp —— 子像素光栅化基类与公共契约
//
// 终端字符是低分辨率网格，但一个字符格能塞下多个"子像素"：盲文 2×4、半块 1×2、
// 象限 2×2。光栅器把一个标量场（image::FloatField）压进这种子像素网格，产出
// CellBuffer——同一份场，换光栅器就是换分辨率/风格，GUI 控件与终端后端复用同一套。
//
// 不变量：
//   * rasterize 无副作用、不抛异常、不写 stdout；
//   * 输出的 CellBuffer 尺寸 = field / (subpixel_w, subpixel_h)；
//   * 单色光栅器：点亮处用 opts.fg，熄灭处用 opts.bg（invert 交换二者并反相）。



namespace termchart::raster {

enum class RasterKind {
    Braille,    // 2×4，单色，分辨率最高
    HalfBlock,  // 1×2，上下双色
    Quadrant,   // 2×2，单色
    Ascii,      // 1×1，密度字符，最朴素
    Block,      // 1×1，八级竖条，单色
    Sextant,    // 2×3，六分仪字符（Unicode 1FB00 区）
};

struct RasterOptions {
    Color fg = Color::ansi16(7);    // 点亮色
    Color bg = Color::ansi16(0);    // 熄灭色
    bool invert = false;            // 反相（照片负片效果）
    double threshold = 0.5;         // 二进制子像素的开/关阈值
};

class Rasterizer {
public:
    virtual ~Rasterizer();
    [[nodiscard]] virtual CellBuffer rasterize(const image::FloatField& field,
                                              const RasterOptions& opts) const = 0;
    [[nodiscard]] virtual std::uint32_t subpixel_width() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t subpixel_height() const noexcept = 0;
    [[nodiscard]] virtual RasterKind kind() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

// 一次性光栅化：按 kind 选实现，缺省选项。无对应实现时返回空 buffer。
TERMCHART_API CellBuffer rasterize(const image::FloatField& field, RasterKind kind,
                                   const RasterOptions& opts = {});

TERMCHART_API const char* raster_name(RasterKind kind) noexcept;

}  // namespace termchart::raster

// ===== include/termchart/raster/ascii.hpp =====
// ascii.hpp —— 密度字符 1×1 光栅器（最朴素、零依赖）
//
// 用 . : - = + * # % @ 把强度映射成由淡到浓的 ASCII 字符。适合纯文本环境、
// 不能显示块字符的终端，以及 golden 快照的可读对照。


namespace termchart::raster {

class AsciiRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 1; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Ascii; }
    [[nodiscard]] const char* name() const noexcept override { return "ascii"; }
};

}  // namespace termchart::raster

// ===== include/termchart/raster/auto_select.hpp =====
// auto_select.hpp —— 按终端能力自动挑光栅器
//
// 注意：本文件**不**依赖 term/（term 是更高层），能力以扁平布尔传入，与
// TerminalCaps 解耦——这样光栅器层就能在不知晓终端对象的情况下完成选择。


namespace termchart::raster {

// 终端对各子像素光栅器的支持度（由调用方从 TerminalCaps 折算而来）。
struct RasterCaps {
    bool braille = false;
    bool halfblock = false;
    bool quadrant = false;
    bool block = false;
    bool ascii = false;
    bool sextant = false;  // 六分仪字符（Unicode 1FB00），默认关闭，调用方按需开启
    bool sixel = false;  // 预留：真彩色图像协议，光栅器层不管
};

// 首选若支持则用之；否则按保真度（盲文 > 象限 > 半块 > 竖条 > ascii）降级；
// 全不支持兜底 ascii（任何终端都认）。
TERMCHART_API RasterKind pick_raster(const RasterCaps& caps,
                                     RasterKind preferred = RasterKind::Braille) noexcept;

}  // namespace termchart::raster

// ===== include/termchart/raster/block.hpp =====
// block.hpp —— 八级竖条 1×1 光栅器
//
// 每格按平均强度取 1/8 高度的填充块（▁▂▃▄▅▆▇█）。分辨率最低但零子像素开销，
// 适合大色块、进度条、密度概览。invert 翻转强度（照片负片）。


namespace termchart::raster {

class BlockRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 1; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Block; }
    [[nodiscard]] const char* name() const noexcept override { return "block"; }
};

}  // namespace termchart::raster

// ===== include/termchart/raster/braille.hpp =====
// braille.hpp —— 盲文 2×4 子像素光栅器（分辨率最高）
//
// 每个终端格 = 2 列 × 4 行的点阵，点亮的点拼成 U+2800 系盲文字形。子像素→点
// 位的几何约定全部在 detail/braille_table.hpp，本文件只做"采样 + 查表"。


namespace termchart::raster {

class BrailleRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 4; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Braille; }
    [[nodiscard]] const char* name() const noexcept override { return "braille"; }
};

}  // namespace termchart::raster

// ===== include/termchart/raster/halfblock.hpp =====
// halfblock.hpp —— 半块 1×2 光栅器（上下双色）
//
// 一个格 = 上、下两半。▀ 上半亮、▄ 下半亮、█ 全亮、空格全灭。因为半块字符让
// 上/下两半分别取前景/背景，单格即可表现两种颜色——这是比盲文更"彩色"的选择。


namespace termchart::raster {

class HalfBlockRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 1; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 2; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::HalfBlock; }
    [[nodiscard]] const char* name() const noexcept override { return "halfblock"; }
};

}  // namespace termchart::raster

// ===== include/termchart/raster/quadrant.hpp =====
// quadrant.hpp —— 象限块 2×2 光栅器
//
// 一个格 = 2×2 子像素。标准象限块字符（U+2596–U+259F）只覆盖 10 种图案，
// 缺 3 点亮的组合，所以那种情况改画"唯一的熄灭象限"为熄灭色、其余为点亮色——
// 视觉效果等同 3 点亮。几何与点位约定见 src/raster/quadrant.cpp 的映射表。


namespace termchart::raster {

class QuadrantRasterizer final : public Rasterizer {
public:
    [[nodiscard]] CellBuffer rasterize(const image::FloatField& field,
                                       const RasterOptions& opts) const override;
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 2; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Quadrant; }
    [[nodiscard]] const char* name() const noexcept override { return "quadrant"; }
};

}  // namespace termchart::raster

// ===== include/termchart/raster/registry.hpp =====
// registry.hpp —— 光栅器注册表（可扩展、构造期冻结）
//
// 用 function-local static 的 instance() 保证"构造前注册、构造后只读"的注册表
// 契约（见 docs/design/registry.md）。
//
// 两类来源：
//   * 内置种类（Braille/HalfBlock/...）：硬编码在编译单元里，create(kind) 直接给；
//   * 自定义种类：通过 register_raster(name, creator) 登记，create(name) 查表。
// 后者是**扩展点**——新增光栅器不必改本模块的枚举或 switch（见
// tests/integration/replace_raster_custom.cpp）。



namespace termchart::raster {

class RasterRegistry {
public:
    using Creator = std::function<std::unique_ptr<Rasterizer>()>;

    // 全局唯一实例；首次调用即冻结，之后不可增删。
    static RasterRegistry& instance() noexcept;

    RasterRegistry(const RasterRegistry&) = delete;
    RasterRegistry& operator=(const RasterRegistry&) = delete;

    // 扩展点：登记一个自定义光栅器工厂（重复名字以最后一次为准）。
    void register_raster(std::string_view name, Creator creator);
    [[nodiscard]] bool supports_name(std::string_view name) const;

    // 按种类造一个光栅器；未实现返回 nullptr。
    [[nodiscard]] std::unique_ptr<Rasterizer> create(RasterKind kind) const;
    // 按名字造：先查自定义登记，再查内置名字；未知返回 nullptr。
    [[nodiscard]] std::unique_ptr<Rasterizer> create(std::string_view name) const;

    [[nodiscard]] bool supports(RasterKind kind) const noexcept;
    [[nodiscard]] static const char* name(RasterKind kind) noexcept;

private:
    RasterRegistry() = default;
    std::map<std::string, Creator, std::less<>> custom_;
};

}  // namespace termchart::raster

// ===== include/termchart/raster/sextant.hpp =====
// sextant.hpp —— 六分仪字符光栅器（2×3 子像素，Unicode 1FB00 区）
//
// 保真度介于象限(2×2=4) 与盲文(2×4=8) 之间，是终端字符图里"中等保真度"的一档。
// 每个字符格切成 2 列 × 3 行的 6 个 sextant 区域，按填充情况映射到 Unicode
// "Symbols for Legacy Computing" 的六分仪块（U+1FB00 起，码点 = 0x1FB00 + 6 位掩码；
// 位序约定：左上 0 / 左中 1 / 左下 2 / 右上 3 / 右中 4 / 右下 5）。


namespace termchart::raster {

class SextantRasterizer final : public Rasterizer {
public:
    [[nodiscard]] std::uint32_t subpixel_width() const noexcept override { return 2; }
    [[nodiscard]] std::uint32_t subpixel_height() const noexcept override { return 3; }
    [[nodiscard]] RasterKind kind() const noexcept override { return RasterKind::Sextant; }
    [[nodiscard]] const char* name() const noexcept override { return "sextant"; }
    CellBuffer rasterize(const image::FloatField& field,
                         const RasterOptions& opts) const override;
};

}  // namespace termchart::raster

// ===== include/termchart/network/socket.hpp =====
// socket.hpp —— 跨平台 TCP socket 抽象（network 模块，L7）
//
// 平台差异全部落在 src/network/socket_posix.cpp 与 socket_win32.cpp，
// 公开头**没有任何 #ifdef**（与 term/terminal.hpp 同哲学）。
//
// 契约：
//   * 不抛异常：失败返回 Result（IoError / PlatformError / InvalidArgument）；
//   * connect 超时由 poll/select **显式实现**（非阻塞 connect + 等待 + 恢复阻塞）
//     ——SO_SNDTIMEO 在 Linux 上对 connect() 无效，不能靠它；
//   * 单次读/写超时经 SO_RCVTIMEO / SO_SNDTIMEO 实现；
//   * DNS 解析（域名 → 地址）在 connect 内完成（进程级 TTL 缓存，见
//     src/network/dns_cache.cpp），调用方无需自己 getaddrinfo；
//   * 移动语义（PIMPL），拷贝删除——一个 socket 只有一份底层句柄。



namespace termchart::network {

struct SocketOptions {
    std::uint32_t connect_timeout_ms = 10000;  // 连接超时
    std::uint32_t io_timeout_ms = 30000;      // 单次读/写超时
    // 关 Nagle（TCP_NODELAY）。默认开：HTTP 请求-响应是"写请求→等响应"的
    // 两段式，不开会让小请求包与后续包在 40ms 级的 delayed-ACK/Nagle 交互里
    // 干等。需要批量吞吐的上层可关掉自行拼包。
    bool tcp_nodelay = true;
    // 开 SO_KEEPALIVE：io_timeout 长的连接可在半开（对端宕机）时被内核探针
    // 收敛，否则read/write 的超时要等满 io_timeout_ms 才报。默认关（探针周期
    // 以小时计，图表库的短连接等不到它）。
    bool keepalive = false;
};

// 已连接的 TCP socket。默认构造为无效句柄（valid() == false）。
class TcpSocket {
public:
    TcpSocket() noexcept;
    ~TcpSocket();

    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 连接 host:port。host 可以是 IPv4/IPv6 字面量或域名（内部 getaddrinfo）。
    // 失败：InvalidArgument（参数非法）/ IoError（连接失败）/ PlatformError。
    [[nodiscard]] static Result<TcpSocket> connect(std::string_view host,
                                                   std::uint16_t port,
                                                   const SocketOptions& options = {}) noexcept;

    // 读最多 buffer.size() 字节；返回实际读到的字节数（0 = 对端关闭）。
    // 超时返回 IoError。无效句柄返回 InvalidArgument。
    [[nodiscard]] Result<std::size_t> read(std::span<char> buffer) noexcept;

    // 写 data 的全部字节（循环发送直到写完或出错）。
    // 超时返回 IoError。无效句柄返回 InvalidArgument。
    [[nodiscard]] Result<std::size_t> write(std::span<const char> data) noexcept;

    void close() noexcept;

    [[nodiscard]] bool valid() const noexcept;

private:
    struct Impl;
    explicit TcpSocket(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart::network

// ===== include/termchart/network/http.hpp =====
// http.hpp —— HTTP/1.1 客户端（network 模块，L7）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ http:// 明文与 https://（TLS 走 network/tls 注册表的后端，fail-loud）；
//   ✅ 请求：任意 method、自定义头、请求体（Content-Length 自动补）；
//   ✅ 响应：状态行 + 响应头 + 响应体；体长按 Content-Length / chunked /
//      读到连接关闭三种规则解析（HEAD 与 204/304 按规范不收体）；
//   ✅ 重定向：301/302/303/307/308 跟 Location（默认上限 5 次；303 降为 GET）；
//   ✅ 每请求独立连接，发 `Connection: close`（库内不做连接池——图表库的
//      拉取场景一次一个 URL，池化是纯负担）；
//   ⛔ 不支持：HTTP/2、Cookie 管理、代理、摘要认证、请求体流式分块上传、
//      非 ASCII 头的原始字节透传（按 std::string 收）。
// 错误：连接失败 IoError；TLS 后端缺失/握手失败 Unsupported / IoError；
// 响应超限 OutOfRange；协议解析失败 ParseError。



namespace termchart::network::http {

// 解析后的 URL。path 含前导 '/' 与查询串；port == 0 表示按 scheme 取默认端口。
struct Uri {
    std::string scheme;  // "http" / "https"（小写）
    std::string host;    // 不含端口；IPv6 字面量保留方括号
    std::uint16_t port = 0;
    std::string path = "/";

    // 解析 URL。失败：InvalidArgument（空/缺 scheme/缺 host/端口非法/非 HTTP 系
    // scheme）。不做百分号解码——原样保留，交由调用方按需处理。
    [[nodiscard]] static Result<Uri> parse(std::string_view text) noexcept;

    // scheme 的有效默认端口（http=80，https=443；其余返回 0）。
    [[nodiscard]] std::uint16_t default_port() const noexcept;
};

// 请求。headers 由调用方给全（Host 与 Connection 由库补，调用方的同名项忽略）。
struct Request {
    std::string method = "GET";
    Uri uri;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

// 响应。headers 保持服务端给出的顺序，重复头不合并。
struct Response {
    std::uint16_t status = 0;
    std::string reason;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    // 按名字取响应头（大小写不敏感）；多个同名取第一个；没有返回空 view。
    [[nodiscard]] std::string_view header(std::string_view name) const noexcept;
    // Content-Type 的主体部分（去掉 ';' 后的参数）；没有返回空 view。
    [[nodiscard]] std::string_view content_type() const noexcept;
};

struct HttpOptions {
    SocketOptions socket;                        // 连接/IO 超时
    std::string tls_backend;                     // https 用的 TLS 后端名；空则随便一个已注册的
    std::size_t max_header_bytes = 64u * 1024u;  // 响应头体积上限
    std::size_t max_body_bytes = 16u * 1024u * 1024u;  // 响应体体积上限
    std::uint32_t max_redirects = 5;             // 重定向跟随后跟上限
};

// 执行一次请求-响应（阻塞；内部按 options.max_redirects 跟重定向）。
[[nodiscard]] TERMCHART_API Result<Response> execute(const Request& request,
                                                     const HttpOptions& options = {}) noexcept;

// 便捷入口：GET 一个 URL（等价于填好 method/uri 的 execute）。
[[nodiscard]] TERMCHART_API Result<Response> get(std::string_view url,
                                                 const HttpOptions& options = {}) noexcept;

}  // namespace termchart::network::http

// ===== include/termchart/network/tls/backend.hpp =====
// backend.hpp —— TLS 后端抽象接口（network/tls，L7）
//
// 可插拔契约（与 raster/chart/scale 注册表同构，见 docs/design/registry.md）：
//   * 内置后端（network/tls/builtin/，自主实现）与第三方后端（BearSSL/mbedTLS…）
//     实现同一接口，经 TlsRegistry 注册，调用方按名字取用；
//   * 命名项缺失时**显式报错**（fail-loud），不静默降级——请求 "builtin" 而
//     未链接该后端即 Unsupported，绝不偷偷退回明文；
//   * 后端只做"客户端 TLS 记录层"：握手 + 加解密读写 + close_notify。
//     证书链验证由后端负责（hostname 同时用于 SNI 与主机名校验）。



namespace termchart::network::tls {

class TlsBackend {
public:
    // 声明与定义分离：虚表锚点放 src/network/tls/backend.cpp（基类也要有 cpp）。
    virtual ~TlsBackend();

    // 客户端握手：对已连接的 socket 完成 TLS 握手。
    // hostname 用于 SNI 扩展与证书主机名校验。失败返回 IoError/ParseError/
    // Unsupported（证书不受信等，具体语义由后端定义，消息可经 diagnostic 观察）。
    // 只回报成败 → Status（core/expected.hpp 约定，不用 Result<void>：
    // Expected<void,E> 依赖"void 形参构造函数"的方言容忍度，不跨编译器可靠）。
    [[nodiscard]] virtual Status handshake(TcpSocket& socket,
                                          std::string_view hostname) noexcept = 0;

    // 记录层读：解密后最多 buffer.size() 字节明文；0 = 对端关闭。
    [[nodiscard]] virtual Result<std::size_t> read(TcpSocket& socket,
                                                  std::span<char> buffer) noexcept = 0;

    // 记录层写：加密后发送全部字节。
    [[nodiscard]] virtual Result<std::size_t> write(TcpSocket& socket,
                                                   std::span<const char> data) noexcept = 0;

    // 关闭 TLS 会话（best-effort 发 close_notify；不关闭底层 socket）。
    virtual void shutdown(TcpSocket& socket) noexcept = 0;

    // 后端名字（如 "builtin"、"bearssl"），用于注册表键与诊断。
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

}  // namespace termchart::network::tls

// ===== include/termchart/network/tls/registry.hpp =====
// registry.hpp —— TLS 后端注册表（可扩展、构造期冻结）
//
// 与 raster/registry.hpp 同构（docs/design/registry.md 的口径）：
//   * instance() 一律 function-local static；
//   * 构造后只读（拷贝/赋值删除）；注册的唯一安全时机是 App 构造前；
//   * 注册的是**工厂**（每次 create 给新实例），不是原型；
//   * 查询：supports(name) / create(name)；未知名字返回 nullptr，
//     由调用方显式报错（fail-loud，不静默降级——绝不偷偷退回明文）。



namespace termchart::network::tls {

class TlsRegistry {
public:
    using Creator = std::function<std::unique_ptr<TlsBackend>()>;

    // 全局唯一实例；首次调用即冻结，之后不可增删。
    static TlsRegistry& instance() noexcept;

    TlsRegistry(const TlsRegistry&) = delete;
    TlsRegistry& operator=(const TlsRegistry&) = delete;
    TlsRegistry(TlsRegistry&&) = default;
    TlsRegistry& operator=(TlsRegistry&&) = default;

    // 扩展点：登记一个 TLS 后端工厂（重复名字以最后一次为准）。
    void register_backend(std::string_view name, Creator creator);

    // 是否已有该名字的后端工厂。
    [[nodiscard]] bool supports(std::string_view name) const;

    // 已登记的后端名字（按字典序），用于"空名字时挑默认"与诊断列举。
    [[nodiscard]] std::vector<std::string> names() const;

    // 按名字造一个后端实例；未登记返回 nullptr（调用方负责显式报错）。
    [[nodiscard]] std::unique_ptr<TlsBackend> create(std::string_view name) const;

private:
    TlsRegistry() = default;

    // 造一份登记好内置后端的注册表（见 registry.cpp 的"不能重入 instance()"注释）。
    [[nodiscard]] static TlsRegistry with_builtins();

    std::map<std::string, Creator, std::less<>> creators_;
};

}  // namespace termchart::network::tls

// ===== include/termchart/network/tls/schannel.hpp =====
// schannel.hpp —— Windows 原生 TLS 后端（SChannel / Schannel SSP）
//
// 平台后端之一（另一支是 OpenSSL，随可选依赖编译）：
//   * 实现 TlsBackend（握手 + 记录层加解密 + close_notify）；
//   * 一个后端实例服务**一条连接**（握手掌状态 CtxtHandle 在实例里）——
//     每次 create() 拿新实例，勿跨连接复用；
//   * 证书链与主机名**手动验证**（SCH_CRED_MANUAL_CRED_VALIDATION +
//     CertGetCertificateChain / CertVerifyCertificateChainPolicy(SSL)），
//     失败即握手失败，绝不接受"链错了但握手过了"的连接；
//   * 注册：registry.cpp 构造注册表时直接把本后端登记进去（显式引用
//     create_backend，避免"静态库里无人引用的 TU 被链接器丢弃"陷阱）；
//     register_backend() 留给需要显式控制注册时机的调用方。
//
// 已知环境限制：某些沙箱/策略加固的 Windows 上 AcquireCredentialsHandleW
// 会一律返回 SEC_E_UNKNOWN_CREDENTIALS（0x8009030D，连带客户端证书也一样，
// 而 NTLM/证书系统库正常）——那类机器上本后端 fail-loud 报 Unsupported，
// 绝不静默退回明文。2026-10-19 在开发沙箱里实测命中该策略。



namespace termchart::network::tls::schannel {

// 建一个 SChannel 后端实例。非 Windows 平台返回 nullptr（本Header 仍可编译）。
[[nodiscard]] TERMCHART_API std::unique_ptr<TlsBackend> create_backend() noexcept;

// 注册进 TlsRegistry（幂等）。静态初始化已自动注册一次，重复调用无副作用。
TERMCHART_API void register_backend() noexcept;

}  // namespace termchart::network::tls::schannel

// ===== include/termchart/term/ansi.hpp =====
// ansi.hpp —— ANSI 序列常量与生成
//
// 边界（重要，别在这里加东西）：
//   * 本文件只管 **光标/擦除/屏幕切换** 这类控制序列；
//   * 颜色与属性序列（SGR）的唯一生成点是 render/sgr.hpp —— 收窄边已登记，
//     任何别的模块都不许自己拼 ESC[...m。
// 序列常量表见 detail/ansi_tables.hpp。



namespace termchart::term::ansi {

// ── 光标 ────────────────────────────────────────────────────────────────
TERMCHART_API void append_cursor_to(std::string& out, int row, int col) noexcept;  // 1-based
TERMCHART_API void append_cursor_up(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_down(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_forward(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_back(std::string& out, int n = 1) noexcept;
TERMCHART_API void append_cursor_home(std::string& out) noexcept;

TERMCHART_API std::string cursor_to(int row, int col) noexcept;
TERMCHART_API std::string cursor_home() noexcept;

// ── 擦除 ────────────────────────────────────────────────────────────────
TERMCHART_API void append_erase_to_end_of_line(std::string& out) noexcept;
TERMCHART_API void append_erase_line(std::string& out) noexcept;
TERMCHART_API void append_erase_display(std::string& out) noexcept;
TERMCHART_API std::string erase_line() noexcept;

// ── 光标可见性 / 交替屏幕 ───────────────────────────────────────────────
TERMCHART_API std::string hide_cursor() noexcept;
TERMCHART_API std::string show_cursor() noexcept;
TERMCHART_API std::string enter_alt_screen() noexcept;
TERMCHART_API std::string exit_alt_screen() noexcept;

// 位置跳转：给定当前与目标行列，优先选用最短序列。
// 由 render/renderer.hpp 使用；别处自行拼制符会让差分输出失控。
TERMCHART_API void append_move(std::string& out, int from_row, int from_col, int to_row,
                               int to_col) noexcept;

}  // namespace termchart::term::ansi

// ===== include/termchart/term/capability_query.hpp =====
// capability_query.hpp —— DA1 / DA2 主动探测
//
// 环境变量（见 term/caps.hpp）只能"猜"，DA 是终端**自报**的能力，二者互补：
//   DA1  ESC [ c     → 终端回 ESC [ ? Ps ; … c
//   DA2  ESC [ > c   → 终端回 ESC [ > Ps ; … c
//
// 本模块只做两件事：生成查询序列、解析回复。真正的读写终端由 term/terminal 或
// app/ 负责——保持这里无 I/O，单测才能确定性地验证解析（含畸形输入）。



namespace termchart::term {

struct DeviceAttributes {
    bool valid = false;
    bool secondary = false;   // 回复来自 DA2（'>' 前缀）
    int device_class = 0;     // 首个参数
    std::vector<int> params;  // 其余参数（不含 device_class）
};

// 需要写入终端的查询序列。
[[nodiscard]] TERMCHART_API std::string query_da1();
[[nodiscard]] TERMCHART_API std::string query_da2();

// 解析终端回复。容忍前置 ESC、大小写无关的终止符 'c'/'C'；
// 结构不合法（缺 '['、无参数、无终止符、出现非数字）时返回 false 且不改动 out。
[[nodiscard]] TERMCHART_API bool parse_device_attributes(std::string_view reply,
                                                         DeviceAttributes& out) noexcept;

// DA1 里对渲染有意义的几位（编号取自 xterm 的 ctlseqs）。
struct Da1Features {
    bool sixel = false;            // 4
    bool window_ops = false;       // 18
    bool ansi_color = false;       // 22
    bool rectangular_ops = false;  // 28
};

[[nodiscard]] TERMCHART_API Da1Features da1_features(
    const DeviceAttributes& attributes) noexcept;

}  // namespace termchart::term

// ===== include/termchart/term/caps.hpp =====
// caps.hpp —— 终端能力探测
//
// 优先级规则见 docs/reference/terminal_capabilities.md：
//   显式环境变量 > TERM 名称 > 默认保守值。
// **纯函数**：能力探测不读写终端、不发查询序列，输入是环境变量快照 + 是否 tty。
// 因此 CI / 单元测试可以把环境变量塞进来，完全可复现。



namespace termchart::term {

// 图像协议：term 只知道"能不能画"，真正编码在 protocol/（L9）。
enum class ImageProtocolKind : std::uint8_t {
    None = 0,        // 不能内联画图：退化为半块字符
    Halfblock = 1,   // 用 ▀▄ 拼图：全平台可用
    Sixel = 2,
    Kitty = 3,
    ITerm2 = 4,
};

struct CapEnvironment {
    std::string_view term;          // TERM
    std::string_view colorterm;     // COLORTERM
    std::string_view term_program;  // TERM_PROGRAM
    bool ci = false;                // CI / GITHUB_ACTIONS 等
    // WT_SESSION：Windows Terminal 指纹。它必设、而其他终端几乎不设，是识别
    // 真彩色宿主的可靠信号（原生 Windows 会话里 TERM 往往为空）。**放在末尾**，
    // 以免破坏既有按位置初始化 {term, colorterm, term_program, ci} 的调用点。
    std::string_view wt_session;
};

struct TerminalCaps {
    bool unicode = true;
    ColorDepth color_depth = ColorDepth::TrueColor;
    ImageProtocolKind image = ImageProtocolKind::Halfblock;
    bool mouse = false;
    bool alt_screen = true;
    bool title_setting = true;
};

// 按优先级合成能力。tty == false 时先套 daemon_safe 的保守假设。
TERMCHART_API TerminalCaps detect(const CapEnvironment& env, bool tty) noexcept;

// 最保守能力：纯 ASCII、无色、无图像。用于非 tty / CI / 老旧终端。
TERMCHART_API TerminalCaps minimal() noexcept;

// 单独的规则单元，**公开以便测试**：它们是那份优先级表的可执行形式。
TERMCHART_API ColorDepth color_depth_for(const CapEnvironment& env) noexcept;
TERMCHART_API ImageProtocolKind image_protocol_for(const CapEnvironment& env) noexcept;
TERMCHART_API bool unicode_for(const CapEnvironment& env) noexcept;

TERMCHART_API const char* image_protocol_name(ImageProtocolKind kind) noexcept;

}  // namespace termchart::term

// ===== include/termchart/term/clipboard.hpp =====
// clipboard.hpp —— OSC 52 剪贴板
//
// 写入： ESC ] 52 ; <target> ; <base64 文本> ST
// 读取： 同样发一条，但 base64 为空，终端会**回包**同样形状的序列
//
// 安全提醒：OSC 52 是否生效由终端决定——很多终端默认关闭写入，也有终端只允许
// 读取。因此本模块只生成序列，不假设它一定成功；调用方不应把剪贴板当作可靠存储。



namespace termchart::term {

enum class ClipboardTarget : char {
    Selection = 'c',  // 系统剪贴板
    Primary = 'p',    // X11 主选择区
};

// 写入剪贴板（文本先按 UTF-8 原样 base64）。
[[nodiscard]] TERMCHART_API std::string clipboard_set(
    std::string_view text, ClipboardTarget target = ClipboardTarget::Selection);

// 请求终端回传剪贴板内容；回包交给 parse_clipboard_reply 解析。
[[nodiscard]] TERMCHART_API std::string clipboard_request(
    ClipboardTarget target = ClipboardTarget::Selection);

// 解析回包的 OSC 52 载荷（形如 "c;Zm9vYmFy"）。成功时 out 为解码后的原文。
// 空载荷（终端拒绝读取或剪贴板为空）返回 false，且不改动 out。
[[nodiscard]] TERMCHART_API bool parse_clipboard_reply(std::string_view payload,
                                                       std::string& out);

}  // namespace termchart::term

// ===== include/termchart/term/color_fallback.hpp =====
// color_fallback.hpp —— 颜色降级（TerminalCaps → 实际可用的颜色深度）
//
// 降级**算法只有一份**，在 color/quantize（L1）：真彩 → 256 色 → 16 色 → 无色的
// 量化与最邻近匹配都在那里。本文件是 term 层的桥接：从 TerminalCaps 取出实际可用
// 的 ColorDepth，再转发给 quantize::degrade，避免"每个调用点自己判断一遍"。


namespace termchart::term {

// 终端真正支持的深度（None 表示只能用默认前/背景色）。
[[nodiscard]] TERMCHART_API ColorDepth depth_of(const TerminalCaps& caps) noexcept;

// 把颜色压到该深度；已是可表示的颜色则原样返回。
[[nodiscard]] TERMCHART_API Color apply_fallback(const Color& color,
                                                  const TerminalCaps& caps) noexcept;

// 前/背景色降级，属性位（加粗/下划线等）保持不变。
[[nodiscard]] TERMCHART_API Style apply_fallback(const Style& style,
                                                  const TerminalCaps& caps) noexcept;

}  // namespace termchart::term

// ===== include/termchart/term/hyperlink.hpp =====
// hyperlink.hpp —— OSC 8 超链接
//
// 序列形状（OSC 8 用 ST `ESC \` 结束，不用 BEL）：
//   打开： ESC ] 8 ; [id=<id>;]<uri> ST
//   文本
//   关闭： ESC ] 8 ; ; ST
//
// 支持的范围很广（iTerm2 / kitty / WezTerm / Windows Terminal / VTE 系），
// 不支持的终端会**原样显示** URI 与文本——因此 hyperlink() 只在明确需要时才用。



namespace termchart::term {

// 打开超链接。id 非空时同一 id 的相邻链接会被终端视作同一个链接
// （避免鼠标悬停时闪烁），也便于"同一目标多段文本"合并。
[[nodiscard]] TERMCHART_API std::string hyperlink_open(std::string_view uri,
                                                        std::string_view id = {});

[[nodiscard]] TERMCHART_API std::string hyperlink_close();

// 便捷：一次给出"打开 + 文本 + 关闭"。
[[nodiscard]] TERMCHART_API std::string hyperlink(std::string_view text,
                                                   std::string_view uri,
                                                   std::string_view id = {});

}  // namespace termchart::term

// ===== include/termchart/term/mouse_mode.hpp =====
// mouse_mode.hpp —— 鼠标上报开关（DECSET / DECRST）
//
// 只生成序列，不写 stdout、不改自身状态：调用方（term/terminal 或 app/）决定何时
// 打开、何时关闭，并负责在异常退出时复位（见 term/terminal 的 RAII 门面）。
//
// 模式与坐标编码是**两组独立开关**，必须分别设置：
//   * 模式（9 / 1000 / 1002 / 1003）决定"什么时候上报"；
//   * 编码（1005 / 1006 / 1015）决定"怎么编码坐标"。
// 默认编码是 X10 风格，坐标上限 223 列且有歧义，所以实际使用一律建议配 SGR(1006)。



namespace termchart::term {

// 枚举值**就是** DECSET 的编号本身（9 / 1000 / 1002 / 1003 与 1005 / 1006 / 1015），
// 生成序列时直接写进 `ESC [ ? <n> h`，不必再维护一张映射表。
enum class MouseMode : std::uint16_t {
    Off = 0,               // 不上报
    X10 = 9,               // 只上报按下
    Normal = 1000,         // 按下 / 释放 / 滚轮
    ButtonMotion = 1002,   // 按住拖动时上报移动
    AnyMotion = 1003,      // 任何移动都上报
};

enum class MouseEncoding : std::uint16_t {
    Default = 0,    // 终端默认（X10 风格，坐标上限 223）
    Utf8 = 1005,    // UTF-8 扩展坐标
    Sgr = 1006,     // SGR 扩展：坐标无上限，且能明确区分按下与释放（推荐）
    Urxvt = 1015,   // urxvt 扩展
};

// 打开/关闭指定模式；mode == Off 时本函数不产生模式序列。
[[nodiscard]] TERMCHART_API std::string set_mouse_mode(MouseMode mode) noexcept;

// 打开/关闭指定坐标编码；encoding == Default 时返回空串。
[[nodiscard]] TERMCHART_API std::string set_mouse_encoding(MouseEncoding encoding) noexcept;

// 一次打开"模式 + 编码"（编码为 Default 时只设模式）。
[[nodiscard]] TERMCHART_API std::string enable_mouse(
    MouseMode mode, MouseEncoding encoding = MouseEncoding::Sgr);

// 关闭指定模式与编码（顺序与打开相反）。
[[nodiscard]] TERMCHART_API std::string disable_mouse(
    MouseMode mode, MouseEncoding encoding = MouseEncoding::Sgr);

// 把 9/1000/1002/1003 全部关掉：切换模式前用它清场，避免多个模式叠加
// （终端的鼠标模式是**叠加**的，只关当前模式会留下上一次打开的另一个模式）。
[[nodiscard]] TERMCHART_API std::string disable_all_mouse(
    MouseEncoding encoding = MouseEncoding::Sgr);

}  // namespace termchart::term

// ===== include/termchart/term/raw_mode.hpp =====
// raw_mode.hpp —— 原始模式 RAII
//
// 关键约束：**即使抛异常也必须恢复终端**。因此这里只提供 RAII 一种用法，
// 不暴露裸 enable/disable 给用户——"忘记恢复"是这类 bug 的一半来源。
// 平台差异全部在 src/term/terminal_posix.cpp / terminal_win32.cpp，头文件无 #ifdef。


namespace termchart::term {

class RawMode {
public:
    RawMode() = default;

    // 进入原始模式。失败时 active() == false（例如 stdin 不是 tty）。
    TERMCHART_API void enter() noexcept;

    // 显式退出；可多次调用（幂等）。
    TERMCHART_API void leave() noexcept;

    TERMCHART_API ~RawMode();

    RawMode(const RawMode&) = delete;
    RawMode& operator=(const RawMode&) = delete;

    // 移动：把"恢复责任"移交给新对象。
    RawMode(RawMode&& other) noexcept : active_(other.active_) { other.active_ = false; }
    RawMode& operator=(RawMode&& other) noexcept {
        if (this != &other) {
            leave();
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

    [[nodiscard]] TERMCHART_API bool active() const noexcept { return active_; }

    // 测试探针：不进真实原始模式，只翻转内部标志（用于终端恢复回归测试）。
    static TERMCHART_API RawMode make_noop(bool pretend_active) noexcept;

private:
    explicit RawMode(bool pretend_active) noexcept : pretend_(true), active_(pretend_active) {}

    bool pretend_ = false;
    bool active_ = false;
};

}  // namespace termchart::term

// ===== include/termchart/term/size.hpp =====
// size.hpp —— 终端尺寸查询与 SIGWINCH
//
// 尺寸是"终端说了算"的外部状态：任何时候都可能变（用户拉窗口）。
// 这里只提供**查询**；真正的循环怎么响应 resize 在 app/loop.hpp，
// 信号转发走 term/signal.hpp + term/waker.hpp（绝不进 signal handler 干活）。



namespace termchart::term {

struct TerminalSize {
    std::uint16_t cols = 80;
    std::uint16_t rows = 24;

    constexpr bool operator==(const TerminalSize&) const noexcept = default;
    constexpr bool operator!=(const TerminalSize&) const noexcept = default;
    [[nodiscard]] constexpr bool valid() const noexcept { return cols > 0 && rows > 0; }
    [[nodiscard]] constexpr Point as_point() const noexcept {
        return Point{static_cast<double>(cols), static_cast<double>(rows)};
    }
};

// 平台实现隔离在 src/term/terminal_posix.cpp / terminal_win32.cpp，
// 失败（非 tty / ioctl 拒绝）时回落到 80×24。
TERMCHART_API TerminalSize query_size() noexcept;

// 环境变量回退（COLUMNS / LINES）：CI 里没有 tty，这是唯一可用来源。
TERMCHART_API TerminalSize size_from_env() noexcept;

}  // namespace termchart::term

// ===== include/termchart/term/terminal.hpp =====
// terminal.hpp —— 终端 RAII 门面
//
// 唯一"拥有"终端状态的地方：
//   * 构造时不改动任何东西，调用 enter() 才真正进入；
//   * 析构（含异常栈展开）必定**按相反顺序**撤销 enter() 做过的每一件事；
//   * daemon_safe.hpp 负责"非 tty 时退行为"。
//
// 平台差异全部落在 src/term/terminal_posix.cpp 与 terminal_win32.cpp：
// **本头文件没有任何 #ifdef**（违反这条就是架构泄漏）。
//
// 返回值约定：只回报"成功与否"的操作返回 Status（即 ErrorCode）；
// 需要携带值的才用 Result<T>（见 core/expected.hpp）。



namespace termchart::term {

struct TerminalOptions {
    bool raw_mode = false;      // 关闭回显/行缓冲：仅交互场景需要
    bool alt_screen = true;     // 用交替屏幕：退出后自动回到原内容
    bool hide_cursor = true;    // 隐藏光标只在 enter 期间生效
    bool enable_mouse = false;  // 阶段 5 的交互需要
};

class Terminal {
public:
    explicit Terminal(TerminalOptions options = {}) noexcept;
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&& other) noexcept;
    Terminal& operator=(Terminal&& other) noexcept;

    // 进入：按 options 逐项启用。**记录已生效项**，供析构精确回滚。
    TERMCHART_API Status enter() noexcept;
    TERMCHART_API Status leave() noexcept;
    TERMCHART_API void restore() noexcept;  // leave() 的 noexcept 版，给析构/信号处理用

    [[nodiscard]] TERMCHART_API bool entered() const noexcept { return entered_; }
    [[nodiscard]] TERMCHART_API const TerminalOptions& options() const noexcept { return options_; }

    // 写入 / 立即生效：不使用 iostream，避免 locale 与缓冲策略差异。
    TERMCHART_API Status write(std::string_view bytes) noexcept;
    TERMCHART_API Status flush() noexcept;

    [[nodiscard]] TERMCHART_API TerminalSize size() const noexcept;
    [[nodiscard]] TERMCHART_API TerminalCaps caps() const noexcept;

    // ── 静态探测 ──────────────────────────────────────────────────────
    [[nodiscard]] static TERMCHART_API bool stdout_is_tty() noexcept;
    [[nodiscard]] static TERMCHART_API bool stdin_is_tty() noexcept;

private:
    struct Changes {
        bool alt_screen = false;
        bool cursor_hidden = false;
        bool mouse_enabled = false;
    };

    TerminalOptions options_{};
    RawMode raw_mode_;
    Changes changes_{};
    bool entered_ = false;
    TerminalCaps caps_{};
};

}  // namespace termchart::term

// ===== include/termchart/term/daemon_safe.hpp =====
// daemon_safe.hpp —— 非 tty 安全模式
//
// 场景：库被 cron / systemd / CI / 管道用。此时若照常发 ANSI 序列，
// 日志里会留下 \x1b[2J 之类的噪声，而用户只会反馈"输出脏了"。
//
// 策略（与本文件同名的 docs 节点保持一致）：
//   * 非 tty → 不发颜色、不进交替屏幕、不隐藏光标、不进原始模式；
//   * CI 明确要求"可预期输出"时，进一步把颜色档位打到 None。


namespace termchart::term {

struct DaemonSafeSettings {
    bool enabled = false;      // 是否处于"非 tty 安全"状态
    bool allow_color = true;   // 允许颜色（tty 时）
    bool allow_alt_screen = true;
    bool allow_raw_mode = true;
    bool allow_cursor_hiding = true;
};

// 依据"是否 tty + 是否 CI" 合成设置。纯函数，便于测试。
TERMCHART_API DaemonSafeSettings daemon_safe_for(bool tty, bool ci) noexcept;

// 把安全设置叠加到能力上（降级而非报错）。
TERMCHART_API TerminalCaps apply(const TerminalCaps& caps,
                                 const DaemonSafeSettings& safe) noexcept;

// 把安全设置叠加到终端选项上。**返回新值**，不改动入参：调用方需要能看出差异。
TERMCHART_API TerminalOptions apply(const TerminalOptions& options,
                                    const DaemonSafeSettings& safe) noexcept;

}  // namespace termchart::term

// ===== include/termchart/term/waker.hpp =====
// waker.hpp —— 跨线程唤醒原语
//
// 并发模型（docs/design/concurrency.md）：
//   单线程主循环 + select/poll。后台线程**不得**触碰任何绘制对象，
//   只能通过 app/post.hpp 投递数据，然后 notify() 唤醒主线程。
// POSIX 用 self-pipe，Windows 用手动重置 event —— 差异封在实现里，头文件无 #ifdef。



namespace termchart::term {

class Waker {
public:
    TERMCHART_API Waker() noexcept;
    TERMCHART_API ~Waker();

    Waker(const Waker&) = delete;
    Waker& operator=(const Waker&) = delete;

    // 移动：所有权转移，"可被 select 的那个 fd/handle"跟着走。
    TERMCHART_API Waker(Waker&& other) noexcept;
    TERMCHART_API Waker& operator=(Waker&& other) noexcept;

    // 唤醒一次。可重入：信号处理里调用是安全的。
    TERMCHART_API void notify() noexcept;

    // 阻塞等待被唤醒；timeout_ms < 0 表示无限等待。返回是否由 notify 唤醒（false = 超时）。
    TERMCHART_API bool wait(int timeout_ms) noexcept;

    // 消费已排队的唤醒信号（不清空则下次 wait 立即返回）。
    TERMCHART_API void drain() noexcept;

    [[nodiscard]] TERMCHART_API bool valid() const noexcept { return native_ != nullptr; }

    // 给主循环的 select/poll；Windows 返回 HANDLE，POSIX 返回读端 fd。
    // **平台差异在此收口**：调用方只做转发，不解释类型。
    [[nodiscard]] TERMCHART_API std::uintptr_t native_handle() const noexcept;

private:
    void* native_ = nullptr;
};

}  // namespace termchart::term

// ===== include/termchart/term/signal.hpp =====
// signal.hpp —— SIGWINCH 转发给主线程
//
// 铁律：**信号处理器里只做两件事**——置 atomic 标志 + 唤醒 waker。
// 其余（重新布局、重绘、QString 那样的任何分配）一律带回主线程做。
// 违反这条的后果是" resize 时偶发死锁/堆损坏"，且不可复现。
//
// 平台差异：Windows 没有 SIGWINCH（靠 ReadConsoleInput 的 WINDOW_BUFFER_SIZE_EVENT），
// 因此本头文件导出的是"安装/卸载 resize 转发"的**抽象动作**而非具体信号。


namespace termchart::term {

// 安装转发：把 resize 事件转成 "标志 + waker.notify()"。
// 重复安装会替换旧 waker；返回是否成功。
TERMCHART_API bool install_resize_forwarding(Waker& waker) noexcept;

// 卸载并恢复原处理方式（可以在任意线程调用一次）。
TERMCHART_API void uninstall_resize_forwarding() noexcept;

// 主线程消费：读到 true 表示"发生过 resize，需要重新查询尺寸"。
// **读取即清零**，避免重复布局。
TERMCHART_API bool take_resize_flag() noexcept;

// 测试探针：模拟收到 resize（不真的发信号）。
TERMCHART_API void simulate_resize_for_test() noexcept;

}  // namespace termchart::term

// ===== include/termchart/document/span.hpp =====
// span.hpp —— 带样式的文本片段（Document IR 的叶）
//
// Span 是"语义文本 + 样式"，不含像素/位置信息：位置由后端或上层决定。
// header-only 值类型。



namespace termchart::document {

struct Span {
    std::string text;
    Style style;

    Span() = default;
    explicit Span(std::string t, Style s = Style{}) : text(std::move(t)), style(s) {}
};

}  // namespace termchart::document

// ===== include/termchart/document/table_model.hpp =====
// table_model.hpp —— 表格的语义模型（行列字符串，非像素）
//
// backend/structured 负责把它排版成 HTML/Markdown；本层只表达"表头 + 若干行"。
// header-only 值类型。


namespace termchart::document {

class TableModel {
public:
    void set_header(std::vector<std::string> header) { header_ = std::move(header); }
    void add_row(std::vector<std::string> row) { rows_.push_back(std::move(row)); }

    [[nodiscard]] const std::vector<std::string>& header() const noexcept { return header_; }
    [[nodiscard]] const std::vector<std::vector<std::string>>& rows() const noexcept {
        return rows_;
    }
    [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }
    [[nodiscard]] std::size_t column_count() const noexcept {
        std::size_t n = header_.size();
        for (const auto& r : rows_) {
            if (r.size() > n) {
                n = r.size();
            }
        }
        return n;
    }
    [[nodiscard]] bool empty() const noexcept { return header_.empty() && rows_.empty(); }

private:
    std::vector<std::string> header_;
    std::vector<std::vector<std::string>> rows_;
};

}  // namespace termchart::document

// ===== include/termchart/document/block.hpp =====
// block.hpp —— Document 的内容块（语义单元）
//
// 一个 Block 是 Paragraph / Heading / Code / Table / Chart 之一。Chart 块持有
// Scene（矢量 IR）——于是"图表"作为文档元素不需要 backend 认识 chart 类型，
// 这正是 backend > chart 禁止边成立的前提。



namespace termchart::document {

enum class BlockKind {
    Paragraph,
    Heading,
    Code,
    Table,
    Chart,
};

class Block {
public:
    Block() = default;

    static Block paragraph(std::vector<Span> spans);
    static Block heading(std::string text, int level = 1);
    static Block code(std::string text, std::string language = {});
    static Block table(TableModel model);
    static Block chart(scene::Scene scene);

    [[nodiscard]] BlockKind kind() const noexcept { return kind_; }
    [[nodiscard]] const std::vector<Span>& spans() const noexcept { return spans_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] const std::string& language() const noexcept { return language_; }
    [[nodiscard]] int level() const noexcept { return level_; }
    [[nodiscard]] const TableModel& table() const noexcept { return table_; }
    [[nodiscard]] const scene::Scene& scene() const noexcept { return scene_; }

private:
    BlockKind kind_ = BlockKind::Paragraph;
    std::vector<Span> spans_;
    std::string text_;
    std::string language_;
    int level_ = 0;
    TableModel table_;
    scene::Scene scene_;
};

}  // namespace termchart::document

// ===== include/termchart/document/document.hpp =====
// document.hpp —— Document：内容块容器（structured 物种的输入）
//
// Document 是 backend/structured 的输入：它只表达语义（段落/标题/表格/图表），
// 不含任何渲染或像素决策。`to_plain_text()` 是给测试与 text 导出的最小序列化，
// 真正的 HTML/Markdown 由 backend/structured/* 负责。



namespace termchart::document {

class Document {
public:
    void add_block(Block block) { blocks_.push_back(std::move(block)); }

    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] std::size_t size() const noexcept { return blocks_.size(); }
    [[nodiscard]] bool empty() const noexcept { return blocks_.empty(); }
    [[nodiscard]] BlockKind kind_of(std::size_t index) const;

    // 最小纯文本序列化：标题/段落/代码/表格/图表占位，便于快照与测试。
    [[nodiscard]] TERMCHART_API std::string to_plain_text() const;

private:
    std::vector<Block> blocks_;
};

}  // namespace termchart::document

// ===== include/termchart/protocol/protocol.hpp =====
// protocol.hpp —— 终端图形协议抽象基类
//
// protocol/ 只负责一件事：把 image::Bitmap **编码成可写入终端的字节流**。
// 它不做布局、不移动光标、不写 stdout——定位由调用方决定。于是同一张位图可以
// 用 sixel / kitty / 半块字符三种方式表达，降级链见 fallback.hpp。
//
// 注：iTerm2 内联图像只认 PNG 负载（原始 RGBA 不被识别），其编码器
// （protocol/iterm2_inline.hpp）建立在内置的 image/png_encode 之上。



namespace termchart::protocol {

enum class ProtocolKind {
    None,       // 不支持任何图像协议（调用方应转而画字符）
    Sixel,      // DEC sixel（DCS 0;0;0q ... ST）
    Kitty,      // Kitty 图形协议（ESC_G，原始 RGBA）
    Iterm2,     // iTerm2 内联图像（OSC 1337，PNG + base64）
    HalfBlock,  // 真彩半块字符（▀ + 前景/背景色），任何 24-bit 终端可用
};

struct ProtocolOptions {
    std::uint32_t cell_width_px = 10;   // 目标格的像素宽（kitty 缩放用）
    std::uint32_t cell_height_px = 20;  // 目标格的像素高
    bool rle = true;                    // sixel 游程压缩
};

class Protocol {
public:
    virtual ~Protocol();

    [[nodiscard]] virtual ProtocolKind kind() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;

    // 编码为字节流（不含光标定位）。空位图返回空串。
    [[nodiscard]] virtual std::string encode(const image::Bitmap& bitmap,
                                            const ProtocolOptions& opts = {}) const = 0;
};

TERMCHART_API const char* protocol_name(ProtocolKind kind) noexcept;

}  // namespace termchart::protocol

// ===== include/termchart/protocol/fallback.hpp =====
// fallback.hpp —— 图像协议降级：能力 → ProtocolKind
//
// 与 raster/auto_select 同一套思路：能力以扁平布尔传入，协议层不依赖 term/。


namespace termchart::protocol {

struct ProtocolCaps {
    bool kitty = false;
    bool sixel = false;
    bool iterm2 = false;
    bool truecolor = false; // 24-bit 颜色，半块降级需要它
};

// 选择顺序：Kitty > Iterm2 > Sixel > HalfBlock（需 truecolor）> None。
// 排在前面的协议保真度更高（像素级原图 + 无需量化），半块只作最后兜底。
TERMCHART_API ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept;

}  // namespace termchart::protocol

// ===== include/termchart/protocol/detect.hpp =====
// detect.hpp —— 从环境变量探测终端支持的图像协议


namespace termchart::protocol {

// 读 TERM / TERM_PROGRAM / COLORTERM / KITTY_WINDOW_ID 等推断能力。
// 纯环境读取，不改环境；探测不到一律 false。
TERMCHART_API ProtocolCaps detect_protocol_caps() noexcept;

}  // namespace termchart::protocol

// ===== include/termchart/protocol/halfblock_image.hpp =====
// halfblock_image.hpp —— 真彩半块字符图像（任何 24-bit 终端可用的降级）
//
// 每格塞 2 个像素：上像素当前景、下像素当背景，字符恒为 ▀（U+2580）。
// 于是 1 格 = 1×2 像素，观感接近位图，且不需要任何图形协议。


namespace termchart::protocol {

class HalfBlockProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::HalfBlock; }
    [[nodiscard]] const char* name() const noexcept override { return "halfblock"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

// ===== include/termchart/protocol/iterm2_inline.hpp =====
// iterm2_inline.hpp —— iTerm2 内联图像协议（OSC 1337）
//
// iTerm2（及其兼容终端 WezTerm 等）只认 **PNG** 负载：原始 RGBA 不会被识别，
// 因此本编码器建立在内置的 image/png_encode 之上。
//
// 序列形状（BEL 结束）：
//   ESC ] 1337 ; File = inline=1;size=<PNG字节数>;width=<w>px;height=<h>px : <base64> BEL
//
// 与 sixel/kitty 一致，编码结果不含光标定位——放哪儿由调用方决定。



namespace termchart::protocol {

class Iterm2Inline final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override {
        return ProtocolKind::Iterm2;
    }
    [[nodiscard]] const char* name() const noexcept override { return "iterm2"; }

    // 空位图返回空串；ProtocolOptions 的格尺寸对本协议无意义（宽高就是像素尺寸）。
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                     const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

// ===== include/termchart/protocol/kitty_graphics.hpp =====
// kitty_graphics.hpp —— Kitty 图形协议（ESC_G，原始 RGBA/RGB + base64 分块）


namespace termchart::protocol {

class KittyProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::Kitty; }
    [[nodiscard]] const char* name() const noexcept override { return "kitty"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

// ===== include/termchart/protocol/sixel.hpp =====
// sixel.hpp —— DEC sixel 编码器（DCS 0;0;0q … ST）


namespace termchart::protocol {

class SixelProtocol final : public Protocol {
public:
    [[nodiscard]] ProtocolKind kind() const noexcept override { return ProtocolKind::Sixel; }
    [[nodiscard]] const char* name() const noexcept override { return "sixel"; }
    [[nodiscard]] std::string encode(const image::Bitmap& bitmap,
                                    const ProtocolOptions& opts = {}) const override;
};

}  // namespace termchart::protocol

// ===== include/termchart/canvas/canvas.hpp =====
// canvas.hpp —— 浮点坐标画布 → CellBuffer
//
// 终端实时路径的中段：
//   Scene →（scene_renderer）→ Canvas → to_buffer() → CellBuffer → render/renderer
//
// 与 raster/ 的分工（重要）：
//   * 本文件工作在**格**粒度：一个浮点坐标落到哪一格，就写那一格；
//   * 子像素（一格内 2×4 盲文点、半块上下双色）是 raster/ 的事——阶段 7 接入后
//     draw_line / fill_rect 会转而调用光栅器，本文件的对外接口**不变**。
// 因此这里的图元字符是"占位级"的（线用 '*'、填充用 █），不是最终观感。



namespace termchart::canvas {

class Canvas {
public:
    Canvas() = default;
    TERMCHART_API Canvas(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();

    [[nodiscard]] std::uint32_t width() const noexcept { return buffer_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return buffer_.height(); }

    // 浮点坐标 → 格：向下取整（与 core/rect 的半开区间约定一致）。
    // 越界一律**裁剪**（丢弃），不折回边缘、不报错；返回值告诉你到底落没落格——
    // 调用方要统计"实际画了多少格"时不必自己再判一次边界。
    TERMCHART_API bool put(Point p, char32_t cp, const Style& style = Style{}) noexcept;
    TERMCHART_API bool put(std::uint32_t x, std::uint32_t y, char32_t cp,
                           const Style& style = Style{}) noexcept;

    TERMCHART_API void fill_rect(Rect rect, char32_t cp, const Style& style = Style{}) noexcept;
    TERMCHART_API void draw_line(Point from, Point to, char32_t cp,
                                 const Style& style = Style{}) noexcept;
    TERMCHART_API void draw_text(Point at, std::string_view text,
                                 const Style& style = Style{}) noexcept;

    // 读取（测试与 layout 布局计算用）。
    [[nodiscard]] TERMCHART_API const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept;

    // 交给渲染管线。返回副本：Canvas 与 CellBuffer 生命周期解耦。
    [[nodiscard]] TERMCHART_API CellBuffer to_buffer() const;

private:
    CellBuffer buffer_;
};

}  // namespace termchart::canvas

// ===== include/termchart/canvas/clip.hpp =====
// clip.hpp —— 裁剪区域（矩形与凸多边形）
//
// 两种区域：
//   * 矩形：最常用。线段裁剪走精确的 Liang–Barsky；多边形裁剪走 4 个半平面。
//   * **凸**多边形：逐点判定 + Sutherland–Hodgman 逐边裁剪。
//
// 非凸多边形**只支持逐点判定**：多边形裁剪会原样返回，且 supports_polygon_clip()
// 报 false。这是刻意的——Sutherland–Hodgman 对非凸裁剪区会给出看似合理实则错误
// 的结果，宁可在接口上明说，也不让调用方拿到错的形状。



namespace termchart::canvas {

class ClipRegion {
public:
    ClipRegion() = default;  // 无约束：全部放行

    [[nodiscard]] static ClipRegion rect(const Rect& bounds);
    // 传入的点列会被规范化为逆时针（闭合与否都接受，内部按闭合处理）。
    [[nodiscard]] static ClipRegion polygon(std::vector<Point> points);

    // 没有任何约束（contains 恒真、裁剪恒返回原值）。
    [[nodiscard]] bool unconstrained() const noexcept { return !has_rect_ && !has_polygon_; }
    [[nodiscard]] bool is_rect() const noexcept { return has_rect_; }

    [[nodiscard]] TERMCHART_API bool contains(Point p) const noexcept;

    // 仅当 is_rect() 时有意义；否则返回零矩形。
    [[nodiscard]] Rect rect_bounds() const noexcept { return rect_; }

    // 是否支持多边形裁剪（矩形恒支持；多边形区域要求凸）。
    [[nodiscard]] bool supports_polygon_clip() const noexcept;

    // 裁剪多边形：矩形与凸多边形走 Sutherland–Hodgman；不支持时原样返回。
    [[nodiscard]] TERMCHART_API std::vector<Point> clip_polygon(
        std::vector<Point> polygon) const;

    // 裁剪线段：完全落在区域外返回 false，否则 out_a/out_b 为裁剪后的端点。
    [[nodiscard]] TERMCHART_API bool clip_segment(Point a, Point b, Point& out_a,
                                                  Point& out_b) const noexcept;

private:
    // 半平面裁剪的单步：把 input 裁到"边 (edge_a → edge_b) 的左侧"。
    void clip_half_plane(std::vector<Point>& poly, Point edge_a, Point edge_b,
                         bool keep_left) const;

    bool has_rect_ = false;
    Rect rect_{};
    bool has_polygon_ = false;
    std::vector<Point> polygon_;  // 逆时针
    bool polygon_convex_ = false;
};

}  // namespace termchart::canvas

// ===== include/termchart/canvas/fill.hpp =====
// fill.hpp —— 多边形扫描线填充（含纵向超采样）
//
// 扫描线思路：按**格行**推进，对每条采样线求它与多边形各边的交点，排序后按
// 奇偶规则配对成若干半开区间 [x0, x1)，区间内的格才点亮。
//
// 为什么还要超采样：字符格没有灰度，边界格"点亮与否"直接决定图形边缘的观感。
// supersample = n 时每格纵向取 n 条采样线，横向用**精确重叠长度**算覆盖比例，
// 覆盖率 >= coverage 才写入 —— 边界格因此会按实际覆盖面积决定取舍，
// 比"格中心在内就点亮"稳得多（细长三角形不会被画丢）。



namespace termchart::canvas {

// 一个格行内的半开区间 [x0, x1)，row 为格行号（非负）。
struct Span {
    std::uint32_t row = 0;
    double x0 = 0.0;
    double x1 = 0.0;
};

struct FillOptions {
    char32_t on = U'\u2588';       // 填充字符（实心块）
    Style style{};
    std::uint32_t supersample = 1;  // 每格纵向子采样数（1 = 只取格中心）
    double coverage = 0.5;          // 写入阈值：格被覆盖的比例
};

// 单条采样线上的区间（y 为像素坐标）；便于单测与复用。
[[nodiscard]] TERMCHART_API std::vector<Span> scanline_spans(
    const std::vector<Point>& polygon);

// 逐格填充多边形（奇偶规则），返回实际写入画布（未越界）的格数。
TERMCHART_API std::size_t fill_polygon(Canvas& canvas, const std::vector<Point>& polygon,
                                       const FillOptions& opts = {});

}  // namespace termchart::canvas

// ===== include/termchart/canvas/layer.hpp =====
// layer.hpp —— 图层合成
//
// 终端没有 alpha 通道，但有三种**真实可用**的合成语义：
//   * Overlay：只写"非空白"格（空白处露出下层）—— 等价于透明，最常用；
//   * Replace：连空白格一起覆盖（"这块区域归我"）；
//   * Tint   ：保留字符、统一换成给定样式（高亮 / 遮罩 / 变暗）。
//
// 空白的口径：`is_blank()`（空格且无样式）。**带样式的空格会被当作内容写入**，
// 这正是"铺底色"的用法；不想铺就用默认样式留空格。
//
// 另外提供最小重绘支持：mark_clean() 记快照，dirty_bounds() 给出与快照的差异包围盒。



namespace termchart::canvas {

class Layer {
public:
    Layer() = default;
    TERMCHART_API Layer(std::uint32_t width, std::uint32_t height);

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear();

    [[nodiscard]] std::uint32_t width() const noexcept { return canvas_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return canvas_.height(); }

    // 在本层作画。
    [[nodiscard]] Canvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] const Canvas& canvas() const noexcept { return canvas_; }
    [[nodiscard]] const Cell& at(std::uint32_t x, std::uint32_t y) const noexcept {
        return canvas_.at(x, y);
    }
    // 快照（拷贝），交给渲染管线或做对比用。
    [[nodiscard]] TERMCHART_API CellBuffer to_buffer() const;

    enum class Mode {
        Overlay,  // 只写非空白格
        Replace,  // 整层覆盖
        Tint,     // 非空白格保留字符、套用 tint 样式
    };

    // 合成到目标画布：at 为落点（可为负，负处按格裁剪丢弃）。
    // tint 仅 Tint 模式使用；传 nullptr 表示保留原样式。
    TERMCHART_API std::size_t composite_to(Canvas& target, Point at = {},
                                           Mode mode = Mode::Overlay,
                                           const Style* tint = nullptr) const;

    // 记下"当前状态即干净"；dirty_bounds() 之后给出差异包围盒。
    TERMCHART_API void mark_clean();

    // 与上次 mark_clean() 之间的差异包围盒（层坐标）。
    // 若从未 mark_clean() 或没有任何差异，返回零矩形（w == h == 0）。
    [[nodiscard]] TERMCHART_API Rect dirty_bounds() const;

private:
    Canvas canvas_;
    CellBuffer clean_;
    bool has_clean_ = false;
};

}  // namespace termchart::canvas

// ===== include/termchart/canvas/path.hpp =====
// path.hpp —— 路径对象：构建 → 展平 → 描边/填充
//
// 路径只描述几何，不绑定样式与画布：先 add_* 建好，再交给 stroke()/fill()。
// 曲线（二次/三次贝塞尔、圆）统一用**自适应细分**展平成折线，容差以格为单位
// （默认 0.25 格）：容差越小越贴合，折线也越长。
//
// 圆用四段三次贝塞尔（kappa = 0.5523）表示，而不是固定分段数的多边形 ——
// 这样细分的密度由容差决定，放大缩小时都不会出现肉眼可见的多边形棱角。



namespace termchart::canvas {

enum class PathVerb : std::uint8_t {
    Move,   // a = 目标点
    Line,   // a = 目标点
    Quad,   // a = 控制点，b = 目标点
    Cubic,  // a = 控制点 1，b = 控制点 2，c = 目标点
    Close,  // 回到当前子路径起点
};

struct PathCommand {
    PathVerb verb = PathVerb::Move;
    Point a{};
    Point b{};
    Point c{};
};

class Path {
public:
    TERMCHART_API void move_to(Point p);
    TERMCHART_API void line_to(Point p);
    TERMCHART_API void quadratic_to(Point control, Point end);
    TERMCHART_API void cubic_to(Point c1, Point c2, Point end);
    TERMCHART_API void close();
    TERMCHART_API void add_rect(const Rect& rect);
    TERMCHART_API void add_circle(Point center, double radius);
    // 折线：自动 move_to + 逐点 line_to；closed 为真时闭合。
    TERMCHART_API void add_polyline(const std::vector<Point>& points, bool closed = false);

    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }
    [[nodiscard]] std::size_t command_count() const noexcept { return commands_.size(); }
    [[nodiscard]] const std::vector<PathCommand>& commands() const noexcept {
        return commands_;
    }

    // 展平：每个子路径一条折线（点列）。少于两个点的子路径被丢弃。
    [[nodiscard]] TERMCHART_API std::vector<std::vector<Point>> flatten(
        double tolerance = 0.25) const;

    // 保守包围盒：把曲线控制点也算进去，因此**一定**包住实际形状。
    [[nodiscard]] TERMCHART_API Rect bounds() const;

    // 描边：逐段画线。返回实际落在画布内的格数。
    [[nodiscard]] TERMCHART_API std::size_t stroke(Canvas& canvas, char32_t pen,
                                                   const Style& style = {},
                                                   double tolerance = 0.25) const;

    // 填充：对每个展平后的子路径做扫描线填充。返回写入的格数。
    [[nodiscard]] TERMCHART_API std::size_t fill(Canvas& canvas,
                                                 const FillOptions& opts = {},
                                                 double tolerance = 0.25) const;

private:
    std::vector<PathCommand> commands_;
};

}  // namespace termchart::canvas

// ===== include/termchart/canvas/primitives.hpp =====
// primitives.hpp —— 图元离散化（浮点 → 格坐标序列）
//
// 纯函数层：**只产出"要写哪些格"**，不碰 Canvas，也不决定用什么字符。
// 于是光栅器（raster/）将来可以在同一份离散结果上换字符，而图元算法不变。



namespace termchart::canvas {

// 直线：DDA 采样，步长取一格；返回按行走的格坐标（已向下取整，可能重复）。
TERMCHART_API std::vector<Point> line_points(Point from, Point to) noexcept;

// 矩形轮廓（四条边，角落不重复）。
TERMCHART_API std::vector<Point> rect_outline_points(Rect rect) noexcept;

// 整圆 / 圆弧：按角度采样后去重。起止角为弧度，默认整圆 [0, 2π)。
TERMCHART_API std::vector<Point> circle_points(Point center, double radius) noexcept;
TERMCHART_API std::vector<Point> arc_points(Point center, double radius, double start,
                                            double end) noexcept;

}  // namespace termchart::canvas

// ===== include/termchart/canvas/scene_renderer.hpp =====
// scene_renderer.hpp —— Scene → Canvas，**终端路径上的唯一桥接点**
//
// 为什么只有这一处能解释 Scene 指令：
//   * vector 后端（backend/vector/*）直接消费 Scene，不经过 Canvas；
//   * 终端路径必须先把矢量指令离散成格，这一层转换只应存在一份。
// 若第二个文件开始 switch Command 的类别，两份实现对同一 Scene 的解释迟早分叉。


namespace termchart::canvas {

// 把 Scene 画到 Canvas。无返回、不抛异常：画不下的部分静默丢弃。
TERMCHART_API void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept;

}  // namespace termchart::canvas

// ===== include/termchart/canvas/transform.hpp =====
// transform.hpp —— 2D 仿射变换（含终端字符的宽高比补偿）
//
// 约定：列向量，p' = M · [x y 1]^T，元素按行主书写
//     | m00 m01 m02 |
//     | m10 m11 m12 |
//     |  0   0   1  |
// `then(next)` 表示"先做 this、再做 next"（等价于矩阵乘法 next·this），
// 因此链式写法与阅读顺序一致：T.translation(...).then(T.rotation(...)) 先平移后旋转。
//
// 宽高比补偿：终端字符格不是正方形，宽:高通常约 1:2。几何上的正圆直接落格会显扁，
// 所以画圆/放射布局时先用 aspect_correction() 把 y 方向压缩 1/aspect，
// 渲染出来才接近正圆。这也是本模块区别于通用线性代数的唯一一点。



namespace termchart::canvas {

class Transform {
public:
    Transform() = default;  // 单位变换

    [[nodiscard]] static Transform translation(double dx, double dy) noexcept;
    [[nodiscard]] static Transform scaling(double sx, double sy) noexcept;
    [[nodiscard]] static Transform rotation(double radians) noexcept;
    [[nodiscard]] static Transform shearing(double shx, double shy) noexcept;
    // 绕给定中心旋转。
    [[nodiscard]] static Transform rotation_around(Point center, double radians) noexcept;
    // 供"字符网格上的圆/放射布局"用：y 方向压缩 1/cell_aspect。
    [[nodiscard]] static Transform aspect_correction(double cell_aspect = 2.0) noexcept;

    // 先本变换、后 next。
    [[nodiscard]] TERMCHART_API Transform then(const Transform& next) const noexcept;
    [[nodiscard]] TERMCHART_API double determinant() const noexcept;
    [[nodiscard]] TERMCHART_API bool invertible() const noexcept;
    // 不可逆（行列式为 0）时返回单位变换；调用方若需要区分，先查 invertible()。
    [[nodiscard]] TERMCHART_API Transform inverse() const noexcept;

    [[nodiscard]] TERMCHART_API Point apply(Point p) const noexcept;
    // 变换后的**轴对齐包围盒**：四个角分别变换后取并集（含负缩放情形）。
    [[nodiscard]] TERMCHART_API Rect apply_bounds(const Rect& rect) const noexcept;
    [[nodiscard]] TERMCHART_API std::vector<Point> apply_all(
        const std::vector<Point>& points) const;

    [[nodiscard]] double m00() const noexcept { return m00_; }
    [[nodiscard]] double m01() const noexcept { return m01_; }
    [[nodiscard]] double m02() const noexcept { return m02_; }
    [[nodiscard]] double m10() const noexcept { return m10_; }
    [[nodiscard]] double m11() const noexcept { return m11_; }
    [[nodiscard]] double m12() const noexcept { return m12_; }

    [[nodiscard]] bool is_identity() const noexcept;

private:
    double m00_ = 1.0;
    double m01_ = 0.0;
    double m02_ = 0.0;
    double m10_ = 0.0;
    double m11_ = 1.0;
    double m12_ = 0.0;
};

}  // namespace termchart::canvas

// ===== include/termchart/layout/facet.hpp =====
// facet.hpp —— 纯几何的矩形划分（facet 布局）
//
// 只做一件事：把一块矩形按 rows × cols 切成带间距的小矩形。**不建图、不画图**，
// 也不认识 chart —— 调用方拿到 Rect 列表后自己决定每格画什么（这正是分面图与
// 仪表盘拼版需要的原语，也让本文件可以被 GUI/布局层无条件复用）。
//
// 两个约定：
//   * 行优先：索引 row * cols + col，facet(0,0) 在左上角；
//   * 尺寸除不尽时，余数按"从左到右、从上到下"逐格 +1 分配，各格尺寸最多相差
//     一格；分配后宽度或高度不足 1 格的格子返回空矩形（w 或 h 为 0），
//     由调用方跳过 —— 不产生"看起来有其实画不下"的格子。



namespace termchart::layout {

struct FacetOptions {
    std::uint32_t rows = 1;
    std::uint32_t cols = 1;
    double gap_x = 1.0;  // 列间距（格）
    double gap_y = 1.0;  // 行间距（格）
};

// 全部格子（行优先）。rows 或 cols 为 0 时返回空列表。
[[nodiscard]] TERMCHART_API std::vector<Rect> split_rect(
    Rect area, const FacetOptions& options = {});

// 单个格子；越界返回空矩形。
[[nodiscard]] TERMCHART_API Rect facet_cell(Rect area, const FacetOptions& options,
                                            std::uint32_t row, std::uint32_t col);

}  // namespace termchart::layout

// ===== include/termchart/layout/label_avoid.hpp =====
// label_avoid.hpp —— 标签避让（通用实现，位于 layout 层）
//
// 为什么在 layout 而不是 graph：图模块（L13）与图表模块（L12）都要用同一套避让
// 规则，而它们只能向下依赖。把唯一实现放在 layout，上层转引即可 —— 否则同一条
// "候选位顺序 + 不相交 + 不越界"的规则会各写一份，迟早分叉。
//
// 规则（与既有 graph 侧行为完全一致，gap 默认 1 即"紧贴锚点外侧一格"）：
//   * 候选顺序：右、左、上、下；
//   * 候选位必须不与 occupied 中任何矩形相交（半开区间，共边不算相交），
//     且不越出左/上边界与可选的整体 bounds；
//   * 全部不可用时返回首选位并置 placed = false —— 调用方应**放弃绘制**，
//     而不是硬画上去覆盖别人的内容。



namespace termchart::layout {

struct LabelBox {
    Point anchor{};
    std::uint32_t width = 0;
    std::uint32_t height = 1;
};

struct LabelAvoidOptions {
    // 标签与锚点之间的间距（格）。
    double gap = 1.0;
    // 允许区域；w 或 h 为 0 表示该方向不限制。
    Rect bounds{};
};

struct LabelSlot {
    Rect rect{};
    // false 表示没有可用位置（rect 为首选位，仅供诊断）。
    bool placed = false;
};

// 单个标签：在锚点四周找第一个可用位置。
[[nodiscard]] TERMCHART_API LabelSlot place_label_avoiding(
    const LabelBox& label, const std::vector<Rect>& occupied,
    const LabelAvoidOptions& options = {}) noexcept;

// 批量：按给定顺序逐个放置；每个成功放置的标签会**自动加入占用集**，
// 因此同批标签之间也不会互相重叠。返回与输入等长的落点列表。
[[nodiscard]] TERMCHART_API std::vector<LabelSlot> place_labels(
    const std::vector<LabelBox>& labels, std::vector<Rect> occupied,
    const LabelAvoidOptions& options = {});

}  // namespace termchart::layout

// ===== include/termchart/layout/layout.hpp =====
// layout.hpp —— 图表布局公共类型与工具
//
// layout/ 是 stage 3 的几何层：在 canvas（格粒度画布）之上做"轴/网格/图例/标题"
// 的划分与绘制，本身不碰渲染管线（render 层更高，chart 不依赖 render）。所有
// 绘制函数直接作用于 canvas::Canvas，与未来的 GUI 平行栈共用同一套几何。



namespace termchart::layout {

// 四边留白（单位：格）。默认给 y 轴刻度标签留出左侧空间、给 x 轴标签留出底部。
struct Margin {
    double top = 1.0;
    double right = 1.0;
    double bottom = 3.0;
    double left = 8.0;
};

enum class Side { Left, Right, Top, Bottom };
enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

// 把刻度值格式化成短标签：大数少小数位，小数多小数位。确定性强，便于测试。
TERMCHART_API std::string format_tick(double value) noexcept;

}  // namespace termchart::layout

// ===== include/termchart/layout/auto_margin.hpp =====
// auto_margin.hpp —— 由内容反推四边留白
//
// 终端图表的留白不是美学问题，而是"标签放得下吗"的问题：y 刻度标签占左边、
// x 刻度标签占下边、标题占上边、图例占右边。把这几项**显式建模**成输入再算，
// 比在 chart 里散落一堆硬编码数字（然后每次换数据就压字）可靠得多。
//
// 口径：所有留白**向上取整到整格**（字符网格上没有半格），并保证每边至少为 padding。



namespace termchart::layout {

struct MarginInputs {
    std::uint32_t y_tick_label_width = 0;  // y 刻度标签最大显示宽度（列）
    std::uint32_t y_tick_label_gap = 1;    // 标签与轴线之间的空隙（列）
    std::uint32_t x_tick_label_height = 0;  // x 刻度标签占的行数（通常 1）
    std::uint32_t x_tick_label_gap = 1;     // 标签与轴线之间的空隙（行）
    // x 刻度标签最宽宽度：最右一个标签是**居中**在刻度上的，所以右侧只需预留一半。
    std::uint32_t x_tick_label_width = 0;
    std::uint32_t title_lines = 0;    // 标题占的行数
    std::uint32_t legend_width = 0;   // 图例块宽度（列）；0 表示不画图例
    std::uint32_t legend_gap = 1;     // 图例与绘图区之间的空隙
    std::uint32_t axis_label_rows = 0;  // x 轴名占的行数
    double padding = 1.0;             // 四边最小留白
};

// 由建模后的输入算四边留白。
[[nodiscard]] TERMCHART_API Margin compute_margins(const MarginInputs& inputs) noexcept;

// 便捷：直接从刻度标签文本算（取最大**显示宽度**，宽字符按 2 列计）。
// base 里的其余字段（标题行数、图例宽度等）照常生效；标签相关的三个字段被覆盖。
[[nodiscard]] TERMCHART_API Margin compute_margins_for(
    const std::vector<std::string>& y_tick_labels,
    const std::vector<std::string>& x_tick_labels, const MarginInputs& base = {});

}  // namespace termchart::layout

// ===== include/termchart/layout/box.hpp =====
// box.hpp —— 绘图区（PlotArea）：画布留白 + 数据↔格 比例尺
//
// 一个 PlotArea 把"画布尺寸 + 四边留白 + 数据域"固化成两个 LinearScale：
//   * x_scale_：数据 x → [inner.left, inner.right]
//   * y_scale_：数据 y → [inner.bottom, inner.top]（注意 y 反转，格坐标向下增长）
// 于是 chart 只需 cell_of(x,y) 就能把任意数据点落到正确的格。



namespace termchart::layout {

class PlotArea {
public:
    PlotArea(double canvas_w, double canvas_h, Margin margin, double data_xmin,
             double data_xmax, double data_ymin, double data_ymax) noexcept
        : canvas_w_(canvas_w),
          canvas_h_(canvas_h),
          margin_(margin),
          inner_(margin.left, margin.top,
                 canvas_w - margin.left - margin.right,
                 canvas_h - margin.top - margin.bottom),
          x_scale_(data_xmin, data_xmax, inner_.x, inner_.right()),
          y_scale_(data_ymin, data_ymax, inner_.bottom(), inner_.y) {}

    [[nodiscard]] const Rect& inner_rect() const noexcept { return inner_; }
    [[nodiscard]] const scale::LinearScale& x_scale() const noexcept { return x_scale_; }
    [[nodiscard]] const scale::LinearScale& y_scale() const noexcept { return y_scale_; }
    [[nodiscard]] Margin margin() const noexcept { return margin_; }

    // 数据坐标 → 浮点格坐标（交给 canvas 向下取整）。
    [[nodiscard]] Point cell_of(double x, double y) const noexcept {
        return Point{x_scale_.forward(x), y_scale_.forward(y)};
    }

private:
    double canvas_w_;
    double canvas_h_;
    Margin margin_;
    Rect inner_;
    scale::LinearScale x_scale_;
    scale::LinearScale y_scale_;
};

}  // namespace termchart::layout

// ===== include/termchart/layout/annotation.hpp =====
// annotation.hpp —— 参考线、区域高亮、标注
//
// 三者都是"叠在数据之上"的辅助标记，共用 `PlotArea` 的数据 ↔ 格映射，并且一律
// **裁剪在绘图区内**（不会画到轴标签上，也不会溢出画布）。
//
// 区域高亮的语义刻意做成"只改背景色"：终端里"高亮一个区间"应当是让原有数据
// 换底色，而不是把数据涂掉。需要填充字符时才用 fill_char。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

enum class Orientation {
    Horizontal,  // 水平：跨整个绘图区宽度，用 y 值定位
    Vertical,    // 垂直：跨整个绘图区高度，用 x 值定位
};

struct ReferenceLine {
    Orientation orientation = Orientation::Horizontal;
    double value = 0.0;             // 数据坐标：水平线是 y 值，垂直线是 x 值
    char32_t cp = U'\u2504';        // 默认 ┄（虚线，不喧宾夺主）
    Style style{};
};

struct HighlightBand {
    Orientation orientation = Orientation::Horizontal;
    double from = 0.0;  // 数据坐标（含）
    double to = 0.0;    // 数据坐标（含）；顺序可反
    Color color = Color::ansi16(8);
    // 0 表示不改字符、只改背景色（保留数据内容）；非 0 则用该字符填充整条带。
    char32_t fill_char = U'\0';
};

struct Annotation {
    Point at{};  // 数据坐标锚点
    std::string text;
    Style style{};
};

// 返回 false 表示取值落在数据域之外，或算出的行/列不在绘图区内（此时不画）。
[[nodiscard]] TERMCHART_API bool draw_reference_line(canvas::Canvas& canvas,
                                                     const PlotArea& area,
                                                     const ReferenceLine& line);

// 返回被改动的格数（已裁剪到绘图区；宽字符续接格跳过，由头部统一着色）。
[[nodiscard]] TERMCHART_API std::size_t draw_highlight_band(canvas::Canvas& canvas,
                                                            const PlotArea& area,
                                                            const HighlightBand& band);

// 标注落点用通用避让（layout/label_avoid）在锚点四周找位，并限制在绘图区内。
// 放不下时返回 false 且**什么都不画** —— 宁可少一个标注，也不覆盖已有内容。
[[nodiscard]] TERMCHART_API bool draw_annotation(canvas::Canvas& canvas,
                                                 const PlotArea& area,
                                                 const Annotation& annotation,
                                                 const std::vector<Rect>& occupied = {});

}  // namespace termchart::layout

// ===== include/termchart/layout/axis.hpp =====
// axis.hpp —— 坐标轴绘制（左 y 轴 / 下 x 轴）：轴体 + 刻度 + 数字标签 + 轴名



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

// 下 x 轴：在 inner 底边画横线，向上长出刻度并标注数值；label 居中置于轴下方。
TERMCHART_API void draw_axis_x(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks,
                               std::string_view label = {},
                               const Style& style = Style{});

// 左 y 轴：在 inner 左边画竖线，向右长出刻度并标注数值；label 置于轴左侧。
TERMCHART_API void draw_axis_y(canvas::Canvas& canvas, const PlotArea& area,
                               const std::vector<double>& ticks,
                               std::string_view label = {},
                               const Style& style = Style{});

}  // namespace termchart::layout

// ===== include/termchart/layout/grid.hpp =====
// grid.hpp —— 网格线（虚线），在刻度位置铺背景参照



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_grid(canvas::Canvas& canvas, const PlotArea& area,
                             const std::vector<double>& x_ticks,
                             const std::vector<double>& y_ticks,
                             const Style& style = Style{});

}  // namespace termchart::layout

// ===== include/termchart/layout/label.hpp =====
// label.hpp —— 轴名等文本标签



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_axis_label(canvas::Canvas& canvas, std::string_view text,
                                   const Rect& rect, Side side,
                                   HAlign halign = HAlign::Center,
                                   const Style& style = Style{});

}  // namespace termchart::layout

// ===== include/termchart/layout/legend.hpp =====
// legend.hpp —— 图例：色块 + 名称，水平排布在给定矩形内



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

struct LegendEntry {
    std::string name;
    Color color = Color::ansi16(7);
};

TERMCHART_API void draw_legend(canvas::Canvas& canvas,
                               std::span<const LegendEntry> entries, const Rect& rect,
                               HAlign halign = HAlign::Left,
                               const Style& base = Style{});

}  // namespace termchart::layout

// ===== include/termchart/layout/title.hpp =====
// title.hpp —— 标题：在矩形顶部居中（或按对齐）绘制



namespace termchart::canvas {
class Canvas;
}

namespace termchart::layout {

TERMCHART_API void draw_title(canvas::Canvas& canvas, std::string_view title,
                              const Rect& rect, HAlign align = HAlign::Center,
                              const Style& style = Style{});

}  // namespace termchart::layout

// ===== include/termchart/chart/context.hpp =====
// context.hpp —— 图表绘制上下文（DrawContext）
//
// 从 `chart.hpp` 独立出来（一个上下文、一个基类、一个系列类型，各自成文件）：
// 上下文描述"这一次绘制用什么开关和样式"，与 `Chart` 基类解耦后，GUI 侧与
// 文档导出路径可以构造自己的上下文（例如不画轴、不画标题、只用网格），
// 而不必继承或伪造一个 Chart。
//
// 所有开关默认与既有行为一致（全开），因此老调用点只需 `DrawContext{canvas}`。


namespace termchart::chart {

struct DrawContext {
    canvas::Canvas& canvas;

    // 画什么（默认全开）。
    bool draw_title = true;
    bool draw_axes = true;
    bool draw_grid = true;
    bool draw_legend = true;

    // 样式覆盖：`fg()` 为 None 时图表用各自的默认配色（网格是暗灰、轴是默认前景）。
    Style grid_style{};
    Style axis_style{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/node.hpp =====
// node.hpp —— hierarchical/ 共享的树节点与布局辅助（自给，不依赖 graph/）
//
// chart 是 L12、graph 是 L13，`chart > graph` 是禁止边（cmake/TermchartModules.cmake），
// 因此层次图的数据输入（HierNode）与"聚合 / 配色 / 首码点"等小工具都在这里自给，
// 不 include graph/ 的任何布局器。规格表（结构.md 附二）只列了 6 个图种头，
// 本文件是为避免 6 处重复而增设的子目录内共享头，随 6 个图种一起交付。



namespace termchart::chart::hier {

// 层次图通用输入树：叶子携带度量，内部节点 value<=0 表示由子节点聚合。
struct HierNode {
    std::string name;
    double value = 0.0;
    Color color = Color::none();  // none → 按第一层分支自动配色
    std::vector<HierNode> children;

    [[nodiscard]] bool is_leaf() const noexcept { return children.empty(); }
};

// 自底向上聚合：叶子取 max(value, 0)，内部节点 value<=0 时覆盖为子树和。
// 返回根的聚合值。
TERMCHART_API double aggregate_values(HierNode& node);

// 确定性默认配色：按分支序号循环取 ANSI 16 色（跳过黑/白，保证可见）。
TERMCHART_API Color hier_palette(std::size_t branch) noexcept;

// 给整棵树补默认色：depth==1 的节点按序取 palette，后代继承所在分支的色。
TERMCHART_API void assign_colors(HierNode& node, std::size_t branch = 0,
                                 std::size_t depth = 0);

// 取字符串首个 UTF-8 码点（非法序列容错），供"一格标签"用；空串返回空格。
TERMCHART_API char32_t first_codepoint(const std::string& text);

}  // namespace termchart::chart::hier

// ===== include/termchart/chart/hit_region.hpp =====
// hit_region.hpp —— 通用命中区域（chart → interaction 的唯一契约）
//
// 交互层要能判断"点到了哪个图元"，但不能认识任何具体图表类型。解决办法：
// chart 侧登记若干 HitRegion（形状 + 几何 + **不透明 payload id**），交互层只
// 按形状做几何判定并回报 id。于是 interaction 不需要 switch 图表名——这正是
// ROADMAP 约束"hit_test 只 switch HitRegion::Shape，禁图表名"的落点。



namespace termchart::chart {

enum class HitShape {
    Rect,
    Circle,
    Polygon,
};

class HitRegion {
public:
    HitRegion() = default;

    static HitRegion rect(Rect bounds, std::uint32_t payload = 0);
    static HitRegion circle(Point center, double radius, std::uint32_t payload = 0);
    static HitRegion polygon(std::vector<Point> points, std::uint32_t payload = 0);

    [[nodiscard]] HitShape shape() const noexcept { return shape_; }
    // 不透明标识：interaction 只负责原样回报，不解释含义。
    [[nodiscard]] std::uint32_t payload() const noexcept { return payload_; }
    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }

    [[nodiscard]] TERMCHART_API bool contains(double x, double y) const noexcept;

private:
    HitShape shape_ = HitShape::Rect;
    Rect bounds_{};
    double radius_ = 0.0;
    std::vector<Point> points_;
    std::uint32_t payload_ = 0;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/series.hpp =====
// series.hpp —— 数据系列（值类型，header-only）
//
// 顺带放**默认配色的唯一真源**：它在本文件而不是 chart.hpp，是因为 chart.hpp
// include 了本文件，反过来会成环。



namespace termchart::chart {

// 历史上每个图种各自硬编码 `Color::ansi16(7/2/1/4/8/6/3)`：同一件事七处写法，
// 将来配色表（palette）落地时要改得动，必须先收敛到一处。
// **色号本身没变**（逐个映射的结果与原来逐字相同），这一步只做"命名"。
enum class DefaultColor {
    Series,   // 7：中性前景，数据与系列默认
    Accent,   // 2：绿，正向 / 主强调
    Warn,     // 1：红，负向 / 告警
    Neutral,  // 4：蓝，统计类默认
    Muted,    // 8：暗灰，次要元素（轨道、连线）
    Cyan,     // 6：青，金融 / 极坐标类
    Amber,    // 3：黄，星标等点缀
};

inline Color default_color(DefaultColor which = DefaultColor::Series) noexcept {
    switch (which) {
        case DefaultColor::Accent:
            return Color::ansi16(2);
        case DefaultColor::Warn:
            return Color::ansi16(1);
        case DefaultColor::Neutral:
            return Color::ansi16(4);
        case DefaultColor::Muted:
            return Color::ansi16(8);
        case DefaultColor::Cyan:
            return Color::ansi16(6);
        case DefaultColor::Amber:
            return Color::ansi16(3);
        case DefaultColor::Series:
            break;
    }
    return Color::ansi16(7);
}

struct Series {
    std::string name;
    Color color = default_color(DefaultColor::Series);
    std::vector<Point> points;  // 数据坐标 (x, y)
};

}  // namespace termchart::chart

// ===== include/termchart/chart/chart.hpp =====
// chart.hpp —— Chart 抽象基类
//
// chart/ 是 stage 3 的顶点：把数据系列 + 比例尺 + layout 拼成可见图表，画进
// canvas::Canvas。**不碰 render 层**（架构禁止边），于是未来 to_document()
// 通道独立存在。绘制上下文见 context.hpp。



namespace termchart::chart {

// 折线/柱共享的字符与几何辅助（定义见 chart.cpp）。
inline constexpr char32_t kLineChar = U'\u2022';     // • 连线
inline constexpr char32_t kMarkerChar = U'\u25CF';   // ● 数据点
inline constexpr char32_t kBarChar = U'\u2588';      // █ 柱

// 默认配色的唯一真源在 `chart/series.hpp`（`DefaultColor` / `default_color()`）——
// 放在那里是因为 chart.hpp 自己 include series.hpp，反过来不行。

void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax,
                  double& ymin, double& ymax);
layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin,
                           double xmin, double xmax, double ymin, double ymax,
                           bool auto_domain, bool include_zero);
void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series,
                     const layout::PlotArea& area, bool show);

class Chart {
public:
    virtual ~Chart();
    virtual void draw(DrawContext ctx) = 0;

    void set_title(std::string title) { title_ = std::move(title); }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }

    void set_margins(double top, double right, double bottom, double left) {
        margin_top_ = top;
        margin_right_ = right;
        margin_bottom_ = bottom;
        margin_left_ = left;
    }
    [[nodiscard]] double margin_top() const noexcept { return margin_top_; }
    [[nodiscard]] double margin_right() const noexcept { return margin_right_; }
    [[nodiscard]] double margin_bottom() const noexcept { return margin_bottom_; }
    [[nodiscard]] double margin_left() const noexcept { return margin_left_; }

protected:
    std::string title_;
    double margin_top_ = 1.0;
    double margin_right_ = 1.0;
    double margin_bottom_ = 3.0;
    double margin_left_ = 8.0;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/area.hpp =====
// area.hpp —— 笛卡尔面积图
//
// 与折线图同构：网格 → 坐标轴+刻度 → 折线轮廓 + 向 baseline 的逐列填充 →
// 图例 → 标题。填充按"列"做（每列求出曲线高度，填 [曲线, 基线) 之间的格），
// 与格粒度画布天然对齐，也直接复用 canvas 的既有原语，不自写扫描线。
//
// 单系列：面积图的堆叠/百分比/流图变体是后续扩展（见 ROADMAP），这里只做
// 最基础的"折线到基线"一档。



namespace termchart::chart {

class AreaChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<Point> points,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "series");

    // 填充基线（数据坐标）。默认 0：最常见的"从零起"面积图。改它会同时改
    // 定义域（基线被纳入 y 域）与填充落点。
    void set_baseline(double baseline) { baseline_ = baseline; }
    // 填充字符。默认低调的 '·'：不与折线/标记抢视觉，也便于测试单独计数。
    void set_fill_char(char32_t cp) { fill_char_ = cp; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    double baseline_ = 0.0;
    char32_t fill_char_ = U'\u00B7';
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/bar_chart.hpp =====
// bar_chart.hpp —— 笛卡尔柱状图（单系列，垂直柱）
//
// 与 LineChart 同构：网格 → 轴 → 从基线起按值填充竖条（█）→ 图例 → 标题。
// 分组/堆叠在多系列场景才需要，作为后续扩展（见 ROADMAP）。



namespace termchart::chart {

class BarChart final : public Chart {
public:
    // 柱用单系列即可；values 为每个类别的值，categories 为对应名称。
    void set_values(std::vector<double> values, std::vector<std::string> categories,
                    Color color = default_color(DefaultColor::Accent), std::string name = "series");
    void set_show_legend(bool on) { show_legend_ = on; }

    void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    std::vector<std::string> categories_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/boxplot.hpp =====
// boxplot.hpp —— 箱线图：五数概括 + 须 + 离群点
//
// 终端字符化策略：箱体（q1–q3）画成 █ 实心块，中位线用 ─ 覆盖箱体中行，
// 上下须是 │ 竖线、须端帽是 ─ 短横，离群点用 ○ 单字符。输入直接接受
// 五数概括（min/q1/med/q3/max），不在 chart 层重复实现分位数算法；
// 违反 lo≤q1≤med≤q3≤hi 的组整体剔除。



namespace termchart::chart {

class BoxPlotChart final : public Chart {
public:
    struct BoxStats {
        double lo = 0.0;   // 须下端（通常 min，不含离群点）
        double q1 = 0.0;
        double med = 0.0;
        double q3 = 0.0;
        double hi = 0.0;   // 须上端
        std::vector<double> outliers;  // 须外单点
    };
    struct Options {
        Color color = default_color(DefaultColor::Neutral);
        bool show_outliers = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<BoxStats> groups) { groups_ = std::move(groups); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<BoxStats> groups_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/bubble.hpp =====
// bubble.hpp —— 气泡图：散点 + 第三维（半径）
//
// 终端字符化策略：终端格没有"面积"可缩放，第三维用**点阵符号的大小阶梯**
// 表达——r 归一化到 [rmin, rmax] 后取 · → • → ● 三档，越大越显眼；
// 全体同值时取中档（此时第三维不携带信息）。



namespace termchart::chart {

class BubbleChart final : public Chart {
public:
    struct Bubble {
        Point p{};
        double r = 0.0;  // 第三维：非负半径语义，负值/非有限按 0 处理
    };
    struct Options {
        Color color = default_color(DefaultColor::Neutral);
        std::string name = "bubble";
        bool show_legend = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Bubble> bubbles) { bubbles_ = std::move(bubbles); }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Bubble> bubbles_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/bullet.hpp =====
// bullet.hpp —— 子弹图：每行一条水平刻度条，定性背景带 + 实心度量条 + 目标竖线。
//
// 终端字符化策略：背景带用 ░▒▓ 三档明暗区分定性区间，度量值用 █ 实心条，目标用 │ 竖线，
// 全部信息靠字符明暗与位置表达，不依赖颜色。



namespace termchart::chart {

class BulletChart final : public Chart {
public:
    struct Row {
        std::string label;
        double value = 0.0;                  // 度量值（实心条）
        double target = 0.0;                 // 目标值（竖线）
        std::array<double, 3> bands{};       // 三段定性背景带的递增右端点
    };

    struct Options {
        char32_t target_char = U'\u2502';    // │
        Color bar_color = default_color(DefaultColor::Neutral);
        bool show_legend = false;
    };

    TERMCHART_API void set_rows(std::vector<Row> rows);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Row> rows_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/candlestick.hpp =====
// candlestick.hpp —— K 线图：OHLC → 实体 + 影线
//
// 终端字符化策略：开盘-收盘区间画成 █ 实体块（涨/跌分色），最高-最低画成
// │ 影线（先画影线再画实体，实体自然覆盖中段）；开盘=收盘（十字星）时实体
// 退化为单行 ─。x 轴是 K 线序号（第 i 根），不是连续数值。



namespace termchart::chart {

class CandlestickChart final : public Chart {
public:
    struct Ohlc {
        double open = 0.0;
        double high = 0.0;
        double low = 0.0;
        double close = 0.0;
    };
    struct Options {
        Color up = default_color(DefaultColor::Accent);
        Color down = default_color(DefaultColor::Warn);
        bool show_wicks = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Ohlc> candles) { candles_ = std::move(candles); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Ohlc> candles_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/contour.hpp =====
// contour.hpp —— 等高线：标量场 → marching squares
//
// 终端字符化策略：对每条等值线跑 marching squares 提取折线段，段画成 · 连线
// （draw_line 自带裁剪与取整）。场按**行**给定且 row 0 = 最上 = ymax；等值线
// 级数缺省自动取值域内均分（不含端点），也可显式指定。



namespace termchart::chart {

class ContourChart final : public Chart {
public:
    struct Options {
        Color color = default_color(DefaultColor::Cyan);
        // 显式等值线级；非有限值剔除。为空时按 level_count 自动均分。
        std::vector<double> levels;
        int level_count = 4;  // 自动模式的级数，clamp 到 [1, 16]
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    // rows[j][i]：第 j 行第 i 列的场值，j=0 为最上（对应 ymax）。
    void set_field(std::vector<std::vector<double>> rows) { rows_ = std::move(rows); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<std::vector<double>> rows_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/dumbbell.hpp =====
// dumbbell.hpp —— 哑铃图：每行一个类别，同一行内两个数值之间用"线段 + 两端实心点"呈现差异。
//
// 终端字符化策略：行内差异用 • 连线、● 标两端值，一行一类别，不依赖颜色也能读出差距。



namespace termchart::chart {

class DumbbellChart final : public Chart {
public:
    struct Row {
        std::string label;  // 类别名（画在左侧留白，超宽被画布裁剪）
        double a = 0.0;     // 端点一
        double b = 0.0;     // 端点二
    };

    struct Options {
        Color line_color = default_color(DefaultColor::Muted);  // 连线颜色（暗灰，突出端点）
        Color dot_color = default_color(DefaultColor::Neutral);
        bool show_legend = false;  // 单系列图例意义不大，默认关
    };

    TERMCHART_API void set_rows(std::vector<Row> rows);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Row> rows_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/errorbar.hpp =====
// errorbar.hpp —— 误差棒：中心点 + 不对称误差区间
//
// 终端字符化策略：数据点画 ●，y 误差区间是 │ 竖线 + ─ 端帽，x 误差区间是
// ─ 横线 + │ 端帽；误差上下界支持不对称（lo/hi 独立给定，lo>hi 自动交换）。



namespace termchart::chart {

class ErrorBarChart final : public Chart {
public:
    struct ErrorPoint {
        double x = 0.0;
        double y = 0.0;
        double y_lo = 0.0;  // y 误差下界；非有限表示该向无误差
        double y_hi = 0.0;
        double x_lo = 0.0;
        double x_hi = 0.0;
    };
    struct Options {
        Color color = default_color(DefaultColor::Neutral);
        bool show_caps = true;  // 误差端帽
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<ErrorPoint> points) { points_ = std::move(points); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<ErrorPoint> points_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/gantt.hpp =====
// gantt.hpp —— 甘特图：任务时间条 + 行标签
//
// 终端字符化策略：每个任务占一行，时间条是按 [start, end] 落在时间轴上的
// █ 条带（行内垂直居中）；行标签写在左侧留白里（按码点截断，不撕裂 UTF-8），
// 行间只画垂直时间网格线（横向网格线对任务行没有意义）。



namespace termchart::chart {

class GanttChart final : public Chart {
public:
    struct Task {
        std::string name;
        double start = 0.0;
        double end = 0.0;
    };
    struct Options {
        Color bar = default_color(DefaultColor::Neutral);
        bool show_grid = true;
        bool show_labels = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Task> tasks) { tasks_ = std::move(tasks); }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<Task> tasks_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/heatmap.hpp =====
// heatmap.hpp —— 笛卡尔热力图（二维点集 → 网格 → 计数 → 连续色表着色）
//
// 与"矩阵热力图"不同：输入是**散点坐标**而非规整矩阵，因此要先落到一个网格
// （由数据范围与单元边长决定），统计每格点数，再把计数归一化后用连续色表上色。
// 密度同时由"字符"与"颜色"两条通道编码——单一字符表达不了连续量，单一颜色在
// 低对比终端上也可能不可读；两者叠加才既可见又可比。
//
// 网格**先算后画**：先把所有点落格得到计数矩阵（索引一律 clamp，绝不出现负索引
// 折回），再把非空格画进画布、并对每个格矩形与内框求交做边界裁剪。



namespace termchart::chart {

class HeatmapChart final : public Chart {
public:
    // points 为数据坐标下的散点；kind 指定连续色表；name 为图例名。
    TERMCHART_API void set_values(std::vector<Point> points,
                                  color::ColormapKind kind = color::ColormapKind::Viridis,
                                  std::string name = "heatmap");

    // 网格单元在**数据坐标**下的边长（x/y 同值）。传 <=0 表示自动（按点数选）。
    void set_cell_size(double size) { cell_size_ = size; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何（含定义域端点有限性）。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    color::ColormapKind kind_ = color::ColormapKind::Viridis;
    std::string name_ = "heatmap";
    double cell_size_ = 0.0;  // <=0：自动
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/hist.hpp =====
// hist.hpp —— 笛卡尔直方图（原始样本 → 自动分箱 → 柱）
//
// 与柱状图同构，但输入是**未聚合的原始样本**：先自动分箱并统计每箱频数，再按箱
// 画柱。分箱规则内建而非由调用方给定，是因为"箱数该随样本量增长"本身就是直方图
// 的语义（样本越多、分辨率应越细）；调用方仍可用 set_bins 覆盖。分箱结果通过
// bin_edges()/bin_counts() 暴露，便于测试与上层复用（例如叠加密度曲线）。



namespace termchart::chart {

class HistChart final : public Chart {
public:
    // values 为原始样本（未分箱）。color/name 默认值沿用柱状图，便于并排绘制。
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "series");

    // 覆盖自动分箱的箱数；返回前会 clamp 到 [1, 64]。传 <=0 恢复自动（Sturges）。
    void set_bins(int bins) {
        bins_ = bins;
        rebin();
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何（含定义域端点有限性）。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

    // 最近一次输入对应的分箱结果（set_values/set_bins 后立即有效）。
    [[nodiscard]] std::size_t bin_count() const noexcept {
        return edges_.empty() ? 0 : edges_.size() - 1;
    }
    [[nodiscard]] const std::vector<double>& bin_edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<std::size_t>& bin_counts() const noexcept {
        return counts_;
    }

private:
    // 由 set_values/set_bins 触发的确定性分箱（剔除非有限样本后重算）。
    void rebin();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    int bins_ = 0;  // 0（或负）：自动（Sturges）
    bool show_legend_ = true;
    std::vector<double> edges_;          // bins+1 个边界（升序）
    std::vector<std::size_t> counts_;    // bins 个频数
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/line_chart.hpp =====
// line_chart.hpp —— 笛卡尔折线图
//
// 把若干 Series 画进 Canvas：网格 → 坐标轴+刻度 → 折线（含数据点标记）→
// 图例 → 标题。域可自动按数据范围 + 5% 留白 + nice 对齐（scale::nice_domain）。



namespace termchart::chart {

class LineChart final : public Chart {
public:
    // 追加一条系列（append 语义）。
    void add_series(Series s) { series_.push_back(std::move(s)); }
    // 替换全部系列（replace 语义）。**两个动词成对提供**：全库约定 `add_*` 是追加、
    // `set_*` 是替换，只给一半会逼调用方自己 clear()，而 clear() 并不总是对外可见。
    void set_series(std::vector<Series> series) { series_ = std::move(series); }
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }

    void set_auto_domain(bool on, bool include_zero = false) {
        auto_domain_ = on;
        include_zero_ = include_zero;
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/mosaic.hpp =====
// mosaic.hpp —— 马赛克（marimekko）：列宽∝第一变量边际、列内高度∝第二变量占比。
//
// 终端字符化策略：列内自下而上按行类别堆叠 █ 色块，列边界画 │ 分隔，
// 边际与条件分布同时靠"宽"与"高"两条几何通道编码，无需坐标轴即可比较。



namespace termchart::chart {

class MosaicChart final : public Chart {
public:
    struct Options {
        bool show_legend = true;   // 行类别图例
        bool show_separators = true;
    };

    // counts[r][c]：行类别 r 在列类别 c 的计数；负值按 0 处理。
    TERMCHART_API void set_counts(std::vector<std::vector<double>> counts,
                                  std::vector<std::string> row_labels,
                                  std::vector<std::string> col_labels);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<std::vector<double>> counts_;
    std::vector<std::string> row_labels_;
    std::vector<std::string> col_labels_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/parallel.hpp =====
// parallel.hpp —— 平行坐标：每个维度一条竖轴均分画布宽，每条观测跨轴连成折线。
//
// 终端字符化策略：各维独立 min-max 归一化到竖轴后逐段画 • 折线（颜色按观测轮换），
// 高维相关模式在二维终端上靠"线的走向"直接可读。



namespace termchart::chart {

class ParallelChart final : public Chart {
public:
    struct Options {
        bool show_axes = true;     // 每维竖轴 + 顶部维度名
        bool show_markers = false; // 观测折线的顶点是否加 ●
    };

    TERMCHART_API void set_axes(std::vector<std::string> names,
                                std::vector<std::vector<double>> observations);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<std::string> names_;
    std::vector<std::vector<double>> observations_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/qq.hpp =====
// qq.hpp —— QQ 图：样本分位数对理论正态分位数逐点落格，附参考线判断正态性。
//
// 终端字符化策略：x 轴为理论正态分位数（Acklam 逆逼近），y 轴为排序样本值，
// 点用 ●，参考线 y=μ+σx 用 •；点贴线即正态，弯曲形态即偏差方向。



namespace termchart::chart {

class QQChart final : public Chart {
public:
    struct Options {
        bool show_line = true;  // y = μ + σ·x 参考线
        Color point_color = default_color(DefaultColor::Neutral);
    };

    TERMCHART_API void set_sample(std::vector<double> sample);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> sample_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/ridgeline.hpp =====
// ridgeline.hpp —— 山脊图：多组分布按行堆叠，密度轮廓用块条填充，行间部分重叠成"山脊"。
//
// 终端字符化策略：每组密度归一化后按列填充实心块（顶部分数行用下八分块），
// 后画的行覆盖先画的，天然形成前后遮挡的山脊观感。



namespace termchart::chart {

class RidgelineChart final : public Chart {
public:
    struct Group {
        std::string name;
        std::vector<double> values;
        Color color = default_color(DefaultColor::Series);
    };

    struct Options {
        int band_height = 4;   // 每组最大密度对应的行数（≥2）
        int bins = 24;         // 直方分箱数（clamp 到 [4,128]）
        bool show_legend = true;
    };

    TERMCHART_API void set_groups(std::vector<Group> groups);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Group> groups_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/scatter_chart.hpp =====
// scatter_chart.hpp —— 散点图（可选气泡权重）
//
// 与折线图的唯一区别：**不连线**，每个点独立标记。点可以带"权重"（气泡图）：
// 权重被归一化后映射到四档标记字符（· ○ ● ◉），而不是浮点半径 —— 字符网格上
// 画不出真正的半径，硬做只会得到看起来随机的大小。



namespace termchart::chart {

class ScatterChart final : public Chart {
public:
    TERMCHART_API void add_series(Series series);
    // 气泡系列：weights 与 series.points 按下标对应，缺的按 0 处理。
    TERMCHART_API void add_bubble_series(Series series, std::vector<double> weights);
    // 替换语义（与 add_* 成对提供，见 line_chart.hpp 的同名注释）。
    // 注意：替换系列会一并清掉气泡权重——两者按下标一一对应，留下半套会错位。
    TERMCHART_API void set_series(std::vector<Series> series);
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }

    void set_auto_domain(bool on, bool include_zero = false) {
        auto_domain_ = on;
        include_zero_ = include_zero;
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> series_;
    std::vector<std::vector<double>> weights_;  // 与 series_ 一一对应；空表示非气泡
    bool auto_domain_ = true;
    bool include_zero_ = false;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/sparkline.hpp =====
// sparkline.hpp —— 迷你图：单行趋势线，逐列取下八分块 ▁▂▃…█。
//
// 终端字符化策略：min-max 归一化到 0..8 后逐列取下八分块字符，一行即成图，
// 可直接嵌进状态栏或表格行；末列可选 ● 标出最新值。



namespace termchart::chart {

class SparklineChart final : public Chart {
public:
    struct Options {
        Color color = default_color(DefaultColor::Accent);
        bool show_last_marker = true;
    };

    TERMCHART_API void set_values(std::vector<double> values);
    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    Options options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/step.hpp =====
// step.hpp —— 笛卡尔阶梯图
//
// 与折线图同构，唯一区别是相邻点之间不直连，而走"横—竖—横"台阶：数据在采样
// 区间内保持某一档（前一档/后一档/中点换档），而不是线性过渡。适合表达离散
// 状态、累计量台阶、保持型信号等。三档语义见下面的 Where。



namespace termchart::chart {

class StepChart final : public Chart {
public:
    // 台阶落在哪一侧：
    //   Post —— 先水平后垂直：采样区间保持"前一点"的值（最常用的阶梯，默认）；
    //   Pre  —— 先垂直后水平：采样区间保持"后一点"的值；
    //   Mid  —— 在两点中点处换档，两侧各留半段台阶。
    enum class Where { Pre, Post, Mid };

    TERMCHART_API void set_values(std::vector<Point> points,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "series");
    void set_where(Where where) { where_ = where; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 暴露最后使用的绘图区，供测试断言几何。
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "series";
    Where where_ = Where::Post;
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/streamgraph.hpp =====
// streamgraph.hpp —— 流图：多层序列围绕居中基线上下堆叠，形成"流动的河"。
//
// 终端字符化策略：逐列从基线向上/向下堆叠各层，用 █ 实心块按层着色；
// 基线可选 silhouette（对称居中）或 wiggle（最小摆动），x 轴是观测序号。



namespace termchart::chart {

class StreamgraphChart final : public Chart {
public:
    enum class Baseline { Silhouette, Wiggle };

    struct Options {
        Baseline baseline = Baseline::Silhouette;
    };

    TERMCHART_API void add_series(Series series);
    // 替换语义（与 add_* 成对提供，见 line_chart.hpp 的同名注释）。
    TERMCHART_API void set_series(std::vector<Series> layers);
    [[nodiscard]] const std::vector<Series>& series() const noexcept { return layers_; }

    void set_options(const Options& options) { options_ = options; }
    [[nodiscard]] const Options& options() const noexcept { return options_; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Series> layers_;
    Options options_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/violin.hpp =====
// violin.hpp —— 小提琴图：核密度轮廓
//
// 终端字符化策略：每组样本先做高斯核密度估计（Silverman 带宽），再沿 y 方向
// 切成若干水平带，每带的密度映射为**以组中线为轴的对称 █ 条带宽度**——
// 密度越高条带越宽，轮廓自然呈小提琴形；中位数画 ─。



namespace termchart::chart {

class ViolinChart final : public Chart {
public:
    struct Options {
        Color color = default_color(DefaultColor::Neutral);
        int bands = 0;  // 密度带数；0（或负）= 自动（随绘图区高度，clamp 到 [4, 64]）
        bool show_median = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    // groups[g] 为第 g 组的原始样本；空组/全非有限组在 draw 时整体跳过。
    void set_data(std::vector<std::vector<double>> groups) {
        groups_ = std::move(groups);
    }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    Options opts_{};
    std::vector<std::vector<double>> groups_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/cartesian/waterfall.hpp =====
// waterfall.hpp —— 瀑布图：顺序增量/减量 + 小计
//
// 终端字符化策略：每段是**从累计基线画到新累计值**的 █ 柱（增/减/小计三色
// 区分语义，这是瀑布图区别于普通柱状图的关键）；相邻柱之间在上一段结束的
// 累计高度画 ─ 连接线，视觉上串起"流水"。x 轴是条目序号。



namespace termchart::chart {

class WaterfallChart final : public Chart {
public:
    struct Item {
        std::string label;
        double value = 0.0;
        bool is_total = false;  // true：累计重置为 value（基线回到 0）
    };
    struct Options {
        Color increase = default_color(DefaultColor::Accent);
        Color decrease = default_color(DefaultColor::Warn);
        Color total = default_color(DefaultColor::Neutral);
        bool show_connectors = true;
    };

    void set_options(Options opts) { opts_ = std::move(opts); }
    void set_data(std::vector<Item> items);

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // set_data 后的累计序列（与保留的条目一一对应，小计项为其自身值）。
    [[nodiscard]] const std::vector<double>& cumulative() const noexcept {
        return cumulative_;
    }

private:
    Options opts_{};
    std::vector<Item> items_;
    std::vector<double> cumulative_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/dataset.hpp =====
// dataset.hpp —— 数据集（系列集合 + 轴名 + 范围查询）
//
// `Dataset` 是"画图前的那份数据"：若干具名系列 + 两个轴名，并提供图表需要的
// 范围查询（自动定义域、是否含零）。它**不认识画布**，因此 GUI 侧、导出路径与
// 未来的 DSL 都能复用它作为中间表示 —— 这也是它独立成文件（而不是塞进 chart.hpp）
// 的原因。



namespace termchart::chart {

struct DataRange {
    double xmin = 0.0;
    double xmax = 0.0;
    double ymin = 0.0;
    double ymax = 0.0;
    bool valid = false;  // 没有任何点时 false，其余字段无意义
};

class Dataset {
public:
    Dataset() = default;

    TERMCHART_API void add(Series series);
    // 便捷：只有 y 值时 x 取 0..n-1（最常用的调用形态）。
    TERMCHART_API void add(std::string name, std::vector<double> values,
                           Color color = default_color(DefaultColor::Series));
    // 便捷：x 与 y 等长，长度不等时按较短者截断（不抛异常）。
    TERMCHART_API void add(std::string name, std::vector<double> xs,
                           std::vector<double> ys, Color color = default_color(DefaultColor::Series));

    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;   // 系列数
    [[nodiscard]] TERMCHART_API std::size_t point_count() const noexcept;  // 点数合计

    [[nodiscard]] const std::vector<Series>& series() const noexcept { return series_; }
    [[nodiscard]] std::vector<Series>& series() noexcept { return series_; }

    void set_x_label(std::string label) { x_label_ = std::move(label); }
    void set_y_label(std::string label) { y_label_ = std::move(label); }
    [[nodiscard]] const std::string& x_label() const noexcept { return x_label_; }
    [[nodiscard]] const std::string& y_label() const noexcept { return y_label_; }

    // 数据范围；include_zero 为真时把 0 纳入 y 域（柱状图需要）。
    [[nodiscard]] TERMCHART_API DataRange bounds(bool include_zero = false) const noexcept;

    TERMCHART_API void clear();

private:
    std::vector<Series> series_;
    std::string x_label_;
    std::string y_label_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/depth.hpp =====
// depth.hpp —— 深度图（订单簿买卖两侧阶梯填充）
//
// 字符化策略：买盘（bid）与卖盘（ask）各自按 (price, qty) 画阶梯，
// 阶梯线下方到量基线之间填 ░，两侧同框、以中间价自然分界。



namespace termchart::chart {

struct DepthOptions {
    Color bid = default_color(DefaultColor::Accent);  // 买盘（绿）
    Color ask = default_color(DefaultColor::Warn);  // 卖盘（红）
};

class DepthChart final : public Chart {
public:
    void set_book(std::vector<double> bid_price, std::vector<double> bid_qty,
                  std::vector<double> ask_price, std::vector<double> ask_qty);

    void draw(DrawContext ctx) override;

    DepthOptions options;

private:
    std::vector<double> bid_price_;
    std::vector<double> bid_qty_;
    std::vector<double> ask_price_;
    std::vector<double> ask_qty_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/drawdown.hpp =====
// drawdown.hpp —— 回撤图（水下曲线）
//
// 字符化策略：回撤线 '•' 逐点落格，线与 0 基线之间铺 '░'（水下填充）。
// 统计口径：dd_i = p_i / peak_i - 1（peak_i 为前缀最大价），恒 ≤ 0；y 域
// [min dd, 0]。非有限价格剔除，剩余不足 1 个不绘制。



namespace termchart::chart {

struct DrawdownOptions {
    Color color = default_color(DefaultColor::Warn);  // 水下红
    bool fill = true;                // 0 基线以下铺 '░'
};

class DrawdownChart final : public Chart {
public:
    void set_prices(std::vector<double> prices,
                    Color color = default_color(DefaultColor::Warn));

    void set_options(DrawdownOptions options) { options_ = options; }

    void draw(DrawContext ctx) override;

    // 回撤序列（与有效样本一一对应，恒 ≤ 0；无有效样本时为空）。
    [[nodiscard]] const std::vector<double>& drawdown() const noexcept { return dd_; }
    [[nodiscard]] double max_drawdown() const noexcept;
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> dd_;
    Color color_ = default_color(DefaultColor::Warn);
    DrawdownOptions options_{};
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/kagi.hpp =====
// kagi.hpp —— 卡吉图（转折摆动列，粗细表强弱）
//
// 字符化策略：按反转阈值抽出 zigzag 摆动，每个摆动占一列：竖段用
// ┃（阳：超过前一同向极值）/ │（阴），转折位用 ─ 连到下一列。
// reversal<=0 时自动取全距 5%。



namespace termchart::chart {

struct KagiOptions {
    double reversal = 0.0;  // <=0 → 自动 = 全距 * 5%
    Color yang = default_color(DefaultColor::Accent);
    Color yin = default_color(DefaultColor::Warn);
};

class KagiChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    KagiOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/point_figure.hpp =====
// point_figure.hpp —— 点数图（Point & Figure）
//
// 字符化策略：价格按 box_size 量化为格档，X 列（上行情）落 'X'、O 列（下行
// 情落 'O'，每档一格、沿列纵向堆叠；方向反转需回调 reversal 格（默认 3）。
// 统计口径：box = floor(p/box_size)；上行列只抬顶、下行列只沉底；box_size
// ≤ 0 时取全距/10，全距为 0 或价格不足 2 个不绘制。列的 top/bottom 以格档
// 号给出（top ≥ bottom，单档列为 top == bottom）。



namespace termchart::chart {

struct PointFigureOptions {
    double box_size = 0.0;  // ≤0 → 自动 = 全距/10
    int reversal = 3;       // 反转所需格数（≥1）
    Color up = default_color(DefaultColor::Accent);
    Color down = default_color(DefaultColor::Warn);
};

struct PfColumn {
    bool up = true;
    int top = 0;     // 最高格档号
    int bottom = 0;  // 最低格档号
};

class PointFigureChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    PointFigureOptions options;

    [[nodiscard]] const std::vector<PfColumn>& columns() const noexcept { return columns_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    std::vector<PfColumn> columns_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/renko.hpp =====
// renko.hpp —— 砖形图（固定砖值的价格阶梯）
//
// 字符化策略：价格每越过一档 brick_size 生成一块砖，升砖绿、降砖红，
// 每砖 2 格宽 █、沿时间轴逐格右移并上下阶梯。brick_size<=0 时取全距/10。



namespace termchart::chart {

struct RenkoOptions {
    double brick_size = 0.0;  // <=0 → 自动 = (max-min)/10
    Color up = default_color(DefaultColor::Accent);
    Color down = default_color(DefaultColor::Warn);
};

class RenkoChart final : public Chart {
public:
    void set_prices(std::vector<double> prices);

    void draw(DrawContext ctx) override;

    RenkoOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> prices_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/volume.hpp =====
// volume.hpp —— 成交量图（基线起柱，方向色）
//
// 字符化策略：每根柱占一个槽位、从量基线起填 █，高度 ∝ 成交量；
// 方向表（涨/跌）存在时分别用 up/down 色，缺省用中性灰。



namespace termchart::chart {

struct VolumeOptions {
    Color up = default_color(DefaultColor::Accent);    // 价涨柱
    Color down = default_color(DefaultColor::Warn);  // 价跌柱
    Color neutral = default_color(DefaultColor::Muted);
};

class VolumeChart final : public Chart {
public:
    // dirs 可空或与 values 等长：>0 涨、<0 跌、0/缺省中性。
    void set_values(std::vector<double> values, std::vector<int> dirs = {});

    void draw(DrawContext ctx) override;

    VolumeOptions options;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    std::vector<int> dirs_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/financial/yield_curve.hpp =====
// yield_curve.hpp —— 收益率曲线（到期期限 → 收益率）
//
// 字符化策略：点间以 '•' 连线、每个观测点落 '●'；x = 到期期限（绘制前按
// 升序重排，重复期限取首见），y = 收益率。非有限观测剔除。



namespace termchart::chart {

struct YieldCurveOptions {
    bool show_markers = true;
};

class YieldCurveChart final : public Chart {
public:
    TERMCHART_API void set_curve(std::vector<double> maturities,
                                 std::vector<double> yields,
                                 Color color = default_color(DefaultColor::Cyan),
                                 std::string name = "yield");

    void set_options(YieldCurveOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const std::vector<Point>& points() const noexcept { return points_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;  // (期限, 收益率)，按期限升序
    Color color_ = default_color(DefaultColor::Cyan);
    std::string name_ = "yield";
    YieldCurveOptions options_{};
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/flow/funnel.hpp =====
// funnel.hpp —— 漏斗图（逐段居中横条）
//
// 字符化策略：每个阶段占一行，条宽 ∝ value / 最大值、水平居中，填 █；
// 行首画名称文本（可选附值）。无轴，纯条 + 文本。



namespace termchart::chart {

struct FunnelStage {
    std::string name;
    double value = 0.0;
    Color color = Color::none();  // none → 按 stage 序号自动配色
};

struct FunnelOptions {
    bool show_values = true;  // 名称后附数值
};

class FunnelChart final : public Chart {
public:
    void set_stages(std::vector<FunnelStage> stages);

    // 选项走 set_options()/options()，与其余图种一致（**不再**是裸 public 成员）：
    // 裸成员让"改了选项但没重画"成为合法的沉默错误，也让将来加不变量（例如
    // show_values 与 stage 数目的约束）无处可放。
    void set_options(const FunnelOptions& options) { options_ = options; }
    [[nodiscard]] const FunnelOptions& options() const noexcept { return options_; }

    void draw(DrawContext ctx) override;

private:
    std::vector<FunnelStage> stages_;
    FunnelOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/flow/stream.hpp =====
// stream.hpp —— 流向图（ThemeRiver 语义）
//
// 规格里本文件是"转发 cartesian/streamgraph"的同层语义包装；但
// cartesian/streamgraph 属另一工作包且尚未落地，为不让本包空转，
// 这里先自给堆叠算法（各列按占比从基线向另一侧堆叠 █），待
// cartesian/streamgraph 落地后可改回纯转发。布局不依赖 graph/。



namespace termchart::chart {

struct StreamOptions {
    bool centered = true;  // true: ThemeRiver 中心对称；false: 自底向上堆叠
};

class StreamChart final : public Chart {
public:
    // 各系列点数必须一致（按 y 值逐列堆叠，x 仅作对齐索引）。
    void set_series(std::vector<Series> series);

    void draw(DrawContext ctx) override;

    StreamOptions options;

private:
    std::vector<Series> series_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/geo/choropleth.hpp =====
// choropleth.hpp —— 分级统计图（示意地图级别）
//
// **精度立场**：区域由调用方给定的**字符格集合**表达，不做真实地理投影、
// 不读取边界数据。格粒度轮廓有台阶是终端字符网格的固有限制，**不做假
// 精度**；本图只负责"值 → 分级色"这一步。分级：把区域值域均分成 bins
// 档，值越高灰阶越深（256 色灰阶段 232..255）。



namespace termchart::chart {

struct GeoRegion {
    std::string name;
    std::vector<Point> cells;   // 字符格坐标（画布格）
    double value = 0.0;
};

struct ChoroplethOptions {
    int bins = 5;   // 分级数，收敛到 [1, 8]
};

class Choropleth final : public Chart {
public:
    void add_region(GeoRegion region) { regions_.push_back(std::move(region)); }
    void set_options(const ChoroplethOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<GeoRegion> regions_;
    ChoroplethOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/geo/dot_density.hpp =====
// dot_density.hpp —— 点密度（示意地图级别）
//
// **精度立场**：点只落在调用方给定的区域格集合内，不做真实投影、不做
// 随机抖动（确定性优先），**不做假精度**。每 unit 单位数值画一个点
// （'•'），一格至多一个点；点数超出格数时截断到格数。



namespace termchart::chart {

struct DotDensityRegion {
    std::string name;
    std::vector<Point> cells;   // 字符格坐标（画布格）
    double value = 0.0;
    Color color = default_color(DefaultColor::Accent);
};

struct DotDensityOptions {
    double unit = 1.0;   // 每多少数值画一个点，收敛到 > 0
};

class DotDensity final : public Chart {
public:
    void add_region(DotDensityRegion region) { regions_.push_back(std::move(region)); }
    void set_options(const DotDensityOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t region_count() const noexcept { return regions_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<DotDensityRegion> regions_;
    DotDensityOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/geo/geo_map.hpp =====
// chart/geo/geo_map.hpp —— 真地图图种（GeoJSON + 投影 + 区域着色）
//
// 区别于 Choropleth：
//   * Choropleth：调用方给字符格集合（cells）+ 值，本库只画"值 → 颜色"。
//   * GeoMap：本库读 GeoJSON → 经纬度多边形 → 投影 → 字符格画多边形 + 按值着色。
//
// 设计选择：
//   * 输入：assets/maps/*.json（GeoJSON FeatureCollection）+ 一组 "feature_id → value"
//     映射（默认按 properties.name 匹配）；feature_id 与 name 都不匹配时该多边形灰显。
//   * 投影：等距矩形（默认）/ Mercator / 正射 / 兰勃特 —— enum 切换。
//   * 输出有三条，分辨率从低到高（共用同一份投影几何，不重复实现投影）：
//     1. draw(DrawContext)     —— 字符格 Bresenham 描边，兼容旧调用方（最低清）
//     2. rasterize(cols, rows) —— 子像素栅格化：盲文 2×4 / 象限 2×2 / 半块 1×2，
//                                 逐格保留分级色（一次出图即有 8× 分辨率与颜色）
//     3. render_bitmap(w, h)   —— 真彩色位图任意像素尺寸（GUI / PNG / sixel 的数据源）
//     仍不直接 emit Scene：Scene 走 0..1 归一化坐标，而地图是"经纬度 ↔ 目标网格"的
//     强契约，塞进归一化坐标就丢掉了栅格化时的子像素归属。



namespace termchart::chart {

enum class GeoProjection : std::uint8_t {
    Equirectangular, WebMercator, Orthographic, LambertCylindrical,
};

[[nodiscard]] TERMCHART_API const char* geo_projection_name(GeoProjection p) noexcept;

struct GeoMapOptions {
    GeoProjection projection = GeoProjection::Equirectangular;
    std::uint32_t width = 0;        // 0 = 自适应画布
    std::uint32_t height = 0;       // 0 = 自适应画布
    std::uint32_t bins = 5;         // 分级数（[1,8]）
    bool          show_graticule = false;   // 经纬网格
    std::int32_t  ortho_center_lat = 0;     // 正射投影中心纬度
    std::int32_t  ortho_center_lon = 0;     // 正射投影中心经度
    double        lambert_ref_lat = 0.0;    // 兰勃特参考纬度
};

// 子像素样式。后面的倍数是对字符格的分辨率倍数：
//   Braille   2×4 = 8×（最高清；单格内多区域时取覆盖面积最大的区域取色）
//   Quadrant  2×2 = 4×（U+2596–U+259F 象限块，字体覆盖好）
//   HalfBlock 1×2 = 2×（上下半，字形最通用）
// 刻意不提供 sextant（2×3）：U+1FB00 区字体覆盖差，且位模式顺序有特殊规则，
// 收益低于维护成本。
enum class GeoRasterStyle : std::uint8_t {
    Braille, Quadrant, HalfBlock,
};

[[nodiscard]] TERMCHART_API const char* geo_raster_style_name(GeoRasterStyle s) noexcept;

// 子像素数（每字符格）。给调用方换算"要多少字符格才能达到目标分辨率"。
[[nodiscard]] TERMCHART_API std::uint32_t geo_raster_subpixel_width(GeoRasterStyle s) noexcept;
[[nodiscard]] TERMCHART_API std::uint32_t geo_raster_subpixel_height(GeoRasterStyle s) noexcept;

struct GeoRasterOptions {
    GeoRasterStyle style = GeoRasterStyle::Braille;
    Rgb   no_data{120, 120, 120};                   // 无值多边形
    // 子像素点亮阈值：该子像素被多边形覆盖的面积比例 >= 此值才置位。
    double coverage = 0.35;
    bool   draw_borders = true;                     // 区域边界压一条暗边
    Color  border       = Color::rgb(30, 30, 34);
    // 值色带：t∈[0,1] 采样。默认 Turbo（与热力图同源，颜色排序单调）。
    color::ColormapKind colormap = color::ColormapKind::Turbo;
};

struct GeoBitmapOptions {
    double margin = 16.0;               // 四周留白（像素）
    Rgb    background{18, 18, 22};
    Rgb    no_data{120, 120, 120};
    // 区域边界色：填充后按"相邻像素覆盖率突变"检测边缘并压暗一条 1 像素线。
    // 与 background 同色即等于关闭描边。
    Rgb    border{28, 28, 32};
    // 纵向超采样（1 = 关）。水平方向本来就按精确重叠算覆盖率，纵向靠这个补。
    std::uint32_t supersample = 2;
    color::ColormapKind colormap = color::ColormapKind::Turbo;
};

class GeoMap final : public Chart {
public:
    GeoMap();
    ~GeoMap() override;

    GeoMap(const GeoMap&) = delete;
    GeoMap& operator=(const GeoMap&) = delete;

    // 加载 GeoJSON 文本（feature_id 默认按 properties.name 取）。
    [[nodiscard]] bool load_geojson(const std::string& text, std::string& err);
    [[nodiscard]] bool load_geojson_file(const std::string& path, std::string& err);

    // 叠加图层：把另一个 GeoJSON 追加到已加载数据之上，共用同一套投影、取景与
    // 值映射（先 load 底图，再 add_overlay 数据层）。
    // 栅格化时每个子像素记录"最后覆盖它的 feature"，所以后叠加的层天然画在上面；
    // 收口仍按覆盖面积最大者取色，不被蹭到的底图不会漏色。
    // 典型用法：国家轮廓当底图（不 set_value → 灰显）+ 城市气泡当数据层（有值 → 分级着色）。
    [[nodiscard]] bool add_overlay_geojson(const std::string& text, std::string& err);
    [[nodiscard]] bool add_overlay_geojson_file(const std::string& path, std::string& err);

    // 值映射：feature name → value。未命中的多边形不参与分级（仍按灰底画）。
    void set_value(const std::string& name, double value);
    void clear_values();

    void set_options(const GeoMapOptions& options) { options_ = options; }
    [[nodiscard]] const GeoMapOptions& options() const noexcept { return options_; }

    [[nodiscard]] std::size_t polygon_count() const noexcept;

    // 已加载区域的全部名字（按文件顺序）。set_value 的键必须与这里一致，
    // 否则那个区域会静默留在"无值灰底"——这是这个 getter 存在的唯一理由。
    [[nodiscard]] TERMCHART_API std::vector<std::string> feature_names() const;

    void draw(DrawContext ctx) override;

    // ── 高清出口 2：子像素栅格化（盲文/象限/半块）────────────────────────────
    // 输出 CellBuffer 的字符尺寸是 cols × rows；每格内部再按 style 切 2×4 等子像素。
    // 每个子像素记录"被哪个 feature 覆盖"，收口时每格取覆盖面积最大的 feature 取色——
    // 于是既拿到 8× 分辨率，又保住分级着色（这是单通道 FloatField 做不到的）。
    // 与 draw() 共用同一份投影，不重复实现投影逻辑。
    [[nodiscard]] TERMCHART_API CellBuffer rasterize(std::uint32_t cols, std::uint32_t rows,
                                                    const GeoRasterOptions& opts = {}) const;

    // ── 高清出口 3：真彩色位图 ────────────────────────────────────────────────
    // 任意像素尺寸，扫描线填充 + 区域描边 + 可选超采样抗锯齿。GUI / PNG / sixel 的数据源。
    [[nodiscard]] TERMCHART_API image::Bitmap render_bitmap(std::uint32_t width,
                                                           std::uint32_t height,
                                                           const GeoBitmapOptions& opts = {}) const;

private:
    struct Impl;
    void append_collection(util::geo::GeoFeatureCollection&& fc);
    std::unique_ptr<Impl> impl_;
    GeoMapOptions         options_;
    std::unordered_map<std::string, double> values_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/geo/hexbin.hpp =====
// hexbin.hpp —— 六边形分箱（示意地图级别）
//
// **精度立场**：偏移网格近似六边形铺排（奇数行右移半径一半），不是几何
// 精确的正六边形，也不做真实投影，**不做假精度**。输入点 (x, y) 落进最近
// 的六边形桶，桶内计数再按 bins 档分级着色（计数越多越深）。



namespace termchart::chart {

struct HexbinOptions {
    double radius = 2.0;   // 桶半径（格），收敛到 >= 1
    int bins = 5;          // 计数分级数，收敛到 [1, 8]
};

class Hexbin final : public Chart {
public:
    // 非有限坐标的点在 draw 时被跳过，不崩溃。
    void add_point(double x, double y) { points_.push_back(Point{x, y}); }
    void set_options(const HexbinOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t point_count() const noexcept { return points_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<Point> points_;
    HexbinOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/geo/tile_grid.hpp =====
// tile_grid.hpp —— 瓦片网格（示意地图级别）
//
// **精度立场**：每个区域一个瓦片（1 格），不做真实地理投影，**不做假
// 精度**——这是"一个区域一格"的示意图（类似 tile grid map），不适合
// 面积/形状读数。瓦片字符取区域名的首簇（空名用 █），颜色按值分级
// （灰阶 232..255，值越高越深）。



namespace termchart::chart {

struct GeoTile {
    std::string name;
    int col = 0;    // 瓦片在网格中的列（画布格）
    int row = 0;
    double value = 0.0;
};

struct TileGridOptions {
    int bins = 5;   // 分级数，收敛到 [1, 8]
};

class TileGrid final : public Chart {
public:
    void add_tile(GeoTile tile) { tiles_.push_back(std::move(tile)); }
    void set_options(const TileGridOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t tile_count() const noexcept { return tiles_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<GeoTile> tiles_;
    TileGridOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/dendrogram.hpp =====
// dendrogram.hpp —— 树状图（叶均布底线、父节点横杠连接）
//
// 字符化策略：叶子沿底线均布并画 ●，父节点 x = 子节点均值、row = 深度，
// 连接件用 ─（横杠）与 │（垂线）拼 U 形。布局自给（chart > graph 禁止边）。


namespace termchart::chart {

struct DendrogramOptions {
    bool show_leaves = true;  // 叶子画 ● 标记
};

class Dendrogram final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    DendrogramOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/icicle.hpp =====
// icicle.hpp —— 冰柱图（每层一行、宽度 ∝ value 的嵌套分段）
//
// 字符化策略：深度 → 行（自上而下一层一行），子节点按 value 比例分割父节点
// 的水平区间；每段填 █，段宽 ≥2 时段首放名称首字符。布局自给（chart > graph
// 禁止边）。


namespace termchart::chart {

struct IcicleOptions {
    bool show_labels = true;  // 段首放名称首字符
};

class Icicle final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    IcicleOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/pack.hpp =====
// pack.hpp —— 圆堆积（bubble pack）
//
// 字符化策略：半径 ∝ √value 的圆，行式贪心排布（放不下换行）；圆周用 ●
// 描边、内部填 ·，半径足够时圆心放名称首字符。布局自给（chart > graph 禁止边）。


namespace termchart::chart {

struct PackOptions {
    double gap = 1.0;  // 气泡之间的空隙（格）
};

class PackChart final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    PackOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/sunburst.hpp =====
// sunburst.hpp —— 旭日图（径向环 + 角向扇区）
//
// 字符化策略：极坐标逐格采样——修正终端 1:2 宽高比后，环号 = 深度、
// 角向按聚合 value 比例分扇区，命中的格填 █。布局自给（同 treemap 的理由，
// chart > graph 是禁止边）。


namespace termchart::chart {

struct SunburstOptions {
    bool show_root = true;  // 最内环画根；关闭时子节点从内环起
};

class Sunburst final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    SunburstOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/tree_view.hpp =====
// tree_view.hpp —— 树形视图（缩进树）
//
// 字符化策略：一行一节点，├─ / └─ / │ 缩进前缀 + 名称文本（可选后缀值），
// 纯文本绘制走 draw_text，超出画布由裁剪兜底。布局自给（chart > graph 禁止边）。


namespace termchart::chart {

struct TreeViewOptions {
    bool show_values = false;  // 名称后附聚合值
};

class TreeView final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    TreeViewOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/hierarchical/treemap.hpp =====
// treemap.hpp —— 树图（矩形切片）
//
// 字符化策略：slice-and-dice 递归切片，面积 ∝ 聚合 value（沿较长边切分，
// 让格子尽量偏方）；叶子格块填 █，块足够大时在左上角放名称首字符。
// 布局自给：chart 是 L12、graph 是 L13 且 `chart > graph` 为禁止边，
// 层次布局由本文件内 ~30 行递归完成，不 include graph/。


namespace termchart::chart {

struct TreemapOptions {
    bool show_labels = true;  // 叶子左上角放名称首字符
};

class Treemap final : public Chart {
public:
    void set_root(hier::HierNode root);

    void draw(DrawContext ctx) override;

    TreemapOptions options;

private:
    hier::HierNode root_;
    bool has_root_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/indicator/gauge_kpi.hpp =====
// gauge_kpi.hpp —— KPI 卡片：紧凑数字面板
//
// 卡片 = 边框 + 标签（上）+ 主值（中）+ 变化量（下，▲/▼）。
// 主值是**预格式化文本**——数字格式化策略（千分位、单位缩写）留给调用方，
// 库不替用户做决定。与 progress_ring 的分工：KPI 强调"读数"，不做弧形。



namespace termchart::chart {

struct GaugeKpiOptions {
    bool show_delta = true;
};

class GaugeKpi final : public Chart {
public:
    void set_label(std::string text) { label_ = std::move(text); }
    void set_value(std::string text) { value_ = std::move(text); }
    void set_delta(std::string text, bool up) {
        delta_ = std::move(text);
        delta_up_ = up;
    }
    void set_options(const GaugeKpiOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::string label_;
    std::string value_;
    std::string delta_;
    bool delta_up_ = true;
    GaugeKpiOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/indicator/progress_ring.hpp =====
// progress_ring.hpp —— 进度环：单值环形
//
// **与 text/progress 的区分**（规格注明，勿混）：本图是**单值环形**读数
// （圆弧从顶部顺时针扫过 fraction 圈），text/progress 是横向条形组。
// 环在字符格粒度上用椭圆采样近似，格粒度有台阶，**不做假精度**。


namespace termchart::chart {

struct ProgressRingOptions {
    bool show_percent = true;
    Color track_color = default_color(DefaultColor::Muted);   // 未完成轨道
};

class ProgressRing final : public Chart {
public:
    // fraction 收敛到 [0, 1]，非有限按 0。
    void set_fraction(double fraction) { fraction_ = fraction; }
    void set_color(Color color) { color_ = color; }
    void set_options(const ProgressRingOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    double fraction_ = 0.0;
    Color color_ = default_color(DefaultColor::Cyan);
    ProgressRingOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/indicator/star_rating.hpp =====
// star_rating.hpp —— 星级评分
//
// 满星 ★ / 半星 ◐（评分四舍五入到半星）/ 空星 ☆，一行排开。
// 评分收敛到 [0, max_stars]，非有限按 0。


namespace termchart::chart {

struct StarRatingOptions {
    int max_stars = 5;                      // 收敛到 [1, 20]
    Color star_color = default_color(DefaultColor::Amber);    // 金黄
    bool dim_empty = true;                  // 空星是否用暗色
};

class StarRating final : public Chart {
public:
    void set_rating(double rating) { rating_ = rating; }
    void set_options(const StarRatingOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    double rating_ = 0.0;
    StarRatingOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/indicator/status_light.hpp =====
// status_light.hpp —— 状态灯
//
// 一个 ● 按状态着色（Ok 绿 / Warning 黄 / Error 红 / Unknown 灰），
// 可带标签。是全部 indicator 里最轻的读数件：一格即语义。



namespace termchart::chart {

enum class LightState : std::uint8_t { Ok, Warning, Error, Unknown };

struct StatusLightOptions {
    bool show_label = true;
};

class StatusLight final : public Chart {
public:
    void set_state(LightState state) { state_ = state; }
    void set_label(std::string text) { label_ = std::move(text); }
    void set_options(const StatusLightOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    LightState state_ = LightState::Unknown;
    std::string label_;
    StatusLightOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/indicator/thermometer.hpp =====
// thermometer.hpp —— 温度计
//
// 竖管 + 底部球泡 + 右侧刻度。液柱高度按 (value - vmin) / (vmax - vmin)
// 线性映射，液色随填充比例分档（青→绿→黄→红）。值收敛到域内，非有限
// 值只画空管不画液柱，不崩溃。


namespace termchart::chart {

struct ThermometerOptions {
    double vmin = 0.0;
    double vmax = 100.0;
    int ticks = 5;    // 刻度数，收敛到 [0, 10]
};

class Thermometer final : public Chart {
public:
    void set_value(double value) { value_ = value; }
    void set_options(const ThermometerOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    double value_ = 0.0;
    ThermometerOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/donut.hpp =====
// donut.hpp —— 环形图（带中心孔的饼图）
//
// 字符化策略：在饼图的格粒度解析填充上加一道**环带判定**——归一化半径
// rnorm ≤ 1（外缘）且 rnorm ≥ hole_ratio（内孔）的格才落笔；内孔刻意留空，
// 中心读数与背景因此可见。扇区仍用专属字符阶梯区分（█▓▒░■●▲◆○ 循环），
// 角度约定与 pie 一致（start_angle 度、clockwise 扫角）。



namespace termchart::chart {

struct DonutOptions {
    double start_angle = 90.0;   // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;       // 扫角方向
    double hole_ratio = 0.45;    // 内孔半径占比（归一化半径），钳到 [0, 0.9)
    bool show_legend = true;
};

class DonutChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const DonutOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    DonutOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/gauge.hpp =====
// gauge.hpp —— 仪表盘（弧形刻度带 + 指针，含刻度）
//
// 字符化策略：弧 = 沿数学角**按角度采样**落格（与 canvas::arc_points 同法，
// 步长约每周 4·(rx+ry) 格保证连续；格粒度下必有台阶，不做假精度）。弧体用
// '●'，主刻度用 '+' 压在弧体与内侧（rnorm 0.88）两处，指针是从圆心到
// rnorm 0.72 的 DDA 线段（'*'），圆心一粒 '●' 作轴帽。数值域 [value_min,
// value_max] 线性映射到 [start_angle, end_angle] 的顺时针扫角（默认 225°→
// -45°，即经典 270° 开口朝下的表盘）。


namespace termchart::chart {

struct GaugeOptions {
    double value_min = 0.0;
    double value_max = 100.0;
    double start_angle = 225.0;  // 起始方向（度，数学方向：0=东，90=北）
    double end_angle = -45.0;    // 结束方向；clockwise 时扫角 = start - end
    bool clockwise = true;
    int tick_count = 9;          // 主刻度数（含两端）
    bool show_value = true;      // 在轴帽下方标注当前值
};

class GaugeChart final : public Chart {
public:
    void set_value(double value) { value_ = value; }
    void set_color(Color color) { color_ = color; }
    void set_options(const GaugeOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    double value_ = 0.0;
    Color color_ = default_color(DefaultColor::Accent);
    GaugeOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/pie.hpp =====
// pie.hpp —— 饼图
//
// 字符化策略：圆盘 = 绘图区内切椭圆的**格粒度解析填充**——逐格算归一化半径
// （≤ 1 才落笔），与 graph/shape 的 Ellipse 同一哲学：曲线在格粒度下必然有台阶，
// 不做假精度。扇区不依赖颜色区分（单色终端仍可读）：每个扇区从阶梯表取**专属
// 填充字符**（█▓▒░■●▲◆○ 循环），扇区边界即字符边界。角度从 start_angle（度，
// 数学方向，90 = 正上）起按 clockwise 方向扫过，格的归属角 = 起始方向起扫过的角。



namespace termchart::chart {

struct PieOptions {
    double start_angle = 90.0;  // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;      // 扫角方向
    bool show_legend = true;
};

class PieChart final : public Chart {
public:
    // values 为各类别的量（非有限/非正值跳过），labels 与之一一对应（可缺省）。
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const PieOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    PieOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/polar_bar.hpp =====
// polar_bar.hpp —— 极坐标柱状图（圆形排布的径向柱）
//
// 字符化策略：柱 = 角域 × 半径域的格粒度解析填充——格落在第 i 个槽位
// （等角 2π/n 的中央 bar_width 比例）且 rnorm ≤ value_i/vmax 才写 '█'，
// 与 graph/shape 的椭圆同一哲学：格粒度下必有台阶，不做假精度。参考环
// （0.25/0.5/0.75/1.0 半径）与槽位射线用 '·' 沿角度采样画（DrawContext::
// draw_grid 开关控制）；没有直角坐标轴，DrawContext::draw_axes 被忽略。



namespace termchart::chart {

struct PolarBarOptions {
    double start_angle = 0.0;   // 第一个槽位的起始方向（度，0 = 东）
    bool clockwise = true;      // 槽位排布方向
    double bar_width = 0.8;     // 柱占槽位的角宽比例，钳到 (0, 1]
    bool show_legend = true;
};

class PolarBarChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> categories = {});
    void set_options(const PolarBarOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> categories_;
    PolarBarOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/polar_heat.hpp =====
// polar_heat.hpp —— 极坐标热力图（角向 × 径向分箱的密度上色）
//
// 字符化策略：把 (角度°, 半径) 点落入 angle_bins × radial_bins 的极坐标分箱，
// 填充仍是格粒度解析判定——格落在盘内（rnorm ≤ 1）即按其归属角/环查计数；
// 无数据的格留空（保留参考环），有数据的格用 4 级阴影字符（░▒▓█，计数越多
// 越实，最低档非空格以便与"无数据"区分）+ colormap 颜色。参考环/槽位射线
// 用 '·'（DrawContext::draw_grid 开关）；色标画在内框上方（沿用笛卡尔
// heatmap 的做法）。



namespace termchart::chart {

struct PolarHeatOptions {
    int angle_bins = 12;    // 角向分箱数，钳到 [1, 360]
    int radial_bins = 5;    // 径向分箱数，钳到 [1, 64]
    double rmax = 0.0;      // 半径满量程；≤ 0 时取数据最大值
    color::ColormapKind colormap = color::ColormapKind::Viridis;
    std::string name = "density";
    bool show_colorbar = true;
};

class PolarHeatChart final : public Chart {
public:
    // 每个点 p.x = 角度（度，图表角），p.y = 半径；非有限点跳过。
    void set_points(std::vector<Point> points) { points_ = std::move(points); }
    void set_options(const PolarHeatOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<Point> points_;
    PolarHeatOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/polar_line.hpp =====
// polar_line.hpp —— 极坐标折线（角度 × 半径的数据序列连成线）
//
// 字符化策略：数据点 (角度°, 半径) 先映射到格坐标（角度从 start_angle 起沿
// clockwise 方向扫、半径除以 rmax 归一化成 rnorm），相邻点之间走 DDA 线段
// 离散（canvas::line_points）写 '•'，数据点本身压 '●'；外圈参考环用 '·'
// 沿角度采样（DrawContext::draw_grid 开关）。格粒度下线必有台阶，不做假精度。



namespace termchart::chart {

struct PolarLineOptions {
    double start_angle = 0.0;   // 角度 0 的方向（度，0 = 东）
    bool clockwise = true;      // 角度增长方向
    bool close = false;         // 末点与首点是否相连（成环）
    double rmax = 0.0;          // 半径满量程；≤ 0 时取数据最大值
    bool show_markers = true;   // 数据点是否压 '●'
};

class PolarLineChart final : public Chart {
public:
    // 每个点 p.x = 角度（度，图表角），p.y = 半径；非有限点跳过。
    void set_points(std::vector<Point> points) { points_ = std::move(points); }
    void set_color(Color color) { color_ = color; }
    void set_options(const PolarLineOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Accent);
    PolarLineOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/radar.hpp =====
// radar.hpp —— 雷达图（多轴蛛网 + 数据多边形）
//
// 字符化策略：蛛网的射线与环全部走 DDA 线段离散（canvas::line_points，与
// graph/shape 六边形同法）——**环是缩小的多边形**（连接各轴在 1/3、2/3、1
// 半径处的点），不是圆；数据多边形用 '•' 描边、'●' 标顶点，蛛网用 '·'，
// 三种字符互不相同，单色终端也能分层读。轴标签画在外顶点外推 1.15 半径处
// （越界由 canvas 裁剪）。DrawContext::draw_grid 关掉时不画蛛网。



namespace termchart::chart {

struct RadarOptions {
    double start_angle = 90.0;  // 第一根轴的方向（度，90 = 正上）
    bool clockwise = true;      // 轴的排布方向
    bool show_legend = true;
};

class RadarChart final : public Chart {
public:
    // 每根轴一个值（非有限值按 0 处理）与一个可选标签；少于 3 根轴不成多边形。
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_series_name(std::string name) { name_ = std::move(name); }
    void set_color(Color color) { color_ = color; }
    void set_options(const RadarOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    std::string name_ = "series";
    Color color_ = default_color(DefaultColor::Accent);
    RadarOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/polar/rose.hpp =====
// rose.hpp —— 南丁格尔玫瑰（等角扇区，半径编码数值）
//
// 字符化策略：与饼图同款的格粒度解析填充，差别在半径——各扇区等角（2π/n），
// 扇区 i 的落笔判据是 rnorm ≤ value_i / vmax（半径 ∝ 数值，面积夸大是本图种
// 的表达意图）。扇区用专属字符阶梯（█▓▒░■●▲◆○ 循环）+ 调色板色区分；
// DrawContext::draw_grid 关掉时连外圈参考环也不画。



namespace termchart::chart {

struct RoseOptions {
    double start_angle = 90.0;  // 起始方向（度，数学方向：0=东，90=北）
    bool clockwise = true;      // 扫角方向
    bool show_legend = true;
};

class RoseChart final : public Chart {
public:
    void set_values(std::vector<double> values, std::vector<std::string> labels = {});
    void set_options(const RoseOptions& options) { options_ = options; }

    void draw(DrawContext ctx) override;

private:
    std::vector<double> values_;
    std::vector<std::string> labels_;
    RoseOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/registry.hpp =====
// registry.hpp —— 图表注册表（RAII 友好、构造期冻结）
//
// 与 RasterRegistry 同一套契约：instance() 是 function-local static，构造后只读。
// 注册的是工厂而非原型，避免 clone 语义；create() 每次给新实例。



namespace termchart::chart {

class ChartRegistry {
public:
    using Factory = std::function<std::unique_ptr<Chart>()>;

    static ChartRegistry& instance() noexcept;

    ChartRegistry(const ChartRegistry&) = delete;
    ChartRegistry& operator=(const ChartRegistry&) = delete;

    void register_chart(std::string_view name, Factory factory);
    [[nodiscard]] std::unique_ptr<Chart> create(std::string_view name) const;
    [[nodiscard]] bool supports(std::string_view name) const;

private:
    ChartRegistry() = default;
    std::map<std::string, Factory, std::less<>> factories_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/sparkline.hpp =====
// sparkline.hpp —— 迷你折线（无轴、无网格、无标签）
//
// 用途：表格单元格里的一格趋势、仪表盘上的小趋势线。整块画布就是数据区，
// 因此它**不是** Chart 的子类（没有标题/边距/图例这些概念），只接受一个画布。
//
// 定域口径：默认按数据 min/max 自适应；`set_zero_baseline(true)` 时把 0 纳入域，
// 用于"涨跌是否越过基准线"的读数场景。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::chart {

class Sparkline {
public:
    TERMCHART_API void set_values(std::vector<double> values);
    void set_color(Color color) { color_ = color; }
    void set_marker(bool on) { marker_ = on; }
    void set_zero_baseline(bool on) { zero_baseline_ = on; }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

    // 返回实际写入画布的格数（画布为空或数据不足 2 点时返回 0）。
    [[nodiscard]] TERMCHART_API std::size_t draw(canvas::Canvas& canvas) const;

private:
    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Cyan);
    bool marker_ = true;
    bool zero_baseline_ = false;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/autocorr.hpp =====
// autocorr.hpp —— 自相关函数（ACF）
//
// 字符化策略：每个滞后期一根棒棒糖——零线到 r_k 的竖线 '│'、顶端 '●'；
// ±1.96/√n 置信带画水平 '·' 参考线。统计口径：r_k = Σ(x_i-x̄)(x_{i-k}-x̄) /
// Σ(x_i-x̄)²，k = 1..max_lag（clamp 到 n-1），样本方差为 0（常数序列）或
// n < 2 时不绘制。



namespace termchart::chart {

struct AutocorrOptions {
    int max_lag = 16;       // 最大滞后期，内部 clamp 到 [1, n-1]
    bool show_band = true;  // ±1.96/√n 置信带
};

class AutocorrChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral));

    void set_options(AutocorrOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // r[k] 为滞后 k 的自相关（k = 0..实际最大滞后；r[0] = 1；未算成为空）。
    [[nodiscard]] const std::vector<double>& acf() const noexcept { return acf_; }
    // 置信带半宽 1.96/√n（未算成为 0）。
    [[nodiscard]] double conf_band() const noexcept { return conf_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Neutral);
    AutocorrOptions options_{};
    std::vector<double> acf_;
    double conf_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/bland_altman.hpp =====
// bland_altman.hpp —— 一致性图（Bland-Altman）
//
// 字符化策略：每对方法画一点 '●'，横轴为两法均值、纵轴为差值；mean 差画
// '─' 实线，mean ± 1.96·SD 一致性界限画 '·' 虚线。统计口径：对有限且成对
// 的样本计算 d_i = a_i - b_i、m_i = (a_i + b_i)/2，mean = avg(d)、
// SD = 样本标准差（除以 n-1）；有效对 < 2 不绘制。



namespace termchart::chart {

struct BlandAltmanOptions {
    Color color = default_color(DefaultColor::Neutral);
    bool show_limits = true;  // ±1.96·SD 界限
};

class BlandAltmanChart final : public Chart {
public:
    TERMCHART_API void set_pairs(std::vector<double> method_a,
                                 std::vector<double> method_b,
                                 Color color = default_color(DefaultColor::Neutral),
                                 std::string name = "diff");

    void set_options(BlandAltmanOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] double mean_diff() const noexcept { return mean_; }
    [[nodiscard]] double sd_diff() const noexcept { return sd_; }
    [[nodiscard]] double upper_limit() const noexcept { return mean_ + 1.96 * sd_; }
    [[nodiscard]] double lower_limit() const noexcept { return mean_ - 1.96 * sd_; }
    [[nodiscard]] const std::vector<Point>& pairs() const noexcept { return pairs_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    BlandAltmanOptions options_{};
    bool show_legend_ = true;
    std::string name_ = "diff";
    std::vector<Point> pairs_;  // (均值, 差值)
    bool valid_ = false;
    double mean_ = 0.0;
    double sd_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/corr.hpp =====
// corr.hpp —— 相关矩阵（Pearson r → 字符色阶）
//
// 字符化策略：|r| 量化为 5 档映射到 '·'/'░'/'▒'/'▓'/'█'（阈值 0.15/0.40/
// 0.65/0.85），符号用前景色区分——正相关青、负相关红、近零暗灰；统计口径：
// Pearson 积差相关 r = cov/(σa·σb)，任一方差为 0（常数列）或长度不符的
// 部分按 r = 0（无相关）处理。



namespace termchart::chart {

struct CorrMatrixOptions {
    double cell_w = 3.0;  // 每对变量横向占格（放不下时收缩，下限 1）
    double cell_h = 2.0;  // 纵向占格（放不下时收缩，下限 1）
};

class CorrMatrixChart final : public Chart {
public:
    // columns[j] 为第 j 个变量的观测序列；names 可为空，长度超过变量数时截断。
    TERMCHART_API void set_matrix(std::vector<std::string> names,
                                  std::vector<std::vector<double>> columns);

    void set_options(CorrMatrixOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // r[i][j] = 变量 i 与 j 的相关系数（对角线恒 1，退化格为 0）。
    [[nodiscard]] const std::vector<std::vector<double>>& r_matrix() const noexcept {
        return r_;
    }

private:
    // Pearson 积差相关；长度取两者较短者，零方差返回 0。
    static double pearson(const std::vector<double>& a,
                          const std::vector<double>& b) noexcept;

    std::vector<std::string> names_;
    std::vector<std::vector<double>> columns_;
    std::vector<std::vector<double>> r_;
    CorrMatrixOptions options_{};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/distribution.hpp =====
// distribution.hpp —— 分布曲线（高斯核密度估计 KDE）
//
// 字符化策略：KDE 曲线逐点落 '•'，曲线下方按列铺浅色 '▒'；统计口径：带宽
// h = 0.9·min(样本标准差, IQR/1.34)·n^(-1/5)（Silverman 经验法则，退化时取
// 单位带宽），曲线在 [min-3h, max+3h] 上等距评估 grid_points 个点，y 为
// 归一化密度。非有限样本剔除。



namespace termchart::chart {

struct DistributionOptions {
    int grid_points = 96;    // KDE 评估点数，内部 clamp 到 [16, 512]
    double bandwidth = 0.0;  // <=0：自动（Silverman）
    bool fill_under = true;  // 曲线下方铺 '▒'
};

class DistributionChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Accent),
                                  std::string name = "density");

    void set_options(DistributionOptions options) {
        options_ = options;
        evaluate();
    }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 最近一次评估的 KDE 曲线（x 升序；无有效样本时为空）。
    [[nodiscard]] const std::vector<double>& curve_x() const noexcept { return xs_; }
    [[nodiscard]] const std::vector<double>& curve_y() const noexcept { return ys_; }

private:
    // 由 set_values/set_options 触发的确定性重估。
    void evaluate();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Accent);
    std::string name_ = "density";
    DistributionOptions options_{};
    bool show_legend_ = true;
    std::vector<double> xs_;
    std::vector<double> ys_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/ecdf.hpp =====
// ecdf.hpp —— 经验分布函数（ECDF）
//
// 字符化策略：升序样本逐级"跳台阶"——水平段用 '─'、垂直抬升用 '│'、每个
// 样本在 (x_i, i/n) 处落 '●'；统计口径：F̂(x) = #{x_i ≤ x}/n，n 为有限样本
// 数，重复值合并为一次完整抬升。y 域固定 [0, 1]。



namespace termchart::chart {

struct EcdfOptions {
    bool show_risers = true;   // 台阶间的垂直抬升线
    bool show_markers = true;  // 每个样本处的 '●'
};

class EcdfChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral),
                                  std::string name = "ECDF");

    void set_options(EcdfOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 升序有限样本；无有效样本时为空。
    [[nodiscard]] const std::vector<double>& sorted_values() const noexcept { return sorted_; }

private:
    std::vector<double> values_;
    std::vector<double> sorted_;
    Color color_ = default_color(DefaultColor::Neutral);
    std::string name_ = "ECDF";
    EcdfOptions options_{};
    bool show_legend_ = true;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/forest.hpp =====
// forest.hpp —— 森林图（各研究点估计 + 置信区间横线）
//
// 字符化策略：每行一条研究——CI 用 '─' 横线、点估计落 '●'、合计行落 '◆'；
// x = 0 处画竖直 '·' 参考线（无效应基线）。统计口径：CI 直接按给定
// [lower, upper] 画，不做再计算；x 域自动并入 0，保证参考线在场。



namespace termchart::chart {

struct ForestStudy {
    std::string label;
    double estimate = 0.0;
    double lower = 0.0;  // CI 下界
    double upper = 0.0;  // CI 上界
};

struct ForestOptions {
    Color color = default_color(DefaultColor::Neutral);
    bool show_zero_line = true;
};

class ForestChart final : public Chart {
public:
    TERMCHART_API void set_studies(std::vector<ForestStudy> studies);
    // 可选合计行（常为菱形），画在最后；不设则不画。
    void set_total(ForestStudy total) { total_ = std::move(total); has_total_ = true; }

    void set_options(ForestOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<ForestStudy> studies_;
    ForestStudy total_{};
    bool has_total_ = false;
    ForestOptions options_{};
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/pca.hpp =====
// pca.hpp —— 主成分投影（协方差矩阵 + 幂迭代）
//
// 字符化策略：样本经 top-2 主成分投影后以 '●' 落格，轴即 PC1/PC2；统计
// 口径：列中心化后取协方差矩阵（除以 n），幂迭代求 PC1、对 C - λ1·v1·v1ᵀ
// 降幂求 PC2，解释方差比 = 特征值 / 总方差（迹）。投影退化（单点）时给
// 单位跨度；有效样本 <2 或维度 0 时不绘制。



namespace termchart::chart {

struct PcaOptions {
    int max_iter = 100;  // 幂迭代上限
    double tol = 1e-12;  // 收敛阈值（分量最大变化 / 归一化范数下限）
};

class PcaChart final : public Chart {
public:
    // rows：n 个样本，每个 d 维（以首行维度为准，长度不符或含非有限值的行剔除）。
    TERMCHART_API void set_data(std::vector<std::vector<double>> rows);

    void set_options(PcaOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }
    // 投影后的 2D 点（与有效样本一一对应；未拟合时为空）。
    [[nodiscard]] const std::vector<Point>& projected() const noexcept { return projected_; }
    // [PC1, PC2] 的解释方差比（未拟合时为空）。
    [[nodiscard]] const std::vector<double>& explained_ratio() const noexcept {
        return ratio_;
    }

private:
    void fit();

    std::vector<std::vector<double>> rows_;
    PcaOptions options_{};
    std::vector<Point> projected_;
    std::vector<double> ratio_;
    Color color_ = default_color(DefaultColor::Neutral);
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/regression.hpp =====
// regression.hpp —— 线性回归 + 残差
//
// 字符化策略：样本落 '●'、拟合线 '─'、残差竖线 '·'（点到拟合线的竖直偏差）。
// 统计口径：普通最小二乘 y = a + b·x，只对有限样本拟合，n ≥ 2 且 x 方差 > 0
// 才成立；R² = 1 - SSres/SStot。拟合失败时 slope/intercept/R² 为 0、
// fitted() 为 false，绘制退化为纯散点。



namespace termchart::chart {

struct RegressionOptions {
    bool show_residuals = true;  // 残差竖线
};

class RegressionChart final : public Chart {
public:
    TERMCHART_API void set_points(std::vector<Point> points,
                                  Color color = default_color(DefaultColor::Neutral),
                                  std::string name = "y");

    void set_options(RegressionOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    [[nodiscard]] bool fitted() const noexcept { return fitted_; }
    [[nodiscard]] double slope() const noexcept { return slope_; }
    [[nodiscard]] double intercept() const noexcept { return intercept_; }
    [[nodiscard]] double r_squared() const noexcept { return r_squared_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<Point> points_;
    Color color_ = default_color(DefaultColor::Neutral);
    std::string name_ = "y";
    RegressionOptions options_{};
    bool show_legend_ = true;
    bool fitted_ = false;
    double slope_ = 0.0;
    double intercept_ = 0.0;
    double r_squared_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/roc.hpp =====
// roc.hpp —— ROC/PR 曲线 + AUC
//
// 字符化策略：曲线逐点落 '•'，ROC 模式叠对角参考线 '·'（随机分类器基线）。
// 统计口径：按分数降序扫描阈值（同分合并为同一阈值），累积 (FPR, TPR) 得
// ROC、(recall, precision) 得 PR；AUC 用梯形积分。正类 = label > 0。正负
// 样本不同时缺（无法定义两指标）不绘制。



namespace termchart::chart {

enum class RocMode : std::uint8_t {
    Roc = 0,  // TPR 对 FPR
    Pr = 1,   // precision 对 recall
};

struct RocOptions {
    RocMode mode = RocMode::Roc;
    bool show_diagonal = true;  // 仅 ROC 模式生效
};

class RocChart final : public Chart {
public:
    // scores 与 labels 等长取较短者；label > 0 视为正类。非有限分数剔除。
    TERMCHART_API void set_scores(std::vector<double> scores,
                                  std::vector<int> labels,
                                  std::string name = "ROC");

    void set_options(RocOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // ROC/PR 曲线（数据坐标；正负类不全时为空）。
    [[nodiscard]] const std::vector<Point>& roc_curve() const noexcept { return roc_; }
    [[nodiscard]] const std::vector<Point>& pr_curve() const noexcept { return pr_; }
    [[nodiscard]] double auc_roc() const noexcept { return auc_roc_; }
    [[nodiscard]] double auc_pr() const noexcept { return auc_pr_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    std::vector<double> scores_;
    std::vector<int> labels_;
    std::string name_ = "ROC";
    RocOptions options_{};
    bool show_legend_ = true;
    std::vector<Point> roc_;
    std::vector<Point> pr_;
    double auc_roc_ = 0.0;
    double auc_pr_ = 0.0;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/seasonal.hpp =====
// seasonal.hpp —— 季节分解（趋势 + 季节 + 余项）
//
// 字符化策略：内区纵向三分带（trend/season/remainder），自上而下各画一条
// 曲线 '•'，带名写在左侧留白区。统计口径：period = m（≥2，不足两个周期不
// 绘制）；trend 为窗口 m 的居中移动平均（边缘不足半窗为 NaN）；season[j]
// = 按 i mod m 相位平均去趋势残差；remainder = x - trend - season[相位]。



namespace termchart::chart {

struct SeasonalOptions {
    int period = 12;  // 周期长度 m，内部 clamp 到 ≥2
};

class SeasonalChart final : public Chart {
public:
    TERMCHART_API void set_values(std::vector<double> values,
                                  Color color = default_color(DefaultColor::Neutral));

    void set_options(SeasonalOptions options) { options_ = options; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // 三个分量与输入等长（未定义处为 NaN；未分解时为空）。
    [[nodiscard]] const std::vector<double>& trend() const noexcept { return trend_; }
    [[nodiscard]] const std::vector<double>& seasonal() const noexcept { return seasonal_; }
    [[nodiscard]] const std::vector<double>& remainder() const noexcept { return remainder_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    void decompose();

    std::vector<double> values_;
    Color color_ = default_color(DefaultColor::Neutral);
    SeasonalOptions options_{};
    std::vector<double> trend_;
    std::vector<double> seasonal_;
    std::vector<double> remainder_;
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/statistical/survival.hpp =====
// survival.hpp —— 生存曲线（Kaplan-Meier 估计）
//
// 字符化策略：KM 阶梯自 1 递降——水平段 '─'、事件处垂直抬升（下降）'│'、
// 删失（censored）时刻落 '+' 刻度。统计口径：S(t) = ∏(1 - d_i/n_i)，d_i 为
// 事件数、n_i 为处于风险的个体数（time ≥ t_i），同刻事件合并一步；times 与
// events 按较短者取齐，非有限时间剔除。y 域固定 [0, 1]。



namespace termchart::chart {

struct SurvivalOptions {
    Color color = default_color(DefaultColor::Accent);
    bool show_censored = true;  // 删失 '+' 刻度
};

class SurvivalChart final : public Chart {
public:
    // times[i] 为观测时长，events[i] != 0 表示事件发生、0 表示删失。
    TERMCHART_API void set_data(std::vector<double> times, std::vector<int> events,
                                Color color = default_color(DefaultColor::Accent),
                                std::string name = "survival");

    void set_options(SurvivalOptions options) { options_ = options; }
    void set_show_legend(bool on) { show_legend_ = on; }

    TERMCHART_API void draw(DrawContext ctx) override;

    // KM 曲线（(时间, S) 数据坐标；无有效样本时为空）。
    [[nodiscard]] const std::vector<Point>& curve() const noexcept { return curve_; }
    [[nodiscard]] const layout::PlotArea& last_area() const noexcept { return area_; }

private:
    SurvivalOptions options_{};
    bool show_legend_ = true;
    std::string name_ = "survival";
    std::vector<Point> curve_;
    std::vector<double> censored_;  // 删失时刻（升序）
    mutable layout::PlotArea area_{1, 1, layout::Margin{}, 0, 1, 0, 1};
};

}  // namespace termchart::chart

// ===== include/termchart/chart/text/progress.hpp =====
// progress.hpp —— 进度条组（横向条形，多值并列）
//
// **与 indicator/progress_ring 的区分**（规格注明，勿混）：本图是**横向
// 条形组**（多个 0..1 值并列，行式读数），progress_ring 是**单值环形**
// 读数。二者服务不同场景，不互相替代。
//
// 每行：标签 + 液条（█ 已完成 / ░ 未完成）+ 百分比。fraction 收敛到
// [0,1]，非有限值按 0 处理，不崩溃。



namespace termchart::chart {

struct ProgressOptions {
    std::size_t bar_width = 0;    // 0 = 自动：内容区宽 - 标签宽 - 百分比列
    bool show_percent = true;
};

class Progress final : public Chart {
public:
    void add_bar(std::string label, double fraction, Color color = default_color(DefaultColor::Accent));
    void set_options(const ProgressOptions& options) { options_ = options; }

    [[nodiscard]] std::size_t bar_count() const noexcept { return bars_.size(); }

    void draw(DrawContext ctx) override;

private:
    struct Bar {
        std::string label;
        double fraction = 0.0;
        Color color;
    };
    std::vector<Bar> bars_;
    ProgressOptions options_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/text/table.hpp =====
// table.hpp —— 数据表格
//
// 列宽 = 表头与该列所有单元格的最大**显示宽度**（util/wcwidth.hpp，CJK 宽
// 2 列）。总宽超出画布时整列裁掉（从右往左丢弃，至少保一列），单元格文本
// 超列宽按显示宽度截断（绝不切在组合记号中间）。表头加粗，表头下画一条
// ├─┤ 风格的分隔线。



namespace termchart::chart {

class Table final : public Chart {
public:
    void add_column(std::string name);
    // 不足列数的行尾部留空，超出列数的单元格忽略。
    void add_row(std::vector<std::string> cells);

    [[nodiscard]] std::size_t column_count() const noexcept { return columns_.size(); }
    [[nodiscard]] std::size_t row_count() const noexcept { return rows_.size(); }

    void draw(DrawContext ctx) override;

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/text/tag_cloud.hpp =====
// tag_cloud.hpp —— 标签云：流式排版
//
// 与 wordcloud 的分工：wordcloud 用螺线做二维自由布局，本图是**一维流式**
// 排版（像文本换行：从左到右，放不下换行）。标签宽度按**显示宽度**计算
// （util/wcwidth.hpp：CJK 宽 2 列）；权重决定顺序（降序）与醒目程度
// （前若干词加粗、按排名分档配色）。



namespace termchart::chart {

struct TagCloudOptions {
    std::size_t gap = 2;        // 标签之间的空列
    std::size_t bold_top = 3;   // 权重前若干词加粗
};

class TagCloud final : public Chart {
public:
    // 空文本或权重非有限/<= 0 的标签在 draw 时被跳过，不崩溃。
    void add_tag(std::string text, double weight) {
        tags_.push_back(Tag{std::move(text), weight});
    }
    void set_options(const TagCloudOptions& options) { options_ = options; }

    // 最近一次 draw 实际画出的标签数（画布装不下时 < 标签数）。
    [[nodiscard]] std::size_t placed_count() const noexcept { return placed_; }

    void draw(DrawContext ctx) override;

private:
    struct Tag {
        std::string text;
        double weight = 0.0;
    };
    std::vector<Tag> tags_;
    TagCloudOptions options_;
    std::size_t placed_ = 0;
};

}  // namespace termchart::chart

// ===== include/termchart/chart/text/wordcloud.hpp =====
// wordcloud.hpp —— 词云：螺旋布局
//
// 布局策略：词按权重降序，从画布中心沿阿基米德螺线找第一个无碰撞的落点。
// 碰撞检测按**显示宽度**逐格判定（util/wcwidth.hpp：中日韩全宽字符占 2 列），
// 因此中文词不会互相覆盖；螺线纵坐标除以 2 补偿字符格约 1:2 的宽高比。
// 布局是字符格粒度的（坐标按格吸附），不做子像素，不做假精度。
// "字号"用属性位与颜色表达：权重最高的若干词加粗、按排名分档配色。



namespace termchart::chart {

struct WordcloudOptions {
    std::size_t max_words = 64;   // 最多画的词数
    double spiral_scale = 1.0;    // 螺旋扩张速度（格/弧度），过小则词挤在中心
    std::size_t bold_top = 3;     // 权重前若干词加粗
};

class Wordcloud final : public Chart {
public:
    // 空文本或权重非有限/<= 0 的词在 draw 时被跳过，不崩溃。
    void add_word(std::string text, double weight) {
        words_.push_back(Word{std::move(text), weight});
    }
    void set_options(const WordcloudOptions& options) { options_ = options; }

    // 最近一次 draw 实际放置的词数（画布太小时 < 词数）。
    [[nodiscard]] std::size_t placed_count() const noexcept { return placed_; }

    void draw(DrawContext ctx) override;

private:
    struct Word {
        std::string text;
        double weight = 0.0;
    };
    std::vector<Word> words_;
    WordcloudOptions options_;
    std::size_t placed_ = 0;
};

}  // namespace termchart::chart

// ===== include/termchart/chart3d/common.hpp =====
// common.hpp —— chart3d 模块共享的 3D 着色/取景辅助（header-only）
//
// scatter3d 与 surface3d 都用得到：深度→颜色着色（shade）与按包围盒自动取景
// （make_camera）。抽到独立头，保证 scatter.hpp / surface.hpp 都能独立 include。



namespace termchart::chart3d {

// 深度 t（0=近，1=远）→ 颜色：近处暖亮、远处冷暗。
inline Color shade(double t) noexcept {
    const double k = util::clamp<double>(t, 0.0, 1.0);
    const auto lerp_u8 = [](int a, int b, double kk) {
        return static_cast<std::uint8_t>(a + (b - a) * kk);
    };
    return Color::rgb(lerp_u8(255, 40, k), lerp_u8(210, 70, k), lerp_u8(80, 150, k));
}

// 自动取景：按点云/曲面包围盒居中、按半径定距离与近/远裁剪面。
inline math3d::Camera make_camera(double cols, double rows, double azimuth_deg,
                                  double elevation_deg, double fovy_deg,
                                  const math3d::Vec3& center, double radius) noexcept {
    math3d::Camera cam;
    cam.aspect = cols / rows;
    cam.fovy_deg = fovy_deg;
    const double fovy = fovy_deg * math3d::kDeg2Rad;
    const double dist = radius / std::tan(fovy / 2.0) * 1.8;
    cam.near = std::max(0.01, dist - radius * 1.5);
    cam.far = dist + radius * 1.5;
    cam.target = center;
    cam.orbit(azimuth_deg, elevation_deg, dist);
    return cam;
}

}  // namespace termchart::chart3d

// ===== include/termchart/chart3d/projector.hpp =====
// projector.hpp —— chart3d 的投影缝：3D 世界坐标 → 画布坐标的多态接口
//
// 为什么抽接口：scatter3d / surface3d 原先直接持有 math3d::Camera（Mat4
// 视图×投影），把"怎么看"焊死在"画什么"里。抽 IProjector 之后：
//   * Projector3D：现有 Camera 的薄封装（默认实现，行为逐字节不变）；
//   * 未来 Projector4D：4D→3D 投影产出 Vec3 序列，再经本接口完成
//     3D→2D——两阶段各自可替换，scatter/surface 与 chart4d 都不用改；
//   * 其他投影算法（正交 / 弱透视 / 自定义相机）同样只实现本接口即可注入。
//
// 注入方式：scatter3d / surface3d 的可选尾参数 `projector`，nullptr =
// 按 opts 的 azimuth/elevation/fovy 现算 Projector3D（既有调用方零改动，
// opts 里的视角参数在注入投影器时被忽略——视图归投影器所有）。


namespace termchart::chart3d {

class TERMCHART_API IProjector {
public:
    // 类内只声明、src/chart3d/projector.cpp 内定义（key function 惯用法）：
    // vtable/typeinfo 只在这一个 TU 发强符号。
    virtual ~IProjector();

    // 世界坐标 → 画布坐标。vw/vh 为画布尺寸（格，左上原点、y 向下）。
    // 返回值语义与 math3d::Camera::project 一致：depth 归一化（0=近、
    // 1=远，供着色），visible 表示在视锥内且位于相机前方。
    [[nodiscard]] virtual math3d::Projected project(const math3d::Vec3& world, double vw,
                                                    double vh) const noexcept = 0;
};

// 默认实现：把 math3d::Camera（轨道相机 + 透视投影 + 视锥裁剪）包一层。
// 按值持有 Camera（拷贝便宜：三个 Vec3 + 5 个 double），投影原样转发。
class TERMCHART_API Projector3D final : public IProjector {
public:
    explicit Projector3D(math3d::Camera camera) noexcept;

    [[nodiscard]] math3d::Projected project(const math3d::Vec3& world, double vw,
                                            double vh) const noexcept override;

private:
    math3d::Camera camera_;
};

}  // namespace termchart::chart3d

// ===== include/termchart/chart3d/scatter.hpp =====
// scatter.hpp —— 3D 散点图：把三维点云投影成 Scene（header-only）
//
// 不做终端专属渲染：产出的是与 2D 图表同构的 scene::Scene，交给已有的
// render_scene → Canvas → Renderer 管线，复用 braille/halfblock 光栅化。深度只用于
// 着色（近亮远暗），立体感由此而来，无需自造 z-buffer。



namespace termchart::chart3d {

struct Scatter3dOpts {
    double cols = 72.0;        // 画布宽（格）
    double rows = 28.0;        // 画布高（格）
    double azimuth_deg = 35.0;   // 轨道方位角（projector=nullptr 时生效）
    double elevation_deg = 22.0; // 轨道仰角（projector=nullptr 时生效）
    double fovy_deg = 55.0;       // 垂直视场角（projector=nullptr 时生效）
    double point_size = 0.9;      // 每个点在画布上的边长（格）
    bool axes = true;             // 是否画 X/Y/Z 轴
};

inline void bounding(const std::vector<math3d::Vec3>& pts, math3d::Vec3& minv,
                     math3d::Vec3& maxv, math3d::Vec3& center, double& radius) noexcept {
    if (pts.empty()) {
        return;
    }
    minv = maxv = pts.front();
    for (const auto& p : pts) {
        minv.x = std::min(minv.x, p.x);
        minv.y = std::min(minv.y, p.y);
        minv.z = std::min(minv.z, p.z);
        maxv.x = std::max(maxv.x, p.x);
        maxv.y = std::max(maxv.y, p.y);
        maxv.z = std::max(maxv.z, p.z);
    }
    center = (minv + maxv) * 0.5;
    radius = (maxv - minv).length() * 0.5;
    if (!(radius > 0.0)) {
        radius = 1.0;
    }
}

// projector 为 nullptr 时按 opts 的 azimuth/elevation/fovy 现算 Projector3D
// （既有行为，逐字节不变）；非空则全权委托注入的投影器（opts 里的三个视角
// 参数被忽略——视图归投影器所有），为 Projector4D 等留出注入缝。
inline scene::Scene scatter3d(const std::vector<math3d::Vec3>& pts, const Scatter3dOpts& o = {},
                              const IProjector* projector = nullptr) {
    scene::Scene s;
    if (pts.empty()) {
        return s;
    }

    math3d::Vec3 minv, maxv, center;
    double radius = 0.0;
    bounding(pts, minv, maxv, center, radius);

    const Projector3D default_projector{
        make_camera(o.cols, o.rows, o.azimuth_deg, o.elevation_deg, o.fovy_deg, center, radius)};
    const IProjector& proj = (projector != nullptr) ? *projector : default_projector;

    // 坐标轴：从包围盒最小角沿三轴方向各画一条，颜色固定 X红/Y绿/Z蓝。
    if (o.axes) {
        const math3d::Vec3 axes[3] = {math3d::Vec3{maxv.x, minv.y, minv.z},
                                      math3d::Vec3{minv.x, maxv.y, minv.z},
                                      math3d::Vec3{minv.x, minv.y, maxv.z}};
        const char* labels[3] = {"X", "Y", "Z"};
        const Color axis_color[3] = {Color::rgb(230, 80, 80), Color::rgb(90, 200, 110),
                                     Color::rgb(90, 140, 230)};
        for (int a = 0; a < 3; ++a) {
            const auto p0 = proj.project(minv, o.cols, o.rows);
            const auto p1 = proj.project(axes[a], o.cols, o.rows);
            if (!p0.visible || !p1.visible) {
                continue;
            }
            s.push(scene::StrokeOp{axis_color[a], 1.0});
            s.push(scene::MoveTo{Point{p0.x, p0.y}});
            s.push(scene::LineTo{Point{p1.x, p1.y}});
            s.push(scene::TextOp{Point{p1.x, p1.y}, std::string(labels[a]), Style{}});
        }
    }

    // 点：每个点一个填充小方块，颜色随深度变化。
    const double h = o.point_size / 2.0;
    for (const auto& p : pts) {
        const auto pr = proj.project(p, o.cols, o.rows);
        if (!pr.visible) {
            continue;
        }
        s.push(scene::FillOp{shade(pr.depth)});
        s.push(scene::RectOp{Rect{pr.x - h, pr.y - h, o.point_size, o.point_size}});
    }
    return s;
}

}  // namespace termchart::chart3d

// ===== include/termchart/chart3d/surface.hpp =====
// surface.hpp —— 3D 曲面/网格图：把 z=f(x,y) 投影成 Scene 线框（header-only）
//
// 与 scatter 同构：只产出 scene::Scene，复用现有终端光栅管线。曲面以**线框**呈现
// （每行/每列相邻的网格点连线），连线颜色按平均深度着色；这也是 3D 曲面最易辨识、
// 最稳的画法，避免在终端上赌多边形填充的清晰度。



namespace termchart::chart3d {

struct Surface3dOpts {
    double cols = 72.0;
    double rows = 28.0;
    double azimuth_deg = 35.0;   // projector=nullptr 时生效
    double elevation_deg = 25.0;  // projector=nullptr 时生效
    double fovy_deg = 55.0;       // projector=nullptr 时生效
    int res_x = 24;  // 网格列数
    int res_y = 24;  // 网格行数
    bool axes = true;
};

// projector 为 nullptr 时按 opts 现算 Projector3D（既有行为，逐字节不变）；
// 非空则全权委托注入的投影器（opts 里的视角参数被忽略），为 Projector4D
// 等留出注入缝。
inline scene::Scene surface3d(const std::function<double(double, double)>& f, double x0, double x1,
                              double y0, double y1, const Surface3dOpts& o = {},
                              const IProjector* projector = nullptr) {
    scene::Scene s;
    const std::size_t nx = static_cast<std::size_t>(std::max(1, o.res_x));
    const std::size_t ny = static_cast<std::size_t>(std::max(1, o.res_y));

    // 采样网格，记录包围盒以自动取景。
    std::vector<std::vector<math3d::Vec3>> grid(ny + 1, std::vector<math3d::Vec3>(nx + 1));
    math3d::Vec3 minv{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::infinity()};
    math3d::Vec3 maxv{-std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity()};
    for (std::size_t j = 0; j <= ny; ++j) {
        const double yy = util::lerp(y0, y1, static_cast<double>(j) / static_cast<double>(ny));
        for (std::size_t i = 0; i <= nx; ++i) {
            const double xx = util::lerp(x0, x1, static_cast<double>(i) / static_cast<double>(nx));
            const double zz = f(xx, yy);
            const math3d::Vec3 p{xx, yy, zz};
            grid[j][i] = p;
            minv.x = std::min(minv.x, p.x);
            minv.y = std::min(minv.y, p.y);
            minv.z = std::min(minv.z, p.z);
            maxv.x = std::max(maxv.x, p.x);
            maxv.y = std::max(maxv.y, p.y);
            maxv.z = std::max(maxv.z, p.z);
        }
    }

    const math3d::Vec3 center = (minv + maxv) * 0.5;
    double radius = (maxv - minv).length() * 0.5;
    if (!(radius > 0.0)) {
        radius = 1.0;
    }

    const Projector3D default_projector{
        make_camera(o.cols, o.rows, o.azimuth_deg, o.elevation_deg, o.fovy_deg, center, radius)};
    const IProjector& proj = (projector != nullptr) ? *projector : default_projector;

    if (o.axes) {
        const math3d::Vec3 axes[3] = {math3d::Vec3{maxv.x, minv.y, minv.z},
                                      math3d::Vec3{minv.x, maxv.y, minv.z},
                                      math3d::Vec3{minv.x, minv.y, maxv.z}};
        const char* labels[3] = {"X", "Y", "Z"};
        const Color axis_color[3] = {Color::rgb(230, 80, 80), Color::rgb(90, 200, 110),
                                     Color::rgb(90, 140, 230)};
        for (int a = 0; a < 3; ++a) {
            const auto p0 = proj.project(minv, o.cols, o.rows);
            const auto p1 = proj.project(axes[a], o.cols, o.rows);
            if (!p0.visible || !p1.visible) {
                continue;
            }
            s.push(scene::StrokeOp{axis_color[a], 1.0});
            s.push(scene::MoveTo{Point{p0.x, p0.y}});
            s.push(scene::LineTo{Point{p1.x, p1.y}});
            s.push(scene::TextOp{Point{p1.x, p1.y}, std::string(labels[a]), Style{}});
        }
    }

    // 线框：逐行、逐列相邻点连线，颜色按平均深度着色。
    const auto seg = [&](const math3d::Vec3& a, const math3d::Vec3& b) {
        const auto pa = proj.project(a, o.cols, o.rows);
        const auto pb = proj.project(b, o.cols, o.rows);
        if (!pa.visible || !pb.visible) {
            return;
        }
        s.push(scene::StrokeOp{shade((pa.depth + pb.depth) * 0.5), 1.0});
        s.push(scene::MoveTo{Point{pa.x, pa.y}});
        s.push(scene::LineTo{Point{pb.x, pb.y}});
    };
    for (std::size_t j = 0; j <= ny; ++j) {
        for (std::size_t i = 0; i < nx; ++i) {
            seg(grid[j][i], grid[j][i + 1]);
        }
    }
    for (std::size_t i = 0; i <= nx; ++i) {
        for (std::size_t j = 0; j < ny; ++j) {
            seg(grid[j][i], grid[j + 1][i]);
        }
    }
    return s;
}

}  // namespace termchart::chart3d

// ===== include/termchart/chart4d/projector4d.hpp =====
// projector4d.hpp —— 4D 投影器：实现 chart3d::IProjector，串起两阶段 4D→2D
//
// 数据流（用户口径："Vec4 -> Projector4D -> Vec3 -> Projector3D -> Vec2"）：
//   阶段一（math4d，本类持有）：Vec4 经 4D 旋转（6 平面组合）→ 透视除以 W → Vec3；
//   阶段二（内层 IProjector，通常是 chart3d::Projector3D）：Vec3 → 画布 Projected。
// 两个阶段各自可替换：换旋转/换 4D 相机只动阶段一；换 3D 视图（正交、弱透视、
// 自定义轨道相机）只换内层 IProjector——这正是当初把 chart3d 抽成 IProjector 的目的。
//
// 为什么继承 IProjector 却又多一个 Vec4 入口：IProjector::project 的入参是
// Vec3（3D 缝），4D 点无法穿过它。故本类：
//   * 新增 project(const Vec4&, ...) —— 真正的两阶段 4D 入口（线框/超立方体走这条）；
//   * 覆写 project(const Vec3&, ...) —— 满足 IProjector：喂 3D 点时等价于内层
//     投影器（4D 旋转/透视不参与），使本类也能直接插进 scatter3d/surface3d。



namespace termchart::chart4d {

class TERMCHART_API Projector4D final : public chart3d::IProjector {
public:
    Projector4D(math4d::Mat5 rotation, math4d::Camera4 camera,
                std::shared_ptr<chart3d::IProjector> inner) noexcept;

    // 4D 入口——真正的两阶段流程：
    //   Vec4 --(4D 旋转 rotation_)--> Vec4 --(透视除 W camera_)--> Vec3
    //        --(内层 inner_->project)--> 画布 Projected。
    // 内层为空或阶段一判定不可见（相机后方/近裁剪）→ 返回 visible=false。
    [[nodiscard]] math3d::Projected project(const math4d::Vec4& world, double vw,
                                            double vh) const noexcept;

    // 实现 IProjector：直接喂 3D 点时等价于内层投影器（4D 阶段不参与）。
    [[nodiscard]] math3d::Projected project(const math3d::Vec3& world, double vw,
                                            double vh) const noexcept override;

private:
    math4d::Mat5 rotation_;
    math4d::Camera4 camera_;
    std::shared_ptr<chart3d::IProjector> inner_;
};

}  // namespace termchart::chart4d

// ===== include/termchart/chart4d/wire.hpp =====
// wire.hpp —— 4D 线框/点云 → scene::Scene（header-only，chart3d 的 4D 对应物）
//
// 与 chart3d/scatter.hpp 同构：发射器只管几何与图元，视图全权委托 Projector4D
// （两阶段 4D→2D）。每条边的两端经投影器落到画布，两端均可见才画；顶点可选画
// 成按深度着色的小方块。边按两端平均深度着色，给旋转中的 4D 形体以深度提示。



namespace termchart::chart4d {

struct Wire4dOpts {
    double cols = 72.0;       // 画布宽（格）
    double rows = 28.0;       // 画布高（格）
    double point_size = 0.9;  // 顶点方块边长（格）
    bool draw_vertices = true;  // 是否在边之外叠加顶点标记
};

// 一条边 = 顶点索引对（超立方体等线框模型的边表即为此型的 vector）。
using Edge = std::pair<std::uint32_t, std::uint32_t>;

// 把 4D 顶点集 + 边表发射成 Scene。越界索引被跳过（不抛异常）；两端投影后
// 任一不可见（4D 相机后方/近裁剪，或 3D 相机视锥外）的边被整段丢弃。
inline scene::Scene wire4d(const std::vector<math4d::Vec4>& verts,
                           const std::vector<Edge>& edges, const Wire4dOpts& o,
                           const Projector4D& projector) {
    scene::Scene s;
    const double h = o.point_size / 2.0;

    for (const auto& e : edges) {
        if (e.first >= verts.size() || e.second >= verts.size()) {
            continue;
        }
        const auto p0 = projector.project(verts[e.first], o.cols, o.rows);
        const auto p1 = projector.project(verts[e.second], o.cols, o.rows);
        if (!p0.visible || !p1.visible) {
            continue;
        }
        s.push(scene::StrokeOp{chart3d::shade((p0.depth + p1.depth) * 0.5), 1.0});
        s.push(scene::MoveTo{Point{p0.x, p0.y}});
        s.push(scene::LineTo{Point{p1.x, p1.y}});
    }

    if (o.draw_vertices) {
        for (const auto& v : verts) {
            const auto p = projector.project(v, o.cols, o.rows);
            if (!p.visible) {
                continue;
            }
            s.push(scene::FillOp{chart3d::shade(p.depth)});
            s.push(scene::RectOp{Rect{p.x - h, p.y - h, o.point_size, o.point_size}});
        }
    }
    return s;
}

}  // namespace termchart::chart4d

// ===== include/termchart/graph/attributes.hpp =====
// attributes.hpp —— 节点/边的键值元数据（header-only 值类型）
//
// 图元数据是开放的（label、color、shape、rank、weight 等由各层约定），因此
// 这里用字符串键值袋而不是固定字段：新增元数据不需要改图的数据结构。
// 用透明比较器，查找可直接接受 std::string_view，避免临时构造 std::string。


namespace termchart::graph {

class Attributes {
public:
    void set(std::string key, std::string value) { values_[std::move(key)] = std::move(value); }

    [[nodiscard]] bool has(std::string_view key) const {
        return values_.find(key) != values_.end();
    }

    // 缺失时返回 fallback（默认空串），不抛异常。
    [[nodiscard]] std::string_view get(std::string_view key,
                                       std::string_view fallback = {}) const {
        const auto it = values_.find(key);
        return it == values_.end() ? fallback : std::string_view(it->second);
    }

    bool erase(std::string_view key) { return values_.erase(std::string(key)) != 0; }
    void clear() { values_.clear(); }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

private:
    std::map<std::string, std::string, std::less<>> values_;
};

}  // namespace termchart::graph

// ===== include/termchart/graph/crossing.hpp =====
// crossing.hpp —— 边交叉计数（布局质量度量）



namespace termchart::graph {

// 两条线段是否真正交叉（真交叉）：共端点、共线、仅相切都不算。
[[nodiscard]] TERMCHART_API bool segments_cross(Point a1, Point a2, Point b1,
                                                Point b2) noexcept;

// 一组线段的交叉对数（两两比较，O(n^2)）。用于评估布局/路由质量。
[[nodiscard]] TERMCHART_API std::size_t count_crossings(
    const std::vector<std::pair<Point, Point>>& segments) noexcept;

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/adjacency_matrix.hpp =====
// adjacency_matrix.hpp —— 邻接矩阵（行列标尺 + 值分级的块字符）
//
// 字符化策略：首行放列名标尺、每行行首放行名（均按显示宽度截断对齐），格子
// 按值相对最大值的比例分级为 █（强）▓（中）░（弱），零值画 ·——格粒度下
// "深浅"只有离散三档，不做假精度。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct AdjacencyMatrixOptions {
    int min_cell_width = 3;  // 每列最小宽（列名按此对齐，超宽截断）
    char32_t zero_char = U'\u00B7';  // 零值字符（·）
    Style header_style{};
    Style label_style{};
    Style cell_style{};
};

// 就地绘制；返回整表包围盒。names 为空返回零矩形。
// matrix 行列不足 n 的部分按零值处理，多余的行列忽略。
[[nodiscard]] TERMCHART_API Rect render_adjacency_matrix(
    const std::vector<std::string>& names,
    const std::vector<std::vector<double>>& matrix, canvas::Canvas& canvas,
    const AdjacencyMatrixOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/arc_diagram.hpp =====
// arc_diagram.hpp —— 弧线图（节点一排 + 跨度定高的弧）
//
// 字符化策略：节点按登记顺序沿底行一字排开（标签居中于各自的列），连接画成
// 底行上方的二次贝塞尔弧——跨度（两端列距）越大弧顶越高，高度封顶防出界；
// 弧体逐格按走向取 ─ │ ╱ ╲。同一节点的自连接画成小拱。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct ArcNode {
    std::string label;
};

// 一条弧：from/to 是 nodes 的下标；跨度越大弧越高。
struct ArcLink {
    std::size_t from = 0;
    std::size_t to = 0;
};

struct ArcDiagramOptions {
    double h_gap = 3.0;   // 相邻节点之间的空隙（格）
    int max_height = 6;   // 弧顶最大高度（格，自节点行向上计）
    Style label_style{};
    Style arc_style{};
};

// 就地绘制；返回标签与弧的包围盒（节点为空返回零矩形）。下标越界的弧跳过。
[[nodiscard]] TERMCHART_API Rect render_arc_diagram(const std::vector<ArcNode>& nodes,
                                                    const std::vector<ArcLink>& links,
                                                    canvas::Canvas& canvas,
                                                    const ArcDiagramOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/chord.hpp =====
// chord.hpp —— 和弦图（实体环形排布 + 弧带连接，值定带宽）
//
// 字符化策略：实体沿椭圆环等分排布（横向半径按字符格 1:2 宽高比放大为纵向的
// 两倍，环本身复用 graph/shape 的 Ellipse 轮廓——格粒度台阶不做假精度）；弦
// 画成弯向圆心的二次贝塞尔弧，逐格按走向取 ─ │ ╱ ╲；连接值越大带越宽，做法
// 是把锚点沿环的切向展开成若干条平行弧。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct ChordEntity {
    std::string label;
};

// 一条弦：from/to 是 entities 的下标；value 决定带宽（<=0 的弦跳过不画，
// from == to 的自弦在环上没有意义，同样跳过）。
struct ChordLink {
    std::size_t from = 0;
    std::size_t to = 0;
    double value = 1.0;
};

struct ChordOptions {
    double radius = 8.0;   // 环的纵向半径（格）；横向半径 = 2 * radius 补偿宽高比
    int max_band = 3;      // 最大带宽（平行弧条数，>=1）
    Style ring_style{};
    Style band_style{};
    Style label_style{};
    bool draw_ring = true;
};

// 就地绘制；返回环与标签的包围盒（实体为空返回零矩形）。
// 越界部分由 canvas 裁剪；下标越界的弦跳过。
[[nodiscard]] TERMCHART_API Rect render_chord(const std::vector<ChordEntity>& entities,
                                              const std::vector<ChordLink>& links,
                                              canvas::Canvas& canvas,
                                              const ChordOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/sankey.hpp =====
// sankey.hpp —— 桑基图（节点 + 流，按 value 定横条高度、按阶段分层）
//
// 字符化策略：节点画 █ 实心竖条（高 ∝ 流量合计），流画 ░ 丝带（源条右缘到
// 目标条左缘，上下沿线性过渡）；阶段按流的可达深度分列，可按内置调色板
// 着色（节点取调色板色，丝带继承源节点色）。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct SankeyNode {
    std::string name;
};

struct SankeyFlow {
    std::size_t source = 0;
    std::size_t target = 0;
    double value = 0.0;
};

struct SankeyOptions {
    double margin = 1.0;
    double bar_width = 2.0;  // 节点横条宽（格）
    double h_gap = 5.0;      // 列间距（丝带宽度下限）
    double v_gap = 1.0;      // 同列相邻条间隙
    Style node_style{};
    Style flow_style{};
    bool colorize = true;  // 节点按内置 6 色调色板着色，丝带继承源节点色
};

// 就地布局并绘制桑基图，返回整图包围盒。端点越界或 value <= 0 的流被跳过。
TERMCHART_API Rect render_sankey(const std::vector<SankeyNode>& nodes,
                                 const std::vector<SankeyFlow>& flows,
                                 canvas::Canvas& canvas,
                                 const SankeyOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/syntax_tree.hpp =====
// syntax_tree.hpp —— 语法树（(label, children) 递归节点 + tree 风格连接线）
//
// 字符化策略：根在左、深度沿列向右增长，逐层用 ├ └ ─ 画连接、祖先未完的层画
// │ 延续——即终端 tree 命令的形态；一行一个节点，超宽标签由画布裁剪。遍历用
// 显式栈（深树不会爆调用栈）。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct SyntaxNode {
    std::string label;
    std::vector<SyntaxNode> children;
};

struct SyntaxTreeOptions {
    int indent = 4;  // 每层缩进格数（<3 时按 3 处理，否则放不下 ├─ 与间隔）
    Style label_style{};
    Style line_style{};
};

// 就地绘制；返回整棵树的包围盒（空标签的根返回零矩形）。
[[nodiscard]] TERMCHART_API Rect render_syntax_tree(const SyntaxNode& root,
                                                    canvas::Canvas& canvas,
                                                    const SyntaxTreeOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/uml_sequence.hpp =====
// uml_sequence.hpp —— UML 时序图（参与者头框 + 生命线 + 消息箭头 + 便签 + 激活条）
//
// 字符化策略：参与者画成顶部一排**头框**（矩形 + 居中名），框中心列向下引
// `│` 生命线；消息画在两条生命线之间的水平线上——实线 `─` / 虚线 `┄`，箭头按
// 语义选形：实心 ▶/◀、开口 ▷/◁、叉 ×（丢失消息）；Note 按 left/right/over
// 落在生命线旁或跨在两条生命线之间，整段用 [方括号] 包住与消息文本区分；
// 激活条画成生命线上的 `█` 段。
//
// 本层自持模型（graph(13) 不得 include dsl(17)，故不复用 AstSequence；
// dsl/mermaid_sequence 的 AstSequence 可机械映射到本模型，由上层适配：
//   SequenceParticipant/SequenceMessage/SequenceNote 的字段一一对应）。
// 参与者按 key（字符串）引用；渲染时未知 key 的消息跳过、便签退化为 over 单人。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class SeqArrowKind : std::uint8_t {
    Solid = 0,     //  实线 + 实心箭头（同步调用）
    Dotted,        //  虚线 + 实心箭头（同步返回）
    Open,          //  实线 + 开口箭头（异步）
    OpenDotted,    //  虚线 + 开口箭头
    Cross,         //  实线 + 叉箭头（丢失消息）
    CrossDotted,   //  虚线 + 叉箭头
};

struct SeqParticipant {
    std::string key;     // 引用名（消息/便签用它指认参与者）
    std::string label;   // 显示名；空则回退 key
    bool actor = false;  // true：小人人设（保留字段；本渲染器统一画头框）
};

struct SeqMessage {
    std::string from;
    std::string to;
    std::string text;
    SeqArrowKind kind = SeqArrowKind::Solid;
    bool activate_target = false;    // 到达后激活 to（Mermaid `+` 简写）
    bool deactivate_source = false;  // 发出前停用 from（Mermaid `-` 简写）
};

struct SeqNote {
    enum class Placement : std::uint8_t { Left, Right, Over };
    Placement placement = Placement::Over;
    std::string anchor;    // Left/Right：单参与者；Over 单人时也用它
    std::string anchor_b;  // Over 跨两人时的第二人
    std::string text;
    std::size_t after = static_cast<std::size_t>(-1);  // 出现在第 after 条消息之后
};

struct SeqModel {
    std::vector<SeqParticipant> participants;
    std::vector<SeqMessage> messages;
    std::vector<SeqNote> notes;

    // key → 参与者下标；未找到返回 size_t(-1)。
    [[nodiscard]] TERMCHART_API std::size_t index_of(std::string_view key) const noexcept;
};

struct UmlSequenceOptions {
    double h_gap = 8.0;    // 头框之间的间隔（给消息与文本留位）
    double margin = 1.0;   // 左边距
    Style header_style{};    // 头框
    Style lifeline_style{};  // 生命线与激活条
    Style message_style{};   // 消息线、箭头与文本
    Style note_style{};      // 便签
};

// 就地绘制到画布（超出画布的部分由 canvas 裁剪）。返回内容包围盒；空模型返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_uml_sequence(const SeqModel& model,
                                                     canvas::Canvas& canvas,
                                                     const UmlSequenceOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/label_place.hpp =====
// label_place.hpp —— 标签避让（在锚点周围挑一个不与已有矩形相交的位置）



namespace termchart::graph {

struct LabelPlacement {
    Rect rect{};
    // false 表示四个候选位都与 occupied 相交或越出左/上边界，此时 rect 为首选位。
    bool placed = false;
};

// 候选顺序：右、左、上、下。返回第一个可用位置（不与 occupied 相交，且不越出
// 左/上边界）。occupied 可为空。
[[nodiscard]] TERMCHART_API LabelPlacement place_label(
    Point anchor, std::uint32_t width, std::uint32_t height,
    const std::vector<Rect>& occupied) noexcept;

}  // namespace termchart::graph

// ===== include/termchart/graph/node.hpp =====
// node.hpp —— 图节点（标识 + 标签 + 元数据 + 布局外框）
//
// Node 只持有"语义"与"布局结果"两类信息：语义（id/label/attributes）由调用方
// 填写，外框（box）由 graph/layout 写入，供 route/ 与渲染消费。节点不持有任何
// 绘制状态，因此同一张图可以用字符画、SVG 或 GUI 三种方式渲染。



namespace termchart::graph {

using NodeId = std::uint32_t;

// 0 保留为"无效/无节点"，便于把 0 当作默认值。
inline constexpr NodeId kInvalidNode = 0;

class Node {
public:
    Node(NodeId id, std::string label);

    [[nodiscard]] NodeId id() const noexcept { return id_; }

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void set_label(std::string label) { label_ = std::move(label); }

    [[nodiscard]] Attributes& attributes() noexcept { return attributes_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    // 布局结果：节点在画布上的外框（格坐标）。未布局时为默认空矩形。
    [[nodiscard]] const Rect& box() const noexcept { return box_; }
    void set_box(Rect box) noexcept { box_ = box; }

private:
    NodeId id_;
    std::string label_;
    Attributes attributes_;
    Rect box_{};
};

}  // namespace termchart::graph

// ===== include/termchart/graph/edge.hpp =====
// edge.hpp —— 图的有向边（端点 + 标签 + 元数据 + 路由折线）



namespace termchart::graph {

enum class ArrowStyle {
    None = 0,
    AtTarget,  // 仅终点带箭头
    Both,      // 两端都带箭头
};

class Edge {
public:
    Edge(NodeId from, NodeId to, std::string label = {});

    [[nodiscard]] NodeId from() const noexcept { return from_; }
    [[nodiscard]] NodeId to() const noexcept { return to_; }

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    void set_label(std::string label) { label_ = std::move(label); }

    [[nodiscard]] Attributes& attributes() noexcept { return attributes_; }
    [[nodiscard]] const Attributes& attributes() const noexcept { return attributes_; }

    [[nodiscard]] ArrowStyle arrow() const noexcept { return arrow_; }
    void set_arrow(ArrowStyle arrow) noexcept { arrow_ = arrow; }

    // 由 graph/route 写入的折线（格坐标，首尾为端点锚点）。
    [[nodiscard]] const std::vector<Point>& route() const noexcept { return route_; }
    void set_route(std::vector<Point> route) { route_ = std::move(route); }

    // 路由是否"被迫退化为直连兜底"（正交/平滑/弧线路由在 Z 形/L 形/贝塞尔全部失败后
    // 写出的 2 点直线，不保证不穿过节点框/障碍）。由 graph/route 在写出兜底直线时置
    // true；调用方/渲染器可读它提示用户"此处过挤、路由未达预期"，而不是被静默画出的
    // 穿障直线蒙蔽（见路由层审查 雷区1）。直线风格（RouteStyle::Straight）是显式需求，
    // 不算失败。
    [[nodiscard]] bool route_failed() const noexcept { return route_failed_; }
    void set_route_failed(bool v) noexcept { route_failed_ = v; }

private:
    NodeId from_;
    NodeId to_;
    std::string label_;
    Attributes attributes_;
    ArrowStyle arrow_ = ArrowStyle::AtTarget;
    std::vector<Point> route_;
    bool route_failed_ = false;
};

}  // namespace termchart::graph

// ===== include/termchart/graph/graph.hpp =====
// graph.hpp —— 图容器（节点 + 有向边 + 邻接查询）
//
// Graph 是图可视化的语义层：只管"有哪些节点、怎么连"，不管布局与绘制。
// 布局（graph/layout）、路由（graph/route）、形状（graph/shape）与图元
// （graph/diagrams）都在这之上构建，因此同一张 Graph 可以被多种渲染消费。
//
// 注意：add_node 返回的是 vector 内元素的引用，后续 add_node 可能使引用失效；
// 需要长期持有请用 id() 再 find()。



namespace termchart::graph {

class Graph {
public:
    // 新节点 id 从 1 起自增（0 保留为"无效"）。返回引用可能在后续插入后失效。
    Node& add_node(std::string label);

    [[nodiscard]] Node* find(NodeId id) noexcept;
    [[nodiscard]] const Node* find(NodeId id) const noexcept;

    // 删除节点及其全部关联边；不存在返回 false。
    bool remove_node(NodeId id);

    // 端点必须已存在；否则返回 nullptr 且不添加。**返回指针**（而 add_node 返回引用）
    // 是因为加边真的可能失败；需要长期持有请取 `Edge::id()` 再 `find_edge()`。
    Edge* add_edge(NodeId from, NodeId to, std::string label = {});

    // 按端点找回第一条匹配边：与节点的 find(NodeId) 对称（此前只有节点能按 id
    // 找回，边只能靠"当时拿到的指针"——而指针在容器重排后会失效）。
    // 边本身没有 id（from/to 才是它的身份），所以这里按 (from, to) 查。
    [[nodiscard]] Edge* find_edge(NodeId from, NodeId to) noexcept;
    [[nodiscard]] const Edge* find_edge(NodeId from, NodeId to) const noexcept;
    // 删除 from→to 的第一条边；不存在返回 false。
    bool remove_edge(NodeId from, NodeId to);

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Edge>& edges() const noexcept { return edges_; }
    // 可变访问：布局写 Node::set_box、路由写 Edge::set_route 时需要。
    [[nodiscard]] std::vector<Node>& nodes() noexcept { return nodes_; }
    [[nodiscard]] std::vector<Edge>& edges() noexcept { return edges_; }
    [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }

    // 与该节点相邻的边（出边与入边都算），按边的登记顺序。
    [[nodiscard]] TERMCHART_API std::vector<const Edge*> edges_of(NodeId id) const;
    [[nodiscard]] TERMCHART_API std::size_t out_degree(NodeId id) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t in_degree(NodeId id) const noexcept;

    // 所有节点外框的并集；无节点返回默认空矩形。布局之后才有意义。
    [[nodiscard]] TERMCHART_API Rect bounds() const noexcept;

    void clear();

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    NodeId next_id_ = 1;
};

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/layout.hpp =====
// layout.hpp —— 图布局（把节点摆到画布格上，写入 Node::set_box）
//
// 布局只做几何：给定图与选项，为每个节点算出外框。它不画任何东西，也不
// 关心节点形状与连线，因此同一份布局结果可以被字符画、SVG 或 GUI 复用。



namespace termchart::graph {

struct LayoutOptions {
    double min_width = 10.0;   // 节点外框最小宽（格）
    // 最小高 4：3 行的框除上下边框外只剩 1 个内部行，同侧多条边会被迫挤到
    // 边框行上（观感像从角上进出）。4 行有 2 个内部行可供端口扇出。
    double min_height = 4.0;
    double h_gap = 4.0;        // 水平间距
    double v_gap = 2.0;        // 垂直间距
    double margin = 1.0;       // 画布边距
    // 层内坐标分配是否做"邻居对齐"（Sugiyama 第三步的朴素版）。默认关。
    // 实测（examples/60_dense_sugiyama，70 节点 / 98 边）：
    //   紧堆（false）：交叉 37、总长 1328 格、拐点 180
    //   朴素对齐（true）：交叉 40、总长 1942 格、拐点 204
    // 朴素版把节点推到"邻居中位数"却不限制层高膨胀，空隙全摊到竖直段上，
    // 除长边折返（2→0）外全指标更差。正确做法需要 Brandes–Köpf 的 block/shift
    // 来约束膨胀，等它落地后再考虑把这步打开。
    bool align_rows = false;
    // align_rows = true 时的层高膨胀上限（相对紧堆高度）。对齐用"图更高"换"边更直"，
    // 但换太多就退回来：超过该比例的层整体回退到紧堆位置，该层的对齐收益放弃、
    // 图高不受影响。0 = 只允许零膨胀（几乎等于不回退），1 = 允许翻倍。
    double align_slack = 0.15;
};

// 单个节点的外框尺寸（原点在 0,0）：宽度按标签显示宽度取 max(最小宽, 标签宽+2)。
[[nodiscard]] TERMCHART_API Rect node_box_for(const Node& node,
                                              const LayoutOptions& opts) noexcept;

// 网格布局：按 cols 列从左到右、从上到下摆放；cols 为 0 时按 1 处理。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_grid(Graph& graph, std::uint32_t cols,
                                              const LayoutOptions& opts = {});

// 分层布局：按最长路径给节点定层（层即列），层内按登记顺序纵向排列。
// 有向环上的节点无法定层，退到第 0 层（布局仍保证不重叠）。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_layered(Graph& graph,
                                                 const LayoutOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/social_network.hpp =====
// social_network.hpp —— 社交网络图（环形布局的节点-连线视图）
//
// 字符化策略：人画成带边框的盒子并居中标名，按注册顺序均分圆环摆位，
// 关系画直线连线（社交关系无向，默认不带箭头）。环形布局在本层内联实现
// （等弦间隔防重叠），绘制复用 diagrams 的共享骨架。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct SocialNetworkOptions {
    LayoutOptions layout{};
    double radius = 0.0;  // 0 = 按节点尺寸与间距自动取
    Style node_style{};
    Style edge_style{};
    bool node_border = true;
    bool arrows = false;
};

// 就地环形布局并绘制社交网络图，返回整图包围盒。
TERMCHART_API Rect render_social_network(Graph& graph, canvas::Canvas& canvas,
                                         const SocialNetworkOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/circular.hpp =====
// circular.hpp —— 圆周布局（所有节点均匀分布在一个圆上）
//
// 与 radial 的“按父子关系分环”不同，圆周布局不看边，只按**节点登记顺序**把
// 全部节点均匀摆在一个圆上：第 i 个节点的角度为 θ_i = start_angle + 2π·i/n。
// 它适合“没有层次、只想看一圈并列节点”的场景（如维度环、雷达式清单）。
//
// 半径由节点数与节点尺寸推出，保证相邻节点框不重叠：
//   相邻中心在格坐标系里的最小距离为 d_min = 2·(R/aspect)·sin(π/n)
//   （y 方向被压缩了 1/aspect，最紧的方向在 x 轴附近，故分母带 aspect）。
// 以节点框的外接圆半径 ρ_i = √(w²+h²)/2 作为“两框不重叠”的**充分**判据：
//   d_min ≥ ρ_i + ρ_j + gap  ⇒  外接圆互不相交  ⇒  矩形互不相交。
// 于是 R = aspect·(max 相邻 ρ_i+ρ_j + gap) / (2·sin(π/n))，再用 min_radius 兜底。
//   · 用外接圆是刻意取保守（“推一个足够的半径”）：它忽略矩形朝向，绝不会算小；
//   · gap 还额外吸收落格取整带来的约 ±1 格误差，默认 2 足够。
// n = 1 时半径取 0（单节点落在圆心）；n ≥ 2 时半径才由上式与 min_radius 共同决定。


namespace termchart::graph {

struct CircularOptions {
    LayoutOptions layout{};       // 节点尺寸与边距
    double gap = 2.0;             // 相邻节点框之间的期望间距（格），并吸收取整误差
    double min_radius = 8.0;      // 半径兜底下限（n ≥ 2 时生效）
    double aspect = 2.0;          // 字符格宽高比（宽:高）；口径同 canvas/transform.hpp
    // 第一个节点的角度（弧度），默认 -π/2，即第一个节点在正上方，顺时针排开。
    double start_angle = -1.57079632679489661923;
};

// 圆周布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_circular(Graph& graph,
                                                 const CircularOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/cluster.hpp =====
// cluster.hpp —— 聚类布局（按组内聚、组间分离地摆放节点）
//
// 分组规则：节点带 attributes["cluster"]（字符串）时按属性值分组；没带属性的
// 节点按**无向连通分量**自动归组。每个聚类内部用网格（列数取 ceil(√n)）摆放，
// 整个聚类的包围盒再按网格排布（列数取 ceil(√k)），聚类之间留 cluster_gap。
// 返回所有节点外框的并集；不同聚类的包围盒保证互不重叠。


namespace termchart::graph {

struct ClusterOptions {
    LayoutOptions layout{};   // 节点尺寸与边距（margin 同时是聚类的起点边距）
    double cluster_gap = 4.0; // 聚类包围盒之间的间距（格）
};

// 聚类布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_cluster(Graph& graph,
                                                const ClusterOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/force.hpp =====
// force.hpp —— 力导向布局（Fruchterman-Reingold 简化版）
//
// 两条力：节点两两斥力 k^2/d，边方向弹簧引力 d^2/k。每轮按"温度"限制单步
// 位移并逐步降温，收敛后取整落格。
//
// 确定性：初始摆放固定为圆环（不用随机数），迭代与浮点运算顺序固定，因此同一
// 张图两次布局结果完全一致——这样布局才能进 golden 与回归测试。



namespace termchart::graph {

struct ForceOptions {
    LayoutOptions layout{};          // 节点尺寸与边距
    double ideal_edge_length = 12.0; // 理想边长 k
    std::uint32_t iterations = 240;  // 迭代轮数
    double cooling = 0.95;           // 每轮温度衰减
    double min_step = 0.01;          // 温度低于此值提前收敛
    double gravity = 0.05;           // 朝原点的弱引力（收拢无连边分量，防漂散）
};

// 力导向布局：就地写入 Node::set_box，返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_force(Graph& graph,
                                              const ForceOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/kamada_kawai.hpp =====
// kamada_kawai.hpp —— KK 全局能量布局（弹簧模型）
//
// 以图的最短路径（跳数）为目标距离做弹簧能量最小化：每轮选能量残差最大的节点
// 对，对其一端做 Newton 迭代解局部能量极小，直到所有残差低于 tolerance 或迭代
// 耗尽。初始摆放固定为圆环（不用随机数），同一张图两次布局结果完全一致。
//
// 退化口径：N=0 返回空矩形；N=1 只落一个节点框；N=2 直接按目标距离水平对放，
// 不进迭代；非连通图按连通分量各自独立布局后从左到右并排摆放（分量间无弹簧
// 约束，最短路径距离不存在）。



namespace termchart::graph {

struct KamadaKawaiOptions {
    LayoutOptions layout{};              // 节点尺寸与边距
    double edge_length = 12.0;           // 单位跳数的目标距离（格）
    std::uint32_t max_iterations = 200;  // 主循环（选对 + 移动）上限
    double tolerance = 1e-3;             // 最大残差（相对目标距离）低于此值提前收敛
};

// KK 全局能量布局：就地写入 Node::set_box，返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_kamada_kawai(
    Graph& graph, const KamadaKawaiOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/radial.hpp =====
// radial.hpp —— 径向树布局（极坐标 + 子树扇区均分）
//
// 思路与经典“径向树（radial tree）”一致：先把有向图按 BFS 摊成一棵根树（与
// tree_layout 同法：入度为 0 的第一个节点为根，全有入边时取第一个节点；只沿出边、
// 首次访问为准，因此有环图也能摆），再给每个节点分配一个角扇区：
//   · 根独占整圆 [start_angle, start_angle + 2π)；
//   · 父节点把自己的扇区**按子节点数均分**给每个孩子（每个孩子再继续细分）。
// 于是同一父下的兄弟角度间隔相等；兄弟的扇区互不相交 → 以兄弟为根的子树占据
// 互不相交的角度范围，子树之间天然不交叉。
//
// 半径只由层号决定：radius(level) = ring_start + level * ring_step。
// 为什么半径按层线性取：径向树的“环”本就是等距同心圆，层号相同 → 半径相同、
// 一眼能看出层次；层号越大 → 离根越远。ring_step 取“节点高 + 间距”量级即可，
// 默认 12：竖直方向经宽高比压缩后为 12/2 = 6 ≥ 节点最小高 4 + 2，相邻环不挤。
//
// 字符格不是正方形（宽 ≈ 高的 2 倍），正圆直接落格会显扁。落格时按
// canvas/transform.hpp::aspect_correction 的同一口径把 y 压缩 1/aspect：
//     gx = cx + r·cos θ，gy = cy + r·sin θ / aspect。
// 因此在格坐标里量到的距离要按同口径还原（dx、dy·aspect）才等于几何半径 r，
// 本布局的“层半径”指的就是这个还原后的真实半径。
//
// 重叠：同一层里角度极近的两个节点，取整落格后可能落在同一格。本布局**允许**
// 这种重叠，但保证**确定**——纯几何、无随机，同输入逐格一致。要减少重叠可把
// min_angle 调大：它给同一父的相邻子扇区之间留出最小角间隙。


namespace termchart::graph {

struct RadialOptions {
    LayoutOptions layout{};       // 节点尺寸与边距
    double ring_step = 12.0;      // 相邻层半径增量（格）
    double ring_start = 0.0;      // 根的半径；0 表示根落在圆心
    double min_angle = 0.0;       // 同一父相邻子扇区的最小角间隙（弧度）；0 = 纯均分
    double aspect = 2.0;          // 字符格宽高比（宽:高）；口径同 canvas/transform.hpp
    // 根扇区起始角（弧度），默认 -π/2，即第一个子节点朝正上方，顺时针铺开。
    double start_angle = -1.57079632679489661923;
};

// 径向树布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
// 根取入度为 0 的第一个节点；全部节点都有入边（整图是环）时取第一个节点。
[[nodiscard]] TERMCHART_API Rect layout_radial(Graph& graph,
                                               const RadialOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/sugiyama.hpp =====
// sugiyama.hpp —— 四步分层布局（Sugiyama 框架）
//
// 在 layout_layered 的"分层 + 坐标分配"之上补上第二步"同层顺序交叉最小化"
// （median / barycenter 启发式），让复杂 DAG 的同层节点不再按登记顺序简单堆叠
// 而交叉凌乱。四步：① 最长路径分层（含破环）② 同层交叉最小化 ③ 坐标分配
// ④ 边路由（由调用方在布局后调用 route，本函数只做几何）。


namespace termchart::graph {

// 四步分层布局。iterations 为交叉最小化双向扫描轮数（默认 4，越大越收敛越慢）。
// 返回所有节点外框的并集。
[[nodiscard]] TERMCHART_API Rect layout_sugiyama(Graph& graph,
                                                 const LayoutOptions& opts = {},
                                                 std::uint32_t iterations = 4);

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/tree_layout.hpp =====
// tree_layout.hpp —— 根树布局（层级 + 兄弟子树行槽）
//
// 做法：从根开始 BFS 建树（首次访问为准，回边与重复边忽略，因此有环图也能摆），
// 后序分配"行槽"——叶子各占一行，父节点的行取子树中心；列由深度决定。
// left_right 模式把根的子树按登记顺序分成两半，一半向右生长、一半镜像向左，
// 根落在中间，用于思维导图。


namespace termchart::graph {

struct TreeOptions {
    LayoutOptions layout{};   // 节点尺寸与边距
    double row_gap = 2.0;     // 行间距
    bool left_right = false;  // true：根居中，子树分列左右（思维导图）
};

// 树布局：就地写入 Node::set_box，返回所有节点外框的并集。
// 根取入度为 0 的第一个节点；全部节点都有入边时取第一个节点。
[[nodiscard]] TERMCHART_API Rect layout_tree(Graph& graph,
                                             const TreeOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/port.hpp =====
// port.hpp —— 节点外框上的连接端口（连线路由的锚点）


namespace termchart::graph {

enum class PortSide {
    North = 0,
    East,
    South,
    West,
};

// 外框某一侧上的锚点，落在**边框格**上（不含框外），越界坐标已被夹取。
// t 为侧向归一化位置（0 = 该侧起点格，1 = 该侧终点格），按格索引取中点：
// 高 3 的框取 t=0.5 得中间那一行，而不是连续几何上的 1.5 格。
[[nodiscard]] TERMCHART_API Point port_anchor(Rect box, PortSide side,
                                               double t = 0.5) noexcept;

[[nodiscard]] TERMCHART_API PortSide opposite(PortSide side) noexcept;

// 从 from 指向 to 时，最合适的离开/进入侧（按主轴方向判定）。
[[nodiscard]] TERMCHART_API PortSide side_towards(Point from, Point to) noexcept;

}  // namespace termchart::graph

// ===== include/termchart/graph/arrow.hpp =====
// arrow.hpp —— 箭头头部（字符与占格几何）



namespace termchart::graph {

// 箭头头部字符（按指向方向）：U+25B6 / U+25C0 / U+25B2 / U+25BC。
[[nodiscard]] TERMCHART_API char32_t arrow_head_char(PortSide dir) noexcept;

// 箭头头部占据的格：tip 及其后一格（沿指向的反方向退一格），按远离 tip 排序。
[[nodiscard]] TERMCHART_API std::vector<Point> arrow_head_cells(Point tip, PortSide dir);

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/circuit.hpp =====
// circuit.hpp —— 电路图（元件字符符号 + 引脚正交连线）
//
// 字符化策略：元件画成固定尺寸的字符符号——电阻是带内标的矩形框、电容是两块
// 平行板（─│ │─，板间断开）、电池是长板+短板、地是逐级收窄的横杠（│ + ─── +
// ─ + ·）；引脚连线走正交走线，转折收口成 ┌ ┐ └ ┘，中段逐格避让元件框
// （找不到空档才按 L 形直连，宁可直穿也不消失——与 route/ 的退化口径一致）。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class ComponentKind {
    Resistor = 0,  // 电阻：5×3 矩形框，标签画在框内（放不下画到框下）
    Capacitor,     // 电容：─│ │─ 两块平行板，板间断开
    Battery,       // 电池：长板（3 行）+ 短板（1 行）
    Ground,        // 地：│ + ─── + ─ + ·（5×5，引脚在顶）
};

struct CircuitComponent {
    ComponentKind kind = ComponentKind::Resistor;
    std::string label;
    Point at{};  // 元件外框左上角（格坐标）
};

// 引脚 = 元件下标 + 引出侧；坐标由该元件外框推算（落在边框格上）。
struct CircuitPin {
    std::size_t component = 0;
    PortSide side = PortSide::East;
};

struct CircuitWire {
    CircuitPin from{};
    CircuitPin to{};
};

struct CircuitOptions {
    Style glyph_style{};
    Style label_style{};
    Style wire_style{};
};

// 就地绘制；返回元件与标签的包围盒（元件表为空返回零矩形）。
// 元件下标越界的连线跳过。
[[nodiscard]] TERMCHART_API Rect render_circuit(const std::vector<CircuitComponent>& parts,
                                                const std::vector<CircuitWire>& wires,
                                                canvas::Canvas& canvas,
                                                const CircuitOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/route/junction.hpp =====
// junction.hpp —— 轴对齐折线的交汇口字符
//
// 为什么把它抽成公共件：这套"按这一格有上/下/左/右哪几个方向来选方块字符"的规则
// 原先在 `src/graph/diagrams/orgchart.cpp` 与 `examples/17_uml_class.cpp` 里**各有一份**
// （各写一张 switch 表）。地铁线路图会是第三个消费者 —— 三份复制必然漂移，所以在
// 第三个出现之前把它收到这里，规则只留一份。
//
// 与 `segment_char` 的分工：`segment_char`（diagrams 骨架内部）判的是**单段线**该用
// `─`/`│`/`╱`/`╲`；本函数判的是**交汇口**（横竖相接处）该用哪个 T 形/十字字符，
// 否则折角会渲染成两个独立字符拼出的"断角"。


namespace termchart::graph {

// 四种方向连通性 → 方块字符。规则（方块字符按"缺哪边"选）：
//
//   上+下          │      只有竖向延续
//   左+右          ─      只有横向延续
//   下+右          ┌      上、左缺
//   下+左          ┐      上、右缺
//   上+右          └      下、左缺
//   上+左          ┘      下、右缺
//   上+下+右       ├      左缺
//   上+下+左       ┤      右缺
//   左+右+下       ┬      上缺
//   左+右+上       ┴      下缺
//   四向           ┼
//
// 退化口径（刻意定义清楚，免得调用方各自猜）：
//   * **只有一个方向**：按该方向的轴返回线字符（左/右 → `─`，上/下 → `│`）；
//     单方向不是交汇口，但那里确实有一段线，返回线字符比返回空格更不容易出错。
//   * **零个方向**：返回空格（`U' '`）—— 调用方据此跳过该格。
//
// 本函数是**纯数据表**判据，故做成 header-only `constexpr`（在
// `cmake/header_only.txt` 登记）：调用方可以直接在常量表达式里用（示例里的
// `constexpr` 包装函数就依赖这一点），也没有任何运行期开销。
[[nodiscard]] constexpr char32_t junction_char(bool up, bool down, bool left,
                                               bool right) noexcept {
    switch (static_cast<unsigned>(up) | (static_cast<unsigned>(down) << 1) |
            (static_cast<unsigned>(left) << 2) | (static_cast<unsigned>(right) << 3)) {
        case 0x3:  // 上+下
            return U'\u2502';
        case 0xC:  // 左+右
            return U'\u2500';
        case 0xA:  // 下+右
            return U'\u250C';
        case 0x6:  // 下+左
            return U'\u2510';
        case 0x9:  // 上+右
            return U'\u2514';
        case 0x5:  // 上+左
            return U'\u2518';
        case 0xB:  // 上+下+右
            return U'\u251C';
        case 0x7:  // 上+下+左
            return U'\u2524';
        case 0xE:  // 左+右+下
            return U'\u252C';
        case 0xD:  // 左+右+上
            return U'\u2534';
        case 0xF:  // 四向
            return U'\u253C';
        // 单方向：不是交汇口，但那一格确实有一段线，返回对应轴的线字符。
        case 0x4:
        case 0x8:
            return U'\u2500';
        case 0x1:
        case 0x2:
            return U'\u2502';
        default:  // 零方向
            return U' ';
    }
}

}  // namespace termchart::graph

// ===== include/termchart/graph/shape/shape.hpp =====
// shape.hpp —— 节点形状（字符单元格粒度的轮廓）
//
// 形状只描述"哪些格构成边框、用什么字符"，不含填充与样式决策；绘制由 draw_shape
// 完成。轮廓格同时服务于两件事：绘制，以及占用/命中判定。
//
// 精度说明：这里全部是**字符格粒度**的解析轮廓（逐行算左右边界），不是子像素
// 光栅化。曲线（椭圆/胶囊端）在格粒度下必然有台阶，这是终端字符网格的固有限制，
// 不做假精度；需要更平滑的曲线请走 raster/（盲文/象限）那条路径。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class ShapeKind {
    Box = 0,    // 直角矩形
    Rounded,    // 圆角矩形
    Diamond,    // 菱形（用于判定节点）
    Ellipse,    // 椭圆（起止节点）
    Stadium,    // 胶囊形（两端半圆，流程图里的"终止"）
    Hexagon,    // 六边形（准备/循环）
    Cylinder,   // 圆柱（数据存储）
    Icon,       // 不画边框：内容由调用方提供（图标/图片节点）
};

// 形状边框占据的格（不含内部；宽高不足 2 时退化为单格或空）。
// Icon 返回空（它没有边框）。
[[nodiscard]] TERMCHART_API std::vector<Point> shape_outline(ShapeKind kind, Rect box);

// 把形状边框画到画布上。越界部分由 canvas 自行裁剪。Icon 不写入任何格。
TERMCHART_API void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box,
                              const Style& style);

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/orgchart.hpp =====
// orgchart.hpp —— 组织架构图（层级树 + 共享树干 + ├/└ 接点）
//
// 与思维导图的区别（两者都用树）：
//   * 思维导图把根摆在中间、子树左右分列，连线按序号错开成"扇形"；
//   * 组织架构图是**单向层级**：深度沿 x 向右增长，同父的兄弟共用一条**树干**
//     （一条竖线），每个子节点从树干上"分叉"出去，接点画成 `├` / `└` / `┌`
//     而不是把直角拆成两个独立字符 —— 这正是终端里 `tree` 类工具的形状，
//     在字符网格上比逐条正交路由（互相穿插的 Z 形）好读得多。
//
// 节点内容就是 `Node::label()`（居中一行）。要显示"姓名 + 职务"这类两行内容，
// 请把职务并进标签（例如 "CTO / 平台"）——本图刻意不做第二行，避免与边框争夺行数。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct OrgChartOptions {
    TreeOptions tree{};   // 节点尺寸、行距（深度向右，行距即兄弟间距）
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
    bool node_border = true;  // false：只画标签与连线
};

// 就地布局 + 路由 + 绘制；返回所有节点外框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_orgchart(Graph& graph, canvas::Canvas& canvas,
                                                 const OrgChartOptions& options = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/uml_usecase.hpp =====
// uml_usecase.hpp —— UML 用例图（参与者小人 + 椭圆用例 + 无箭头关联线）
//
// 字符化策略：参与者画成三行**小人**（O 头 / /|\ 身 / / \ 腿，名字在下），
// 用例画成**椭圆**（ShapeKind::Ellipse + 居中名）；参与者—用例之间连**无箭头**
// 直线（关联）。参与者站两翼：节点属性 "side" == "left"/"right" 显式指定，
// 缺省按边方向判（只作为 from → 左；只作为 to → 右；混合 → 左）。
//
// 模型约定（沿用 graph::Graph）：Node::attributes() 的 "kind" == "actor" 为
// 参与者，其余节点一律按用例画椭圆。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct UmlUsecaseOptions {
    LayoutOptions layout{};   // 用例框的最小尺寸与间距；参与者框高固定 4 行
    double h_gap = 6.0;       // 参与者列与用例列之间的间隔
    Style actor_style{};      // 小人与参与者名
    Style usecase_style{};    // 椭圆与用例名
    Style edge_style{};       // 关联线
};

// 就地布局并绘制用例图，返回所有元素外框的并集。空图返回零矩形。
// 布局从左往右排（左参与者列 → 用例列 → 右参与者列）；画布不够宽的部分被裁剪。
[[nodiscard]] TERMCHART_API Rect render_uml_usecase(Graph& graph, canvas::Canvas& canvas,
                                                    const UmlUsecaseOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/wiregrid.hpp =====
// wiregrid.hpp —— 连线占位栅格（避免路由互相压线）
//
// 路由算法把候选线段先放进 WireGrid 探测：整段可用才真正占用。占用是
// **原子**的——中途碰到已占用的格，整段都不占用并返回 0，便于调用方直接换
// 下一条候选路径重试，不会留下半截占用。



namespace termchart::graph {

class WireGrid {
public:
    WireGrid() = default;
    WireGrid(std::uint32_t width, std::uint32_t height);

    void resize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t occupied_count() const noexcept { return occupied_; }
    [[nodiscard]] bool empty() const noexcept { return occupied_ == 0; }

    [[nodiscard]] bool in_bounds(std::uint32_t x, std::uint32_t y) const noexcept {
        return in_range(x, y);
    }
    // 越界视为未占用；需要区分"越界"时先查 in_bounds。
    [[nodiscard]] bool occupied(std::uint32_t x, std::uint32_t y) const noexcept;

    // 单格占用：越界或已占用返回 false，且不改动状态。
    bool occupy(std::uint32_t x, std::uint32_t y);
    void release(std::uint32_t x, std::uint32_t y);
    void clear();

    // 只探测不占用：整段都在界内且未占用才返回 true。
    [[nodiscard]] bool can_occupy_h(std::uint32_t x0, std::uint32_t x1,
                                    std::uint32_t y) const noexcept;
    [[nodiscard]] bool can_occupy_v(std::uint32_t y0, std::uint32_t y1,
                                    std::uint32_t x) const noexcept;

    // 原子占用水平/垂直线段；成功返回占用的格数，失败返回 0 且不改动状态。
    std::size_t occupy_h(std::uint32_t x0, std::uint32_t x1, std::uint32_t y);
    std::size_t occupy_v(std::uint32_t y0, std::uint32_t y1, std::uint32_t x);

private:
    [[nodiscard]] bool in_range(std::uint32_t x, std::uint32_t y) const noexcept {
        return x < width_ && y < height_;
    }
    [[nodiscard]] std::size_t index_of(std::uint32_t x, std::uint32_t y) const noexcept {
        return static_cast<std::size_t>(y) * width_ + x;
    }

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::size_t occupied_ = 0;
    std::vector<std::uint8_t> cells_;
};

}  // namespace termchart::graph

// ===== include/termchart/graph/route/route.hpp =====
// route.hpp —— 连线路由（为每条边算出折线）
//
// 路由的输入是"已布局"的图（节点外框已就位）；输出写进 Edge::set_route。
// 直线是最小实现；正交路由走"Z 形"，并可借助 WireGrid 避让已有连线（先是
// 中点，其次向两侧逐格绕行；全部失败则退化为直线，不占用栅格）。
// 端点在同一行/同一列时，Z 形的共线中间点会被化简掉，结果是一条轴对齐直线段
// （这本身就是正交路由的合法退化，不是错误）。
// 两条约定：同侧多条边按 (节点, 侧) 分组均匀分配锚点，避免共用同一格；锚点
// 吸附到整数格（字符网格上没有半格），绕行中点同样取整。



namespace termchart::graph {

enum class RouteStyle {
    Straight = 0,  // 锚点直连
    Orthogonal,    // 轴对齐 Z 形折线
};

struct RouteOptions {
    int max_detour = 8;  // 正交路由向两侧尝试的最大偏移（格）
    // 是否把"已路由的连线"也当障碍。默认开：稀疏图里连线互不重叠更好看。
    // 密集图（几十条边挤在层间通道里）建议关：连线之间本就必然相交，硬要
    // 互避会让后来的边找不到任何正交通路、退化成斜线；交叉处由 junction_char
    // 合成 ┼ 表达，比斜线乱穿清楚得多。节点外框无论此开关如何都是障碍。
    bool avoid_wires = true;
    // 尊重已有的折线：布局阶段（layout_sugiyama）会给跨多层的长边写一条沿
    // dummy 槽位走的正交通道折线，比这里的通用 Z 形更准（不会撞中间列的节点框）。
    // 默认为 false：不看已有 route，全部按本函数重算，行为与以前一致。
    bool keep_existing_routes = false;
};

// 为所有边计算折线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// grid 非空且风格为 Orthogonal 时做避让。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_edges(Graph& graph, RouteStyle style,
                                                    const RouteOptions& opts = {},
                                                    WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/dag.hpp =====
// dag.hpp —— 有向无环图渲染入口
//
// 入口先做环检测（Kahn 拓扑排序，见 is_acyclic），渲染策略：
//   * 无环：分层布局（层 = 拓扑深度，所有边一律从左往右跨层）+ 正交路由。
//   * 有环：**不报错、不丢边**——退化为普通分层布局（layout_layered 自带破环，
//     环上节点挂到已定层前驱的下一层），环的回边照常画出并带箭头。调用方可先
//     用 is_acyclic() 自行决定是否提示用户。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct DagOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
    bool arrows = true;
    bool edge_labels = false;
};

// 环检测：Kahn 拓扑排序能覆盖全部节点即无环（自环也算环）。
[[nodiscard]] TERMCHART_API bool is_acyclic(const Graph& graph);

// 就地布局并绘制 DAG，返回整图包围盒。有环时按上面的退化策略渲染。
TERMCHART_API Rect render_dag(Graph& graph, canvas::Canvas& canvas,
                              const DagOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/dependency.hpp =====
// dependency.hpp —— 依赖图渲染入口（建在 dag 之上）
//
// 约定：边 A→B 表示"A 依赖 B"。布局与 dag 相同（分层 + 依赖方向一致：所有边
// 从左往右跨层推进，被依赖者位于更右的层）。在 dag 的基础上多一件事：
// **依赖环标记**——mark_cycles 打开时，处于环上的边（两端在同一强连通分量，
// 自环也算）被改写 ArrowStyle::Both（两端箭头，⇄ 观感）以提示互相依赖；
// 这是会改写图的绘制状态的操作，关闭选项则不碰任何 ArrowStyle。
// 环判定用"逐边反向 BFS"（边数 * (V+E)），终端图规模下足够；不引入 Tarjan。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct DependencyOptions {
    DagOptions dag{};
    bool mark_cycles = true; // 环上的边两端画箭头（会改写 ArrowStyle）
};

// 就地布局并绘制依赖图，返回整图包围盒。有环时同样退化渲染（见 dag.hpp）。
TERMCHART_API Rect render_dependency(Graph& graph, canvas::Canvas& canvas,
                                     const DependencyOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/er_diagram.hpp =====
// er_diagram.hpp —— ER 图（实体表框 + 鸭掌记号基数 + 关系名标签）
//
// 字符化策略：实体画成**表框**——实体名居中一行、├─┤ 分隔、字段左对齐（关系
// 模式即"表"）；关系按正交折线绘制，两端用**鸭掌记号**（crow's foot）标注基数，
// 记号画在紧贴实体边框的那段线上：
//   * 恰一（One）     → 两道垂直短杠 `││`
//   * 多（Many）      → 鸭掌 `<` `>` `∧` `∨`（按进框方向选）
//   * 零或一（0..1）  → 圆圈 `○` + 一道短杠 `│`
// 关系名作边标签，用 label_place 避让实体框（放不下宁可不画）。
//
// 模型约定（沿用 graph::Graph）：
//   * Node::label() 是实体名；Node::attributes() 的 "fields" 是字段清单，
//     多行以 '\n' 连接；
//   * Edge::label() 是关系名；Edge::attributes() 的 "card_from" / "card_to"
//     是两端基数，取值 "1"（恰一）、"N"/"many"/"*"（多）、"0..1"（零或一），
//     缺省按恰一处理。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

enum class ErCardinality : std::uint8_t {
    One = 0,       // 恰一：两道短杠
    Many,          // 多：鸭掌
    ZeroOrOne,     // 零或一：圆圈 + 短杠
};

// 基数拼写 → 枚举；无法识别时按恰一（One）。
[[nodiscard]] TERMCHART_API ErCardinality parse_er_cardinality(std::string_view text) noexcept;

struct ErDiagramOptions {
    LayoutOptions layout{};  // min_width/min_height 会被实体内容自动撑大
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style entity_style{};   // 实体框与分隔线
    Style name_style{};     // 实体名
    Style field_style{};    // 字段
    Style edge_style{};     // 关系线、鸭掌与关系名
    bool draw_cardinality = true;
};

// 就地布局、路由并绘制 ER 图，返回所有实体框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_er_diagram(Graph& graph, canvas::Canvas& canvas,
                                                   const ErDiagramOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/flowchart.hpp =====
// flowchart.hpp —— 流程图（图 --> 画布 的一步到位入口）
//
// 组合关系固定为：分层布局 -> 路由（正交/直线）-> 绘制形状、标签、箭头。
// 需要单独控制某一步时，直接调用 graph/layout、graph/route、graph/shape。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct FlowchartOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Box;
    Style node_style{};
    Style edge_style{};
};

// 就地布局并绘制到画布（超出画布的部分由 canvas 裁剪）。返回整图包围盒。
TERMCHART_API Rect render_flowchart(Graph& graph, canvas::Canvas& canvas,
                                    const FlowchartOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/knowledge_graph.hpp =====
// knowledge_graph.hpp —— 知识图谱（实体-关系网络）
//
// 字符化策略：实体画带边框的盒子并居中标名，关系画直线连线（可带箭头与
// 关系标签）；布局复用力导向（layout_force），绘制复用节点-连线视图
// （render_node_link）。本层只做"布局 + 视图"的选项组合，不新增绘制逻辑。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct KnowledgeGraphOptions {
    ForceOptions force{};   // 力导向布局参数
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Straight;
    Style node_style{};
    Style edge_style{};
    bool node_border = true;
    bool arrows = false;       // 知识图谱关系方向常弱于结构，默认不画箭头
    bool edge_labels = false;  // 关系名（复用 label_place 避让节点框）
};

// 就地力导向布局并绘制知识图谱，返回整图包围盒。
TERMCHART_API Rect render_knowledge_graph(Graph& graph, canvas::Canvas& canvas,
                                          const KnowledgeGraphOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/mindmap.hpp =====
// mindmap.hpp —— 思维导图 + 通用"节点-连线"视图
//
// render_node_link 是**不做布局**的通用渲染：调用方先自行布局（layout_force /
// layout_grid / ...），它只负责把节点与边画出来。放在本头文件，是因为思维导图
// 正是这类视图的典型形态；力导向图（`examples/20_force_graph`）复用它。
//
// render_mindmap = layout_tree(left_right) + render_node_link（默认不画边框，
// 标签直接作为节点，连线即分支）。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct NodeLinkOptions {
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Straight;
    Style node_style{};
    Style edge_style{};
    bool node_border = false;
    bool arrows = false;
    bool edge_labels = false;
};

// 通用节点-连线渲染（不做布局，调用方先布局）。
TERMCHART_API void render_node_link(Graph& graph, canvas::Canvas& canvas,
                                    const NodeLinkOptions& opts = {});

struct MindmapOptions {
    TreeOptions tree;       // 默认 left_right = true（根居中，分支分列两侧）
    ShapeKind shape = ShapeKind::Box;
    Style topic_style{};    // 标签样式
    Style branch_style{};   // 分支连线样式
    bool node_border = false;

    MindmapOptions() { tree.left_right = true; }
};

// 就地布局并绘制思维导图，返回整图包围盒。
TERMCHART_API Rect render_mindmap(Graph& graph, canvas::Canvas& canvas,
                                  const MindmapOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/petri_net.hpp =====
// petri_net.hpp —— Petri 网（库所 / 变迁 / 有向弧 + token）
//
// 字符化策略：库所画椭圆、token 画 ●（超过上限改画数字），变迁画窄高矩形，
// 弧走正交折线带箭头；节点按弧可达深度分列（库所偶数列、变迁奇数列），
// 名称画在形状下方。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct PetriNet {
    struct Place {
        std::string name;
        std::uint32_t tokens = 0;
    };
    struct Transition {
        std::string name;
    };
    // 弧端点：(kind, index) 引用库所或变迁。
    struct Endpoint {
        bool is_place = true;
        std::size_t index = 0;
    };
    struct Arc {
        Endpoint from;
        Endpoint to;
    };

    static constexpr Endpoint place(std::size_t index) noexcept {
        return Endpoint{true, index};
    }
    static constexpr Endpoint transition(std::size_t index) noexcept {
        return Endpoint{false, index};
    }

    std::vector<Place> places;
    std::vector<Transition> transitions;
    std::vector<Arc> arcs;
};

struct PetriNetOptions {
    LayoutOptions layout{};  // margin/h_gap/v_gap 生效；节点尺寸按图种固定
    RouteOptions route{};
    Style place_style{};
    Style transition_style{};
    Style arc_style{};
    std::uint32_t max_token_dots = 5;  // token 数超过上限改画数字
};

// 就地布局并绘制 Petri 网，返回整图包围盒。弧端点越界时该弧被跳过。
TERMCHART_API Rect render_petri_net(const PetriNet& net, canvas::Canvas& canvas,
                                    const PetriNetOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/state_machine.hpp =====
// state_machine.hpp —— 状态机图（分层布局 + 转移标签）
//
// 与流程图的差别：状态画圆角框、**转移事件名画在边上**（复用 label_place 避让），
// 并按节点属性标注起止状态：
//   attributes["initial"] == "1" → 左侧画 ● 与引入短线
//   attributes["final"]   == "1" → 右侧画 ◎


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct StateMachineOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    ShapeKind shape = ShapeKind::Rounded;  // 状态用圆角框
    Style node_style{};
    Style edge_style{};
    bool mark_initial_final = true;
};

// 就地布局并绘制状态机图，返回整图包围盒。
TERMCHART_API Rect render_state_machine(Graph& graph, canvas::Canvas& canvas,
                                        const StateMachineOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/uml_activity.hpp =====
// uml_activity.hpp —— UML 活动图（圆角动作框 + 菱形判定 + ●/◎ 起止标记）
//
// 字符化策略：动作 = 圆角框；判定节点 = **菱形**（节点属性 "kind" == "decision"，
// 守卫条件走边标签，如 "[yes]"）；起止不占整框——没有显式 "initial"/"final"
// 属性时，自动把**首个人度 0 的节点**标为起点（框外左侧 `●` + 引入线）、
// **首个出度 0 的节点**标为终点（框外右侧 `◎`），标记方案与 state_machine 一致。
//
// 模型约定（沿用 graph::Graph）：Node::label() 是动作/判定名；节点属性
// "kind" == "decision" 判定，其余为动作；"initial"/"final" == "1" 显式指定起止。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct UmlActivityOptions {
    LayoutOptions layout{};
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style node_style{};      // 动作框
    Style decision_style{};  // 判定菱形
    Style edge_style{};      // 连线、箭头、守卫标签与起止标记
    bool mark_start_end = true;
};

// 就地布局并绘制活动图，返回整图包围盒。空图返回零矩形。
// 自动起止标记会把 "initial"/"final" 属性写回图（显式指定过则不动）。
[[nodiscard]] TERMCHART_API Rect render_uml_activity(Graph& graph, canvas::Canvas& canvas,
                                                     const UmlActivityOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/uml_class.hpp =====
// uml_class.hpp —— UML 类图（三栏类框 + 空心三角泛化 + 正交连线）
//
// 字符化策略：类框画成**三栏矩形**——类名居中、属性/操作左对齐，两栏之间用
// ├─┤ 分隔线（UML 分类格在字符网格上的等价表达，画法同 examples/17_uml_class）；
// 泛化边（默认）按正交折线绘制，并在**基类（from）端的边框格**上画空心三角
// `◁`（UML ▷ 语义指向基类）；边属性 "kind" == "assoc" 的边退化为普通箭头。
//
// 模型约定（沿用 graph::Graph，无需新容器）：
//   * Node::label() 是类名；
//   * Node::attributes() 的 "attrs" / "ops" 分别是属性栏与操作栏内容，
//     多行以 '\n' 连接，每行自带 UML 可见性前缀（+ 公开 / - 私有 / # 保护）。


namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct UmlClassOptions {
    LayoutOptions layout{};  // min_width/min_height 会被成员内容自动撑大
    RouteOptions route{};
    RouteStyle route_style = RouteStyle::Orthogonal;
    Style border_style{};   // 类框边框与分隔线
    Style name_style{};     // 类名
    Style member_style{};   // 属性与操作
    Style edge_style{};     // 连线与三角/箭头
    bool generalization_triangles = true;  // false：泛化也画普通箭头
};

// 就地布局、路由并绘制类图，返回所有类框的并集。空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect render_uml_class(Graph& graph, canvas::Canvas& canvas,
                                                  const UmlClassOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/layout/metro.hpp =====
// metro.hpp —— 地铁布局（线网 45° 走线）
//
// 真实地铁路网的画法约定：站点吸附到八线栅格（0°/45°/90°/135°），线路沿
// 45° 斜线与轴对齐段走线。本文件的实现分两层：
//
// **layout_metro（布局）**：把图按无向邻接贪心分解成若干条"线"（链，覆盖全部
// 节点，孤立节点是长度 1 的链），每条线在自己的水平带内沿 45° 之字排列——
// 链上相邻节点的外框原点恰好相差 (step, ±step)（换向处相差 (step, 0)），
// 因此锚点直连天然就是 45° 斜线或水平段。step 会被抬到不小于最大节点框的
// 宽与高，保证相邻框不相交：45° 段只在框外空档里走，**链内走线不穿框**由
// 几何直接保证，不依赖栅格检查。
//
// **route_metro（路由）**：与布局配套的 45° 走线。每条边先试锚点直连（仅当
// 恰为 45° 或轴对齐），再试 45° 步进走线（斜着走到对齐、再轴对齐直进），逐步
// 在栅格上查障碍；不通则退化为正交路由（复用 route_edges 的 Z 形策略），最后
// 退化为直线。跨线边（链与链之间）没有布局层的几何保证，靠这里的栅格检查兜底。



namespace termchart::graph {

struct MetroOptions {
    LayoutOptions layout{};  // 节点尺寸与边距
    double step = 6.0;       // 45° 步长（相邻链节点的 x/y 位移）；会被抬到 ≥ 最大框宽高
    std::uint32_t zigzag = 3;  // 之字半周期（每 zigzag 个节点换向一次）
    double line_gap = 3.0;   // 两条线（水平带）之间的间距
};

// 地铁布局：就地写入 Node::set_box，返回所有节点外框的并集；空图返回零矩形。
[[nodiscard]] TERMCHART_API Rect layout_metro(Graph& graph,
                                              const MetroOptions& opts = {});

struct MetroRouteOptions {
    RouteOptions route{};  // 正交兜底时的绕行上限
};

// 45° 走线：为所有边计算折线并写入 Edge::set_route（首尾为节点边框锚点）。
// grid 非空时在其上避让并占用；为空时自建覆盖全图并留绕行余量的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_metro(Graph& graph,
                                                    const MetroRouteOptions& opts = {},
                                                    WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/graph/diagrams/metro_map.hpp =====
// metro_map.hpp —— 地铁线路图（线路 = 有序站点序列 + 线色）
//
// 字符化策略：站点画带边框的小盒并居中标名（同名站点合并为换乘站），线路
// 沿 45° 走线并按线色着色（复用 layout_metro + route_metro，不画箭头）。



namespace termchart::canvas {
class Canvas;
}

namespace termchart::graph {

struct MetroLine {
    std::string name;                   // 线路名（预留，当前不绘制）
    Color color{};                      // 线色；none 时用 fallback_line_style
    std::vector<std::string> stations;  // 有序站点（同名 = 换乘站）
};

struct MetroMapOptions {
    MetroOptions metro{};
    Style station_style{};
    Style fallback_line_style{};  // 线路未指定颜色时的走线样式
    bool station_borders = true;
};

// 就地布局并绘制地铁线路图，返回整图包围盒。空站点序列的线路被忽略。
TERMCHART_API Rect render_metro_map(const std::vector<MetroLine>& lines,
                                    canvas::Canvas& canvas,
                                    const MetroMapOptions& opts = {});

}  // namespace termchart::graph

// ===== include/termchart/graph/route/arc.hpp =====
// arc.hpp —— 弧线边路由（含自环）
//
// 输出与其它路由一致：格坐标折线，且是 **8 连通格序列**（相邻格只差一行/一列/一格
// 对角），斜向步由既有 segment_char 渲染成 ╱ ╲，因此这里只产出几何、不产出字符。
//
// 两种形态：
//   * 普通边（两端是不同节点）：三次贝塞尔弧——控制点沿弦方向外推一格距（保证离开
//     锚点时先朝框**外**走，不会先贴着边框列切进节点）、再按垂直于弦的方向隆起，
//     隆起量约为弦长的一半（"跨列半圆"的观感）。
//   * 自环边（起终点同一节点）：绕到节点框**外侧**的一圈。锚点取在同一个节点相邻
//     两条边上（高框取东侧上下两个锚点，矮框取东侧 + 南侧绕右下角），先水平离框，
//     再把外侧折角用二次贝塞尔抹圆；因为整圈都在框外，节点标签（画在框内）不会被
//     覆盖。
//
// 退化策略（刻意，且可从"不穿框"反推）：候选弧一旦压到任何节点框内部或已占用栅格
// 就作废，依次尝试更小的隆起量 / 两侧方向；都不通则退化为直线（放不下时宁可直连，
// 也不让弧线斜穿节点）。自环放不下时退化为未抹圆的正交外形，再不行退化为锚点直连。



namespace termchart::graph {

// 为所有边计算弧线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// opts.max_detour 同时充当自环向外绕出的格数与隆起量上限；grid 非空时在其上做
// 避让并占用，为空时自建一块覆盖全图的栅格。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_arc(Graph& graph, const RouteOptions& opts = {},
                                                  WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/graph/route/bundling.hpp =====
// bundling.hpp —— 边捆绑（共享目标节点的边在末段并束）
//
// 逐边独立路由时，多条边各占一条走廊涌入目标节点，观感杂乱。边捆绑把
// **共享同一目标节点**的边分成一组：组内先在目标锚点外侧 bundle_gap 格处定一个
// 公共"并束点"，每条边先以正交 Z 形走到并束点，再共享从并束点到锚点的**末段**
// 进框。整组只占用目标的一个锚点与一条末段走廊。
//
// 并束方向：从目标中心指向组内各源点中心的质心，取主轴侧作为进入侧；
// 并束点必须落在所有节点框之外，否则逐步加大距离重试，仍不行则该组放弃捆绑，
// 逐边按普通正交路由。自环边不参与捆绑（锚点重合，没有"末段"可言）。



namespace termchart::graph {

struct BundlingOptions {
    RouteOptions route{};    // 组内逐边走到并束点的绕行上限
    double bundle_gap = 3.0; // 并束点离目标锚点的距离（格）
};

// 为所有边计算折线并写入 Edge::set_route（首尾为节点边框锚点）。
// grid 非空时在其上避让并占用；为空时自建覆盖全图并留绕行余量的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_bundled(Graph& graph,
                                                      const BundlingOptions& opts = {},
                                                      WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/graph/route/obstacle.hpp =====
// obstacle.hpp —— 障碍避让路由（外部矩形障碍绕行）
//
// 画布上常有不属于图的元素（图例、表格、已画好的面板），连线必须绕开。
// 把"外部障碍"（std::vector<Rect>，画布坐标系）登记进 WireGrid，再走
// route_edges 的正交路由（Z 形逐级绕行，失败退化为直线）。
//
// 约定：障碍四周外扩 clearance 格禁行；负坐标障碍裁剪到 0（WireGrid 是
// 无符号栅格）；grid 为空时自建覆盖全部节点、障碍与绕行余量的栅格。



namespace termchart::graph {

struct ObstacleOptions {
    RouteOptions route{};   // 正交绕行上限
    double clearance = 1.0; // 障碍四周的额外禁行格数
};

// 障碍避让路由：为所有边计算正交折线并写入 Edge::set_route，折线（锚点除外）
// 不进入任何节点框或外扩后的障碍。返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_avoiding(
    Graph& graph, const std::vector<Rect>& obstacles, const ObstacleOptions& opts = {},
    WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/graph/route/spline.hpp =====
// spline.hpp —— 平滑曲线边路由（Catmull-Rom 穿过正交途经点）
//
// 与 route.hpp 的直线/正交一样，输出仍是"格坐标折线"，只是把折线换成平滑曲线：
// 以锚点之间的正交途经点当控制多边形，按弦长自适应采样 Catmull-Rom 曲线，再逐格
// 取整成 **8 连通格序列**（相邻格只差一行/一列/一格对角）。斜向步由既有
// graph/diagrams/internal.hpp 的 segment_char 渲染成 ╱ ╲，横向/纵向渲染成 ─ │，
// 因此本文件只产出几何、不产出字符（与 primitives.hpp 的分工一致）。
//
// 三条刻意约定：
//   * **宁可折线也不穿框**：曲线一旦压到任何节点框内部（或已占用栅格），该候选直接
//     作废，回退到同一控制多边形对应的正交折线；正交也不通时回退直线。曲线在格粒度
//     下必然有台阶，回退折线比斜穿节点好。
//   * 首末点严格等于锚点：控制多边形以锚点起止，采样后又显式钉死首末格，取整漂移
//     不会传到端点。
//   * grid 为空时自建障碍栅格：不穿框是硬约定，不能因为调用方没给栅格就放弃避让
//     （这点与 route_edges 不同）。
//
// 自环边（A→A）的锚点会重合，spline 不处理自环，请改用 graph/route/arc.hpp。



namespace termchart::graph {

// 为所有边计算平滑曲线并写入 Edge::set_route（首尾为节点边框上的锚点）。
// 参数语义与 route_edges 一致：opts.max_detour 决定途经点向两侧尝试的偏移上限；
// grid 非空时在其上做避让并占用，为空时自建一块覆盖全图的栅格。
// 返回写入折线的边数。
[[nodiscard]] TERMCHART_API std::size_t route_spline(Graph& graph, const RouteOptions& opts = {},
                                                     WireGrid* grid = nullptr);

}  // namespace termchart::graph

// ===== include/termchart/render/compositor.hpp =====
// compositor.hpp —— 多图层合成（把若干 CellBuffer 按偏移叠加成最终帧）
//
// GUI / 重叠元素 / 多 Pass 渲染都需要"图层"概念：本组件把任意数量的图层
// （每个是一个 CellBuffer + 左上角偏移）合成到一张目标缓冲。后绘制的图层覆盖
// 先绘制的；transparent 时，空格单元不覆盖下层（透出底色）。



namespace termchart::render {

struct Layer {
    const CellBuffer* buffer = nullptr;  // 非空图层缓冲（不拷贝，零成本）
    std::int32_t x = 0;                  // 左上角列偏移
    std::int32_t y = 0;                  // 左上角行偏移
    bool transparent = true;             // 空格单元是否透出下层
};

class Compositor {
public:
    // 合成到给定的 out（out 尺寸需已足够容纳最远图层；否则用重载版本自动扩展）。
    TERMCHART_API static void composite(const std::vector<Layer>& layers, CellBuffer& out);
    // 自动按包围盒分配 out 并返回。
    [[nodiscard]] TERMCHART_API static CellBuffer composite(const std::vector<Layer>& layers);
};

}  // namespace termchart::render

// ===== include/termchart/render/damage.hpp =====
// damage.hpp —— 脏区域跟踪
//
// 与 diff.hpp 的分工：
//   * diff.hpp 回答"两个 buffer 之间哪里变了"（纯函数，可测）；
//   * 本文件维护"本帧累计的脏区"（有状态），供 renderer 决定重绘范围与
//     将来 canvas 的部分重绘。
// 存储形态：每行若干条**已合并**的半开区间 [begin, end)。



namespace termchart::render {

class DamageTracker {
public:
    struct Run {
        std::uint32_t begin = 0;
        std::uint32_t end = 0;

        constexpr bool operator==(const Run&) const noexcept = default;
        [[nodiscard]] constexpr bool empty() const noexcept { return end <= begin; }
    };

    struct RowRun {
        std::uint32_t row = 0;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;

        constexpr bool operator==(const RowRun&) const noexcept = default;
    };

    TERMCHART_API void resize(std::uint32_t width, std::uint32_t height);
    TERMCHART_API void clear() noexcept;

    TERMCHART_API void mark(std::uint32_t x, std::uint32_t y) noexcept;
    TERMCHART_API void mark_run(std::uint32_t row, std::uint32_t begin, std::uint32_t end) noexcept;
    TERMCHART_API void mark_row(std::uint32_t row) noexcept;
    TERMCHART_API void mark_all() noexcept;

    [[nodiscard]] TERMCHART_API bool dirty(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] TERMCHART_API bool dirty_row(std::uint32_t row) const noexcept;

    // 展平后的脏区列表（按行升序、区间升序）。
    [[nodiscard]] TERMCHART_API std::vector<RowRun> runs() const noexcept;

    [[nodiscard]] TERMCHART_API std::span<const Run> runs_of(std::uint32_t row) const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t run_count() const noexcept;

    [[nodiscard]] TERMCHART_API std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] TERMCHART_API std::uint32_t height() const noexcept { return height_; }

private:
    TERMCHART_API void insert(std::uint32_t row, Run run) noexcept;

    std::vector<std::vector<Run>> rows_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

}  // namespace termchart::render

// ===== include/termchart/render/diff.hpp =====
// diff.hpp —— 差分重绘
//
// 纯函数层：输入两个 CellBuffer，输出"变化的连续段"。
// 合并规则：同一行内相邻/重叠的变化段合并成一段（多写一个字节的跳转序列
// 比多写十个字节的样式重置便宜得多）。



namespace termchart::render {

struct DiffRegion {
    std::uint32_t row = 0;
    std::uint32_t begin = 0;   // 含
    std::uint32_t end = 0;     // 不含

    constexpr bool operator==(const DiffRegion&) const noexcept = default;
    [[nodiscard]] constexpr bool empty() const noexcept { return end <= begin; }
};

// 尺寸不同视为"全变"：返回 true（调用方应走全量重绘）。
TERMCHART_API bool identical(const CellBuffer& previous, const CellBuffer& current) noexcept;

// 逐行比较。size 不同返回空列表（调用方必须先处理尺寸变化）。
// merge_gap <= 0 表示"不合并"，> 0 表示相隔不超过该距离的段也合并。
TERMCHART_API std::vector<DiffRegion> diff(const CellBuffer& previous, const CellBuffer& current,
                                           std::uint32_t merge_gap = 4) noexcept;

}  // namespace termchart::render

// ===== include/termchart/render/renderer.hpp =====
// renderer.hpp —— 终端实时渲染管线（输出点）
//
// 终端实时路径的最后一环：CellBuffer → 增量字节流。
//   * 持有 prev 状态与 damage 跟踪，因此**一帧只输出变化的部分**；
//   * 与 snapshot 导出（backend/snapshot/*）的区别就在"有 prev 状态、有 diff"；
//   * 本类只生成字节，不写 stdout —— 写由 api/print.hpp 或调用方负责，
//     于是渲染在 CI / headless / 单元测试里完全可用。



namespace termchart::render {

class Renderer {
public:
    explicit Renderer(ColorDepth depth = ColorDepth::TrueColor) noexcept;
    TERMCHART_API ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // 可移动：否则"按值返回 Renderer"或放进容器都得绕道（历史上删了拷贝却漏了移动，
    // 配合裸 Impl* 更是误加拷贝就 double-free）。
    // 移出后的对象只剩"可析构 / 可被重新赋值"两个合法操作，与标准库的约定一致。
    TERMCHART_API Renderer(Renderer&&) noexcept;
    TERMCHART_API Renderer& operator=(Renderer&&) noexcept;

    TERMCHART_API void set_depth(ColorDepth depth) noexcept;
    [[nodiscard]] TERMCHART_API ColorDepth depth() const noexcept { return depth_; }

    // 增量渲染：与内部 prev 比较。首帧或尺寸变化时自动退化为全量。
    TERMCHART_API std::string render(const CellBuffer& current) noexcept;

    // 全量渲染：忽略 prev（用于首帧、能力变化、被强制刷新）。
    TERMCHART_API std::string render_full(const CellBuffer& current) noexcept;

    // 丢弃 prev 状态：下次 render() 必为全量（终端被清屏后必须调用）。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] TERMCHART_API const CellBuffer* previous() const noexcept;
    [[nodiscard]] TERMCHART_API const DamageTracker& damage() const noexcept;
    [[nodiscard]] TERMCHART_API DamageTracker& damage() noexcept;

private:
    class Impl;
    // unique_ptr 而非裸指针：所有权由类型表达，误加拷贝会在编译期被拦住，
    // 而不是运行期 double-free。析构与移动因此在 .cpp 里定义（那里 Impl 才完整）。
    std::unique_ptr<Impl> impl_;
    ColorDepth depth_ = ColorDepth::TrueColor;
};

}  // namespace termchart::render

// ===== include/termchart/render/sgr.hpp =====
// sgr.hpp —— SGR（颜色 / 属性）序列的**唯一生成点**
//
// 阶段 1 的硬决策（ROADMAP R2）：所有 SGR 只从这里出去。
//   * 终端实时路径（render/renderer.hpp）用它；
//   * snapshot 导出（backend/snapshot/ansi_backend.hpp）也用它 —— 为此登记了
//     唯一收窄边 `backend/snapshot/ansi_backend > render/sgr`（见 cmake/TermchartModules.cmake）。
// 后果：改序列化规则只需改本文件，所有路径同步生效，不会出现"两套 SGR"。



namespace termchart::render::sgr {

// 完整样式序列（含从"默认状态"出发的全部参数）。depth == None 时不输出颜色参数。
TERMCHART_API void append_style(std::string& out, const Style& style, ColorDepth depth) noexcept;
TERMCHART_API std::string style_sequence(const Style& style, ColorDepth depth) noexcept;

// 增量：只输出 from → to 变化的那部分。渲染器每帧的输出量取决于它。
// 属性只发"变化的位"；颜色未变则完全不产生字节。
TERMCHART_API void append_style_delta(std::string& out, const Style& from, const Style& to,
                                      ColorDepth depth) noexcept;

// 复位到终端默认：ESC[0m。行尾/帧尾收口用。
TERMCHART_API std::string reset() noexcept;

// 生成"关闭指定属性"的序列（bold→22 等）；供 delta 内部与测试复用。
TERMCHART_API void append_attr_off(std::string& out, AttrMask off_bits) noexcept;
TERMCHART_API void append_attr_on(std::string& out, AttrMask on_bits) noexcept;

// 单个颜色的参数段（不含 ESC[ 与 m）：38;2;r;g;b / 38;5;n / 31 / 39 ...
TERMCHART_API void append_color_params(std::string& out, const Color& color, bool foreground,
                                       ColorDepth depth) noexcept;

}  // namespace termchart::render::sgr

// ===== include/termchart/interaction/brush.hpp =====
// brush.hpp —— 刷选（数据点集的"笔刷式"吸附多选）
//
// 职责切分（为什么单开一个文件）：
//   * 只做"指针途经了哪些点"这一件事：给定 (数据坐标, payload) 点集，按吸附
//     半径把落笔/拖动途经的点收进选中集。不碰终端、不碰渲染、不认识图表类型，
//     只依赖 L0 的 Point —— 与 zoom/pan/select 同族的纯几何交互组件。
//   * "事件 → 手势"的路由（按下/拖动/松开映射到 begin/stroke/end）由上层或
//     immediate.hpp 完成；本类只吃几何量，可在无终端的单测里完整驱动。
//
// 手势模型（三段：落笔 → 途经 → 提笔）：
//   * begin(p)    进入刷选态，并按 p 吸附一次；
//   * stroke(p)   仅刷选态有效：途经吸附（未 begin 时是 no-op，防呆）；
//   * end()       退出刷选态；选中集**保留**供查询（像画笔提笔，画不擦）。
//
// 不变量（构造后与每次状态变更后都维持）：
//   * selected() 的顺序 = 各 payload 的**首次吸附顺序**，且无重复：同一
//     payload 对应的多个点、或同一点被反复途经，都只记一次；
//   * 选中集只增不减（begin 不清空上一笔——"刷"是累积语义，多笔画选多个
//     区域后统一读出），唯一的减少途径是 clear()；
//   * set_items() 随时可换点集，不改变当前选中集与手势状态（选中集按
//     payload 记录，与点集解耦；被移除点的 payload 仍留在选中集里）。
//
// 退化输入（明确不吸附、不改选中集；无需特判，比较语义天然给出）：
//   * radius 非有限（NaN/Inf）：半径平方为 NaN，所有距离比较为 false，
//     永不吸附；
//   * radius = 0：仅与点精确重合时吸附（精确拾取）；radius < 0：平方后与
//     |radius| 等价；
//   * 指针坐标或点坐标非有限：距离比较为 false，不吸附、不崩溃。



namespace termchart::interaction {

struct BrushOptions {
    double radius = 1.5;  // 吸附半径（数据坐标量纲，由调用方决定）
};

class Brush {
public:
    explicit Brush(BrushOptions options = {}) noexcept;

    // 替换数据点集（内部拷贝一份）；不改变当前选中集与手势状态。
    void set_items(const std::vector<std::pair<Point, std::uint32_t>>& items) noexcept;

    // 落笔：进入刷选态，并按 p 吸附一次。已在刷选态再次 begin 等价于继续刷。
    void begin(Point p) noexcept;

    // 拖动途经：按 p 吸附（去重）。未 begin 时 no-op。
    void stroke(Point p) noexcept;

    // 提笔：退出刷选态，选中集保留。
    void end() noexcept;

    // 清空选中集并退出刷选态（唯一能减少选中集的入口）。
    void clear() noexcept;

    [[nodiscard]] bool active() const noexcept;  // begin 与 end 之间

    // 选中 payload 列表：按首次吸附顺序，去重。
    [[nodiscard]] std::vector<std::uint32_t> selected() const;

    // payload 是否在选中集里。
    [[nodiscard]] bool selected(std::uint32_t payload) const noexcept;

    [[nodiscard]] const BrushOptions& options() const noexcept;

private:
    // 把 p 半径内的点并入选中集（去重、保持首次顺序）。
    void snap(Point p) noexcept;

    BrushOptions options_{};
    std::vector<std::pair<Point, std::uint32_t>> items_;
    std::vector<std::uint32_t> order_;  // 首次吸附顺序
    bool active_ = false;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/crosshair.hpp =====
// crosshair.hpp —— 十字准线（悬停读数覆盖层）
//
// 职责：把一次"悬停/定位"落成画布上的水平 + 垂直准线（可选轴侧读数），并在
// 下一次绘制或 clear() 时**精确还原**被改写的格 —— 否则悬停一次就永久污染画布
// （覆盖层必须能在撤销时把原字符与样式逐格放回去）。这是本类唯一的核心不变量。
//
// 与既有模块的关系：
//   * 只依赖 L0/L10（core + canvas），不碰 render / term / 终端；
//   * 不拥有画布，只在调用方传入的 Canvas 上叠加，故可在无终端单测里完整驱动。
//
// 叠加口径（默认"保留数据字符、只改样式/背景"）：
//   * preserve_data == true（默认）：准线经过的格**保留 cp**，只把样式换成
//     "原样式叠加 line_style"（line_style 里未指定的前景/背景保持原值，属性位按
//     或并入）。于是折线/柱状的数据字符不会被准线吃掉，只被高亮。
//   * preserve_data == false：写入连接字符（交叉点 cross_cp，横线 horizontal_cp，
//     竖线 vertical_cp），整格换成 line_style。
//
// 裁剪与退化口径：
//   * 准线**严格**裁剪在 plot_rect 内（半开区间 [x, x+w) × [y, y+h)）：区外一格不写；
//   * at 落在 plot_rect 外，或 plot_rect 任一方向不足 2 格（退化成点/线段，失去
//     "十字"语义）→ 明确不画、返回 false（对应"绘图区只有 1 格"的退化输入）；
//   * readout 是**唯一**允许落在绘图区外的元素（画在轴侧，即绘图区下边框的下方），
//     仍由画布裁剪兜底；默认 show_readout == false，需要读数时显式打开——
//     这样"draw 只改绘图区内的格"在默认配置下是硬不变量。



namespace termchart::interaction {

struct CrosshairOptions {
    Rect plot_rect{};             // 绘图区（半开区间）；任一方向 < 2 格视为退化
    bool preserve_data = true;    // 默认保留数据字符、只改样式
    bool show_readout = false;    // 是否在轴侧写读数（默认关，保证"只改区内"）
    char32_t horizontal_cp = U'─';
    char32_t vertical_cp = U'│';
    char32_t cross_cp = U'┼';
    // 准线样式：默认给一层暗底，肉眼可见又不动字符。未设的很/背景不覆盖原值。
    Style line_style = Style{}.set_bg(Color::ansi16(8));
};

class Crosshair {
public:
    Crosshair() = default;
    explicit Crosshair(CrosshairOptions options) noexcept;

    // 换配置。不主动碰画布：新的配置会在下一次 draw() 时生效，而 draw() 会先还原
    // 上一次的改写，因此"换配置后重绘"不会残留旧准线。
    TERMCHART_API void set_options(CrosshairOptions options) noexcept;
    [[nodiscard]] TERMCHART_API const CrosshairOptions& options() const noexcept;

    // 绘制到格坐标 at（只画落在 plot_rect 内的部分）。readout 非空且 show_readout
    // 时，把该文本写在下边框外侧（轴侧）。返回是否真的落笔。
    // 重复调用会先还原上一次的改写再按新点重绘 —— 幂等叠加，不累积污染。
    TERMCHART_API bool draw(canvas::Canvas& canvas, Point at, std::string_view readout = {});

    // 还原全部被改写的格（逐格逐 char/样式）。返回是否真的还原过；未绘制时为 no-op。
    // 注意：若画布尺寸在绘制后被改小，被裁掉的格无法还原 —— 调用方应先 clear 再 resize。
    TERMCHART_API bool clear(canvas::Canvas& canvas);

    [[nodiscard]] TERMCHART_API bool active() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t patch_count() const noexcept;

private:
    struct Patch {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        Cell before{};
    };

    // 还原 patches_ 并清空；返回是否还原过任何格。
    bool restore(canvas::Canvas& canvas);
    // 记录 (x,y) 的原格（若在画布内且尚未记录过）；去重靠 captured_ 标记表。
    void capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y);
    // 把去重标记表重置为 width × height（全 0）。
    void reset_captured(std::uint32_t width, std::uint32_t height);

    CrosshairOptions options_{};
    std::vector<Patch> patches_;
    std::vector<std::uint8_t> captured_;  // 尺寸 = captured_w_ * captured_h_
    std::uint32_t captured_w_ = 0;
    std::uint32_t captured_h_ = 0;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/detail/overlay_cells.hpp =====
// overlay_cells.hpp —— 覆盖层的"改写格簿记"共享实现（interaction 模块私有 detail）
//
// 为什么单独抽出来：crosshair 与 tooltip 都要"记住被覆盖的格 → 精确还原"，此前
// 各自复制了一份（~25 行）。还原口径一旦分叉，就会出现"某个覆盖层悬浮一次永久污染
// 画布"的隐形 bug，且很难被单测发现。把唯一一份算法放这里，两个覆盖层共用。
//
// 为什么用模板而不是具体类型：两个覆盖层各自持有**私有嵌套**的 Patch 结构体
// （仅存 x/y/原格），以 Patch 作模板参数即可在**不改动各自公开头**的前提下复用；
// 本头从而不引入任何具体类型、不产生外部符号（纯模板 + inline）。
//
// 物理布局：本头位于 include/.../detail/，按仓库约定 header-only、无同名 .cpp。
// tools/check_header_only.py 只豁免**顶层** detail/，故本头已登记在 cmake/header_only.txt。



namespace termchart::interaction::detail {

// 开始一次新的绘制：清空补丁，并按画布尺寸重建"去重标记表"（width × height，全 0）。
// 去重的意义：一次绘制内横线、竖线、读数文本可能覆盖同一格，只有**第一次**看到的
// 原值才是真正的"绘制前状态"，后续重复记录会把被改写的值当成原值、还原不干净。
template <class Patch>
inline void begin_log(std::vector<Patch>& patches, std::vector<std::uint8_t>& markers,
                      std::uint32_t width, std::uint32_t height) {
    patches.clear();
    markers.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                   std::uint8_t{0});
}

// 记录 (x,y) 的原格：画布外或已记录则忽略。width 与 begin_log 传的必须一致
// （用于把二维坐标压成线性索引）。
template <class Patch>
inline void capture_cell(const canvas::Canvas& canvas, std::vector<Patch>& patches,
                         std::vector<std::uint8_t>& markers, std::uint32_t width,
                         std::uint32_t x, std::uint32_t y) {
    if (x >= canvas.width() || y >= canvas.height()) {
        return;  // 画布外：既不记录也不绘制
    }
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(x);
    if (index >= markers.size() || markers[index] != 0) {
        return;  // 已记录：保留第一次的原值
    }
    markers[index] = 1;
    patches.push_back(Patch{x, y, canvas.at(x, y)});
}

// 逐格还原（原字符 + 原样式）并清空；返回是否真的还原过任何格。
// patches 为空时是纯 no-op（返回 false），调用方可据此跳过无谓重绘。
template <class Patch>
inline bool restore_cells(canvas::Canvas& canvas, std::vector<Patch>& patches,
                          std::vector<std::uint8_t>& markers) {
    if (patches.empty()) {
        return false;
    }
    for (const Patch& patch : patches) {
        canvas.put(patch.x, patch.y, patch.before.cp, patch.before.style);
    }
    patches.clear();
    markers.clear();
    return true;
}

}  // namespace termchart::interaction::detail

// ===== include/termchart/interaction/focus.hpp =====
// focus.hpp —— 焦点管理（在给定的可聚焦顺序上前后移动）



namespace termchart::interaction {

class FocusChain {
public:
    // id == 0 表示"无焦点"。
    TERMCHART_API void set(std::uint32_t id) noexcept;
    [[nodiscard]] TERMCHART_API std::uint32_t current() const noexcept;
    [[nodiscard]] TERMCHART_API bool is_focused(std::uint32_t id) const noexcept;
    TERMCHART_API void clear() noexcept;

    // 在 order 内移动；当前不在 order 中时落到首/末项。order 为空则不变。
    TERMCHART_API void next(const std::vector<std::uint32_t>& order) noexcept;
    TERMCHART_API void prev(const std::vector<std::uint32_t>& order) noexcept;

private:
    std::uint32_t current_ = 0;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/highlight.hpp =====
// highlight.hpp —— 悬停高亮 + 锚点放大（纯状态，不吃 Event）
//
// 职责切分（为什么单开一个文件）：
//   * "悬停到哪个数据点 + 以该点为中心看多大范围"是纯几何/纯状态问题，与终端、
//     渲染管线、具体图表类型统统无关，只依赖 L0 的 Point/Rect。
//   * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
//     set_hover/clear 调用。payload 是**不透明 id**（同 chart::HitRegion 的
//     契约），本文件只原样存取，不解释含义。
//
// 不变量（每次状态变更后都维持）：
//   * active() == true 当且仅当最近一次 set_hover 之后未 clear；
//   * active() == false 时 payload() 恒为 0（悬停无对象即无 id）；
//   * options().magnify **原样保存**，不归一化——"<=1 或非有限视为不放大"
//     是 magnified_around 的判定规则，不是存储规则。
//
// 退化输入（明确不改状态）：
//   * set_hover 传入非有限坐标：视为无效悬停，**不改任何状态**
//     （此前的悬停保持有效，不会因一条坏事件被清掉）；
//   * magnified_around：magnify <= 1 / 非有限、visible 为空矩形、或当前未
//     悬停（无锚点即无放大中心）时，**原样返回 visible**。



namespace termchart::interaction {

struct HighlightOptions {
    // 放大倍数；<=1 或非有限视为不放大。
    double magnify = 2.0;
};

class Highlight {
public:
    explicit Highlight(HighlightOptions options = {}) noexcept;

    // 进入/移动悬停：payload 原样保存（0 也合法，由调用方自行约定含义）。
    TERMCHART_API void set_hover(std::uint32_t payload, Point data_pos) noexcept;

    // 离开悬停：回到未激活态，payload 归零。
    TERMCHART_API void clear() noexcept;

    [[nodiscard]] TERMCHART_API bool active() const noexcept;
    [[nodiscard]] TERMCHART_API std::uint32_t payload() const noexcept;
    [[nodiscard]] TERMCHART_API Point anchor() const noexcept;
    [[nodiscard]] TERMCHART_API const HighlightOptions& options() const noexcept;

    // 高亮放大的核心：以锚点为中心的可见矩形，跨度 = 原跨度 / magnify，
    // 锚点保持居中（锚点性质：缩放前后 anchor 在矩形内的归一化位置不变，
    // 严格为中心 0.5）。不夹取——本函数不知道 data_bounds，夹取是调用方的事。
    // magnify <= 1 / 非有限、visible 为空、或未悬停时**原样返回 visible**。
    [[nodiscard]] TERMCHART_API Rect magnified_around(Rect visible) const noexcept;

private:
    HighlightOptions options_{};
    Point anchor_{};
    std::uint32_t payload_ = 0;
    bool active_ = false;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/hit_test.hpp =====
// hit_test.hpp —— 命中测试（只做几何，禁图表名）
//
// 约束（ROADMAP 阶段 3）：本文件**不得出现任何图表类型名**。它只调用
// chart::HitRegion 的几何接口（形状判定封装在 HitRegion::contains 内），
// 因此新增图表类型不需要改这里——这正是"通用 HitRegion"的意义。



namespace termchart::interaction {

// 最上面（**最后**登记）的命中项；无命中返回 nullptr。
[[nodiscard]] TERMCHART_API const chart::HitRegion* topmost_hit(
    const std::vector<chart::HitRegion>& regions, double x, double y) noexcept;

// 全部命中项的 payload，自下而上（登记顺序）。
[[nodiscard]] TERMCHART_API std::vector<std::uint32_t> hit_payloads(
    const std::vector<chart::HitRegion>& regions, double x, double y);

}  // namespace termchart::interaction

// ===== include/termchart/interaction/input.hpp =====
// input.hpp —— 原始输入字节缓冲（攒字节，解析交给 InputParser）



namespace termchart::interaction {

class Input {
public:
    TERMCHART_API void push(std::string_view bytes);
    TERMCHART_API void push(char byte);

    [[nodiscard]] TERMCHART_API std::string_view pending() const noexcept;
    [[nodiscard]] TERMCHART_API bool empty() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;
    TERMCHART_API void clear();

private:
    std::string buffer_;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/key.hpp =====
// key.hpp —— 按键定义（header-only 值类型）


namespace termchart::interaction {

enum class Key {
    Unknown = 0,
    Char,       // 可打印字符，具体码点在 KeyEvent::ch
    Enter,
    Tab,
    Backspace,
    Escape,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

using ModMask = std::uint8_t;

namespace mod {
inline constexpr ModMask kNone = 0;
inline constexpr ModMask kShift = 1u << 0;
inline constexpr ModMask kAlt = 1u << 1;
inline constexpr ModMask kCtrl = 1u << 2;
}  // namespace mod

[[nodiscard]] constexpr bool has_mod(ModMask mask, ModMask bit) noexcept {
    return (mask & bit) != 0;
}

struct KeyEvent {
    Key key = Key::Unknown;
    char32_t ch = 0;             // key == Char 时有效
    ModMask mods = mod::kNone;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/legend_toggle.hpp =====
// legend_toggle.hpp —— 图例点击切换
//
// 职责切分：图例的**画**归渲染侧（chart/backend 想怎么画都行），本文件只管
// "点了图例之后系列可见性怎么变"。交互层不知道"系列"是什么——它只维护
// id → 可见性 的映射，而 id 的语义由登记方（把图例项的 HitRegion payload
// 设成 series id 的人）决定。这与 hit_test 的"不透明 payload"约定一致。
//
// 不变量：
//   * 未登记的 id 视为可见（visible() 返回 true）——这样新增系列不需要先登记；
//   * click() 只在 id 已登记且状态真的翻转时返回 true（供调用方决定要不要重绘）；
//   * hidden() 的顺序与登记顺序一致（渲染侧可以稳定地画"已隐藏"角标）。
//
// 退化输入：payload == 0 是 HitRegion 的默认值，**不**当作合法系列 id
// （登记时 id 为 0 会被拒绝），避免"点中了没登记的图元却切掉了某个系列"。



namespace termchart::interaction {

class LegendToggle {
public:
    // 登记一个系列（id 必须非 0；重复登记幂等，不更新 label）。
    // 返回是否真正登记（false = id 为 0 或已存在）。
    TERMCHART_API bool register_series(std::uint32_t id, std::string label);

    [[nodiscard]] TERMCHART_API std::size_t series_count() const noexcept;

    // 未登记的 id 视为可见。
    [[nodiscard]] TERMCHART_API bool visible(std::uint32_t id) const noexcept;
    TERMCHART_API void set_visible(std::uint32_t id, bool visible) noexcept;
    TERMCHART_API void toggle(std::uint32_t id) noexcept;

    TERMCHART_API void show_all() noexcept;
    TERMCHART_API void hide_all() noexcept;

    // 点击路由：payload 即 series id。已登记且发生翻转 → true；未登记 → false。
    [[nodiscard]] TERMCHART_API bool click(std::uint32_t payload) noexcept;

    // 便利路径：在图例的 HitRegion 集合里做命中测试，命中则切换其 payload 对应系列。
    // （图例项登记为 HitRegion::rect 且 payload = series id；0 payload 视为未登记。）
    [[nodiscard]] TERMCHART_API bool click_at(const std::vector<chart::HitRegion>& regions,
                                              double x, double y) noexcept;

    // 隐藏中的系列 id（按登记顺序）；全部可见时为空。
    [[nodiscard]] TERMCHART_API std::vector<std::uint32_t> hidden() const;

    // 未登记的 id 返回空串。
    [[nodiscard]] TERMCHART_API std::string label(std::uint32_t id) const;

private:
    struct Entry {
        std::uint32_t id = 0;
        std::string label;
        bool visible = true;
    };
    std::vector<Entry> entries_;   // 按登记顺序，天然支持 hidden() 的稳定顺序

    // 查找辅助：Entry 是私有嵌套类型，不能写在匿名 namespace 的自由函数里。
    static Entry* find(std::vector<Entry>& entries, std::uint32_t id) noexcept;
    static const Entry* find(const std::vector<Entry>& entries, std::uint32_t id) noexcept;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/mouse.hpp =====
// mouse.hpp —— 鼠标事件（header-only 值类型）


namespace termchart::interaction {

enum class MouseButton {
    None = 0,
    Left,
    Middle,
    Right,
};

enum class MouseAction {
    Move = 0,
    Press,
    Release,
    ScrollUp,
    ScrollDown,
};

struct MouseEvent {
    MouseAction action = MouseAction::Move;
    MouseButton button = MouseButton::None;
    int x = 0;  // 1-based 终端列
    int y = 0;  // 1-based 终端行
    ModMask mods = mod::kNone;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/event.hpp =====
// event.hpp —— 输入事件变体（header-only）
//
// Event 用 std::variant 表达"按键 / 鼠标 / 尺寸 / 焦点 / 粘贴"五类事件；
// variant 的 index() 即 EventKind，与 EventKind 的枚举顺序一一对应。



namespace termchart::interaction {

struct ResizeEvent {
    int cols = 0;
    int rows = 0;
};

struct FocusEvent {
    bool focused = true;
};

struct PasteEvent {
    std::string text;
};

enum class EventKind : std::uint8_t {
    Key = 0,
    Mouse,
    Resize,
    Focus,
    Paste,
};

using Event = std::variant<KeyEvent, MouseEvent, ResizeEvent, FocusEvent, PasteEvent>;

[[nodiscard]] constexpr EventKind kind_of(const Event& event) noexcept {
    return static_cast<EventKind>(event.index());
}

[[nodiscard]] constexpr const char* kind_name(EventKind kind) noexcept {
    switch (kind) {
        case EventKind::Key:
            return "key";
        case EventKind::Mouse:
            return "mouse";
        case EventKind::Resize:
            return "resize";
        case EventKind::Focus:
            return "focus";
        case EventKind::Paste:
            return "paste";
    }
    return "unknown";
}

}  // namespace termchart::interaction

// ===== include/termchart/interaction/dispatcher.hpp =====
// dispatcher.hpp —— 事件分发（订阅顺序即优先级，被消费即停）



namespace termchart::interaction {

class Dispatcher {
public:
    // 返回 true 表示"已消费"，分发立即停止（冒泡终止）。
    using Handler = std::function<bool(const Event&)>;

    // 订阅某一类事件。
    void subscribe(EventKind kind, Handler handler);
    // 订阅全部事件（作为兜底/全局观察者）。
    void subscribe_all(Handler handler);

    // 按订阅顺序调用匹配的 handler；返回是否被消费。
    TERMCHART_API bool dispatch(const Event& event);

    [[nodiscard]] TERMCHART_API std::size_t handler_count() const noexcept;
    void clear();

private:
    struct Entry {
        bool all = false;
        EventKind kind = EventKind::Key;
        Handler handler;
    };
    std::vector<Entry> entries_;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/input_parser.hpp =====
// input_parser.hpp —— 字节流状态机：终端输入字节 → Event
//
// 解析 ASCII、UTF-8、CSI（方向键/功能键/Home/End/PageUp…）、SS3（F1–F4）、
// SGR 鼠标（ESC [ < b ; x ; y M/m）、OSC（跳过）、Alt+字符、Ctrl+字母。
// 不完整序列留在内部缓冲，等后续字节——这是流式解析的必备性质（read() 可能
// 把一条序列切成两半）。
//
// 但缓冲是**有上限的**（`kMaxPendingBytes`）：积压超过上限说明收到的不是"还没收完
// 的一条序列"，而是垃圾或恶意输入（例如 `ESC ]` 之后跟着无穷多个字节，永远等不到
// OSC 终止符）。此时**丢弃缓冲并跳到下一个 ESC 重新同步**，避免内存无界增长，也避免
// 把被丢弃序列的中段当成按键产出伪事件。
//
// 代价要说清楚：超过上限的**合法**长序列也会被丢弃（例如 iTerm2 内联图像的 base64
// 负载本身就可能大于 256 KiB）。本库的解析器本来就不产出 OSC 内容事件，所以常规用法
// 不受影响；若你的应用需要接收这种超长 OSC，请在应用层单独处理，或调大这个常量。



namespace termchart::interaction {

class InputParser {
public:
    // 未完成序列的缓冲上限（256 KiB）。超限即丢弃缓冲重新同步，见文件头说明。
    static constexpr std::size_t kMaxPendingBytes = 256 * 1024;

    // 喂入字节，返回本次能解析出的全部**完整**事件。
    TERMCHART_API std::vector<Event> feed(std::string_view bytes);

    // 把孤立的尾随 ESC 收尾成 Escape 事件（无超时机制的解析器需要显式收尾）。
    TERMCHART_API std::vector<Event> flush();

    [[nodiscard]] TERMCHART_API std::size_t pending() const noexcept;
    TERMCHART_API void reset();

private:
    std::string buffer_;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/select.hpp =====
// select.hpp —— 框选状态机（按下 → 拖动 → 松开/取消，纯几何，不吃 Event）
//
// 职责切分（为什么单开一个文件）：
//   * "拖出一个正矩形并回报框内数据点的 id"与终端、渲染管线、具体图表类型
//     统统无关，只依赖 L0 的 Point/Rect 与不透明 payload id。
//   * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
//     begin/update/end/cancel 调用。
//
// 状态机（两态：空闲 / 拖拽中）：
//   * begin(start)      空闲 → 拖拽中；**拖拽中重复 begin 视为重新起笔**，
//                       起点重置、已积累的矩形作废；
//   * update(current)   仅拖拽中有效：更新当前点并使矩形可查询；
//                       空闲时是 **no-op**（防呆：不允许凭空"继续拖"）；
//   * end()             拖拽中 → 空闲；矩形**保留**供查询（松开后仍要高亮框内点）；
//                       空闲时是 no-op；
//   * cancel()          任何态 → 空闲并**清空矩形**（ESC 语义：什么都别选）。
//
// 不变量：
//   * has_rect() == true 当且仅当"拖拽中发生过至少一次 update"且此后未 cancel
//     （纯点击、只 begin 未拖动不算矩形——零面积矩形没有框选意义）；
//   * rect() 始终是归一化正矩形（x/y 为左上角、宽高非负），与 Rect 约定一致；
//   * active() == true 时 has_rect() 可以为 false（刚 begin 还没动）。
//
// 边界判定：payloads_inside 与 Rect::contains 的闭-开语义一致
//   （含左/上边界，不含右/下边界），从而与 HitRegion 的矩形命中可互推。



namespace termchart::interaction {

class BoxSelect {
public:
    BoxSelect() noexcept = default;

    // 按下：进入拖拽态并记起点；拖拽中再次调用 = 重新起笔（重置起点、作废旧矩形）。
    TERMCHART_API void begin(Point start) noexcept;

    // 拖动：更新当前点。未 begin 时 no-op。
    TERMCHART_API void update(Point current) noexcept;

    // 松开：退出拖拽态，矩形保留供查询。未 begin 时 no-op。
    TERMCHART_API void end() noexcept;

    // ESC 语义：退出拖拽态并清空矩形。
    TERMCHART_API void cancel() noexcept;

    [[nodiscard]] TERMCHART_API bool active() const noexcept;    // 正在拖拽
    [[nodiscard]] TERMCHART_API bool has_rect() const noexcept;  // 松开后矩形仍可查询

    // 归一化正矩形（任意方向拖拽都返回左上角 + 非负宽高）；无矩形返回 {}。
    [[nodiscard]] TERMCHART_API Rect rect() const noexcept;

    // 框内数据点：items = (数据坐标, payload)；返回框内 payload，**保持输入顺序**。
    // 边界判定与 Rect::contains 的矩形语义一致（含左/上边界，不含右/下）。
    // 无矩形（has_rect() == false）时返回空。
    [[nodiscard]] TERMCHART_API std::vector<std::uint32_t> payloads_inside(
        const std::vector<std::pair<Point, std::uint32_t>>& items) const;

private:
    Point start_{};
    Point current_{};
    bool dragging_ = false;
    bool has_rect_ = false;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/tooltip.hpp =====
// tooltip.hpp —— 悬停数据标签（带边框的小浮层）
//
// 职责：给定锚点格 + 文本，在画布上画一个带边框、可多行的浮层；并在 clear() 或
// 下一次 draw() 时**精确还原**被改写的格（否则悬浮一次就永久污染画布）。
//
// 定位规则（复用 layout/label_avoid 的既有思路/实现）：
//   * 候选顺序：右上 → 左上 → 右下 → 左下（角位，而非 label_avoid 的边位）；
//   * "可用"的判定**直接调用** layout::place_label_avoiding —— 把候选矩形反推成它
//     的"右"候选来做探测，因此重叠判定、gap、半开区间与越界规则完全复用，不另写一套；
//   * 锚点格恒计入占用集，故浮层**永不覆盖锚点**；
//   * 四角都不可用 → 返回 placed=false 且**不改画布**（对应"宽度不足以放任何浮层"
//     与"锚点在画布外"的退化输入），而不是硬塞。
//
// 宽度：按 util::display_width（宽字符算 2 列）计算；超过 max_width 时按显示宽度
// 截断并追加省略号（用 prefix_by_width，绝不切在组合记号中间），既有边框也不会被撑破。



namespace termchart::interaction {

struct TooltipOptions {
    Rect bounds{};                // 允许区域（通常 = 画布尺寸矩形）；w/h <= 0 表示该方向不限
    std::uint32_t max_width = 0;  // 内容最大显示宽（不含左右边框）；0 = 不限
    double gap = 1.0;             // 浮层与锚点之间的间距（格）
    char32_t border_h = U'─';
    char32_t border_v = U'│';
    char32_t corner_tl = U'┌';
    char32_t corner_tr = U'┐';
    char32_t corner_bl = U'└';
    char32_t corner_br = U'┘';
    char32_t ellipsis = U'…';
    Style box_style{};            // 内容与底板样式（浮层是不透明替换语义）
    Style border_style{};         // 边框样式
    util::WidthOptions width_options{};  // 宽度口径，与 util::display_width 一致
};

struct TooltipPlacement {
    Rect rect{};   // 浮层外框（含边框）的落点（格矩形）
    bool placed = false;
};

class Tooltip {
public:
    Tooltip() = default;
    explicit Tooltip(TooltipOptions options) noexcept;

    // 换配置。不主动碰画布：新配置在下一次 draw() 生效，而 draw() 会先还原旧浮层。
    TERMCHART_API void set_options(TooltipOptions options) noexcept;
    [[nodiscard]] TERMCHART_API const TooltipOptions& options() const noexcept;

    // 单行便捷：等价于 draw(canvas, anchor, {std::string(text)})。
    TERMCHART_API TooltipPlacement draw(canvas::Canvas& canvas, Point anchor,
                                        std::string_view text);
    // 多行：逐行按显示宽度截断（必要时加省略号），逐行补空格对齐右边框。
    // 退化（空行列表 / 行宽全 0 / 锚点在允许区域外 / 四角都放不下）→ placed=false
    // 且**不改画布**。重复调用先还原上一次浮层再重绘（幂等）。
    TERMCHART_API TooltipPlacement draw(canvas::Canvas& canvas, Point anchor,
                                        const std::vector<std::string>& lines);

    // 还原全部被改写的格；返回是否真的还原过。未绘制时为 no-op。
    TERMCHART_API bool clear(canvas::Canvas& canvas);

    [[nodiscard]] TERMCHART_API bool visible() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t patch_count() const noexcept;

private:
    struct Patch {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        Cell before{};
    };

    bool restore(canvas::Canvas& canvas);
    void capture(const canvas::Canvas& canvas, std::uint32_t x, std::uint32_t y);
    void reset_captured(std::uint32_t width, std::uint32_t height);

    TooltipOptions options_{};
    std::vector<Patch> patches_;
    std::vector<std::uint8_t> captured_;  // 尺寸 = captured_w_ * captured_h_
    std::uint32_t captured_w_ = 0;
    std::uint32_t captured_h_ = 0;
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/zoom.hpp =====
// zoom.hpp —— 视口（数据坐标里的可见矩形）与缩放
//
// 职责切分（为什么单开一个文件）：
//   * 图表的"缩放"本质是"在数据坐标里换一个更小/更大的可见矩形"，这与终端、
//     渲染管线、具体图表类型统统无关，故只依赖 L0 的 Point/Rect，谁都能用。
//   * 平移（含拖拽手势）另见 pan.hpp；本文件只负责"视口 + 缩放"这一半。
//
// 不变量（构造后与每次状态变更后都维持）：
//   * visible() 始终完全落在 data_bounds 内 —— 不会露出界外空白；
//   * 任一方向的可见跨度 ∈ [min(min_span, data 该方向跨度), data 该方向跨度]。
//     即 data_bounds 比 min_span 还窄时以 data_bounds 为准（数据界优先于最小跨度）。


namespace termchart::interaction {

// min_span 的缺省正下界：量纲由调用方的数据坐标决定，这里只保证"大于 0"。
inline constexpr double kDefaultMinSpan = 1e-9;

struct ViewportOptions {
    Rect data_bounds{};  // 数据坐标里允许被观察的完整范围（夹取上限）
    double min_span = kDefaultMinSpan;  // 任一方向允许的最小可见跨度
    bool keep_aspect = false;           // 缩放时是否严格保持可见矩形的宽高比
};

// 数据坐标 → 屏幕/视口的可见窗口模型。纯值语义、纯函数式状态变换：
// 无隐藏状态、无随机、无时间/终端依赖，便于单测与 GUI 复用。
class Viewport {
public:
    Viewport() noexcept = default;
    explicit Viewport(const ViewportOptions& options) noexcept;
    Viewport(Rect data_bounds, double min_span = kDefaultMinSpan,
             bool keep_aspect = false) noexcept;

    // 回到初始态：可见矩形 == data_bounds。
    TERMCHART_API void reset() noexcept;

    // 以 anchor（数据坐标）为不动点做"乘法"缩放：
    //   factor > 1 放大（可见跨度 = 原跨度 / factor，看得更细），factor < 1 缩小。
    // 锚点性质：缩放前后 anchor 在视口内的归一化位置不变（夹取不介入时严格成立）。
    // 夹取策略：先按 factor 求出新跨度并在 [min_span, data 跨度] 内夹取（两轴在
    //   keep_aspect=false 时独立夹取；=true 时用同一系数联合夹取以锁死宽高比）；
    //   再由 anchor 反推原点并夹进 data_bounds。一旦夹取介入，**贴边优先**，
    //   锚点性质让步（此时 anchor 的归一化位置可能改变，这是有意的取舍）。
    // 退化输入：factor 非有限或 <= 0、或 data_bounds 有零/负跨度时，**明确不改状态**。
    TERMCHART_API void zoom_at(double factor, Point anchor) noexcept;

    // 把可见矩形在数据坐标里平移 (dx_data, dy_data)：正值 = 窗口向右/下移动
    // （屏幕上内容相对左移）。结果夹在 data_bounds 内，越界时贴边、不回绕。
    // 退化输入：平移量非有限、或 data_bounds 有零/负跨度时，**明确不改状态**。
    TERMCHART_API void pan_by(double dx_data, double dy_data) noexcept;

    [[nodiscard]] TERMCHART_API Rect visible() const noexcept;
    [[nodiscard]] TERMCHART_API Rect data_bounds() const noexcept;
    [[nodiscard]] TERMCHART_API const ViewportOptions& options() const noexcept;

    // 数据坐标 → 视口内归一化位置 [0,1]（视口外的点会越界，便于上层判断"在不在范围内"）。
    // 某方向可见跨度非正时该方向恒返回 0.5（无定义区间，取中点避免除零）。
    [[nodiscard]] TERMCHART_API double normalized_x(double data_x) const noexcept;
    [[nodiscard]] TERMCHART_API double normalized_y(double data_y) const noexcept;

    // 相对整个数据界的放大倍数 = data 跨度 / 可见跨度。reset 后为 1，放大后 > 1。
    // 可见跨度非正时返回 1（无定义，取恒等）。
    [[nodiscard]] TERMCHART_API double scale_x() const noexcept;
    [[nodiscard]] TERMCHART_API double scale_y() const noexcept;

private:
    ViewportOptions options_{};
    Rect view_{};
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/pan.hpp =====
// pan.hpp —— 平移手势（纯几何拖拽状态机）
//
// 为什么与 zoom.hpp 分开：缩放是"换视口"的状态变换，拖拽是"把指针运动折算成位移"
// 的输入状态机，两者生命周期与依赖都不同（拖拽要记按下/释放）。
//
// 只吃几何量：本文件不碰终端、不碰渲染，也不做"终端格坐标 → 数据坐标"换算——
// 那步由上层用 Viewport 的映射函数完成。这样手势可在无终端的单测里完整驱动。
//
// 方向约定：拖拽采用"内容跟随指针"（像抓住地图拖动）。指针位移 (dx,dy) 对应视口
// 平移 (-dx,-dy)；drag_to() 返回**原始指针增量**，drag_into() 已封装这个取反。



namespace termchart::interaction {

// 按下 → 拖动 → 释放 的增量累积状态机。
class PanGesture {
public:
    // 按下：开始一次拖拽并记录起点。已在拖拽中再次按下会以新点重新开始。
    TERMCHART_API void press(Point at) noexcept;
    // 释放：结束拖拽。未在拖拽中调用是 no-op。
    TERMCHART_API void release() noexcept;
    [[nodiscard]] TERMCHART_API bool dragging() const noexcept;

    // 拖拽中：返回"自上次采样（或按下点）以来的**原始指针增量**"，并更新采样点。
    // 未拖拽：返回 std::nullopt —— 调用方必须忽略该次移动（视口保持不变）。
    [[nodiscard]] TERMCHART_API std::optional<Point> drag_to(Point at) noexcept;

    // 便捷：把本次拖拽按"内容跟随指针"折算后施加到视口（等价于 pan_by(-dx,-dy)）。
    // 返回是否消费了该次移动；未拖拽时返回 false 且**不动视口**。
    TERMCHART_API bool drag_into(Point at, Viewport& view) noexcept;

    [[nodiscard]] TERMCHART_API Point origin() const noexcept;   // 按下点
    [[nodiscard]] TERMCHART_API Point last() const noexcept;     // 最近采样点
    // 累计指针位移 = last - origin（各次增量之和），**不加到视口**，仅作状态查询。
    [[nodiscard]] TERMCHART_API Point pointer_displacement() const noexcept;

    // 结束拖拽并清空记录（不改变任何视口）。
    TERMCHART_API void reset() noexcept;

private:
    bool dragging_ = false;
    Point origin_{};
    Point last_{};
};

}  // namespace termchart::interaction

// ===== include/termchart/interaction/immediate.hpp =====
// immediate.hpp —— 即时模式绑定（Event 流 → 交互组件的最小路由层）
//
// 职责切分（为什么单开一个文件）：
//   * 只做路由与簿记：维护最近鼠标位置与按键按下集合，把 Event 流翻译成
//     Viewport（滚轮缩放 + 拖拽平移）与 BoxSelect（框选三段）的状态变更。
//     不持有终端、不绘制、不认识图表类型；组件状态都在调用方手里
//     （参照 Dear ImGui 的"每帧重放"：本类自身不沉淀任何图表状态）。
//   * 复用既有组件而非重写：平移语义交给 PanGesture（drag_into 的
//     "内容跟随指针"与未拖拽 no-op 被原样尊重），框选三段交给
//     BoxSelect（select.hpp，begin/update/end），本文件只补"按键归属
//     哪个手势"的判定。
//
// 滚轮策略（为什么用滚轮动作而不是 Key '+'/'-'）：
//   MouseEvent 没有 wheel 数值字段，但有 MouseAction::ScrollUp/ScrollDown
//   —— 它们就是滚轮事件本身，比 Key 事件 '+'/'-' 更贴近 wheel_step
//   "滚轮缩放因子"的语义，故采用滚轮动作：
//     * ScrollUp   → zoom_at(+wheel_step, 鼠标位置)   （放大，看得更细）
//     * ScrollDown → zoom_at(1/wheel_step, 鼠标位置)  （缩小，回看全局）
//
// 按键编号：以 mouse.hpp 的 MouseButton 为准（None=0/Left=1/Middle=2/
// Right=3）。bindings 的缺省值是刻意的：pan_button=1（左键拖拽平移），
// select_button=0（= None，即**缺省不启用框选**）——两者都缺省到左键会
// 让拖拽与框选互相打架，调用方要框选须显式给 select_button 分配按键
// （通常右键 3）。button == None 的 Press/Release 事件被整体忽略。
//
// 坐标系约定：feed() 不做"终端格坐标 → 数据坐标"换算，鼠标格坐标被原样
// 当作数据坐标喂给 Viewport/BoxSelect（与 pan.hpp 的纯几何约定一致）；
// 两者尺度不同时由调用方先行换算。
//
// 不变量：
//   * mouse() 是最近一次鼠标事件的坐标（未收到过为 {0,0}）；
//   * 按键按下集合只由 Press/Release 翻转，Move/Scroll 不改变按键状态；
//   * 非鼠标事件（Key/Resize/Focus/Paste）不改变任何状态。
//
// 退化输入：
//   * wheel_step 非有限或 <= 0：滚轮事件被整体忽略（不调用 zoom_at，
//     即使 zoom_at 自身也能拒绝这些值——双保险，见契约"非有限值不调用"）；
//   * 未按下对应按键时，Move 不驱动视口/框选（PanGesture::drag_to 返回
//     nullopt、BoxSelect::update no-op 的语义被尊重）。


namespace termchart::interaction {

struct ImmediateBindings {
    int pan_button = 1;        // 拖拽平移使用的鼠标键（MouseButton 编号）
    int select_button = 0;     // 框选使用的鼠标键；0 = None，缺省不启用框选
    double wheel_step = 1.25;  // 滚轮缩放因子（ScrollUp 放大 / ScrollDown 缩小）
};

class ImmediateContext {
public:
    explicit ImmediateContext(ImmediateBindings bindings = {}) noexcept;

    // 喂一个事件：内部维护鼠标位置/按键状态，并驱动 viewport（滚轮缩放 +
    // 拖拽平移）与 select（框选三段：按下/拖动/松开）。
    void feed(const Event& event, Viewport& viewport, BoxSelect& select);

    [[nodiscard]] Point mouse() const noexcept;  // 最近一次鼠标位置（未收到过为 {0,0}）
    [[nodiscard]] bool dragging() const noexcept;  // 平移拖拽进行中
    [[nodiscard]] const ImmediateBindings& bindings() const noexcept;

private:
    void handle_mouse(const MouseEvent& ev, Viewport& viewport, BoxSelect& select) noexcept;
    // 以当前鼠标位置为锚做一次滚轮缩放；factor 非有限或 <= 0 时忽略。
    void zoom_wheel(double factor, Viewport& viewport) noexcept;

    ImmediateBindings bindings_{};
    Point mouse_{};         // 最近鼠标位置
    unsigned pressed_ = 0;  // 按键按下集合，bit n = MouseButton 编号 n
    PanGesture pan_{};
};

}  // namespace termchart::interaction

// ===== include/termchart/backend/backend.hpp =====
// backend.hpp —— 所有后端的抽象基类（begin/end 生命周期）
//
// 后端的铁律（见 ROADMAP 阶段 3 约束）：**不做跨渲染模型转换**。
// 输入是 CellBuffer、Scene 还是 Document，由子接口在类型上钉死：
//   * backend/snapshot/*    ← CellBuffer
//   * backend/vector/*      ← Scene
//   * backend/structured/*  ← Document
// 因此 backend 不需要认识 chart 类型（backend > chart 是禁止边）。



namespace termchart::backend {

class Backend {
public:
    virtual ~Backend();

    // 生命周期：begin 清空内部缓冲，end 收口（补尾部字节）。二者可重入。
    virtual void begin() = 0;
    virtual void end() = 0;

    // 结果视图；调用方不得保存跨越下一次 begin/render 的视图。
    [[nodiscard]] virtual std::string_view bytes() const = 0;
};

}  // namespace termchart::backend

// ===== include/termchart/backend/snapshot/backend.hpp =====
// backend/snapshot/backend.hpp —— 快照物种子接口（CellBuffer → bytes）
//
// 快照导出是一次性的：无 diff、无 prev 状态、可重入。所有具体后端（ansi/text/
// html_pre/json）都只是把同一个 CellBuffer 用不同字节流表达。



namespace termchart::backend::snapshot {

struct SnapshotOptions {
    ColorDepth depth = ColorDepth::TrueColor;  // ansi 后端用；text/json 忽略
    bool trim_trailing_blank = true;           // 行尾"空白且无样式"裁掉
};

class SnapshotBackend : public Backend {
public:
    ~SnapshotBackend() override;

    void set_options(const SnapshotOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const SnapshotOptions& options() const noexcept { return opts_; }

    // 快照物种的输入恒为 CellBuffer。
    virtual void render(const CellBuffer& buffer) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    // 该行有效宽度：trim_trailing_blank 时裁掉行尾"空白且无样式"的格。
    [[nodiscard]] std::uint32_t row_width(const CellBuffer& buffer,
                                          std::uint32_t y) const noexcept;

    SnapshotOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_snapshot(SnapshotBackend& backend,
                                                        const CellBuffer& buffer);

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/snapshot/ansi_backend.hpp =====
// backend/snapshot/ansi_backend.hpp —— CellBuffer → 完整 ANSI 字节流
//
// **唯一**允许 include render/sgr 的 backend 文件（收窄边，见 TermchartModules.cmake）。
// 一次性、无 diff、无 prev 状态、可重入；不自建 SGR——序列化规则改动只在 sgr.hpp。


namespace termchart::backend::snapshot {

class AnsiBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/snapshot/html_pre_backend.hpp =====
// backend/snapshot/html_pre_backend.hpp —— CellBuffer → <pre> 包装的 HTML
//
// 只做最小转义（& < >），颜色/属性留白——需要彩色 HTML 用 backend/structured。


namespace termchart::backend::snapshot {

class HtmlPreBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/snapshot/json_backend.hpp =====
// backend/snapshot/json_backend.hpp —— CellBuffer + 属性 → 结构化 JSON
//
// 逐格序列化：码点、UTF-8 字符、前/背景（含 kind/index/rgb）、属性位掩码。
// 供机器消费（比如把终端产物喂给前端做 diff 比对）。


namespace termchart::backend::snapshot {

class JsonBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/snapshot/png_screenshot.hpp =====
// backend/snapshot/png_screenshot.hpp —— CellBuffer → PNG 截图
//
// 复用 image/png_encode 的 encode_png（零依赖 stored deflate），不重复实现
// PNG 编码。每个字符格映射为 cell_w×cell_h 的像素块：整块先填背景色
// （Style 未设色用 ResolveOptions 的默认背景），非空格再整块覆盖前景色。
// 前景是"块状示意"而非字形光栅化——逐字形渲染需要字体栅格化器，超出
// 本后端职责（要带字形请走 protocol/ 的 sixel/kitty 路径）。
//
// 注：继承 SnapshotOptions 里的 depth / trim_trailing_blank 对 PNG 无意义，
// 保留只是为了共用快照物种的生命周期与 render_snapshot 便捷入口。



namespace termchart::backend::snapshot {

class PngScreenshotBackend final : public SnapshotBackend {
public:
    // 每格像素尺寸；0 按 1 处理（0×0 只由空 buffer 的退化路径产生）。
    void set_cell_size(std::uint32_t w, std::uint32_t h) noexcept;
    [[nodiscard]] std::uint32_t cell_width() const noexcept { return cell_w_; }
    [[nodiscard]] std::uint32_t cell_height() const noexcept { return cell_h_; }

    void render(const CellBuffer& buffer) override;

private:
    std::uint32_t cell_w_ = 8;
    std::uint32_t cell_h_ = 16;
};

// 便捷入口：默认 8×16 每格，返回 PNG 字节串；空 buffer 返回空串。
[[nodiscard]] TERMCHART_API std::string render_png_screenshot(
    const CellBuffer& buffer, std::uint32_t cell_w = 8, std::uint32_t cell_h = 16);

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/snapshot/text_backend.hpp =====
// backend/snapshot/text_backend.hpp —— CellBuffer → 纯文本（无 SGR）


namespace termchart::backend::snapshot {

class TextBackend final : public SnapshotBackend {
public:
    void render(const CellBuffer& buffer) override;
};

}  // namespace termchart::backend::snapshot

// ===== include/termchart/backend/structured/backend.hpp =====
// backend/structured/backend.hpp —— 结构化物种子接口（Document → bytes）
//
// 结构化导出把 Document（语义块）翻成 HTML/Markdown 等"带结构"的文本格式。
// 输入是 Document 而非 chart：backend > chart 是禁止边，这就是它成立的原因。



namespace termchart::backend::structured {

struct StructuredOptions {
    std::string title;
    std::string css_class = "termchart";
};

class StructuredBackend : public Backend {
public:
    ~StructuredBackend() override;

    void set_options(const StructuredOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const StructuredOptions& options() const noexcept { return opts_; }

    // 结构化物种的输入恒为 Document。
    virtual void render(const document::Document& doc) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    StructuredOptions opts_;
    std::string out_;
};

[[nodiscard]] TERMCHART_API std::string render_structured(StructuredBackend& backend,
                                                          const document::Document& doc);

}  // namespace termchart::backend::structured

// ===== include/termchart/backend/structured/html_backend.hpp =====
// backend/structured/html_backend.hpp —— Document → HTML


namespace termchart::backend::structured {

class HtmlBackend final : public StructuredBackend {
public:
    void render(const document::Document& doc) override;
};

}  // namespace termchart::backend::structured

// ===== include/termchart/backend/structured/markdown_backend.hpp =====
// backend/structured/markdown_backend.hpp —— Document → Markdown


namespace termchart::backend::structured {

class MarkdownBackend final : public StructuredBackend {
public:
    void render(const document::Document& doc) override;
};

}  // namespace termchart::backend::structured

// ===== include/termchart/backend/vector/backend.hpp =====
// backend/vector/backend.hpp —— 矢量物种子接口（Scene → bytes）
//
// 矢量导出把 Scene（非完备图形 IR）翻成某种向量格式。与 snapshot 不同，这里
// 输入是**保留几何**的 Scene，因此能无损缩放（SVG viewBox / TikZ 坐标）。
// 后端不做跨渲染模型转换：Scene 进来，字节流出去。



namespace termchart::backend::vector {

struct VectorOptions {
    double scale = 12.0;   // 1 场景单位 → 多少输出单位（px / pt）
    bool autofit = true;   // 用 Scene::bounds() 自动定 viewBox
    double width = 640.0;  // autofit=false 时使用的画布尺寸
    double height = 480.0;
    // 文字的字号（场景单位）。0 表示沿用旧行为 = scale：scene 单位当作像素时
    // 恰好是 1px 字（历史金标如此）。以"格"为场景单位的图形（如 graph）要把
    // 它设成 1.0——字号是场景单位而不是输出单位，否则会放大 scale 倍。
    double font_size = 0.0;
};

class VectorBackend : public Backend {
public:
    ~VectorBackend() override;

    void set_options(const VectorOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const VectorOptions& options() const noexcept { return opts_; }

    // 矢量物种的输入恒为 Scene。
    virtual void render(const scene::Scene& scene) = 0;

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

protected:
    VectorOptions opts_;
    std::string out_;
};

[[nodiscard]] TERMCHART_API std::string render_vector(VectorBackend& backend,
                                                      const scene::Scene& scene);

}  // namespace termchart::backend::vector

// ===== include/termchart/backend/vector/latex_backend.hpp =====
// backend/vector/latex_backend.hpp —— CellBuffer → LaTeX（TikZ）
//
// 与 svg_backend 同款接口形态：Options 结构 + render 入口 + bytes()/str() 返回
// 字符串 + 便捷函数 render_latex（对齐 render_vector / render_snapshot）。
// 差异只在输入物种：按 backend/backend.hpp 的铁律"不做跨渲染模型转换"，
// 快照导出的输入恒为 CellBuffer，因此这里不继承 VectorBackend（它的 render
// 把输入钉死在 Scene），而是直接实现 Backend 生命周期。
//
// 映射口径：
//   * 坐标：格 (x, y) → TikZ (x, -y)；单位由 tikzpicture 的 x/y 选项定成
//     scale pt，y 翻转让第一行落在最上方（TikZ 坐标向上为正）。
//   * 非空格（空白格与宽字符续接格不输出）→ \node，文字经 LaTeX 转义。
//   * 横向连续的 U+2500（─）→ \draw 直线段（格中心连线）；
//     U+2588（█）→ \fill 实心矩形（左右各扩半格，视觉上无缝）。
//   * 颜色：Style 前景色经 color::resolve 解析成 RGB，用 xcolor 的
//     {rgb,255:red,R;green,G;blue,B} 模型内联；None 不发色键，沿用默认黑。



namespace termchart::backend::vector {

struct LatexOptions {
    double scale = 12.0;             // 1 格 → 多少 pt（tikzpicture 的 x/y 单位）
    bool standalone = true;          // 包 documentclass/document 还是裸 tikzpicture
    bool trim_trailing_blank = true; // 行尾"空白且无样式"的格不输出
};

class LatexBackend final : public Backend {
public:
    void set_options(const LatexOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const LatexOptions& options() const noexcept { return opts_; }

    void begin() override { out_.clear(); }
    void end() override {}
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

    // 快照物种的输入恒为 CellBuffer。
    void render(const CellBuffer& buffer);

private:
    // 该行有效宽度：trim 时裁掉行尾"空白且无样式"的格（语义同 SnapshotBackend）。
    [[nodiscard]] std::uint32_t row_width(const CellBuffer& buffer,
                                          std::uint32_t y) const noexcept;

    LatexOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_latex(LatexBackend& backend,
                                                     const CellBuffer& buffer);

}  // namespace termchart::backend::vector

// ===== include/termchart/backend/vector/pdf_backend.hpp =====
// backend/vector/pdf_backend.hpp —— Scene → PDF（极简 PDF 1.4）
//
// 设计目标：与 svg_backend 同款接口形态（Options + render + bytes()/str()）。
// 范围：
//   * Scene 的子集：MoveTo / LineTo / RectOp (fill+stroke) / FillOp / StrokeOp /
//     TextOp（仅 ASCII / Helvetica 字族，不嵌入字体）；
//   * 真彩 RGB（颜色类型 / PDF Color Operators rg/RG）；
//   * 内嵌 PNG（走 image::Bitmap → encode_png → /Filter /FlateDecode）；
//   * 单页（多页留给后续切片，初始只支持 1 页）；
//   * 文本流：仅 BaseFont / Helvetica（PDF 14 标准字体之一，零嵌入）。
//
// 故意不写：
//   * 圆弧 / 贝塞尔（PDF 支持但与 Scene 圆弧表示不一致，留 TODO）；
//   * 透明度 / 混合模式；
//   * 字体嵌入（用标准字体零成本，但 ASCII only）。
//
// 输出：完整 PDF 字节流（%PDF-1.4 + objects + xref + trailer），可在浏览器/Acrobat 打开。



namespace termchart::backend::vector {

struct PdfOptions {
    double width_pt  = 612.0;   // US Letter 宽（pt）
    double height_pt = 792.0;   // US Letter 高（pt）
    bool   compress  = true;    // 内容流走 zlib /FlateDecode（需 TERMCHART_HAVE_ZLIB）
};

class PdfBackend final : public VectorBackend {
public:
    void set_options(const PdfOptions& options) noexcept { opts_ = options; }
    [[nodiscard]] const PdfOptions& options() const noexcept { return opts_; }

    void begin() override { out_.clear(); }
    void end() override;
    [[nodiscard]] std::string_view bytes() const override { return out_; }
    [[nodiscard]] const std::string& str() const noexcept { return out_; }

    void render(const scene::Scene& scene) override;

private:
    PdfOptions opts_;
    std::string out_;
};

// 便捷：begin + render + end，返回字节流拷贝。
[[nodiscard]] TERMCHART_API std::string render_pdf(PdfBackend& backend,
                                                   const scene::Scene& scene);

}  // namespace termchart::backend::vector

// ===== include/termchart/backend/vector/svg_backend.hpp =====
// backend/vector/svg_backend.hpp —— Scene → SVG


namespace termchart::backend::vector {

class SvgBackend final : public VectorBackend {
public:
    void render(const scene::Scene& scene) override;
};

}  // namespace termchart::backend::vector

// ===== include/termchart/backend/vector/svg_entities.hpp =====
// svg_entities.hpp —— SVG 序列化原语（转义 / 颜色 / 数字格式）
//
// header-only：纯函数，无状态。颜色复用 color/ 的调色板换算（from_index_16 /
// from_index_256），因此 SVG 与终端两条路径对同一 Color 的观感一致。



namespace termchart::backend::vector {

// XML 文本转义（& < > " '）。
inline std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

// Rgb → "#RRGGBB"。
inline std::string rgb_hex(Rgb color) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.r, color.g, color.b);
    return buf;
}

// Color → SVG paint；None 表示"不画"（fill/stroke 用 "none"）。
inline std::string svg_color(const Color& color) {
    switch (color.kind()) {
        case ColorKind::None:
            return "none";
        case ColorKind::Ansi16:
            return rgb_hex(color::from_index_16(color.index()));
        case ColorKind::Indexed:
            return rgb_hex(color::from_index_256(color.index()));
        case ColorKind::Rgb:
            return rgb_hex(color.rgb());
    }
    return "none";
}

// 稳定数字格式：最多 3 位小数、去尾零（golden 需要确定性）。
inline std::string num(double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    std::string s(buf);
    if (s.find('.') != std::string::npos) {
        while (s.size() > 1 && s.back() == '0') {
            s.pop_back();
        }
        if (s.back() == '.') {
            s.pop_back();
        }
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

}  // namespace termchart::backend::vector

// ===== include/termchart/dsl/ast.hpp =====
// ast.hpp —— DSL 解析结果的中立模型（L17）
//
// 设计要点：AST **不依赖任何绘图类型**，但字段与 `graph::Graph` 一一对应，
// 于是"方言 → AST → Graph"是一条没有信息损失的单向路（见 to_graph）。
//
// 为什么不让方言直接吐 Graph：
//   1. 同一张 AST 可以被吐成图、表格、树三种产物（table_markdown / csv_sniff 也要用）；
//   2. 解析期的诊断需要带原文位置，而 Graph 里没有位置概念；
//   3. 后续 DSL（plantuml / d2）的差异可以只在 AST 上体现，下游零改动。



// 注意：这里 include graph/graph.hpp 而不是前向声明——`to_graph()` 的返回类型是
// `Result<graph::Graph>`，而 `Expected<T, E>` 内部持有 `std::variant<T, E>`，
// T 不完整就无法实例化（任何调用 to_graph 的 TU 都会撞上）。层号上 dsl(17) ≥
// graph(13)，这个方向本来就是允许的。

namespace termchart::dsl {

// 箭头语义。数值与 graph::ArrowStyle 对齐由 to_graph 负责，AST 自己不 include graph。
enum class ArrowKind : std::uint8_t {
    None = 0,      // ---
    AtTarget = 1,  // -->
    Both = 2,      // <-->
};

// 开放键值袋：方言里"不认识但已解析"的属性都落在这里，绝不静默丢弃。
struct AstAttribute {
    std::string key;
    std::string value;
};

struct AstNode {
    std::string key;      // 方言内的标识符（mermaid 的 A、dot 的 node_1）
    std::string label;    // 显示文本；方言没写标签时等于 key
    std::string shape;    // "box"/"round"/"stadium"/"diamond"/"hexagon"/"circle"/""（空 = 方言默认）
    std::vector<AstAttribute> attributes;
};

struct AstEdge {
    std::string from;                          // 引用 AstNode::key
    std::string to;
    std::string label;
    ArrowKind arrow = ArrowKind::AtTarget;
    std::string line_style;                    // ""(实线) / "dashed" / "thick"
    std::vector<AstAttribute> attributes;
};

struct AstGraph {
    std::string name;        // 图名（digraph G / flowchart 无名时为空）
    std::string directive;   // 原文指令，例如 "flowchart TD"、"digraph"
    std::string direction;   // "TD"/"LR"/... 或空；由渲染侧决定用哪个布局
    // 图级元信息：方言里"不属于任何节点"的属性（D2 的顶层 `label:`、rankdir 原文等）。
    // 放在图上而不是塞进第一个节点，避免凭空造出幽灵节点。
    std::vector<AstAttribute> attributes;
    std::vector<AstNode> nodes;
    std::vector<AstEdge> edges;

    // 两份重载（const / 非 const）：解析器要在找到的节点上继续填字段，
    // 历史上只能 `const_cast<AstNode*>(find_node(...))`，散在 8 处方言文件里。
    // 现在非 const 版本是第一等公民，const_cast 只保留在实现内部那一处。
    [[nodiscard]] TERMCHART_API const AstNode* find_node(std::string_view key) const noexcept;
    [[nodiscard]] TERMCHART_API AstNode* find_node(std::string_view key) noexcept;
    [[nodiscard]] TERMCHART_API bool has_node(std::string_view key) const noexcept;
    // 找不到就创建（解析期的常见动作：边端点可能先于声明出现）。
    TERMCHART_API AstNode& ensure_node(std::string_view key);
};

// AST → Graph 的**唯一**适配点。alias 与 style 类属性直接写进 Attributes。
// 失败只可能来自"边引用了不存在的节点"（解析器已保证不会发生，因此这是内部不变量）。
[[nodiscard]] TERMCHART_API Result<graph::Graph> to_graph(const AstGraph& ast);

}  // namespace termchart::dsl

// ===== include/termchart/dsl/dsl_registry.hpp =====
// dsl_registry.hpp —— 方言注册表（L17）
//
// 与 chart / raster / scale 注册表同构（docs/design/registry.md，五注册表统一模式）：
//   * `instance()` 用 **function-local static**，规避静态初始化顺序问题；
//   * 注册表不持有对象所有权，只持有**函数指针 + 字符串视图**（方言入口是纯函数）；
//   * 注册的唯一安全时机是 `App` 构造之前；`App` 构造时做浅拷贝快照并冻结；
//   * 内置方言由 `register_builtin_dsls()` 在静态初始化期登记（幂等，std::call_once 守卫）。
//
// 与另四个注册表的差别：方言入口签名是"纯函数"，因此描述符是 trivially copyable 的，
// 快照即一次 vector 拷贝——没有工厂对象生命周期问题。



namespace termchart::dsl {

struct DslOptions {
    std::size_t max_tokens = 200000;  // 词法上限（fuzz 输入可能极长）
    std::size_t max_nodes = 10000;    // 图规模上限：防止一句话吃光内存
};

// 解析入口指针：纯函数，无捕获、无状态。
using DslParseFn = Result<AstGraph> (*)(std::string_view source, const DslOptions& options);

struct DslDescriptor {
    std::string_view name;         // 注册名（"mermaid"、"flowchart"…），小写
    std::string_view description;  // 一行说明（给人看，也进 --help）
    std::string_view directive;    // 典型首行指令（"flowchart"、"digraph"…），识别用
    DslParseFn parse = nullptr;    // 非空是注册的有效性条件
};

class DslRegistry {
public:
    TERMCHART_API static DslRegistry& instance() noexcept;

    // 重名（不区分大小写）或 parse 为空 → 拒绝并返回 false。
    TERMCHART_API bool add(const DslDescriptor& descriptor) noexcept;

    // 未注册返回 nullptr。
    [[nodiscard]] TERMCHART_API const DslDescriptor* find(std::string_view name) const noexcept;

    // 未注册 → ErrorCode::Unsupported；已注册但解析失败 → 解析器给出的错误码。
    [[nodiscard]] TERMCHART_API Result<AstGraph> parse(std::string_view name,
                                                      std::string_view source,
                                                      const DslOptions& options = {}) const;

    // 注册名列表（按注册顺序，稳定）。
    [[nodiscard]] TERMCHART_API std::vector<std::string_view> names() const;

    // 快照语义：App 构造时调用一次，此后只读快照。
    [[nodiscard]] TERMCHART_API std::vector<DslDescriptor> snapshot() const;

    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;

    // 仅供测试：清空注册表（清空后 register_builtin_dsls 会重新登记）。
    TERMCHART_API void clear_for_test() noexcept;

private:
    DslRegistry() = default;
    std::vector<DslDescriptor> entries_;
};

// 幂等登记内置方言（mermaid 等）。首次调用发生在静态初始化期或首次解析前。
TERMCHART_API bool register_builtin_dsls() noexcept;

// 内置描述符表（定义在 dsl_registry.cpp；方言实现分散在自己的 TU 里）。
[[nodiscard]] TERMCHART_API std::vector<DslDescriptor> builtin_descriptors();

}  // namespace termchart::dsl

// ===== include/termchart/dsl/lexer.hpp =====
// lexer.hpp —— DSL 词法分析器（L17）
//
// 各 DSL（mermaid / dot / plantuml / d2）的**词法骨架是同一个**：
// 标识符、字符串、数字、箭头、标点、换行、注释。差异只有两处，都以参数表达：
//   * 关键字集合（方言自定义）；
//   * 注释前缀（mermaid "%%"、dot "//" 与 "#"、plantuml "'"）。
//
// 契约：
//   * **永不抛异常**；非法输入产出 TokenKind::Invalid 并带位置，由 parser 决定是否致命；
//   * 位置（行列）从 1 开始，用于诊断信息，不参与语义；
//   * 长度为 0 的 token 只可能是 End（防止调用方死循环）。



namespace termchart::dsl {

enum class TokenKind : std::uint8_t {
    End = 0,
    Identifier,    // A、node_1、foo.bar
    Keyword,       // 方言关键字（大小写不敏感匹配）
    Number,        // 42、3.14
    Text,          // 引号字符串（引号已去掉、转义已还原）
    Arrow,         // -->、---、-.->、==>、<-->、--
    Punctuation,   // [ ] ( ) { } | ; , : = 等单字符
    Newline,       // 语句分隔
    Comment,       // 仅 keep_comments 时产出
    Invalid,       // 非法输入（未闭合字符串、怪异字符）
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    std::size_t offset = 0;   // 相对源串的字节偏移
    std::size_t length = 0;
    std::size_t line = 1;     // 1-based
    std::size_t column = 1;   // 1-based（按字节，足够定位）

    [[nodiscard]] bool is(TokenKind expected) const noexcept { return kind == expected; }
    [[nodiscard]] bool is_punct(std::string_view expected) const noexcept {
        return kind == TokenKind::Punctuation && text == expected;
    }
};

// 关键字集合：大小写不敏感。用 span 指向方言语义里的静态表，不持有所有权。
class KeywordSet {
public:
    constexpr KeywordSet() noexcept = default;
    constexpr explicit KeywordSet(std::span<const std::string_view> words) noexcept
        : words_(words) {}

    [[nodiscard]] TERMCHART_API bool contains(std::string_view word) const noexcept;
    [[nodiscard]] constexpr std::size_t size() const noexcept { return words_.size(); }

private:
    std::span<const std::string_view> words_{};
};

struct LexerOptions {
    bool keep_comments = false;   // 解析器通常不需要注释
    bool keep_newlines = true;    // 只有"整篇重排"的场景才关掉
    // 注释前缀；默认按 mermaid。dot 用 {"//", "#"}，plantuml 用 {"'"}。
    std::vector<std::string_view> comment_prefixes{"%%"};
    std::size_t max_tokens = 200000;  // 防御性上限（fuzz 输入可能极长）
};

class Lexer {
public:
    TERMCHART_API Lexer(std::string_view source, KeywordSet keywords,
                        LexerOptions options = {});
    // 每次调用前进一个 token；到末尾后恒返回 End。
    TERMCHART_API Token next() noexcept;
    // 一次性取完（含结尾的 End），超过 max_tokens 返回 OutOfRange。
    TERMCHART_API Result<std::vector<Token>> tokenize() noexcept;

    [[nodiscard]] TERMCHART_API std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] TERMCHART_API std::string_view source() const noexcept { return source_; }

private:
    std::string_view source_;
    KeywordSet keywords_{};
    LexerOptions options_{};
    std::size_t offset_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

// 便捷入口：一次调用拿到全部 token。
TERMCHART_API Result<std::vector<Token>> tokenize(std::string_view source, KeywordSet keywords,
                                                  LexerOptions options = {});

}  // namespace termchart::dsl

// ===== include/termchart/dsl/parser_base.hpp =====
// parser_base.hpp —— 递归下降解析框架（L17）
//
// 所有方言共用同一套"读 token 的手感"：peek/advance/match/skip_line + 诊断记录。
// 设计取向（与 core/error.hpp 的错误策略一致）：
//   * 解析**不抛异常**：错误记录进 ParseDiagnostic，由方言入口转成 Result 的错误码；
//   * 只保留**第一个**错误（后续错误往往是它引发的连锁噪声）；
//   * 关键不变量：任何循环都必须保证 index 前进，否则 fuzz 输入会让解析器挂死。



namespace termchart::dsl {

struct ParseDiagnostic {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
};

class ParserBase {
public:
    TERMCHART_API ParserBase(std::vector<Token> tokens, std::string dialect,
                             std::string_view source = {});
    virtual ~ParserBase() = default;

    [[nodiscard]] TERMCHART_API const ParseDiagnostic& diagnostic() const noexcept;
    [[nodiscard]] TERMCHART_API bool failed() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t error_count() const noexcept;

protected:
    // ── 游标 ───────────────────────────────────────────────────────────
    [[nodiscard]] TERMCHART_API const Token& current() const noexcept;
    [[nodiscard]] TERMCHART_API const Token& peek(std::size_t ahead = 1) const noexcept;
    [[nodiscard]] TERMCHART_API bool at_end() const noexcept;
    TERMCHART_API Token advance() noexcept;
    TERMCHART_API void skip_newlines() noexcept;
    [[nodiscard]] TERMCHART_API std::size_t token_index() const noexcept;
    [[nodiscard]] TERMCHART_API std::size_t token_count() const noexcept;

    // ── 匹配（匹配成功才前进） ─────────────────────────────────────────
    [[nodiscard]] TERMCHART_API bool check_punct(std::string_view text) const noexcept;
    [[nodiscard]] TERMCHART_API bool check_keyword(std::string_view text) const noexcept;
    [[nodiscard]] TERMCHART_API bool check_kind(TokenKind kind) const noexcept;
    TERMCHART_API bool match_punct(std::string_view text) noexcept;
    TERMCHART_API bool match_keyword(std::string_view text) noexcept;
    TERMCHART_API bool match_identifier(std::string_view text) noexcept;
    TERMCHART_API bool match_kind(TokenKind kind) noexcept;

    [[nodiscard]] TERMCHART_API std::string_view previous_text() const noexcept;
    [[nodiscard]] TERMCHART_API const Token& previous_token() const noexcept;

    // ── 诊断 ───────────────────────────────────────────────────────────
    TERMCHART_API void report(ErrorCode code, std::string message) noexcept;
    TERMCHART_API void report_at(const Token& token, ErrorCode code,
                                 std::string message) noexcept;
    // 声称"到此结束"：若当前不是行尾/流尾，记为 ParseError（避免静默吞掉尾部）。
    TERMCHART_API bool expect_end_of_statement() noexcept;
    // 跳过本行剩余 token（用于"认得出但本切片不做"的指令）。
    TERMCHART_API void skip_to_end_of_line() noexcept;
    // 跳过一个配对的括号区（含嵌套），用于忽略整段指令。
    TERMCHART_API void skip_balanced(std::string_view open, std::string_view close) noexcept;

    [[nodiscard]] TERMCHART_API std::string_view source() const noexcept;
    [[nodiscard]] TERMCHART_API const std::string& dialect() const noexcept;

private:
    std::vector<Token> tokens_;
    std::size_t index_ = 0;
    std::size_t last_index_ = 0;
    ParseDiagnostic diagnostic_;
    std::size_t error_count_ = 0;
    std::string dialect_;
    std::string_view source_;
};

}  // namespace termchart::dsl

// ===== include/termchart/dsl/csv_sniff.hpp =====
// csv_sniff.hpp —— CSV 解析与列类型推断（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 自定义单字符 delimiter；表头可选（has_header=false 时列名为 col1..colN）
//   ✅ 引号字段 "..."：内含分隔符与换行；"" 表示一个引号；未闭合 → ParseError
//   ✅ 开头 UTF-8 BOM 跳过（首个列名不带 BOM）
//   ✅ 列类型推断：Boolean → Integer → Real → Date → Text；整列空 → Empty
//   ⛔ 认出但忽略：闭合引号后到分隔符/行尾的剩余字符按字面并入字段（宽松，不报错）；
//      引号字段内的 CR/LF 原样保留
//   ⛔ 本切片不支持：多字符分隔符、注释行、`sep=` 指令、反斜杠转义
//      （CSV 只认引号翻倍）、delimiter 之外的方言自动识别
//
// 行/列策略：完全空白的行被跳过；推断时缺失的列按"空"计。
// 类型判定顺序（先命中先定；列内所有非空样本须全部满足）：
//   Empty（无非空样本）→ Boolean → Integer → Real → Date → Text。
//   Boolean：true/false/yes/no/0/1（大小写不敏感）；
//   Integer：可选正负号 + 纯数字；Real：十进制 / 科学计数法；
//   Date：YYYY-MM-DD，可选 (空格|T) HH:MM(:SS)。
// 样本前先裁剪首尾空白；sniff_rows==0 表示用全部数据行，否则只用前 N 个数据行。
//
// 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic；
// 未闭合引号的位置指向**起始引号**。



namespace termchart::dsl {

enum class ColumnType : std::uint8_t {
    Empty = 0,
    Boolean,
    Integer,
    Real,
    Date,
    Text,
};

struct ColumnProfile {
    std::string name;
    ColumnType type = ColumnType::Empty;
    std::size_t filled = 0;   // 非空样本数
    std::size_t empty = 0;    // 空样本数
};

struct CsvOptions {
    char delimiter = ',';
    bool has_header = true;
    std::size_t sniff_rows = 100;    // 只用前 N 行做类型推断；0 = 全部行
    std::size_t max_rows = 10000;    // 数据行上限（不含表头）
};

// 解析 CSV 为 document::TableModel：has_header 时首行作表头，其余为数据行。
// 失败返回 ErrorCode::ParseError / OutOfRange / InvalidArgument。
[[nodiscard]] TERMCHART_API Result<document::TableModel> parse_csv(
    std::string_view source, const CsvOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 逐列推断类型，返回每列的 ColumnProfile（name/type/filled/empty）。
// 列数：has_header 时取表头列数，否则取所有行的最大列数。
[[nodiscard]] TERMCHART_API Result<std::vector<ColumnProfile>> sniff_columns(
    std::string_view source, const CsvOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::dsl

// ===== include/termchart/dsl/d2.hpp =====
// d2.hpp —— D2 语法（L17）
//
// ✅ 支持
//   `name: Type` 节点声明（`Type` 映射到图元形状，原值记进节点属性 `d2_shape`）
//   `name: { … }` 块：容器节点（属性 `container=true`），块内子节点记 `parent`
//   `label: 文字` / `shape: 圆` / `width|height|near|tooltip|icon|link|classes: v`
//       —— 作用于当前块（或顶层最近声明的节点）
//   `name.prop: value` 形式（同上，但显式指定节点）
//   连接：`a -> b`、`a <- b`、`a -- b`（无箭头）、链 `a -> b -> c`、`a -> b: 标签`
//   `direction: right|left|up|down` → AstGraph::direction（LR/RL/BT/TB）
// ⛔ 认出但忽略：`style: { … }` 块（连同其 `}`）、`#` 注释、未知键值对
// ⛔ 不支持：`...` 常量节点、泛型 `**a**` 强调语法、SQL/class/markdown 等嵌入块、
//   `a -> b: { label: x }` 这种"边上的属性块"（按普通边解析，标签取不到）
//
// 与 graphviz_dot 的关系：两者都是"文本 → 图"，但 D2 的**默认形状语义**不同
// （`name: Type` 是形状声明而不是属性），因此不复用 DOT 的解析器。



namespace termchart::dsl {

struct D2ParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_d2(std::string_view source,
                                                     const D2ParseOptions& options = {},
                                                     ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kD2Directive = "d2";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/graphviz_dot.hpp =====
// graphviz_dot.hpp —— Graphviz DOT 语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 头部 `digraph [名字] {` / `graph [名字] {`（`strict` 前缀忽略）；`}` 收尾（缺失 → ParseError）
//   ✅ 方向：`rankdir=LR|TB|BT|RL` / `rankdir="LR"`，以及属性行 `graph [rankdir=LR]`
//      （值统一转大写写进 AstGraph::direction；DOT 默认 TB，故未写时留空，由渲染侧取默认）
//   ✅ 节点：`a;`、`a [label="文字", shape=box]`、多条属性列表 `a [k=v][k2="v2"]`
//      （`label`→AstNode::label；`shape` 认 box/ellipse/diamond/hexagon/cylinder/round，
//        其余 shape 原样落进 AstNode::attributes，不静默丢弃）
//   ✅ 边：`a -> b`（有向）、`a -- b`（无向）、链 `a -> b -> c`、`a -> b [label="x"]`
//      （边属性 `style=dashed|dotted`→line_style="dashed"，`style=bold`→"thick"，
//        `dir=both`→ArrowKind::Both）
//   ✅ 并列节点集：`a -> {b c}`、`{a b} -> c`、`a -> {b c} -> d`（按笛卡尔积展开为多条边）
//   ⛔ 认出但**忽略**（不产生节点/边）：
//      `strict`；`graph/node/edge [attr]` 默认属性行（`graph` 行里的 rankdir 仍会被取用）；
//      `subgraph 名称 { … }` 块（块头与 `}` 被吞掉，块内语句按顶层继续解析，不建分组属性）；
//      `id = value` 形式的图属性赋值（仅 `rankdir` 有效，其余丢弃）
//   ⛔ **不支持**（不做或降级，且不假装支持）：
//      HTML 风格标签 `<…>`（会被当作标点拆散，含它的语句报 ParseError）；
//      端口 `a:port`（端口被跳过、port 名丢失）；一个文件里多张图（只解析第一张）；
//      `->` 之外带属性语义的复杂边（如 `dir=none` 之外的方向微调）
//
// 为什么这样切：DOT 的属性列表允许任意 k=v 且值可含空格/标点/中文，按 token 拼接极易出错，
// 因此属性一律用**原文切片**读取（`[` 到配对 `]` 之间），再在切片上按逗号/等号切分——
// 于是 `label="a -> b"` 这类"引号里长得像箭头"的内容天然不会被误解。
//
// 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
//       可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。



namespace termchart::dsl {

struct DotParseOptions {
    std::size_t max_nodes = 10000;  // 节点上限（超限返回 OutOfRange，不抛异常）
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_dot(
    std::string_view source, const DotParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 典型首行指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kDotDirective = "digraph";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/json.hpp =====
// json.hpp —— JSON 解析（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 完整 JSON 值：null / true / false / 数字 / 字符串 / 数组 / 对象，任意嵌套；
//   ✅ 数字：负号、小数、科学计数法（1e5、-2.5E-3）；按 double 存储（大整数
//      会丢精度，如实记录而非假装精确）；
//   ✅ 字符串转义：\" \\ \/ \b \f \n \r \t \uXXXX（含代理对 → UTF-8）；
//   ✅ 对象保持**插入序**（vector<pair> 而非 map），重复键后者覆盖前者；
//   ✅ 嵌套深度上限（默认 64）与输入长度上限（默认 16MB），超限返回错误而非
//      栈爆/内存爆；
//   ✅ 诊断：首个错误带行列与字节偏移（与 csv_sniff 同口径）。
//   ⛔ 不支持：注释、尾逗号、单引号字符串、NaN/Infinity、数字前导 + 号、
//      控制字符未转义直出（按 JSON 规范拒绝）。
//
// 错误：解析失败返回 ErrorCode::ParseError / OutOfRange / InvalidArgument，
// 可选地把**首个**诊断写到 out_diagnostic。



namespace termchart::dsl {

// JSON 值：带 tag 的结构体（递归容器用 vector 成员，C++17 起对不完整类型合法）。
// 不用 variant：array/object 递归嵌套时 variant 需要额外一层间接，可读性更差。
class JsonValue {
public:
    enum class Type : std::uint8_t {
        Null = 0,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    JsonValue() noexcept = default;  // Null

    // 工厂
    static JsonValue null() noexcept;
    static JsonValue boolean(bool value) noexcept;
    static JsonValue number(double value) noexcept;
    static JsonValue string(std::string value);
    static JsonValue array();
    static JsonValue object();

    // ── 类型判定 ──────────────────────────────────────────────────────────
    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool is_null() const noexcept { return type_ == Type::Null; }
    [[nodiscard]] bool is_bool() const noexcept { return type_ == Type::Bool; }
    [[nodiscard]] bool is_number() const noexcept { return type_ == Type::Number; }
    [[nodiscard]] bool is_string() const noexcept { return type_ == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type_ == Type::Object; }

    // ── 取值（类型不匹配返回 fallback，不抛异常）────────────────────────────
    [[nodiscard]] bool as_bool(bool fallback = false) const noexcept;
    [[nodiscard]] double as_number(double fallback = 0.0) const noexcept;
    [[nodiscard]] std::string_view as_string(std::string_view fallback = {}) const noexcept;

    // ── 数组访问 ──────────────────────────────────────────────────────────
    // size()：array/object 的元素数，其它类型为 0。
    [[nodiscard]] std::size_t size() const noexcept;
    // 越界返回 Null 单例（与"缺失即 null"的 JSON 语义一致，调用方无需判空）。
    [[nodiscard]] const JsonValue& at(std::size_t index) const noexcept;
    [[nodiscard]] const JsonValue& operator[](std::size_t index) const noexcept;

    // ── 对象访问 ──────────────────────────────────────────────────────────
    // 找不到返回 nullptr（与 at 的 Null 单例互补：需要区分"缺失"与"显式 null"时用）。
    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;
    // 找不到返回 Null 单例。
    [[nodiscard]] const JsonValue& at(std::string_view key) const noexcept;

    // ── 构建（程序化构造；解析产物不需要）──────────────────────────────────
    void push(JsonValue value);                    // 非 array 时忽略
    void set(std::string key, JsonValue value);    // 非 object 时忽略；同名键覆盖

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;                              // Type::Array
    std::vector<std::pair<std::string, JsonValue>> object_;     // Type::Object（保序）
};

struct JsonOptions {
    std::size_t max_depth = 64;        // 嵌套深度上限（数组/对象各算一层）
    std::size_t max_bytes = 16u * 1024u * 1024u;  // 输入长度上限
};

// 解析 JSON 文本为 JsonValue。顶层必须是完整值（对象/数组/标量均可），
// 尾部只允许空白。失败返回错误码，可选地把首个诊断写到 out_diagnostic。
[[nodiscard]] TERMCHART_API Result<JsonValue> parse_json(
    std::string_view source, const JsonOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::dsl

// ===== include/termchart/dsl/mermaid.hpp =====
// mermaid.hpp —— Mermaid 总入口（L17）
//
// 职责只有两件：
//   1. **识别**首行指令，判断这是哪种图（docs/guide/dsl_reference.md 的表格口径）；
//   2. 分发到对应方言解析器。
//
// 分发去处的产物类型不同，这是刻意的：
//   flowchart / classDiagram / stateDiagram → `AstGraph`（可 to_graph 落到 graph::Graph）
//   sequenceDiagram                        → **不在这里**：序列图是时间轴模型，
//     见 mermaid_sequence.hpp 的 `parse_sequence()`；本入口对它返回 `Unsupported`
//     并在诊断里说明原因，**绝不假装解析成功**。
//
// 自动识别规则（首个非空、非注释行）：
//   flowchart / graph   → Flowchart
//   sequenceDiagram     → Sequence
//   classDiagram        → Class
//   stateDiagram(-v2)   → State
//   其它                → Unknown（由调用方决定是报错还是继续探测别的方言）



namespace termchart::dsl {

enum class MermaidDiagram : std::uint8_t {
    Unknown = 0,
    Flowchart,
    Sequence,
    Class,
    State,
};

struct MermaidOptions {
    std::size_t max_nodes = 10000;
    bool allow_subgraph = true;
};

// 纯文本识别，不解析、不分配（可在探测循环里反复调用）。
[[nodiscard]] TERMCHART_API MermaidDiagram detect_diagram(std::string_view source) noexcept;
[[nodiscard]] TERMCHART_API const char* diagram_name(MermaidDiagram diagram) noexcept;

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_mermaid(
    std::string_view source, const MermaidOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

}  // namespace termchart::dsl

// ===== include/termchart/dsl/mermaid_class.hpp =====
// mermaid_class.hpp —— Mermaid classDiagram（L17）
//
// 类图是"图 + 节点内容"，因此产物仍是 `AstGraph`：
//   * 类 = 节点，类体与方法逐行原文进节点属性 `member`（多条）；
//   * 关系 = 边，**语义写进边属性 `relation`**：inheritance / composition /
//     aggregation / association / dependency / realization；
//   * 布局仍交给 `graph::layout`，渲染交给 `graph/diagrams`（本层不画东西）。
//
// ✅ 支持
//   `classDiagram` 头部；`direction LR`
//   `class 名称`、`class 名称 { … }`、`class 名称 as 别名`、`class 名称["显示名"]`
//   `A <|-- B`、`A --|> B`、`A *-- B`、`A o-- B`、`A --> B`、`A ..> B`、`A ..|> B`
//   `A --> B : 标签`、`A "1" --> "*" B`（多重度进边属性 `from_multiplicity` / `to_multiplicity`）
//   `A : +成员()`（单行成员补写）
//   `<<interface>>` / `<<abstract>>` 构造型 → 节点属性 `stereotype`
// ⛔ 认出但忽略：`note for`、`style`/`classDef`/`class` 着色语句、`namespace`/`package`（记 `namespace` 属性）
// ⛔ 不支持：泛型 `~T~` 的语义解释（原文保留在成员文本里）、关系方向的双箭头细节
//
// 方向语义（最容易搞反，测试锁死）：
//   `A <|-- B` → B 继承 A，边 from=B to=A（三角指向父类 A）
//   `A --|> B` → A 继承 B，边 from=A to=B



namespace termchart::dsl {

struct ClassParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_class_diagram(
    std::string_view source, const ClassParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kClassDiagramDirective = "classDiagram";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/mermaid_flow.hpp =====
// mermaid_flow.hpp —— Mermaid flowchart 语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 头部 `flowchart TD` / `graph LR` / `graph G { }`（图名与方向）
//   ✅ 节点：裸引用 `A`、`A[标签]`、`A(标签)`、`A([标签])`、`A((标签))`、
//      `A{标签}`、`A{{标签}}`、`A>标签]`
//   ✅ 边：`-->` `---` `-.->` `-.-` `==>` `===` `<-->` `--x` `--o`
//   ✅ 边标签两种写法：`A -->|文字| B` 与 `A -- 文字 --> B`
//   ✅ 边链：`A --> B --> C`
//   ✅ 子图：`subgraph 名称` … `end`（子图名记为节点属性 `group`，不做嵌套布局）
//   ⛔ 忽略但**不报错**的指令：`classDef` / `class` / `click` / `style` / `linkStyle` /
//      `direction`（后者的值仍会被记录到 AstGraph::direction）
//   ⛔ 本切片不支持的：节点 `:::class` 后缀、`&` 并列节点、`--o`/`--x` 的
//      方向语义细节（统一按 AtTarget 处理）
//
// 诊断：解析失败返回解析器的错误码，可选地把首个诊断写到 out_diagnostic。



namespace termchart::dsl {

struct FlowchartParseOptions {
    bool allow_subgraph = true;   // 关掉后 subgraph 视为语法错误（严格模式）
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_flowchart(
    std::string_view source, const FlowchartParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 方言描述符用到的典型指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kFlowchartDirective = "flowchart";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/mermaid_sequence.hpp =====
// mermaid_sequence.hpp —— Mermaid sequenceDiagram（L17）
//
// **为什么这里不是 AstGraph**：序列图的时间轴语义（消息顺序、激活、生命线）
// 无法用"节点 + 边"无损表达——硬塞进去会得到一个既不是图、也不能画序列图的
// 半成品。这与本库对"三物种后端不做跨渲染模型转换"的态度一致：
// 模型不匹配就承认，不硬套。
//
// 因此本方言导出**自己的类型** `AstSequence`，并且**不登记进 DslRegistry**
// （注册表的契约是 `Result<AstGraph>`，见 dsl_registry.hpp）。序列图渲染器落地时
// 直接消费 `AstSequence` 即可，不需要改动本层。
//
// ✅ 支持
//   `sequenceDiagram` 头部
//   `participant A as 别名` / `actor A` / `database A`
//   `A->>B: 文本`、`A-->>B: 文本`（虚线）、`A-)B`（开口）、`A-xB`（叉）、
//   `A->B` / `A-->B`（无箭头），后接 `+` / `-` 的激活简写
//   `activate A` / `deactivate A`（记到**上一条消息**的 `activate`/`deactivate`）
//   `Note left of A: 文本` / `Note right of A: …` / `Note over A,B: …`
//   `%%` 注释
// ⛔ 认出但忽略：`loop`/`alt`/`else`/`opt`/`par`/`end` 等块语句（不展开嵌套），
//   `autonumber`、`title`、`box`、`link`、`links`、`participant` 的样式参数
// ⛔ 不支持：`create`/`destroy` 参与者、`break`、递归激活栈的精确配对



namespace termchart::dsl {

enum class MessageKind : std::uint8_t {
    Solid = 0,     // ->
    Dotted = 1,    // -->
    Async = 2,     // ->>
    AsyncDotted = 3,  // -->>
    Open = 4,      // -)
    OpenDotted = 5,   // --)
    Cross = 6,     // -x
    CrossDotted = 7,  // --x
};

struct SequenceParticipant {
    std::string key;      // 原文标识符
    std::string label;    // 显示名（`as` 后，缺省等于 key）
    std::string kind;     // "participant" / "actor" / "database" / ...
    std::vector<AstAttribute> attributes;
};

struct SequenceMessage {
    std::string from;
    std::string to;
    std::string text;
    MessageKind kind = MessageKind::Solid;
    std::size_t order = 0;       // 0-based 出现次序（渲染时的时间轴）
    bool activate_target = false;
    bool deactivate_source = false;
};

struct SequenceNote {
    enum class Placement : std::uint8_t { Left, Right, Over };
    Placement placement = Placement::Over;
    std::vector<std::string> participants;   // 1~2 个；over 可跨两人
    std::string text;
};

struct AstSequence {
    std::string directive;      // `sequenceDiagram`
    std::string title;          // `title …`（若有）
    std::vector<SequenceParticipant> participants;
    std::vector<SequenceMessage> messages;
    std::vector<SequenceNote> notes;
    std::vector<AstAttribute> attributes;   // 未识别但已登记的元信息

    // 两份重载（const / 非 const）：理由同 AstGraph::find_node——非 const 版本是
    // 第一等公民，解析器要改的时候不必在各处 const_cast。
    [[nodiscard]] TERMCHART_API const SequenceParticipant* find_participant(
        std::string_view key) const noexcept;
    [[nodiscard]] TERMCHART_API SequenceParticipant* find_participant(
        std::string_view key) noexcept;
    [[nodiscard]] TERMCHART_API bool has_participant(std::string_view key) const noexcept;
    TERMCHART_API SequenceParticipant& ensure_participant(std::string_view key);
};

struct SequenceParseOptions {
    std::size_t max_participants = 1000;
    std::size_t max_messages = 100000;
};

[[nodiscard]] TERMCHART_API Result<AstSequence> parse_sequence(
    std::string_view source, const SequenceParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

[[nodiscard]] TERMCHART_API const char* message_kind_name(MessageKind kind) noexcept;

inline constexpr std::string_view kSequenceDirective = "sequenceDiagram";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/mermaid_state.hpp =====
// mermaid_state.hpp —— Mermaid stateDiagram（L17）
//
// 状态机天然是**图**（状态=节点、迁移=边），因此本方言的产物就是 `AstGraph`，
// 可以直接 `to_graph()` 落到 `graph::Graph` 复用分层布局与正交路由。
//
// ✅ 支持
//   `stateDiagram` / `stateDiagram-v2` 头部、`direction LR` 等方向
//   `A --> B`、`A --> B : 事件[守卫]/动作`、`A -- B`（等价于 -->）
//   `[*] --> A`（A 记为初始态，节点属性 `initial=true`）
//   `A --> [*]`（A 记为终态，节点属性 `final=true`）
//   `state X { … }` 组合态：X 记 `composite=true`，块内状态记 `parent=X`（不做嵌套布局）
//   `state X as 显示名`、`state "显示名" as X`
//   `note left of X : …` / `note right of X : …` → 节点属性 `note`
// ⛔ 认出但忽略（不影响图结构）：`note` 的其它形态、`classDef`/`class`/`style`/`click`
// ⛔ 不支持：`--` 上的并发区域（`--` 分行）、`<<choice>>`/`<<fork>>` 等构造型语义



namespace termchart::dsl {

struct StateParseOptions {
    std::size_t max_nodes = 10000;
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_state_diagram(
    std::string_view source, const StateParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

inline constexpr std::string_view kStateDiagramDirective = "stateDiagram";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/plantuml.hpp =====
// plantuml.hpp —— PlantUML 类图语法（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 包裹：`@startuml [名字]` … `@enduml`（任一端缺失 → ParseError）
//   ✅ 注释：`'` 行注释、单行 `/' … '/` 块注释（多行块注释见"不支持"）
//   ✅ 类声明：`class 名称`、`class 名称 as 别名`、`class 名称 { …体… }`，
//      另把 `interface` / `enum` / `abstract class` 当同类处理（都建一个节点）
//   ✅ 类体：`{` … `}` 内**每一行原文**落进该节点 attributes（键 "member"，可多条），
//      **不解析**成员语法（可见性/类型/方法一律原样保留）
//   ✅ 关系：`-->`、`--`、`--|>`、`<|--`、`..>`、`..|>`、`*--`、`o--`（另收 `--*`/`--o`/`<|..`）
//      —— 语义写进 AstEdge::attributes 键 "relation"：
//         inheritance / composition / aggregation / association / dependency；
//         虚线（`..`）写进 AstEdge::line_style="dashed"；箭头统一用 ArrowKind；
//         `A <|-- B` 这类"箭头在左端"的写法会**交换** from/to（B 继承 A ⇒ from=B,to=A）
//   ✅ 关系标签 `A --> B : label`（取 `:` 到行尾的原文）
//   ✅ 多重度 `A "1" --> "*" B`（写进 edge attributes 键 from_multiplicity / to_multiplicity）
//   ⛔ 认出但**忽略**（不改变图结构）：`note`（单行 `note … : …` 与块 `note …`/`end note`）、
//      `skinparam`、`title`/`caption`/`legend`、`package`/`namespace` 块头（块内语句仍按顶层解析）、
//      以及一切既不是类声明也不是已识别关系的行——**整行跳过、继续**
//   ⛔ **不支持**：多行 `/' … '/` 块注释（词法层按行注释处理，内部行会被当作未识别行）、
//      泛型 `Foo<T>`、构造型 `<<…>>`、`hide/show` 的过滤语义、组件/时序/活动等其他 PlantUML 图族
//
// 为什么宽松：PlantUML 方言极杂且大量语法与布局有关。这里只承诺"类图的关系骨架"，
// 其余一律不猜——**认不出就整行跳过**，绝不让未识别的行静默改变已解析出的节点/边。
//
// 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
//       可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。



namespace termchart::dsl {

struct PlantUmlParseOptions {
    std::size_t max_nodes = 10000;  // 节点上限（超限返回 OutOfRange，不抛异常）
};

[[nodiscard]] TERMCHART_API Result<AstGraph> parse_plantuml(
    std::string_view source, const PlantUmlParseOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr);

// 典型首行指令；供注册表/自动识别共享，避免两处硬编码字符串。
inline constexpr std::string_view kPlantUmlDirective = "@startuml";

}  // namespace termchart::dsl

// ===== include/termchart/dsl/table_markdown.hpp =====
// table_markdown.hpp —— GitHub-Flavored Markdown 管道表格（L17）
//
// 支持范围（**明确列出，避免"以为支持"**）：
//   ✅ 表头行 + 分隔行（`---` / `:---` / `---:` / `:---:`）+ 若干数据行
//   ✅ 行首尾的 `|` 可省；单元格前后空白被裁剪
//   ✅ 单元格内 `\|` 还原为字面 `|`；反引号代码段内的 `|` 不作列分隔
//   ✅ 对齐信息（None/Left/Center/Right）经 TableAlignment 出参返回
//   ⛔ 认出但忽略：除 `\|` 与 `\\` 外的反斜杠转义按原文保留，不做解释；
//      未闭合的反引号按普通文本处理（不报错）
//   ⛔ 本切片不支持：HTML `<table>`、单元格内换行、colspan/rowspan、嵌套表格
//
// 列数策略：**以表头列数为准**。数据行多出的列被裁剪、缺失的列补空串；
// 分隔行必须与表头列数一致，否则按解析错误处理（GFM 要求相等）。
// 表格在首个空行或首个不含 `|` 的行处结束。
//
// 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic。



namespace termchart::dsl {

// 列对齐：与 GFM 分隔行语义一一对应（None = 默认左对齐）。
struct TableAlignment {
    enum class Kind : std::uint8_t { None = 0, Left, Center, Right };
    std::vector<Kind> columns;
};

struct MarkdownTableOptions {
    bool require_separator = true;   // 缺分隔行（|---|---|）时视为错误（GFM 要求有）
    std::size_t max_rows = 10000;    // 数据行上限（不含表头与分隔行）
};

// 解析一张 GFM 管道表格：
//   * 成功返回 document::TableModel（表头 + 数据行，行已规范到表头列数）；
//   * 失败返回 ErrorCode::ParseError / OutOfRange，可选写出首个诊断；
//   * out_alignment 非空时写入每列对齐方式（长度 = 表头列数；无分隔行时全 None）。
[[nodiscard]] TERMCHART_API Result<document::TableModel> parse_markdown_table(
    std::string_view source, const MarkdownTableOptions& options = {},
    ParseDiagnostic* out_diagnostic = nullptr, TableAlignment* out_alignment = nullptr);

}  // namespace termchart::dsl

// ===== include/termchart/widget/reactive.hpp =====
// reactive.hpp —— 响应式值：变更通知（订阅/发布）
//
// 线程约束（选型）：**单线程，不加锁**。理由：
//   1) widget 树与它的 Observable 归属同一 UI 线程，锁没有保护对象；
//   2) 加锁只会给"回调里再 set 同一个值"的重入路径以虚假的安全感——
//      通知顺序与重入语义应该由本文件写明的契约管住，而不是靠锁掩盖。
//   跨线程推送：调用方自行把新值排到 UI 线程再 set()。
//
// 契约：
//   * 值未变（operator== 判等）不通知；通知顺序 = 订阅顺序；
//   * 回调里再 set 同一 Observable 是**允许**的：重入请求被挂起，
//     等本轮通知走完后再补一次判定与通知（重入多次取最后一次的值）；
//   * 通知遍历**快照**：回调中订阅/退订（含自毁）只影响下一次通知，
//     不会悬空、不会跳过本轮已在快照里的其他监听者。



namespace termchart::widget {

using ListenerId = std::uint64_t;

class ListenerBook;  // 与值类型无关的订阅簿记（类型擦除后），reactive.cpp 实现

// Subscription —— RAII 订阅句柄：析构/被覆盖/显式 release 即退订，幂等。
// 句柄以裸指针回指簿记，因此 Observable 不可拷贝/移动（见下）。
class Subscription {
public:
    TERMCHART_API Subscription() noexcept = default;
    TERMCHART_API ~Subscription();
    TERMCHART_API Subscription(Subscription&& other) noexcept;
    TERMCHART_API Subscription& operator=(Subscription&& other) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    [[nodiscard]] bool valid() const noexcept { return book_ != nullptr; }
    [[nodiscard]] ListenerId id() const noexcept { return id_; }

    // 显式退订；未订阅时调用是 no-op（幂等）。
    TERMCHART_API void release() noexcept;
    TERMCHART_API void swap(Subscription& other) noexcept;

private:
    friend class ListenerBook;
    TERMCHART_API Subscription(ListenerBook* book, ListenerId id) noexcept;

    ListenerBook* book_ = nullptr;
    ListenerId id_ = 0;
};

// 订阅簿记：只管 (id, 回调) 的增删与快照遍历，不认识值类型。
class ListenerBook {
public:
    // 类型擦除后的回调：拿到 new/old 两个值的只读指针。
    using ErasedFn = std::function<void(const void* new_value, const void* old_value)>;

    TERMCHART_API ListenerBook() = default;
    TERMCHART_API ~ListenerBook();
    ListenerBook(const ListenerBook&) = delete;
    ListenerBook& operator=(const ListenerBook&) = delete;

    [[nodiscard]] TERMCHART_API Subscription add(ErasedFn fn);
    TERMCHART_API bool remove(ListenerId id) noexcept;
    [[nodiscard]] TERMCHART_API std::size_t size() const noexcept;

    // 快照遍历：先拷贝当前订阅表再逐个调用（回调中的增删不影响本轮）。
    TERMCHART_API void notify(const void* new_value, const void* old_value) const;

private:
    friend class Subscription;
    std::vector<std::pair<ListenerId, ErasedFn>> listeners_;
    ListenerId next_id_ = 1;
};

// Observable<T> —— 可观察值。T 需要可拷贝构造 + operator==。
// 不可拷贝/不可移动：Subscription 以裸指针回指内部的 ListenerBook，
// 挪动 Observable 会让在场句柄悬空——这是契约，不是实现偷懒。
template <typename T>
class Observable {
public:
    using Listener = std::function<void(const T& new_value, const T& old_value)>;

    Observable() = default;
    explicit Observable(T initial) : value_(std::move(initial)) {}

    Observable(const Observable&) = delete;
    Observable& operator=(const Observable&) = delete;
    Observable(Observable&&) = delete;
    Observable& operator=(Observable&&) = delete;

    [[nodiscard]] Subscription subscribe(Listener listener) {
        return book_.add([listener = std::move(listener)](const void* nv, const void* ov) {
            listener(*static_cast<const T*>(nv), *static_cast<const T*>(ov));
        });
    }

    // 见文件头"契约"。重入 set 在通知期间只登记挂起值（最后写入者胜），
    // 本轮走完后按同一套"值未变不通知"规则补发。
    void set(T value) {
        if (notifying_) {
            pending_ = std::move(value);
            return;
        }
        if (value == value_) {
            return;
        }
        T old = value_;
        value_ = std::move(value);
        notifying_ = true;
        book_.notify(&value_, &old);
        notifying_ = false;
        if (pending_) {
            T next = std::move(*pending_);
            pending_.reset();
            set(std::move(next));
        }
    }

    [[nodiscard]] const T& get() const noexcept { return value_; }
    [[nodiscard]] std::size_t listener_count() const noexcept { return book_.size(); }

private:
    T value_{};
    ListenerBook book_;
    std::optional<T> pending_;  // 通知期间的重入 set（最后写入者胜）
    bool notifying_ = false;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/widget.hpp =====
// widget.hpp —— widget 模块主头：保留式控件基类 + 主题色板值类型
//
// 模块定位（结构.md 二十二，层号 L18 见 TermchartModules.cmake）：
//   * Widget 是控件树的最小基类：身份（id）+ 几何（bounds，格坐标，半开区间）
//     + 状态旗标（可见/可用/可聚焦/已聚焦）+ 可覆写的命中测试。
//     它**不定义事件系统**——事件路由归 interaction/，无状态组件见
//     immediate_widget.hpp（即时模式），响应式值见 reactive.hpp。
//   * ThemePalette 是值语义色板：theme_dark/light/mono 各提供一张
//     inline constexpr 表；不可变、无全局状态，可按值传递、可编译期取用。



namespace termchart::widget {

// 主题色板：纯值聚合，语义槽位固定。success/danger 在单色主题下与前景同色
// （亮度无法编码语义，形态差异由组件字符承担，如 checkbox 的 'x'/' '）。
struct ThemePalette {
    Rgb background;    // 面板/控件底色
    Rgb foreground;    // 常规文字
    Rgb border;        // 边框、括号等结构字符
    Rgb accent;        // 强调：聚焦、选中标记、按压反白
    Rgb text_dim;      // 次要/禁用文字
    Rgb success;       // 语义：成功
    Rgb danger;        // 语义：危险
    Rgb highlight_bg;  // 悬停/选区底色

    constexpr bool operator==(const ThemePalette&) const noexcept = default;
    constexpr bool operator!=(const ThemePalette&) const noexcept = default;
};

// 便捷 Style 构造。保持 constexpr：主题表 + 这两个函数可在编译期拼出常量样式。
[[nodiscard]] constexpr Style style_fg(Rgb fg) noexcept {
    Style s{};
    s.set_fg(Color::rgb(fg));
    return s;
}
[[nodiscard]] constexpr Style style_fg_bg(Rgb fg, Rgb bg) noexcept {
    Style s{};
    s.set_fg(Color::rgb(fg));
    s.set_bg(Color::rgb(bg));
    return s;
}

// 控件基类。以身份参与控件树与命中测试：禁止拷贝/移动（父子指针与
// 焦点链都按地址走）。几何与 interaction::immediate 的坐标约定一致——
// 本层不做"格坐标 → 数据坐标"换算，鼠标格坐标原样进入 hit_test。
class Widget {
public:
    explicit Widget(std::string id);
    virtual ~Widget();

    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(Widget&&) = delete;

    [[nodiscard]] const std::string& id() const noexcept;

    // bounds 为格坐标矩形（core::Rect 半开区间）；负坐标与空矩形都合法，
    // 命中测试自然会拒绝它们。
    [[nodiscard]] const Rect& bounds() const noexcept;
    void set_bounds(Rect bounds) noexcept;

    [[nodiscard]] bool visible() const noexcept;
    void set_visible(bool on) noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    void set_enabled(bool on) noexcept;

    // set_focus 只在 focusable 时生效：对不可聚焦控件调 set_focus(true) 是
    // no-op 而不是错误（调用方不必先查询）。
    [[nodiscard]] bool focusable() const noexcept;
    void set_focusable(bool on) noexcept;
    [[nodiscard]] bool focused() const noexcept;
    void set_focus(bool on) noexcept;

    // 命中测试：默认 = visible && enabled && bounds.contains。
    // 子类可覆写（圆形按钮、镂空区、整树代理转发等）；覆写者自行决定
    // 是否沿用旗标判定。
    [[nodiscard]] virtual bool hit_test(Point p) const noexcept;

    // 整树重排钩子：默认空实现（叶子控件无需重排）。容器覆写为"先排自己、
    // 再递归子容器"，从而一次 relayout() 拍平整棵控件树。Widget 基类不定义
    // layout（那是容器的事），所以这里只提供安全的递归落点。
    virtual void relayout() noexcept {}

private:
    std::string id_;
    Rect bounds_{};
    bool visible_ = true;
    bool enabled_ = true;
    bool focusable_ = true;
    bool focused_ = false;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/border.hpp =====
// border.hpp —— 即时模式边框绘制（header-only，无状态）
//
// 与 immediate_widget.hpp 同风格：不持有状态，调用方传入 canvas + rect +
// 主题，每次调用重描完整边框。box-drawing 字符取自 Unicode，配色用
// ThemePalette.border。矩形不足 2 格宽/高时退化为单边（不崩、不画四角）。


namespace termchart::widget {

// 边框线型。None 表示不画（调用方自行决定）。
enum class BorderStyle {
    None,
    Single,   // ┌─┐ │ └┘
    Double,   // ╔═╗ ║ ╚╝
    Rounded,  // ╭─╮ │ ╰╯
};

// 用 box-drawing 字符把 rect 描边到 canvas（即时风格，与 immediate_widget 一致）。
// rect 宽高需 >= 2 才能画出四角；不足时退化为单边。
inline void draw_border(canvas::Canvas& c, Rect rect, const ThemePalette& theme,
                        BorderStyle style = BorderStyle::Single) noexcept {
    if (style == BorderStyle::None) {
        return;
    }
    const Style s = style_fg(theme.border);
    const char32_t h = (style == BorderStyle::Double) ? U'═' : U'─';
    const char32_t v = (style == BorderStyle::Double) ? U'║' : U'│';
    const char32_t tl = (style == BorderStyle::Double) ? U'╔'
                      : (style == BorderStyle::Rounded) ? U'╭' : U'┌';
    const char32_t tr = (style == BorderStyle::Double) ? U'╗'
                      : (style == BorderStyle::Rounded) ? U'╮' : U'┐';
    const char32_t bl = (style == BorderStyle::Double) ? U'╚'
                      : (style == BorderStyle::Rounded) ? U'╰' : U'└';
    const char32_t br = (style == BorderStyle::Double) ? U'╝'
                      : (style == BorderStyle::Rounded) ? U'╯' : U'┘';

    const double x0 = rect.x;
    const double y0 = rect.y;
    const double x1 = rect.x + rect.w - 1.0;
    const double y1 = rect.y + rect.h - 1.0;

    // 四角
    c.put(Point{x0, y0}, tl, s);
    c.put(Point{x1, y0}, tr, s);
    c.put(Point{x0, y1}, bl, s);
    c.put(Point{x1, y1}, br, s);

    // 上/下横线
    for (double x = x0 + 1.0; x < x1; ++x) {
        c.put(Point{x, y0}, h, s);
        c.put(Point{x, y1}, h, s);
    }
    // 左/右竖线
    for (double y = y0 + 1.0; y < y1; ++y) {
        c.put(Point{x0, y}, v, s);
        c.put(Point{x1, y}, v, s);
    }
}

}  // namespace termchart::widget

// ===== include/termchart/widget/container.hpp =====
// container.hpp —— 容器基类：控件树中枢（保留模式几何 + 命中测试）
//
// 与 widget.hpp 的设计一致：Widget 只管身份/几何/命中测试，绘制走 immediate_widget
// 的即时函数。Container 在此基础上持有子控件（裸指针，生命周期由调用方管理），
// 提供递归命中测试与统一的 layout 契约。它是布局控件的公共基类。



namespace termchart::widget {

// 子控件相对容器主轴的对齐方式（交叉轴同理）。
enum class Alignment {
    Start,    // 靠起始边
    Center,   // 居中
    End,      // 靠结束边
    Stretch,  // 拉伸填满交叉轴
};

// 容器基类。持有子控件并提供递归命中测试与 layout 契约。
class Container : public Widget {
public:
    explicit Container(std::string id);
    ~Container() override;

    // 加入子控件。引用版本存地址，指针版本直接存；空指针或重复添加被忽略。
    void add(Widget& child);
    void add(Widget* child);

    // 移除：成功返回 true；不在树中返回 false（no-op，不报错）。
    [[nodiscard]] bool remove(Widget& child) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::span<Widget* const> children() const noexcept { return children_; }

    // 递归命中测试：任一可见子控件命中即命中；否则回退到自身 bounds。
    [[nodiscard]] bool hit_test(Point p) const noexcept override;

    // layout 契约（纯虚）：依据自身 bounds() 重排子控件几何。基类 Widget 不定义
    // layout（那是容器的事），所以这里声明一个新的纯虚函数，子类用 override 覆写。
    virtual void layout() = 0;

    // 整树重排：先排自己，再递归子容器（叶子控件是 Widget::relayout 的空实现）。
    void relayout() noexcept;

    void set_dirty(bool on = true) noexcept { dirty_ = on; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

protected:
    std::vector<Widget*> children_;
    bool dirty_ = true;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/dock.hpp =====
// dock.hpp —— 五区停靠布局（top/bottom/left/right/center）



namespace termchart::widget {

// 停靠区域。处理顺序固定为 Top→Bottom→Left→Right→Center，从而嵌套出正确的
// 剩余矩形（中心区拿到被四边切剩的部分）。
enum class DockRegion {
    Top,
    Bottom,
    Left,
    Right,
    Center,
};

// 停靠布局：边栏按自身尺寸切走对应长条，中心区占据剩余矩形。典型用法是每区一个
// 子控件（多为布局容器）；同区多个子控件会重叠，由调用方嵌套布局容器避免。
class Dock : public Container {
public:
    explicit Dock(std::string id);

    void add(DockRegion region, Widget& child);
    void add(DockRegion region, Widget* child);

    [[nodiscard]] std::span<const std::pair<DockRegion, Widget*>> docked() const noexcept {
        return docked_;
    }

    void layout() override;

private:
    std::vector<std::pair<DockRegion, Widget*>> docked_;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/grid_layout.hpp =====
// grid_layout.hpp —— 网格布局（子控件按行列均分单元格）



namespace termchart::widget {

// 网格布局：把内容区均分为 columns × rows 的单元格，按加入顺序先行后列填入。
// 每个子控件在其单元格内按对齐方式定位（Stretch 填满单元格）。
class GridLayout : public Container {
public:
    explicit GridLayout(std::string id);

    void set_columns(std::size_t cols) noexcept { columns_ = cols ? cols : 1; set_dirty(); }
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    void set_spacing(double s) noexcept { spacing_ = s; set_dirty(); }
    [[nodiscard]] double spacing() const noexcept { return spacing_; }
    void set_padding(double p) noexcept { padding_ = p; set_dirty(); }
    [[nodiscard]] double padding() const noexcept { return padding_; }
    void set_alignment(Alignment a) noexcept { align_ = a; set_dirty(); }
    [[nodiscard]] Alignment alignment() const noexcept { return align_; }

    void layout() override;

private:
    std::size_t columns_ = 1;
    double spacing_ = 0.0;
    double padding_ = 0.0;
    Alignment align_ = Alignment::Center;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/hbox.hpp =====
// hbox.hpp —— 水平盒子布局（子控件自左向右排列，主轴 = x）


namespace termchart::widget {

// 水平盒子：子控件按加入顺序自左向右排，主轴为 x，交叉轴为 y。
// 子控件宽度取自其当前 bounds().w；高度按对齐方式决定（Stretch 填满内容高）。
class HBox : public Container {
public:
    explicit HBox(std::string id);

    void set_spacing(double s) noexcept { spacing_ = s; set_dirty(); }
    [[nodiscard]] double spacing() const noexcept { return spacing_; }
    void set_padding(double p) noexcept { padding_ = p; set_dirty(); }
    [[nodiscard]] double padding() const noexcept { return padding_; }
    void set_alignment(Alignment a) noexcept { align_ = a; set_dirty(); }
    [[nodiscard]] Alignment alignment() const noexcept { return align_; }

    void layout() override;

private:
    double spacing_ = 0.0;
    double padding_ = 0.0;
    Alignment align_ = Alignment::Stretch;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/immediate_widget.hpp =====
// immediate_widget.hpp —— 无状态即时组件：按钮 / 复选框
//
// 组件模型（即时模式，参照 interaction/immediate.hpp 的路由思路但不依赖终端）：
//   * **不持有任何状态**：输入旗标（hovered/pressed/...）由调用方每帧从自己的
//     事件流推导后传入，组件每次调用都重画完整外观；
//   * **返回值是"意图"不是"沿"**：hovered && pressed && enabled 即报告
//     clicked/toggled。不做按下→松开的沿检测——那需要记忆，属于状态，
//     调用方比对自己上一帧收到的返回值即可得到沿；
//   * **事件与几何正交**：返回意图只由输入旗标决定（调用方负责用同一个
//     rect 做命中测试），几何只决定绘制——退化几何不绘制、不崩、不影响
//     返回值。这样组件没有"画不出来就算没点中"的隐藏耦合。
//
// 标签按 UTF-8 码点截断（宽度按主码点数估，宽字符按 1 格计；需要精确
// East Asian 宽度时调用方应自行截断后再传入）。



namespace termchart::widget {

// 调用方每帧推导的输入旗标。组件不保存它，传完即忘。
struct ControlState {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool enabled = true;

    constexpr bool operator==(const ControlState&) const noexcept = default;
    constexpr bool operator!=(const ControlState&) const noexcept = default;
};

// 按钮：填充 face 色 + 居中标签。配色映射：
//   禁用 → text_dim / background；按压(hovered+pressed) → background / accent 反白；
//   悬停 → foreground / highlight_bg；仅聚焦 → accent 文字加粗；常态 → foreground。
// 返回：本轮是否报告点击意图（见文件头注）。
[[nodiscard]] TERMCHART_API bool button_clicked(canvas::Canvas& canvas, Rect rect,
                                                std::string_view label,
                                                const ThemePalette& theme,
                                                const ControlState& state) noexcept;

// 复选框：`[x]`/`[ ]` + 标签，单行顶对齐；标记用 accent（选中）/border（未选中）。
// 返回：本轮是否报告切换意图（调用方据此翻转自己持有的 checked，下一帧传入）。
[[nodiscard]] TERMCHART_API bool checkbox_toggled(canvas::Canvas& canvas, Rect rect,
                                                  std::string_view label, bool checked,
                                                  const ThemePalette& theme,
                                                  const ControlState& state) noexcept;

}  // namespace termchart::widget

// ===== include/termchart/widget/controls.hpp =====
// controls.hpp —— 无状态即时组件：下拉框 / 单行文本框 / 单选组
//
// 模型与 immediate_widget.hpp（按钮/复选框）完全一致：
//   * **不持有状态**：开合、高亮项、光标、选中项全部由调用方持有，每帧传入；
//   * **返回"意图"不返回"沿"**：hovered+pressed 即报告点击/提交；调用方比对
//     自己上一帧收到的返回值即可得到沿；
//   * **事件与几何正交**：退化几何只跳过绘制，不影响返回值；
//   * 文本一律按 UTF-8 码点操作（光标按码点移动、退格删整个码点），
//     宽度按主码点数估（宽字符按 1 格计）。



namespace termchart::widget {

// “无选中 / 无命中”哨兵：std::size_t 没有负值，用全 1 表示（调用方判 != kNoSelection）。
constexpr std::size_t kNoSelection = static_cast<std::size_t>(-1);

// ── 下拉框 ────────────────────────────────────────────────────────────────

enum class DropdownIntent : std::uint8_t {
    None,
    Toggle,    // 折叠↔展开（点到折叠行）
    MoveUp,    // 展开态高亮上移一项
    MoveDown,  // 展开态高亮下移一项
    Commit,    // 展开态确认（回车 / 点到某行）
    Cancel,    // 展开态取消（Esc）
};

// 本轮输入。has_click + click 单元格坐标（格坐标、向下取整）由调用方从
// 自己的事件流转来；键盘类输入用 move_delta / commit / cancel。
struct DropdownInput {
    bool has_click = false;
    std::size_t click_x = 0;
    std::size_t click_y = 0;
    int move_delta = 0;   // 高亮移动量：正下一项、负上一项（键盘或滚轮）
    bool commit = false;  // 回车
    bool cancel = false;  // Esc
};

struct DropdownResult {
    DropdownIntent intent = DropdownIntent::None;
    std::size_t index = 0;  // MoveUp/MoveDown：夹取后的新高亮；Commit：被确认项
};

// 绘制 + 意图。展开态从第 1 行起列候选项（第 0 行始终是折叠行），高亮行
// accent 反白、选中项前缀 "• "；候选项超过可视行数时开窗保证高亮可见。
// 折叠行画 "[ 当前值 ]"（括号 accent，选中值前景色）。
[[nodiscard]] TERMCHART_API DropdownResult dropdown(
    canvas::Canvas& canvas, Rect rect, const std::vector<std::string>& items,
    std::size_t selected, bool open, std::size_t active, const ThemePalette& theme,
    const ControlState& state, const DropdownInput& input = {}) noexcept;

// 展开态命中查询：输入格坐标（row/col 向下取整），返回**可视行内序号
// （0 起）**；行 0（折叠行）、清单外、已折叠（请调用方先看 open）返回
// kNoSelection。候选项多于可视行时该值是相对窗口首项的偏移——绝对下标由
// dropdown() 内部统一加窗口偏移，调用方做 hover 粗判可直接用本值。
[[nodiscard]] TERMCHART_API std::size_t dropdown_row_at(Rect rect,
                                                        std::size_t item_count,
                                                        std::uint32_t row,
                                                        std::uint32_t col) noexcept;

// ── 单行文本框 ────────────────────────────────────────────────────────────

struct TextBoxInput {
    std::size_t caret = 0;       // 光标字节偏移（调用方持有并逐帧回传）
    int move_delta = 0;          // 光标按码点移动（±1）
    int erase_back = 0;          // 退格次数（删光标前一个码点）
    int erase_forward = 0;       // Delete 次数（删光标处一个码点）
    std::string_view inserted;   // 本轮插入文本（控制字符被丢弃）
    bool submit = false;         // 回车
};

struct TextBoxResult {
    bool changed = false;     // 文本被改动
    bool submitted = false;   // 本轮回车
    std::size_t caret = 0;    // 编辑后的光标字节偏移（调用方下一帧回传）
};

// 编辑 text（原地修改）并绘制 "[ text ]" 单行。文本超宽时开窗滚动保证光标
// 可见，光标格画 Reverse 空格。disabled 时仍绘制，但不接受输入。
[[nodiscard]] TERMCHART_API TextBoxResult textbox(canvas::Canvas& canvas, Rect rect,
                                                  std::string& text,
                                                  const ThemePalette& theme,
                                                  const ControlState& state,
                                                  const TextBoxInput& input = {}) noexcept;

// ── 单选组 ────────────────────────────────────────────────────────────────

// 互斥项组：第 i 项画第 i 行（行高 1），选中项 " (•) label"、其余 " ( ) label"。
// hovered_row 由调用方用 radio_row_at 推导；pressed/enabled 语义同按钮。
// 返回本轮被点击项下标；无点击返回 kNoSelection。
[[nodiscard]] TERMCHART_API std::size_t radio_group_clicked(
    canvas::Canvas& canvas, Rect rect, const std::vector<std::string>& items,
    std::size_t selected, std::size_t hovered_row, bool pressed, bool enabled,
    bool focused, const ThemePalette& theme) noexcept;

// 命中行查询：p 落在第 i 项行内返回 i；越界/负坐标返回 kNoSelection。
[[nodiscard]] TERMCHART_API std::size_t radio_row_at(Rect rect, std::size_t item_count,
                                                    Point p) noexcept;

}  // namespace termchart::widget

// ===== include/termchart/widget/panel.hpp =====
// panel.hpp —— 带边框的容器（Container + Border）
//
// Panel 在 Container 之上加一层边框：content_rect() 给出减去边框厚度后的内容区，
// 子控件应布局在其中（典型做法是内部再放一个 VBox/HBox）。draw_border() 用
// 即时风格把边框描到 canvas（与 immediate_widget 一致）。Panel 不做子控件打包，
// layout() 仅把子控件整体铺进内容区——嵌套布局容器即可获得真实排版。


namespace termchart::widget {

class Panel : public Container {
public:
    explicit Panel(std::string id);

    void set_border_style(BorderStyle s) noexcept { border_ = s; set_dirty(); }
    [[nodiscard]] BorderStyle border_style() const noexcept { return border_; }

    // 内容区：bounds 减去边框厚度（None=0，Single=1，Double=2）。
    [[nodiscard]] Rect content_rect() const noexcept;

    // 即时绘制边框（配合 immediate_widget 的使用方式）。
    void draw_border(canvas::Canvas& c, const ThemePalette& theme) const noexcept {
        termchart::widget::draw_border(c, bounds(), theme, border_);
    }

    // 把子控件整体铺进内容区（不打包）。Container::relayout 会再递归子容器。
    void layout() override;

private:
    BorderStyle border_ = BorderStyle::Single;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/scroll.hpp =====
// scroll.hpp —— 可滚动容器（带视口偏移，裁剪由调用方负责）
//
// ScrollArea 持有一个"内容坐标空间"，子控件按各自 bounds 留在内容空间里，
// layout() 把每个子控件按其坐标减去 offset 摆到视口（= bounds）坐标系。offset
// 会被夹在 [0, content - viewport] 内（内容小于视口则不滚）。真正的视口裁剪
// （只显示 bounds 覆盖的格）由调用方负责——例如借 Compositor 按偏移合成，或
// 在绘制前对 canvas 做范围判断。



namespace termchart::widget {

class ScrollArea : public Container {
public:
    explicit ScrollArea(std::string id);

    void set_content_size(double w, double h) noexcept {
        content_w_ = w;
        content_h_ = h;
        set_dirty();
    }
    [[nodiscard]] std::pair<double, double> content_size() const noexcept {
        return {content_w_, content_h_};
    }

    void set_offset(double x, double y) noexcept;
    [[nodiscard]] std::pair<double, double> offset() const noexcept {
        return {off_x_, off_y_};
    }
    void scroll_by(double dx, double dy) noexcept { set_offset(off_x_ + dx, off_y_ + dy); }

    // 视口 = 容器自身 bounds。
    [[nodiscard]] Rect viewport() const noexcept { return bounds(); }

    void layout() override;

private:
    double content_w_ = 0.0;
    double content_h_ = 0.0;
    double off_x_ = 0.0;
    double off_y_ = 0.0;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/styles.hpp =====
// styles.hpp —— 控件样式值类型 + 基于 ThemePalette 的样式构造（header-only）
//
// WidgetStyle 是一组按交互状态分槽的 Style（normal/hover/active/disabled/
// focused）。它与绘制解耦：即时组件（immediate_widget）或应用层把 WidgetStyle
// 按当前 ControlState 选一份 Style 后交给 canvas。全部 constexpr/inline，无全局
// 状态，可按值传递。


namespace termchart::widget {

// 交互状态槽位键。与 immediate_widget::ControlState 正交——这里只描述"该状态
// 长什么样"，不描述"怎么进入该状态"。
enum class WidgetState {
    Normal,
    Hover,
    Active,
    Disabled,
    Focused,
};

// 一套控件的五态样式。未显式设置的槽位保持默认 Style（沿用终端默认色）。
struct WidgetStyle {
    Style normal;
    Style hover;
    Style active;
    Style disabled;
    Style focused;
};

// 按钮样式：常态浅字深底，悬停高亮底，按压反白，禁用淡字，聚焦强调字。
[[nodiscard]] inline WidgetStyle button_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal   = style_fg_bg(t.foreground, t.background);
    s.hover    = style_fg_bg(t.foreground, t.highlight_bg);
    s.active   = style_fg_bg(t.background, t.accent);
    s.disabled = style_fg_bg(t.text_dim, t.background);
    s.focused  = style_fg_bg(t.accent, t.background);
    return s;
}

// 面板样式：背景填充 + 常态/聚焦两态（边框由 draw_border 单独负责）。
[[nodiscard]] inline WidgetStyle panel_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal  = style_fg_bg(t.foreground, t.background);
    s.focused = style_fg_bg(t.accent, t.background);
    return s;
}

// 标签样式：常规前景色，禁用淡色，聚焦强调色。
[[nodiscard]] inline WidgetStyle label_style(const ThemePalette& t) noexcept {
    WidgetStyle s;
    s.normal   = style_fg(t.foreground);
    s.disabled = style_fg(t.text_dim);
    s.focused  = style_fg(t.accent);
    return s;
}

// 按状态取样式槽；未知状态回落 normal。
[[nodiscard]] inline const Style& style_for(const WidgetStyle& ws,
                                            WidgetState st) noexcept {
    switch (st) {
        case WidgetState::Hover:
            return ws.hover;
        case WidgetState::Active:
            return ws.active;
        case WidgetState::Disabled:
            return ws.disabled;
        case WidgetState::Focused:
            return ws.focused;
        case WidgetState::Normal:
        default:
            return ws.normal;
    }
}

}  // namespace termchart::widget

// ===== include/termchart/widget/style_sheet.hpp =====
// style_sheet.hpp —— 类名 → WidgetStyle 映射（header-only，无全局状态）
//
// 即时模式下没有"控件持有自己样式"的说法，样式按类名查表取用：调用方给控件
// 一个 class 名（如 "primary-button"），渲染时从 StyleSheet 取对应 WidgetStyle。
// 与 CSS 的类选择器同构，但只做精确名匹配（不含继承/层叠）。



namespace termchart::widget {

class StyleSheet {
public:
    // 注册/覆盖一个类样式。
    void add_class(std::string name, WidgetStyle style) {
        classes_.insert_or_assign(std::move(name), std::move(style));
    }

    // 查表；找不到返回 nullptr（调用方据此回落默认样式）。
    [[nodiscard]] const WidgetStyle* find(std::string_view name) const noexcept {
        auto it = classes_.find(std::string(name));
        return it == classes_.end() ? nullptr : &it->second;
    }

    // 取样式：找不到返回 fallback（默认空样式）。
    [[nodiscard]] WidgetStyle style_of(std::string_view name,
                                       const WidgetStyle& fallback = {}) const {
        auto it = classes_.find(std::string(name));
        return it == classes_.end() ? fallback : it->second;
    }

    // 把某类样式整体赋给 into（覆盖）。
    void merge_into(std::string_view name, WidgetStyle& into) const {
        if (const WidgetStyle* p = find(name)) {
            into = *p;
        }
    }

    [[nodiscard]] bool empty() const noexcept { return classes_.empty(); }

private:
    std::unordered_map<std::string, WidgetStyle> classes_;
};

}  // namespace termchart::widget

// ===== include/termchart/widget/theme_dark.hpp =====
// theme_dark.hpp —— 深色主题：值语义色板常量表（header-only，不可变）
//
// 无全局状态、无注册表：要用就按值取用这张 constexpr 表。色值取向是
// 深底浅字的终端默认观感（背景接近纯黑、强调色取终端常见的蓝）。


namespace termchart::widget {

// clang-format off
inline constexpr ThemePalette kDarkTheme{
    .background   = Rgb{16, 18, 24},
    .foreground   = Rgb{220, 224, 232},
    .border       = Rgb{70, 76, 90},
    .accent       = Rgb{86, 156, 214},
    .text_dim     = Rgb{130, 136, 150},
    .success      = Rgb{106, 190, 120},
    .danger       = Rgb{224, 98, 98},
    .highlight_bg = Rgb{38, 79, 120},
};
// clang-format on

}  // namespace termchart::widget

// ===== include/termchart/widget/theme_light.hpp =====
// theme_light.hpp —— 浅色主题：值语义色板常量表（header-only，不可变）
//
// 与 theme_dark 同构，槽位一致；背景亮、文字暗。浅底下的"高亮底色"
// 用淡蓝而不是加深前景，保证与 background 的对比方向一致。


namespace termchart::widget {

// clang-format off
inline constexpr ThemePalette kLightTheme{
    .background   = Rgb{246, 247, 249},
    .foreground   = Rgb{30, 32, 38},
    .border       = Rgb{200, 204, 214},
    .accent       = Rgb{0, 102, 204},
    .text_dim     = Rgb{110, 116, 128},
    .success      = Rgb{40, 140, 80},
    .danger       = Rgb{200, 50, 50},
    .highlight_bg = Rgb{205, 225, 250},
};
// clang-format on

}  // namespace termchart::widget

// ===== include/termchart/widget/theme_mono.hpp =====
// theme_mono.hpp —— 单色主题：值语义色板常量表（header-only，不可变）
//
// 只用灰阶（r==g==b），给无真彩/无 16 色降级路径与黑白终端用。
// success/danger 与前景同白：亮度编码不了语义，语义差异由组件自己的
// 字符形态承担（如 checkbox 的 'x'/' '、按钮的按压反白）。


namespace termchart::widget {

// clang-format off
inline constexpr ThemePalette kMonoTheme{
    .background   = Rgb{0, 0, 0},
    .foreground   = Rgb{255, 255, 255},
    .border       = Rgb{128, 128, 128},
    .accent       = Rgb{255, 255, 255},
    .text_dim     = Rgb{160, 160, 160},
    .success      = Rgb{255, 255, 255},
    .danger       = Rgb{255, 255, 255},
    .highlight_bg = Rgb{64, 64, 64},
};
// clang-format on

}  // namespace termchart::widget

// ===== include/termchart/widget/theme.hpp =====
// theme.hpp —— 主题聚合 + 按名取用（header-only）
//
// Theme 把名字与 ThemePalette 绑在一起，theme_by_name() 提供按名取用的薄入口
// （dark/light/mono），语义色板常量来自 theme_dark/light/mono。无注册表、无全局
// 状态——要扩展主题就再加一张 constexpr 表并在这个 switch 里加一行。



namespace termchart::widget {

// 主题 = 名字 + 色板。
struct Theme {
    std::string name;
    ThemePalette palette;
};

// 按名取主题；未知名回落 dark。
[[nodiscard]] inline Theme theme_by_name(std::string_view name) noexcept {
    if (name == "light") {
        return Theme{"light", kLightTheme};
    }
    if (name == "mono") {
        return Theme{"mono", kMonoTheme};
    }
    return Theme{"dark", kDarkTheme};
}

}  // namespace termchart::widget

// ===== include/termchart/widget/vbox.hpp =====
// vbox.hpp —— 垂直盒子布局（子控件自上而下堆叠，主轴 = y）


namespace termchart::widget {

// 垂直盒子：子控件按加入顺序自上而下排，主轴为 y，交叉轴为 x。
// 子控件的高度取自其当前 bounds().h（调用方应先设好期望高度）；宽度按
// 对齐方式决定（Stretch 填满内容宽，其余保留自身宽度）。
class VBox : public Container {
public:
    explicit VBox(std::string id);

    void set_spacing(double s) noexcept { spacing_ = s; set_dirty(); }
    [[nodiscard]] double spacing() const noexcept { return spacing_; }
    void set_padding(double p) noexcept { padding_ = p; set_dirty(); }
    [[nodiscard]] double padding() const noexcept { return padding_; }
    void set_alignment(Alignment a) noexcept { align_ = a; set_dirty(); }
    [[nodiscard]] Alignment alignment() const noexcept { return align_; }

    void layout() override;

private:
    double spacing_ = 0.0;
    double padding_ = 0.0;
    Alignment align_ = Alignment::Stretch;
};

}  // namespace termchart::widget

// ===== include/termchart/plugin/manifest.hpp =====
// manifest.hpp —— 插件元数据
//
// 纯值类型：可拷贝、可比较。注册表快照（App 构造期冻结）就是一次深拷贝，
// 因此 Manifest 不持有任何句柄或引用。



namespace termchart::plugin {

// name 是注册键（非空，比较不区分大小写）；version / author / description
// 是给人看的（日志、--list-plugins），允许为空。
struct Manifest {
    std::string name;
    std::string version = "0.0.0";
    std::string author;
    std::string description;
};

// 最小约束：name 非空即合法。version 等字段不做格式校验——宽进严出，
// 格式问题是展示层的事，不该在加载路径上拦人。
[[nodiscard]] TERMCHART_API bool manifest_valid(const Manifest& manifest) noexcept;

// 单行人读描述："name@version — description"。
[[nodiscard]] TERMCHART_API std::string describe(const Manifest& manifest);

}  // namespace termchart::plugin

// ===== include/termchart/plugin/dynamic.hpp =====
// dynamic.hpp —— 动态库加载（可选能力）
//
// 平台差异全部落在 src/plugin/dynamic.cpp（公开头里没有 #ifdef，与 term/ 的
// 平台隔离约定一致）。本模块是**可选能力**：静态插件与注册表路径不依赖它，
// 在禁用动态加载的环境下照常工作。
//
// 入口点约定：动态库导出
//   extern "C" const termchart::plugin::Manifest* termchart_plugin_entry();
// 返回**静态存储期**的 Manifest（不得为 nullptr）；宿主据其 name/version
// 决定是否加载。Manifest 指向宿主不会修改的静态数据，因此指针语义安全。



namespace termchart::plugin {

class DynamicLibrary {
public:
    DynamicLibrary() noexcept = default;
    TERMCHART_API ~DynamicLibrary();

    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // 打开动态库。空路径 → InvalidArgument；打不开 → PlatformError。
    [[nodiscard]] TERMCHART_API static Result<DynamicLibrary> open(std::string_view path) noexcept;

    // 符号查找；未打开或未找到 → nullptr。
    [[nodiscard]] TERMCHART_API void* symbol(const char* name) const noexcept;

    [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }

    // 幂等：未打开时是 no-op。析构自动调用。
    TERMCHART_API void close() noexcept;

    // 入口点符号名（见文件头约定）。
    static constexpr const char* kEntrySymbol = "termchart_plugin_entry";

private:
    explicit DynamicLibrary(void* handle) noexcept : handle_(handle) {}

    void* handle_ = nullptr;
};

// 从已打开的动态库读取插件清单：未打开 / 找不到入口点 → Unsupported；
// 入口点返回 nullptr 或清单 name 为空 → InvalidArgument。
[[nodiscard]] TERMCHART_API Result<Manifest> read_manifest(const DynamicLibrary& library) noexcept;

}  // namespace termchart::plugin

// ===== include/termchart/plugin/plugin.hpp =====
// plugin.hpp —— 插件基类与插件注册表（L19）
//
// 注册时序协议（与 chart / raster / scale / dsl 注册表同构，docs/design/registry.md
// 的统一模式；api/quick.hpp 顶部"注册时机另有协议"指的就是这一条）：
//   * PluginRegistry::instance() 是 **function-local static**，规避静态初始化顺序问题；
//   * 注册的唯一安全时机是 **App 构造之前**（单线程）：App 构造时对注册表做
//     浅拷贝快照并冻结，之后注册的插件对已构造的 App 不可见；
//   * 注册的是**工厂而非实例**：create() 每次返回新实例，生命周期归调用方，
//     没有原型 clone、没有共享所有权；
//   * 本注册表不跨线程：注册与创建都假定发生在宿主线程（跨线程数据走
//     app/post.hpp，不经过注册表）。



namespace termchart::plugin {

// 插件基类：宿主经由 manifest() 认识插件，经由 on_load/on_unload 驱动生命周期。
class Plugin {
public:
    Plugin() = default;
    virtual ~Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    [[nodiscard]] virtual const Manifest& manifest() const noexcept = 0;

    // 加载钩子：返回非 Ok 表示拒绝启用，宿主据此决定丢弃实例。
    // 默认实现是"什么都不做的加载"，多数插件只需要其中一端。
    virtual Status on_load() noexcept { return ErrorCode::Ok; }
    virtual void on_unload() noexcept {}
};

// 工厂签名：create() 每次调用都应产出新实例。
using PluginFactory = std::function<std::unique_ptr<Plugin>()>;

// 注册表条目：描述符 + 工厂。快照 = 一次 vector 拷贝（工厂可拷贝）。
struct PluginDescriptor {
    Manifest manifest;
    PluginFactory factory;
};

// 不区分大小写的名字比较（注册键口径，全模块统一用它）。
[[nodiscard]] TERMCHART_API bool name_equals(std::string_view lhs, std::string_view rhs) noexcept;

class PluginRegistry {
public:
    using Snapshot = std::vector<PluginDescriptor>;

    TERMCHART_API static PluginRegistry& instance() noexcept;

    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    // 注册：重名（不区分大小写）、空名或空工厂 → 拒绝并返回 false。
    TERMCHART_API bool add(const Manifest& manifest, PluginFactory factory);

    // 未注册 → nullptr。返回的指针指向注册表内部，注册表存活期内有效，
    // 但**不得**跨"可能触发注册"的调用持有。
    [[nodiscard]] TERMCHART_API const Manifest* find(std::string_view name) const noexcept;

    // 未注册或工厂为空 → nullptr；否则产出新实例（不调用 on_load，加载由宿主驱动）。
    [[nodiscard]] TERMCHART_API std::unique_ptr<Plugin> create(std::string_view name) const;

    [[nodiscard]] bool supports(std::string_view name) const noexcept;

    // 快照语义：App 构造时调用一次，此后只读快照（浅拷贝：工厂可拷贝）。
    [[nodiscard]] TERMCHART_API Snapshot snapshot() const;

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    // 仅供测试：清空注册表。
    TERMCHART_API void clear_for_test() noexcept;

private:
    PluginRegistry() = default;
    std::vector<PluginDescriptor> entries_;
};

}  // namespace termchart::plugin

// ===== include/termchart/plugin/loader.hpp =====
// loader.hpp —— 静态插件注册与加载
//
// 两种静态供给方式：
//   1. 直接向 PluginRegistry::instance().add(...) 登记描述符；
//   2. 用 add_static(bootstrap) 登记"引导函数"（往下游注册表做一次性注入、
//      一次性初始化），由 load_static_plugins() 统一执行。
//
// 引导函数只执行一次（执行后即移出队列，失败的也不重试）：重复调用
// load_static_plugins() 只会执行新登记的引导。
//
// 图表插件：register_chart_plugin() 把"一个 Chart 工厂"包装成插件——
// 加载时把工厂注入 chart::ChartRegistry，同时插件本身可按名字 create()。
// chart::Chart 的子类即可注册（examples/31"注册自定义图表"走这条路）。
// plugin(L19) → chart(L12) 的依赖由分层规则允许：插件的本质就是往
// L12–L18 的注册表里注入能力。



namespace termchart::plugin {

// 图表工厂：与 chart::ChartRegistry::Factory 同型。
using ChartFactory = std::function<std::unique_ptr<chart::Chart>()>;

// 图表插件：on_load() 把工厂注入 chart::ChartRegistry。重复加载安全
// （register_chart 对同名是覆盖语义，见 chart/registry.hpp 的契约）。
class ChartPlugin final : public Plugin {
public:
    ChartPlugin(std::string_view name, std::string_view version, ChartFactory factory);

    [[nodiscard]] const Manifest& manifest() const noexcept override { return manifest_; }

    Status on_load() noexcept override;

    // 经插件创建图表实例。工厂本身无状态，未 on_load 也可用。
    [[nodiscard]] std::unique_ptr<chart::Chart> create_chart() const;

private:
    Manifest manifest_;
    ChartFactory factory_;
};

// 登记一个图表插件：同时进入 PluginRegistry（可 create）与静态加载队列
// （load_static_plugins() 时注入 chart::ChartRegistry）。
// 空名 / 空工厂 / 重名 → false（此时两边都不登记）。
TERMCHART_API bool register_chart_plugin(std::string_view name, std::string_view version,
                                         ChartFactory factory);

// 静态引导：返回 false = 该条加载失败（宿主可据此打日志）。
using BootstrapFn = std::function<bool()>;

// 登记引导函数（App 构造之前、单线程）。空函数 → false。
TERMCHART_API bool add_static(BootstrapFn bootstrap);

// 执行所有未执行的引导，返回成功的条数。队列随即清空：失败的条目同样
// 移除——静态引导不重试，重试与否应由宿主重新登记。
TERMCHART_API std::size_t load_static_plugins() noexcept;

}  // namespace termchart::plugin

// ===== include/termchart/app/animation.hpp =====
// animation.hpp —— 动画与缓动
//
// 口径（全模块统一，调用方不需要再猜）：
//   * 输入 t 与输出都在 [0,1]；ease() 先把 t 钳位到 [0,1] 再过缓动曲线；
//     非有限 t（NaN/Inf）不可比较，一律视为界外 → 0；
//   * 时间单位是**秒**（double），与 util/timer.hpp 一致；
//   * 时钟可注入（AnimationClock）：单测与 headless 不依赖真实时间流逝，
//     真实时间只是缺省实现（steady_clock，单调，见 util/timer.hpp 的理由）。



namespace termchart::app {

// 缓动曲线族。选择原则：终端动画大多是 UI 过渡（面板展开、数值滚动），
// 二次/三次曲线族够用且单调；不带 overshoot 的 Back/elastic——回弹会让
// 字符画布上的元素"越过"目标格，与"输出在 [0,1]"的口径冲突。
enum class Easing : std::uint8_t {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
};

// 缓动映射：t 会被钳位；输出在 [0,1]。
[[nodiscard]] TERMCHART_API double ease(Easing easing, double t) noexcept;

// 时钟：返回"当前时刻"（秒）。缺省实现基于 steady_clock。
using AnimationClock = std::function<double()>;

// 单段动画：时长 + 缓动 + 起点。时间全部可注入（value_at / finished_at
// 是纯函数，真实时钟版本只是包一层）。
class Animation {
public:
    // duration_seconds <= 0 视为 0：start 后立即 finished、value 为 1。
    explicit Animation(double duration_seconds, Easing easing = Easing::EaseInOutQuad,
                       AnimationClock clock = AnimationClock{});

    // 以当前时钟为起点。
    void start() noexcept;

    // 以指定时刻为起点（测试 / 与外部时钟对齐用）。
    void start_at(double now_seconds) noexcept;

    // 当前进度（缓动后，[0,1]）。未 start → 0。
    [[nodiscard]] double value() noexcept;

    // 纯函数版本：不读时钟。
    [[nodiscard]] double value_at(double now_seconds) const noexcept;

    [[nodiscard]] bool finished() noexcept;
    [[nodiscard]] bool finished_at(double now_seconds) const noexcept;

    [[nodiscard]] double duration() const noexcept { return duration_; }
    [[nodiscard]] Easing easing() const noexcept { return easing_; }

private:
    [[nodiscard]] double now() const noexcept;

    double duration_;
    Easing easing_;
    AnimationClock clock_;
    double start_time_ = 0.0;
    bool started_ = false;
};

}  // namespace termchart::app

// ===== include/termchart/app/config.hpp =====
// config.hpp —— 全局配置（键值对，线程安全）
//
// global() 是 function-local static（与其他注册表同一套契约）；独立实例
// 用于测试与多实例场景。
//
// 类型口径：值是 variant，**读什么类型得什么类型**——跨类型读取回退默认值，
// 不做隐式转换（字符串 "1" 不会当 bool 用）。配置错误应当显式暴露，
// 而不是被静默宽容掉。

#include <mutex>


namespace termchart::app {

using ConfigValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

class Config {
public:
    TERMCHART_API static Config& global() noexcept;

    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // 同 key 覆盖写。monostate 等价于"占位但不带值"（get 会回退默认值）。
    TERMCHART_API void set(std::string_view key, ConfigValue value);

    // 未设置 → std::monostate。
    [[nodiscard]] TERMCHART_API ConfigValue get(std::string_view key) const;

    [[nodiscard]] TERMCHART_API bool has(std::string_view key) const noexcept;
    // 返回是否真的删了（不存在 → false）。
    TERMCHART_API bool remove(std::string_view key);
    TERMCHART_API void clear() noexcept;

    // 类型化读取（带默认值）。
    [[nodiscard]] TERMCHART_API std::string get_string(std::string_view key,
                                                       std::string_view fallback = {}) const;
    [[nodiscard]] TERMCHART_API std::int64_t get_int(std::string_view key,
                                                     std::int64_t fallback = 0) const;
    [[nodiscard]] TERMCHART_API double get_double(std::string_view key, double fallback = 0.0) const;
    [[nodiscard]] TERMCHART_API bool get_bool(std::string_view key, bool fallback = false) const;

    [[nodiscard]] std::size_t size() const noexcept;

private:
    mutable std::mutex mutex_;
    std::map<std::string, ConfigValue, std::less<>> values_;
};

// 全局配置的便捷读写（等价 Config::global() 的对应方法）。
TERMCHART_API void set_config(std::string_view key, ConfigValue value);
[[nodiscard]] TERMCHART_API ConfigValue get_config(std::string_view key);

}  // namespace termchart::app

// ===== include/termchart/app/app.hpp =====
// app.hpp —— App：应用壳（构造时冻结注册表快照）
//
// 职责边界：App 是宿主与库之间的**编排骨架**，不画图、不碰终端、不依赖
// widget——GUI 栈会重写这层（docs/design/gui_stack 的分工），终端栈由
// ImmediateApp / HeadlessSession 补足。
//
// 快照语义（plugin/plugin.hpp 顶部注册时序协议的宿主侧落点）：
//   * 构造时对 PluginRegistry 做一次浅拷贝快照并**冻结**；
//   * 此后（包括 App 存活期内）的注册对**本实例**不可见；
//   * create_plugin 只查自己的快照，不回查全局注册表——宿主看到的插件
//     集合在构造那一刻就确定了，不会因为别的代码注册新插件而漂移。



namespace termchart::app {

class App {
public:
    // 取 PluginRegistry 快照（冻结）。必须在注册完成之后构造。
    TERMCHART_API App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // 从快照创建插件实例（不调用 on_load，加载由宿主驱动）。
    // 未注册 → nullptr。
    [[nodiscard]] TERMCHART_API std::unique_ptr<plugin::Plugin>
    create_plugin(std::string_view name) const;

    [[nodiscard]] TERMCHART_API bool supports_plugin(std::string_view name) const noexcept;

    // 快照内的插件清单（按注册顺序；描述符是快照副本，可安全持有）。
    [[nodiscard]] TERMCHART_API std::vector<plugin::Manifest> plugins() const;

    // 快照整体（含工厂，可用于绕过名字直接创建）。
    [[nodiscard]] TERMCHART_API const plugin::PluginRegistry::Snapshot& plugin_descriptors()
        const noexcept;

    // 全局配置（App 不持有配置，只是入口：配置天然是进程级的）。
    [[nodiscard]] static Config& config() noexcept { return Config::global(); }

private:
    plugin::PluginRegistry::Snapshot plugins_;
};

}  // namespace termchart::app

// ===== include/termchart/app/headless.hpp =====
// headless.hpp —— 无终端模式
//
// 提供"渲染到 CellBuffer 而**不碰 stdout、不触碰 term/**"的入口封装：
//   Canvas（格粒度绘制）→ CellBuffer → render::Renderer → 字节串
//
// 与实时路径（Terminal → Renderer → stdout）共享同一个 Renderer，因此在
// CI / headless / 单元测试里得到的字节流与真终端一致；差异只剩"谁来写
// stdout"——这里谁都不写，字节串直接交给调用方（golden、断言、存文件）。



namespace termchart::app {

// 在画布上作画的回调（ImmediateApp 与 render_once 共用）。
using DrawFn = std::function<void(canvas::Canvas& frame)>;

// 一次性渲染：width×height 画布交给 draw 作画，返回渲染字节串。
// width 或 height 为 0 → InvalidArgument（空画布没有可渲染的东西）。
[[nodiscard]] TERMCHART_API Result<std::string> render_once(std::uint32_t width,
                                                            std::uint32_t height,
                                                            const DrawFn& draw,
                                                            ColorDepth depth =
                                                                ColorDepth::TrueColor);

// 多帧会话：保留画布与 Renderer 的 prev 状态，frame() 出**增量**帧
// （首帧自动全量）；reset() 丢弃 prev，下一帧恢复全量。
// 画面未变时 frame() 返回空串（无差异就无输出）——这正是增量路径的契约。
class HeadlessSession {
public:
    TERMCHART_API HeadlessSession(std::uint32_t width, std::uint32_t height,
                                  ColorDepth depth = ColorDepth::TrueColor);

    [[nodiscard]] canvas::Canvas& canvas() noexcept { return canvas_; }
    [[nodiscard]] const canvas::Canvas& canvas() const noexcept { return canvas_; }

    // 画完再调用。返回本帧字节串（增量）。
    [[nodiscard]] TERMCHART_API std::string frame();

    // 无视 prev 的全量渲染（导出 / 校准用）。
    [[nodiscard]] TERMCHART_API std::string frame_full();

    // 丢弃 prev 状态：下一帧 frame() 必为全量。
    TERMCHART_API void reset() noexcept;

    [[nodiscard]] std::uint32_t width() const noexcept { return canvas_.width(); }
    [[nodiscard]] std::uint32_t height() const noexcept { return canvas_.height(); }

private:
    canvas::Canvas canvas_;
    render::Renderer renderer_;
};

}  // namespace termchart::app

// ===== include/termchart/app/loop.hpp =====
// loop.hpp —— 主事件循环
//
// 节奏由**注入时钟**驱动（缺省 steady_clock 秒，同 util/timer.hpp 的理由）：
// 每帧先读时钟算 delta（首帧 delta = 0），再依次调用 tick 回调。
//
// delta 会被钳制到 max_delta（掉帧保护）：一次 10 秒的卡顿不应该让动画
// 直接跳到终点。上限缺省 0.1 秒，可用 set_max_delta 调整（非有限或 <=0
// 的值被忽略——"不限速"不是合法配置，那等于关掉保护）。
//
// tick 回调里可以安全调用 stop() 与 remove_tick()：每帧在回调列表的副本上
// 迭代，增删不影响当前帧。帧数极小（每秒几十），副本开销可忽略。



namespace termchart::app {

class Loop {
public:
    // tick(delta_seconds)：delta 为本帧距上一帧的秒数（首帧 0，已钳制）。
    using TickFn = std::function<void(double delta_seconds)>;
    // 时钟：当前时刻（秒），可注入。
    using ClockFn = std::function<double()>;

    explicit Loop(ClockFn clock = ClockFn{});

    // 登记每帧回调，返回 id（从 1 起）；空回调被忽略并返回 0。
    TERMCHART_API std::uint64_t add_tick(TickFn fn);

    // 返回是否真的删了（不存在 → false）。
    TERMCHART_API bool remove_tick(std::uint64_t id);

    // 停止标志：当前 run() 立即（或在本帧回调后）退出，可从 tick 内调用。
    TERMCHART_API void stop() noexcept;

    // 最多跑 max_iterations 帧，返回实际帧数。max_iterations = 0 → 0。
    // 每次调用 run() 都会先清除 stop 标志。
    TERMCHART_API std::uint64_t run(std::uint64_t max_iterations);

    [[nodiscard]] TERMCHART_API double max_delta() const noexcept { return max_delta_; }
    TERMCHART_API void set_max_delta(double seconds) noexcept;

    // 累计帧数（跨多次 run()）。
    [[nodiscard]] std::uint64_t iterations() const noexcept { return iterations_; }

private:
    [[nodiscard]] double now() const noexcept;

    ClockFn clock_;
    struct Ticker {
        std::uint64_t id;
        TickFn fn;
    };
    std::vector<Ticker> tickers_;
    std::uint64_t next_id_ = 1;
    std::uint64_t iterations_ = 0;
    double max_delta_ = 0.1;
    double last_time_ = 0.0;
    bool has_last_time_ = false;
    bool stopped_ = false;
};

}  // namespace termchart::app

// ===== include/termchart/app/immediate_app.hpp =====
// immediate_app.hpp —— 即时模式主循环
//
// Dear ImGui 式"每帧重放"：**所有状态都在 frame 回调的捕获里**，本类不沉淀
// 任何图表状态，只负责两件事——节奏（时钟、帧数上限、delta，复用 Loop 的
// 语义）与渲染管线接入（经 HeadlessSession，全程不碰 stdout；换到真终端时
// 只需把渲染段换成实时 Renderer，frame 回调原样保留）。
//
// 每帧的顺序固定：清画布 → frame 回调作画 → 增量渲染进 frames_。
// 清画布是即时模式的定义：画面完全由本帧的回调决定，前一帧不残留。



namespace termchart::app {

// 每帧作画回调：拿到的画布刚被清空；delta_seconds 为帧间隔（已钳制）。
using FrameFn = std::function<void(canvas::Canvas& frame, double delta_seconds)>;

class ImmediateApp {
public:
    // frame 为空 → 每帧渲染空画布（合法：纯节奏测试）。
    ImmediateApp(std::uint32_t width, std::uint32_t height, FrameFn frame,
                 Loop::ClockFn clock = Loop::ClockFn{});

    // 跑 max_frames 帧（0 → 0 帧），返回实际帧数。
    // 每帧：Loop 的一帧（算 delta）→ 清画布 → frame() → 增量渲染。
    TERMCHART_API std::uint64_t run(std::uint64_t max_frames);

    // 停止标志：当前 run() 在本帧回调后退出（转发给内部 Loop）。
    TERMCHART_API void stop() noexcept;

    // 每次 run() 追加（跨 run 保留），便于外部取帧做断言或存 golden。
    [[nodiscard]] const std::vector<std::string>& frames() const noexcept { return frames_; }

    // 累计帧数（跨多次 run()）。
    [[nodiscard]] std::uint64_t frame_count() const noexcept { return loop_.iterations(); }

    [[nodiscard]] Loop& loop() noexcept { return loop_; }
    [[nodiscard]] HeadlessSession& session() noexcept { return session_; }

private:
    void do_frame(double delta_seconds);

    HeadlessSession session_;
    Loop loop_;
    FrameFn frame_;
    std::vector<std::string> frames_;
};

}  // namespace termchart::app

// ===== include/termchart/app/post.hpp =====
// post.hpp —— 跨线程数据投递
//
// 模型：生产线程 post()，消费线程（通常是主循环）take()/drain()。
// 一把互斥锁护一个队列——这不是 lock-free 队列：投递频率是"用户事件"级
// （键盘、网络回调、其他线程的刷新请求），正确性与可读性优先，吞吐不是目标。
//
// 两个原语各管一种形态：
//   * PostQueue  —— 闭包队列：每个消息自带"怎么处理"，适合事件投递；
//   * Mailbox<T> —— 类型化单值信箱：最新值语义（后写覆盖未消费旧值），
//     适合"最新状态"型数据（鼠标位置、当前尺寸）。模板部分只能住在头文件。

#include <deque>


namespace termchart::app {

class PostQueue {
public:
    using Task = std::function<void()>;

    PostQueue() = default;
    PostQueue(const PostQueue&) = delete;
    PostQueue& operator=(const PostQueue&) = delete;

    // 线程安全。空 task 被忽略（无意义投递不占队列位）。
    TERMCHART_API void post(Task task);

    [[nodiscard]] std::size_t pending() const noexcept;

    // 消费接口（1）：把当前队列整体取走，由消费线程自行决定怎么执行。
    // （不加 noexcept：搬移元素时 vector 扩容可能 bad_alloc，不在库内吞掉。）
    [[nodiscard]] TERMCHART_API std::vector<Task> take();

    // 消费接口（2）：取走并**在本线程**依次执行，返回执行数。
    // Task 内部抛出的异常向上传播（消费线程的职责边界，不在这里吞）。
    TERMCHART_API std::size_t drain();

private:
    mutable std::mutex mutex_;
    std::deque<Task> queue_;
};

template <class T>
class Mailbox {
public:
    Mailbox() = default;
    Mailbox(const Mailbox&) = delete;
    Mailbox& operator=(const Mailbox&) = delete;

    // 线程安全。覆盖语义：未消费的旧值被丢弃。
    void post(T value) {
        const std::lock_guard<std::mutex> lock(mutex_);
        slot_ = std::move(value);
    }

    // 取走当前值；无值 → false。
    [[nodiscard]] bool consume(T& out) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!slot_.has_value()) {
            return false;
        }
        out = std::move(*slot_);
        slot_.reset();
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const std::lock_guard<std::mutex> lock(mutex_);
        return !slot_.has_value();
    }

private:
    mutable std::mutex mutex_;
    std::optional<T> slot_;
};

}  // namespace termchart::app

// ===== include/termchart/api/opts.hpp =====
// opts.hpp —— ApiOptions 链式配置（旧名 `Opts`，见文件末尾的兼容别名）
//
// 面向"一行出图"的入口：所有可选参数都在这一处，且**每个都有默认值**，
// 于是 tc::quick::hello() 之类的调用可以完全零配置。
// 语义：未设置的项（std::optional 为空）表示"由库自行决定"。
//
// 命名：全库其余位置一律 `XxxOptions`（MetroOptions / SankeyOptions / …），
// 这里原名 `Opts` 是唯一的例外；为"同一语义同一命名"收敛成 `ApiOptions`，
// 旧名以别名保留，因此**不破坏**任何既有调用点。



namespace termchart::api {

class ApiOptions {
public:
    // ── 尺寸 ──────────────────────────────────────────────────────────
    TERMCHART_API ApiOptions& width(std::uint16_t value) noexcept;
    TERMCHART_API ApiOptions& height(std::uint16_t value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> width() const noexcept;
    [[nodiscard]] TERMCHART_API std::optional<std::uint16_t> height() const noexcept;

    // ── 颜色 ──────────────────────────────────────────────────────────
    TERMCHART_API ApiOptions& color_depth(ColorDepth value) noexcept;
    [[nodiscard]] TERMCHART_API std::optional<ColorDepth> color_depth() const noexcept;

    // ── 文本模式 ──────────────────────────────────────────────────────
    // unicode == false 时一切绘制退化到 ASCII（制表符 → '+'、块字符 → '#'）。
    TERMCHART_API ApiOptions& unicode(bool value) noexcept;
    TERMCHART_API ApiOptions& ascii_only() noexcept;
    [[nodiscard]] TERMCHART_API bool unicode() const noexcept;

    // ── 输出 ──────────────────────────────────────────────────────────
    // newline：是否在输出末尾补一个换行（默认补）。
    TERMCHART_API ApiOptions& newline(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool newline() const noexcept;

    // reset：帧尾是否复位 SGR（默认复位，避免污染后续输出）。
    TERMCHART_API ApiOptions& reset_at_end(bool value) noexcept;
    [[nodiscard]] TERMCHART_API bool reset_at_end() const noexcept;

private:
    std::optional<std::uint16_t> width_;
    std::optional<std::uint16_t> height_;
    std::optional<ColorDepth> depth_;
    bool unicode_ = true;
    bool newline_ = true;
    bool reset_ = true;
};

// 兼容别名：老代码写 `api::Opts` 仍然成立（全库只有这一处叫 Opts）。
using Opts = ApiOptions;

}  // namespace termchart::api

// ===== include/termchart/api/chart.hpp =====
// chart.hpp —— 一行出图（chart 侧的 api 门面）
//
// 为什么要有它：examples/03 的"标准姿势"有五步（建 Canvas → 建 Chart → 喂 Series
// → chart.draw({canvas}) → Renderer::render_full → 写 stdout），其中四步是纯样板，
// 且**每一步都能写错**（忘了 render_full 会拿到空帧、忘了 flush 会丢半屏）。
// `render_chart` 把它们收成一次调用：你只负责数据与图表类型。
//
// 与 `quick::render`（CellBuffer → 字节）的分工：quick 面向"我已经有一块 buffer"，
// 这里面向"我只有一个图表"。
//
// 尺寸规则是**刻板**的（见 cpp 里的实现注释）：不自动探测终端。理由——
// 自动探测会让同一段代码在 CI/headless 与真终端里产出不同尺寸，测试和文档都没法
// 复现；要自适应请显式传 `opts.width(height_of_terminal)`（另见 CODEBUDDY.md 坑 11：
// 画布大于可视区会折行 + 滚动，图像错位）。



namespace termchart::api {

// 任意 Chart（折线 / 柱 / 散点 / 迷你线…）→ 一次成帧的字节流（全量，不含差分）。
// 尺寸：opts.width()/height() 有值就用（显式 0 是 InvalidArgument），否则 80×24。
[[nodiscard]] TERMCHART_API Result<std::string> render_chart(chart::Chart& chart,
                                                             const Opts& opts = {}) noexcept;

// = render_chart + 写 stdout + flush。返回 stdout 的写入状态（渲染失败时返回该错误）。
TERMCHART_API Status print_chart(chart::Chart& chart, const Opts& opts = {}) noexcept;

}  // namespace termchart::api

// ===== include/termchart/api/print.hpp =====
// print.hpp —— stdout 输出封装
//
// 为什么单独一个文件：输出是**唯一会触碰进程全局状态的动作**。
// 把它收在一处，headless / CI / 测试就能整体替换（或直接不调用）。
// 约定：不做 locale 相关转换；写的是渲染管线产出的原始字节。



namespace termchart::api {

// 写入 FILE*（默认 stdout）：短写返回 IoError。
TERMCHART_API Status write_to(std::FILE* stream, std::string_view bytes) noexcept;
TERMCHART_API Status write_stdout(std::string_view bytes) noexcept;
TERMCHART_API Status write_stderr(std::string_view bytes) noexcept;

// 刷缓冲：终端交互下必须显式调用，否则输出会粘在缓冲区里。
TERMCHART_API Status flush(std::FILE* stream) noexcept;
TERMCHART_API Status flush_stdout() noexcept;

// 二进制安全诊断：把不可打印字节变成可读形式，供日志/golden 失败信息使用。
TERMCHART_API std::string escape_for_debug(std::string_view bytes);

}  // namespace termchart::api

// ===== include/termchart/api/quick.hpp =====
// quick.hpp —— 总入口
//
// 两件事：
//   1. init_core()：进程内一次性初始化（**std::call_once 守卫**，可重复调用）。
//      初始化内容是"全局副作用"的最后容身处：日志级别、默认能力快照。
//      注意：**注册表不在这里** —— 注册时机另有协议（见 docs/design/registry.md）。
//   2. 一行出图的便捷接口：给定文本与样式，走完整的
//      CellBuffer → Renderer → stdout 链路。



namespace termchart::api {

// 幂等。首次调用完成初始化；后续调用直接返回。
TERMCHART_API void init_core() noexcept;
[[nodiscard]] TERMCHART_API bool core_initialized() noexcept;

// 渲染成字节流：**不写 stdout**。headless / golden / 单元测试用这个。
TERMCHART_API Result<std::string> render_line(std::string_view text, const Style& style,
                                              const Opts& opts = {}) noexcept;

// 渲染 + 写出。返回写状态；渲染错误（如宽度非法）直接透传。
TERMCHART_API Status print_line(std::string_view text, const Style& style,
                                const Opts& opts = {}) noexcept;

// 零配置演示：一行调用即看到彩色输出（examples/32_quick_one_liner.cpp）。
TERMCHART_API Status hello(const Opts& opts = {}) noexcept;

// ── 一行出图（matplotlib 式零配置）────────────────────────────────────────────
//
// 目标：新手 `api::plot(x, y)` 一行即见图，**无需注册 backend、无需选字体、无需懂
// Scene/Chart/Backend 分层**。内部固定走 CellBuffer → Canvas → chart → Renderer →
// stdout（ANSI），把 backend 选择/颜色降级全藏在壳里。headless / 测试用 render_*。

struct PlotOpts {
    std::uint32_t width = 80;
    std::uint32_t height = 24;
    std::string title;
    ColorDepth depth = ColorDepth::TrueColor;
    bool show_legend = false;  // 单系列默认不画图例（新手向）
};

// 折线图：(x[i], y[i]) 逐点连线 + 数据点标记。
TERMCHART_API Result<std::string> render_plot(const std::vector<double>& x,
                                              const std::vector<double>& y,
                                              const PlotOpts& opts = {}) noexcept;
TERMCHART_API Status plot(const std::vector<double>& x, const std::vector<double>& y,
                          const PlotOpts& opts = {}) noexcept;

// 散点图：不连线，仅标记每个点。
TERMCHART_API Result<std::string> render_scatter(const std::vector<double>& x,
                                                 const std::vector<double>& y,
                                                 const PlotOpts& opts = {}) noexcept;
TERMCHART_API Status scatter(const std::vector<double>& x, const std::vector<double>& y,
                             const PlotOpts& opts = {}) noexcept;

// 柱状图：values[i] 配 labels[i]（labels 缺省按序号）。
TERMCHART_API Result<std::string> render_bar(const std::vector<double>& values,
                                             const std::vector<std::string>& labels,
                                             const PlotOpts& opts = {}) noexcept;
TERMCHART_API Status bar(const std::vector<double>& values,
                         const std::vector<std::string>& labels,
                         const PlotOpts& opts = {}) noexcept;

}  // namespace termchart::api

// ===== include/termchart/termchart.hpp =====
// termchart.hpp —— 伞形头（极简）
//
// 只包含"开箱即用"所需的最小集合：核心数据结构 + 终端 + 渲染 + 一行接口。
// 全量见 termchart_full.hpp（阶段 6）。
//
// 分层豁免：伞形头是聚合入口，天然依赖全模块，check_layers.py 对它不做层号判定。


// core

// util

// color

// term

// render

// api

// 便捷别名：绝大多数用户代码只写 tc::。
namespace tc = termchart;

// 版本查询（定义在 src/termchart.cpp）。
namespace termchart {
const char* version_string() noexcept;
}

// ===== include/termchart/gui/font.hpp =====
// font.hpp —— GUI 文本的唯一出口（对调用方透明的混合字体后端）
//
// 渲染后端分工（接口不变，底层实现见 font.cpp）：
//   * ASCII 0x20..0x7E：内置 5×7 点阵。零成本、零外部依赖，且"ASCII 字形
//     落像素"的像素契约由测试钉住；
//   * 其余码点（CJK / Latin-1 扩展 / 全角符号…）：惰性加载系统 TTF 字体
//     （Windows 自动尝试 C:/Windows/Fonts/msyh.ttc、simhei.ttf 等；可用
//     环境变量 TERMCHART_GUI_FONT 显式指定字体文件路径），找不到字体或
//     解析失败时 fail-safe 退回点阵占位框——不抛异常、不崩溃。
//   等宽语义不变：列数由 util 的显示宽度决定（CJK 占 2 列），每格 advance
//   恒等于 cell_w；TTF 只决定格内像素，不参与排版步进（无 shaping/hinting，
//   不存在字距把布局挤歪的问题）。
//
// 字形是 5×7 列位点阵（数据与位序约定见 font.cpp 的 kFont5x7），渲染时映射进
// cell_w×cell_h 的整数格：字形画在前 5/6 格宽、前 7/8 格高（末列/末行留作
// 字距），保证 advance == cell_w 整数对齐。TTF 也没有覆盖到的码点画"虚线
// 占位框"而不是留白——中文标签掉成空白比掉成虚框更难排查，可见且可度量是底线。
namespace termchart::gui {

struct TextMetrics { double width = 0.0; double height = 0.0; };

// 字体后端多态基类：gui/font 的唯一文本出口经它分派到具体字形来源。
// 三个派生实现（5×7 点阵 / 系统 TTF / 二者组合的降级链）是
// src/gui/font.cpp 的实现细节，不进公开头——加后端不改公开 API。
//
// 三条虚函数的契约：
//   * has_glyph：本后端是否**可能**覆盖该码点（组合链的委派依据）。
//     允许保守声明（宁可说覆盖、画时再判），因为组合链按 draw_glyph
//     的返回值决定胜出，不依赖 has_glyph 的精确性；
//   * measure_width：单码点像素宽度。等宽网格契约——列数由
//     util::codepoint_width 决定（CJK=2 列），advance = 列数 × cell_w。
//     后端不得自行改变步进，否则排版会因字体环境不同而漂移；
//   * draw_glyph：画一个码点（占 columns 列，at 为格盒左上角）。返回
//     true = 画出了内容；false = 本后端不覆盖，组合链继续问下一个，
//     全不覆盖由 draw_text 画虚线占位框。
class TERMCHART_API FontBackend {
public:
    // 类内只声明、font.cpp 内定义（key function 惯用法）：vtable/typeinfo
    // 只在这一个 TU 发强符号，匿名 namespace 的派生类各自只发弱表。
    virtual ~FontBackend();
    [[nodiscard]] virtual bool has_glyph(char32_t codepoint) const noexcept = 0;
    [[nodiscard]] virtual double measure_width(char32_t codepoint, double cell_w,
                                               double cell_h) const noexcept = 0;
    [[nodiscard]] virtual bool draw_glyph(image::Bitmap& bitmap, char32_t codepoint, Point at,
                                          const Rgb& color, int columns, double cell_w,
                                          double cell_h) const noexcept = 0;
};

// ASCII 0x20..0x7E 内置字形覆盖查询（**契约不变**：只回答内置点阵表，
// 其余返回 false；TTF 命中不经过此查询，TTF 能力走 draw_text 组合链）。
[[nodiscard]] TERMCHART_API bool has_glyph(char32_t codepoint) noexcept;

// 文本像素尺寸：width = 列数 × cell_w（列数来自 util 的显示宽度，CJK=2 列），height = cell_h。
[[nodiscard]] TERMCHART_API TextMetrics measure_text(std::string_view utf8_text,
                                                     double cell_w, double cell_h) noexcept;

// 在 (at.x, at.y) 为**左上角**画一行 UTF-8 文本，返回实际占用宽度（= measure 的 width）。
// 缺字码点画"虚线占位框"（占 display_width 格），保证中文可见、可度量。
[[nodiscard]] TERMCHART_API double draw_text(image::Bitmap& bitmap, std::string_view utf8_text,
                                             Point at, Rgb color,
                                             double cell_w, double cell_h) noexcept;
}  // namespace termchart::gui

// ===== include/termchart/gui/scene_renderer.hpp =====
// scene_renderer.hpp —— Scene → Bitmap（GUI 平行栈的第一块落地）
//
// 定位（docs/design/gui_stack.md）：GUI 与终端是平行栈，共享到 `scene`（L4）
// 为止。本文件是 GUI 侧的 `render/scene_renderer`——消费与终端/SVG **同一份**
// Scene 指令集、**同一份** StatefulVisitor 时序；区别只在"往哪画"：
// 终端往字符格画，这里往 image::Bitmap 画。
//
// 渲染质量决策（为什么这样画）：
//   * 几何（线/弧/矩形）走 **S×S 超采样 + 盒式降采样**得到抗锯齿——实现小、
//     质量对得起"生产级"，且不需要解析覆盖率这类容易写错的数学；
//   * 文本**不参与超采样**：gui/font 是位图点阵字体，超采样只会把它糊掉，
//     所以文本在下采样后按最终分辨率直绘，且**永远在几何之上**
//     （图表标签/图例压在网格线上是生产惯例）；
//   * 线宽语义与 SVG 同构：StrokeOp.width 是 Scene 单位，随 autofit 缩放。
//
// 不做的事（边界）：不解析 ImageOp 的像素（Scene 不持有位图，画占位棋盘格）；
// 不做渐变/裁剪/图层（Scene 刻意不是完备 IR，见 scene/command.hpp）。



namespace termchart::gui {

struct RenderOptions {
    // 输出位图尺寸（像素）。
    std::uint32_t width = 960;
    std::uint32_t height = 540;
    // true：按 Scene bounds 自动算缩放，内容带 margin 铺满画布（与 SVG autofit 同构）；
    // false：用固定 scale，Scene 坐标即像素坐标。
    bool autofit = true;
    double margin = 24.0;  // autofit 边距（像素）
    double scale = 1.0;    // autofit=false 时生效：Scene 单位 → 像素

    // 底色与"未设色"兜底。Color::none() 的描边/文本用 foreground。
    bool transparent = false;  // true → 背景 alpha 0（供再合成），几何不透明
    Rgb background{20, 20, 20};
    Rgb foreground{229, 229, 229};

    // 等宽文本：TextOp 没有字号（Scene 不建模排版），字符格大小在这里给，
    // 单位是 Scene 坐标（随 autofit 一起缩放）。
    double text_cell_w = 9.0;
    double text_cell_h = 16.0;

    // 抗锯齿超采样倍数。1 = 关（逐像素锯齿）；2–4 = 盒式降采样；越界值收 3。
    int supersample = 3;
};

// 无状态可复用：同一 renderer 可连续 render 多份 Scene（内部状态每次重置）。
class SceneRenderer final : private scene::StatefulVisitor {
public:
    explicit SceneRenderer(RenderOptions options) noexcept;

    // 渲染整份 Scene。输出恒为 4 通道 RGBA（transparent 时背景 alpha=0）。
    [[nodiscard]] image::Bitmap render(const scene::Scene& scene);

private:
    // ── StatefulVisitor 的"往哪画" ──
    // 路径：线段在 extend_path 时立即按 current_stroke() 落到超采样缓冲——
    // 时序保证此时读到的是"这段几何出现时的描边色"，无需 end_path 再收口。
    void begin_path(const Point& from) override;
    void extend_path(const Point& from, const Point& to) override;
    void end_path() override;  // 几何已即时落盘，无需收尾动作（保持幂等）
    void emit_filled_rect(const Rect& rect, const Color& fill) override;
    void emit_stroked_rect(const Rect& rect, const Color& stroke, double width) override;
    void emit_arc(const scene::ArcOp& arc, const Color& stroke, double width) override;
    void emit_text(const scene::TextOp& text) override;
    void emit_image(const scene::ImageOp& image) override;

    // ── 超采样缓冲上的几何原语（坐标均为 Scene → 像素换算后的值）──
    void stamp(double cx, double cy, double radius, const Rgb& rgb);
    void draw_segment(double x0, double y0, double x1, double y1, double radius, const Rgb& rgb);
    void fill_rect_ss(double x, double y, double w, double h, const Rgb& rgb);
    void stroke_rect_ss(const Rect& rect, double radius, const Rgb& rgb);

    // Scene 坐标 → 超采样像素。
    [[nodiscard]] double to_px_x(double x) const noexcept;
    [[nodiscard]] double to_px_y(double y) const noexcept;

    // 待下采样后直绘的文本（位图字体不参与超采样，理由见文件头）。
    struct PendingText {
        double x = 0.0;  // 最终像素坐标（左上角）
        double y = 0.0;
        double cell_w = 0.0;
        double cell_h = 0.0;
        Rgb color{};
        std::string text;
    };

    RenderOptions opts_{};
    // 几何变换：px = scene * view_scale + offset（像素域），超采样时再乘 ss_。
    double view_scale_ = 1.0;
    double view_tx_ = 0.0;
    double view_ty_ = 0.0;
    int ss_ = 3;
    std::uint32_t sw_ = 0;  // 超采样缓冲宽
    std::uint32_t sh_ = 0;  // 超采样缓冲高
    std::vector<std::uint8_t> buffer_;  // RGBA8，sw_ × sh_ × 4
    Rgb stroke_rgb_{};
    double stroke_radius_ = 1.0;  // 半线宽（超采样像素）
    std::vector<PendingText> texts_;
    color::ResolveOptions resolve_{};
};

}  // namespace termchart::gui

// ===== include/termchart/termchart_full.hpp =====
// termchart_full.hpp —— 全量伞形头（自动生成，请勿手改；见 tools/gen_full_header.py）
// 包含整个 termchart 公共 API：termchart.hpp 覆盖 core/util/color/term/render/api，
// 这里再补齐 chart/graph/scale/raster/dsl/widget/gui/backend/interaction 等全部子模块。


namespace tc = termchart;

// ===== include/termchart/gui/hover_chart.hpp =====
// hover_chart.hpp —— GUI 悬停层：命中 → 把浮层翻译成 Scene 图元
//
// 定位：GUI 平行栈的交互呈现。核心设计是"**悬停不改渲染器，只改 Scene**"——
// gui::SceneRenderer 根本不知道交互存在，它只是又消费了一份 Scene。
//
// 复用面（都是既有件，不是新写的状态机）：
//   * chart::HitRegion         —— 图表渲染时登记的命中形状 + 不透明 payload，
//                                 是 chart → interaction 的唯一契约；
//   * interaction::topmost_hit —— 纯几何判定（最后登记者在上），禁图表名；
//   * scene::SceneBuilder      —— 浮层就是普通的 RectOp / TextOp 图元。
//
// 与终端侧 interaction::Tooltip 的关系：**刻意不共用实现**。终端 tooltip 是
// cell 补丁语义（记住被改写的格、clear() 后逐格还原），因为它画在字符画布上、
// 覆盖即破坏；GUI 每帧重建整份 Scene，浮层只是"这一帧多塞几个图元"，既不需要
// patch/restore，也不该把"字符格"这个概念带进像素域。二者共享的只有上游的
// HitRegion / topmost_hit，以及"贴边要翻方向"这条直觉。
//
// 坐标系：HitRegion 与浮层坐标都在 **Scene 坐标系**里，与底图同一坐标系。
// 调用方负责把窗口/鼠标像素换算成 Scene 坐标；autofit=false 时二者恒等。



namespace termchart::gui {

class HoverLayer {
public:
    // base    底图 Scene；
    // regions 命中区域（约定 payload ≥ 1，0 视为"只有几何、未登记语义"）；
    // labels  labels[payload-1] 是该 payload 的浮层文本，可含 '\n' 多行；
    // options 与渲染底图时**同一份** RenderOptions（坐标系/文本格必须一致）。
    TERMCHART_API HoverLayer(scene::Scene base,
                             std::vector<chart::HitRegion> regions,
                             std::vector<std::string> labels,
                             RenderOptions options);

    // 不带浮层的底图。
    [[nodiscard]] TERMCHART_API image::Bitmap render_base() const;

    // (x,y) 命中则返回"底图 + 浮层"；未命中等价于 render_base()。
    // 浮层位置：优先锚点右上，越界翻左上、再越界翻下方，最后夹进画布。
    [[nodiscard]] TERMCHART_API image::Bitmap render_at(double x, double y) const;

    // 只查询不渲染：命中返回该 payload 的浮层文本，未命中/未登记返回空串。
    [[nodiscard]] TERMCHART_API std::string hit_label(double x, double y) const;

private:
    scene::Scene base_;
    std::vector<chart::HitRegion> regions_;
    std::vector<std::string> labels_;
    RenderOptions options_;
};

}  // namespace termchart::gui

#endif  // TERMCHART_SINGLE_HEADER
