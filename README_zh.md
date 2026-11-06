# termchart

在终端里画**真正的图**的 C++20 库：折线 / 柱状 / 散点 / 迷你趋势、流程图 / 状态机 /
思维导图 / 组织架构图 / 力导向图、热力图 / 分形 / 图像预览。输出走**盲文（2×4）与
半块（1×2）子像素**，不是拿字符凑形状。

（英文版与完整细节见 [`README.md`](README.md)；本文是中文速览，两者事实一致。）

## 30 秒示例

```cpp
#include "termchart/api/quick.hpp"

termchart::api::init_core();
termchart::api::hello();  // 终端上色 + 正常恢复，一条最短链路
```

带坐标轴的折线图（显式用法，渲染与写 stdout 分离，CI / headless 可跑）：

```cpp
#include "termchart/canvas/canvas.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/render/renderer.hpp"

termchart::canvas::Canvas canvas(80, 24);
termchart::chart::LineChart chart;
chart.set_title("sine wave");
termchart::chart::Series series;
series.name = "sin(x)";
series.color = termchart::Color::ansi16(2);
for (int i = 0; i <= 80; ++i) {
    const double x = static_cast<double>(i) / 8.0;
    series.points.push_back(termchart::Point{x, std::sin(x)});
}
chart.add_series(std::move(series));
chart.draw({canvas});

termchart::render::Renderer renderer;          // 只产字节流
termchart::api::write_stdout(renderer.render_full(canvas.to_buffer()));
termchart::api::flush_stdout();
```

## 特性表

| 特性 | 说明 |
| --- | --- |
| 零强制依赖 | 库本体不链接任何第三方包；PNG 编解码自带 inflate，不依赖 zlib |
| 子像素渲染 | 盲文 2×4（线）+ 半块 1×2（面），可叠加多份 Scene |
| 按能力降级 | 真彩 / 256 / 16 色；sixel / kitty / iTerm2 / 半块，逐级回退 |
| 三条出口 | 终端实时渲染；矢量导出（SVG / HTML / Mermaid / JSON）；位图（PNG） |
| 可扩展 | 光栅器 / 图表类型 / 比例尺注册表，核心零改动接入 |
| 图可视化 | Graph → 布局（分层 / 力导向 / 网格 / metro）→ 路由 → 图种（流程图 / 状态机 / 思维导图等 21 种） |
| GUI 与统一 Scene | 并列 header-only 库 `termchart_gui`（原生 Win32 / Tk / Qt 后端 + 部件树）；图表统一经 `to_scene` 产出 `Scene` IR |
| 护栏齐全 | 分层规则、头文件自洽、生成物一致、每头独立编译、golden、fuzz、性能基线，全部机器强制 |

## 构建三步

```bash
cmake --preset ninja              # 1. 配置
cmake --build --preset ninja -j   # 2. 构建：库 + 测试 + 示例 + bench
ctest --preset ninja              # 3. 全部测试与冒烟
```

当前状态（诚实口径，权威口径见 [`ROADMAP.md`](ROADMAP.md)）：阶段 0–5 出口条件已
满足，阶段 6 推进中；`ctest 50/50`、doctest `794 用例 / 190985 断言` 全绿；
`check_all`（290 头 / 250 实现）通过。能力矩阵与示例画廊见
`docs/reference/capability_matrix.md` 与 `docs/gallery.md`。

## 许可

**MIT License**（全文见 [`LICENSE`](LICENSE) 与 [`LICENSES/MIT.txt`](LICENSES/MIT.txt)）。
第三方资产：Turbo 色表：Anton Mikhailov / Google LLC（Apache-2.0，全文见
[`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt)）；doctest：MIT（vendored，仅测试使用）；
stb_image / stb_image_write：public domain（vendored 于 `third_party/stb/`）。

**Qt 6（可选 GUI 后端，`plugins/gui_qt/`）**：The Qt Company Ltd. 及贡献者，
**LGPLv3 / GPLv3 / 商业** 三重授权。本项目**不分发**任何 Qt 二进制，
`TERMCHART_BUILD_GUI_QT=ON` 时由使用者 `find_package` 自备，且**只允许动态链接**
（CMake 遇静态 Qt 直接报错）。据此本项目本体许可不被 Qt 传染；但你若分发含该后端的
可执行文件，须随附 LGPLv3 全文与 Qt 源码获取途径，并保证终端用户能替换 Qt 库本身。
