// test_mp4_encode.cpp —— 自写 MP4/H.264 编码器最小测试
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <doctest.h>

#include "termchart/api/export_image.hpp"
#include "termchart/core/expected.hpp"
#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/h264_encode.hpp"
#include "termchart/image/hevc_encode.hpp"
#include "termchart/image/mp4.hpp"

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

bool has_box(const std::string& hay, const char* box) {
    return hay.find(box, 0, 4) != std::string::npos;
}

std::uint32_t rd_u32(const std::string& d, std::size_t pos) {
    return (static_cast<unsigned char>(d[pos]) << 24) |
           (static_cast<unsigned char>(d[pos + 1]) << 16) |
           (static_cast<unsigned char>(d[pos + 2]) << 8) |
           (static_cast<unsigned char>(d[pos + 3]));
}
std::uint16_t rd_u16(const std::string& d, std::size_t pos) {
    return static_cast<std::uint16_t>((static_cast<unsigned char>(d[pos]) << 8) |
                                      static_cast<unsigned char>(d[pos + 1]));
}

struct Box { std::string type; std::size_t off; std::size_t size; };

// 递归收集容器盒（moov/trak/... 与 stsd/hvc1/avc1 内的子盒），用于结构断言。
void collect_boxes(const std::string& d, std::size_t off, std::size_t end, std::vector<Box>& out) {
    std::size_t i = off;
    while (i + 8 <= end) {
        const std::size_t s = rd_u32(d, i);
        if (s < 8) break;
        const std::string t = d.substr(i + 4, 4);
        out.push_back({t, i, s});
        if (t == "moov" || t == "trak" || t == "mdia" || t == "minf" || t == "stbl" ||
            t == "dinf" || t == "edts") {
            collect_boxes(d, i + 8, i + s, out);
        } else if (t == "stsd") {
            collect_boxes(d, i + 16, i + s, out);
        } else if (t == "hvc1" || t == "avc1") {
            collect_boxes(d, i + 8 + 78, i + s, out);
        }
        i += s;
    }
}

}  // namespace

TEST_CASE("mp4: 单帧编码产出合法容器结构") {
    std::vector<Bitmap> frames = {make_frame(32, 16, 0)};
    auto mp4 = encode_mp4(frames, {});
    REQUIRE_FALSE(mp4.empty());
    CHECK(has_box(mp4, "ftyp"));
    CHECK(has_box(mp4, "moov"));
    CHECK(has_box(mp4, "trak"));
    CHECK(has_box(mp4, "mdia"));
    CHECK(has_box(mp4, "minf"));
    CHECK(has_box(mp4, "stbl"));
    CHECK(has_box(mp4, "avc1"));
    CHECK(has_box(mp4, "avcC"));
    CHECK(has_box(mp4, "mdat"));
}

TEST_CASE("mp4: 多帧仍产出非空且含 mdat") {
    std::vector<Bitmap> frames = {make_frame(48, 32, 0), make_frame(48, 32, 1),
                                  make_frame(48, 32, 2)};
    auto mp4 = encode_mp4(frames, {});
    REQUIRE_FALSE(mp4.empty());
    CHECK(has_box(mp4, "mdat"));
}

TEST_CASE("mp4: 空输入 / 不等尺寸返回空串") {
    CHECK(encode_mp4({}, {}).empty());
    std::vector<Bitmap> bad = {make_frame(32, 16, 0), make_frame(33, 16, 1)};
    CHECK(encode_mp4(bad, {}).empty());
}

TEST_CASE("h264: Annex-B 含 SPS / PPS / IDR 起始码") {
    std::vector<Bitmap> frames = {make_frame(32, 16, 0)};
    auto annexb = encode_h264_annexb(frames, {});
    REQUIRE_FALSE(annexb.empty());
    int starts = 0;
    for (size_t i = 0; i + 4 <= annexb.size(); ++i) {
        if (static_cast<std::uint8_t>(annexb[i]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 1]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 2]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 3]) == 1) {
            ++starts;
        }
    }
    CHECK(starts >= 3);  // SPS + PPS + IDR
}

