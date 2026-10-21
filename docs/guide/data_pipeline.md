# 数据管线：从 CSV 到图

termchart 的数据链路由三个模块组成，全部零第三方依赖、不抛异常
（`Result<T>` / `Status`，见 [design/error_handling.md](../design/error_handling.md)）：

| 模块 | 层 | 职责 | 代表头文件 |
| --- | --- | --- | --- |
| `io` | L2 | 数据入口：读进来 | `io/csv` / `io/json` / `io/stream` |
| `data` | L4 | 数据加工：清洗/窗口/重采样 | `data/scalar_parse` / `data/dataframe` / `data/fill_missing` / `data/rolling` / `data/ohlc` / `data/resample` / `data/aggregate` |
| `math` | L1 | 数值内核：统计/插值/变换 | `math/stats` / `math/interpolate` / `math/fft` / `math/linalg` / `math/special` |

典型流向：

```
CSV/JSON/流 ──io──► vector<double> ──data──► 清洗/均线/OHLC ──math──► 统计/插值
                                                                      │
                                              chart/api ◄─────────────┘
```

## io：数据入口

### CSV（`io/csv.hpp`）

零拷贝 `std::string_view` 状态机 + `std::from_chars` 数值解析。设计成**数值列入口**（Fail-fast）：
每列要么全是数字，要么空/`NA` 族映射 NaN——**非数字字段直接带行列诊断报错**
（`ErrorCode::ParseError` + `ParseDiagnostic`），不静默跳过。要容错读"脏表"（文本列保留、
脏值清洗、行列不齐不报错），走 [`data/dataframe`](#容错-csv-读取data-dataframehpp)。

```cpp
#include "termchart/io/csv.hpp"

const auto table = termchart::io::parse_csv(text);  // string_view / std::string
if (!table.has_value()) {
    // diag.line / diag.column / diag.code（可选 out 参数拿诊断）
    return;
}
for (const auto& col : table.value().columns) {
    const std::vector<double>& values = col.values;  // 空与 "NA" 已是 NaN
}
```

自动处理：delimiter 自动嗅探（`,` `;` tab `|`，跨行计数）、BOM、`#` 注释行、
CRLF、双引号转义（含 `""`）。`CsvOptions` 可显式指定 delimiter 与是否嗅探。

### JSON（`io/json.hpp`）

完整 JSON 解析器（嵌套/转义/深度与体积上限/行列诊断）。`dsl/json.hpp` 是它的
兼容 shim（`using` 别名），两份用法等价。

### 流式读取（`io/stream.hpp`）

`LineReader`：64KB 分块 + 超长行 compact/翻倍路径，GB 级文件不 OOM。
`LineReader::open(path)` 走 `Result`，`LineReader::from_string(...)` 供测试。

## data：数据加工

### 缺失值（`data/fill_missing.hpp`）

NaN 即缺失，三种策略：`ForwardFill`（前向填充，时间序列）、`LinearInterp`
（相邻非缺失插值）、`Drop`（返回仅含非缺失的新 vector）。

### 滚动窗口（`data/rolling.hpp`）

`rolling(values, RollingStat::Mean, {window, min_periods})` 支持
mean/sum/stddev/min/max/median，右对齐或居中；`ema(values, window)` 是指数移动平均
（O(N) 递推，`alpha = 2/(window+1)`，pandas `ewm(span=window).mean()` 口径）。

`rolling` 的全部统计量均为 **O(N) 滑窗**：mean/sum 用滑动和、stddev 用 Welford 加/减
（方差钳到非负）、min/max 用单调双端队列、median 用双 multiset（O(N log W)）。NaN 在
窗口内跳过，非 NaN 计数低于 `min_periods` 时该点输出 NaN。

### 脏标量解析（`data/scalar_parse.hpp`）

"赛博法医"入口：把 `io/csv` / `io/json` 读到的原始字符串尽力清洗成数值——千分位逗号、货币
符号（`$`/`¥`/`€`/`£`/`RMB`/`USD`/`CNY` 与中文后缀 `元`/`美元`）、百分号 `15%→0.15`、会计括号
`(123.45)→-123.45`、Unicode 负号 `−123`、全角数字/负号/句点、NA 族（`NA`/`N/A`/`null`/`-` 等
→ 缺失）。返回 `ParsedScalar{status, value, unit, flags}`：认不出 → `Invalid`（**保留原文**，
上层拿去做分类维度）、缺失记号 → `Empty`（`value=NaN`）、成功 → `Numeric` 并把清洗动作记成
`ScalarFlag`（HasComma / AccountingNegative / Percent / FullWidth / Currency），货币/中文单位
后缀抽到 `unit`。**绝不抛异常、绝不崩**——fail-loud 的是"结果状态"，不是输入。

### 容错 CSV 读取（`data/dataframe.hpp`）

"法医级"宽容读取器，与 `io/csv` 的严格数值入口**刻意并存**（两份，不是遗漏）：

- `io/csv`：Fail-fast——列主序 `vector<vector<double>>`，文本列/脏值/行列不齐直接
  `ParseError`。适合"已知干净的数值表"（如 `examples/76`）。
- `data/dataframe`：Fail-safe——每个字段是一个 `Cell{raw, num, type, flags, unit}`，**文本列
  保留原文**（分类维度）、脏值清洗成数并记 flag、缺列补 `Empty(Ragged)` 空位，**整张表绝不
  报错**，任何一行都继续读。适合 AutoViz 的"地狱级脏表"入口。

```cpp
#include "termchart/data/dataframe.hpp"

const auto f = termchart::data::parse_dataframe(csv_text);   // 不返回 Result，永不失败
for (std::size_t c = 0; c < f.columns.size(); ++c) {
    for (const auto& cell : f.columns[c]) {
        if (cell.is_numeric()) use(cell.num);
        else if (cell.is_text()) keep_category(cell.raw);   // "华东" 之类原样保留
        else if (cell.is_missing()) skip();                 // 空 / NA / 缺列
    }
}
```

> ⚠️ CSV 分隔符与千分位逗号天然冲突：未加引号的 `1,234.50` 会被逗号切成两列。标准 CSV 里
> 千分位值必须加引号（`"1,234.50"`）；`scalar_parse` 只在"值作为单字段到达"时（JSON、引号
> 单元格、单列输入）才认出千分位。

### K 线 OHLC（`data/ohlc.hpp`）

时间轴等宽分桶 → `OhlcBar{open, high, low, close, count}`，`chart/cartesian`
candlestick 的数据入口。返回 `Result<std::vector<OhlcBar>>`：timestamps **必须非降序**，
违反即 fail-loud 返回 `InvalidArgument`（无序输入会把 K 线切成错乱桶，比报错更糟）；空桶
不产 bar、NaN 在桶内跳过。

### 重采样与聚合（`data/resample.hpp` / `data/aggregate.hpp`）

`resample` 按显式桶边界聚合（mean/sum/min/max/first/last）；`aggregate` 提供
histogram / quartiles / group_reduce（内部统计已收编 `math::stats`）。

## math：数值内核

| 头文件 | 内容 |
| --- | --- |
| `math/stats.hpp` | mean / median / variance / stddev / covariance / pearson（Kahan 补偿 + Welford 单遍，NaN 跳过） |
| `math/interpolate.hpp` | `linear_at` + `CubicSpline`（自然三次样条，Thomas 三对角 O(N) 构造，越界钳位不外推） |
| `math/fft.hpp` | radix-2 Cooley-Tukey 原地变换；逆变换缩放 1/N，往返恒等；非 2 幂 fail-loud 返回 false |
| `math/linalg.hpp` | 稠密 n×n 行主序 multiply / inverse / solve / determinant（部分选主元高斯消元，span + double 无模板） |
| `math/special.hpp` | gamma / ln_gamma / erf / erfc（统一 NaN 边界口径）+ Bessel J0/J1/Jn（级数 + Hankel 渐近自实现） |

全部为 header-only，输入用 `std::span<const double>`，不抛异常。

## 端到端示例

`examples/76_csv_to_chart.cpp`（进 ctest，确定性输出）演示完整链路：内置 CSV
（含 3 个 NA）→ LinearInterp 清洗 → EMA(5) → braille 双曲线 + 尾部统计行
（mean / pearson / 均线末值）。最小的手工组合：

```cpp
const auto table = termchart::io::parse_csv(csv_text).value();
std::vector<double> ys = table->columns[1];              // 列主序，空/NA 已是 NaN
(void)termchart::data::fill_missing(ys, termchart::data::FillStrategy::LinearInterp);
const auto ma = termchart::data::ema(ys, 5);              // 5 周期指数均线
const double r = termchart::math::pearson(xs, ys);       // 相关系数
```

## 已知边界（如实登记）

- `data::rolling` 全部统计量已 O(N)（运行和 / Welford / 单调队列 / 双 multiset）；median 为
  O(N log W)，比对朴素 O(N·W·log W) 已优一个 W 因子；
- `math::interpolate` 无 monotone (PCHIP) 变体（自然样条在噪声数据上轻微过冲）；
- `io/csv` 与 `dsl/csv_sniff` 两份解析器并存（后者输出 `document::TableModel`，层号 L8
  无法下沉 io，远期合并见 backlog）；
- `data`/`io` 到 `chart` 的封装接线（“CSV→均线→一行出图”）进行中。
