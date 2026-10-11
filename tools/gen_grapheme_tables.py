#!/usr/bin/env python3
"""
gen_grapheme_tables.py —— 生成 include/termchart/detail/grapheme_tables.hpp

产出一个**简化版 UAX #29** 的 Grapheme_Cluster_Break 属性区间表：
遗漏全部的非 ICU 场景不需要的那些精细规则，但保留 GB1–GB999 判定的全部
必要类别（CR / LF / Control / Extend / ZWJ / Regional_Indicator /
Prepend / SpacingMark / Hangul L·V·T·LV·LVT / Any）。

真实分割算法在 `util/grapheme.hpp`（L1），本文件只提供数据。
"由本脚本生成，禁手改"——改区间必须改这里并跑 check_generated.sh。
"""
from __future__ import annotations

import sys
from pathlib import Path

from _genutil import ROOT, emit, wants_check

OUT = ROOT / "include" / "termchart" / "detail" / "grapheme_tables.hpp"

HEADER = """// grapheme_tables.hpp —— 由 tools/gen_grapheme_tables.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
// 只提供 Grapheme_Cluster_Break 属性（UAX #29 简化版）的区间数据。
// 分割算法在 util/grapheme.hpp（L1）；core/grapheme.hpp 只定义数据结构。
#ifndef TERMCHART_DETAIL_GRAPHEME_TABLES_HPP
#define TERMCHART_DETAIL_GRAPHEME_TABLES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace termchart::detail {

// Grapheme_Cluster_Break 属性值。取值与 UAX #29 同名。
enum class Gb : std::uint8_t {
    Any = 0,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RegionalIndicator,
    Prepend,
    SpacingMark,
    L,          // Hangul 初声
    V,          // Hangul 中声
    T,          // Hangul 终声
    LV,         // Hangul LV 音节
    LVT,        // Hangul LVT 音节
};

// 区间表项：[first, last] 闭区间。
struct GbRange {
    std::uint32_t first;
    std::uint32_t last;
    Gb value;
};

%s

// 二分查找：返回码点的 Grapheme_Cluster_Break 类别，缺省 Gb::Any。
constexpr Gb grapheme_class(std::uint32_t cp) noexcept {
    std::size_t lo = 0;
    std::size_t hi = kGbRanges.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const GbRange& r = kGbRanges[mid];
        if (cp < r.first) {
            hi = mid;
        } else if (cp > r.last) {
            lo = mid + 1;
        } else {
            return r.value;
        }
    }
    return Gb::Any;
}

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_GRAPHEME_TABLES_HPP
"""

