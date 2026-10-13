<!-- 由 tools/gen_gallery.py 生成，禁手改。更新方式：重跑该脚本。 -->
# 示例画廊

每个示例在非 tty 环境下运行的 stdout 快照（已剥 ANSI，文本内容即构图）。
二进制来自 `--build-dir` 指定的构建目录；职责一句话取自示例源文件首行注释。

| 示例 | 职责 | 快照 |
| --- | --- | --- |
| `01_hello_color` | 终端上色并正常退出 | [文本快照](gallery/01_hello_color.txt) |
| `02_sine_wave` | 浮点画布 → 盲文子像素 → CellBuffer → 终端（阶段 2 出口示例） | [文本快照](gallery/02_sine_wave.txt) |
| `05_showcase` | 子像素光栅 + 真彩旗舰展示 | [文本快照](gallery/05_showcase.txt) |
| `06_dashboard` | 画图能力展示台（line | bar | heatmap） | [文本快照](gallery/06_dashboard.txt) |
| `07_fractal` | 子像素光栅 + 真彩：Mandelbrot 分形（含降级对比开关） | [文本快照](gallery/07_fractal.txt) |
| `08_fractal_levels` | 四连拍：同一张 Mandelbrot 在 真彩/256/16/ASCII 四档色深同屏 | [文本快照](gallery/08_fractal_levels.txt) |
| `03_function_plot` | 高级示例：多曲线 + 图例 + 坐标刻度（阶段 2 能力上限演示） | [文本快照](gallery/03_function_plot.txt) |
| `03_line_chart` | 带坐标轴的折线图（阶段出口条件） | [文本快照](gallery/03_line_chart.txt) |
| `04_bar_chart` | 柱状图（阶段出口条件） | [文本快照](gallery/04_bar_chart.txt) |
| `04_heatmap` | 连续色阶热力图（Mandelbrot，viridis 色表 + 半块双纵向像素） | [文本快照](gallery/04_heatmap.txt) |
| `09_radar` | 雷达图（规格示例） | [文本快照](gallery/09_radar.txt) |
| `10_gauge` | 仪表盘（规格示例） | [文本快照](gallery/10_gauge.txt) |
| `11_wordcloud` | 词云（规格示例） | [文本快照](gallery/11_wordcloud.txt) |
| `12_treemap` | 树图（规格示例） | [文本快照](gallery/12_treemap.txt) |
| `13_sankey` | 桑基图（规格示例） | [文本快照](gallery/13_sankey.txt) |
| `18_uml_sequence` | UML 时序图（规格示例） | [文本快照](gallery/18_uml_sequence.txt) |
| `28_ascii_mode` | 颜色降级阶梯（规格示例：降级纯文本/ASCII 场景的能力底座） | [文本快照](gallery/28_ascii_mode.txt) |
| `29_256color_mode` | 256 色降级（规格示例） | [文本快照](gallery/29_256color_mode.txt) |
| `14_flowchart` | 流程图：Graph -> 分层布局 -> 正交路由 -> 画布 -> stdout | [文本快照](gallery/14_flowchart.txt) |
| `15_state_machine` | 状态机图：分层布局 + 转移事件标签 + 起止标记 | [文本快照](gallery/15_state_machine.txt) |
| `16_mindmap` | 思维导图：树布局（根居中、分支分列左右）+ 无箭头连线 | [文本快照](gallery/16_mindmap.txt) |
| `17_uml_class` | UML 类图（三栏类框 + 空心三角泛化箭头 + 总线接点） | [文本快照](gallery/17_uml_class.txt) |
| `20_force_graph` | 力导向布局 + 通用节点-连线渲染 | [文本快照](gallery/20_force_graph.txt) |
| `19_metro_map` | 地铁线路图（示意式：环线 + 两条直径 + 交路） | [文本快照](gallery/19_metro_map.txt) |
| `21_hover_tooltip` | 悬停数据标签 + 十字准线（规格示例） | [文本快照](gallery/21_hover_tooltip.txt) |
| `22_zoom_pan` | 滚轮缩放 + 拖拽平移（规格示例） | [文本快照](gallery/22_zoom_pan.txt) |
| `30_legend_toggle` | 图例点击切换（规格示例，阶段 5 出口条件之一） | [文本快照](gallery/30_legend_toggle.txt) |
| `32_quick_one_liner` | 零配置一行出图 | [文本快照](gallery/32_quick_one_liner.txt) |
| `33_mermaid_flowchart` | Mermaid 文本 → 流程图（阶段 5 出口条件之一） | [文本快照](gallery/33_mermaid_flowchart.txt) |
| `42_single_header_usage` | 单头产物自包含验证（阶段 7） | [文本快照](gallery/42_single_header_usage.txt) |
| `23_multi_chart_dashboard` | 多图联动仪表盘：多个面板共享同一个 Viewport | [文本快照](gallery/23_multi_chart_dashboard.txt) |
| `24_theme_switching` | 运行时切主题：同一图表用两张主题色板各渲一帧 | [文本快照](gallery/24_theme_switching.txt) |
| `25_animation` | 动画与缓动：注入时钟扫描帧序列（输出逐字节可复现） | [文本快照](gallery/25_animation.txt) |
| `26_realtime_stream` | 实时数据流：主循环驱动滚动窗口折线（注入时钟） | [文本快照](gallery/26_realtime_stream.txt) |
| `27_background_thread` | 后台线程投递：生产线程 post，主线程 join 后消费 | [文本快照](gallery/27_background_thread.txt) |
| `31_plugin_custom_chart` | 注册自定义图表：自定义 Chart 子类注册进注册表 | [文本快照](gallery/31_plugin_custom_chart.txt) |
| `39_headless_render` | CI 无终端渲染：全程不碰 stdout，收尾一次性输出 | [文本快照](gallery/39_headless_render.txt) |
| `40_immediate_mode` | 即时模式组件：无状态绘制 + 返回"意图"驱动调用方状态 | [文本快照](gallery/40_immediate_mode.txt) |
| `41_reactive_dashboard` | 响应式仪表盘：Observable 值变更驱动面板刷新 | [文本快照](gallery/41_reactive_dashboard.txt) |
| `43_contour` | 等高线：标量场 → marching squares（规格示例） | [文本快照](gallery/43_contour.txt) |
| `44_violin` | 小提琴图：核密度轮廓（规格示例） | [文本快照](gallery/44_violin.txt) |
| `45_ridgeline` | 山脊图：多组分布按行堆叠、后行遮前行（规格示例） | [文本快照](gallery/45_ridgeline.txt) |
| `46_parallel` | 平行坐标：每维一条竖轴，观测跨轴连折线（规格示例） | [文本快照](gallery/46_parallel.txt) |
| `47_gantt` | 甘特图：任务时间条 + 行标签（规格示例） | [文本快照](gallery/47_gantt.txt) |
| `48_forest` | 森林图：各研究点估计 + CI 横线 + 合计菱形（规格示例） | [文本快照](gallery/48_forest.txt) |
| `49_ecdf` | 经验分布函数：逐级跳台阶（规格示例） | [文本快照](gallery/49_ecdf.txt) |
| `50_qq` | QQ 图：样本分位数对正态理论分位数（规格示例） | [文本快照](gallery/50_qq.txt) |
| `51_hexbin` | 六边形分箱：偏移网格近似铺排 + 计数分级着色（规格示例） | [文本快照](gallery/51_hexbin.txt) |
| `52_chord` | 和弦图：实体环形排布 + 弧带连接（规格示例） | [文本快照](gallery/52_chord.txt) |
| `53_er_diagram` | ER 图：实体表框 + 鸭掌记号基数 + 关系名标签（规格示例） | [文本快照](gallery/53_er_diagram.txt) |
| `54_dag` | DAG：无环走分层+正交路由；带环输入走退化路径（规格示例） | [文本快照](gallery/54_dag.txt) |
| `34_export_svg` | 矢量导出：Scene → SVG（写 stdout） | [文本快照](gallery/34_export_svg.txt) |
| `37_export_latex` | 导出 LaTeX TikZ（规格示例） | [文本快照](gallery/37_export_latex.txt) |
| `35_export_html` | 结构化导出：Document → HTML（写 stdout） | [文本快照](gallery/35_export_html.txt) |
| `36_export_json` | 快照导出：Canvas → CellBuffer → JSON（写 stdout） | [文本快照](gallery/36_export_json.txt) |
| `38_image_sixel` | 终端内显示图片（按能力选择 kitty / iterm2 / sixel / 半块） | [文本快照](gallery/38_image_sixel.txt) |
| `40_advanced_master` | termchart 高级能力旗舰展示 | [文本快照](gallery/40_advanced_master.txt) |

