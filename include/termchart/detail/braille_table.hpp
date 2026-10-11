// braille_table.hpp —— 由 tools/gen_braille_table.py 生成，禁手改。
//
// 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
// 盲文点阵 vil：**点 i ↔ bit (i-1)**，码点 = U+2800 | dots，这一约定只在本处定义。
#ifndef TERMCHART_DETAIL_BRAILLE_TABLE_HPP
#define TERMCHART_DETAIL_BRAILLE_TABLE_HPP

#include <array>
#include <cstdint>

namespace termchart::detail {

// 点 i（1..8）的位掩码。
inline constexpr std::uint8_t kBrailleDot1 = 0x01;
inline constexpr std::uint8_t kBrailleDot2 = 0x02;
inline constexpr std::uint8_t kBrailleDot3 = 0x04;
inline constexpr std::uint8_t kBrailleDot4 = 0x08;
inline constexpr std::uint8_t kBrailleDot5 = 0x10;
inline constexpr std::uint8_t kBrailleDot6 = 0x20;
inline constexpr std::uint8_t kBrailleDot7 = 0x40;
inline constexpr std::uint8_t kBrailleDot8 = 0x80;

// 码点 = U+2800 | dots（dots 为上述位掩码的或）。查表仅为显式化该约定。
// clang-format off
inline constexpr std::array<char32_t, 256> kBrailleChars{
    U'\u2800', U'\u2801', U'\u2802', U'\u2803', U'\u2804', U'\u2805', U'\u2806', U'\u2807',
    U'\u2808', U'\u2809', U'\u280A', U'\u280B', U'\u280C', U'\u280D', U'\u280E', U'\u280F',
    U'\u2810', U'\u2811', U'\u2812', U'\u2813', U'\u2814', U'\u2815', U'\u2816', U'\u2817',
    U'\u2818', U'\u2819', U'\u281A', U'\u281B', U'\u281C', U'\u281D', U'\u281E', U'\u281F',
    U'\u2820', U'\u2821', U'\u2822', U'\u2823', U'\u2824', U'\u2825', U'\u2826', U'\u2827',
    U'\u2828', U'\u2829', U'\u282A', U'\u282B', U'\u282C', U'\u282D', U'\u282E', U'\u282F',
    U'\u2830', U'\u2831', U'\u2832', U'\u2833', U'\u2834', U'\u2835', U'\u2836', U'\u2837',
    U'\u2838', U'\u2839', U'\u283A', U'\u283B', U'\u283C', U'\u283D', U'\u283E', U'\u283F',
    U'\u2840', U'\u2841', U'\u2842', U'\u2843', U'\u2844', U'\u2845', U'\u2846', U'\u2847',
    U'\u2848', U'\u2849', U'\u284A', U'\u284B', U'\u284C', U'\u284D', U'\u284E', U'\u284F',
    U'\u2850', U'\u2851', U'\u2852', U'\u2853', U'\u2854', U'\u2855', U'\u2856', U'\u2857',
    U'\u2858', U'\u2859', U'\u285A', U'\u285B', U'\u285C', U'\u285D', U'\u285E', U'\u285F',
    U'\u2860', U'\u2861', U'\u2862', U'\u2863', U'\u2864', U'\u2865', U'\u2866', U'\u2867',
    U'\u2868', U'\u2869', U'\u286A', U'\u286B', U'\u286C', U'\u286D', U'\u286E', U'\u286F',
    U'\u2870', U'\u2871', U'\u2872', U'\u2873', U'\u2874', U'\u2875', U'\u2876', U'\u2877',
    U'\u2878', U'\u2879', U'\u287A', U'\u287B', U'\u287C', U'\u287D', U'\u287E', U'\u287F',
    U'\u2880', U'\u2881', U'\u2882', U'\u2883', U'\u2884', U'\u2885', U'\u2886', U'\u2887',
    U'\u2888', U'\u2889', U'\u288A', U'\u288B', U'\u288C', U'\u288D', U'\u288E', U'\u288F',
    U'\u2890', U'\u2891', U'\u2892', U'\u2893', U'\u2894', U'\u2895', U'\u2896', U'\u2897',
    U'\u2898', U'\u2899', U'\u289A', U'\u289B', U'\u289C', U'\u289D', U'\u289E', U'\u289F',
    U'\u28A0', U'\u28A1', U'\u28A2', U'\u28A3', U'\u28A4', U'\u28A5', U'\u28A6', U'\u28A7',
    U'\u28A8', U'\u28A9', U'\u28AA', U'\u28AB', U'\u28AC', U'\u28AD', U'\u28AE', U'\u28AF',
    U'\u28B0', U'\u28B1', U'\u28B2', U'\u28B3', U'\u28B4', U'\u28B5', U'\u28B6', U'\u28B7',
    U'\u28B8', U'\u28B9', U'\u28BA', U'\u28BB', U'\u28BC', U'\u28BD', U'\u28BE', U'\u28BF',
    U'\u28C0', U'\u28C1', U'\u28C2', U'\u28C3', U'\u28C4', U'\u28C5', U'\u28C6', U'\u28C7',
    U'\u28C8', U'\u28C9', U'\u28CA', U'\u28CB', U'\u28CC', U'\u28CD', U'\u28CE', U'\u28CF',
    U'\u28D0', U'\u28D1', U'\u28D2', U'\u28D3', U'\u28D4', U'\u28D5', U'\u28D6', U'\u28D7',
    U'\u28D8', U'\u28D9', U'\u28DA', U'\u28DB', U'\u28DC', U'\u28DD', U'\u28DE', U'\u28DF',
    U'\u28E0', U'\u28E1', U'\u28E2', U'\u28E3', U'\u28E4', U'\u28E5', U'\u28E6', U'\u28E7',
    U'\u28E8', U'\u28E9', U'\u28EA', U'\u28EB', U'\u28EC', U'\u28ED', U'\u28EE', U'\u28EF',
    U'\u28F0', U'\u28F1', U'\u28F2', U'\u28F3', U'\u28F4', U'\u28F5', U'\u28F6', U'\u28F7',
    U'\u28F8', U'\u28F9', U'\u28FA', U'\u28FB', U'\u28FC', U'\u28FD', U'\u28FE', U'\u28FF'
};
// clang-format on

constexpr char32_t braille_char(std::uint8_t dots) noexcept {
    return kBrailleChars[dots];
}

// 2×4 子像素网格 → 点掩码。约定几何排布：
//     列 0（左）→ dot1/dot2/dot3/dot7     列 1（右）→ dot4/dot5/dot6/dot8
//     行序自上而下：dot1,dot2,dot3 后接 dot7（左列），dot4,dot5,dot6 后接 dot8（右列）
inline constexpr std::array<std::uint8_t, 8> kBrailleCellDots{
    kBrailleDot1,  // 左上  (row 0, col 0)
    kBrailleDot2,  // 左中上(row 1, col 0)
    kBrailleDot3,  // 左中下(row 2, col 0)
    kBrailleDot7,  // 左下  (row 3, col 0)
    kBrailleDot4,  // 右上  (row 0, col 1)
    kBrailleDot5,  // 右中上(row 1, col 1)
    kBrailleDot6,  // 右中下(row 2, col 1)
    kBrailleDot8,  // 右下  (row 3, col 1)
};

// 子像素线性下标（col * 4 + row，先列后行）→ 点掩码。越界返回 0（不点亮）。
constexpr std::uint8_t braille_dots_for_cell(int col, int row) noexcept {
    if (col < 0 || col > 1 || row < 0 || row > 3) {
        return 0;
    }
    return kBrailleCellDots[static_cast<std::size_t>(col) * 4 + static_cast<std::size_t>(row)];
}

// 清空全部点（空白盲文）：U+2800。
inline constexpr char32_t kBrailleBlank = U'\u2800';

}  // namespace termchart::detail

#endif  // TERMCHART_DETAIL_BRAILLE_TABLE_HPP
