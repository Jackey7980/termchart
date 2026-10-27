// synthetic_font.hpp —— 确定性合成 TTF 字节流构造器（仅供测试）。
//
// 'A' = 方形轮廓 (200,0)-(400,200)；' ' = 空轮廓。unitsPerEm = 1000。
// hmtx 步进：.notdef = 500，'A' = 600，' ' = 250。
// 几何完全确定，不依赖任何真实字体文件 —— 用来测试 ttf_font 解析与文字轮廓化。
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace termchart::testing {

inline void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}
inline void put_i16(std::vector<std::uint8_t>& v, std::int16_t x) {
    put_u16(v, static_cast<std::uint16_t>(x));
}
inline void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 24));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

inline std::vector<std::uint8_t> build_font() {
    std::vector<std::uint8_t> glyf;
    // glyph 0（.notdef）：空轮廓
    put_i16(glyf, 0);
    put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0); put_i16(glyf, 0);
    // glyph 1（'A'）：简单轮廓，1 个轮廓 4 个 on 点（方形）
    put_i16(glyf, 1);  // numberOfContours
    put_i16(glyf, 200); put_i16(glyf, 0); put_i16(glyf, 400); put_i16(glyf, 200);  // bbox
    put_u16(glyf, 3);   // endPtsOfContours[0]
    put_u16(glyf, 0);   // instructionLength
    // flags ×4：每点一字节；0x01 = on-curve，x/y 均为 2 字节长向量
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

}  // namespace termchart::testing
