# 分层（Layering）

## 决策

模块 = `include/termchart/` 下的一级目录，层号 L0–L22（同层模块互不依赖）固定写在
`cmake/TermchartModules.cmake` 的三张表里：

- `TERMCHART_MODULE_LAYERS`：每个模块的层号；
- `TERMCHART_NARROW_EDGES`：收窄边（模块间默认关闭、白名单文件级放行）;
- `TERMCHART_FORBIDDEN_EDGES`：禁止边（现含 `gui > canvas/term/app`：GUI 平行栈不得碰终端呈现）。

由 `tools/check_layers.py` 执行，判定顺序固定：**禁止边 → 收窄边 → 层号**。
`cmake --build --preset <预设> --target check_layers` 一键校验。

## 规则内容

| 类别 | 条目 | 理由 |
| --- | --- | --- |
| 禁止边 | `backend > chart` | backend 只消费 IR（CellBuffer/Scene/Document），绝不认识具体图表类型 |
| 禁止边 | `chart > render` | 保住 chart → Document → structured 后端的独立通路，防止图表直接拼终端字节 |
| 禁止边 | `render > interaction` | 维持单向；interaction 可以 include render，反之不行 |
| 收窄边 | `backend/snapshot/ansi_backend > render/sgr` | 导出路径必须复用唯一 SGR 原语，但其余 render 头不放行 |

## 例外条款

1. `detail/`（顶层）层号 -1：任何层都可 include——横切生成表与内部工具。
2. 顶层 `version.hpp / export.hpp / deprecate.hpp / fwd.hpp` 视为 core（层号 0）。
3. 两个伞形头（`termchart.hpp` / fwd 类）豁免层号判定。
4. `interaction > chart`（规格条款）：interaction 经由 `chart/hit_region.hpp` 的
   `HitRegion`（形状 + 不透明 payload）做命中测试，只 switch `HitShape`，禁图表名——
   这是唯一允许 interaction 认识 chart 的通道，且通道本身不含图表类型。

## 为什么机器强制

分层约束靠人记必然漂移（历史上层号与直觉就不同：`term` 是 L7 而 `render` 是 L14，
所以 render 能依赖 term，反之不行）。机器可执行的表让「新增一个 include」的合法性
在 CI 一秒内判定，评审时不再讨论 taste。

## 反例（会被 check_layers 拒绝）

- 在 `chart/` 里 include `render/`——图表想直接画 SGR。
- 在 `backend/*` 里 include `chart/*`——导出后端想读图表字段。
- 在 `render/` 里 include `interaction/`——渲染器想处理输入。

正确做法分别是：chart 画进 `canvas::Canvas`；chart 先落 `document::Document`；
输入处理放 interaction 并依赖 render 的产物。
