# 写自定义后端（区分三物种）

## 第一步：确定物种

后端铁律：**不做跨渲染模型转换**。先问"我的输入是什么"：

| 你的输入 | 物种 | 继承 |
| --- | --- | --- |
| `CellBuffer`（格化结果） | snapshot | `backend::snapshot::SnapshotBackend` |
| `scene::Scene`（矢量 IR） | vector | `backend::vector::VectorBackend` |
| `document::Document`（语义块） | structured | `backend::structured::StructuredBackend` |

选错的信号：你发现需要"先把 CellBuffer 反推成几何"或"把 Scene 降级成字符"——
那说明物种选错了，回到数据源头重新选。

## 第二步：实现

基类 `backend::Backend`（begin/end/bytes 生命周期）+ 物种子接口的 `render(input)`：

```cpp
class MyBackend final : public termchart::backend::snapshot::SnapshotBackend {
public:
    void render(const termchart::CellBuffer& buffer) override;
};
```

- `begin()` 已清空 `out_`、`end()` 可留空（基类默认实现够用）；
- 结果写进保护的 `out_`（`std::string`），`bytes()` 视图自动生效；
- snapshot 物种可选实现 `row_width()`（trim 行尾空白格）；
- 便捷包装：`render_snapshot(backend, buffer)` = begin + render + end。

## 物种特有约定

- **snapshot**：ANSI 类输出必须复用 `render/sgr.hpp`（唯一 SGR 生成点；
  收窄边白名单只对 `ansi_backend` 开放）。`SnapshotOptions{depth,
  trim_trailing_blank}`。
- **vector**：消费 Scene 指令时必须与 canvas 同构——`FillOp`/`StrokeOp` 是
  **状态指令，只作用于其后出现的图元**（SVG 同构语义，着色错位皆因违背此条）。
  `VectorOptions{scale, autofit, width, height}`。
- **structured**：输入是语义块（段落/标题/表格/图表 Scene），绝不 switch
  图表类型（`backend > chart` 禁止边）。`StructuredOptions{title, css_class}`。

## 三物种现有实现（参照物）

| 物种 | 文件 |
| --- | --- |
| snapshot | ansi_backend / text_backend / html_pre_backend / json_backend |
| vector | svg_backend + svg_entities |
| structured | html_backend / markdown_backend |

## 验证

- 三组可扩展性契约之一是 backend：`tests/integration/replace_backend.cpp`
  证明新增后端核心零改动；
- 新后端至少配一个 golden 或单元测试（bytes() 输出断言）；
- 跑 `check_all`（尤其分层：别让后端 include 了不该 include 的层）。
