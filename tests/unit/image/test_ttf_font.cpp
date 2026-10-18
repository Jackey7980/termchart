// test_ttf_font.cpp —— image/ttf_font：自主 TTF 解析 + 光栅化
//
// 夹具是**程序内构造的合成 sfnt**（一个方形轮廓的 'A' + 一个空 ' '），
// 于是断言可以精确到像素：方形 200x200 字体单位 @1000px → 202x202 位图，
// 中心 255、四角 0、left/top 与步进都可由构造值算出。
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/image/ttf_font.hpp"

namespace {

using termchart::ErrorCode;
using termchart::image::TtfFont;
using termchart::image::TtfGlyph;
using termchart::image::TtfMetrics;
using termchart::Result;

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}
void put_i16(std::vector<std::uint8_t>& v, std::int16_t x) {
    put_u16(v, static_cast<std::uint16_t>(x));
}
void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 24));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

// 合成字体：'A' = 方形 (200,0)-(400,200)，' ' = 空。unitsPerEm=1000。
// hmtx 步进：.notdef=500，'A'=600，' '=250。
std::vector<std::uint8_t> build_font() {
    std::vector<std::uint8_t> glyf;
    // glyph 0（.notdef）：空轮廓
    put_i16(glyf, 0);
    put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0);
    // glyph 1（'A'）：简单轮廓，1 个轮廓 4 个 on 点（方形）
    put_i16(glyf, 1);  // numberOfContours
    put_i16(glyf, 200); put_i16(glyf, 0); put_i16(glyf, 400); put_i16(glyf, 200);  // bbox
    put_u16(glyf, 3);   // endPtsOfContours[0]
    put_u16(glyf, 0);   // instructionLength
    // flags ×4：每点**一字节**；0x01 = on-curve，x/y 均为 2 字节长向量
    for (int i = 0; i < 4; ++i) {
        glyf.push_back(0x01);
    }
    const std::int16_t xs[4] = {200, 200, 0, -200};
    const std::int16_t ys[4] = {0, 0, 200, 0};
    for (int i = 0; i < 4; ++i) {
        put_i16(glyf, xs[i]);
    }
    for (int i = 0; i < 4; ++i) {
        put_i16(glyf, ys[i]);
    }
    // glyph 2（' '）：空轮廓
    put_i16(glyf, 0);
    put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0);
    while (glyf.size() % 2 != 0) {
        glyf.push_back(0);  // short loca 要求偶偏移
    }

    const std::uint32_t g0 = 0;
    const std::uint32_t g1 = 10;
    const std::uint32_t g2 = static_cast<std::uint32_t>(glyf.size()) - 10;
    const std::uint32_t g3 = static_cast<std::uint32_t>(glyf.size());

    std::vector<std::uint8_t> loca;
    put_u16(loca, static_cast<std::uint16_t>(g0 / 2));
    put_u16(loca, static_cast<std::uint16_t>(g1 / 2));
    put_u16(loca, static_cast<std::uint16_t>(g2 / 2));
    put_u16(loca, static_cast<std::uint16_t>(g3 / 2));

    std::vector<std::uint8_t> head(54, 0);
    head[0] = 0; head[1] = 1; head[2] = 0; head[3] = 0;          // version 1.0
    head[12] = 0x5F; head[13] = 0x0F; head[14] = 0x3C; head[15] = 0xF5;  // magic
    head[18] = 0x03; head[19] = 0xE8;                            // unitsPerEm = 1000
    head[50] = 0; head[51] = 0;                                  // indexToLocFormat = short

    std::vector<std::uint8_t> hhea(36, 0);
    hhea[0] = 0; hhea[1] = 1; hhea[2] = 0; hhea[3] = 0;  // version
    hhea[4] = 0x03; hhea[5] = 0x20;                      // ascender = 800
    hhea[6] = 0xFF; hhea[7] = 0x38;                      // descender = -200
    hhea[34] = 0; hhea[35] = 3;                          // numberOfHMetrics = 3

    std::vector<std::uint8_t> hmtx;
    put_u16(hmtx, 500); put_i16(hmtx, 0);  // .notdef
    put_u16(hmtx, 600); put_i16(hmtx, 0);  // 'A'
    put_u16(hmtx, 250); put_i16(hmtx, 0);  // ' '

    std::vector<std::uint8_t> maxp;
    put_u32(maxp, 0x00005000);  // version 0.5
    put_u16(maxp, 3);           // numGlyphs

    // cmap：format 4，三段：0x20→' '(2)，0x41→'A'(1)，0xFFFF→.notdef(0)
    std::vector<std::uint8_t> sub4;
    put_u16(sub4, 4);        // format
    put_u16(sub4, 40);       // length = 14 + 8 + 8*3
    put_u16(sub4, 0);        // language
    put_u16(sub4, 6);        // segCountX2 = 3 段
    put_u16(sub4, 12);       // searchRange
    put_u16(sub4, 2);        // entrySelector
    put_u16(sub4, 0);        // rangeShift
    put_u16(sub4, 0x20);     // endCode[0]
    put_u16(sub4, 0x41);     // endCode[1]
    put_u16(sub4, 0xFFFF);   // endCode[2]
    put_u16(sub4, 0);        // reservedPad
    put_u16(sub4, 0x20);     // startCode[0]
    put_u16(sub4, 0x41);     // startCode[1]
    put_u16(sub4, 0xFFFF);   // startCode[2]
    put_i16(sub4, static_cast<std::int16_t>(2 - 0x20));   // idDelta[0]
    put_i16(sub4, static_cast<std::int16_t>(1 - 0x41));   // idDelta[1]
    put_i16(sub4, 1);                                       // idDelta[2]
    put_u16(sub4, 0);        // idRangeOffset[0]
    put_u16(sub4, 0);        // idRangeOffset[1]
    put_u16(sub4, 0);        // idRangeOffset[2]

    std::vector<std::uint8_t> cmap;
    put_u16(cmap, 0);  // version
    put_u16(cmap, 1);  // numTables
    put_u16(cmap, 3);  // platformID = 3 (Windows)
    put_u16(cmap, 1);  // encodingID = 1 (BMP)
    put_u32(cmap, 12); // offset
    cmap.insert(cmap.end(), sub4.begin(), sub4.end());

    struct Entry {
        const char* tag;
        const std::vector<std::uint8_t>* data;
    };
    const Entry entries[] = {
        {"cmap", &cmap}, {"glyf", &glyf}, {"head", &head}, {"hhea", &hhea},
        {"hmtx", &hmtx}, {"loca", &loca}, {"maxp", &maxp},
    };
    const std::size_t num_tables = sizeof(entries) / sizeof(entries[0]);

    // 目录 12 + 16*n，表体 4 字节对齐依次排布。
    std::vector<std::uint8_t> font;
    font.reserve(12 + num_tables * 16 + 4096);
    put_u32(font, 0x00010000);  // sfnt version
    put_u16(font, static_cast<std::uint16_t>(num_tables));
    put_u16(font, 0);  // searchRange（未用）
    put_u16(font, 0);  // entrySelector
    put_u16(font, 0);  // rangeShift
    std::size_t offset = 12 + num_tables * 16;
    for (const Entry& e : entries) {
        put_u32(font, (static_cast<std::uint32_t>(e.tag[0]) << 24) |
                          (static_cast<std::uint32_t>(e.tag[1]) << 16) |
                          (static_cast<std::uint32_t>(e.tag[2]) << 8) |
                          static_cast<std::uint32_t>(e.tag[3]));
        put_u32(font, 0);                                      // checksum（未校验）
        put_u32(font, static_cast<std::uint32_t>(offset));
        put_u32(font, static_cast<std::uint32_t>(e.data->size()));
        offset = (offset + e.data->size() + 3) & ~static_cast<std::size_t>(3);
    }
    for (const Entry& e : entries) {
        font.insert(font.end(), e.data->begin(), e.data->end());
        while (font.size() % 4 != 0) {
            font.push_back(0);
        }
    }
    return font;
}

