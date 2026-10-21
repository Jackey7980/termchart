# 3D 与 4D 图表

terminal chart 的高维扩展：`math3d`/`math4d`（数学原语）+ `chart3d`/`chart4d`
（发射器）。核心思想——**不另造渲染管线**：投影结果统一产出 `scene::Scene`，
复用 `render_scene → Canvas → Renderer` 终端链与 `gui::SceneRenderer` 像素链。

| 模块 | 层 | 内容 |
| --- | --- | --- |
| `math3d` | L3 | `vec3` / `mat4`（行主序）/ `camera`（轨道相机 + 透视投影） |
| `math4d` | L3 | `vec4` / `mat5`（5×5 齐次）/ 6 个旋转平面 / `camera4`（4D→3D 透视除 W） |
| `chart3d` | L12 | `scatter3d` / `surface3d` / `IProjector` 投影缝 |
| `chart4d` | L12 | `Projector4D` / `wire4d`（4D 线框/点云发射器） |

## 3D（示例 `72_3d`）

```cpp
#include "termchart/chart3d/common.hpp"     // make_camera
#include "termchart/chart3d/projector.hpp"  // Projector3D
#include "termchart/chart3d/scatter.hpp"    // scatter3d

const math3d::Camera cam = chart3d::make_camera(
    width, height, /*azimuth*/ 30.0, /*elevation*/ 20.0, /*fovy*/ 55.0,
    math3d::Vec3{0, 0, 0}, /*radius*/ 2.2);
const chart3d::Projector3D projector(cam);
const scene::Scene s = chart3d::scatter3d(points, opts, &projector);
```

- `surface3d` 产线框曲面（z = f(x,y) 网格）；`scatter3d` 产点云；
- 深度只用于着色（`chart3d::shade`），**刻意不造 z-buffer**；
- `scatter3d`/`surface3d` 的可选尾参 `const IProjector*`（nullptr = 按 opts 现算
  `Projector3D`）是投影缝——现有调用方零改动，`Projector4D` 即经此插入。

## 4D（示例 `74_hypercube_4d` / `75_gui_cjk_4d`）

4D→2D 走**两阶段投影**：`Vec4 --(4D 旋转 + 透视除 W)--> Vec3 --(内层 IProjector)--> 画布`。

```cpp
#include "termchart/chart4d/projector4d.hpp"
#include "termchart/chart4d/wire.hpp"

math4d::Camera4 cam4;
cam4.distance = 3.0;                                  // 相机架在第 4 轴 +W
cam4.focal = math4d::focal_from_fov_deg(60.0);

const math4d::Mat5 rot =
    math4d::rotate_xw(angle_xw) * math4d::rotate_yz(angle_yz);  // 6 平面任选
const chart4d::Projector4D projector(rot, cam4, inner_3d_projector);
const scene::Scene s = chart4d::wire4d(verts, edges, wire_opts, projector);
```

- 数学要点：正角把第 i 轴送向第 j 轴；`Camera4` 的 `scale = focal / w_view` 即
  “除以 W”的齐次除法，相机处/后方/近裁剪内不可见（fail-loud 丢边，不猜）；
- `wire4d` 边按两端平均深度着色 + 顶点方块；两端任一不可见则整段丢弃；
- 超立方体 = 16 顶点（±1,±1,±1,±1）/ 32 边（二进制位差 1）。

## 出口选择

| 出口 | 路径 | 示例 |
| --- | --- | --- |
| 终端（动画） | wire4d → `canvas::render_scene`（braille）→ `app::HeadlessSession` 差分重绘 | `74_hypercube_4d` |
| 终端（单帧出版） | Scene → `gui::SceneRenderer` 3× 超采样 → PNG | `75_gui_cjk_4d` |
| 真窗口（交互） | 同 75 管线 + `TkCanvas::blit` 上 Win32 后端 | `plugins/gui_win32 hypercube_4d_demo`（拖拽旋转/滚轮缩放） |

## 已知边界

- 曲面/超立方体只到**线框档**：无实体填充、无遮挡排序（z-buffer）；
- 4D 交互（轨道旋转）只在 GUI 插件 demo，终端版是脚本化自转；
- `IProjector` 缝支持把任意 3D→2D 投影器插进 scatter3d/surface3d。
