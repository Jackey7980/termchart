// fwd.hpp —— 公开类型前向声明
//
// 用途：让只"持有指针/引用"的头不必拉进完整定义（降低编译耦合）。
// 前向声明的类型必须与真实声明保持一致（namespace / 模板参数 / class vs struct），
// 否则会有 ODR 风险——新增前向声明时先看真实定义。
#ifndef TERMCHART_FWD_HPP
#define TERMCHART_FWD_HPP

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
class Opts;
}  // namespace api

}  // namespace termchart

#endif  // TERMCHART_FWD_HPP
