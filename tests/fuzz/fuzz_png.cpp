// fuzz_png.cpp —— PNG 解码（**规格清单外的补充**，但它是全仓最理想的 fuzz 入口）
//
// 为什么加它：`decode_png` 是纯函数、无状态、不抛异常（返回 `Result`）、内部有
// 完整 inflate（stored/固定/动态 Huffman）+ 五种滤波 + 六种颜色类型 —— 这些正是
// 最容易出越界的代码。输入完全由外部字节驱动，天然适合 fuzz。
//
// 两类不变量（不是"顺便看一眼"，违反了就 abort）：
//   1) **绝不抛异常**：任意字节都只能得到错误码；
//   2) **往返自洽**：解出来的图编码回去必须能再解出来，且尺寸一致 —— 这条能抓到
//      "解码接受、编码却产出非法字节"这类单边 bug。
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "fuzz_common.hpp"

// 生成物：一批已知合法的 PNG（滤波 1–4、动态 Huffman、调色板 4/8 位、16 位灰度…）。
// 共享语料里的 PNG 全都"残缺到一进门就早退"，往返路径根本走不到，所以这里必须
// 借这批真样本。
#include "../unit/image/test_png_samples.inc"
#include "termchart/image/png.hpp"
#include "termchart/image/png_encode.hpp"

namespace {

std::string be32(std::uint32_t value) {
    std::string out;
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
    }
    return out;
}

std::string chunk(const std::string& type, const std::string& payload) {
    std::string body = type + payload;
    return be32(static_cast<std::uint32_t>(payload.size())) + body +
           be32(termchart::image::crc32(reinterpret_cast<const std::uint8_t*>(body.data()),
                                        body.size()));
}

// 造一个"签名 + IHDR + IEND"的最小 PNG：`ihdr_length` 可以**故意写错**，但 CRC
// 始终按实际字节算对 —— 否则会被 CRC 挡在门外，测不到我们想测的那条分支。
//
// **IEND 不能省**：解码器先跑 chunk 解析，缺 IEND 会直接判 ParseError，于是根本
// 到不了"宽高撒谎 → 尺寸守卫"和"块长度非法"这两条我们真正想压的分支（第一版就是
// 漏了 IEND，这两条种子实际只在测"缺 IEND"，白白浪费）。
std::string png_with_ihdr(std::uint32_t width, std::uint32_t height,
                          std::uint32_t ihdr_length) {
    std::string payload = be32(width) + be32(height);
    payload.push_back(8);   // bit depth
    payload.push_back(6);   // color type: RGBA
    payload.push_back(0);   // compression
    payload.push_back(0);   // filter
    payload.push_back(0);   // interlace
    if (ihdr_length < payload.size()) {
        payload.resize(ihdr_length);
    }

    // IHDR 用**故意写错**的长度字段，CRC 按实际字节算。
    std::string body = "IHDR" + payload;
    std::string out("\x89PNG\r\n\x1a\n", 8);
    out += be32(ihdr_length);
    out += body;
    out += be32(termchart::image::crc32(
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size()));
    out += chunk("IEND", "");
    return out;
}

// 目标专属语料：让解码走**深层分支**（尺寸守卫、块长度守卫、真实往返）。
std::vector<std::string> png_seeds() {
    std::vector<std::string> seeds;

    // 1) 已知合法 PNG：走通"解码 → 再编码 → 再解码"的完整往返。
    seeds.emplace_back(reinterpret_cast<const char*>(kSubPng), sizeof(kSubPng));
    seeds.emplace_back(reinterpret_cast<const char*>(kDynamicPng), sizeof(kDynamicPng));
    seeds.emplace_back(reinterpret_cast<const char*>(kPalette4Png), sizeof(kPalette4Png));
    seeds.emplace_back(reinterpret_cast<const char*>(kGray16Png), sizeof(kGray16Png));

    // 2) 宽高撒谎：IHDR 合法（长度 13、CRC 正确），但宽高是 65535×65535。
    //    解码器必须靠 kPngMaxRawBytes 拒绝，而不是照着 4G 像素去分配内存。
    seeds.push_back(png_with_ihdr(0xFFFF, 0xFFFF, 13));

    // 3) 块长度非法：IHDR 声明长度 5（PNG 规定必须 13），CRC 却是对的。
    //    必须报错退出，不能按 5 字节去读后面的字段。
    seeds.push_back(png_with_ihdr(8, 8, 5));

    return seeds;
}

void target(const std::uint8_t* data, std::size_t size) {
    const termchart::Result<termchart::image::Bitmap> decoded =
        termchart::image::decode_png(data, size);
    if (!decoded.has_value()) {
        // 只允许是明确的错误码：任何异常泄漏都算失败（fuzz 里异常会直接终止进程）。
        (void)decoded.error();
        return;
    }

    const termchart::image::Bitmap& bitmap = decoded.value();
    const std::string reencoded = termchart::image::encode_png(bitmap);
    if (reencoded.empty()) {
        return;  // 空位图：编码器按约定返回空串
    }

    const auto again = termchart::image::decode_png(
        reinterpret_cast<const std::uint8_t*>(reencoded.data()), reencoded.size());
    // 真不变量：编码器写出的字节流必须能被解码器读回，且尺寸与通道数一致。
    // （解码器统一产出 RGBA，编码器对 3/4 通道都能写，因此这里三者都应成立。）
    if (!again.has_value() || again.value().width() != bitmap.width() ||
        again.value().height() != bitmap.height() ||
        again.value().channels() != bitmap.channels()) {
        std::abort();
    }
}

}  // namespace

TERMCHART_FUZZ_MAIN_WITH_SEEDS(target, png_seeds)
