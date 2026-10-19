// test_bmp.cpp —— image/bmp：自主 BMP 解码
//
// 夹具是**程序内构造的 BMP 字节**（与 test_ttf_font.cpp 同一思路）：
// 24bpp 已对齐/需补齐、8bpp 调色板、top-down 负高、BITFIELDS 32bpp 带 alpha、
// 以及各条错误路径。
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "termchart/image/bmp.hpp"

namespace {

using termchart::ErrorCode;
using termchart::image::Bitmap;
using termchart::image::decode_bmp;

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
}
void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
}
void put_i32(std::vector<std::uint8_t>& v, std::int32_t x) { put_u32(v, static_cast<std::uint32_t>(x)); }

// 通用的 40 字节 BITMAPINFOHEADER。
void put_info_header(std::vector<std::uint8_t>& v, std::int32_t w, std::int32_t h,
                     std::uint16_t bpp, std::uint32_t compression, std::uint32_t clr_used) {
    put_u32(v, 40);          // biSize
    put_i32(v, w);           // biWidth
    put_i32(v, h);           // biHeight（负 = top-down）
    put_u16(v, 1);           // biPlanes
    put_u16(v, bpp);         // biBitCount
    put_u32(v, compression); // biCompression
    put_u32(v, 0);           // biSizeImage（BI_RGB 可为 0）
    put_i32(v, 2835);        // biXPelsPerMeter
    put_i32(v, 2835);        // biYPelsPerMeter
    put_u32(v, clr_used);    // biClrUsed
    put_u32(v, 0);           // biClrImportant
}

// 收尾：写文件头（'BM' + 偏移）。`body_pixel_start` 是像素数据在"文件头之后那一段
// 主体"内的起始偏移（即信息头 + 调色板长度），绝对像素偏移 = 14 + body_pixel_start。
void put_file_header(std::vector<std::uint8_t>& v, std::uint32_t body_pixel_start) {
    std::vector<std::uint8_t> head;
    head.push_back('B');
    head.push_back('M');
    put_u32(head, static_cast<std::uint32_t>(v.size() + 14));  // 文件总大小
    put_u32(head, 0);                                          // 保留
    put_u32(head, 14 + body_pixel_start);                      // 像素数据绝对偏移
    v.insert(v.begin(), head.begin(), head.end());
}

TEST_CASE("image/bmp: 24bpp bottom-up，行 4 字节对齐") {
    std::vector<std::uint8_t> bmp;
    put_info_header(bmp, 3, 2, 24, 0, 0);
    // 3px × 3 字节 = 9 字节 → 行补齐到 12。
    // 数据行顺序 bottom-up：先末行（图像的 y=1）再首行（y=0）。
    const std::uint8_t row0[] = {0, 0, 255,   0, 255, 0,   255, 0, 0,  0, 0, 0};   // 红 绿 蓝 + pad
    const std::uint8_t row1[] = {255, 255, 255, 128, 128, 128, 0, 0, 0, 0, 0, 0};  // 白 灰 黑 + pad
    bmp.insert(bmp.end(), row1, row1 + 12);  // bottom-up：先 y=1
    bmp.insert(bmp.end(), row0, row0 + 12);
    put_file_header(bmp, 40);                // 信息头 40 字节后接像素

    const auto img = decode_bmp(bmp);
    REQUIRE(img.has_value());
    CHECK(img->width() == 3);
    CHECK(img->height() == 2);
    CHECK(img->channels() == 4);
    // y=0 行：红、绿、蓝
    CHECK(img->at(0, 0, 0) == 255);
    CHECK(img->at(0, 0, 1) == 0);
    CHECK(img->at(0, 0, 2) == 0);
    CHECK(img->at(0, 0, 3) == 255);
    CHECK(img->at(1, 0, 1) == 255);
    CHECK(img->at(2, 0, 2) == 255);
    // y=1 行：白、灰、黑
    CHECK(img->at(0, 1, 0) == 255);
    CHECK(img->at(0, 1, 1) == 255);
    CHECK(img->at(1, 1, 0) == 128);
    CHECK(img->at(2, 1, 3) == 255);  // 24bpp 无 alpha → 不透明
}

