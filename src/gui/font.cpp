// font.cpp —— 字体后端多态：BitmapBackend（5×7 点阵）/ TtfBackend（系统
// TTF）/ FallbackBackend（有序组合链），公开 API 经组合链单例分派
//
// 字形数据来源：经典公共域 5×7 "classic" 列位字体（Arduino / Adafruit GFX
// glcdfont.c 采用的那套，源头是早期 HD44780 LCD 的公共域字模）。列位序：
// 每字节一列，bit n 为字形第 n 行的点，只用低 7 位（5×7）；首列在左。
//
// 装配（进程内单例，首次绘制时惰性初始化）：
//   FallbackBackend{ TtfBackend, BitmapBackend }
//     * 全部码点（含 ASCII 0x20..0x7E）：TtfBackend 优先用 image::TtfFont 解析
//       系统字体、在**格内**矢量光栅化（4×4 超采样），清晰且带字距；拉丁字形
//       取自候选清单靠前的拉丁字体，汉字落到靠后的 CJK 字体，逐码点取首个含
//       轮廓的字体；
//     * 内置 5×7 点阵 + 虚线占位框：TTF 整体不可用（无系统字体 / 解析失败 /
//       某码点无轮廓）时的 fail-safe 兜底，不抛异常、不崩溃、不留白；
//     * advance 仍是 util 显示宽度的整格（CJK=2 列），字体只决定格内像素，
//       不参与排版步进（无 shaping/hinting，不存在字距把布局挤歪的问题）。
//
// 线程模型：单例的 function-local static 初始化由 C++11 保证线程安全；
// 但 TtfFont::glyph() 的 cache 无锁（见 ttf_font.hpp 约定），故 TTF
// 绘制路径要求单线程调用——GUI 主循环（native_loop dispatcher）本就单线程。
#include "termchart/gui/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "termchart/media/image/ttf_font.hpp"
#include "termchart/util/utf8.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::gui {
namespace {

// 运行时字体覆盖：set_font_file 设置，优先于环境变量 TERMCHART_GUI_FONT 与
// 系统候选清单；clear_font_file 复位为"使用默认链"。g_font_dirty 触发组合链重建。
std::optional<std::string> g_font_override;
bool g_font_dirty = true;          // 首次需构建
std::mutex g_chain_mutex;          // 护住"运行时改字体 / 重建链"的并发
std::unique_ptr<FontBackend> g_chain;
bool g_chain_built = false;

constexpr char32_t kFirstCp = 0x20;  // 内置字形覆盖 0x20..0x7E（95 个可见 ASCII）
constexpr char32_t kLastCp = 0x7E;
constexpr int kGlyphCols = 5;  // 字形实宽：格内第 6 列留作字距（5/6 格宽）
constexpr int kGlyphRows = 7;  // 字形实高：格内第 8 行留作行距（7/8 格高）
constexpr double kDashStep = 2.0;  // 占位框虚线：每隔 ~2 像素一个点
constexpr double kMaxExtent = 1.0e6;  // cell_w/cell_h 的合理上限（防御非法输入）

// 95 个可见 ASCII 的 5×7 列位字模，索引 = codepoint - 0x20。
// 来源：经典公共域 5x7 字体（与 Adafruit GFX glcdfont.c 的 classic font 同源，
// 源头是早期 HD44780 LCD 公共域字模）。kFont5x7[i][col] 的 bit n 是字形第 n
// 行、第 col 列的点（bit0 在上，首列在左）。
constexpr std::uint8_t kFont5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // 0x20 ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00},  // 0x21 '!'
    {0x00, 0x07, 0x00, 0x07, 0x00},  // 0x22 '"'
    {0x14, 0x7F, 0x14, 0x7F, 0x14},  // 0x23 '#'
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},  // 0x24 '$'
    {0x23, 0x13, 0x08, 0x64, 0x62},  // 0x25 '%'
    {0x36, 0x49, 0x55, 0x22, 0x50},  // 0x26 '&'
    {0x00, 0x05, 0x03, 0x00, 0x00},  // 0x27 '\''
    {0x00, 0x1C, 0x22, 0x41, 0x00},  // 0x28 '('
    {0x00, 0x41, 0x22, 0x1C, 0x00},  // 0x29 ')'
    {0x14, 0x08, 0x3E, 0x08, 0x14},  // 0x2A '*'
    {0x08, 0x08, 0x3E, 0x08, 0x08},  // 0x2B '+'
    {0x00, 0x50, 0x30, 0x00, 0x00},  // 0x2C ','
    {0x08, 0x08, 0x08, 0x08, 0x08},  // 0x2D '-'
    {0x00, 0x60, 0x60, 0x00, 0x00},  // 0x2E '.'
    {0x20, 0x10, 0x08, 0x04, 0x02},  // 0x2F '/'
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0x30 '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00},  // 0x31 '1'
    {0x42, 0x61, 0x51, 0x49, 0x46},  // 0x32 '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31},  // 0x33 '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10},  // 0x34 '4'
    {0x27, 0x45, 0x45, 0x45, 0x39},  // 0x35 '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 0x36 '6'
    {0x01, 0x71, 0x09, 0x05, 0x03},  // 0x37 '7'
    {0x36, 0x49, 0x49, 0x49, 0x36},  // 0x38 '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E},  // 0x39 '9'
    {0x00, 0x36, 0x36, 0x00, 0x00},  // 0x3A ':'
    {0x00, 0x56, 0x36, 0x00, 0x00},  // 0x3B ';'
    {0x00, 0x08, 0x14, 0x22, 0x41},  // 0x3C '<'
    {0x14, 0x14, 0x14, 0x14, 0x14},  // 0x3D '='
    {0x41, 0x22, 0x14, 0x08, 0x00},  // 0x3E '>'
    {0x02, 0x01, 0x51, 0x09, 0x06},  // 0x3F '?'
    {0x32, 0x49, 0x79, 0x41, 0x3E},  // 0x40 '@'
    {0x7E, 0x11, 0x11, 0x11, 0x7E},  // 0x41 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36},  // 0x42 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22},  // 0x43 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C},  // 0x44 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41},  // 0x45 'E'
    {0x7F, 0x09, 0x09, 0x01, 0x01},  // 0x46 'F'
    {0x3E, 0x41, 0x41, 0x51, 0x32},  // 0x47 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F},  // 0x48 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00},  // 0x49 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01},  // 0x4A 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41},  // 0x4B 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40},  // 0x4C 'L'
    {0x7F, 0x02, 0x04, 0x02, 0x7F},  // 0x4D 'M'
    {0x7F, 0x04, 0x08, 0x10, 0x7F},  // 0x4E 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  // 0x4F 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06},  // 0x50 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E},  // 0x51 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46},  // 0x52 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31},  // 0x53 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01},  // 0x54 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F},  // 0x55 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F},  // 0x56 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F},  // 0x57 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63},  // 0x58 'X'
    {0x03, 0x04, 0x78, 0x04, 0x03},  // 0x59 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43},  // 0x5A 'Z'
    {0x00, 0x00, 0x7F, 0x41, 0x41},  // 0x5B '['
    {0x02, 0x04, 0x08, 0x10, 0x20},  // 0x5C '\\'
    {0x41, 0x41, 0x7F, 0x00, 0x00},  // 0x5D ']'
    {0x04, 0x02, 0x01, 0x02, 0x04},  // 0x5E '^'
    {0x40, 0x40, 0x40, 0x40, 0x40},  // 0x5F '_'
    {0x00, 0x01, 0x02, 0x04, 0x00},  // 0x60 '`'
    {0x20, 0x54, 0x54, 0x54, 0x78},  // 0x61 'a'
    {0x7F, 0x48, 0x44, 0x44, 0x38},  // 0x62 'b'
    {0x38, 0x44, 0x44, 0x44, 0x20},  // 0x63 'c'
    {0x38, 0x44, 0x44, 0x48, 0x7F},  // 0x64 'd'
    {0x38, 0x54, 0x54, 0x54, 0x18},  // 0x65 'e'
    {0x08, 0x7E, 0x09, 0x01, 0x02},  // 0x66 'f'
    {0x08, 0x14, 0x54, 0x54, 0x3C},  // 0x67 'g'
    {0x7F, 0x08, 0x04, 0x04, 0x78},  // 0x68 'h'
    {0x00, 0x44, 0x7D, 0x40, 0x00},  // 0x69 'i'
    {0x20, 0x40, 0x44, 0x3D, 0x00},  // 0x6A 'j'
    {0x00, 0x7F, 0x10, 0x28, 0x44},  // 0x6B 'k'
    {0x00, 0x41, 0x7F, 0x40, 0x00},  // 0x6C 'l'
    {0x7C, 0x04, 0x18, 0x04, 0x78},  // 0x6D 'm'
    {0x7C, 0x08, 0x04, 0x04, 0x78},  // 0x6E 'n'
    {0x38, 0x44, 0x44, 0x44, 0x38},  // 0x6F 'o'
    {0x7C, 0x14, 0x14, 0x14, 0x08},  // 0x70 'p'
    {0x08, 0x14, 0x14, 0x18, 0x7C},  // 0x71 'q'
    {0x7C, 0x08, 0x04, 0x04, 0x08},  // 0x72 'r'
    {0x48, 0x54, 0x54, 0x54, 0x20},  // 0x73 's'
    {0x04, 0x3F, 0x44, 0x40, 0x20},  // 0x74 't'
    {0x3C, 0x40, 0x40, 0x20, 0x7C},  // 0x75 'u'
    {0x1C, 0x20, 0x40, 0x20, 0x1C},  // 0x76 'v'
    {0x3C, 0x40, 0x30, 0x40, 0x3C},  // 0x77 'w'
    {0x44, 0x28, 0x10, 0x28, 0x44},  // 0x78 'x'
    {0x0C, 0x50, 0x50, 0x50, 0x3C},  // 0x79 'y'
    {0x44, 0x64, 0x54, 0x4C, 0x44},  // 0x7A 'z'
    {0x00, 0x08, 0x36, 0x41, 0x00},  // 0x7B '{'
    {0x00, 0x00, 0x7F, 0x00, 0x00},  // 0x7C '|'
    {0x00, 0x41, 0x36, 0x08, 0x00},  // 0x7D '}'
    {0x08, 0x08, 0x2A, 0x1C, 0x08},  // 0x7E '~'
};

