// overlay_cells.hpp —— 覆盖层的"改写格簿记"共享实现（interaction 模块私有 detail）
//
// 为什么单独抽出来：crosshair 与 tooltip 都要"记住被覆盖的格 → 精确还原"，此前
// 各自复制了一份（~25 行）。还原口径一旦分叉，就会出现"某个覆盖层悬浮一次永久污染
// 画布"的隐形 bug，且很难被单测发现。把唯一一份算法放这里，两个覆盖层共用。
//
// 为什么用模板而不是具体类型：两个覆盖层各自持有**私有嵌套**的 Patch 结构体
// （仅存 x/y/原格），以 Patch 作模板参数即可在**不改动各自公开头**的前提下复用；
// 本头从而不引入任何具体类型、不产生外部符号（纯模板 + inline）。
//
// 物理布局：本头位于 include/.../detail/，按仓库约定 header-only、无同名 .cpp。
// tools/check_header_only.py 只豁免**顶层** detail/，故本头已登记在 cmake/header_only.txt。
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "termchart/canvas/canvas.hpp"

namespace termchart::interaction::detail {

// 开始一次新的绘制：清空补丁，并按画布尺寸重建"去重标记表"（width × height，全 0）。
// 去重的意义：一次绘制内横线、竖线、读数文本可能覆盖同一格，只有**第一次**看到的
// 原值才是真正的"绘制前状态"，后续重复记录会把被改写的值当成原值、还原不干净。
template <class Patch>
inline void begin_log(std::vector<Patch>& patches, std::vector<std::uint8_t>& markers,
                      std::uint32_t width, std::uint32_t height) {
    patches.clear();
    markers.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                   std::uint8_t{0});
}

// 记录 (x,y) 的原格：画布外或已记录则忽略。width 与 begin_log 传的必须一致
// （用于把二维坐标压成线性索引）。
template <class Patch>
inline void capture_cell(const canvas::Canvas& canvas, std::vector<Patch>& patches,
                         std::vector<std::uint8_t>& markers, std::uint32_t width,
                         std::uint32_t x, std::uint32_t y) {
    if (x >= canvas.width() || y >= canvas.height()) {
        return;  // 画布外：既不记录也不绘制
    }
    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(x);
    if (index >= markers.size() || markers[index] != 0) {
        return;  // 已记录：保留第一次的原值
    }
    markers[index] = 1;
    patches.push_back(Patch{x, y, canvas.at(x, y)});
}

// 逐格还原（原字符 + 原样式）并清空；返回是否真的还原过任何格。
// patches 为空时是纯 no-op（返回 false），调用方可据此跳过无谓重绘。
template <class Patch>
inline bool restore_cells(canvas::Canvas& canvas, std::vector<Patch>& patches,
                          std::vector<std::uint8_t>& markers) {
    if (patches.empty()) {
        return false;
    }
    for (const Patch& patch : patches) {
        canvas.put(patch.x, patch.y, patch.before.cp, patch.before.style);
    }
    patches.clear();
    markers.clear();
    return true;
}

}  // namespace termchart::interaction::detail