# (first, last, Gb) —— 覆盖 UAX #29 判定所需的全部类别。
RANGES: list[tuple[int, int, str]] = [
    # ── Control ───────────────────────────────────────────────────────
    (0x0000, 0x0009, "Control"),
    (0x000A, 0x000A, "LF"),
    (0x000B, 0x000C, "Control"),
    (0x000D, 0x000D, "CR"),
    (0x000E, 0x001F, "Control"),
    (0x007F, 0x009F, "Control"),
    (0x00AD, 0x00AD, "Control"),
    (0x0378, 0x0378, "Control"),
    # 0x0600–0x0605 / 0x070F / 0x08E2 / 0x110BD 虽是 Cf，但 UAX #29 把它们归为
    # Prepend：**一个码点只能有一个 GCB 值**，Prepend 优先，故这里不列。
    (0x061C, 0x061C, "Control"),
    (0x08B5, 0x08B5, "Control"),
    (0x1D159, 0x1D159, "Control"),
    (0x1D173, 0x1D17A, "Control"),
    (0xE0001, 0xE0001, "Control"),
    (0xE0020, 0xE007F, "Control"),
    (0xE0099, 0xE0099, "Control"),
    # ── Extend（含 emoji 变体选择符、肤色修饰符、组合记号） ───────────
    (0x0300, 0x036F, "Extend"),
    (0x0483, 0x0489, "Extend"),
    (0x0591, 0x05BD, "Extend"),
    (0x05BF, 0x05BF, "Extend"),
    (0x05C1, 0x05C2, "Extend"),
    (0x0610, 0x061A, "Extend"),
    (0x064B, 0x065F, "Extend"),
    (0x0670, 0x0670, "Extend"),
    (0x06D6, 0x06DC, "Extend"),
    (0x06DF, 0x06E4, "Extend"),
    (0x06E7, 0x06E8, "Extend"),
    (0x0711, 0x0711, "Extend"),
    (0x0730, 0x074A, "Extend"),
    (0x07A6, 0x07B0, "Extend"),
    (0x07EB, 0x07F3, "Extend"),
    (0x0816, 0x0819, "Extend"),
    (0x081B, 0x0823, "Extend"),
    (0x0825, 0x0827, "Extend"),
    (0x0829, 0x082D, "Extend"),
    (0x0859, 0x085B, "Extend"),
    (0x08D4, 0x08E1, "Extend"),
    (0x08E3, 0x0902, "Extend"),
    (0x093A, 0x093A, "Extend"),
    (0x093C, 0x093C, "Extend"),
    (0x0941, 0x0948, "Extend"),
    (0x094D, 0x094D, "Extend"),
    (0x0951, 0x0957, "Extend"),
    (0x0962, 0x0963, "Extend"),
    (0x0981, 0x0981, "Extend"),
    (0x09BC, 0x09BC, "Extend"),
    (0x09C1, 0x09C4, "Extend"),
    (0x09CD, 0x09CD, "Extend"),
    (0x0A01, 0x0A02, "Extend"),
    (0x0A3C, 0x0A3C, "Extend"),
    (0x0A41, 0x0A42, "Extend"),
    (0x0A47, 0x0A48, "Extend"),
    (0x0A4B, 0x0A4D, "Extend"),
    (0x0A70, 0x0A71, "Extend"),
    (0x0A81, 0x0A82, "Extend"),
    (0x0ABC, 0x0ABC, "Extend"),
    (0x0AC1, 0x0AC5, "Extend"),
    (0x0AC7, 0x0AC8, "Extend"),
    (0x0ACD, 0x0ACD, "Extend"),
    (0x0B01, 0x0B01, "Extend"),
    (0x0B3C, 0x0B3C, "Extend"),
    (0x0B3F, 0x0B3F, "Extend"),
    (0x0B41, 0x0B44, "Extend"),
    (0x0B4D, 0x0B4D, "Extend"),
    (0x0B82, 0x0B82, "Extend"),
    (0x0BC0, 0x0BC0, "Extend"),
    (0x0BCD, 0x0BCD, "Extend"),
    (0x0C00, 0x0C00, "Extend"),
    (0x0C04, 0x0C04, "Extend"),
    (0x0C3E, 0x0C40, "Extend"),
    (0x0C46, 0x0C48, "Extend"),
    (0x0C4A, 0x0C4D, "Extend"),
    (0x0D01, 0x0D01, "Extend"),
    (0x0D3B, 0x0D3C, "Extend"),
    (0x0D41, 0x0D44, "Extend"),
    (0x0D4D, 0x0D4D, "Extend"),
    (0x0E31, 0x0E31, "Extend"),
    (0x0E34, 0x0E3A, "Extend"),
    (0x0E47, 0x0E4E, "Extend"),
    (0x0EB1, 0x0EB1, "Extend"),
    (0x0EB4, 0x0EBC, "Extend"),
    (0x0EC8, 0x0ECD, "Extend"),
    (0x0F18, 0x0F19, "Extend"),
    (0x0F35, 0x0F35, "Extend"),
    (0x0F37, 0x0F37, "Extend"),
    (0x0F39, 0x0F39, "Extend"),
    (0x0F71, 0x0F7E, "Extend"),
    (0x0F80, 0x0F84, "Extend"),
    (0x0F86, 0x0F87, "Extend"),
    (0x0FB0, 0x0FB4, "Extend"),
    (0x102D, 0x1030, "Extend"),
    (0x1032, 0x1037, "Extend"),
    (0x1039, 0x103A, "Extend"),
    (0x103D, 0x103E, "Extend"),
    (0x1058, 0x1059, "Extend"),
    (0x105E, 0x1060, "Extend"),
    (0x1071, 0x1074, "Extend"),
    (0x1082, 0x1082, "Extend"),
    (0x135D, 0x135F, "Extend"),
    (0x1712, 0x1714, "Extend"),
    (0x1732, 0x1734, "Extend"),
    (0x1752, 0x1753, "Extend"),
    (0x1772, 0x1773, "Extend"),
    (0x17B4, 0x17B5, "Extend"),
    (0x17B7, 0x17BD, "Extend"),
    (0x17C6, 0x17C6, "Extend"),
    (0x17C9, 0x17D3, "Extend"),
    (0x17DD, 0x17DD, "Extend"),
    (0x180B, 0x180D, "Extend"),
    (0x18A9, 0x18A9, "Extend"),
    (0x1920, 0x1922, "Extend"),
    (0x1927, 0x1928, "Extend"),
    (0x1932, 0x1932, "Extend"),
    (0x1939, 0x193B, "Extend"),
    (0x1A17, 0x1A18, "Extend"),
    (0x1A1B, 0x1A1B, "Extend"),
    (0x1A56, 0x1A56, "Extend"),
    (0x1A58, 0x1A5E, "Extend"),
    (0x1A60, 0x1A60, "Extend"),
    (0x1A62, 0x1A62, "Extend"),
    (0x1A65, 0x1A6C, "Extend"),
    (0x1A73, 0x1A7C, "Extend"),
    (0x1A7F, 0x1A7F, "Extend"),
    (0x1AB0, 0x1ACE, "Extend"),
    (0x1B00, 0x1B03, "Extend"),
    (0x1B34, 0x1B34, "Extend"),
    (0x1B36, 0x1B3A, "Extend"),
    (0x1B3C, 0x1B3C, "Extend"),
    (0x1B42, 0x1B42, "Extend"),
    (0x1B6B, 0x1B73, "Extend"),
    (0x1B80, 0x1B81, "Extend"),
    (0x1BA2, 0x1BA5, "Extend"),
    (0x1BA8, 0x1BA9, "Extend"),
    (0x1BAB, 0x1BAD, "Extend"),
    (0x1BE6, 0x1BE6, "Extend"),
    (0x1BE8, 0x1BE9, "Extend"),
    (0x1BED, 0x1BED, "Extend"),
    (0x1BEF, 0x1BF1, "Extend"),
    (0x1C2C, 0x1C33, "Extend"),
    (0x1C36, 0x1C37, "Extend"),
    (0x1CD0, 0x1CD2, "Extend"),
    (0x1CD4, 0x1CE0, "Extend"),
    (0x1CE2, 0x1CE8, "Extend"),
    (0x1CED, 0x1CED, "Extend"),
    (0x1CF4, 0x1CF4, "Extend"),
    (0x1CF8, 0x1CF9, "Extend"),
    (0x1DC0, 0x1DF5, "Extend"),
    (0x1DFC, 0x1DFF, "Extend"),
    (0x20D0, 0x20F0, "Extend"),
    (0x2CEF, 0x2CF1, "Extend"),
    (0x2D7F, 0x2D7F, "Extend"),
    (0x2DE0, 0x2DFF, "Extend"),
    (0x3099, 0x309A, "Extend"),
    (0xA66F, 0xA672, "Extend"),
    (0xA674, 0xA67D, "Extend"),
    (0xA69E, 0xA69F, "Extend"),
    (0xA6F0, 0xA6F1, "Extend"),
    (0xA802, 0xA802, "Extend"),
    (0xA806, 0xA806, "Extend"),
    (0xA80B, 0xA80B, "Extend"),
    (0xA825, 0xA826, "Extend"),
    (0xA8C4, 0xA8C5, "Extend"),
    (0xA8E0, 0xA8F1, "Extend"),
    (0xA926, 0xA92D, "Extend"),
    (0xA947, 0xA951, "Extend"),
    (0xA980, 0xA982, "Extend"),
    (0xA9B3, 0xA9B3, "Extend"),
    (0xA9B6, 0xA9B9, "Extend"),
    (0xA9BC, 0xA9BC, "Extend"),
    (0xA9E5, 0xA9E5, "Extend"),
    (0xAA29, 0xAA2E, "Extend"),
    (0xAA31, 0xAA32, "Extend"),
    (0xAA35, 0xAA36, "Extend"),
    (0xAA43, 0xAA43, "Extend"),
    (0xAA4C, 0xAA4C, "Extend"),
    (0xAA7C, 0xAA7C, "Extend"),
    (0xAAB0, 0xAAB0, "Extend"),
    (0xAAB2, 0xAAB4, "Extend"),
    (0xAAB7, 0xAAB8, "Extend"),
    (0xAABE, 0xAABF, "Extend"),
    (0xAAC1, 0xAAC1, "Extend"),
    (0xAAEC, 0xAAED, "Extend"),
    (0xAAF6, 0xAAF6, "Extend"),
    (0xABE5, 0xABE5, "Extend"),
    (0xABE8, 0xABE8, "Extend"),
    (0xABED, 0xABED, "Extend"),
    (0xFB1E, 0xFB1E, "Extend"),
    (0xFE00, 0xFE0F, "Extend"),      # 变体选择符 VS1–VS16
    (0xFE20, 0xFE2F, "Extend"),
    (0x1F3FB, 0x1F3FF, "Extend"),     # emoji 肤色修饰符
    (0xE0100, 0xE01EF, "Extend"),     # VS17–VS256
    # ── ZWJ ───────────────────────────────────────────────────────────
    (0x200D, 0x200D, "ZWJ"),
    # ── Regional_Indicator ────────────────────────────────────────────
    (0x1F1E6, 0x1F1FF, "RegionalIndicator"),
    # ── Prepend ───────────────────────────────────────────────────────
    (0x0600, 0x0605, "Prepend"),
    (0x06DD, 0x06DD, "Prepend"),
    (0x070F, 0x070F, "Prepend"),
    (0x0890, 0x0891, "Prepend"),
    (0x08E2, 0x08E2, "Prepend"),
    (0x0D4E, 0x0D4E, "Prepend"),
    (0x110BD, 0x110BD, "Prepend"),
    (0x110CD, 0x110CD, "Prepend"),
    (0x111C2, 0x111C3, "Prepend"),
    (0x1193F, 0x1193F, "Prepend"),
    (0x11941, 0x11941, "Prepend"),
    (0x11A3A, 0x11A3A, "Prepend"),
    (0x11A84, 0x11A89, "Prepend"),
    (0x11D46, 0x11D46, "Prepend"),
    (0x11F02, 0x11F02, "Prepend"),
    (0x1E2FF, 0x1E2FF, "Prepend"),
    # ── SpacingMark ───────────────────────────────────────────────────
    (0x0903, 0x0903, "SpacingMark"),
    (0x093B, 0x093B, "SpacingMark"),
    (0x093E, 0x0940, "SpacingMark"),
    (0x0949, 0x094C, "SpacingMark"),
    (0x094E, 0x094F, "SpacingMark"),
    (0x0982, 0x0983, "SpacingMark"),
    (0x09BE, 0x09C0, "SpacingMark"),
    (0x09C7, 0x09C8, "SpacingMark"),
    (0x09CB, 0x09CC, "SpacingMark"),
    (0x09D7, 0x09D7, "SpacingMark"),
    (0x0A03, 0x0A03, "SpacingMark"),
    (0x0A3E, 0x0A40, "SpacingMark"),
    (0x0A83, 0x0A83, "SpacingMark"),
    (0x0ABE, 0x0AC0, "SpacingMark"),
    (0x0AC9, 0x0AC9, "SpacingMark"),
    (0x0ACB, 0x0ACC, "SpacingMark"),
    (0x0B02, 0x0B03, "SpacingMark"),
    (0x0B3E, 0x0B3E, "SpacingMark"),
    (0x0B40, 0x0B40, "SpacingMark"),
    (0x0B47, 0x0B48, "SpacingMark"),
    (0x0B4B, 0x0B4C, "SpacingMark"),
    (0x0BBE, 0x0BBF, "SpacingMark"),
    (0x0BC1, 0x0BC2, "SpacingMark"),
    (0x0BC6, 0x0BC8, "SpacingMark"),
    (0x0BCA, 0x0BCC, "SpacingMark"),
    (0x0BD7, 0x0BD7, "SpacingMark"),
    (0x0C01, 0x0C03, "SpacingMark"),
    (0x0C41, 0x0C44, "SpacingMark"),
    (0x0C82, 0x0C83, "SpacingMark"),
    (0x0CBE, 0x0CBE, "SpacingMark"),
    (0x0CC0, 0x0CC4, "SpacingMark"),
    (0x0CC7, 0x0CC8, "SpacingMark"),
    (0x0CCA, 0x0CCB, "SpacingMark"),
    (0x0CD5, 0x0CD6, "SpacingMark"),
    (0x0D02, 0x0D03, "SpacingMark"),
    (0x0D3E, 0x0D40, "SpacingMark"),
    (0x0D46, 0x0D48, "SpacingMark"),
    (0x0D4A, 0x0D4C, "SpacingMark"),
    (0x0D57, 0x0D57, "SpacingMark"),
    (0x0D82, 0x0D83, "SpacingMark"),
    (0x0DCF, 0x0DD1, "SpacingMark"),
    (0x0DD8, 0x0DDF, "SpacingMark"),
    (0x0DF2, 0x0DF3, "SpacingMark"),
    (0x0E33, 0x0E33, "SpacingMark"),
    (0x0EB3, 0x0EB3, "SpacingMark"),
    (0x0F3E, 0x0F3F, "SpacingMark"),
    (0x0F7F, 0x0F7F, "SpacingMark"),
    (0x102B, 0x102C, "SpacingMark"),
    (0x1031, 0x1031, "SpacingMark"),
    (0x1038, 0x1038, "SpacingMark"),
    (0x103B, 0x103C, "SpacingMark"),
    (0x1056, 0x1057, "SpacingMark"),
    (0x1062, 0x1064, "SpacingMark"),
    (0x1067, 0x106D, "SpacingMark"),
    (0x1083, 0x1084, "SpacingMark"),
    (0x108F, 0x108F, "SpacingMark"),
    (0x109A, 0x109B, "SpacingMark"),
    (0x17B6, 0x17B6, "SpacingMark"),
    (0x17BE, 0x17C5, "SpacingMark"),
    (0x17C7, 0x17C8, "SpacingMark"),
    (0x1923, 0x1926, "SpacingMark"),
    (0x1929, 0x192B, "SpacingMark"),
    (0x1930, 0x1931, "SpacingMark"),
    (0x1933, 0x1938, "SpacingMark"),
    (0x19B0, 0x19C0, "SpacingMark"),
    (0x19C8, 0x19C9, "SpacingMark"),
    (0x1A19, 0x1A1A, "SpacingMark"),
    (0x1A55, 0x1A55, "SpacingMark"),
    (0x1A57, 0x1A57, "SpacingMark"),
    (0x1A61, 0x1A61, "SpacingMark"),
    (0x1A63, 0x1A64, "SpacingMark"),
    (0x1A6D, 0x1A72, "SpacingMark"),
    (0x1B04, 0x1B04, "SpacingMark"),
    (0x1B35, 0x1B35, "SpacingMark"),
    (0x1B3B, 0x1B3B, "SpacingMark"),
    (0x1B3D, 0x1B41, "SpacingMark"),
    (0x1B43, 0x1B44, "SpacingMark"),
    (0x1B82, 0x1B82, "SpacingMark"),
    (0x1BA1, 0x1BA1, "SpacingMark"),
    (0x1BA6, 0x1BA7, "SpacingMark"),
    (0x1BAA, 0x1BAA, "SpacingMark"),
    (0x1BE7, 0x1BE7, "SpacingMark"),
    (0x1BEA, 0x1BEC, "SpacingMark"),
    (0x1BEE, 0x1BEE, "SpacingMark"),
    (0x1BF2, 0x1BF3, "SpacingMark"),
    (0x1C24, 0x1C2B, "SpacingMark"),
    (0x1C34, 0x1C35, "SpacingMark"),
    (0x1CE1, 0x1CE1, "SpacingMark"),
    (0x1CF2, 0x1CF3, "SpacingMark"),
    (0x1CF7, 0x1CF7, "SpacingMark"),
    (0x302E, 0x302F, "SpacingMark"),
    (0xA823, 0xA824, "SpacingMark"),
    (0xA827, 0xA827, "SpacingMark"),
    (0xA880, 0xA881, "SpacingMark"),
    (0xA8B4, 0xA8C3, "SpacingMark"),
    (0xA952, 0xA953, "SpacingMark"),
    (0xA983, 0xA983, "SpacingMark"),
    (0xA9B4, 0xA9B5, "SpacingMark"),
    (0xA9BA, 0xA9BB, "SpacingMark"),
    (0xA9BD, 0xA9C0, "SpacingMark"),
    (0xAA2F, 0xAA30, "SpacingMark"),
    (0xAA33, 0xAA34, "SpacingMark"),
    (0xAA4D, 0xAA4F, "SpacingMark"),
    (0xAA7B, 0xAA7B, "SpacingMark"),
    (0xAA7D, 0xAA7D, "SpacingMark"),
    (0xAAB1, 0xAAB1, "SpacingMark"),
    (0xAAB5, 0xAAB6, "SpacingMark"),
    (0xAAB9, 0xAABD, "SpacingMark"),
    (0xAAC0, 0xAAC0, "SpacingMark"),
    (0xAAC2, 0xAAC2, "SpacingMark"),
    (0xAAF5, 0xAAF5, "SpacingMark"),
    (0xABE3, 0xABE4, "SpacingMark"),
    (0xABE6, 0xABE7, "SpacingMark"),
    (0xABE9, 0xABEA, "SpacingMark"),
    (0xABEC, 0xABEC, "SpacingMark"),
    # U+FB1E 的 GCB 是 Extend（上面已列），不在此重复。
    (0x11300, 0x11301, "SpacingMark"),
    (0x114B0, 0x114C1, "SpacingMark"),
    (0x115AF, 0x115B4, "SpacingMark"),
    (0x11930, 0x1193B, "SpacingMark"),
    (0x11A33, 0x11A38, "SpacingMark"),
    (0x11A47, 0x11A51, "SpacingMark"),
    (0x11A99, 0x11A9F, "SpacingMark"),
    (0x11D42, 0x11D44, "SpacingMark"),
    (0x11F00, 0x11F01, "SpacingMark"),
    (0x1D165, 0x1D166, "SpacingMark"),
    (0x1D16D, 0x1D172, "SpacingMark"),
    # ── Hangul ────────────────────────────────────────────────────────
    (0x1100, 0x115F, "L"),
    (0xA960, 0xA97C, "L"),
    (0x1160, 0x11A7, "V"),
    (0xD7B0, 0xD7C6, "V"),
    (0x11A8, 0x11FF, "T"),
    (0xD7CB, 0xD7FB, "T"),
    # 音节块（AC00–D7A3）不能用两个大区间表示：LV 与 LVT 是**交替**的
    # （每 28 个音节一组，组内第 0 个是 LV，其余 27 个是 LVT），
    # 由 hangul_syllable_ranges() 展开成精确的区间序列。
]