// 画一个像素：坐标先取整后判界，NaN/±Inf/越界一律静默丢弃（与 Bitmap::set
// 的"写越界忽略"同语义，且避免浮点→整数转换的未定义行为）。
void put_pixel(image::Bitmap& bitmap, double x, double y, const Rgb& color) noexcept {
    if (!(x >= 0.0) || !(y >= 0.0)) {
        return;  // NaN 与负坐标都落不进位图
    }
    if (x >= 4.0e9 || y >= 4.0e9) {
        return;  // 超过 uint32 可表示范围，夹不住就丢
    }
    const auto ux = static_cast<std::uint32_t>(x);
    const auto uy = static_cast<std::uint32_t>(y);
    if (ux >= bitmap.width() || uy >= bitmap.height()) {
        return;
    }
    bitmap.set(ux, uy, 0, color.r);
    bitmap.set(ux, uy, 1, color.g);
    bitmap.set(ux, uy, 2, color.b);
    if (bitmap.channels() == 4) {
        bitmap.set(ux, uy, 3, 255);
    }
}

// 格尺寸的防御：非有限或负值按 0 处理。接口是 noexcept，任何输入都不许把
// 后续的浮点→整数转换变成 UB；合法输入（普通像素尺寸）完全不受影响。
constexpr double sane_extent(double v) noexcept {
    return (v >= 0.0 && v < kMaxExtent) ? v : 0.0;
}

