# AutoViz：脏数据自动选图

给一个文件路径，termchart 自己完成**清洗 → 探测 → 路由 → 否决 → 打分 → 选图 → 出图**。适合 exploratory analysis（探索性分析）：刚拿到一份不知道长什么样的数据，想 30 秒看到它的大概。

```cpp
#include "termchart/api/autoviz_plot.hpp"

termchart::api::auto_plot("sales.csv");   // 一行，出图到 stdout
```

要评分明细（选了哪张图、为什么、各候选多少分）：

```cpp
#include "termchart/autoviz/autoviz.hpp"

const auto result = termchart::autoviz::auto_visualize_jsonl(text);
if (result) {
    for (const auto& c : result->candidates) {
        // c.name / c.pipeline / c.analytic_score / c.vetoes …
    }
    if (auto chart = result->build_chosen()) {
        termchart::api::print_chart(*chart);
    }
}
```

## 流程图

```
                       ┌──────────────────────────────────────────────┐
                       │  输入：CSV / JSONL 文件（api::auto_plot）      │
                       └───────────────────┬──────────────────────────┘
                                           │ 格式嗅探（首非空白字符 '{' → JSONL）
                                           ▼
   ┌───────────────────────────────────────────────────────────────────────────┐
   │ 阶段一 · 解析（data L4 / io L2）                                          │
   │   CSV：无表头自动 Col_0..；容错读取，任何输入不报错                        │
   │   JSONL：绝对保留原始 Key；嵌套展平点路径（loc.lat）；脏行跳过并计数        │
   │   字符串值过 scalar_parse：千分位 / 货币 / 会计括号 / 全角 / NA 族          │
   │   产出轻底座 DataFrame——不算任何统计量                                    │
   └───────────────────────────────────┬───────────────────────────────────────┘
                                       ▼
   ┌───────────────────────────────────────────────────────────────────────────┐
   │ 阶段二 · 特征探测（data L4，单遍 + 熔断）                                  │
   │   列级画像：类型推断（Timestamp>Numeric>Boolean>Text，抽样）               │
   │   基数分档：Constant(丢) / Low(≤20 可分组着色) / Mid(≤200 仅 facet)        │
   │             / High(仅去重计数)                                            │
   │   空值率：有 Low 档分类列时**先按 event 分桶再查**（防误杀关键字段）；      │
   │            没有 event 列才做全局剔除（默认阈值 0.6）                       │
   │   列名体检：度量名却是 Text / 分类名却是 Numeric → suspects（只诊断不改路由）│
   │   量级体检：数值列等距抽样 → Tukey 篱外的值记入 outliers（只诊断）          │
   └───────────────────────────────────┬───────────────────────────────────────┘
                                       ▼
   ┌───────────────────────────────────────────────────────────────────────────┐
   │ 阶段三 · 六条管道路由（autoviz L21，按优先级）                             │
   │   Time   有 ts+数值      → 折线（桶内均值，天然有序）/ 热力图 / 桶计数     │
   │   Geo    有 lat/lng     → 散点（按实体分系列，≤6 类）/ Hexbin / 气泡      │
   │   Event  有低基数 event  → 均值柱状 / 箱线（贴标签）/ 饼图（≤8 片）        │
   │   Cat+Num 分类+≥2 数值   → 着色散点 / 气泡（>6 类降级单系列）              │
   │   Numeric 纯数值        → 1列直方图 / 2列散点 / ≥3列平行坐标+相关矩阵     │
   │   Text   纯文本         → 词云                                            │
   │   Preview 兜底          → 数据预览表格（任何输入都有答案）                 │
   └───────────────────────────────────┬───────────────────────────────────────┘
                                       ▼
   ┌───────────────────────────────────────────────────────────────────────────┐
   │ 阶段四 · 语义否决 + 沙盒打分（autoviz L21）                                │
   │   一票否决（结构化理由，不静默）：                                          │
   │     饼图负值/分片>8、时间轴未排序、分类×分类、高/中基数着色、              │
   │     地理未去重、词云<3 词、散点<3 点、折线<2 点、平行坐标<3 轴、单值直方图 │
   │   两段式打分：                                                             │
   │     ① 解析式海选（零渲染）：管道适配分 + 数据量 + 空值惩罚 + 类别数惩罚    │
   │     ② 画布级复核（仅 Top-2）：覆盖率/行使用率 ramp——只当决赛裁判          │
   │        乘性下调 ≤15%，绝不奖励"被复核"；排序唯一口径是 total_score         │
   └───────────────────────────────────┬───────────────────────────────────────┘
                                       ▼
                       ┌──────────────────────────────────────────────┐
                       │  输出：Top 1 图表 → api::print_chart（TUI）    │
                       │  （GUI 路径由应用层接 AutoVizResult，api 不碰） │
                       └──────────────────────────────────────────────┘
```

