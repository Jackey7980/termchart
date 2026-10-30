// gif_encode.cpp —— 自写 GIF89a 编码器（I_PCM / 自包含 LZW / median-cut 调色板）
#include "termchart/image/gif_encode.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace termchart::image {
namespace {

struct RGB {
    std::uint8_t r = 0, g = 0, b = 0;
};

// ── 调色板量化：median-cut，最多 max_colors 色 ────────────────────────────────
struct Palette {
    std::vector<RGB> colors;
    bool exact = false;  // true：colors 即原始唯一色集合，可精确映射（无损）
};

Palette build_palette(std::span<const Bitmap> frames, int max_colors) {
    std::unordered_map<std::uint32_t, int> hist;
    for (const auto& f : frames) {
        for (std::uint32_t y = 0; y < f.height(); ++y)
            for (std::uint32_t x = 0; x < f.width(); ++x) {
                const std::uint32_t c = (std::uint32_t(f.at(x, y, 0)) << 16) |
                                        (std::uint32_t(f.at(x, y, 1)) << 8) |
                                        std::uint32_t(f.at(x, y, 2));
                hist[c]++;
            }
    }
    std::vector<std::pair<RGB, int>> items;
    items.reserve(hist.size());
    for (const auto& kv : hist) {
        items.push_back({{static_cast<std::uint8_t>(kv.first >> 16),
                          static_cast<std::uint8_t>((kv.first >> 8) & 0xFF),
                          static_cast<std::uint8_t>(kv.first & 0xFF)},
                         kv.second});
    }
    if (static_cast<int>(items.size()) <= max_colors) {
        Palette p;
        p.exact = true;
        p.colors.reserve(items.size());
        for (const auto& it : items) p.colors.push_back(it.first);
        return p;
    }
    // 把 items 反复按最大色域（加权）对半切，直到达到 max_colors 个盒子。
    struct Box {
        std::vector<std::pair<RGB, int>> items;
    };
    std::vector<Box> boxes(1);
    boxes[0].items = std::move(items);
    while (static_cast<int>(boxes.size()) < max_colors) {
        int best = -1;
        double best_ext = -1.0;
        int best_ch = 0;
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            if (boxes[i].items.empty()) continue;
            for (int ch = 0; ch < 3; ++ch) {
                int mn = 255, mx = 0;
                for (const auto& it : boxes[i].items) {
                    const int v = ch == 0 ? it.first.r : ch == 1 ? it.first.g : it.first.b;
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                }
                const double ext = static_cast<double>(mx - mn);
                if (ext > best_ext) {
                    best_ext = ext;
                    best = i;
                    best_ch = ch;
                }
            }
        }
        if (best < 0 || best_ext <= 0.0) break;
        auto& b = boxes[best];
        std::sort(b.items.begin(), b.items.end(), [&](const std::pair<RGB, int>& a,
                                                      const std::pair<RGB, int>& c) {
            const int va = best_ch == 0 ? a.first.r : best_ch == 1 ? a.first.g : a.first.b;
            const int vc = best_ch == 0 ? c.first.r : best_ch == 1 ? c.first.g : c.first.b;
            return va < vc;
        });
        long total = 0;
        for (const auto& it : b.items) total += it.second;
        long half = 0;
        int split = 1;
        for (int i = 0; i < static_cast<int>(b.items.size()); ++i) {
            half += b.items[i].second;
            if (half >= total / 2) {
                split = i + 1;
                break;
            }
        }
        if (split <= 0 || split >= static_cast<int>(b.items.size())) break;
        Box nb;
        nb.items.assign(b.items.begin() + split, b.items.end());
        b.items.resize(split);
        boxes.push_back(std::move(nb));
    }
    Palette p;
    p.exact = false;
    p.colors.reserve(boxes.size());
    for (const auto& b : boxes) {
        long sr = 0, sg = 0, sb = 0, tot = 0;
        for (const auto& it : b.items) {
            sr += static_cast<long>(it.first.r) * it.second;
            sg += static_cast<long>(it.first.g) * it.second;
            sb += static_cast<long>(it.first.b) * it.second;
            tot += it.second;
        }
        if (tot == 0) {
            p.colors.push_back({0, 0, 0});
            continue;
        }
        p.colors.push_back({static_cast<std::uint8_t>(sr / tot), static_cast<std::uint8_t>(sg / tot),
                            static_cast<std::uint8_t>(sb / tot)});
    }
    return p;
}