// 缺字占位框：cells 格宽 × 1 格高的虚线框，四边每隔 ~2 像素一个点。
// 目的不是好看，而是让缺字可见、可度量：中文标签掉成空白比掉成虚框更难排查。
void draw_placeholder(image::Bitmap& bitmap, double x, double y, int cells, double cw, double ch,
                      const Rgb& color) noexcept {
    const double box_w = std::floor(static_cast<double>(cells) * cw);
    const double box_h = std::floor(ch);
    if (!(box_w > 0.0) || !(box_h > 0.0)) {
        return;
    }
    const double x1 = x + box_w - 1.0;  // 最右/最下像素（double 运算，无整型溢出）
    const double y1 = y + box_h - 1.0;
    for (double dx = 0.0; dx < box_w; dx += kDashStep) {
        put_pixel(bitmap, x + dx, y, color);
        put_pixel(bitmap, x + dx, y1, color);
    }
    for (double dy = 0.0; dy < box_h; dy += kDashStep) {
        put_pixel(bitmap, x, y + dy, color);
        put_pixel(bitmap, x1, y + dy, color);
    }
}

// 按覆盖率把一个字形像素混入目标位图（源 = 文字色，coverage 来自字形 alpha）。
// 目标 3/4 通道都支持；4 通道时 alpha 置 255（画布不透明语义）。
void blend_pixel(image::Bitmap& bitmap, std::uint32_t x, std::uint32_t y, const Rgb& color,
                 std::uint8_t coverage) noexcept {
    if (coverage == 0u) {
        return;
    }
    if (x >= bitmap.width() || y >= bitmap.height()) {
        return;
    }
    if (coverage == 255u) {
        put_pixel(bitmap, static_cast<double>(x), static_cast<double>(y), color);
        return;
    }
    const double a = static_cast<double>(coverage) / 255.0;
    const double ia = 1.0 - a;
    const auto mix = [a, ia](std::uint8_t dst, std::uint8_t src) noexcept {
        const double v = static_cast<double>(src) * a + static_cast<double>(dst) * ia;
        const double rounded = v + 0.5;
        const long clamped =
            rounded < 0.0 ? 0 : (rounded > 255.0 ? 255 : static_cast<long>(rounded));
        return static_cast<std::uint8_t>(clamped);
    };
    bitmap.set(x, y, 0, mix(bitmap.at(x, y, 0), color.r));
    bitmap.set(x, y, 1, mix(bitmap.at(x, y, 1), color.g));
    bitmap.set(x, y, 2, mix(bitmap.at(x, y, 2), color.b));
    if (bitmap.channels() == 4) {
        bitmap.set(x, y, 3, 255);
    }
}