性能口径：数据侧（解析/画像/分桶/聚合）严格单遍 O(N)，distinct 探测带熔断；渲染侧只对通过校验的 Top-2 付一次画布成本，与数据行数无关。

## 明确的边界

| 输入 | 行为 |
| --- | --- |
| 仅表头（0 数据行） | `InvalidArgument`——没有数据不是可视化问题，不编造空图 |
| Unix 时间戳数字串（`1700000000`） | **不自动识别**：与 18 亿访问量/ID 无法区分。要时序图显式传 `opts.time_column` |
| 时间格式 | 认 `YYYY[-/./年]M[-/./月]D[ T日][H[:时]M[:分]S]][Z\|±HH:MM]`（月/日/时分秒 1-2 位不补零也认）；`2026/13/40` 这类形状合法历法非法的行在聚合时跳过 |
| 全空文件 / 空表 | `InvalidArgument` |
| 类数 > 6 的着色场景 | 放弃颜色通道，降级单系列（同色不同类比不分色更误导） |
| 多数值列 × 事件管道 | 每个数值列各出一套候选（不并图、不截断） |
| grouped / stacked bar | 暂不支持（`BarChart` 仅单系列），走"每列一图" |
| 时区 | naive UTC（排序分桶够用，不做时区换算）；时间轴刻度标签同一口径，否则同一份数据在不同机器上显示不同时间 |

可调项（`autoviz::AutoVizOptions`）：`cardinality`（基数分档阈值）、`max_null_rate`、`event_column` / `time_column`（逃生门）、`max_candidates` / `raster_confirm_top`、`canvas_width/height`、`clip_outliers` / `outlier_fence`（极端离群值裁剪，见下节）。

## 表头口径体检（suspects）

CSV/Excel 最常见的脏法不是脏值，而是**表头与数据列序错位**（少写/多写一个字段，从某行起整行左移）。这类数据里每一列的类型都还能判，图也画得出来，只是名字与内容对不上——例如 `amount` 列里装的是 `ALIPAY/WECHAT/CARD`、`method` 列里装的是金额。

引擎的处理分两半：

- **路由照旧**：信推断类型、不信列名，所以图本身仍然是"按真实内容分组的真实度量均值"，不会因为名字而崩。
- **另出诊断**：`DatasetFeatures::suspects` 记下列名口径矛盾，供调用方打印或自己提示。

```cpp
const auto result =
    termchart::autoviz::auto_visualize(termchart::data::parse_dataframe(text));
if (result) {
    for (const auto& s : result->features.suspects) {
        // s.name / s.expected（"numeric" | "categorical"）/ s.actual（column_kind_name）
        std::fprintf(stderr, "列 %s 名字像%s，实际是%s——查表头是否与数据列序错位\n",
                     s.name.c_str(), s.expected.c_str(), s.actual.c_str());
    }
}
```

判定规则（**刻意保守，宁漏不误**）：