# Hangul 音节块：AC00 + 28*k 为 LV，其后 27 个为 LVT（k = 0..399）。
kSyllableBase = 0xAC00
kSyllableLast = 0xD7A3
kJamoTCount = 28


def hangul_syllable_ranges() -> list[tuple[int, int, str]]:
    out: list[tuple[int, int, str]] = []
    cp = kSyllableBase
    while cp <= kSyllableLast:
        out.append((cp, cp, "LV"))
        if cp + 1 <= kSyllableLast:
            out.append((cp + 1, min(cp + kJamoTCount - 1, kSyllableLast), "LVT"))
        cp += kJamoTCount
    return out


def build_ranges() -> list[tuple[int, int, str]]:
    """按首码点升序展开；重叠是数据错误（二分查找依赖"有序且互不相交"）。"""
    ranges = RANGES + hangul_syllable_ranges()
    ranges.sort(key=lambda item: (item[0], item[1]))
    for prev, cur in zip(ranges, ranges[1:]):
        if cur[0] <= prev[1]:
            raise SystemExit(f"区间重叠或无序：{prev} 与 {cur}")
    return ranges


def render() -> str:
    ranges = build_ranges()
    rows = ",\n".join(
        "    {0x%04X, 0x%04X, Gb::%s}" % (a, b, kind) for a, b, kind in ranges
    )
    table = (
        "// clang-format off\n"
        "inline constexpr std::size_t kGbRangeCount = %d;\n"
        "inline constexpr std::array<GbRange, kGbRangeCount> kGbRanges{{\n%s,\n}};\n"
        "// clang-format on\n" % (len(ranges), rows)
    )
    return HEADER % table


def main(argv: list[str]) -> int:
    changed = emit(OUT, render(), wants_check(argv))
    return 1 if changed and wants_check(argv) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
