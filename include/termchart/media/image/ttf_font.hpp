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
#ifndef TERMCHART_IMAGE_TTF_FONT_HPP
#define TERMCHART_IMAGE_TTF_FONT_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "termchart/core/expected.hpp"
#include "termchart/core/point.hpp"
#include "termchart/export.hpp"
#include "termchart/media/image/bitmap.hpp"

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

// 字形矢量轮廓（文字轮廓化 / text-to-outline 用）。
// contours：每个闭合轮廓的扁平化多边形点（像素空间，y 向下；原点 = 笔触基线原点，
//   字形主体在基线上方 → y 为负）。已把 glyf 的二次贝塞尔展平成折线，可直接喂给
//   SVG <path>/PDF 路径算子。
// advance_x：排版步进宽度（像素）。left/top：轮廓外接框相对笔触原点的偏移
//   （像素，top 通常为负，因字形位于基线上方），与 TtfGlyph 同义，便于定位。
struct GlyphOutline {
    std::vector<std::vector<Point>> contours;
    double advance_x = 0.0;
    std::int32_t left = 0;
    std::int32_t top = 0;
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

    // 取字形矢量轮廓（带 cache；码点缺失回退 .notdef，再不行回退空轮廓）。
    // 与 glyph() 同源：glyf 轮廓 → 像素空间（y 翻转）→ 二次贝塞尔展平成多边形。
    // 用于把文字转成自包含矢量路径（SVG/PDF 文字轮廓化），不再依赖渲染端字体。
    [[nodiscard]] Result<GlyphOutline> glyph_outline(char32_t codepoint,
                                                     double pixel_size) const noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    explicit TtfFont(std::shared_ptr<Impl> impl) noexcept;
};

}  // namespace termchart::image

#endif  // TERMCHART_IMAGE_TTF_FONT_HPP