| 名字命中 | 推断类型 | 结论 |
| --- | --- | --- |
| 度量词 `amount/price/total/subtotal/revenue/income/cost/fee/tax/sum/balance/qty/quantity/count/weight/score/salary/profit/budget/discount` | `Text` | 报 `expected=numeric` |
| 分类词 `method/gateway/currency/channel/category/brand/vendor/country/city/region` | `Numeric` 且值域不是 0/1 | 报 `expected=categorical` |
| 分类名 + 数值只有 0/1 | `Numeric` | **不报**：flag 编码是正常口径（`status` 常这么写） |

匹配按"词"而非子串：`_`、空格、`.`、`-`、中文都算分隔符，camelCase 会拆（`totalAmount`），复数视同单数（`fees`）。因此 `account` 不会被 `count` 命中，`payment_method` 走分类词而不是度量词。suspects **绝不改变路由**——修不修数据是人的决定。

## 极端离群值（脏大数把图压平）

脏数据破坏图的第二种方式不是"选错图"，而是**轴域被单个值绑架**：金额列里混进一个 `999999999999999999999`（1e21 的哨兵值/手滑多打的 0/单位错），轴域取 `[min, max]` 时其余全部数据被压成贴底的平线——图还在画，信息量归零。引擎分两条腿走，缺一不可：

1. **告警**（`DatasetFeatures::outliers`）：数值列按行**等距抽样**（不是前 N 行——脏值常在尾部；上限 1024 个值）判越界，命中就记一条，说清"哪个值离谱、主体在哪"。
2. **裁轴**（`autoviz/domain.hpp` 的 `robust_y_domain`）：轴域不取极值，取篱内范围；篱外的点由比例尺钳到轴边缘（不消失，也不撑爆画布），并在标题留痕 `· 裁去 N 个极值`。样本太少或越界占比过高时**主动放弃裁剪**（见下面两道保护闸）——那时宁可压平也不拉伸。

```cpp
// api::auto_plot 会自动打 WARN；手动路径自己决定怎么提示
for (const auto& o : result->features.outliers) {
    // o.name      列名
    // o.sampled   抽样到的有效值数   o.count 其中越界的个数
    // o.median    主体（中位数）     o.extreme 越界最远的那个值
    // o.high      true = 上溢（脏大数），false = 下溢（脏负数）
    // o.decision  kApplied 才真裁过轴；kSmallSample / kTooMany 表示命中保护闸、
    //             轴域仍是原始范围（告警文案据此区分，别把"没裁"说成"已裁"）
}
```

### 判定用 Tukey 篱，不用 P1/P99

| | P1 / P99 | Tukey 篱（采用） |
| --- | --- | --- |
| 小样本 | **失效**：11 个值时 P99 就是 max，一点都裁不掉（真实脏订单 CSV 只有 6 个有效值） | 5 个点就能判出离群 |
| 单个极端值 | 分位数被它带跑，篱跟着变宽 | 基于四分位，对单个极端值稳健 |
| 参数 | 要选两个 | 一个系数 `k`（默认 1.5） |

规则：`[Q1 − k·IQR, Q3 + k·IQR]` 之外算极端值，`k = 1.5` 即箱线图默认须长。IQR 退化（半数以上值相同，典型"999 个 1 和 1 个 1e21"）时改用相对中位数的篱 `|v − median| > k·max(|median|, 1e-9)`。非有限值（NaN/Inf）直接跳过——空值不是离群。

### 两道保护闸：宁可压平，不许拉伸

"判得出离群"不等于"该裁轴"。样本越小、越界占比越高，裁剪就越接近**视觉欺骗**：剩下的少数点被拉满整根轴，读者会以为波动剧烈，而业务量级其实平稳。所以裁剪前先过两道闸，任一命中就退回原始域：