// ── 后端一：BitmapBackend（内置 5×7 点阵）──────────────────────────
// 覆盖 0x20..0x7E；其余码点返回 false，把决定权交回组合链。
class BitmapBackend final : public FontBackend {
public:
    [[nodiscard]] bool has_glyph(char32_t codepoint) const noexcept override {
        return codepoint >= kFirstCp && codepoint <= kLastCp;
    }

    [[nodiscard]] double measure_width(char32_t codepoint, double cell_w,
                                       double cell_h) const noexcept override {
        (void)cell_h;
        return static_cast<double>(util::codepoint_width(codepoint)) * cell_w;
    }

    [[nodiscard]] bool draw_glyph(image::Bitmap& bitmap, char32_t codepoint, Point at,
                                  const Rgb& color, int columns, double cell_w,
                                  double cell_h) const noexcept override {
        (void)columns;  // ASCII 恒占 1 列，格内缩放与 columns 无关
        if (!has_glyph(codepoint)) {
            return false;
        }
        const auto& glyph = kFont5x7[static_cast<std::size_t>(codepoint - kFirstCp)];
        for (int col = 0; col < kGlyphCols; ++col) {
            const std::uint8_t bits = glyph[static_cast<std::size_t>(col)];
            if (bits == 0u) {
                continue;  // 空列（如 '1' 的首尾列）直接跳过
            }
            // 字形画在前 5/6 格宽：格内偏移先取整，格间 advance == cell_w 整数对齐。
            const double px = std::floor(at.x + static_cast<double>(col) * cell_w / 6.0);
            for (int row = 0; row < kGlyphRows; ++row) {
                if (((bits >> row) & 1u) != 0u) {
                    const double py = std::floor(at.y + static_cast<double>(row) * cell_h / 8.0);
                    put_pixel(bitmap, px, py, color);
                }
            }
        }
        return true;
    }
};