TEST_CASE("image/bmp: 8bpp 调色板 + 4bpp + 1bpp 索引") {
    // 8bpp：调色板两项（索引 0 黑、1 白），像素 0,1,1,0。
    {
        std::vector<std::uint8_t> bmp;
        put_info_header(bmp, 4, 1, 8, 0, 2);
        put_u32(bmp, 0x00000000);  // BGRA 黑
        put_u32(bmp, 0x00FFFFFF);  // BGRA 白
        const std::uint8_t pixels[] = {0, 1, 1, 0};
        bmp.insert(bmp.end(), pixels, pixels + 4);
        put_file_header(bmp, 48);  // 40 + 调色板 8
        const auto img = decode_bmp(bmp);
        REQUIRE(img.has_value());
        CHECK(img->at(0, 0, 0) == 0);
        CHECK(img->at(1, 0, 0) == 255);
        CHECK(img->at(2, 0, 0) == 255);
        CHECK(img->at(3, 0, 0) == 0);
    }
    // 4bpp：一个字节两个索引（高 nibble 先）。行补齐到 4 字节。
    {
        std::vector<std::uint8_t> bmp;
        put_info_header(bmp, 2, 1, 4, 0, 2);
        put_u32(bmp, 0x00000000);  // 黑
        put_u32(bmp, 0x000000FF);  // 蓝（BGR：B 在低位 → 0x000000FF 即 B=255）
        const std::uint8_t pixels[] = {0x01, 0, 0, 0};  // idx0, idx1 + 行补齐到 4
        bmp.insert(bmp.end(), pixels, pixels + 4);
        put_file_header(bmp, 48);
        const auto img = decode_bmp(bmp);
        REQUIRE(img.has_value());
        CHECK(img->at(0, 0, 2) == 0);   // 索引 0：黑
        CHECK(img->at(1, 0, 2) == 255);  // 索引 1：蓝
        CHECK(img->at(1, 0, 0) == 0);
    }
    // 1bpp：MSB 先行。行补齐到 4 字节。
    {
        std::vector<std::uint8_t> bmp;
        put_info_header(bmp, 4, 1, 1, 0, 2);
        put_u32(bmp, 0x00000000);  // 黑
        put_u32(bmp, 0x0000FF00);  // 绿（BGR：G 在中位 → 0x0000FF00 即 G=255）
        const std::uint8_t pixels[] = {0xA0, 0, 0, 0};  // 1010_0000：bit7=1,bit6=0,bit5=1,bit4=0 + 行补齐到 4
        bmp.insert(bmp.end(), pixels, pixels + 4);
        put_file_header(bmp, 48);
        const auto img = decode_bmp(bmp);
        REQUIRE(img.has_value());
        CHECK(img->at(0, 0, 1) == 255);  // bit7=1 → 绿
        CHECK(img->at(1, 0, 1) == 0);    // bit6=0 → 黑
        CHECK(img->at(2, 0, 1) == 255);  // bit5=1
        CHECK(img->at(3, 0, 1) == 0);    // bit4=0
    }
}

TEST_CASE("image/bmp: top-down（负高）") {
    std::vector<std::uint8_t> bmp;
    put_info_header(bmp, 1, -2, 24, 0, 0);  // height=-2 → top-down
    const std::uint8_t r0[] = {0, 0, 255, 0};  // 图像 y=0 红（BGR：R 在末字节）
    const std::uint8_t r1[] = {255, 0, 0, 0};  // 图像 y=1 蓝（BGR：B 在首字节）
    bmp.insert(bmp.end(), r0, r0 + 4);
    bmp.insert(bmp.end(), r1, r1 + 4);
    put_file_header(bmp, 40);
    const auto img = decode_bmp(bmp);
    REQUIRE(img.has_value());
    CHECK(img->at(0, 0, 0) == 255);  // top-down：首行即 y=0
    CHECK(img->at(0, 1, 2) == 255);
}

TEST_CASE("image/bmp: 32bpp BITFIELDS（带 alpha 掩码，V4 头）") {
    std::vector<std::uint8_t> bmp;
    put_info_header(bmp, 1, 1, 32, 3, 0);  // biSize=40, biCompression=BI_BITFIELDS
    // 扩成 V4（52 字节）：补 12 字节 color space / endpoints 占位。
    put_u32(bmp, 0);
    put_u32(bmp, 0);
    put_u32(bmp, 0);
    // 4 个 DWORD 掩码（紧跟信息头后）。
    put_u32(bmp, 0x00FF0000);  // R
    put_u32(bmp, 0x0000FF00);  // G
    put_u32(bmp, 0x000000FF);  // B
    put_u32(bmp, 0xFF000000);  // A
    // 像素：A=0xCC, R=0xFF, G=0, B=0。
    put_u32(bmp, 0xCCFF0000u);
    bmp[0] = 52;  // 覆写 biSize 为 52（V4），让解码器读 alpha 掩码。
    put_file_header(bmp, 68);  // 信息头 52 + 掩码 16 后接像素
    const auto img = decode_bmp(bmp);
    REQUIRE(img.has_value());
    CHECK(img->at(0, 0, 0) == 255);
    CHECK(img->at(0, 0, 1) == 0);
    CHECK(img->at(0, 0, 2) == 0);
    CHECK(img->at(0, 0, 3) == 204);  // 0xCC = 204
}

TEST_CASE("image/bmp: 错误路径显式报错") {
    // 非 'BM' 魔数 → Unsupported
    const std::string not_bmp = "PNGxxxx";
    auto r = decode_bmp(not_bmp);
    CHECK_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::Unsupported);

    // 太短（只有 'BM' 两字节）→ ParseError
    r = decode_bmp(std::string_view{"BM", 2});
    CHECK_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::ParseError);

    // OS/2 12 字节旧头 → Unsupported
    {
        std::vector<std::uint8_t> bmp;
        put_u32(bmp, 12);   // biSize = 12（BITMAPCOREHEADER）
        bmp.resize(12, 0);  // 补齐到 12 字节信息头
        put_file_header(bmp, 12);  // 像素偏移 = 14 + 12
        r = decode_bmp(bmp);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::Unsupported);
    }

    // 像素数据偏移越界 → ParseError
    {
        std::vector<std::uint8_t> bmp;
        put_info_header(bmp, 2, 1, 24, 0, 0);
        put_u32(bmp, 0);  // 两个黑像素（行 8 字节）
        put_u32(bmp, 0);
        put_file_header(bmp, 40);
        // 手工把文件头的 pixel_offset 改到缓冲区外（绝对偏移字节在 10..13）。
        bmp[10] = 0xF0;
        r = decode_bmp(bmp);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::ParseError);
    }

    // RLE8 压缩 → Unsupported
    {
        std::vector<std::uint8_t> bmp;
        put_info_header(bmp, 2, 1, 8, 1, 0);  // compression=1 (BI_RLE8)
        put_file_header(bmp, 40);
        r = decode_bmp(bmp);
        CHECK_FALSE(r.has_value());
        CHECK(r.error() == ErrorCode::Unsupported);
    }
}

}  // namespace