| 闸 | 条件 | 理由 |
| --- | --- | --- |
| 样本量 | 有效值 **< 10** 个 | 6 个点裁掉 1 个就是 16.7%，剩下 5 个点撑满全轴 |
| 越界占比 | 越界值 **> 20%** | 裁掉两成以上不再是"少数脏值"，而是"整列本来就离散"，裁了等于人为制造趋势 |

命中时的行为是**降级但不闭嘴**：

- **轴域**退回 `[min, max]`——图会略微被压平，但没有人被误导。
- **告警照发**，文案改成「有效值仅 8 个（少于 10 个），样本太小不裁轴，Y 轴保持原始范围」或「越界占 25%（超过 20%），是整列离散而非少数脏值」。
- **标题照留痕**：从 `· 裁去 2 个极值` 改成 `· 2 个极值未裁（有效值太少）`——顶端那个贴边平台必须被解释，否则读者会当成真实峰值。
- `ChartCandidate::clipped_points` 记 **0**（没裁就不谎报裁了多少），`ValueOutlier::decision` 区分 `kApplied` / `kSmallSample` / `kTooMany`，让调用方自己也能判。

阈值在 `autoviz/domain.hpp`：`kOutlierMinSamples`（默认 10）、`kOutlierMaxClipRatio`（默认 0.20）。对照两组真实数据：脏订单 CSV（11 个有效值、2 个越界 = 18%）刚好过闸，裁剪生效；示例 82（8 个有效值、2 个越界 = 25%）两道闸都撞上，退回原始域——这正是设计意图。

### 接线细节（改路由时才需要知道）

- **X 轴不动**：`chart::make_area` 新增的 `y_override` 只接管 Y，X 仍走 `nice_domain`——裁轴不该顺带改掉 X 的外观。
- **多系列取并集**（`merge_domains`）：同一根 Y 轴上的系列共享一个域，绝不让每系列各裁一套（那会让读者以为它们同量级）。系列间量级差异极大是该换对数轴的信号，不是各自裁剪的理由。
- **柱状图必须含 0**：柱的基线是 0，域不含 0 就是"截断轴"的谎，所以裁剪后把 0 并回域内。
- **退化域当没给**：`hi > lo` 不成立时忽略覆盖、回退自动域（否则比例尺 `span = 0` → 除零/全 NaN）。
- **检测 ≠ 裁剪**：`features.outliers` 用 `clipped > 0` 判定（检测到就报），轴域用 `active()` 判定（真裁了才设 `y_override`）；两处口径写在 `ClipDecision` 里，别用 `active()` 决定"要不要告警"。
- **告警用抽样、标题用全量**：`features.outliers` 的 `count` 是抽样口径（用于 WARN），`ChartCandidate::clipped_points` 才是建图时用全量聚合值重算的精确数字（用于标题）。
- 折线/柱状两条管道路由都接了；其余图种（散点/热力图/词云…）暂未接稳健域。

关掉裁剪：`opts.clip_outliers = false`（轴域回到 `[min, max]`，连体检都不做）。调激进程度：`opts.outlier_fence`（`1.0` 已接近"正常波动也裁"，`3.0` 只裁极度离谱的值）。两道保护闸是**编译期常量**、不随 options 变——它们挡的是"裁剪本身会误导"的情形，不该由调用方放松。

**裁剪不是洗数据**：被钳到边缘的点仍然画在图上（顶端那一排），标题里的"裁去 N 个极值"就是提醒你回去改源数据——库替你把轴救回来，但不替你隐瞒脏值。

## 免责声明

**AutoViz 仅供探索性分析。** 它按数据形状选"一张合理的图"，不理解业务语义——选中的图可能对问题是错的图（它不知道你关心的是占比还是趋势）。生产环境（报表、大屏、对外的图）请**显式指定图表类型与列映射**（`chart::` 各图种 + `api::render_chart`），AutoViz 留给"我刚拿到数据，想先看一眼"。

示例：`examples/82_autoviz_csv.cpp`（地狱级脏 CSV 全自动）。