TEST_CASE("image/ttf: 合成字体加载与度量") {
    const std::vector<std::uint8_t> bytes = build_font();
    Result<TtfFont> font = TtfFont::load(bytes);
    REQUIRE(font.has_value());

    const TtfMetrics m = font->metrics(1000.0);
    CHECK(m.ascent == 800.0);
    CHECK(m.descent == -200.0);
    CHECK(m.line_gap == 0.0);
    CHECK(m.line_height() == 1000.0);

    CHECK(font->advance(U'A', 1000.0) == 600.0);
    CHECK(font->advance(U' ', 1000.0) == 250.0);
    CHECK(font->advance(U'B', 1000.0) == 500.0);  // 未收录 → .notdef
    CHECK(font->advance(U'A', 500.0) == 300.0);   // 半尺寸线性缩放
}

TEST_CASE("image/ttf: 方形 'A' 光栅化——尺寸/覆盖率/偏移/步进") {
    const std::vector<std::uint8_t> bytes = build_font();
    Result<TtfFont> font = TtfFont::load(bytes);
    REQUIRE(font.has_value());

    const Result<TtfGlyph> g = font->glyph(U'A', 1000.0);
    REQUIRE(g.has_value());
    // 200x200 单位 → 像素 200x200，四周各留 1px 极值余量。
    CHECK(g->bitmap.width() == 202);
    CHECK(g->bitmap.height() == 202);
    CHECK(g->bitmap.channels() == 4);  // RGBA：RGB 恒白，覆盖率走 A
    CHECK(g->left == 199);
    CHECK(g->top == -201);
    CHECK(g->advance_x == 600.0);

    // 中心必全覆盖，四角（含 1px 极值余量列）必透明。
    CHECK(g->bitmap.at(101, 101, 3) == 255);
    CHECK(g->bitmap.at(0, 0, 3) == 0);
    CHECK(g->bitmap.at(0, 101, 3) == 0);  // 左余量列恒透明
    CHECK(g->bitmap.at(201, 0, 3) == 0);
    CHECK(g->bitmap.at(0, 201, 3) == 0);
    CHECK(g->bitmap.at(201, 201, 3) == 0);

    // 边缘抗锯齿：非整数像素尺寸下，左缘所在列必须出现"半覆盖"。
    const Result<TtfGlyph> aa = font->glyph(U'A', 777.0);
    REQUIRE(aa.has_value());
    bool saw_partial = false;
    for (std::uint32_t x = 0; x < aa->bitmap.width(); ++x) {
        const std::uint8_t a = aa->bitmap.at(x, aa->bitmap.height() / 2, 3);
        if (a > 0 && a < 255) {
            saw_partial = true;
            break;
        }
    }
    CHECK(saw_partial);

    // cache 命中：第二次取到同一内容（值返回，逐通道比对）。
    const Result<TtfGlyph> again = font->glyph(U'A', 1000.0);
    REQUIRE(again.has_value());
    CHECK(again->bitmap.width() == g->bitmap.width());
    CHECK(again->bitmap.at(101, 101, 3) == 255);

    // 亚 0.25px 差别的尺寸不同 key，各渲各的（都该成功）。
    CHECK(font->glyph(U'A', 1000.1).has_value());
    CHECK(font->glyph(U'A', 1000.2).has_value());
}