// ── 后端二：TtfBackend（系统 TTF，封装 image::TtfFont）──────────────
constexpr std::size_t kMaxFontFileBytes = 64u << 20;  // 字体文件体积上限（防病态大文件）

// 平台系统字体候选，按序逐个解析，全部能解析的都收入 font family（顺序即优先
// 级）。Latin 字体放前面：图表轴刻度 / OHLC / 标题等英文与数字优先用系统英文字体
// 矢量光栅化，清晰且带字距；CJK 字体放后面兜底汉字。.ttc 集合由 TtfFont::load
// 取首个字体（微软雅黑即 ttc）。路径全部 ASCII，窄字符串 ifstream 即可。
#ifdef _WIN32
constexpr const char* kSystemFontCandidates[] = {
    // 英文 / Latin 优先：图表文本主要就是英文与数字，用系统英文字体才清晰
    "C:/Windows/Fonts/segoeui.ttf",  // Segoe UI（现代 Windows UI 字体）
    "C:/Windows/Fonts/arial.ttf",    // Arial
    "C:/Windows/Fonts/calibri.ttf",  // Calibri
    "C:/Windows/Fonts/consola.ttf",  // Consolas（等宽，轴数字更整齐）
    "C:/Windows/Fonts/tahoma.ttf",   // Tahoma
    // CJK 兜底：Latin 字体不含汉字时，靠后的 CJK 字体供给
    "C:/Windows/Fonts/msyh.ttc",     // 微软雅黑（ttc 集合）
    "C:/Windows/Fonts/simhei.ttf",   // 黑体
    "C:/Windows/Fonts/simsun.ttc",   // 宋体
};
#elif defined(__APPLE__)
constexpr const char* kSystemFontCandidates[] = {
    "/System/Library/Fonts/Helvetica.ttc",   // Latin
    "/System/Library/Fonts/SFNS.ttf",        // San Francisco（macOS UI 字体）
    "/System/Library/Fonts/PingFang.ttc",    // CJK
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/Library/Fonts/Microsoft/msyh.ttc",
};
#else
constexpr const char* kSystemFontCandidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",  // 等宽
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",  // CJK
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    "/usr/share/fonts/wenquanyi/wqy-zenhei/wqy-zenhei.ttc",
};
#endif

// 读整个文件；ifstream 默认不开异常，读写失败只置流状态，不会抛。
[[nodiscard]] bool read_file_bytes(const char* path, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0 || static_cast<std::uint64_t>(size) > kMaxFontFileBytes) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return file.gcount() == static_cast<std::streamsize>(size);
}

