// test_gif_encode.cpp —— 自写 GIF89a 编码器最小测试
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <doctest.h>

#include "termchart/api/export_image.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/media/image/bitmap.hpp"
#include "termchart/media/image/encode.hpp"
#include "termchart/media/image/gif_encode.hpp"

using namespace termchart;
using namespace termchart::image;

namespace {

Bitmap make_frame(int w, int h, int phase) {
    Bitmap bm(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0,
                   static_cast<std::uint8_t>(x * 255 / w));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1,
                   static_cast<std::uint8_t>(y * 255 / h));
            bm.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2,
                   static_cast<std::uint8_t>((phase * 40) % 256));
        }
    return bm;
}

// 环境里 ffmpeg 可用时做真实解码校验；不可用时返回 false（测试里仅作 best-effort）。
bool ffmpeg_present() {
    return std::system("ffmpeg -version >nul 2>nul") == 0;
}
bool ffmpeg_decodes(const std::string& path) {
    const std::string cmd = "ffmpeg -v error -i \"" + path + "\" -f null - >nul 2>nul";
    return std::system(cmd.c_str()) == 0;
}

}  // namespace

TEST_CASE("gif: 单帧产出合法 GIF89a 结构") {
    std::vector<Bitmap> frames = {make_frame(32, 16, 0)};
    auto gif = encode_gif(frames, {});
    REQUIRE_FALSE(gif.empty());
    REQUIRE(gif.size() >= 13);
    CHECK(gif.substr(0, 6) == "GIF89a");
    CHECK(gif.back() == static_cast<char>(0x3B));  // trailer
    const std::uint8_t packed = static_cast<std::uint8_t>(gif[10]);
    CHECK((packed & 0x80) != 0);  // 全局色表存在
    const int w = static_cast<unsigned char>(gif[6]) | (static_cast<unsigned char>(gif[7]) << 8);
    const int h = static_cast<unsigned char>(gif[8]) | (static_cast<unsigned char>(gif[9]) << 8);
    CHECK(w == 32);
    CHECK(h == 16);
}

TEST_CASE("gif: 空输入 / 不等尺寸返回空串") {
    CHECK(encode_gif({}, {}).empty());
    std::vector<Bitmap> bad = {make_frame(32, 16, 0), make_frame(33, 16, 1)};
    CHECK(encode_gif(bad, {}).empty());
}

TEST_CASE("gif: 多帧动画非空且可被解码") {
    std::vector<Bitmap> frames = {make_frame(48, 32, 0), make_frame(48, 32, 1),
                                  make_frame(48, 32, 2)};
    auto gif = encode_gif(frames, EncodeOptions{.fps = 10, .loop_count = 0});
    REQUIRE_FALSE(gif.empty());
    if (ffmpeg_present()) {
        std::FILE* f = std::fopen("tc_gif.gif", "wb");
        REQUIRE(f != nullptr);
        std::fwrite(gif.data(), 1, gif.size(), f);
        std::fclose(f);
        CHECK(ffmpeg_decodes("tc_gif.gif"));
    }
}

TEST_CASE("gif: api::save_image 多帧导出 GIF 动画") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1),
                                  make_frame(64, 48, 2)};
    const auto st = api::save_image(frames, "tc_api_gif.gif", ImageFormat::Gif,
                                    EncodeOptions{.fps = 10, .loop_count = 0});
    CHECK(st == ErrorCode::Ok);

    std::FILE* fp = std::fopen("tc_api_gif.gif", "rb");
    REQUIRE(fp != nullptr);
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
    std::fclose(fp);
    CHECK(out.substr(0, 6) == "GIF89a");
    CHECK(out.back() == static_cast<char>(0x3B));
}
