# 自动生成画廊

## 规格意图

画廊 = 脚本遍历图表类型，用固定示例数据渲染并导出统一版式的预览页
（HTML / 终端文本），让人一眼看到"每种图表长什么样"。

## 现状（已落地）

`tools/gen_gallery.py` 已落地，按固定示例数据渲染每个示例并导出统一版式的
预览快照到 `docs/gallery/*.txt`（终端文本形态；HTML 形态按需生成）。覆盖示例
`58_gui_hover` / `59_quick_plot` / `60_dense_sugiyama` / `62_zoom_pan` /
`67_world_map` / `68_cn_province` / `69_city_plot` / `70_export_pdf` /
`71_fetch_stb` / `72_3d` / `73_gui_cjk` / `74_hypercube_4d` / `75_gui_cjk_4d` /
`76_csv_to_chart` / `77_chart_animation` / `78_chart_scene` / `82_autoviz_csv` 等。
该脚本依赖已构建的示例二进制，**刻意不进 `check_generated`**（在"构建后"单独跑
`generate_all` 即可重生成）。

## 最接近的现有替代

- `examples/05_showcase.cpp`、`examples/06_dashboard.cpp`：手动编排的多图表展示；
- `docs/reference/capability_matrix.md`：能力对照表（清单维度，哪些图族有哪些图、哪些走真 raster）；
- `tests/golden/`：锁定个别图表外观的快照（ansi_props 类）。

## 后续

- 每图表的标准示例数据集（fixture）随 `gen_gallery` 逐步补全；
- 接入 `check_generated` 一致性校验（待示例二进制纳入校验管线后）。