class TtfBackend final : public FontBackend {
public:
    // 构造即做系统字体发现：运行时 set_font_file() 指定的路径优先（单字体，独占
    // 候选清单）；其次环境变量 TERMCHART_GUI_FONT；随后按平台候选清单逐个解析，
    // 全部能解析的都收入 font family（不去重、顺序即优先级）——首字体优先供给
    // Latin，靠后的 CJK 字体兜底 CJK 码点。
    // 任何一步失败都只减少可用字体，active_ 在至少有一个成功解析时为 true；
    // 之后本后端对"找不到轮廓的码点"返回 false，组合链自然降级到点阵 / 占位框。
    TtfBackend() noexcept {
        try {
            std::vector<std::uint8_t> bytes;
            // 运行时覆盖优先（set_font_file），其次环境变量，最后系统候选清单。
            if (g_font_override.has_value() && !g_font_override->empty()) {
                if (read_file_bytes(g_font_override->c_str(), bytes)) {
                    if (auto loaded = image::TtfFont::load(bytes); loaded.has_value()) {
                        fonts_.push_back(std::move(*loaded));
                        active_ = true;
                    }
                }
                return;  // 显式指定字体即独占，不再扫候选清单 / 环境变量
            }
            if (const char* env = std::getenv("TERMCHART_GUI_FONT");
                env != nullptr && *env != '\0') {
                if (read_file_bytes(env, bytes)) {
                    if (auto loaded = image::TtfFont::load(bytes); loaded.has_value()) {
                        fonts_.push_back(std::move(*loaded));
                        active_ = true;
                    }
                }
                return;  // 显式指定字体即独占，不再扫候选清单
            }
            for (const char* path : kSystemFontCandidates) {
                bytes.clear();
                if (!read_file_bytes(path, bytes)) {
                    continue;
                }
                if (auto loaded = image::TtfFont::load(bytes); loaded.has_value()) {
                    fonts_.push_back(std::move(*loaded));
                }
            }
            active_ = !fonts_.empty();
        } catch (...) {
            active_ = false;  // OOM 等意外：退回点阵，绝不把异常带出 noexcept
        }
    }

    [[nodiscard]] bool has_glyph(char32_t codepoint) const noexcept override {
        // 保守声明：active 时声明覆盖全部码点（含 ASCII）。"这个码点有没有轮廓"
        // 要光栅化才知道（TtfFont 无 cmap 覆盖查询），由 draw_glyph 实测决定
        // 胜出；组合链按 draw 结果委派，不依赖本函数的精确性。
        (void)codepoint;
        return active_;
    }

    [[nodiscard]] double measure_width(char32_t codepoint, double cell_w,
                                       double cell_h) const noexcept override {
        (void)cell_h;
        return static_cast<double>(util::codepoint_width(codepoint)) * cell_w;
    }

    [[nodiscard]] bool draw_glyph(image::Bitmap& bitmap, char32_t codepoint, Point at,
                                  const Rgb& color, int columns, double cell_w,
                                  double cell_h) const noexcept override {
        if (!active_ || columns <= 0) {
            return false;
        }
        const double box_x = at.x;
        const double box_y = at.y;
        const double box_w = static_cast<double>(columns) * cell_w;
        const double box_h = cell_h;
        if (!(box_w > 0.0) || !(box_h > 0.0)) {
            return false;
        }
        // 字号线盒 ≈ 格高；宽字符盒是 2 格宽，CJK 字形 ~0.9em 不会横向溢出。
        double pixel_size = box_h * 0.92;
        for (image::TtfFont& font : fonts_) {
            auto glyph = font.glyph(codepoint, pixel_size);
            if (!glyph.has_value()) {
                continue;
            }
            // 窄盒里的宽 ink（全角符号、粗衬线）：按盒比例缩一档重取（cache 按
            // 0.25px 量化，重取廉价且确定）。
            const std::uint32_t gw = glyph->bitmap.width();
            const std::uint32_t gh = glyph->bitmap.height();
            if (gw != 0u &&
                (static_cast<double>(gw) > box_w || static_cast<double>(gh) > box_h)) {
                const double kw = box_w / static_cast<double>(gw);
                const double kh = box_h / static_cast<double>(gh);
                const double ps = pixel_size * std::min(kw, kh);
                if (!(ps >= 1.0)) {
                    continue;  // 本字体在该盒放不下，试下一个字体
                }
                glyph = font.glyph(codepoint, ps);
                if (!glyph.has_value()) {
                    continue;
                }
            }
            const std::uint32_t w = glyph->bitmap.width();
            const std::uint32_t h = glyph->bitmap.height();
            if (w == 0u || h == 0u) {
                continue;  // 空轮廓（.notdef 空白等）：试下一个字体
            }
            const double tx = std::floor(box_x + (box_w - static_cast<double>(w)) / 2.0);
            const double ty = std::floor(box_y + (box_h - static_cast<double>(h)) / 2.0);
            const bool four_channel = glyph->bitmap.channels() == 4u;
            const double max_x = static_cast<double>(bitmap.width());
            const double max_y = static_cast<double>(bitmap.height());
            for (std::uint32_t gy = 0; gy < h; ++gy) {
                const double py = ty + static_cast<double>(gy);
                if (!(py >= 0.0) || py >= max_y) {
                    continue;  // 负坐标/NaN/越界一律跳过（Bitmap 对越界写是丢弃语义）
                }
                for (std::uint32_t gx = 0; gx < w; ++gx) {
                    const double px = tx + static_cast<double>(gx);
                    if (!(px >= 0.0) || px >= max_x) {
                        continue;
                    }
                    const std::uint8_t coverage =
                        four_channel
                            ? glyph->bitmap.at(gx, gy, 3)
                            : ((glyph->bitmap.at(gx, gy, 0) | glyph->bitmap.at(gx, gy, 1) |
                                glyph->bitmap.at(gx, gy, 2)) != 0u
                                   ? 255u
                                   : 0u);
                    if (coverage == 0u) {
                        continue;
                    }
                    blend_pixel(bitmap, static_cast<std::uint32_t>(px),
                                static_cast<std::uint32_t>(py), color, coverage);
                }
            }
            return true;  // 本字体命中：画完即胜出，组合链不再下探
        }
        return false;  // 所有字体都没覆盖 → 组合链下探到 Bitmap / 占位框
    }

private:
    mutable std::vector<image::TtfFont> fonts_;  // glyph() 写 cache：逻辑 const（memoization）
    bool active_ = false;
};

