# 自动生成画廊

## 规格意图

画廊 = 脚本遍历所有图表类型，用固定示例数据渲染并导出统一版式的预览页
（HTML / 终端文本），让人一眼看到"每种图表长什么样"。

## 现状

**未落地**：`docs/charts/gallery.md` 本身是占位文档；仓库中没有
`tools/gen_gallery.py` 之类的生成脚本，`tests/golden/` 也没有画廊级快照。

## 最接近的现有替代

- `examples/05_showcase.cpp`、`examples/06_dashboard.cpp`：手动编排的多图表
  展示（非自动生成、非全量覆盖）；
- `docs/reference/capability_matrix.md`：能力对照表（哪些图族有哪些图、
  哪些走真 raster）——覆盖"清单"维度，不覆盖"长相"维度；
- `tests/golden/`：锁定个别图表外观的快照（ansi_props 类）。

## 落地时的建议形态（待实现）

1. 生成脚本放 `tools/gen_gallery.py`（与其他 gen_* 一致，产物进生成物清单）；
2. 输入：图表注册表（`chart::ChartRegistry`）+ 每图族的固定示例数据；
3. 输出：每图一个小节，附头文件路径与 Options 说明；
4. 纳入 `check_generated` 一致性校验。

## 未做

- 画廊生成脚本与产物；
- 每图表的标准示例数据集（fixture）。