TEST_CASE("image/ttf: 空字形与缺失码点回退") {
    const std::vector<std::uint8_t> bytes = build_font();
    Result<TtfFont> font = TtfFont::load(bytes);
    REQUIRE(font.has_value());

    const Result<TtfGlyph> space = font->glyph(U' ', 1000.0);
    REQUIRE(space.has_value());
    CHECK(space->bitmap.empty());       // 无轮廓 → 空位图
    CHECK(space->advance_x == 250.0);

    const Result<TtfGlyph> notdef = font->glyph(U'Z', 1000.0);
    REQUIRE(notdef.has_value());
    CHECK(notdef->bitmap.empty());      // 未收录 → .notdef（本夹具也是空的）
    CHECK(notdef->advance_x == 500.0);
}

TEST_CASE("image/ttf: 非法输入显式报错") {
    // 空 / 太短
    CHECK_FALSE(TtfFont::load("").has_value());
    CHECK(TtfFont::load("").error() == ErrorCode::ParseError);
    CHECK_FALSE(TtfFont::load("0x00010000").has_value());

    // OTTO（CFF）→ Unsupported，不是 ParseError
    std::string otto = "OTTO";
    otto.append(64, '\0');
    Result<TtfFont> cff = TtfFont::load(otto);
    CHECK_FALSE(cff.has_value());
    CHECK(cff.error() == ErrorCode::Unsupported);

    // 表目录声明越界 → ParseError
    std::vector<std::uint8_t> bad = build_font();
    // numTables 改成 999：目录远超缓冲。
    bad[4] = 0x03;
    bad[5] = 0xE7;
    Result<TtfFont> overrun = TtfFont::load(bad);
    CHECK_FALSE(overrun.has_value());
    CHECK(overrun.error() == ErrorCode::ParseError);

    // 表的 length 声明巨大 → table_ptr 拒绝 → 解析失败（不会越界读）
    std::vector<std::uint8_t> huge = build_font();
    // 目录第 0 项（cmap）的 length 字段高位：tag 12-15 / checksum 16-19 /
    // offset 20-23 / length 24-27 → 高位字节是 24。
    huge[24] = 0x7F;
    Result<TtfFont> huge_len = TtfFont::load(huge);
    CHECK_FALSE(huge_len.has_value());
    CHECK(huge_len.error() == ErrorCode::ParseError);
}

}  // namespace