// ── 后端三：FallbackBackend（有序组合链）───────────────────────────
// 依次问每个孩子，第一个 draw_glyph 返回 true 的胜出；全不覆盖返回
// false，由 draw_text 画占位框。典型装配：{ TtfBackend, BitmapBackend }
// ——TTF 优先、点阵兜底（TTF 整体 inactive 时等价于纯点阵 + 占位框）。
class FallbackBackend final : public FontBackend {
public:
    explicit FallbackBackend(std::vector<std::unique_ptr<FontBackend>> children) noexcept
        : children_(std::move(children)) {}

    [[nodiscard]] bool has_glyph(char32_t codepoint) const noexcept override {
        for (const auto& child : children_) {
            if (child->has_glyph(codepoint)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] double measure_width(char32_t codepoint, double cell_w,
                                       double cell_h) const noexcept override {
        // 等宽网格契约：宽度与后端无关（列数 × cell_w）。委派给第一个
        // 声明覆盖的孩子；无人覆盖时用首节点——度量不许因字体缺失而漂移，
        // 否则排版会因环境不同而歪。
        for (const auto& child : children_) {
            if (child->has_glyph(codepoint)) {
                return child->measure_width(codepoint, cell_w, cell_h);
            }
        }
        return children_.empty() ? 0.0 : children_.front()->measure_width(codepoint, cell_w,
                                                                          cell_h);
    }

    [[nodiscard]] bool draw_glyph(image::Bitmap& bitmap, char32_t codepoint, Point at,
                                  const Rgb& color, int columns, double cell_w,
                                  double cell_h) const noexcept override {
        for (const auto& child : children_) {
            if (child->draw_glyph(bitmap, codepoint, at, color, columns, cell_w, cell_h)) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::unique_ptr<FontBackend>> children_;
};

// 组合链的构建点：TtfBackend 优先、BitmapBackend 兜底（TTF 整体 inactive 时
// 等价于纯点阵 + 占位框）。抽出为函数以便"运行时改字体"时重建。
[[nodiscard]] std::unique_ptr<FontBackend> build_font_chain() noexcept {
    std::vector<std::unique_ptr<FontBackend>> children;
    children.push_back(std::make_unique<TtfBackend>());
    children.push_back(std::make_unique<BitmapBackend>());
    return std::make_unique<FallbackBackend>(std::move(children));
}

// 进程内单例：组合链的唯一装配点。加/换后端只改 build_font_chain()，公开 API 与
// 调用方零改动。字体加载的异常已在 TtfBackend 构造内兜住；这里 catch 是对分配
// 失败的双保险。运行时 set_font_file/clear_font_file 会置 g_font_dirty 触发重建。
[[nodiscard]] FontBackend& font_chain() noexcept {
    std::lock_guard<std::mutex> lock(g_chain_mutex);
    if (!g_chain_built || g_font_dirty) {
        try {
            g_chain = build_font_chain();
        } catch (...) {
            g_chain = std::make_unique<BitmapBackend>();  // 极端 OOM：纯点阵
        }
        g_chain_built = true;
        g_font_dirty = false;
    }
    return *g_chain;
}

}  // namespace

// vtable/typeinfo 锚点：放 TU 内，匿名 namespace 的派生类各自只发弱表，
// 这里出强表，避免每个包含头文件的 TU 都 emission 一份（key function 惯例）。
FontBackend::~FontBackend() = default;

bool has_glyph(char32_t codepoint) noexcept {
    // 契约不变：只回答"内置点阵表是否覆盖"（0x20..0x7E）——TTF 命中不经
    // 此查询（测试钉住的语义），TTF 能力走 draw_text 的组合链。
    return codepoint >= kFirstCp && codepoint <= kLastCp;
}

TextMetrics measure_text(std::string_view utf8_text, double cell_w, double cell_h) noexcept {
    const double cw = sane_extent(cell_w);
    const double ch = sane_extent(cell_h);
    const FontBackend& chain = font_chain();
    // 逐码点解码累加宽度：与 util::display_width 同口径（主码点决定宽度，
    // 组合记号宽 0，非法字节按 U+FFFD 计 1 列）；高度与内容无关恒为 ch。
    double width = 0.0;
    std::size_t pos = 0;
    while (pos < utf8_text.size()) {
        const util::Utf8Decode d = util::decode(utf8_text, pos);
        if (d.consumed == 0) {
            break;
        }
        pos += static_cast<std::size_t>(d.consumed);
        width += chain.measure_width(d.cp, cw, ch);
    }
    return TextMetrics{width, ch};
}

double draw_text(image::Bitmap& bitmap, std::string_view utf8_text, Point at, Rgb color,
                 double cell_w, double cell_h) noexcept {
    const double cw = sane_extent(cell_w);
    const double ch = sane_extent(cell_h);
    FontBackend& chain = font_chain();
    int cols = 0;  // 已消费的列数：当前码点所在格相对 at.x 的整格偏移
    std::size_t pos = 0;
    while (pos < utf8_text.size()) {
        const util::Utf8Decode d = util::decode(utf8_text, pos);
        if (d.consumed == 0) {
            break;
        }
        pos += static_cast<std::size_t>(d.consumed);
        const int columns = util::codepoint_width(d.cp);
        if (columns > 0) {
            // 组合链：TTF 优先、点阵兜底；全不覆盖才画占位框（可见、可度量）。
            const Point cell_at{at.x + static_cast<double>(cols) * cw, at.y};
            if (!chain.draw_glyph(bitmap, d.cp, cell_at, color, columns, cw, ch)) {
                draw_placeholder(bitmap, cell_at.x, cell_at.y, columns, cw, ch, color);
            }
        }
        // columns == 0（组合记号 / 控制字符）：不占格、不画，跟随前一字符。
        cols += columns;
    }
    return static_cast<double>(cols) * cw;
}

// 运行时指定 GUI 文本使用的 TTF 字体文件路径：优先于环境变量
// TERMCHART_GUI_FONT 与系统字体候选清单。须在首次绘制前调用，也可运行中随时
// 切换（下次绘制即生效）。路径为空 = 清除覆盖，回落到环境变量 / 系统字体。
void set_font_file(std::string path) {
    std::lock_guard<std::mutex> lock(g_chain_mutex);
    g_font_override = std::move(path);
    g_font_dirty = true;
}

void clear_font_file() noexcept {
    std::lock_guard<std::mutex> lock(g_chain_mutex);
    g_font_override.reset();
    g_font_dirty = true;
}

}  // namespace termchart::gui