TEST_CASE("hevc: Annex-B 含 VPS / SPS / PPS / IDR 起始码") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0)};
    auto annexb = encode_hevc_annexb(frames, {});
    REQUIRE_FALSE(annexb.empty());
    // 起始码 00 00 00 01 出现至少 4 次（VPS+SPS+PPS+IDR）。
    int starts = 0;
    for (size_t i = 0; i + 4 <= annexb.size(); ++i) {
        if (static_cast<std::uint8_t>(annexb[i]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 1]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 2]) == 0 &&
            static_cast<std::uint8_t>(annexb[i + 3]) == 1) {
            ++starts;
        }
    }
    CHECK(starts >= 4);
}

TEST_CASE("mp4: HEVC 产出 hvc1 / hvcC 容器结构") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1)};
    auto mp4 = encode_mp4(frames, EncodeOptions{.video_codec = VideoCodec::H265});
    REQUIRE_FALSE(mp4.empty());
    CHECK(has_box(mp4, "ftyp"));
    CHECK(has_box(mp4, "moov"));
    CHECK(has_box(mp4, "trak"));
    CHECK(has_box(mp4, "mdia"));
    CHECK(has_box(mp4, "minf"));
    CHECK(has_box(mp4, "stbl"));
    CHECK(has_box(mp4, "hvc1"));
    CHECK(has_box(mp4, "hvcC"));
    CHECK(has_box(mp4, "mdat"));
}

// 落盘一个样本，供外部 ffprobe / ffmpeg 验证「自写码流确实可被解码」。
TEST_CASE("mp4: 落盘样本供 ffprobe 验证") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1),
                                  make_frame(64, 48, 2)};
    auto mp4 = encode_mp4(frames, EncodeOptions{.fps = 10});
    REQUIRE_FALSE(mp4.empty());
    std::FILE* f = std::fopen("tc_mp4_ffprobe.mp4", "wb");
    REQUIRE(f != nullptr);
    std::fwrite(mp4.data(), 1, mp4.size(), f);
    std::fclose(f);

    // 同时落盘裸 Annex-B，隔离「H.264 码流」与「MP4 容器」两处变量。
    auto annexb = encode_h264_annexb(frames, EncodeOptions{});
    REQUIRE_FALSE(annexb.empty());
    std::FILE* g = std::fopen("tc_h264.264", "wb");
    REQUIRE(g != nullptr);
    std::fwrite(annexb.data(), 1, annexb.size(), g);
    std::fclose(g);

    // HEVC 对照：裸 Annex-B 与封装后 MP4 各落一份，验证自写 HEVC 码流可被解码。
    auto hevc_mp4 = encode_mp4(frames, EncodeOptions{.fps = 10, .video_codec = VideoCodec::H265});
    REQUIRE_FALSE(hevc_mp4.empty());
    std::FILE* h = std::fopen("tc_hevc.mp4", "wb");
    REQUIRE(h != nullptr);
    std::fwrite(hevc_mp4.data(), 1, hevc_mp4.size(), h);
    std::fclose(h);

    auto hevc_annexb = encode_hevc_annexb(frames, {});
    REQUIRE_FALSE(hevc_annexb.empty());
    std::FILE* k = std::fopen("tc_hevc.265", "wb");
    REQUIRE(k != nullptr);
    std::fwrite(hevc_annexb.data(), 1, hevc_annexb.size(), k);
    std::fclose(k);
}