// 像素颜色 → 调色板索引（精确调色板直接查；近似调色板走最近邻 + 记忆化）。
std::uint8_t map_pixel(const Palette& pal, std::uint32_t c,
                       std::unordered_map<std::uint32_t, std::uint8_t>& cache) {
    const auto it = cache.find(c);
    if (it != cache.end()) return it->second;
    const std::uint8_t r = static_cast<std::uint8_t>(c >> 16);
    const std::uint8_t g = static_cast<std::uint8_t>((c >> 8) & 0xFF);
    const std::uint8_t b = static_cast<std::uint8_t>(c & 0xFF);
    std::uint8_t idx = 0;
    if (pal.exact) {
        for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(pal.colors.size()); ++i) {
            if (pal.colors[i].r == r && pal.colors[i].g == g && pal.colors[i].b == b) {
                idx = i;
                break;
            }
        }
    } else {
        int best = 0, bestd = 1 << 30;
        for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(pal.colors.size()); ++i) {
            const int dr = pal.colors[i].r - r, dg = pal.colors[i].g - g, db = pal.colors[i].b - b;
            const int d = dr * dr + dg * dg + db * db;
            if (d < bestd) {
                bestd = d;
                best = i;
            }
        }
        idx = static_cast<std::uint8_t>(best);
    }
    cache[c] = idx;
    return idx;
}

// ── GIF LZW（LSB-first 位流；clear=1<<mcs，early-change 增位，满 4096 复位）──
void lzw_compress(const std::vector<std::uint8_t>& in, int mcs, std::string& out) {
    struct BitWriter {
        std::string& s;
        std::uint32_t acc = 0;
        int n = 0;
        explicit BitWriter(std::string& s_) : s(s_) {}
        void put(int code, int bits) {
            acc |= static_cast<std::uint32_t>(code) << n;
            n += bits;
            while (n >= 8) {
                s.push_back(static_cast<char>(acc & 0xFF));
                acc >>= 8;
                n -= 8;
            }
        }
        void flush() {
            if (n > 0) {
                s.push_back(static_cast<char>(acc & 0xFF));
                acc = 0;
                n = 0;
            }
        }
    } bw(out);

    const int clear = 1 << mcs;
    const int endc = clear + 1;
    int code_size = mcs + 1;
    int next_code = endc + 1;
    std::unordered_map<std::string, int> dict;
    std::vector<std::string> code_str(clear);
    for (int i = 0; i < clear; ++i) code_str[i] = std::string(1, static_cast<char>(i));
    auto reset = [&]() {
        dict.clear();
        next_code = endc + 1;
        code_size = mcs + 1;
    };

    bw.put(clear, code_size);
    if (in.empty()) {
        bw.put(endc, code_size);
        bw.flush();
        return;
    }
    int prefix = in[0];
    for (size_t i = 1; i < in.size(); ++i) {
        const int k = in[i];
        const std::string key = code_str[prefix] + static_cast<char>(k);
        const auto it = dict.find(key);
        if (it != dict.end()) {
            prefix = it->second;
        } else {
            bw.put(prefix, code_size);
            dict[key] = next_code;
            if (code_str.size() <= static_cast<size_t>(next_code)) code_str.resize(next_code + 1);
            code_str[next_code] = key;
            ++next_code;
            if (next_code == (1 << code_size) && code_size < 12) ++code_size;
            if (next_code == 4096) {
                bw.put(clear, code_size);
                reset();
            }
            prefix = k;
        }
    }
    bw.put(prefix, code_size);
    bw.put(endc, code_size);
    bw.flush();
}

void put_u16le(std::string& s, std::uint16_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
}

// 把一段 LZW 数据按 ≤255 字节子块写入（GIF 数据块约定），末尾补 0x00 终止符。
void write_subblocks(std::string& out, const std::string& data) {
    size_t i = 0;
    while (i < data.size()) {
        const size_t n = std::min<size_t>(data.size() - i, 255);
        out.push_back(static_cast<char>(n));
        out.append(data.substr(i, n));
        i += n;
    }
    out.push_back(static_cast<char>(0));
}

}  // namespace

