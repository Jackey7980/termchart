# 数学与几何工具集

termchart 的数学层刻意保持"小而准"：不做通用矩阵库（那是 Eigen 的生态位），只收图表渲染与数据分析真正用得到的那一小撮。所有函数都遵循同一套约定：

- 入参用 `std::span<const double>`，不写模板——`vector` / `array` / C 数组都能直接传；
- 不抛异常；`NaN` 视为缺失（能跳过的跳过，无法定义的返回 `NaN`）；
- 矩阵运算"尺寸不匹配/奇异"一律返回 `false` 且**不修改** `out`（fail-loud）；
- 除 `util/geom.hpp` 外均为 header-only，单头分发时自动带上。

## 描述统计 `math/stats.hpp`

| 函数 | 说明 |
|---|---|
| `mean(xs)` | 算术均值，Kahan 补偿求和 |
| `sum(xs)` | Kahan 补偿求和 |
| `median(xs)` | 中位数，`nth_element` 平均 O(N) |
| `variance(xs, ddof = 0)` | 方差，Welford 单遍；`ddof=1` 为样本方差 |
| `stddev(xs, ddof = 0)` | 标准差 |
| `covariance(xs, ys, ddof = 0)` | 成对协方差，长度不等取较短者 |
| `covariance_matrix(data, n, d, out, ddof = 0)` | **d×d 协方差矩阵**（行主序样本），PCA 前置 |
| `pearson(xs, ys)` | 皮尔逊相关系数 |
| `quickselect(xs, k)` | 原地第 k 小；越界返回 `NaN` |
| `quantile(xs, p)` | 分位数（线性插值法，`pos = p·(n-1)`） |
| `tukey_fence(xs, k = 1.5)` | 箱线图"须"，返回 `TukeyFence`（含 `q1/median/q3/iqr/lo/hi`） |

要点：

- `quantile(xs, 0.5) == median(xs)`，两条路径不会漂移；
- `tukey_fence` 在 IQR 退化为 0 时改用 `median ± k·max(|median|, 1e-9)` 撑开，绝不把整列判成离群；
- `covariance_matrix` 按行 listwise 删除：某行任一维为 `NaN/Inf` 就整行跳过；`PcaChart` 的清洗口径与之一致。

```cpp
// 2 维、4 个样本（行主序）
std::vector<double> data = {1, 2, 2, 4, 3, 6, 4, 8};
std::vector<double> cov(2 * 2);
math::covariance_matrix(data, 4, 2, cov, /*ddof=*/1.0);
// cov[0]=var(x), cov[1]=cov(x,y), cov[2]=cov(y,x), cov[3]=var(y)
```

## 插值与曲线 `math/interpolate.hpp`

| 函数 | 用途 |
|---|---|
| `linear_at(points, x)` | 折线线性插值 |
| `CubicSpline::fit` / `eval` | 数据域三次样条（曲线不必过数据点，适合拟合） |
| `smooth_step(edge0, edge1, x)` | `3t²-2t³` 缓动，动画 / 过渡 |
| `catmull_rom(p0..p3, t)` | 折线变丝滑，**经过每个途经点** |
| `quadratic_bezier(p0, c, p1, t)` | 二次贝塞尔（TTF 字形轮廓） |
| `cubic_bezier(p0, c1, c2, p1, t)` | 三次贝塞尔（canvas Path） |

> 二次/三次不可混用：用二次公式套三次控制点会得到错误曲线。

```cpp
// 折线首尾各补一个端点，再在相邻四点之间细分采样
const Point p = math::catmull_rom(a, b, c, d, 0.5);
```

## 线性代数 `math/linalg.hpp`

| 函数 | 说明 |
|---|---|
| `multiply(a, b, out, n)` | n×n 矩阵乘 |
| `inverse(a, out, n)` | 求逆 |
| `solve(a, b, out, n)` | 解 Ax = b |
| `determinant(a, n)` | 行列式；`n == 0` 返回 1 |
| `euclidean_distance(a, b)` | 欧氏距离 |
| `cosine_similarity(a, b)` | 余弦相似度，结果 ∈ [-1, 1] |

矩阵一律**行主序 n×n**、`span<const double>` 入参、调用方自管缓冲。距离/相似度长度不等时取较短者；空、含 `NaN` 或零向量返回 `NaN`。

## 平面几何 `util/geom.hpp`

几何的唯一真源，`graph/crossing`、`canvas/path`、`canvas/clip` 都直接复用它。

| 函数 | 说明 |
|---|---|
| `cross(a, b, c)` | 叉积 z 分量：左转 > 0、右转 < 0、共线 == 0 |
| `signed_distance_to_line(p, a, b)` | 点到无限直线的带符号距离 |
| `distance_to_segment(p, a, b)` | 点到线段的最短距离 |
| `segments_cross(a, b, c, d)` | 是否**真相交**（贴端点、共线重叠不算） |
| `segment_intersection(a, b, c, d, out)` | 唯一交点且落在线段内时返回 `true` |
| `polygon_area(points, count)` | 鞋带公式；逆时针为正 |
| `polygon_centroid(points, count)` | 面积加权重心 |
| `point_in_polygon(p, points, count)` | 射线法；边界视为在内部 |
| `convex_hull(points)` | Andrew 单调链，返回逆时针顶点 |

> 这个文件不是 header-only：声明带 `TERMCHART_API`，定义在 `src/util/geom.cpp`。

## 可复现随机 `util/random.hpp`

`util::Rng` 是 splitmix64，`constexpr`、跨平台同种子同结果：

```cpp
util::Rng rng(42);                 // 固定种子 → 固定序列
const double u = rng.unit();       // [0, 1)
const double x = rng.range(-1, 1); // [lo, hi)
const std::uint64_t k = rng.integer(100);
```

散点抖动、力导向初始布局、测试 fixture 都用它——不要用 `std::random_device`，那不可复现。

## 选型速查

| 想做的事 | 用哪个 |
|---|---|
| 找离群值 / 稳健 Y 轴域 | `tukey_fence` |
| 折线更顺、仍要过原始点 | `catmull_rom` |
| 数据域平滑拟合（不必过点） | `CubicSpline` |
| 入场/过渡动画曲线 | `smooth_step` |
| 高维样本相似度 | `cosine_similarity` / `euclidean_distance` |
| PCA 前置协方差 | `covariance_matrix` |
| 点是否在多边形内 | `point_in_polygon` |
| 固定随机的布局抖动 | `util::Rng` |