// 防 PotPlayer 类回退：tkhd 必须带合法 track_ID 与正时长，且 edts 存在。
// （早期 bug：track_ID 写成 0、duration 错位进 reserved，导致播放器拒绝建轨。）
TEST_CASE("mp4: tkhd/edts 合法（防 PotPlayer 回退）") {
    for (const auto codec : {VideoCodec::H264, VideoCodec::H265}) {
        std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1),
                                      make_frame(64, 48, 2)};
        auto mp4 = encode_mp4(frames, EncodeOptions{.fps = 10, .video_codec = codec});
        REQUIRE_FALSE(mp4.empty());
        std::vector<Box> boxes;
        collect_boxes(mp4, 0, mp4.size(), boxes);

        bool tkhd_ok = false, edts_ok = false;
        for (const auto& b : boxes) {
            if (b.type == "tkhd") {
                // 盒头(8) + ver_flags(4) 后：creation(4) mod(4) track_ID(4) reserved(4) duration(4)
                const std::uint32_t track_id = rd_u32(mp4, b.off + 8 + 12);
                const std::int32_t duration = static_cast<std::int32_t>(rd_u32(mp4, b.off + 8 + 20));
                CHECK(track_id >= 1);
                CHECK(duration > 0);
                tkhd_ok = true;
            }
            if (b.type == "edts") edts_ok = true;
        }
        CHECK(tkhd_ok);
        CHECK(edts_ok);
    }
}

// hvcC 内 VPS/SPS/PPS 必须是不带尾随零撑大的真实长度（早期 bug 每个 NAL 多 1 字节）。
TEST_CASE("mp4: HEVC hvcC 参数集长度未被撑大") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1)};
    auto mp4 = encode_mp4(frames, EncodeOptions{.video_codec = VideoCodec::H265});
    REQUIRE_FALSE(mp4.empty());
    std::vector<Box> boxes;
    collect_boxes(mp4, 0, mp4.size(), boxes);

    bool hvcC_ok = false;
    for (const auto& b : boxes) {
        if (b.type != "hvcC") continue;
        hvcC_ok = true;
        // 23 字节定长头后：numOfArrays，再逐数组 [type][count][NAL...]
        std::size_t p = b.off + 8 + 23;
        const int num_arrays = static_cast<unsigned char>(mp4[b.off + 8 + 22]);
        int lens[3] = {0, 0, 0};  // VPS/SPS/PPS
        bool found[3] = {false, false, false};
        for (int a = 0; a < num_arrays; ++a) {
            const int typ = static_cast<unsigned char>(mp4[p]) & 0x3F;
            p += 1;
            const int cnt = rd_u16(mp4, p);
            p += 2;
            for (int c = 0; c < cnt; ++c) {
                const int ln = rd_u16(mp4, p);
                p += 2;
                if (typ == 32) { found[0] = true; lens[0] = ln; }
                if (typ == 33) { found[1] = true; lens[1] = ln; }
                if (typ == 34) { found[2] = true; lens[2] = ln; }
                p += static_cast<std::size_t>(ln);
            }
        }
        CHECK(found[0]);
        CHECK(found[1]);
        CHECK(found[2]);
        CHECK(lens[0] == 22);  // VPS
        CHECK(lens[1] == 27);  // SPS
        CHECK(lens[2] > 0);    // PPS（自写 7 字节，FFmpeg 两种皆可解）
    }
    CHECK(hvcC_ok);
}

// 公共门面 api::save_image 多帧重载应能把帧序列落盘成合法 MP4。
TEST_CASE("mp4: api::save_image 多帧导出 HEVC 视频") {
    std::vector<Bitmap> frames = {make_frame(64, 48, 0), make_frame(64, 48, 1),
                                  make_frame(64, 48, 2)};
    const auto st = api::save_image(
        frames, "tc_api_hevc.mp4", ImageFormat::Mp4,
        EncodeOptions{.fps = 10, .video_codec = VideoCodec::H265});
    CHECK(st == ErrorCode::Ok);

    std::FILE* fp = std::fopen("tc_api_hevc.mp4", "rb");
    REQUIRE(fp != nullptr);
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
    std::fclose(fp);
    CHECK(has_box(out, "ftyp"));
    CHECK(has_box(out, "moov"));
    CHECK(has_box(out, "hvc1"));
    CHECK(has_box(out, "hvcC"));
    CHECK(has_box(out, "mdat"));
}