std::string encode_gif(std::span<const Bitmap> frames, const EncodeOptions& opts) {
    if (frames.empty()) return std::string{};
    const std::uint32_t W = frames[0].width();
    const std::uint32_t H = frames[0].height();
    if (W == 0 || H == 0) return std::string{};
    for (const auto& f : frames) {
        if (f.width() != W || f.height() != H) return std::string{};  // 要求统一尺寸
    }

    const Palette pal = build_palette(frames, 256);
    int ncolors = static_cast<int>(pal.colors.size());
    if (ncolors == 0) ncolors = 1;
    int gct_size = 2;
    while (gct_size < ncolors) gct_size <<= 1;
    if (gct_size > 256) gct_size = 256;
    int bits = 0;
    for (int t = gct_size; t > 1; t >>= 1) ++bits;  // log2(gct_size)
    const int mcs = bits < 2 ? 2 : bits;            // LZW 最小码长 ≥ 2

    std::string out;
    out.append("GIF89a", 6);
    // 逻辑屏幕描述符
    put_u16le(out, static_cast<std::uint16_t>(W));
    put_u16le(out, static_cast<std::uint16_t>(H));
    out.push_back(static_cast<char>(0x80 | ((bits - 1) << 4) | (bits - 1)));  // GCT 标志+尺寸
    out.push_back(static_cast<char>(0));  // 背景色索引
    out.push_back(static_cast<char>(0));  // 像素宽高比
    // 全局调色板（补齐到 gct_size 项）
    for (int i = 0; i < gct_size; ++i) {
        if (i < ncolors) {
            out.push_back(static_cast<char>(pal.colors[i].r));
            out.push_back(static_cast<char>(pal.colors[i].g));
            out.push_back(static_cast<char>(pal.colors[i].b));
        } else {
            out.append(3, static_cast<char>(0));
        }
    }
    // NETSCAPE2.0 循环扩展（loop_count=0 表示无限循环）
    out.push_back(static_cast<char>(0x21));
    out.push_back(static_cast<char>(0xFF));
    out.push_back(static_cast<char>(0x0B));
    out.append("NETSCAPE2.0", 11);
    out.push_back(static_cast<char>(0x03));
    out.push_back(static_cast<char>(0x01));
    put_u16le(out, static_cast<std::uint16_t>(opts.loop_count & 0xFFFF));
    out.push_back(static_cast<char>(0x00));

    // 帧间隔（厘秒）
    int delay_cs;
    if (opts.delay_ms > 0) {
        delay_cs = (opts.delay_ms + 5) / 10;
    } else {
        const double fps = opts.fps > 0 ? static_cast<double>(opts.fps) : 25.0;
        delay_cs = static_cast<int>(std::lround(100.0 / fps));
    }
    if (delay_cs < 1) delay_cs = 1;

    std::unordered_map<std::uint32_t, std::uint8_t> cache;
    for (const auto& f : frames) {
        // 图形控制扩展：disposal=1（不处置），不透明
        out.push_back(static_cast<char>(0x21));
        out.push_back(static_cast<char>(0xF9));
        out.push_back(static_cast<char>(0x04));
        out.push_back(static_cast<char>(0x04));
        put_u16le(out, static_cast<std::uint16_t>(delay_cs));
        out.push_back(static_cast<char>(0x00));  // 透明色索引（未用）
        out.push_back(static_cast<char>(0x00));  // 块终止符
        // 图像描述符
        out.push_back(static_cast<char>(0x2C));
        put_u16le(out, 0);
        put_u16le(out, 0);
        put_u16le(out, static_cast<std::uint16_t>(W));
        put_u16le(out, static_cast<std::uint16_t>(H));
        out.push_back(static_cast<char>(0x00));  // 无局部色表、无交错
        // 图像数据：LZW
        std::vector<std::uint8_t> indices;
        indices.reserve(static_cast<size_t>(W) * H);
        for (std::uint32_t y = 0; y < H; ++y)
            for (std::uint32_t x = 0; x < W; ++x) {
                const std::uint32_t c = (std::uint32_t(f.at(x, y, 0)) << 16) |
                                        (std::uint32_t(f.at(x, y, 1)) << 8) |
                                        std::uint32_t(f.at(x, y, 2));
                indices.push_back(map_pixel(pal, c, cache));
            }
        out.push_back(static_cast<char>(mcs));
        std::string lzw;
        lzw_compress(indices, mcs, lzw);
        write_subblocks(out, lzw);
    }
    out.push_back(static_cast<char>(0x3B));  // trailer
    return out;
}

}  // namespace termchart::image
