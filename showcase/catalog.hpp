// showcase/catalog.hpp —— 全功能 catalog 的 Demo 抽象与分区注册声明
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "catalog_util.hpp"

namespace cat {

// 一个最小演示：给定内里尺寸，返回一个已绘好的面板 CellBuffer（含边框标题）。
struct Demo {
    std::string category;
    std::string name;
    std::function<CellBuffer(std::uint32_t, std::uint32_t)> render;
};

// 三个分区各自登记自己的 Demo（实现在对应 .cpp）。
void register_charts(std::vector<Demo>&);   // 全部 ~72 种图表 + 色彩 + math/data 曲线
void register_graphs(std::vector<Demo>&);   // 全部图/diagram + 布局 + dsl/mermaid
void register_systems(std::vector<Demo>&);  // 后端/协议/交互/图像/核心

}  // namespace cat
