# termchart 文档首页

零强制依赖的 C++20 终端图表库：把数据画进终端字符格，也能导出 SVG / HTML / Markdown / JSON。

## 文档地图

| 目录 | 内容 |
| --- | --- |
| [getting_started.md](getting_started.md) | 从零到第一张图 |
| [installation.md](installation.md) | 各平台构建与集成 |
| [architecture.md](architecture.md) | 模块分层与三条渲染路径 |
| [design/](design/) | 设计决策：分层、差分重绘、Scene/Document IR、错误处理、注册表等 |
| [guide/](guide/) | 使用指南：入门、选型、自定义图表/光栅器/后端、导出、性能 |
| [charts/](charts/) | 图族参考：直角坐标 / 极坐标 / 统计 / 层次 / 金融 / 流向 / 地理 / 文本 / 指标 |
| [graph/](graph/) | 图可视化：流程图、状态机、思维导图、UML、DAG、网络关系、布局算法 |
| [reference/](reference/) | 速查：ANSI 序列、制表符、能力矩阵、模块矩阵、后端对比、FAQ |
| [limits.md](limits.md) | 不可信输入的边界：位置式数组、百分比混列、epoch 数字串、时间格式族 |

## 30 秒预览

```cpp
#include "termchart/api/quick.hpp"

int main() {
    termchart::api::init_core();          // 幂等初始化
    termchart::api::hello();              // 零配置一行出图
}
```

## 快速判断该看哪篇

| 你想… | 看 |
| --- | --- |
| 画折线/柱状/散点 | guide/first_chart.md、charts/cartesian.md |
| 画流程图/思维导图/UML | graph/overview.md |
| 导出 SVG / HTML / JSON | guide/exporting.md、reference/backend_comparison.md |
| 在终端里显示位图 | guide/images.md |
| 解析 Mermaid / PlantUML / DOT | guide/dsl_reference.md |
| 自定义图表 / 光栅器 / 后端 | guide/custom_*.md |
| 遇到显示异常 | reference/faq.md |

## 当前能力口径

以 `docs/reference/capability_matrix.md`（手写）与 `ROADMAP.md`（唯一进度口径）为准；
模块依赖矩阵见 `docs/reference/module_matrix.md`（脚本生成，勿手改）。
