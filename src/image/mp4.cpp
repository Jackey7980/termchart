// mp4.cpp —— 自写 MP4（ISO-BMFF）封装 + encode_mp4 入口
//
// 零依赖：自己写 box 组装，不调用 libavformat/ffmpeg。按 EncodeOptions::video_codec 选
// 内部码流（H264 走自写 Annex-B H.264；H265 走自写 Annex-B HEVC），再封装成可被标准
// 播放器 / ffprobe 读取的 MP4。当前 H265 为 I_PCM 档，H264 同。
#include "termchart/image/mp4.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "termchart/image/bitmap.hpp"
#include "termchart/image/encode.hpp"
#include "termchart/image/h264_encode.hpp"
#include "termchart/image/hevc_encode.hpp"

namespace termchart::image {
namespace {

void put_u8(std::string& s, uint8_t v) { s.push_back(static_cast<char>(v)); }
void put_u16(std::string& s, uint16_t v) {
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>(v & 0xFF));
}
void put_u32(std::string& s, uint32_t v) {
    s.push_back(static_cast<char>((v >> 24) & 0xFF));
    s.push_back(static_cast<char>((v >> 16) & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>(v & 0xFF));
}
std::string box(const char t[4], std::string payload) {
    std::string s;
    put_u32(s, static_cast<uint32_t>(payload.size() + 8));
    s.push_back(t[0]); s.push_back(t[1]); s.push_back(t[2]); s.push_back(t[3]);
    s += payload;
    return s;
}
std::string fullbox(const char t[4], uint32_t ver_flags, std::string payload) {
    std::string p;
    put_u32(p, ver_flags);
    p += payload;
    return box(t, p);
}

// 解析 Annex-B：抽出 VPS/SPS/PPS 与每帧 slice（长度前缀样本）。is_hevc 决定 NAL 类型位域。
struct Parsed {
    std::string sps, pps, vps;
    std::vector<std::string> samples;
};
Parsed parse_annexb(const std::string& b, bool is_hevc) {
    Parsed r;
    const size_t n = b.size();
    size_t i = 0;
    while (i + 3 < n) {
        if (b[i] == 0 && b[i + 1] == 0 && b[i + 2] == 1) {
            size_t start = i + 3;
            size_t j = start;
            while (j + 3 < n && !(b[j] == 0 && b[j + 1] == 0 && b[j + 2] == 1)) ++j;
            size_t end;
            if (j + 3 < n) {
                // 编码器产出的是 4 字节起始码 00 00 00 01，其 00 00 01 落点在
                // 第 2 字节；真正起始码起点在 j-1（若前导字节也是 0）。否则为
                // 3 字节起始码，起点即 j。两者都要排除起始码本身，不能把其首字节
                // 算进上一个 NAL（否则每个非末尾 NAL 会多带一个尾随 00）。
                end = (j > 0 && b[j - 1] == 0) ? j - 1 : j;
                i = end;
            } else {
                end = n;
                i = n;
            }
            std::string nal = b.substr(start, end - start);
            if (nal.size() >= (is_hevc ? 2u : 1u)) {
                int type = is_hevc ? (static_cast<uint8_t>(nal[0]) >> 1) & 0x3F
                                    : static_cast<uint8_t>(nal[0]) & 0x1F;
                if (is_hevc) {
                    if (type == 32) r.vps = nal;
                    else if (type == 33) r.sps = nal;
                    else if (type == 34) r.pps = nal;
                    else if (type == 19 || type == 20) {  // IDR / IDR_RADL
                        std::string sample;
                        put_u32(sample, static_cast<uint32_t>(nal.size()));
                        sample += nal;
                        r.samples.push_back(std::move(sample));
                    }
                } else {
                    if (type == 7) r.sps = nal;
                    else if (type == 8) r.pps = nal;
                    else if (type == 1 || type == 5) {
                        std::string sample;
                        put_u32(sample, static_cast<uint32_t>(nal.size()));
                        sample += nal;
                        r.samples.push_back(std::move(sample));
                    }
                }
            }
        } else {
            ++i;
        }
    }
    return r;
}

// ── AVC（H.264）配置 ──
std::string build_avcc(const Parsed& p) {
    std::string payload;
    put_u8(payload, 1);     // configurationVersion
    put_u8(payload, 66);    // AVCProfileIndication = Baseline
    put_u8(payload, 0x80);  // profile_compatibility（constraint_set0=1）
    put_u8(payload, 30);    // AVCLevelIndication
    put_u8(payload, 0xFF);  // reserved(111111) + lengthSizeMinusOne=3
    put_u8(payload, 0xE1);  // reserved(111) + numOfSPS=1
    put_u16(payload, static_cast<uint16_t>(p.sps.size()));
    payload += p.sps;
    put_u8(payload, 1);     // numOfPPS
    put_u16(payload, static_cast<uint16_t>(p.pps.size()));
    payload += p.pps;
    return box("avcC", payload);
}

std::string build_avc1(int cw, int ch, const Parsed& p) {
    std::string payload;
    for (int i = 0; i < 6; ++i) put_u8(payload, 0);
    put_u16(payload, 1);
    put_u16(payload, 0);
    put_u16(payload, 0);
    for (int i = 0; i < 3; ++i) put_u32(payload, 0);
    put_u16(payload, static_cast<uint16_t>(cw));
    put_u16(payload, static_cast<uint16_t>(ch));
    put_u32(payload, 0x00480000);
    put_u32(payload, 0x00480000);
    put_u32(payload, 0);
    put_u16(payload, 1);
    for (int i = 0; i < 32; ++i) put_u8(payload, 0);
    put_u16(payload, 0x0018);
    put_u16(payload, 0xFFFF);
    payload += build_avcc(p);
    return box("avc1", payload);
}

// ── HEVC（H.265）配置 ──
std::string build_hvcc(const Parsed& p) {
    std::string payload;
    put_u8(payload, 1);     // configurationVersion
    // general_profile_space(2)=0 | general_tier_flag(1)=0 | general_profile_idc(5)=1
    put_u8(payload, 1);
    put_u32(payload, 1u << 1);  // general_profile_compatibility_flags（Main）
    // general_constraint_indicator_flags(48)：与 Main profile 位流一致。
    // bit47=progressive, bit45=non_packed, bit44=frame_only, bit41=max_8bit, bit39=max_420。
    const uint64_t constraint = (uint64_t(1) << 47) | (uint64_t(1) << 45) |
                                (uint64_t(1) << 44) | (uint64_t(1) << 41) |
                                (uint64_t(1) << 39);
    put_u32(payload, static_cast<uint32_t>(constraint >> 16));
    put_u16(payload, static_cast<uint16_t>(constraint & 0xFFFF));
    put_u8(payload, 30);    // general_level_idc
    // reserved(4)=1111 | min_spatial_segmentation_idc(12)=0
    put_u8(payload, 0xF0);
    put_u8(payload, 0);
    // reserved(6)=111111 | parallelismType(2)=0
    put_u8(payload, 0xFC);
    // reserved(6)=111111 | chromaFormat(2)=1 (4:2:0)
    put_u8(payload, 0xFC | 0x01);
    // reserved(5)=11111 | bitDepthLumaMinus8(3)=0
    put_u8(payload, 0xF8);
    // reserved(5)=11111 | bitDepthChromaMinus8(3)=0
    put_u8(payload, 0xF8);
    put_u16(payload, 0);    // avgFrameRate
    // 单字节：constantFrameRate(2)=0 | numTemporalLayers(3)=1 | temporalIdNested(1)=1 |
    // lengthSizeMinusOne(2)=3  → 0x0F
    put_u8(payload, 0x0F);
    put_u8(payload, 3);     // numOfArrays = 3（VPS/SPS/PPS）
    auto array = [&](uint8_t type, const std::string& nal) {
        put_u8(payload, 0x80 | type);  // array_completeness(1)=1 | reserved(1)=0 | nal_unit_type(6)
        put_u16(payload, 1);           // numNalus
        put_u16(payload, static_cast<uint16_t>(nal.size()));  // nalUnitLength
        payload += nal;
        // 注意：hvcC 内每个 NAL 仅有一个 2 字节 nalUnitLength 前缀。FFmpeg 的
        // ff_hevc_decode_extradata 里 nalsize = peek_be16 + 2 的「+2」是长度字段本身，
        // 它再用 ff_h2645_packet_split（nal_length_size=2）消费这个前缀；再叠加一层
        // 前缀会让数组错位、读错后续 NAL 大小，报 "Invalid NAL unit size in extradata"。
    };
    array(32, p.vps);
    array(33, p.sps);
    array(34, p.pps);
    return box("hvcC", payload);
}

std::string build_hevc1(int cw, int ch, const Parsed& p) {
    std::string payload;
    for (int i = 0; i < 6; ++i) put_u8(payload, 0);
    put_u16(payload, 1);
    put_u16(payload, 0);
    put_u16(payload, 0);
    for (int i = 0; i < 3; ++i) put_u32(payload, 0);
    put_u16(payload, static_cast<uint16_t>(cw));
    put_u16(payload, static_cast<uint16_t>(ch));
    put_u32(payload, 0x00480000);
    put_u32(payload, 0x00480000);
    put_u32(payload, 0);
    put_u16(payload, 1);
    for (int i = 0; i < 32; ++i) put_u8(payload, 0);
    put_u16(payload, 0x0018);
    put_u16(payload, 0xFFFF);
    payload += build_hvcc(p);
    return box("hvc1", payload);
}

}  // namespace

std::string encode_mp4(std::span<const Bitmap> frames, const EncodeOptions& opts) {
    // 未实现的码流（如 AV1）：显式失败。绝不能落到 H.264 分支静默降级——否则调用方
    // 以为拿到的是 AV1 文件，实际却是 H.264 码流（旧实现正是如此，现已修掉）。
    if (!is_video_codec_supported(opts.video_codec)) return std::string{};
    if (frames.empty()) return std::string{};
    const int W = static_cast<int>(frames[0].width());
    const int H = static_cast<int>(frames[0].height());
    for (const auto& f : frames) {
        if (static_cast<int>(f.width()) != W || static_cast<int>(f.height()) != H) {
            return std::string{};  // 暂要求统一尺寸
        }
    }
    const bool is_hevc = (opts.video_codec == VideoCodec::H265);
    const int cw = is_hevc ? ((W + 63) / 64) * 64 : ((W + 15) / 16) * 16;
    const int ch = is_hevc ? ((H + 63) / 64) * 64 : ((H + 15) / 16) * 16;

    std::string annexb = is_hevc ? encode_hevc_annexb(frames, opts)
                                 : encode_h264_annexb(frames, opts);
    if (annexb.empty()) return std::string{};
    Parsed p = parse_annexb(annexb, is_hevc);
    if (p.sps.empty() || p.pps.empty() || p.samples.empty()) return std::string{};
    if (is_hevc && p.vps.empty()) return std::string{};

    const int num_frames = static_cast<int>(p.samples.size());
    const int fps = opts.fps > 0 ? opts.fps : 25;
    // 时基取 fps*1000：每帧恰好 1000 tick（= 1ms×1000），从而 dur 与 sample_delta 都为整数且精确。
    // 原先 ts=1000 时 dur = num_frames*(ts/fps) 因整数除被截断（1000/60→16），会让 60fps 视频
    // 少算约 4%（300 帧的 5s 视频只剩 4.8s）；改后 5s 严格等于 5.000s。
    const uint32_t ts = static_cast<uint32_t>(fps) * 1000u;
    const uint32_t dur = static_cast<uint32_t>(num_frames) * 1000u;
    const uint32_t sample_delta = 1000u;
    const uint32_t mtx[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

    std::string ftyp;
    ftyp += "isom";
    put_u32(ftyp, 0x00000200);
    ftyp += "isom";
    ftyp += is_hevc ? "hvc1" : "avc1";
    ftyp += "mp41";
    std::string ftyp_box = box("ftyp", ftyp);

    std::string mvhd;
    put_u32(mvhd, 0); put_u32(mvhd, 0); put_u32(mvhd, ts); put_u32(mvhd, dur);
    put_u32(mvhd, 0x00010000); put_u16(mvhd, 0x0100);
    put_u16(mvhd, 0); put_u32(mvhd, 0); put_u32(mvhd, 0);
    for (uint32_t v : mtx) put_u32(mvhd, v);
    for (int i = 0; i < 6; ++i) put_u32(mvhd, 0);
    put_u32(mvhd, 2);
    std::string mvhd_box = fullbox("mvhd", 0, mvhd);

    std::string tkhd;
    put_u32(tkhd, 0); put_u32(tkhd, 0); put_u32(tkhd, 1); put_u32(tkhd, 0);
    put_u32(tkhd, dur); put_u32(tkhd, 0); put_u32(tkhd, 0);
    put_u16(tkhd, 0); put_u16(tkhd, 0); put_u16(tkhd, 0); put_u16(tkhd, 0);
    for (uint32_t v : mtx) put_u32(tkhd, v);
    put_u32(tkhd, static_cast<uint32_t>(cw) << 16);
    put_u32(tkhd, static_cast<uint32_t>(ch) << 16);
    std::string tkhd_box = fullbox("tkhd", 0x00000007, tkhd);

    // edts/elst：把媒体时间轴映射到轨道时间轴（1 条 edit，rate=1.0）。
    // 缺少时部分播放器（如 PotPlayer）会拒绝按轨道时长播放。
    std::string elst;
    put_u32(elst, 1);                         // entry_count
    put_u32(elst, dur);                       // segment_duration（movie 时间刻度）
    put_u32(elst, 0);                         // media_time
    put_u16(elst, 1); put_u16(elst, 0);       // media_rate_integer=1, fraction=0
    std::string elst_box = fullbox("elst", 0, elst);
    std::string edts = box("edts", elst_box);

    std::string mdhd;
    put_u32(mdhd, 0); put_u32(mdhd, 0); put_u32(mdhd, ts); put_u32(mdhd, dur);
    put_u16(mdhd, 0x55C4); put_u16(mdhd, 0);
    std::string mdhd_box = fullbox("mdhd", 0, mdhd);

    std::string hdlr;
    put_u32(hdlr, 0);
    hdlr += "vide";
    for (int i = 0; i < 3; ++i) put_u32(hdlr, 0);
    hdlr += "VideoHandler";
    put_u8(hdlr, 0);
    std::string hdlr_box = fullbox("hdlr", 0, hdlr);

    std::string vmhd;
    put_u16(vmhd, 0); put_u16(vmhd, 0); put_u16(vmhd, 0); put_u16(vmhd, 0);
    std::string vmhd_box = fullbox("vmhd", 0x00000001, vmhd);

    std::string url = fullbox("url ", 0x00000001, "");
    std::string dref;
    put_u32(dref, 1); dref += url;
    std::string dref_box = fullbox("dref", 0, dref);
    std::string dinf = box("dinf", dref_box);

    std::string stsd;
    put_u32(stsd, 1);
    stsd += is_hevc ? build_hevc1(cw, ch, p) : build_avc1(cw, ch, p);
    std::string stsd_box = fullbox("stsd", 0, stsd);

    std::string stts;
    put_u32(stts, 1);
    put_u32(stts, static_cast<uint32_t>(num_frames));
    put_u32(stts, sample_delta);
    std::string stts_box = fullbox("stts", 0, stts);

    std::string stsc;
    put_u32(stsc, 1);
    put_u32(stsc, 1); put_u32(stsc, static_cast<uint32_t>(num_frames)); put_u32(stsc, 1);
    std::string stsc_box = fullbox("stsc", 0, stsc);

    std::string stsz;
    put_u32(stsz, 0); put_u32(stsz, static_cast<uint32_t>(num_frames));
    for (std::size_t i = 0; i < static_cast<size_t>(num_frames); ++i) {
        put_u32(stsz, static_cast<uint32_t>(p.samples[i].size()));
    }
    std::string stsz_box = fullbox("stsz", 0, stsz);

    std::string stco;
    put_u32(stco, 1);
    put_u32(stco, 0);  // 占位，下方回填
    std::string stco_box = fullbox("stco", 0, stco);

    std::string stbl = box("stbl", stsd_box + stts_box + stsc_box + stsz_box + stco_box);
    std::string minf = box("minf", vmhd_box + dinf + stbl);
    std::string mdia = box("mdia", mdhd_box + hdlr_box + minf);
    std::string trak = box("trak", tkhd_box + edts + mdia);
    std::string moov = box("moov", mvhd_box + trak);

    const uint32_t chunk_offset = static_cast<uint32_t>(ftyp_box.size() + moov.size() + 8);
    size_t off = stco_box.size() - 4;
    stco_box[off] = static_cast<char>((chunk_offset >> 24) & 0xFF);
    stco_box[off + 1] = static_cast<char>((chunk_offset >> 16) & 0xFF);
    stco_box[off + 2] = static_cast<char>((chunk_offset >> 8) & 0xFF);
    stco_box[off + 3] = static_cast<char>(chunk_offset & 0xFF);
    stbl = box("stbl", stsd_box + stts_box + stsc_box + stsz_box + stco_box);
    minf = box("minf", vmhd_box + dinf + stbl);
    mdia = box("mdia", mdhd_box + hdlr_box + minf);
    trak = box("trak", tkhd_box + edts + mdia);
    moov = box("moov", mvhd_box + trak);

    std::string mdat_payload;
    for (const auto& s : p.samples) mdat_payload += s;
    std::string mdat = box("mdat", mdat_payload);

    return ftyp_box + moov + mdat;
}

}  // namespace termchart::image
