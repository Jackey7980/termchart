<!-- 由 tools/gen_api_docs.py 生成，禁手改。更新方式：重跑该脚本。 -->
# API 参考

扫描 `include/termchart/**` 的 **372 个公开头文件**生成，按模块（层号源：
`cmake/TermchartModules.cmake`）分节。每个头列出顶部注释与声明——
class / struct / enum class / 命名空间级函数（含头内 inline 定义，签名原样搬运）。
本文件不含手写说明；各声明语义见对应头文件。

## 模块索引

| 模块 | 层 | 头文件数 |
| --- | --- | --- |
| （顶层头） | — | 6 |
| detail | L-1 | 9 |
| core | L0 | 13 |
| math | L1 | 5 |
| util | L1 | 17 |
| color | L2 | 6 |
| io | L2 | 4 |
| math3d | L3 | 3 |
| math4d | L3 | 3 |
| scale | L3 | 10 |
| data | L4 | 5 |
| scene | L4 | 6 |
| image | L5 | 8 |
| raster | L6 | 9 |
| network | L7 | 5 |
| term | L7 | 13 |
| document | L8 | 4 |
| protocol | L9 | 7 |
| canvas | L10 | 8 |
| layout | L11 | 11 |
| chart | L12 | 79 |
| chart3d | L12 | 4 |
| chart4d | L12 | 2 |
| graph | L13 | 46 |
| render | L14 | 5 |
| interaction | L15 | 18 |
| backend | L16 | 15 |
| dsl | L17 | 15 |
| widget | L18 | 18 |
| plugin | L19 | 4 |
| app | L20 | 7 |
| api | L21 | 4 |
| gui | L22 | 3 |

## （顶层头）

### `deprecate.hpp`

> deprecate.hpp —— 弃用标记宏
>
> 弃用流程（docs/contrib/deprecation.md）：
> 第一步加 TERMCHART_DEPRECATED("将于 vX.Y 移除，改用 xxx")，至少一个 minor 周期保留；
> 第二步在下个 major 删除。测试用 -Werror=deprecated-declarations 时，
> 内部调用点必须显式加 TERMCHART_DEPRECATED_IGNORE_BEGIN/END。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `export.hpp`

> export.hpp —— 符号可见性宏
>
> 所有需要跨库边界（或被应用程序直接链接）的符号都标注 TERMCHART_API。
> 静态库构建时为空，避免污染。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `fwd.hpp`

> fwd.hpp —— 公开类型前向声明
>
> 用途：让只"持有指针/引用"的头不必拉进完整定义（降低编译耦合）。
> 前向声明的类型必须与真实声明保持一致（namespace / 模板参数 / class vs struct），
> 否则会有 ODR 风险——新增前向声明时先看真实定义。

- `struct Rgb`
- `enum class ColorDepth : std::uint8_t`
- `enum class Attr : std::uint16_t`
- `enum class ColorKind : std::uint8_t`
- `class Color`
- `class Style`
- `struct Cell`
- `class GraphemeCluster`
- `class CellBuffer`
- `class Arena`
- `class Rng`
- `enum class ColormapKind : std::uint8_t`
- `enum class PaletteKind : std::uint8_t`
- `class Gradient`
- `enum class ImageProtocolKind : std::uint8_t`
- `struct CapEnvironment`
- `struct TerminalCaps`
- `struct TerminalSize`
- `struct TerminalOptions`
- `class Terminal`
- `class RawMode`
- `class Waker`
- `class DamageTracker`
- `class Renderer`
- `class ApiOptions`

### `termchart.hpp`

> termchart.hpp —— 伞形头（极简）
>
> 只包含"开箱即用"所需的最小集合：核心数据结构 + 终端 + 渲染 + 一行接口。
> 全量见 termchart_full.hpp（阶段 6）。
>
> 分层豁免：伞形头是聚合入口，天然依赖全模块，check_layers.py 对它不做层号判定。

- `const char* version_string() noexcept`

### `termchart_full.hpp`

> termchart_full.hpp —— 全量伞形头（自动生成，请勿手改；见 tools/gen_full_header.py）
> 包含整个 termchart 公共 API：termchart.hpp 覆盖 core/util/color/term/render/api，
> 这里再补齐 chart/graph/scale/raster/dsl/widget/gui/backend/interaction 等全部子模块。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `version.hpp`

> version.hpp —— 版本宏
>
> 与本轮的库版本同源：CMake 侧通过 target_compile_definitions 注入同值宏，
> 头文件侧的默认值用于"单头文件 / 直接拷 assume 头文件"场景。

- （无命名空间级声明，或为纯宏 / 伞形头）

## detail（L-1，生成的数据表，禁手改）

### `detail/ansi_tables.hpp`

> ansi_tables.hpp —— ANSI / 控制字符查表（层号 -1 横切数据表）
>
> 数据表职责：C0 控制码 ↔ 脱字符记号（^@、^[…）与助记名，以及 ESC 序列前缀。
> **不负责生成着色序列** —— SGR 的唯一生成点是 render/sgr.hpp（见 ROADMAP 阶段 1）。
> 本文件不依赖 termchart 其余文件；任何层都可 include。

- `struct C0Entry`

### `detail/block_chars.hpp`

> block_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
>
> 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
> 块元素（U+2580 系）的唯一来源：raster/block、chart/progress、indicator/* 共用。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `detail/box_chars.hpp`

> box_chars.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
>
> 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
> 制表符（U+2500 系）的唯一来源：graph/wiregrid、layout/box、widget/border 都走这里，
> 不允许在别处硬编码 U+2500 序列。
>
> 邻接掩码约定（bitOR）：Up = 1, Right = 2, Down = 4, Left = 8

- `enum class BoxMask : std::uint8_t`
- `enum class BoxWeight : std::uint8_t`

### `detail/braille_table.hpp`

> braille_table.hpp —— 由 tools/gen_braille_table.py 生成，禁手改。
>
> 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
> 盲文点阵 vil：**点 i ↔ bit (i-1)**，码点 = U+2800 | dots，这一约定只在本处定义。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `detail/grapheme_tables.hpp`

> grapheme_tables.hpp —— 由 tools/gen_grapheme_tables.py 生成，禁手改。
>
> 层号 -1：横切数据表，任何层都可 include 本文件；本文件不依赖 termchart 其余文件。
> 只提供 Grapheme_Cluster_Break 属性（UAX #29 简化版）的区间数据。
> 分割算法在 util/grapheme.hpp（L1）；core/grapheme.hpp 只定义数据结构。

- `enum class Gb : std::uint8_t`
- `struct GbRange`

### `detail/sixel_table.hpp`

> sixel_table.hpp —— Sixel 编码字符表（层号 -1 横切数据表）
>
> Sixel 每个 6 位数据值 v ∈ [0,63] 用一个可打印字符 0x3F+v 表示：
> v=0  -> '?'（空/透明）      v=1  -> '@'     ...  v=63 -> '~'
> 这张表把"值 ↔ 字符"这件事钉死在一处，protocol/sixel.hpp 与本PROTOCOL 旁的
> 解码测试共用同一份定义。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `detail/sort_network.hpp`

> sort_network.hpp —— 小数组排序网络（层号 -1 横切数据表）
>
> 为什么用排序网络：glibc 的 std::sort 对小 n 也有调用开销，而 layout/label_avoid、
> raster 子像素合成里存在大量"固定长度 2~8 的数组、每帧排序"的热点。
> 排序网络无分支预测失败、无递归，且 constexpr 可在编译期跑完。
>
> 网络本体是"比较器下标对"表：pair {i, j} 表示无条件交换 a[i] 与 a[j]（若逆序）。

- `struct ComparePair`

### `detail/turbo_table.hpp`

> turbo_table.hpp —— 由 tools/gen_turbo_table.py 生成，禁手改。
>
> 数据源：assets/colormaps/turbo.dat（官方 Turbo 色表，Google，Apache-2.0）。
> 生成器只做"数据搬运"，不参与任何数值加工——色值需要改就改 .dat 再重跑。
>
> 层号 -1：横切数据表，任何层都可 include 本文件；它不依赖 termchart 其余文件。

- `struct TurboEntry`

### `detail/unicode_tables.hpp`

> unicode_tables.hpp —— 由 tools/gen_unicode_tables.py 生成，禁手改。
>
> 层号 -1：横切数据表，任何层都可 include；不依赖 termchart 其余文件。
> 只回答"码点级朴素宽度"，不含 emoji 序列规则（见 docs/design/emoji_width.md）；
> cluster 宽度由 util/wcwidth.hpp 结合 core/grapheme 数据结构给出。

- `struct CpRange`

## core（L0）

### `core/arena.hpp`

> arena.hpp —— 帧分配器
>
> 用途：每帧大量的短命小对象（anvas 路径片段、布局候选矩形）。
> 语义：**只能 reset()，不能单独释放**——一帧结束整块回收，无析构开销。
>
> 硬约束（docs/design/memory_model.md）：
> * 持有堆资源的类型**禁止**进 arena：create<T>() 用 static_assert 卡住
> （非 trivially destructible 直接编译失败）。
> * Arena 自身非拷贝非移动：它是"帧"的同义词。

- `class Arena`

### `core/attr.hpp`

> attr.hpp —— 字符属性位（header-only）
>
> 属性只有这五个：Bold / Dim / Italic / Underline / Reverse。
> 位运算全部 constexpr，便于编译期组合常量样式。

- `enum class Attr : std::uint16_t`

### `core/buffer.hpp`

> buffer.hpp —— CellBuffer：连续网格 + 稀疏侧面表
>
> 全库的**中枢结构**（L0）：
> * 终端实时路径：canvas to_buffer() → CellBuffer → render/renderer → 字节流；
> * snapshot 导出：backend/snapshot/* 直接吃 CellBuffer；
> * 差分开销由 prev/cur 两个 CellBuffer 逐行为单位比较（见 render/diff.hpp）。
>
> 侧面表是**稀疏**的（unordered_map<index, Sides>）：绝大多数单元没有组合记号，
> 逐单元存一个数组会让 CellBuffer 尺寸直接翻倍。
>
> 注意：CellBuffer 持有堆资源，**禁止放进 arena**（见 core/arena.hpp）。

- `class CellBuffer`

### `core/cell.hpp`

> cell.hpp —— 终端字符单元
>
> 关键约定（破坏它等于破坏整个宽度模型）：
> * Cell 只存**单个主码点**；组合记号绝不进 Cell。
> * 组合序列（e.g. 'e' + U+0301）由 CellBuffer 的稀疏侧面表承载（side bearing），
> 数据结构定义在 core/grapheme.hpp，分割在 util/grapheme.hpp。
> * 于是 Cell 是 trivially copyable：差分遍历时可 memcpy/memcmp。

- `struct Cell`
- `TERMCHART_API bool is_blank(const Cell& cell) noexcept`

### `core/color_depth.hpp`

> color_depth.hpp —— 终端颜色能力档位（header-only）
>
> 与具体后端无关：它描述"终端能吃下多少颜色"，由 term/caps.hpp 产出，
> 被 render/sgr.hpp 与 color/quantize.hpp 消费。

- `enum class ColorDepth : std::uint8_t`

### `core/error.hpp`

> error.hpp —— 错误枚举与类别
>
> 错误处理边界见 docs/design/error_handling.md：
> * **可恢复的、用户可能遇到的**失败 → Expected / Result<T>（本文件 + expected.hpp）；
> * **契约破坏 / 内部不变量** → util/assert.hpp（Release 下 abort）；
> * 渲染热路径一律不用异常：改为返回 Result 或钳位。

- `enum class ErrorCode : std::int32_t`
- `enum class ErrorCategory : std::uint8_t`
- `const char* to_string(ErrorCode code) noexcept`
- `const char* describe(ErrorCode code) noexcept`

### `core/expected.hpp`

> expected.hpp —— Expected<T, E> 与 Result<T> 别名
>
> 为什么不直接用 std::expected：它是 C++23 库设施，本库要求 C++20；
> 而且我们只需要"值或错误码"这个小子集。语义对齐 std::expected：
> * has_value() / operator bool()；
> * value() 在无值时**抛 std::logic_error**（调用方已是 bug）；
> * error() 在有值时属 UB（与 std::expected 一致，此处以异常兜底）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `core/grapheme.hpp`

> grapheme.hpp —— 组合序列（grapheme cluster）的**数据结构**
>
> 分层约定（见 ROADMAP 阶段 1"一旦定错后续全错"）：
> * 本文件**只定义结构，不做分割**；Splitter 在 util/grapheme.hpp（L1）；
> * 宽度模型是"主码点 + 侧面列表"（side bearing）：Cell 存主码点，
> CellBuffer 用稀疏表保存额外的组合记号。
>
> 为什么定长内联数组而非 vector：CellBuffer 一帧内可能有大量侧面项，
> 逐个堆分配会毁掉帧率与局部性。上限来自 UAX #15 的建议：
> 实用文本极少超过 7 个组合记号，超限则丢弃尾部（并在 add 时返回 false）。

- `class GraphemeCluster`

### `core/point.hpp`

> point.hpp —— 浮点坐标点（header-only）
>
> canvas/ 用浮点，"最后一像素/半格"才能对齐；core 层只提供 POD 与必要算子。

- `struct Point`

### `core/rect.hpp`

> rect.hpp —— 浮点矩形（header-only）
>
> 约定：x/y 为左上角，宽高非负；所有关系谓词为闭-开区间（right/bottom 不含在内），
> 与 canvas 的半开像素约定一致。

- `struct Rect`

### `core/rgb.hpp`

> rgb.hpp —— 颜色 POD（header-only，属 cmake/header_only.txt）
>
> 只表达颜色本身，不含"如何输出"：输出编码在 render/sgr.hpp（唯一 SGR 生成点），
> 降级策略在 color/quantize.hpp 与 term/color_fallback.hpp。

- `struct Rgb`

### `core/span_utils.hpp`

> span_utils.hpp —— span 安全切片（header-only）
>
> CellBuffer 对外暴露 std::span<Cell>；所有"取子区间"必须走这里，
> 让越界行为在一处定义：**钳位而非抛异常**（渲染热路径不允许异常）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `core/style.hpp`

> style.hpp —— 语义样式（属性位 + 前景 + 背景）
>
> Style 与后端无关：它不认识 SGR，不知道 A'tuin 终端能否显示斜体。
> 从 Style 到字节流的唯一转换点在 render/sgr.hpp。
>
> Color 用"带判别标识的值"而非 variant：Cell 是逐帧遍历百万次的热结构，
> sizeof(Color) 必须小而固定（8 字节），不能承担 variant 的分支与额外存储。

- `enum class ColorKind : std::uint8_t`
- `class Color`
- `class Style`

## math（L1）

### `math/fft.hpp`

> math/fft.hpp —— 快速傅里叶变换（header-only）
>
> Radix-2 Cooley-Tukey，原地计算（除 twiddle 外零分配），re/im 双 span 布局
> （复数交错由调用方决定，原语本身不替谁做决定）。
> 设计约定：
> * n 必须是 2 的幂且 re/im 等长——否则返回 false 且**不改数据**
> （fail-loud：让调用方去 pad，不悄悄挑个邻近长度糊弄）。
> * 逆变换缩放 1/N，保证 forward → inverse 往返恒等（与 scipy 口径一致）。
> * twiddle 每个 stage 直接算 cos/sin 不用递推——递推的误差随 log N 累积，
> 可视化频谱不需要为省几次 trig 赔上精度。
> * 不抛异常；span 为空/不等长/非 2 幂均安全返回。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `math/interpolate.hpp`

> math/interpolate.hpp —— 数据插值（header-only）
>
> 让生硬的折线瞬间丝滑：线性插值（零参数、保单调段）与自然三次样条（C² 连续）。
> 设计约定：
> * 输入 std::span<const double>，xs 必须**严格升序**（图表 x 轴的常态）。
> * 越界**钳位**到首/末值（不外推：外推出的值没有数据支撑，画成图会骗人）。
> * NaN 传播：查询点或两端任一为 NaN 时返回 NaN。
> * 自然样条的二阶导在两端为 0；数据噪声大时可能轻微过冲，monotone (PCHIP)
> 变体记入 backlog，刻意不塞进本文件。
> * 与 graph/route/spline 的区别：那边是连线路由的 Catmull-Rom（途经点），
> 这边是数据域的插值，互不相干。

- `class CubicSpline`

### `math/linalg.hpp`

> math/linalg.hpp —— 稠密小矩阵线性代数（行主序、span、无模板）
>
> 定位：服务 chart3d/chart4d 的 3D/4D 矩阵与数据拟合的最小公倍集——乘法、求逆、
> 解方程、行列式。**刻意不做**通用矩阵类型（那属于 Eigen 的生态位）：这里只认
> span<const double> + 行主序 n×n，零分配、零模板、调用方自管缓冲。
> * 所有尺寸不匹配/奇异的输入返回 false 且不改 out —— fail-loud。
> * 算法：部分选主元高斯消元（numerical stability 的底线）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `math/special.hpp`

> math/special.hpp —— 特殊函数（header-only）
>
> Gamma / lnGamma / erf / erfc：对 C++11 <cmath> 数学特殊函数的薄封装——统一 NaN
> 边界口径（阶乘在 0 与负整数无定义，标准库回 ±Inf 的场合这里显式 NaN），
> 行为跨平台确定（MinGW/libstdc++/libc++ 的 tgamma 边界实现不一）。
> Bessel J 系：标准库没有，自行实现——小/中参数幂级数 + 大参数 Hankel 渐近，
> 分界 |x| = 20（两侧均留一位数余量）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `math/stats.hpp`

> math/stats.hpp —— 描述统计（header-only）
>
> NumPy/pandas 统计函数的最小集：均值、中位数、方差/标准差、协方差、皮尔逊相关。
> 设计约定：
> * 输入一律 std::span<const double>，不用模板——调用方传 vector/array/C 数组皆可，
> 且每个 TU 只编译一份展开，守住"编译极速"。
> * NaN 视为缺失并**跳过**（与 util/data/rolling 同口径）；空输入或全 NaN 返回 quiet NaN。
> * 均值用 Kahan 补偿求和；方差用 Welford 单遍算法——长序列上不丢有效位。
> * 不抛异常、不分配堆内存（median 内部的局部 vector 除外）。

- （无命名空间级声明，或为纯宏 / 伞形头）

## util（L1）

### `util/assert.hpp`

> assert.hpp —— 断言宏（header-only）
>
> 错误处理的第二档（另一档是 core/error.hpp 的 Result）：
> * TERMCHART_ASSERT       —— Release 下也生效（契约破坏必须早失败）；
> * TERMCHART_DEBUG_ASSERT —— 仅 NDEBUG 未定义时生效（昂贵的不变量检查）；
> * 渲染热路径**不要**用它做常规校验：那里用钳位或 Result。
>
> 失败时直接 abort 而非抛异常：不动 ODR/ABI，也不让调用方以为"能 catch 回来"。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/base64.hpp`

> base64.hpp —— 标准 base64（RFC 4648，含 '=' 填充）
>
> 本文件是**规格文件清单之外的补充**，理由：OSC 52 剪贴板（term/，L2）与
> kitty / iterm2 图像协议（protocol/，L9）都需要 base64，而 term 在 protocol 之下，
> 不能反向依赖。把唯一实现放在最底层 util/，所有需要方都调这里，避免出现第二份
> 实现后各自漂移（protocol 里原先那份内联实现已改调本文件）。
>
> 纯函数、无状态、无 I/O；列入 header-only 清单。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/bits.hpp`

> bits.hpp —— 位操作（header-only）
>
> 光栅器（raster/*）与制表符绘制大量依赖位掩码：这里是**唯一**的位操作归口，
> 避免各处手写 `1 << (n - 1)` 而错位。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/format.hpp`

> format.hpp —— 轻量整数 / 浮点格式化
>
> 为什么不用 iostream / std::format：
> * iostream 受 locale 影响，同一份程序在不同机器上输出不同（golden 测试会炸）；
> * std::format 依赖较新的标准库且体积大；项目只要"固定小数位 + 去尾零"两种。
> 因此这里用 <charconv> 的 to_chars：locale 无关、无分配、最快。

- `TERMCHART_API std::string format_double(double value, int precision = 2)`
- `TERMCHART_API std::string format_double_trimmed(double value, int precision = 2)`
- `TERMCHART_API void append_double(std::string& out, double value, int precision = 2)`
- `TERMCHART_API std::string format_int(std::int64_t value)`
- `TERMCHART_API std::string format_compact(double value, int precision = 1)`

### `util/geo/geojson.hpp`

> util/geo/geojson.hpp —— GeoJSON 解析（最小集，仅 FeatureCollection）
>
> GeoJSON 规范参考 RFC 7946。本文件只覆盖：
> * FeatureCollection → 一组 Polygon / MultiPolygon（边界数据）；
> * 每个 Feature 的 `properties` 全部拷成 std::string → std::string；
> * `geometry.type == "Polygon" | "MultiPolygon"`（其他类型抛错并返回 false）；
> * 坐标数组 [lon, lat] → Point（高度忽略）。
>
> 用途：把世界地图/中国省份/主要城市的边界从 assets/maps/*.json 读进 termchart，
> 再走 util::geo::projection 投影到画布坐标系。
>
> 故意不写：
> * GeometryCollection（用得少，Poly/MultiPoly 已覆盖 ≥99% 边界数据）；
> * 弧线（RFC 7946 §3.1.6 弧线段，termchart 用直线段足够）；
> * 跨子午线环（antimeridian cutting）—— caller 自己预处理。
>
> 错误：解析失败返回 false，错误信息通过 out_error 输出。

- `struct GeoPolygon`
- `struct GeoFeatureCollection`
- `struct GeoBounds`

### `util/geo/projection.hpp`

> util/geo/projection.hpp —— 经纬度 → 平面投影
>
> 极简集：4 种最常用投影，纯函数、零依赖、终端坐标友好（aspect 由 caller 自定）。
> 输入：纬度（-90..90）+ 经度（-180..180）→ 输出：平面 (x, y)（单位无定义，
> 调用方决定 1 单位 = 多少字符格 / 多少像素）。
>
> 约定：y 向上为正（北向上）；调用方需要"y 向下"时自行取负。
>
> 边界：极点附近 Mercator / Equirectangular 输出趋于无穷，由 caller 钳制。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/geom.hpp`

> geom.hpp —— 平面几何：点线距离、线段关系、多边形面积/重心/凸包
>
> 只依赖 core（Point），纯函数、无状态、无 I/O。`graph/crossing` 的严格相交判定
> 与这里是同一套规则，图模块直接复用本文件，不再各写一份。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/grapheme.hpp`

> grapheme.hpp —— grapheme cluster 边界检测（UAX #29 简化版）
>
> 这里是**唯一做分割**的地方（core/grapheme.hpp 只定义数据结构）。
> 属性区间表来自 detail/grapheme_tables.hpp（层号 -1，任何层可用）。
>
> 实现的规则集：GB3（CR×LF）、GB4/GB5（控制符自成簇）、GB6–GB8（Hangul）、
> GB9（×Extend/ZWJ）、GB9a（×SpacingMark）、GB9b（Prepend×）、
> GB11（ZWJ×扩展象形）、GB12/GB13（地区旗标成对连缀）。
> 未实现 emoji 序列的全部细则（UAX #51 的杂项键帽等）——见 ROADMAP 阶段 7。

- `TERMCHART_API std::size_t next_boundary(std::string_view text, std::size_t pos) noexcept`
- `TERMCHART_API std::size_t cluster_count(std::string_view text) noexcept`
- `TERMCHART_API std::vector<GraphemeCluster> split_clusters(std::string_view text)`
- `TERMCHART_API bool is_extended_pictographic(char32_t cp) noexcept`

### `util/logging.hpp`

> logging.hpp —— 分级日志，编译期可关
>
> 两级开关：
> * 运行期 set_log_level()：默认 Info；
> * 编译期 TERMCHART_MAX_LOG_LEVEL（0..5），高于它的 TERMCHART_LOG_* 直接编译消失，
> 保证 Release 里日志分支零成本。
> sink 可替换：测试用 capture_log() 抓取输出，不写 stderr。

- `enum class LogLevel : std::uint8_t`
- `TERMCHART_API void set_log_level(LogLevel level) noexcept`
- `TERMCHART_API LogLevel log_level() noexcept`
- `TERMCHART_API const char* level_name(LogLevel level) noexcept`
- `TERMCHART_API void write_log(LogLevel level, std::string_view message) noexcept`
- `TERMCHART_API void capture_log_begin() noexcept`
- `TERMCHART_API std::vector<std::string> capture_log_end() noexcept`

### `util/math.hpp`

> math.hpp —— 标量数学（header-only）
>
> 只提供终端绘图真正需要的那几个函数；依赖 <cmath> 但不引入 eigen/boost。
> 所有函数 constexpr / noexcept：布局与缩放是在热路径上跑的。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `util/random.hpp`

> random.hpp —— 可复现伪随机（header-only）
>
> 为什么自带：**可复现**是硬需求。散点抖动、力导向初始布局、用例 fixture
> 都要"同一个种子得到同一个结果"，std::random_device/mt19937 的默认初始化做不到。
> 算法：splitmix64（无状态争用、质量足够、实现 20 行）。

- `class Rng`

### `util/scheduler.hpp`

> scheduler.hpp —— 单线程定时任务调度器
>
> 不建线程、不读时钟：**调用方每个 tick 传入"现在"**，调度器据此决定谁到期。
> 于是它是纯逻辑，可以确定性地测（配合 util/timer 的注入时间点），也不会出现
> "库内部偷偷起了个线程"这种事。线程与事件循环属于 app/（L20）。
>
> 周期任务采用**固定间隔、不补偿追赶**：每次触发后从当次时刻重排下一次。
> 长时间卡顿（例如调试器断住）之后不会一次补跑几十次——那会让界面雪崩。

- `class Scheduler`

### `util/task.hpp`

> task.hpp —— 可取消任务句柄
>
> 用途：把"这件事还要不要继续做"从执行体里解耦出来。执行体只持有只读的
> `CancellationToken`（随时问一句 cancelled()），发起方持有 `TaskHandle`
> （可 cancel、可在收尾时 finish）。两者共享一份原子状态，因此 cancel 可以在
> 别的线程发生。
>
> 边界说明：这里**不建线程、不排队、不调度**——只提供取消语义。要定时请用
> util/scheduler，要线程请用 app/（L20）。

- `class TaskState`
- `class CancellationToken`
- `class TaskHandle`

### `util/text.hpp`

> text.hpp —— 文本排版：截断、对齐、换行、填充
>
> 所有操作以**显示宽度**而非码点数为单位——这是终端排版与 Web 的根本差异之一。
> 因此全部依赖 util/wcwidth.hpp 与 util/grapheme.hpp。

- `enum class Align : std::uint8_t`
- `TERMCHART_API std::string repeat(char32_t cp, std::size_t n)`
- `TERMCHART_API std::string pad(std::string_view text, int width, Align align = Align::Left, char32_t pad_char = U' ')`
- `TERMCHART_API std::string truncate(std::string_view text, int max_width, Align align = Align::Left, std::string_view ellipsis = "...")`
- `TERMCHART_API std::vector<std::string> wrap(std::string_view text, int width, bool force_break = true)`
- `TERMCHART_API std::string fit(std::string_view text, int max_width, Align align = Align::Left)`

### `util/timer.hpp`

> timer.hpp —— 单调时钟、秒表、帧计时、超时判断
>
> 全部基于 `std::chrono::steady_clock`：它单调递增，不受系统时间调整影响，
> 适合测耗时；而"墙上时间"应当用 system_clock，本模块不掺和。
>
> 可测试性：帧计时与超时都提供**注入时间点**的接口（tick_at / expired_at），
> 单测因此不依赖真实时间流逝（也不会因为机器慢而抖动）。

- `class Stopwatch`
- `class FrameTimer`
- `class Timeout`

### `util/utf8.hpp`

> utf8.hpp —— UTF-8 编解码，对非法序列容错
>
> 容错策略（这是本文件的核心决策）：**永不抛异常、永不 UB**。
> 输入可能来自 CSV、来自管道、来自 fuzz（tests/fuzz/fuzz_utf8.cpp）：
> * 非法序列 → 产出 U+FFFD，**只消耗 1 字节**后继续（保证游标一定前进）；
> * 截断序列 → 同上；调用方可选地把 "consumed < 期望" 当作"数据还没到"。

- `struct Utf8Decode`
- `TERMCHART_API Utf8Decode decode(std::string_view bytes, std::size_t pos) noexcept`
- `TERMCHART_API std::size_t encode(char32_t cp, char* out) noexcept`
- `TERMCHART_API void append(std::string& out, char32_t cp)`
- `TERMCHART_API std::string to_utf8(char32_t cp)`
- `TERMCHART_API std::size_t codepoint_count(std::string_view text) noexcept`
- `TERMCHART_API bool is_valid(std::string_view text) noexcept`
- `TERMCHART_API Result<std::string> sanitize(std::string_view text) noexcept`

### `util/wcwidth.hpp`

> wcwidth.hpp —— 显示宽度
>
> 宽度模型：**主码点决定宽度，组合记号 (side bearing) 宽 0**（详情见
> docs/design/utf8_handling.md）。因此宽度查询的输入可以是码点或
> GraphemeCluster，二者语义一致。emoji 宽度策略见 docs/design/emoji_width.md。

- `struct WidthOptions`

## color（L2）

### `color/colormap.hpp`

> colormap.hpp —— 连续色表：viridis / inferno / magma / plasma / turbo
>
> 为什么用**多项式拟合**而不是数据文件：
> * 避免把原始数据（CC0）打包进仓库，也避免需要在运行期加载 asset；
> * 拟合的最大误差约 2/255，终端着色场景不可分辨；
> * 于是 colormap 是纯函数，无状态、线程安全、无需初始化。
> 拟合系数来自公开的多项式拟合（Google, "Analytic Approximations for Colormaps"），
> 见 docs/reference/module_matrix.md 之外的说明；需要权威数据时用 utile/sample 表替换。

- `enum class ColormapKind : std::uint8_t`
- `TERMCHART_API Rgb sample(ColormapKind kind, double t) noexcept`
- `TERMCHART_API std::span<const Rgb> stops(ColormapKind kind, std::size_t count = 256)`
- `TERMCHART_API const char* colormap_name(ColormapKind kind) noexcept`

### `color/gradient.hpp`

> gradient.hpp —— 渐变生成
>
> 线性多段渐变：给色阶条、热度标尺、流式明暗用；真·连续色表走 colormap.hpp。
> 这里是无状态值类型：构造后只读，可被 style/theme 复用。

- `class Gradient`
- `TERMCHART_API Rgb mix(Rgb from, Rgb to, double t) noexcept`
- `TERMCHART_API Rgb contrasting_text_color(Rgb background) noexcept`

### `color/named_colors.hpp`

> named_colors.hpp —— CSS 颜色名查找表（header-only）
>
> 用途：让用户能写 "#2f6f4f" 之外的 "seagreen"，也让示例代码更可读。
> 表按名字升序存放，查询走二分——**因此顺序与内容是契约**：新增必须保持有序
> （重新生成见 tools/gen_colormap.py 的同名策略）。
>
> 当前为 **CSS Color 命名色全集 148 色**（含 gray/grey 两套拼法与 rebeccapurple，
> 即 CSS Color Module Level 4 的完整列表，不含 `transparent`/`currentcolor`
> 这两个关键字——它们不是颜色值，由 Color 侧单独处理）。
> 查不到的名字由 Color 侧回落到 "不是颜色名" 而非魔数。
>
> 契约由 `tests/unit/color/test_color.cpp` 守住：表必须严格升序、无重复、
> 数量等于 kNamedColorCount、抽查若干色值与 CSS 规范一致。

- `struct NamedColorEntry`

### `color/palette.hpp`

> palette.hpp —— 命名调色板
>
> 与 colormap 的区别：**离散**。系列图例、多条折线要的是"彼此可区分的一组色"，
> 而不是连续色带。因此 palette 是一份条目顺序重要的数组（第 i 个系列取第 i 色）。

- `enum class PaletteKind : std::uint8_t`
- `TERMCHART_API std::span<const Rgb> palette(PaletteKind kind) noexcept`
- `TERMCHART_API std::span<const Rgb> palette_by_name(std::string_view name) noexcept`
- `TERMCHART_API Rgb series_color(PaletteKind kind, std::size_t index) noexcept`
- `TERMCHART_API std::size_t palette_size(PaletteKind kind) noexcept`
- `TERMCHART_API const char* palette_name(PaletteKind kind) noexcept`

### `color/quantize.hpp`

> quantize.hpp —— 真彩 → 256 → 16 降级
>
> 颜色管线的一环（docs/design/color_pipeline.md）：
> 真彩 Rgb →（终端能力）→ Ansi256 / Ansi16 / None。
> 能力本身由 term/caps.hpp 给出，本文件只做"退化算法"，不知道终端是否存在。

- `TERMCHART_API std::uint8_t to_index_256(Rgb color) noexcept`
- `TERMCHART_API std::uint8_t to_index_16(Rgb color) noexcept`
- `TERMCHART_API Rgb from_index_256(std::uint8_t index) noexcept`
- `TERMCHART_API Rgb from_index_16(std::uint8_t index) noexcept`
- `TERMCHART_API Color degrade(Color color, ColorDepth depth) noexcept`
- `TERMCHART_API Style degrade(Style style, ColorDepth depth) noexcept`

### `color/resolve.hpp`

> resolve.hpp —— Color / Style → 真正的 RGB（GUI 与位图输出的分界点）
>
> 终端侧通常把 `Color` 原样交给 SGR 序列化，由终端去解释（16 色/256 色/真彩）；
> 但 GUI 和位图输出（PNG 截图、sixel 预览）必须拿到**具体像素色**。这个文件就是
> 那道转换，而且是唯一实现：
> * None  → 用调用方给的默认色（终端默认前景/背景），不是黑色；
> * Ansi16 / Indexed → 查表；
> * Rgb   → 先按 ColorDepth 量化再取色，保证"GUI 里看到的颜色"与"终端实际
> 显示的颜色"一致（深度不足时不假装能显示真彩）。

- `struct ResolveOptions`
- `struct ResolvedStyle`

## io（L2）

### `io/csv.hpp`

> io/csv.hpp —— CSV 数值入口（零拷贝切分，L2）
>
> 与 dsl/csv_sniff 的分工（**刻意两份，不是遗漏**）：
> * 本文件：数值数据进图表的快路径——string_view 切分 + from_chars 直转
> double，列主序 vector<vector<double>>，空字段/NA 落 NaN；
> * dsl/csv_sniff：方言分析与表格文档（列类型推断 → document::TableModel），
> 它依赖 document（L8），而 io=2 不能 include L8，**结构上无法下沉合并**；
> 未来若合并，需先把它的 TableModel 组装层拆到 L8+，记入 backlog。
>
> 支持范围（明确列出，避免"以为支持"）：
> ✅ 单字符 delimiter，0 = 自动嗅探（, ; \t | 中引号外出现最多者）；
> ✅ 表头可选；UTF-8 BOM 跳过；注释行（comment 前缀，允许前导空白）；
> ✅ 引号字段 "..."：内含分隔符与换行；"" 表示一个字面引号；
> 无转义的字段走零拷贝 view 快路径，含 "" 的字段退化为一次分配；
> ✅ 行尾 LF / CRLF / CR；
> ✅ 缺值口径：空字段与 "NaN"/"nan"/"NA"（大小写不敏感）→ quiet NaN。
> ⛔ 非数字字段（如文本列）→ ParseError（带行列诊断）：这是数值入口，
> 要容忍文本请走 dsl/csv_sniff 的 TableModel；
> ⛔ 数字前导 + 号（from_chars 口径，与 io/json 一致）；
> ⛔ 反斜杠转义、多字符分隔符、`sep=` 指令。

- `struct CsvOptions`
- `struct CsvTable`

### `io/diagnostic.hpp`

> io/diagnostic.hpp —— 解析诊断（header-only，L2）
>
> 从 dsl/parser_base.hpp 下沉而来：io 的 csv/json 解析需要同口径的诊断
> （ErrorCode + 行列 + 字节偏移），而 io=2 不能 include dsl=17（层号硬墙）。
> dsl/parser_base.hpp 现以 using 别名 re-export 本类型，既有调用点零改动。
> 设计口径（与 core/error.hpp 的错误策略一致）：
> * 解析不抛异常：错误记录进 ParseDiagnostic，由入口转成 Result 错误码；
> * 只保留第一个错误（后续错误往往是它引发的连锁噪声）。

- `struct ParseDiagnostic`

### `io/json.hpp`

> io/json.hpp —— JSON 解析（L2）
>
> 本文件于 2026-10-21 从 dsl/json.hpp 下沉收编（"数据入口归 io"）：解析核心与
> 方言无关，dsl 及其余上层模块直接复用；dsl/json.hpp 现为 using 别名 shim，
> 既有 dsl::JsonValue / dsl::parse_json 调用点零改动。
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 完整 JSON 值：null / true / false / 数字 / 字符串 / 数组 / 对象，任意嵌套；
> ✅ 数字：负号、小数、科学计数法（1e5、-2.5E-3）；按 double 存储（大整数
> 会丢精度，如实记录而非假装精确）；
> ✅ 字符串转义：\" \\ \/ \b \f \n \r \t \uXXXX（含代理对 → UTF-8）；
> ✅ 对象保持**插入序**（vector<pair> 而非 map），重复键后者覆盖前者；
> ✅ 嵌套深度上限（默认 64）与输入长度上限（默认 16MB），超限返回错误而非
> 栈爆/内存爆；
> ✅ 诊断：首个错误带行列与字节偏移（与 io/csv 同口径）。
> ⛔ 不支持：注释、尾逗号、单引号字符串、NaN/Infinity、数字前导 + 号、
> 控制字符未转义直出（按 JSON 规范拒绝）。
>
> 错误：解析失败返回 ErrorCode::ParseError / OutOfRange / InvalidArgument，
> 可选地把**首个**诊断写到 out_diagnostic。

- `class JsonValue`
- `struct JsonOptions`

### `io/stream.hpp`

> io/stream.hpp —— 分块行读取（L2）
>
> 用途：GB 级 CSV/日志不整文件入内存。内部 64KB 缓冲，逐行产出 string_view
> （视图在下一次 next()/close() 前有效，要跨行保留请自行拷贝）。
> 关键不变量：
> * 任意长的单行都不 OOM：超长行走"compact + 续读"路径，缓冲最坏增长到
> 该行自身长度（行必须放得下，没有第二条路）；
> * 行尾口径：LF / CRLF / CR 均切行，行内容不含行尾符；
> * 不抛异常：打开失败走 Result；读完 next() 返回 false。

- `class LineReader`

## math3d（L3）

### `math3d/camera.hpp`

> camera.hpp —— 轨道相机 + 透视投影到画布坐标（header-only）
>
> 设计取舍：3D 渲染**不**自造 z-buffer/终端光栅器，而是把 3D 点投影到 2D 画布坐标
> 后，作为 Scene 图元交给已有的 Scene→Canvas（braille/halfblock）管线。深度只用来
> 决定着色（近亮远暗），得到可辨识的立体感而不重复造轮子。
>
> project() 返回的屏幕坐标即画布格坐标（左上为原点，y 向下），直接喂给 Scene 的
> MoveTo/LineTo/RectOp 即可被 render_scene 消费。

- `struct Projected`
- `struct Camera`

### `math3d/mat4.hpp`

> mat4.hpp —— 4×4 矩阵（header-only，行主序）
>
> 约定（与绝大多数图形教材一致）：矩阵 × 列向量。元素 m[r*4+c] 即第 r 行第 c 列。
> `transform(p)` 计算 M·p（齐次 w=1）。投影链 = perspective · look_at，即
> clip = perspective(look_at(p))。所有函数 constexpr，便于单测断言确定性结果。

- `struct Mat4`

### `math3d/vec3.hpp`

> vec3.hpp —— 三维向量（header-only）
>
> 仅放 3D 投影真正需要的那几个运算；依赖 <cmath>，不引入 eigen/glm。
> 约定：行主序矩阵 × 列向量（见 mat4.hpp），本文件只负责向量代数。

- `struct Vec3`

## math4d（L3）

### `math4d/camera4.hpp`

> camera4.hpp —— 4D→3D 透视投影（header-only，"除以 W"的齐次除法）
>
> 这是 4D 管线第一阶段的世界→3D 相机：把 4D 相机架在第 4 轴（W）上、看向
> 原点，对一个 Vec4 做透视除法，产出 math3d::Vec3（交给下游 chart3d 的 3D
> 轨道相机做第二阶段 3D→2D）。与 math3d::Camera 的关系：后者吃 Vec3 出画布
> 坐标，本结构吃 Vec4 出 Vec3——两者串起来就是完整的 4D→2D 链。
>
> 之所以返回 math3d::Vec3 而不是 math4d 自有类型：3D 向量的语义归 math3d，
> 且下游（chart4d）本来就要把它喂给 math3d 的相机；math4d 与 math3d 同为 L3，
> 该依赖方向被层号允许。

- `struct Projected4`
- `struct Camera4`

### `math4d/mat5.hpp`

> mat5.hpp —— 5×5 矩阵（header-only，行主序，4D 齐次坐标）
>
> 约定与 mat4.hpp 一致：矩阵 × 列向量。用 5×5（而非 4×4）是为了容纳 4D
> 齐次坐标 (x,y,z,w,1)：`transform(p)` 计算 M·(x,y,z,w,1) 并取前四个分量，
> 于是平移也落进同一个矩阵（与 mat4 的 w=1 齐次平移同思路）。所有函数
> constexpr，便于单测断言确定性结果。
>
> 4D 旋转与 3D 的本质区别：3D 只有 3 个坐标轴、旋转发生在**轴正交的平面**里
> （XY/XZ/YZ）；4D 有 4 个轴，任取两轴张成一个旋转平面，共 C(4,2)=6 个：
> XY(0,1) XZ(0,2) XW(0,3) YZ(1,2) YW(1,3) ZW(2,3)。
> 每个都是在对应二维子平面上嵌一个标准 2D 旋转、其余对角线为 1。

- `struct Mat5`

### `math4d/vec4.hpp`

> vec4.hpp —— 四维向量（header-only）
>
> math3d::Vec3 的 4D 对应物。仅放 4D 投影真正需要的运算；依赖 <cmath>，
> 不引入 eigen/glm。约定与 vec3.hpp 一致：行主序矩阵 × 列向量（见 mat5.hpp），
> 本文件只负责向量代数。4D 没有叉积的直推（那是 7D 才有），故不提供 cross。

- `struct Vec4`

## scale（L3）

### `scale/auto_domain.hpp`

> auto_domain.hpp —— 由数据范围反推「好看」的值域
>
> 图表不该把轴正好卡在 min/max 上（看着压抑且易出界）。这里按 5% 留白后，
> 把两端对齐到 nice 步长；可选 include_zero 让坐标轴穿过原点（财务/差值图常用）。

- `struct Domain`
- `TERMCHART_API Domain nice_domain(double data_min, double data_max, bool include_zero = false) noexcept`

### `scale/band.hpp`

> band.hpp —— 带状比例尺（每个类别占一条等宽带，forward 返回带中心）
>
> 与 CategoryScale 把类别压成"网格点"不同，BandScale 把 [range_min, range_max]
> 均分成 n 条等宽带，第 i 个类别落在第 i 条带的中心。柱状/条形图让柱体落在带
> 内、两侧留 padding 时观感最自然。band_width() 暴露单带宽度供绘制。

- `class BandScale final : public Scale`

### `scale/category.hpp`

> category.hpp —— 类别比例尺（离散类别等距排布）
>
> 把 0..n-1 的类别索引线性映射到 [range_min, range_max]：第 i 个类别落在
> 等距网格点上。柱形/条形/分类型坐标轴常用。forward(类别索引) 取对应网格点，
> inverse(像素) 反算最近类别索引（用于命中判定）。

- `class CategoryScale final : public Scale`

### `scale/linear.hpp`

> linear.hpp —— 线性比例尺（最常用，也是其他比例尺的退化形态）
>
> 默认构造即「单位映射」domain [0,1] → range [0,1]，方便在未知数据范围时
> 先占位、后 set_domain。clamp 默认开启：绘图 API 的越界输入极其常见，
> 默认密封比默认崩溃友好。

- `class LinearScale final : public Scale`

### `scale/log.hpp`

> log.hpp —— 对数比例尺
>
> 域必须为正（或同时为负，这里只支持正域——负域先取绝对值再映射，调用方处理）。
> 零与负数在 forward 里返回 NaN：对数域里它们没有位置，UI 应在此之前过滤或
> 切到线性子段，而不是让整张图消失。

- `class LogScale final : public Scale`

### `scale/nice.hpp`

> nice.hpp —— nice number 算法（刻度与值域对齐的核心）
>
> 「好看的数」= 1 / 2 / 5 × 10^k。它让轴标签对人类可读，而不是 0.317、3.142…
> 这是 Heckbert「Graphics Gems」经典算法的最小可用版，被 ticks/auto_domain 复用。

- `TERMCHART_API double nice_number(double x, bool round = false) noexcept`
- `TERMCHART_API double nice_step(double span, int target_ticks) noexcept`

### `scale/registry.hpp`

> registry.hpp —— 比例尺注册表（与 chart/raster 同一套契约）
>
> instance() 用 function-local static：**构造前注册、构造后只读**（调用方在自身构造
> 时对注册表做浅拷贝快照并冻结，运行期注册属 UB——见 docs/design/registry.md）。
> 内置 `linear` / `log` 直接在 create() 里给出；自定义比例尺用 register_scale()
> 登记，重名以最后一次为准（便于测试里临时替换）。

- `class ScaleRegistry`

### `scale/scale.hpp`

> scale.hpp —— 数据坐标 ↔ 显示坐标的抽象（比例尺基类）
>
> 终端图表和 GUI 控件都要把「数据域」映射到「像素/归一域」。把这件事抽成
> 一个多态基类，是为了让 chart / graph / canvas 在**不知道具体映射**的情况下
> 工作：换线性为对数、换连续为分类，调用方代码一行不动。
>
> 不变量（所有派生类必须遵守）：
> * forward(data) 单调；inverse(forward(x)) ≈ x（在 valid 域内）；
> * 越界输入走 NaN 或夹紧，绝不未定义行为（数据来自用户 CSV，含 NaN）。

- `class Scale`

### `scale/ticks.hpp`

> ticks.hpp —— 刻度生成
>
> 两种入口：
> * nice_ticks：纯数据轴，无需比例尺，直接给 [dmin,dmax] 上的 nice 刻度；
> * ticks_for：已有 Scale（线性/对数都行），在其 range 上均匀取点，但刻度
> 值对应 nice 的数据坐标——所以对数轴上刻度是 1/10/100 而非等距空格。

- `TERMCHART_API std::vector<double> nice_ticks(double domain_min, double domain_max, int target = 5) noexcept`
- `TERMCHART_API std::vector<double> ticks_for(const Scale& scale, int target = 5) noexcept`

### `scale/time.hpp`

> time.hpp —— 时间比例尺（时间戳数值线性映射，语义化时间轴）
>
> 把时间（以数值表示的时间戳，例如 Unix 秒/毫秒；单位由调用方统一）线性映射到
> 显示域。calendar-aware 的刻度（年/月/日/时）由 ticks 模块按数据范围选取，
> 本类只负责坐标变换。与 LinearScale 同构，单列一类以明确"这是时间轴"语义，
> 便于图表层识别并切换到时间刻度策略。

- `class TimeScale final : public Scale`

## data（L4）

### `data/aggregate.hpp`

> util/data/aggregate.hpp —— 聚合（group by + reducer）
>
> 设计目标：pandas DataFrame.groupby().agg() 的"够用版"，零依赖。
> 数据模型：`DataView` 是对外部 std::vector<double> 的非持有视图（指针+长度），
> 调用方负责生命周期；多列对齐靠 `Columns` —— 一组同长度 DataView，
> "分组键"由第一列决定（或显式指定 key column）。
>
> 用途：等宽直方图、按类目求和、按时间桶聚合、箱线图四分位数（min/25/50/75/max）。
>
> 约束：单线程；不抛异常；空输入返回空结果。

- `struct DataView`
- `enum class Reducer : std::uint8_t`
- `struct Group`
- `struct MultiReduceResult`
- `struct HistogramBin`
- `struct Quartiles`

### `data/fill_missing.hpp`

> util/data/fill_missing.hpp —— 缺失值处理（NaN 视为缺失）
>
> NaN 判定：std::isnan(value) —— IEEE 754 静默 NaN / 信号 NaN 都算缺失。
> 三种策略：
> ForwardFill   ：沿用前一个非缺失值（适合时间序列）
> LinearInterp  ：相邻非缺失值线性插值（端点外用前/后端值延拓）
> Drop          ：返回新 vector 仅含非缺失值
>
> 输出：就地把目标 vector 改写（Fill 类）或返回新 vector（Drop）。

- `enum class FillStrategy : std::uint8_t`

### `data/ohlc.hpp`

> data/ohlc.hpp —— 时间轴重采样 → K 线 OHLC（L4）
>
> 按等宽时间桶把 (timestamp, value) 序列聚合成 K 线四价：open/high/low/close。
> 与 resample.hpp 的分工：那边按**值域**分桶（直方图式），这边按**时间轴**
> 分桶（金融序列式）——K 线图（chart/cartesian/candlestick）的数据入口。
>
> 约定：
> * timestamps 必须**非降序**（时间序列常态；io/csv 读入天然有序。
> 乱序输入行为未定义——调用方需先自行排序）；
> * 桶边界**锚定在每个桶的首个样本**（不规则采样下的合理口径，且单遍 O(N)）；
> * 空桶不产生 bar（顺序合并不感知未来样本，无法回填空桶）；
> * NaN 值的样本跳过（不参与任何价格）；
> * bucket_seconds ≤ 0 = 单桶（全量一根，用于"整段统计"）。
> * 单位为秒（Unix 时间戳口径）；调用方持有分钟/小时刻度自行换算。

- `struct OhlcBar`
- `struct OhlcOptions`

### `data/resample.hpp`

> util/data/resample.hpp —— 重采样（按规则桶聚合）
>
> 用途：把不规则间隔的时间序列按"每 N 个一桶"或"按显式桶边界"降采样，
> 输出每桶的聚合值（mean / sum / min / max / first / last）。
>
> 与 `aggregate::histogram` 的差异：resample 不要求等宽，
> 桶边界由 `BinEdges` 显式给出（等宽也只是其中一种生成方式）。

- `struct ResampleBin`

### `data/rolling.hpp`

> data/rolling.hpp —— 滚动窗口聚合
>
> 滚动统计（pandas Series.rolling(window).mean() 的最小集）。
> 中心约定：
> * 窗口长度 = window，**右对齐**：第 i 个输出由 [i-window+1, i] 决定；
> 前 i < window-1 个输出按 partial = true 处理（NaN 或 min_periods 触发）。
> * min_periods：窗口内最少非缺失值，少于则输出 NaN。
> * 支持：mean / sum / stddev / min / max / median。
> * 不抛异常；空 vector 返回空 vector。
>
> 另提供 EMA（指数移动平均）：O(N) 单遍递推，alpha = 2/(window+1)。
> rolling() 本身为 O(N·window)（窗口内每步重算）；纯 SMA 的 O(N) 滑动求和
> 作为已知优化点记入 backlog，刻意不在本文件展开。

- `enum class RollingStat : std::uint8_t`
- `struct RollingOptions`

## scene（L4）

### `scene/bounds.hpp`

> bounds.hpp —— Scene 的几何包围盒
>
> 为什么需要它：GUI 侧的 `render/scene_renderer` 要先知道"这份 Scene 有多大"才能
> 决定画布/无效区；终端侧也可以用它预分配画布。放在 scene/（L2）而不是 canvas/，
> 因为消费 Scene 的**每个**前端都需要它——不只终端那一条路径。
>
> 计算口径（刻意保守，宁可略大也不要漏）：
> * 线段取两端点；
> * 圆弧取"圆心 ± 半径"的正方形（不做最小包围盒）；
> * 文本按**显示宽度** × 1 行估算（宽字符算 2 列）；
> * 图片取它的矩形；
> * **不计描边宽度**：线宽会让实际墨迹略微外扩，本函数只保证几何上包得住。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `scene/builder.hpp`

> builder.hpp —— 流式构建辅助
>
> 存在的唯一理由：让"往 Scene 里追加图元"这件事读起来像绘图代码。
> 它**不持有** Scene（只持有引用），也不缓存当前路径——路径状态属于消费者。

- `class SceneBuilder`

### `scene/command.hpp`

> command.hpp —— Scene 的指令集（header-only）
>
> Scene 是**非完备图形 IR**：这里只放 vector 后端（SVG / LaTeX-TikZ）与
> canvas 都能消费的那部分图元。**不要往里塞 canvas 才有的能力**
> （例如渐变填充、裁剪路径、图层合成）——那些属于 canvas/（L10）。
>
> 指令一律用聚合体 + std::variant：
> * variant 的 index() 就是指令类别，不需要额外的 kind 字段；
> * 聚合体天然可 constexpr 构造、可 operator==，于是 Scene 可整体比较；
> * 新增指令 = 往 variant 里加一个聚合体，访问者会**编译期**被逼着补全。
>
> 本文件是 header-only（见 cmake/header_only.txt），不得有同名 src 实现。

- `struct MoveTo`
- `struct LineTo`
- `struct RectOp`
- `struct ArcOp`
- `struct FillOp`
- `struct StrokeOp`
- `struct TextOp`
- `struct ImageOp`
- `enum class CommandKind : std::uint8_t`

### `scene/scene.hpp`

> scene.hpp —— 绘制指令序列（矢量 IR）
>
> 三条渲染路径在它这里分岔：
> * 终端实时：Scene → canvas/scene_renderer → Canvas → to_buffer() → CellBuffer → renderer；
> * vector 导出：Scene → backend/vector/*（svg / latex-TikZ）；
> * Document：scene 被 document/block.hpp 的 Chart 块持有，走 structured 导出。
>
> 因此 Scene 只有两个承诺：**可顺序回放**与**可整体比较**（golden 与
> roundtrip 测试都依赖后者）。它不理解颜色、不做裁剪、不算布局。

- `class Scene`

### `scene/stateful_visitor.hpp`

> stateful_visitor.hpp —— 有状态 Scene 消费者基类
>
> 把"Fill/Stroke 是状态指令、路径需累积、Rect 有填充/描边分支"这套时序
> 收在一处。派生类只实现"往哪画"，不管"什么时候画"。
>
> 为什么需要它：canvas 的 ScenePainter 与 backend/vector 的 SVG 后端各自
> 写了一遍同一套状态机（MoveTo/LineTo 累积、遇到状态指令先按旧状态收口、
> Rect 按有无填充色分两支）。两处独立演化就会出现"同一份 Scene、两种画法"
> 的偏差；这里是那个偏差的唯一收敛点。
>
> 时序约定（与 svg_backend / scene_renderer 原行为一致）：
> * MoveTo     —— 更新 current_，不 flush（允许多子路径合成一条 <path>）；
> * LineTo     —— 首个 LineTo 触发 begin_path，其余 extend_path；
> * Fill/Stroke—— 先 end_path() 收口旧状态，再更新；重置 path_open_；
> * Rect/Arc/Text/Image —— 先 end_path() 收口，再 emit，并重置 path_open_；
> * run() 结束 —— 收尾一次。
>
> 两条容易写错的约束（写错就是静默错图，不是编译错误）：
> 1. end_path() 必须**无条件**调用，不能在基类里加 `if (path_open_)` 守卫：
> MoveTo 会把 path_open_ 置 false，但派生类累积的几何可能已经 dirty，
> 一守卫就漏掉这次收口。判空交给实现自己（d_.empty() / dirty 标志）。
> 2. end_path() 里读到的 fill_/stroke_ 是**旧值**——因为 FillOp/StrokeOp
> 的处理顺序是「先收口、后赋值」。这正是想要的：几何按它出现时的状态着色。

- `class StatefulVisitor : public CommandVisitor`

### `scene/visitor.hpp`

> visitor.hpp —— 指令访问者接口
>
> 为什么既给 std::visit 又给这套虚函数接口：
> * std::visit 适合"一次性、无状态"的消费（统计、序列化），零分配；
> * 虚函数访问者适合"有状态、可复用"的消费者（canvas 的光栅化器、
> SVG 后端），它们需要把状态挂在对象上而不是塞进 lambda 捕获。
>
> accept() 的实现放在 cpp：variant 的访问模板只实例化一次，不进头文件。

- `class CommandVisitor`
- `TERMCHART_API void accept(const Command& command, CommandVisitor& visitor)`
- `TERMCHART_API void accept_all(const Scene& scene, CommandVisitor& visitor)`

## image（L5）

### `image/bitmap.hpp`

> bitmap.hpp —— 彩色像素缓冲（宽 / 高 / 通道 / 像素数组 / 行对齐）
>
> 这是"图片"在库里的通用表示：protocol/（sixel/kitty/iterm2）与 image/png 都
> 面向它。通道数为 3（RGB）或 4（RGBA）；行按通道紧致对齐，row_bytes() 即步长。
> 标量强度场是另一种东西，见 image/field.hpp（raster 的输入）。
> 纯值类型，列入 header_only。

- `class Bitmap`

### `image/bmp.hpp`

> bmp.hpp —— BMP 解码（自主实现，零依赖；输出 RGBA）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 文件头 'BM' + BITMAPINFOHEADER（40 字节）与后续更长的 V4/V5 头；
> ✅ 位深 1 / 4 / 8（调色板，BI_RGB）、24（BGR）、32（BGRX 或 BITFIELDS）、
> 16（BITFIELDS 或默认 RGB555）；
> ✅ 压缩 0（BI_RGB）与 3（BI_BITFIELDS，带 3~4 个 DWORD 掩码）；
> ✅ 行序 bottom-up（height 为正）与 top-down（height 为负）；
> ✅ 行 4 字节对齐、调色板缺省色表（bpp ≤ 8 且 ciColors 为 0 时按 2^bpp 项）；
> ⛔ 不支持：RLE4/RLE8 压缩（返回 Unsupported）、OS/2 的 12 字节旧头
> （返回 Unsupported）、每通道超过 8 位的 16bpp 掩码组合（按能处理的
> 掩码解析，掩码不连续/不覆盖时返回 ParseError）。
> 错误：头结构非法/尺寸越界/掩码不可解析 → ParseError；魔数或压缩不支持 →
> Unsupported。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `image/convert.hpp`

> convert.hpp —— 位图与标量场的互转（GUI 与终端共用）
>
> 本文件是**规格清单之外的补充**，理由：GUI 侧的图片控件要把 RGBA 位图转成标量场
> 再喂给 raster/（盲文/密度预览），终端侧的图像预览走的也是同一条转换。把唯一实现
> 放在 image/ 里，比让每个上层各写一遍亮度公式可靠。
>
> 两点约定（刻意如此，别再"优化"）：
> * 亮度用 Rec. 601 系数（0.299 / 0.587 / 0.114），**不做 gamma 校正** ——
> GUI 与终端必须看到同一个亮度，加 gamma 只会让两边不一致且难以解释；
> * alpha 不参与亮度，而是**乘进结果**：全透明像素在场里就是 0，
> 不会因为"透明黑"而变成一块黑斑。
>
> 纯函数、无状态；列入 header-only 清单。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `image/decoder.hpp`

> decoder.hpp —— 图像解码入口（按魔数识别格式后分发）
>
> 为什么先"识别再分发"：调用方拿到的常常是一段来路不明的字节（剪贴板、文件、
> 网络），先嗅探格式再交给对应解码器，比让每个解码器各自试一遍要清楚。
>
> 目前 **PNG（自带 inflate）与 BMP（自带解析）是原生零依赖解码器**；JPEG / GIF /
> WebP 走 vendored `stb_image`（可选依赖 `TERMCHART_ENABLE_STB_IMAGE`，默认关，
> 关时如实 `Unsupported`）。所有"识别得出但无解码器"的情况都返回
> `ErrorCode::Unsupported`，不做"猜着解"。解码器注册表等到第四种格式落地再引入
> （与 raster/chart 同构），现在引入只会是空壳。

- `enum class ImageFormat : std::uint8_t`

### `image/field.hpp`

> field.hpp —— 光栅器消费的标量强度场（FloatField）
>
> raster/ 的所有光栅器都把"连续标量场"当成输入：场里每格是一个 0..1 的强度
> （密度 / 高度 / 命中次数归一化后的值）。它不属于"位图"（那是 image/bitmap.hpp
> 的彩色像素缓冲），因此单独成文件，避免一个 bitmap.hpp 既当像素又当标量。
> 纯值类型（无虚函数、无堆外状态），列入 header_only。

- `class FloatField`

### `image/png.hpp`

> png.hpp —— PNG 解码（8/16 位、非隔行；输出 RGBA）
>
> 与 png_encode.hpp 对称：编码器自带（零依赖），解码器也自带——同样是为了不把
> zlib 变成硬依赖。解码器包含一个完整的 **inflate**（stored / 固定 Huffman /
> 动态 Huffman 三种块），因此能读任意常规 PNG，而不只是本库自己写出的那种。
>
> 支持范围与刻意的限制（都在返回值里明确报错，不静默出错图）：
> * 位深：1 / 2 / 4 / 8 / 16；
> * 颜色类型：0 灰度、2 真彩、3 调色板、4 灰度+alpha、6 真彩+alpha；
> * tRNS：调色板透明与"单一透明色"（灰度/真彩）都处理；
> * **不支持隔行（Adam7）**：返回 `ErrorCode::Unsupported`；
> * 16 位样本**取高字节**降为 8 位（终端与 GUI 都用不到 16 位深度）；
> * 每个 chunk 的 CRC 都要对，错一个即返回 `ErrorCode::ParseError`。
> 输出恒为 4 通道 RGBA（调用方要 RGB 用 `Bitmap::without_alpha()`）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `image/png_encode.hpp`

> png_encode.hpp —— 最小 PNG 编码器（8 位 RGB / RGBA，非隔行）
>
> 为什么自带而不是用 libpng/zlib：本仓库的硬约束是"每个外部依赖都必须可缺失"
> （`TERMCHART_ENABLE_PNG` 默认 OFF）。这里只实现 PNG 必需的最小集合：
>
> * CRC-32（IEEE 802.3）—— 每个 chunk 的校验字段；
> * Adler-32 —— zlib 流尾校验；
> * zlib 流里的 **stored（未压缩）deflate 块** —— 不需要字典与霍夫曼编码，
> 体积比压缩版大约多 0.03%，但完全合规，任何解码器都能读。
>
> 将来接入真 deflate（或可选 zlib）时只换内部实现，公开接口不变。
> 代价说清楚：stored 块的输出体积 ≈ 原始像素 + 每 65535 字节 5 字节开销，
> 用于截图/内联图像足够，不适合当压缩格式用。

- `TERMCHART_API std::uint32_t crc32(const std::uint8_t* data, std::size_t len) noexcept`
- `TERMCHART_API std::uint32_t adler32(const std::uint8_t* data, std::size_t len) noexcept`
- `TERMCHART_API std::string encode_png(const Bitmap& bitmap)`

### `image/ttf_font.hpp`

> ttf_font.hpp —— 自主 TTF 解析（sfnt 目录 + cmap + glyf/loca + hmtx + 二次
> 贝塞尔轮廓光栅化 + glyph cache），输出 8-bit 灰度位图
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ sfnt 版本 0x00010000（TrueType）与 'true' 变体；'ttcf' 集合取第一个字体；
> ✅ cmap format 4（BMP，含代理对段）与 format 12（全 Unicode），平台子表
> 选择序：(3,10) > (3,1) > (0,x) > 任意 format 4/12；
> ✅ glyf 简单轮廓（on/off 曲线 + REPEAT 旗标 + 隐含中点）与复合轮廓
> （ARGS_ARE_XY_VALUES / 缩放矩阵 / 分量变换，深度上限 8，循环引用防护）；
> ✅ loca 短/长格式（head.indexToLocFormat）；hhea 行高度量；hmtx 步进宽；
> ✅ 光栅化：二次贝塞尔自适应展平 → 扫描线非零环绕 → 4x4 超采样 → alpha；
> ✅ glyph cache：按（码点, 0.25px 量化后的尺寸）缓存位图。
> ⛔ 不执行 hinting 字节码（指令区正确跳过但不执行）；不解析可变字体
> （fvar/gvar）；不解析位图字形表（EBDT/CBDT → 该字形回退 .notdef）；
> 不解析 OpenType CFF（'CFF ' 轮廓，无 'glyf' → load 即 Unsupported）。
> 错误：表缺失、目录/表体越界读取 → ParseError；'CFF ' 无 glyf → Unsupported；
> 复合轮廓过深或表长度声明溢出 → OutOfRange。

- `struct TtfMetrics`
- `struct TtfGlyph`
- `class TtfFont`

## raster（L6）

### `raster/ascii.hpp`

> ascii.hpp —— 密度字符 1×1 光栅器（最朴素、零依赖）
>
> 用 . : - = + * # % @ 把强度映射成由淡到浓的 ASCII 字符。适合纯文本环境、
> 不能显示块字符的终端，以及 golden 快照的可读对照。

- `class AsciiRasterizer final : public Rasterizer`

### `raster/auto_select.hpp`

> auto_select.hpp —— 按终端能力自动挑光栅器
>
> 注意：本文件**不**依赖 term/（term 是更高层），能力以扁平布尔传入，与
> TerminalCaps 解耦——这样光栅器层就能在不知晓终端对象的情况下完成选择。

- `struct RasterCaps`
- `TERMCHART_API RasterKind pick_raster(const RasterCaps& caps, RasterKind preferred = RasterKind::Braille) noexcept`

### `raster/block.hpp`

> block.hpp —— 八级竖条 1×1 光栅器
>
> 每格按平均强度取 1/8 高度的填充块（▁▂▃▄▅▆▇█）。分辨率最低但零子像素开销，
> 适合大色块、进度条、密度概览。invert 翻转强度（照片负片）。

- `class BlockRasterizer final : public Rasterizer`

### `raster/braille.hpp`

> braille.hpp —— 盲文 2×4 子像素光栅器（分辨率最高）
>
> 每个终端格 = 2 列 × 4 行的点阵，点亮的点拼成 U+2800 系盲文字形。子像素→点
> 位的几何约定全部在 detail/braille_table.hpp，本文件只做"采样 + 查表"。

- `class BrailleRasterizer final : public Rasterizer`

### `raster/halfblock.hpp`

> halfblock.hpp —— 半块 1×2 光栅器（上下双色）
>
> 一个格 = 上、下两半。▀ 上半亮、▄ 下半亮、█ 全亮、空格全灭。因为半块字符让
> 上/下两半分别取前景/背景，单格即可表现两种颜色——这是比盲文更"彩色"的选择。

- `class HalfBlockRasterizer final : public Rasterizer`

### `raster/quadrant.hpp`

> quadrant.hpp —— 象限块 2×2 光栅器
>
> 一个格 = 2×2 子像素。标准象限块字符（U+2596–U+259F）只覆盖 10 种图案，
> 缺 3 点亮的组合，所以那种情况改画"唯一的熄灭象限"为熄灭色、其余为点亮色——
> 视觉效果等同 3 点亮。几何与点位约定见 src/raster/quadrant.cpp 的映射表。

- `class QuadrantRasterizer final : public Rasterizer`

### `raster/rasterizer.hpp`

> rasterizer.hpp —— 子像素光栅化基类与公共契约
>
> 终端字符是低分辨率网格，但一个字符格能塞下多个"子像素"：盲文 2×4、半块 1×2、
> 象限 2×2。光栅器把一个标量场（image::FloatField）压进这种子像素网格，产出
> CellBuffer——同一份场，换光栅器就是换分辨率/风格，GUI 控件与终端后端复用同一套。
>
> 不变量：
> * rasterize 无副作用、不抛异常、不写 stdout；
> * 输出的 CellBuffer 尺寸 = field / (subpixel_w, subpixel_h)；
> * 单色光栅器：点亮处用 opts.fg，熄灭处用 opts.bg（invert 交换二者并反相）。

- `enum class RasterKind`
- `struct RasterOptions`
- `class Rasterizer`
- `TERMCHART_API const char* raster_name(RasterKind kind) noexcept`

### `raster/registry.hpp`

> registry.hpp —— 光栅器注册表（可扩展、构造期冻结）
>
> 用 function-local static 的 instance() 保证"构造前注册、构造后只读"的注册表
> 契约（见 docs/design/registry.md）。
>
> 两类来源：
> * 内置种类（Braille/HalfBlock/...）：硬编码在编译单元里，create(kind) 直接给；
> * 自定义种类：通过 register_raster(name, creator) 登记，create(name) 查表。
> 后者是**扩展点**——新增光栅器不必改本模块的枚举或 switch（见
> tests/integration/replace_raster_custom.cpp）。

- `class RasterRegistry`

### `raster/sextant.hpp`

> sextant.hpp —— 六分仪字符光栅器（2×3 子像素，Unicode 1FB00 区）
>
> 保真度介于象限(2×2=4) 与盲文(2×4=8) 之间，是终端字符图里"中等保真度"的一档。
> 每个字符格切成 2 列 × 3 行的 6 个 sextant 区域，按填充情况映射到 Unicode
> "Symbols for Legacy Computing" 的六分仪块（U+1FB00 起，码点 = 0x1FB00 + 6 位掩码；
> 位序约定：左上 0 / 左中 1 / 左下 2 / 右上 3 / 右中 4 / 右下 5）。

- `class SextantRasterizer final : public Rasterizer`

## network（L7）

### `network/http.hpp`

> http.hpp —— HTTP/1.1 客户端（network 模块，L7）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ http:// 明文与 https://（TLS 走 network/tls 注册表的后端，fail-loud）；
> ✅ 代理：显式 `HttpOptions::proxy`（"http://h:p" / "socks5://h:p" / 裸 "h:p"）
> 或**自动探测**（环境变量 HTTPS_PROXY/HTTP_PROXY/ALL_PROXY + NO_PROXY，
> Windows 注册表系统代理 / macOS SystemConfiguration；都没有则直连）；
> ✅ 请求：任意 method、自定义头、请求体（Content-Length 自动补）；
> ✅ 响应：状态行 + 响应头 + 响应体；体长按 Content-Length / chunked /
> 读到连接关闭三种规则解析（HEAD 与 204/304 按规范不收体）；
> ✅ 重定向：301/302/303/307/308 跟 Location（默认上限 5 次；303 降为 GET）；
> ✅ 每请求独立连接，发 `Connection: close`（库内不做连接池——图表库的
> 拉取场景一次一个 URL，池化是纯负担）；
> ⛔ 不支持：HTTP/2、Cookie 管理、摘要认证、请求体流式分块上传、
> 非 ASCII 头的原始字节透传（按 std::string 收）。
> 代理语义（分级 fail-loud）：
> * 显式配了代理却连不上/隧道建不起 → **报错**（不静默回落直连）；
> * 探测来的代理（环境变量/系统设置）不可用 → warn 后回落直连
> （代理是可选加速路径，直连是正确默认行为，curl/requests 同款行为）。
> 错误：连接失败 IoError；TLS 后端缺失/握手失败 Unsupported / IoError；
> 响应超限 OutOfRange；协议解析失败 ParseError。

- `struct Uri`
- `struct Request`
- `struct Response`
- `struct HttpOptions`

### `network/socket.hpp`

> socket.hpp —— 跨平台 TCP socket 抽象（network 模块，L7）
>
> 平台差异全部落在 src/network/socket_posix.cpp 与 socket_win32.cpp，
> 公开头**没有任何 #ifdef**（与 term/terminal.hpp 同哲学）。
>
> 契约：
> * 不抛异常：失败返回 Result（IoError / PlatformError / InvalidArgument）；
> * connect 超时由 poll/select **显式实现**（非阻塞 connect + 等待 + 恢复阻塞）
> ——SO_SNDTIMEO 在 Linux 上对 connect() 无效，不能靠它；
> * 单次读/写超时经 SO_RCVTIMEO / SO_SNDTIMEO 实现；
> * DNS 解析（域名 → 地址）在 connect 内完成（进程级 TTL 缓存，见
> src/network/dns_cache.cpp），调用方无需自己 getaddrinfo；
> * 移动语义（PIMPL），拷贝删除——一个 socket 只有一份底层句柄。

- `struct SocketOptions`
- `class TcpSocket`

### `network/tls/backend.hpp`

> backend.hpp —— TLS 后端抽象接口（network/tls，L7）
>
> 可插拔契约（与 raster/chart/scale 注册表同构，见 docs/design/registry.md）：
> * 内置后端（network/tls/builtin/，自主实现）与第三方后端（BearSSL/mbedTLS…）
> 实现同一接口，经 TlsRegistry 注册，调用方按名字取用；
> * 命名项缺失时**显式报错**（fail-loud），不静默降级——请求 "builtin" 而
> 未链接该后端即 Unsupported，绝不偷偷退回明文；
> * 后端只做"客户端 TLS 记录层"：握手 + 加解密读写 + close_notify。
> 证书链验证由后端负责（hostname 同时用于 SNI 与主机名校验）。

- `class TlsBackend`

### `network/tls/registry.hpp`

> registry.hpp —— TLS 后端注册表（可扩展、构造期冻结）
>
> 与 raster/registry.hpp 同构（docs/design/registry.md 的口径）：
> * instance() 一律 function-local static；
> * 构造后只读（拷贝/赋值删除）；注册的唯一安全时机是 App 构造前；
> * 注册的是**工厂**（每次 create 给新实例），不是原型；
> * 查询：supports(name) / create(name)；未知名字返回 nullptr，
> 由调用方显式报错（fail-loud，不静默降级——绝不偷偷退回明文）。

- `class TlsRegistry`

### `network/tls/schannel.hpp`

> schannel.hpp —— Windows 原生 TLS 后端（SChannel / Schannel SSP）
>
> 平台后端之一（另一支是 OpenSSL，随可选依赖编译）：
> * 实现 TlsBackend（握手 + 记录层加解密 + close_notify）；
> * 一个后端实例服务**一条连接**（握手掌状态 CtxtHandle 在实例里）——
> 每次 create() 拿新实例，勿跨连接复用；
> * 证书链与主机名**手动验证**（SCH_CRED_MANUAL_CRED_VALIDATION +
> CertGetCertificateChain / CertVerifyCertificateChainPolicy(SSL)），
> 失败即握手失败，绝不接受"链错了但握手过了"的连接；
> * 注册：registry.cpp 构造注册表时直接把本后端登记进去（显式引用
> create_backend，避免"静态库里无人引用的 TU 被链接器丢弃"陷阱）；
> register_backend() 留给需要显式控制注册时机的调用方。
>
> 已知环境限制：**已无**。2026-10-21 修复了此前被误判为"沙箱策略限制"的真 bug：
> MinGW-w64 的 schannel.h 结构体已是 V4（含 dwCredFormat），但 SCH_CRED_VERSION
> 宏仍停留在 0x00000002；SChannel 对 dwVersion 精确匹配，传 2 会被
> AcquireCredentialsHandleW 以 SEC_E_UNKNOWN_CREDENTIALS（0x8009030D）拒绝，
> 导致**所有** https 握手失败（不止沙箱——真机同样命中）。修复为显式传 4。
> 若未来再遇该错误码，先查 dwVersion，勿再归因于环境。

- `TERMCHART_API void register_backend() noexcept`

## term（L7）

### `term/ansi.hpp`

> ansi.hpp —— ANSI 序列常量与生成
>
> 边界（重要，别在这里加东西）：
> * 本文件只管 **光标/擦除/屏幕切换** 这类控制序列；
> * 颜色与属性序列（SGR）的唯一生成点是 render/sgr.hpp —— 收窄边已登记，
> 任何别的模块都不许自己拼 ESC[...m。
> 序列常量表见 detail/ansi_tables.hpp。

- `TERMCHART_API void append_cursor_to(std::string& out, int row, int col) noexcept`
- `TERMCHART_API void append_cursor_up(std::string& out, int n = 1) noexcept`
- `TERMCHART_API void append_cursor_down(std::string& out, int n = 1) noexcept`
- `TERMCHART_API void append_cursor_forward(std::string& out, int n = 1) noexcept`
- `TERMCHART_API void append_cursor_back(std::string& out, int n = 1) noexcept`
- `TERMCHART_API void append_cursor_home(std::string& out) noexcept`
- `TERMCHART_API std::string cursor_to(int row, int col) noexcept`
- `TERMCHART_API std::string cursor_home() noexcept`
- `TERMCHART_API void append_erase_to_end_of_line(std::string& out) noexcept`
- `TERMCHART_API void append_erase_line(std::string& out) noexcept`
- `TERMCHART_API void append_erase_display(std::string& out) noexcept`
- `TERMCHART_API std::string erase_line() noexcept`
- `TERMCHART_API std::string hide_cursor() noexcept`
- `TERMCHART_API std::string show_cursor() noexcept`
- `TERMCHART_API std::string enter_alt_screen() noexcept`
- `TERMCHART_API std::string exit_alt_screen() noexcept`
- `TERMCHART_API void append_move(std::string& out, int from_row, int from_col, int to_row, int to_col) noexcept`

### `term/capability_query.hpp`

> capability_query.hpp —— DA1 / DA2 主动探测
>
> 环境变量（见 term/caps.hpp）只能"猜"，DA 是终端**自报**的能力，二者互补：
> DA1  ESC [ c     → 终端回 ESC [ ? Ps ; … c
> DA2  ESC [ > c   → 终端回 ESC [ > Ps ; … c
>
> 本模块只做两件事：生成查询序列、解析回复。真正的读写终端由 term/terminal 或
> app/ 负责——保持这里无 I/O，单测才能确定性地验证解析（含畸形输入）。

- `struct DeviceAttributes`
- `struct Da1Features`

### `term/caps.hpp`

> caps.hpp —— 终端能力探测
>
> 优先级规则见 docs/reference/terminal_capabilities.md：
> 显式环境变量 > TERM 名称 > 默认保守值。
> **纯函数**：能力探测不读写终端、不发查询序列，输入是环境变量快照 + 是否 tty。
> 因此 CI / 单元测试可以把环境变量塞进来，完全可复现。

- `enum class ImageProtocolKind : std::uint8_t`
- `struct CapEnvironment`
- `struct TerminalCaps`
- `TERMCHART_API TerminalCaps detect(const CapEnvironment& env, bool tty) noexcept`
- `TERMCHART_API TerminalCaps minimal() noexcept`
- `TERMCHART_API ColorDepth color_depth_for(const CapEnvironment& env) noexcept`
- `TERMCHART_API ImageProtocolKind image_protocol_for(const CapEnvironment& env) noexcept`
- `TERMCHART_API bool unicode_for(const CapEnvironment& env) noexcept`
- `TERMCHART_API const char* image_protocol_name(ImageProtocolKind kind) noexcept`

### `term/clipboard.hpp`

> clipboard.hpp —— OSC 52 剪贴板
>
> 写入： ESC ] 52 ; <target> ; <base64 文本> ST
> 读取： 同样发一条，但 base64 为空，终端会**回包**同样形状的序列
>
> 安全提醒：OSC 52 是否生效由终端决定——很多终端默认关闭写入，也有终端只允许
> 读取。因此本模块只生成序列，不假设它一定成功；调用方不应把剪贴板当作可靠存储。

- `enum class ClipboardTarget : char`

### `term/color_fallback.hpp`

> color_fallback.hpp —— 颜色降级（TerminalCaps → 实际可用的颜色深度）
>
> 降级**算法只有一份**，在 color/quantize（L1）：真彩 → 256 色 → 16 色 → 无色的
> 量化与最邻近匹配都在那里。本文件是 term 层的桥接：从 TerminalCaps 取出实际可用
> 的 ColorDepth，再转发给 quantize::degrade，避免"每个调用点自己判断一遍"。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `term/daemon_safe.hpp`

> daemon_safe.hpp —— 非 tty 安全模式
>
> 场景：库被 cron / systemd / CI / 管道用。此时若照常发 ANSI 序列，
> 日志里会留下 \x1b[2J 之类的噪声，而用户只会反馈"输出脏了"。
>
> 策略（与本文件同名的 docs 节点保持一致）：
> * 非 tty → 不发颜色、不进交替屏幕、不隐藏光标、不进原始模式；
> * CI 明确要求"可预期输出"时，进一步把颜色档位打到 None。

- `struct DaemonSafeSettings`
- `TERMCHART_API DaemonSafeSettings daemon_safe_for(bool tty, bool ci) noexcept`
- `TERMCHART_API TerminalCaps apply(const TerminalCaps& caps, const DaemonSafeSettings& safe) noexcept`
- `TERMCHART_API TerminalOptions apply(const TerminalOptions& options, const DaemonSafeSettings& safe) noexcept`

### `term/hyperlink.hpp`

> hyperlink.hpp —— OSC 8 超链接
>
> 序列形状（OSC 8 用 ST `ESC \` 结束，不用 BEL）：
> 打开： ESC ] 8 ; [id=<id>;]<uri> ST
> 文本
> 关闭： ESC ] 8 ; ; ST
>
> 支持的范围很广（iTerm2 / kitty / WezTerm / Windows Terminal / VTE 系），
> 不支持的终端会**原样显示** URI 与文本——因此 hyperlink() 只在明确需要时才用。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `term/mouse_mode.hpp`

> mouse_mode.hpp —— 鼠标上报开关（DECSET / DECRST）
>
> 只生成序列，不写 stdout、不改自身状态：调用方（term/terminal 或 app/）决定何时
> 打开、何时关闭，并负责在异常退出时复位（见 term/terminal 的 RAII 门面）。
>
> 模式与坐标编码是**两组独立开关**，必须分别设置：
> * 模式（9 / 1000 / 1002 / 1003）决定"什么时候上报"；
> * 编码（1005 / 1006 / 1015）决定"怎么编码坐标"。
> 默认编码是 X10 风格，坐标上限 223 列且有歧义，所以实际使用一律建议配 SGR(1006)。

- `enum class MouseMode : std::uint16_t`
- `enum class MouseEncoding : std::uint16_t`

### `term/raw_mode.hpp`

> raw_mode.hpp —— 原始模式 RAII
>
> 关键约束：**即使抛异常也必须恢复终端**。因此这里只提供 RAII 一种用法，
> 不暴露裸 enable/disable 给用户——"忘记恢复"是这类 bug 的一半来源。
> 平台差异全部在 src/term/terminal_posix.cpp / terminal_win32.cpp，头文件无 #ifdef。

- `class RawMode`

### `term/signal.hpp`

> signal.hpp —— SIGWINCH 转发给主线程
>
> 铁律：**信号处理器里只做两件事**——置 atomic 标志 + 唤醒 waker。
> 其余（重新布局、重绘、QString 那样的任何分配）一律带回主线程做。
> 违反这条的后果是" resize 时偶发死锁/堆损坏"，且不可复现。
>
> 平台差异：Windows 没有 SIGWINCH（靠 ReadConsoleInput 的 WINDOW_BUFFER_SIZE_EVENT），
> 因此本头文件导出的是"安装/卸载 resize 转发"的**抽象动作**而非具体信号。

- `TERMCHART_API bool install_resize_forwarding(Waker& waker) noexcept`
- `TERMCHART_API void uninstall_resize_forwarding() noexcept`
- `TERMCHART_API bool take_resize_flag() noexcept`
- `TERMCHART_API void simulate_resize_for_test() noexcept`

### `term/size.hpp`

> size.hpp —— 终端尺寸查询与 SIGWINCH
>
> 尺寸是"终端说了算"的外部状态：任何时候都可能变（用户拉窗口）。
> 这里只提供**查询**；真正的循环怎么响应 resize 在 app/loop.hpp，
> 信号转发走 term/signal.hpp + term/waker.hpp（绝不进 signal handler 干活）。

- `struct TerminalSize`
- `TERMCHART_API TerminalSize query_size() noexcept`
- `TERMCHART_API TerminalSize size_from_env() noexcept`

### `term/terminal.hpp`

> terminal.hpp —— 终端 RAII 门面
>
> 唯一"拥有"终端状态的地方：
> * 构造时不改动任何东西，调用 enter() 才真正进入；
> * 析构（含异常栈展开）必定**按相反顺序**撤销 enter() 做过的每一件事；
> * daemon_safe.hpp 负责"非 tty 时退行为"。
>
> 平台差异全部落在 src/term/terminal_posix.cpp 与 terminal_win32.cpp：
> **本头文件没有任何 #ifdef**（违反这条就是架构泄漏）。
>
> 返回值约定：只回报"成功与否"的操作返回 Status（即 ErrorCode）；
> 需要携带值的才用 Result<T>（见 core/expected.hpp）。

- `struct TerminalOptions`
- `class Terminal`

### `term/waker.hpp`

> waker.hpp —— 跨线程唤醒原语
>
> 并发模型（docs/design/concurrency.md）：
> 单线程主循环 + select/poll。后台线程**不得**触碰任何绘制对象，
> 只能通过 app/post.hpp 投递数据，然后 notify() 唤醒主线程。
> POSIX 用 self-pipe，Windows 用手动重置 event —— 差异封在实现里，头文件无 #ifdef。

- `class Waker`

## document（L8）

### `document/block.hpp`

> block.hpp —— Document 的内容块（语义单元）
>
> 一个 Block 是 Paragraph / Heading / Code / Table / Chart 之一。Chart 块持有
> Scene（矢量 IR）——于是"图表"作为文档元素不需要 backend 认识 chart 类型，
> 这正是 backend > chart 禁止边成立的前提。

- `enum class BlockKind`
- `class Block`

### `document/document.hpp`

> document.hpp —— Document：内容块容器（structured 物种的输入）
>
> Document 是 backend/structured 的输入：它只表达语义（段落/标题/表格/图表），
> 不含任何渲染或像素决策。`to_plain_text()` 是给测试与 text 导出的最小序列化，
> 真正的 HTML/Markdown 由 backend/structured/* 负责。

- `class Document`

### `document/span.hpp`

> span.hpp —— 带样式的文本片段（Document IR 的叶）
>
> Span 是"语义文本 + 样式"，不含像素/位置信息：位置由后端或上层决定。
> header-only 值类型。

- `struct Span`

### `document/table_model.hpp`

> table_model.hpp —— 表格的语义模型（行列字符串，非像素）
>
> backend/structured 负责把它排版成 HTML/Markdown；本层只表达"表头 + 若干行"。
> header-only 值类型。

- `class TableModel`

## protocol（L9）

### `protocol/detect.hpp`

> detect.hpp —— 从环境变量探测终端支持的图像协议

- `TERMCHART_API ProtocolCaps detect_protocol_caps() noexcept`

### `protocol/fallback.hpp`

> fallback.hpp —— 图像协议降级：能力 → ProtocolKind
>
> 与 raster/auto_select 同一套思路：能力以扁平布尔传入，协议层不依赖 term/。

- `struct ProtocolCaps`
- `TERMCHART_API ProtocolKind pick_protocol(const ProtocolCaps& caps) noexcept`

### `protocol/halfblock_image.hpp`

> halfblock_image.hpp —— 真彩半块字符图像（任何 24-bit 终端可用的降级）
>
> 每格塞 2 个像素：上像素当前景、下像素当背景，字符恒为 ▀（U+2580）。
> 于是 1 格 = 1×2 像素，观感接近位图，且不需要任何图形协议。

- `class HalfBlockProtocol final : public Protocol`

### `protocol/iterm2_inline.hpp`

> iterm2_inline.hpp —— iTerm2 内联图像协议（OSC 1337）
>
> iTerm2（及其兼容终端 WezTerm 等）只认 **PNG** 负载：原始 RGBA 不会被识别，
> 因此本编码器建立在内置的 image/png_encode 之上。
>
> 序列形状（BEL 结束）：
> ESC ] 1337 ; File = inline=1;size=<PNG字节数>;width=<w>px;height=<h>px : <base64> BEL
>
> 与 sixel/kitty 一致，编码结果不含光标定位——放哪儿由调用方决定。

- `class Iterm2Inline final : public Protocol`

### `protocol/kitty_graphics.hpp`

> kitty_graphics.hpp —— Kitty 图形协议（ESC_G，原始 RGBA/RGB + base64 分块）

- `class KittyProtocol final : public Protocol`

### `protocol/protocol.hpp`

> protocol.hpp —— 终端图形协议抽象基类
>
> protocol/ 只负责一件事：把 image::Bitmap **编码成可写入终端的字节流**。
> 它不做布局、不移动光标、不写 stdout——定位由调用方决定。于是同一张位图可以
> 用 sixel / kitty / 半块字符三种方式表达，降级链见 fallback.hpp。
>
> 注：iTerm2 内联图像只认 PNG 负载（原始 RGBA 不被识别），其编码器
> （protocol/iterm2_inline.hpp）建立在内置的 image/png_encode 之上。

- `enum class ProtocolKind`
- `struct ProtocolOptions`
- `class Protocol`
- `TERMCHART_API const char* protocol_name(ProtocolKind kind) noexcept`

### `protocol/sixel.hpp`

> sixel.hpp —— DEC sixel 编码器（DCS 0;0;0q … ST）

- `class SixelProtocol final : public Protocol`

## canvas（L10）

### `canvas/canvas.hpp`

> canvas.hpp —— 浮点坐标画布 → CellBuffer
>
> 终端实时路径的中段：
> Scene →（scene_renderer）→ Canvas → to_buffer() → CellBuffer → render/renderer
>
> 与 raster/ 的分工（重要）：
> * 本文件工作在**格**粒度：一个浮点坐标落到哪一格，就写那一格；
> * 子像素（一格内 2×4 盲文点、半块上下双色）是 raster/ 的事——阶段 7 接入后
> draw_line / fill_rect 会转而调用光栅器，本文件的对外接口**不变**。
> 因此这里的图元字符是"占位级"的（线用 '*'、填充用 █），不是最终观感。

- `class Canvas`

### `canvas/clip.hpp`

> clip.hpp —— 裁剪区域（矩形与凸多边形）
>
> 两种区域：
> * 矩形：最常用。线段裁剪走精确的 Liang–Barsky；多边形裁剪走 4 个半平面。
> * **凸**多边形：逐点判定 + Sutherland–Hodgman 逐边裁剪。
>
> 非凸多边形**只支持逐点判定**：多边形裁剪会原样返回，且 supports_polygon_clip()
> 报 false。这是刻意的——Sutherland–Hodgman 对非凸裁剪区会给出看似合理实则错误
> 的结果，宁可在接口上明说，也不让调用方拿到错的形状。

- `class ClipRegion`

### `canvas/fill.hpp`

> fill.hpp —— 多边形扫描线填充（含纵向超采样）
>
> 扫描线思路：按**格行**推进，对每条采样线求它与多边形各边的交点，排序后按
> 奇偶规则配对成若干半开区间 [x0, x1)，区间内的格才点亮。
>
> 为什么还要超采样：字符格没有灰度，边界格"点亮与否"直接决定图形边缘的观感。
> supersample = n 时每格纵向取 n 条采样线，横向用**精确重叠长度**算覆盖比例，
> 覆盖率 >= coverage 才写入 —— 边界格因此会按实际覆盖面积决定取舍，
> 比"格中心在内就点亮"稳得多（细长三角形不会被画丢）。

- `struct Span`
- `struct FillOptions`

### `canvas/layer.hpp`

> layer.hpp —— 图层合成
>
> 终端没有 alpha 通道，但有三种**真实可用**的合成语义：
> * Overlay：只写"非空白"格（空白处露出下层）—— 等价于透明，最常用；
> * Replace：连空白格一起覆盖（"这块区域归我"）；
> * Tint   ：保留字符、统一换成给定样式（高亮 / 遮罩 / 变暗）。
>
> 空白的口径：`is_blank()`（空格且无样式）。**带样式的空格会被当作内容写入**，
> 这正是"铺底色"的用法；不想铺就用默认样式留空格。
>
> 另外提供最小重绘支持：mark_clean() 记快照，dirty_bounds() 给出与快照的差异包围盒。

- `class Layer`

### `canvas/path.hpp`

> path.hpp —— 路径对象：构建 → 展平 → 描边/填充
>
> 路径只描述几何，不绑定样式与画布：先 add_* 建好，再交给 stroke()/fill()。
> 曲线（二次/三次贝塞尔、圆）统一用**自适应细分**展平成折线，容差以格为单位
> （默认 0.25 格）：容差越小越贴合，折线也越长。
>
> 圆用四段三次贝塞尔（kappa = 0.5523）表示，而不是固定分段数的多边形 ——
> 这样细分的密度由容差决定，放大缩小时都不会出现肉眼可见的多边形棱角。

- `enum class PathVerb : std::uint8_t`
- `struct PathCommand`
- `class Path`

### `canvas/primitives.hpp`

> primitives.hpp —— 图元离散化（浮点 → 格坐标序列）
>
> 纯函数层：**只产出"要写哪些格"**，不碰 Canvas，也不决定用什么字符。
> 于是光栅器（raster/）将来可以在同一份离散结果上换字符，而图元算法不变。

- `TERMCHART_API std::vector<Point> line_points(Point from, Point to) noexcept`
- `TERMCHART_API std::vector<Point> rect_outline_points(Rect rect) noexcept`
- `TERMCHART_API std::vector<Point> circle_points(Point center, double radius) noexcept`
- `TERMCHART_API std::vector<Point> arc_points(Point center, double radius, double start, double end) noexcept`

### `canvas/scene_renderer.hpp`

> scene_renderer.hpp —— Scene → Canvas，**终端路径上的唯一桥接点**
>
> 为什么只有这一处能解释 Scene 指令：
> * vector 后端（backend/vector/*）直接消费 Scene，不经过 Canvas；
> * 终端路径必须先把矢量指令离散成格，这一层转换只应存在一份。
> 若第二个文件开始 switch Command 的类别，两份实现对同一 Scene 的解释迟早分叉。

- `TERMCHART_API void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept`

### `canvas/transform.hpp`

> transform.hpp —— 2D 仿射变换（含终端字符的宽高比补偿）
>
> 约定：列向量，p' = M · [x y 1]^T，元素按行主书写
> | m00 m01 m02 |
> | m10 m11 m12 |
> |  0   0   1  |
> `then(next)` 表示"先做 this、再做 next"（等价于矩阵乘法 next·this），
> 因此链式写法与阅读顺序一致：T.translation(...).then(T.rotation(...)) 先平移后旋转。
>
> 宽高比补偿：终端字符格不是正方形，宽:高通常约 1:2。几何上的正圆直接落格会显扁，
> 所以画圆/放射布局时先用 aspect_correction() 把 y 方向压缩 1/aspect，
> 渲染出来才接近正圆。这也是本模块区别于通用线性代数的唯一一点。

- `class Transform`

## layout（L11）

### `layout/annotation.hpp`

> annotation.hpp —— 参考线、区域高亮、标注
>
> 三者都是"叠在数据之上"的辅助标记，共用 `PlotArea` 的数据 ↔ 格映射，并且一律
> **裁剪在绘图区内**（不会画到轴标签上，也不会溢出画布）。
>
> 区域高亮的语义刻意做成"只改背景色"：终端里"高亮一个区间"应当是让原有数据
> 换底色，而不是把数据涂掉。需要填充字符时才用 fill_char。

- `class Canvas`
- `enum class Orientation`
- `struct ReferenceLine`
- `struct HighlightBand`
- `struct Annotation`

### `layout/auto_margin.hpp`

> auto_margin.hpp —— 由内容反推四边留白
>
> 终端图表的留白不是美学问题，而是"标签放得下吗"的问题：y 刻度标签占左边、
> x 刻度标签占下边、标题占上边、图例占右边。把这几项**显式建模**成输入再算，
> 比在 chart 里散落一堆硬编码数字（然后每次换数据就压字）可靠得多。
>
> 口径：所有留白**向上取整到整格**（字符网格上没有半格），并保证每边至少为 padding。

- `struct MarginInputs`

### `layout/axis.hpp`

> axis.hpp —— 坐标轴绘制（左 y 轴 / 下 x 轴）：轴体 + 刻度 + 数字标签 + 轴名

- `class Canvas`

### `layout/box.hpp`

> box.hpp —— 绘图区（PlotArea）：画布留白 + 数据↔格 比例尺
>
> 一个 PlotArea 把"画布尺寸 + 四边留白 + 数据域"固化成两个 LinearScale：
> * x_scale_：数据 x → [inner.left, inner.right]
> * y_scale_：数据 y → [inner.bottom, inner.top]（注意 y 反转，格坐标向下增长）
> 于是 chart 只需 cell_of(x,y) 就能把任意数据点落到正确的格。

- `class PlotArea`

### `layout/facet.hpp`

> facet.hpp —— 纯几何的矩形划分（facet 布局）
>
> 只做一件事：把一块矩形按 rows × cols 切成带间距的小矩形。**不建图、不画图**，
> 也不认识 chart —— 调用方拿到 Rect 列表后自己决定每格画什么（这正是分面图与
> 仪表盘拼版需要的原语，也让本文件可以被 GUI/布局层无条件复用）。
>
> 两个约定：
> * 行优先：索引 row * cols + col，facet(0,0) 在左上角；
> * 尺寸除不尽时，余数按"从左到右、从上到下"逐格 +1 分配，各格尺寸最多相差
> 一格；分配后宽度或高度不足 1 格的格子返回空矩形（w 或 h 为 0），
> 由调用方跳过 —— 不产生"看起来有其实画不下"的格子。

- `struct FacetOptions`

### `layout/grid.hpp`

> grid.hpp —— 网格线（虚线），在刻度位置铺背景参照

- `class Canvas`

### `layout/label.hpp`

> label.hpp —— 轴名等文本标签

- `class Canvas`

### `layout/label_avoid.hpp`

> label_avoid.hpp —— 标签避让（通用实现，位于 layout 层）
>
> 为什么在 layout 而不是 graph：图模块（L13）与图表模块（L12）都要用同一套避让
> 规则，而它们只能向下依赖。把唯一实现放在 layout，上层转引即可 —— 否则同一条
> "候选位顺序 + 不相交 + 不越界"的规则会各写一份，迟早分叉。
>
> 规则（与既有 graph 侧行为完全一致，gap 默认 1 即"紧贴锚点外侧一格"）：
> * 候选顺序：右、左、上、下；
> * 候选位必须不与 occupied 中任何矩形相交（半开区间，共边不算相交），
> 且不越出左/上边界与可选的整体 bounds；
> * 全部不可用时返回首选位并置 placed = false —— 调用方应**放弃绘制**，
> 而不是硬画上去覆盖别人的内容。

- `struct LabelBox`
- `struct LabelAvoidOptions`
- `struct LabelSlot`

### `layout/layout.hpp`

> layout.hpp —— 图表布局公共类型与工具
>
> layout/ 是 stage 3 的几何层：在 canvas（格粒度画布）之上做"轴/网格/图例/标题"
> 的划分与绘制，本身不碰渲染管线（render 层更高，chart 不依赖 render）。所有
> 绘制函数直接作用于 canvas::Canvas，与未来的 GUI 平行栈共用同一套几何。

- `struct Margin`
- `enum class Side`
- `enum class HAlign`
- `enum class VAlign`
- `TERMCHART_API std::string format_tick(double value) noexcept`

### `layout/legend.hpp`

> legend.hpp —— 图例：色块 + 名称，水平排布在给定矩形内

- `class Canvas`
- `struct LegendEntry`

### `layout/title.hpp`

> title.hpp —— 标题：在矩形顶部居中（或按对齐）绘制

- `class Canvas`

## chart（L12）

### `chart/cartesian/area.hpp`

> area.hpp —— 笛卡尔面积图
>
> 与折线图同构：网格 → 坐标轴+刻度 → 折线轮廓 + 向 baseline 的逐列填充 →
> 图例 → 标题。填充按"列"做（每列求出曲线高度，填 [曲线, 基线) 之间的格），
> 与格粒度画布天然对齐，也直接复用 canvas 的既有原语，不自写扫描线。
>
> 单系列：面积图的堆叠/百分比/流图变体是后续扩展（见 ROADMAP），这里只做
> 最基础的"折线到基线"一档。

- `class AreaChart final : public Chart`

### `chart/cartesian/bar_chart.hpp`

> bar_chart.hpp —— 笛卡尔柱状图（单系列，垂直柱）
>
> 与 LineChart 同构：网格 → 轴 → 从基线起按值填充竖条（█）→ 图例 → 标题。
> 分组/堆叠在多系列场景才需要，作为后续扩展（见 ROADMAP）。

- `class BarChart final : public Chart`

### `chart/cartesian/boxplot.hpp`

> boxplot.hpp —— 箱线图：五数概括 + 须 + 离群点
>
> 终端字符化策略：箱体（q1–q3）画成 █ 实心块，中位线用 ─ 覆盖箱体中行，
> 上下须是 │ 竖线、须端帽是 ─ 短横，离群点用 ○ 单字符。输入直接接受
> 五数概括（min/q1/med/q3/max），不在 chart 层重复实现分位数算法；
> 违反 lo≤q1≤med≤q3≤hi 的组整体剔除。

- `class BoxPlotChart final : public Chart`

### `chart/cartesian/bubble.hpp`

> bubble.hpp —— 气泡图：散点 + 第三维（半径）
>
> 终端字符化策略：终端格没有"面积"可缩放，第三维用**点阵符号的大小阶梯**
> 表达——r 归一化到 [rmin, rmax] 后取 · → • → ● 三档，越大越显眼；
> 全体同值时取中档（此时第三维不携带信息）。

- `class BubbleChart final : public Chart`

### `chart/cartesian/bullet.hpp`

> bullet.hpp —— 子弹图：每行一条水平刻度条，定性背景带 + 实心度量条 + 目标竖线。
>
> 终端字符化策略：背景带用 ░▒▓ 三档明暗区分定性区间，度量值用 █ 实心条，目标用 │ 竖线，
> 全部信息靠字符明暗与位置表达，不依赖颜色。

- `class BulletChart final : public Chart`

### `chart/cartesian/candlestick.hpp`

> candlestick.hpp —— K 线图：OHLC → 实体 + 影线
>
> 终端字符化策略：开盘-收盘区间画成 █ 实体块（涨/跌分色），最高-最低画成
> │ 影线（先画影线再画实体，实体自然覆盖中段）；开盘=收盘（十字星）时实体
> 退化为单行 ─。x 轴是 K 线序号（第 i 根），不是连续数值。

- `class CandlestickChart final : public Chart`

### `chart/cartesian/contour.hpp`

> contour.hpp —— 等高线：标量场 → marching squares
>
> 终端字符化策略：对每条等值线跑 marching squares 提取折线段，段画成 · 连线
> （draw_line 自带裁剪与取整）。场按**行**给定且 row 0 = 最上 = ymax；等值线
> 级数缺省自动取值域内均分（不含端点），也可显式指定。

- `class ContourChart final : public Chart`

### `chart/cartesian/dumbbell.hpp`

> dumbbell.hpp —— 哑铃图：每行一个类别，同一行内两个数值之间用"线段 + 两端实心点"呈现差异。
>
> 终端字符化策略：行内差异用 • 连线、● 标两端值，一行一类别，不依赖颜色也能读出差距。

- `class DumbbellChart final : public Chart`

### `chart/cartesian/errorbar.hpp`

> errorbar.hpp —— 误差棒：中心点 + 不对称误差区间
>
> 终端字符化策略：数据点画 ●，y 误差区间是 │ 竖线 + ─ 端帽，x 误差区间是
> ─ 横线 + │ 端帽；误差上下界支持不对称（lo/hi 独立给定，lo>hi 自动交换）。

- `class ErrorBarChart final : public Chart`

### `chart/cartesian/gantt.hpp`

> gantt.hpp —— 甘特图：任务时间条 + 行标签
>
> 终端字符化策略：每个任务占一行，时间条是按 [start, end] 落在时间轴上的
> █ 条带（行内垂直居中）；行标签写在左侧留白里（按码点截断，不撕裂 UTF-8），
> 行间只画垂直时间网格线（横向网格线对任务行没有意义）。

- `class GanttChart final : public Chart`

### `chart/cartesian/heatmap.hpp`

> heatmap.hpp —— 笛卡尔热力图（二维点集 → 网格 → 计数 → 连续色表着色）
>
> 与"矩阵热力图"不同：输入是**散点坐标**而非规整矩阵，因此要先落到一个网格
> （由数据范围与单元边长决定），统计每格点数，再把计数归一化后用连续色表上色。
> 密度同时由"字符"与"颜色"两条通道编码——单一字符表达不了连续量，单一颜色在
> 低对比终端上也可能不可读；两者叠加才既可见又可比。
>
> 网格**先算后画**：先把所有点落格得到计数矩阵（索引一律 clamp，绝不出现负索引
> 折回），再把非空格画进画布、并对每个格矩形与内框求交做边界裁剪。

- `class HeatmapChart final : public Chart`

### `chart/cartesian/hist.hpp`

> hist.hpp —— 笛卡尔直方图（原始样本 → 自动分箱 → 柱）
>
> 与柱状图同构，但输入是**未聚合的原始样本**：先自动分箱并统计每箱频数，再按箱
> 画柱。分箱规则内建而非由调用方给定，是因为"箱数该随样本量增长"本身就是直方图
> 的语义（样本越多、分辨率应越细）；调用方仍可用 set_bins 覆盖。分箱结果通过
> bin_edges()/bin_counts() 暴露，便于测试与上层复用（例如叠加密度曲线）。

- `class HistChart final : public Chart`

### `chart/cartesian/line_chart.hpp`

> line_chart.hpp —— 笛卡尔折线图
>
> 把若干 Series 画进 Canvas：网格 → 坐标轴+刻度 → 折线（含数据点标记）→
> 图例 → 标题。域可自动按数据范围 + 5% 留白 + nice 对齐（scale::nice_domain）。

- `class LineChart final : public Chart`

### `chart/cartesian/mosaic.hpp`

> mosaic.hpp —— 马赛克（marimekko）：列宽∝第一变量边际、列内高度∝第二变量占比。
>
> 终端字符化策略：列内自下而上按行类别堆叠 █ 色块，列边界画 │ 分隔，
> 边际与条件分布同时靠"宽"与"高"两条几何通道编码，无需坐标轴即可比较。

- `class MosaicChart final : public Chart`

### `chart/cartesian/parallel.hpp`

> parallel.hpp —— 平行坐标：每个维度一条竖轴均分画布宽，每条观测跨轴连成折线。
>
> 终端字符化策略：各维独立 min-max 归一化到竖轴后逐段画 • 折线（颜色按观测轮换），
> 高维相关模式在二维终端上靠"线的走向"直接可读。

- `class ParallelChart final : public Chart`

### `chart/cartesian/qq.hpp`

> qq.hpp —— QQ 图：样本分位数对理论正态分位数逐点落格，附参考线判断正态性。
>
> 终端字符化策略：x 轴为理论正态分位数（Acklam 逆逼近），y 轴为排序样本值，
> 点用 ●，参考线 y=μ+σx 用 •；点贴线即正态，弯曲形态即偏差方向。

- `class QQChart final : public Chart`

### `chart/cartesian/ridgeline.hpp`

> ridgeline.hpp —— 山脊图：多组分布按行堆叠，密度轮廓用块条填充，行间部分重叠成"山脊"。
>
> 终端字符化策略：每组密度归一化后按列填充实心块（顶部分数行用下八分块），
> 后画的行覆盖先画的，天然形成前后遮挡的山脊观感。

- `class RidgelineChart final : public Chart`

### `chart/cartesian/scatter_chart.hpp`

> scatter_chart.hpp —— 散点图（可选气泡权重）
>
> 与折线图的唯一区别：**不连线**，每个点独立标记。点可以带"权重"（气泡图）：
> 权重被归一化后映射到四档标记字符（· ○ ● ◉），而不是浮点半径 —— 字符网格上
> 画不出真正的半径，硬做只会得到看起来随机的大小。

- `class ScatterChart final : public Chart`

### `chart/cartesian/sparkline.hpp`

> sparkline.hpp —— 迷你图：单行趋势线，逐列取下八分块 ▁▂▃…█。
>
> 终端字符化策略：min-max 归一化到 0..8 后逐列取下八分块字符，一行即成图，
> 可直接嵌进状态栏或表格行；末列可选 ● 标出最新值。

- `class SparklineChart final : public Chart`

### `chart/cartesian/step.hpp`

> step.hpp —— 笛卡尔阶梯图
>
> 与折线图同构，唯一区别是相邻点之间不直连，而走"横—竖—横"台阶：数据在采样
> 区间内保持某一档（前一档/后一档/中点换档），而不是线性过渡。适合表达离散
> 状态、累计量台阶、保持型信号等。三档语义见下面的 Where。

- `class StepChart final : public Chart`

### `chart/cartesian/streamgraph.hpp`

> streamgraph.hpp —— 流图：多层序列围绕居中基线上下堆叠，形成"流动的河"。
>
> 终端字符化策略：逐列从基线向上/向下堆叠各层，用 █ 实心块按层着色；
> 基线可选 silhouette（对称居中）或 wiggle（最小摆动），x 轴是观测序号。

- `class StreamgraphChart final : public Chart`

### `chart/cartesian/violin.hpp`

> violin.hpp —— 小提琴图：核密度轮廓
>
> 终端字符化策略：每组样本先做高斯核密度估计（Silverman 带宽），再沿 y 方向
> 切成若干水平带，每带的密度映射为**以组中线为轴的对称 █ 条带宽度**——
> 密度越高条带越宽，轮廓自然呈小提琴形；中位数画 ─。

- `class ViolinChart final : public Chart`

### `chart/cartesian/waterfall.hpp`

> waterfall.hpp —— 瀑布图：顺序增量/减量 + 小计
>
> 终端字符化策略：每段是**从累计基线画到新累计值**的 █ 柱（增/减/小计三色
> 区分语义，这是瀑布图区别于普通柱状图的关键）；相邻柱之间在上一段结束的
> 累计高度画 ─ 连接线，视觉上串起"流水"。x 轴是条目序号。

- `class WaterfallChart final : public Chart`

### `chart/chart.hpp`

> chart.hpp —— Chart 抽象基类
>
> chart/ 是 stage 3 的顶点：把数据系列 + 比例尺 + layout 拼成可见图表，画进
> canvas::Canvas。**不碰 render 层**（架构禁止边），于是未来 to_document()
> 通道独立存在。绘制上下文见 context.hpp。

- `void data_bounds(const std::vector<Series>& series, double& xmin, double& xmax, double& ymin, double& ymax)`
- `layout::PlotArea make_area(canvas::Canvas& canvas, const layout::Margin& margin, double xmin, double xmax, double ymin, double ymax, bool auto_domain, bool include_zero)`
- `void draw_legend_for(canvas::Canvas& canvas, const std::vector<Series>& series, const layout::PlotArea& area, bool show)`
- `class Chart`

### `chart/context.hpp`

> context.hpp —— 图表绘制上下文（DrawContext）
>
> 从 `chart.hpp` 独立出来（一个上下文、一个基类、一个系列类型，各自成文件）：
> 上下文描述"这一次绘制用什么开关和样式"，与 `Chart` 基类解耦后，GUI 侧与
> 文档导出路径可以构造自己的上下文（例如不画轴、不画标题、只用网格），
> 而不必继承或伪造一个 Chart。
>
> 所有开关默认与既有行为一致（全开），因此老调用点只需 `DrawContext{canvas}`。

- `struct DrawContext`

### `chart/dataset.hpp`

> dataset.hpp —— 数据集（系列集合 + 轴名 + 范围查询）
>
> `Dataset` 是"画图前的那份数据"：若干具名系列 + 两个轴名，并提供图表需要的
> 范围查询（自动定义域、是否含零）。它**不认识画布**，因此 GUI 侧、导出路径与
> 未来的 DSL 都能复用它作为中间表示 —— 这也是它独立成文件（而不是塞进 chart.hpp）
> 的原因。

- `struct DataRange`
- `class Dataset`

### `chart/financial/depth.hpp`

> depth.hpp —— 深度图（订单簿买卖两侧阶梯填充）
>
> 字符化策略：买盘（bid）与卖盘（ask）各自按 (price, qty) 画阶梯，
> 阶梯线下方到量基线之间填 ░，两侧同框、以中间价自然分界。

- `struct DepthOptions`
- `class DepthChart final : public Chart`

### `chart/financial/drawdown.hpp`

> drawdown.hpp —— 回撤图（水下曲线）
>
> 字符化策略：回撤线 '•' 逐点落格，线与 0 基线之间铺 '░'（水下填充）。
> 统计口径：dd_i = p_i / peak_i - 1（peak_i 为前缀最大价），恒 ≤ 0；y 域
> [min dd, 0]。非有限价格剔除，剩余不足 1 个不绘制。

- `struct DrawdownOptions`
- `class DrawdownChart final : public Chart`

### `chart/financial/kagi.hpp`

> kagi.hpp —— 卡吉图（转折摆动列，粗细表强弱）
>
> 字符化策略：按反转阈值抽出 zigzag 摆动，每个摆动占一列：竖段用
> ┃（阳：超过前一同向极值）/ │（阴），转折位用 ─ 连到下一列。
> reversal<=0 时自动取全距 5%。

- `struct KagiOptions`
- `class KagiChart final : public Chart`

### `chart/financial/point_figure.hpp`

> point_figure.hpp —— 点数图（Point & Figure）
>
> 字符化策略：价格按 box_size 量化为格档，X 列（上行情）落 'X'、O 列（下行
> 情落 'O'，每档一格、沿列纵向堆叠；方向反转需回调 reversal 格（默认 3）。
> 统计口径：box = floor(p/box_size)；上行列只抬顶、下行列只沉底；box_size
> ≤ 0 时取全距/10，全距为 0 或价格不足 2 个不绘制。列的 top/bottom 以格档
> 号给出（top ≥ bottom，单档列为 top == bottom）。

- `struct PointFigureOptions`
- `struct PfColumn`
- `class PointFigureChart final : public Chart`

### `chart/financial/renko.hpp`

> renko.hpp —— 砖形图（固定砖值的价格阶梯）
>
> 字符化策略：价格每越过一档 brick_size 生成一块砖，升砖绿、降砖红，
> 每砖 2 格宽 █、沿时间轴逐格右移并上下阶梯。brick_size<=0 时取全距/10。

- `struct RenkoOptions`
- `class RenkoChart final : public Chart`

### `chart/financial/volume.hpp`

> volume.hpp —— 成交量图（基线起柱，方向色）
>
> 字符化策略：每根柱占一个槽位、从量基线起填 █，高度 ∝ 成交量；
> 方向表（涨/跌）存在时分别用 up/down 色，缺省用中性灰。

- `struct VolumeOptions`
- `class VolumeChart final : public Chart`

### `chart/financial/yield_curve.hpp`

> yield_curve.hpp —— 收益率曲线（到期期限 → 收益率）
>
> 字符化策略：点间以 '•' 连线、每个观测点落 '●'；x = 到期期限（绘制前按
> 升序重排，重复期限取首见），y = 收益率。非有限观测剔除。

- `struct YieldCurveOptions`
- `class YieldCurveChart final : public Chart`

### `chart/flow/funnel.hpp`

> funnel.hpp —— 漏斗图（逐段居中横条）
>
> 字符化策略：每个阶段占一行，条宽 ∝ value / 最大值、水平居中，填 █；
> 行首画名称文本（可选附值）。无轴，纯条 + 文本。

- `struct FunnelStage`
- `struct FunnelOptions`
- `class FunnelChart final : public Chart`

### `chart/flow/stream.hpp`

> stream.hpp —— 流向图（ThemeRiver 语义）
>
> 规格里本文件是"转发 cartesian/streamgraph"的同层语义包装；但
> cartesian/streamgraph 属另一工作包且尚未落地，为不让本包空转，
> 这里先自给堆叠算法（各列按占比从基线向另一侧堆叠 █），待
> cartesian/streamgraph 落地后可改回纯转发。布局不依赖 graph/。

- `struct StreamOptions`
- `class StreamChart final : public Chart`

### `chart/geo/choropleth.hpp`

> choropleth.hpp —— 分级统计图（示意地图级别）
>
> **精度立场**：区域由调用方给定的**字符格集合**表达，不做真实地理投影、
> 不读取边界数据。格粒度轮廓有台阶是终端字符网格的固有限制，**不做假
> 精度**；本图只负责"值 → 分级色"这一步。分级：把区域值域均分成 bins
> 档，值越高灰阶越深（256 色灰阶段 232..255）。

- `struct GeoRegion`
- `struct ChoroplethOptions`
- `class Choropleth final : public Chart`

### `chart/geo/dot_density.hpp`

> dot_density.hpp —— 点密度（示意地图级别）
>
> **精度立场**：点只落在调用方给定的区域格集合内，不做真实投影、不做
> 随机抖动（确定性优先），**不做假精度**。每 unit 单位数值画一个点
> （'•'），一格至多一个点；点数超出格数时截断到格数。

- `struct DotDensityRegion`
- `struct DotDensityOptions`
- `class DotDensity final : public Chart`

### `chart/geo/geo_map.hpp`

> chart/geo/geo_map.hpp —— 真地图图种（GeoJSON + 投影 + 区域着色）
>
> 区别于 Choropleth：
> * Choropleth：调用方给字符格集合（cells）+ 值，本库只画"值 → 颜色"。
> * GeoMap：本库读 GeoJSON → 经纬度多边形 → 投影 → 字符格画多边形 + 按值着色。
>
> 设计选择：
> * 输入：assets/maps/*.json（GeoJSON FeatureCollection）+ 一组 "feature_id → value"
> 映射（默认按 properties.name 匹配）；feature_id 与 name 都不匹配时该多边形灰显。
> * 投影：等距矩形（默认）/ Mercator / 正射 / 兰勃特 —— enum 切换。
> * 输出有三条，分辨率从低到高（共用同一份投影几何，不重复实现投影）：
> 1. draw(DrawContext)     —— 字符格 Bresenham 描边，兼容旧调用方（最低清）
> 2. rasterize(cols, rows) —— 子像素栅格化：盲文 2×4 / 象限 2×2 / 半块 1×2，
> 逐格保留分级色（一次出图即有 8× 分辨率与颜色）
> 3. render_bitmap(w, h)   —— 真彩色位图任意像素尺寸（GUI / PNG / sixel 的数据源）
> 仍不直接 emit Scene：Scene 走 0..1 归一化坐标，而地图是"经纬度 ↔ 目标网格"的
> 强契约，塞进归一化坐标就丢掉了栅格化时的子像素归属。

- `enum class GeoProjection : std::uint8_t`
- `struct GeoMapOptions`
- `enum class GeoRasterStyle : std::uint8_t`
- `struct GeoRasterOptions`
- `struct GeoBitmapOptions`
- `class GeoMap final : public Chart`

### `chart/geo/hexbin.hpp`

> hexbin.hpp —— 六边形分箱（示意地图级别）
>
> **精度立场**：偏移网格近似六边形铺排（奇数行右移半径一半），不是几何
> 精确的正六边形，也不做真实投影，**不做假精度**。输入点 (x, y) 落进最近
> 的六边形桶，桶内计数再按 bins 档分级着色（计数越多越深）。

- `struct HexbinOptions`
- `class Hexbin final : public Chart`

### `chart/geo/tile_grid.hpp`

> tile_grid.hpp —— 瓦片网格（示意地图级别）
>
> **精度立场**：每个区域一个瓦片（1 格），不做真实地理投影，**不做假
> 精度**——这是"一个区域一格"的示意图（类似 tile grid map），不适合
> 面积/形状读数。瓦片字符取区域名的首簇（空名用 █），颜色按值分级
> （灰阶 232..255，值越高越深）。

- `struct GeoTile`
- `struct TileGridOptions`
- `class TileGrid final : public Chart`

### `chart/hierarchical/dendrogram.hpp`

> dendrogram.hpp —— 树状图（叶均布底线、父节点横杠连接）
>
> 字符化策略：叶子沿底线均布并画 ●，父节点 x = 子节点均值、row = 深度，
> 连接件用 ─（横杠）与 │（垂线）拼 U 形。布局自给（chart > graph 禁止边）。

- `struct DendrogramOptions`
- `class Dendrogram final : public Chart`

### `chart/hierarchical/icicle.hpp`

> icicle.hpp —— 冰柱图（每层一行、宽度 ∝ value 的嵌套分段）
>
> 字符化策略：深度 → 行（自上而下一层一行），子节点按 value 比例分割父节点
> 的水平区间；每段填 █，段宽 ≥2 时段首放名称首字符。布局自给（chart > graph
> 禁止边）。

- `struct IcicleOptions`
- `class Icicle final : public Chart`

### `chart/hierarchical/node.hpp`

> node.hpp —— hierarchical/ 共享的树节点与布局辅助（自给，不依赖 graph/）
>
> chart 是 L12、graph 是 L13，`chart > graph` 是禁止边（cmake/TermchartModules.cmake），
> 因此层次图的数据输入（HierNode）与"聚合 / 配色 / 首码点"等小工具都在这里自给，
> 不 include graph/ 的任何布局器。规格表（结构.md 附二）只列了 6 个图种头，
> 本文件是为避免 6 处重复而增设的子目录内共享头，随 6 个图种一起交付。

- `struct HierNode`
- `TERMCHART_API double aggregate_values(HierNode& node)`
- `TERMCHART_API Color hier_palette(std::size_t branch) noexcept`
- `TERMCHART_API void assign_colors(HierNode& node, std::size_t branch = 0, std::size_t depth = 0)`
- `TERMCHART_API char32_t first_codepoint(const std::string& text)`

### `chart/hierarchical/pack.hpp`

> pack.hpp —— 圆堆积（bubble pack）
>
> 字符化策略：半径 ∝ √value 的圆，行式贪心排布（放不下换行）；圆周用 ●
> 描边、内部填 ·，半径足够时圆心放名称首字符。布局自给（chart > graph 禁止边）。

- `struct PackOptions`
- `class PackChart final : public Chart`

### `chart/hierarchical/sunburst.hpp`

> sunburst.hpp —— 旭日图（径向环 + 角向扇区）
>
> 字符化策略：极坐标逐格采样——修正终端 1:2 宽高比后，环号 = 深度、
> 角向按聚合 value 比例分扇区，命中的格填 █。布局自给（同 treemap 的理由，
> chart > graph 是禁止边）。

- `struct SunburstOptions`
- `class Sunburst final : public Chart`

### `chart/hierarchical/tree_view.hpp`

> tree_view.hpp —— 树形视图（缩进树）
>
> 字符化策略：一行一节点，├─ / └─ / │ 缩进前缀 + 名称文本（可选后缀值），
> 纯文本绘制走 draw_text，超出画布由裁剪兜底。布局自给（chart > graph 禁止边）。

- `struct TreeViewOptions`
- `class TreeView final : public Chart`

### `chart/hierarchical/treemap.hpp`

> treemap.hpp —— 树图（矩形切片）
>
> 字符化策略：slice-and-dice 递归切片，面积 ∝ 聚合 value（沿较长边切分，
> 让格子尽量偏方）；叶子格块填 █，块足够大时在左上角放名称首字符。
> 布局自给：chart 是 L12、graph 是 L13 且 `chart > graph` 为禁止边，
> 层次布局由本文件内 ~30 行递归完成，不 include graph/。

- `struct TreemapOptions`
- `class Treemap final : public Chart`

### `chart/hit_region.hpp`

> hit_region.hpp —— 通用命中区域（chart → interaction 的唯一契约）
>
> 交互层要能判断"点到了哪个图元"，但不能认识任何具体图表类型。解决办法：
> chart 侧登记若干 HitRegion（形状 + 几何 + **不透明 payload id**），交互层只
> 按形状做几何判定并回报 id。于是 interaction 不需要 switch 图表名——这正是
> ROADMAP 约束"hit_test 只 switch HitRegion::Shape，禁图表名"的落点。

- `enum class HitShape`
- `class HitRegion`

### `chart/indicator/gauge_kpi.hpp`

> gauge_kpi.hpp —— KPI 卡片：紧凑数字面板
>
> 卡片 = 边框 + 标签（上）+ 主值（中）+ 变化量（下，▲/▼）。
> 主值是**预格式化文本**——数字格式化策略（千分位、单位缩写）留给调用方，
> 库不替用户做决定。与 progress_ring 的分工：KPI 强调"读数"，不做弧形。

- `struct GaugeKpiOptions`
- `class GaugeKpi final : public Chart`

### `chart/indicator/progress_ring.hpp`

> progress_ring.hpp —— 进度环：单值环形
>
> **与 text/progress 的区分**（规格注明，勿混）：本图是**单值环形**读数
> （圆弧从顶部顺时针扫过 fraction 圈），text/progress 是横向条形组。
> 环在字符格粒度上用椭圆采样近似，格粒度有台阶，**不做假精度**。

- `struct ProgressRingOptions`
- `class ProgressRing final : public Chart`

### `chart/indicator/star_rating.hpp`

> star_rating.hpp —— 星级评分
>
> 满星 ★ / 半星 ◐（评分四舍五入到半星）/ 空星 ☆，一行排开。
> 评分收敛到 [0, max_stars]，非有限按 0。

- `struct StarRatingOptions`
- `class StarRating final : public Chart`

### `chart/indicator/status_light.hpp`

> status_light.hpp —— 状态灯
>
> 一个 ● 按状态着色（Ok 绿 / Warning 黄 / Error 红 / Unknown 灰），
> 可带标签。是全部 indicator 里最轻的读数件：一格即语义。

- `enum class LightState : std::uint8_t`
- `struct StatusLightOptions`
- `class StatusLight final : public Chart`

### `chart/indicator/thermometer.hpp`

> thermometer.hpp —— 温度计
>
> 竖管 + 底部球泡 + 右侧刻度。液柱高度按 (value - vmin) / (vmax - vmin)
> 线性映射，液色随填充比例分档（青→绿→黄→红）。值收敛到域内，非有限
> 值只画空管不画液柱，不崩溃。

- `struct ThermometerOptions`
- `class Thermometer final : public Chart`

### `chart/polar/donut.hpp`

> donut.hpp —— 环形图（带中心孔的饼图）
>
> 字符化策略：在饼图的格粒度解析填充上加一道**环带判定**——归一化半径
> rnorm ≤ 1（外缘）且 rnorm ≥ hole_ratio（内孔）的格才落笔；内孔刻意留空，
> 中心读数与背景因此可见。扇区仍用专属字符阶梯区分（█▓▒░■●▲◆○ 循环），
> 角度约定与 pie 一致（start_angle 度、clockwise 扫角）。

- `struct DonutOptions`
- `class DonutChart final : public Chart`

### `chart/polar/gauge.hpp`

> gauge.hpp —— 仪表盘（弧形刻度带 + 指针，含刻度）
>
> 字符化策略：弧 = 沿数学角**按角度采样**落格（与 canvas::arc_points 同法，
> 步长约每周 4·(rx+ry) 格保证连续；格粒度下必有台阶，不做假精度）。弧体用
> '●'，主刻度用 '+' 压在弧体与内侧（rnorm 0.88）两处，指针是从圆心到
> rnorm 0.72 的 DDA 线段（'*'），圆心一粒 '●' 作轴帽。数值域 [value_min,
> value_max] 线性映射到 [start_angle, end_angle] 的顺时针扫角（默认 225°→
> -45°，即经典 270° 开口朝下的表盘）。

- `struct GaugeOptions`
- `class GaugeChart final : public Chart`

### `chart/polar/pie.hpp`

> pie.hpp —— 饼图
>
> 字符化策略：圆盘 = 绘图区内切椭圆的**格粒度解析填充**——逐格算归一化半径
> （≤ 1 才落笔），与 graph/shape 的 Ellipse 同一哲学：曲线在格粒度下必然有台阶，
> 不做假精度。扇区不依赖颜色区分（单色终端仍可读）：每个扇区从阶梯表取**专属
> 填充字符**（█▓▒░■●▲◆○ 循环），扇区边界即字符边界。角度从 start_angle（度，
> 数学方向，90 = 正上）起按 clockwise 方向扫过，格的归属角 = 起始方向起扫过的角。

- `struct PieOptions`
- `class PieChart final : public Chart`

### `chart/polar/polar_bar.hpp`

> polar_bar.hpp —— 极坐标柱状图（圆形排布的径向柱）
>
> 字符化策略：柱 = 角域 × 半径域的格粒度解析填充——格落在第 i 个槽位
> （等角 2π/n 的中央 bar_width 比例）且 rnorm ≤ value_i/vmax 才写 '█'，
> 与 graph/shape 的椭圆同一哲学：格粒度下必有台阶，不做假精度。参考环
> （0.25/0.5/0.75/1.0 半径）与槽位射线用 '·' 沿角度采样画（DrawContext::
> draw_grid 开关控制）；没有直角坐标轴，DrawContext::draw_axes 被忽略。

- `struct PolarBarOptions`
- `class PolarBarChart final : public Chart`

### `chart/polar/polar_heat.hpp`

> polar_heat.hpp —— 极坐标热力图（角向 × 径向分箱的密度上色）
>
> 字符化策略：把 (角度°, 半径) 点落入 angle_bins × radial_bins 的极坐标分箱，
> 填充仍是格粒度解析判定——格落在盘内（rnorm ≤ 1）即按其归属角/环查计数；
> 无数据的格留空（保留参考环），有数据的格用 4 级阴影字符（░▒▓█，计数越多
> 越实，最低档非空格以便与"无数据"区分）+ colormap 颜色。参考环/槽位射线
> 用 '·'（DrawContext::draw_grid 开关）；色标画在内框上方（沿用笛卡尔
> heatmap 的做法）。

- `struct PolarHeatOptions`
- `class PolarHeatChart final : public Chart`

### `chart/polar/polar_line.hpp`

> polar_line.hpp —— 极坐标折线（角度 × 半径的数据序列连成线）
>
> 字符化策略：数据点 (角度°, 半径) 先映射到格坐标（角度从 start_angle 起沿
> clockwise 方向扫、半径除以 rmax 归一化成 rnorm），相邻点之间走 DDA 线段
> 离散（canvas::line_points）写 '•'，数据点本身压 '●'；外圈参考环用 '·'
> 沿角度采样（DrawContext::draw_grid 开关）。格粒度下线必有台阶，不做假精度。

- `struct PolarLineOptions`
- `class PolarLineChart final : public Chart`

### `chart/polar/radar.hpp`

> radar.hpp —— 雷达图（多轴蛛网 + 数据多边形）
>
> 字符化策略：蛛网的射线与环全部走 DDA 线段离散（canvas::line_points，与
> graph/shape 六边形同法）——**环是缩小的多边形**（连接各轴在 1/3、2/3、1
> 半径处的点），不是圆；数据多边形用 '•' 描边、'●' 标顶点，蛛网用 '·'，
> 三种字符互不相同，单色终端也能分层读。轴标签画在外顶点外推 1.15 半径处
> （越界由 canvas 裁剪）。DrawContext::draw_grid 关掉时不画蛛网。

- `struct RadarOptions`
- `class RadarChart final : public Chart`

### `chart/polar/rose.hpp`

> rose.hpp —— 南丁格尔玫瑰（等角扇区，半径编码数值）
>
> 字符化策略：与饼图同款的格粒度解析填充，差别在半径——各扇区等角（2π/n），
> 扇区 i 的落笔判据是 rnorm ≤ value_i / vmax（半径 ∝ 数值，面积夸大是本图种
> 的表达意图）。扇区用专属字符阶梯（█▓▒░■●▲◆○ 循环）+ 调色板色区分；
> DrawContext::draw_grid 关掉时连外圈参考环也不画。

- `struct RoseOptions`
- `class RoseChart final : public Chart`

### `chart/registry.hpp`

> registry.hpp —— 图表注册表（RAII 友好、构造期冻结）
>
> 与 RasterRegistry 同一套契约：instance() 是 function-local static，构造后只读。
> 注册的是工厂而非原型，避免 clone 语义；create() 每次给新实例。

- `class ChartRegistry`

### `chart/series.hpp`

> series.hpp —— 数据系列（值类型，header-only）
>
> 顺带放**默认配色的唯一真源**：它在本文件而不是 chart.hpp，是因为 chart.hpp
> include 了本文件，反过来会成环。

- `enum class DefaultColor`
- `struct Series`

### `chart/sparkline.hpp`

> sparkline.hpp —— 迷你折线（无轴、无网格、无标签）
>
> 用途：表格单元格里的一格趋势、仪表盘上的小趋势线。整块画布就是数据区，
> 因此它**不是** Chart 的子类（没有标题/边距/图例这些概念），只接受一个画布。
>
> 定域口径：默认按数据 min/max 自适应；`set_zero_baseline(true)` 时把 0 纳入域，
> 用于"涨跌是否越过基准线"的读数场景。

- `class Canvas`
- `class Sparkline`

### `chart/statistical/autocorr.hpp`

> autocorr.hpp —— 自相关函数（ACF）
>
> 字符化策略：每个滞后期一根棒棒糖——零线到 r_k 的竖线 '│'、顶端 '●'；
> ±1.96/√n 置信带画水平 '·' 参考线。统计口径：r_k = Σ(x_i-x̄)(x_{i-k}-x̄) /
> Σ(x_i-x̄)²，k = 1..max_lag（clamp 到 n-1），样本方差为 0（常数序列）或
> n < 2 时不绘制。

- `struct AutocorrOptions`
- `class AutocorrChart final : public Chart`

### `chart/statistical/bland_altman.hpp`

> bland_altman.hpp —— 一致性图（Bland-Altman）
>
> 字符化策略：每对方法画一点 '●'，横轴为两法均值、纵轴为差值；mean 差画
> '─' 实线，mean ± 1.96·SD 一致性界限画 '·' 虚线。统计口径：对有限且成对
> 的样本计算 d_i = a_i - b_i、m_i = (a_i + b_i)/2，mean = avg(d)、
> SD = 样本标准差（除以 n-1）；有效对 < 2 不绘制。

- `struct BlandAltmanOptions`
- `class BlandAltmanChart final : public Chart`

### `chart/statistical/corr.hpp`

> corr.hpp —— 相关矩阵（Pearson r → 字符色阶）
>
> 字符化策略：|r| 量化为 5 档映射到 '·'/'░'/'▒'/'▓'/'█'（阈值 0.15/0.40/
> 0.65/0.85），符号用前景色区分——正相关青、负相关红、近零暗灰；统计口径：
> Pearson 积差相关 r = cov/(σa·σb)，任一方差为 0（常数列）或长度不符的
> 部分按 r = 0（无相关）处理。

- `struct CorrMatrixOptions`
- `class CorrMatrixChart final : public Chart`

### `chart/statistical/distribution.hpp`

> distribution.hpp —— 分布曲线（高斯核密度估计 KDE）
>
> 字符化策略：KDE 曲线逐点落 '•'，曲线下方按列铺浅色 '▒'；统计口径：带宽
> h = 0.9·min(样本标准差, IQR/1.34)·n^(-1/5)（Silverman 经验法则，退化时取
> 单位带宽），曲线在 [min-3h, max+3h] 上等距评估 grid_points 个点，y 为
> 归一化密度。非有限样本剔除。

- `struct DistributionOptions`
- `class DistributionChart final : public Chart`

### `chart/statistical/ecdf.hpp`

> ecdf.hpp —— 经验分布函数（ECDF）
>
> 字符化策略：升序样本逐级"跳台阶"——水平段用 '─'、垂直抬升用 '│'、每个
> 样本在 (x_i, i/n) 处落 '●'；统计口径：F̂(x) = #{x_i ≤ x}/n，n 为有限样本
> 数，重复值合并为一次完整抬升。y 域固定 [0, 1]。

- `struct EcdfOptions`
- `class EcdfChart final : public Chart`

### `chart/statistical/forest.hpp`

> forest.hpp —— 森林图（各研究点估计 + 置信区间横线）
>
> 字符化策略：每行一条研究——CI 用 '─' 横线、点估计落 '●'、合计行落 '◆'；
> x = 0 处画竖直 '·' 参考线（无效应基线）。统计口径：CI 直接按给定
> [lower, upper] 画，不做再计算；x 域自动并入 0，保证参考线在场。

- `struct ForestStudy`
- `struct ForestOptions`
- `class ForestChart final : public Chart`

### `chart/statistical/pca.hpp`

> pca.hpp —— 主成分投影（协方差矩阵 + 幂迭代）
>
> 字符化策略：样本经 top-2 主成分投影后以 '●' 落格，轴即 PC1/PC2；统计
> 口径：列中心化后取协方差矩阵（除以 n），幂迭代求 PC1、对 C - λ1·v1·v1ᵀ
> 降幂求 PC2，解释方差比 = 特征值 / 总方差（迹）。投影退化（单点）时给
> 单位跨度；有效样本 <2 或维度 0 时不绘制。

- `struct PcaOptions`
- `class PcaChart final : public Chart`

### `chart/statistical/regression.hpp`

> regression.hpp —— 线性回归 + 残差
>
> 字符化策略：样本落 '●'、拟合线 '─'、残差竖线 '·'（点到拟合线的竖直偏差）。
> 统计口径：普通最小二乘 y = a + b·x，只对有限样本拟合，n ≥ 2 且 x 方差 > 0
> 才成立；R² = 1 - SSres/SStot。拟合失败时 slope/intercept/R² 为 0、
> fitted() 为 false，绘制退化为纯散点。

- `struct RegressionOptions`
- `class RegressionChart final : public Chart`

### `chart/statistical/roc.hpp`

> roc.hpp —— ROC/PR 曲线 + AUC
>
> 字符化策略：曲线逐点落 '•'，ROC 模式叠对角参考线 '·'（随机分类器基线）。
> 统计口径：按分数降序扫描阈值（同分合并为同一阈值），累积 (FPR, TPR) 得
> ROC、(recall, precision) 得 PR；AUC 用梯形积分。正类 = label > 0。正负
> 样本不同时缺（无法定义两指标）不绘制。

- `enum class RocMode : std::uint8_t`
- `struct RocOptions`
- `class RocChart final : public Chart`

### `chart/statistical/seasonal.hpp`

> seasonal.hpp —— 季节分解（趋势 + 季节 + 余项）
>
> 字符化策略：内区纵向三分带（trend/season/remainder），自上而下各画一条
> 曲线 '•'，带名写在左侧留白区。统计口径：period = m（≥2，不足两个周期不
> 绘制）；trend 为窗口 m 的居中移动平均（边缘不足半窗为 NaN）；season[j]
> = 按 i mod m 相位平均去趋势残差；remainder = x - trend - season[相位]。

- `struct SeasonalOptions`
- `class SeasonalChart final : public Chart`

### `chart/statistical/survival.hpp`

> survival.hpp —— 生存曲线（Kaplan-Meier 估计）
>
> 字符化策略：KM 阶梯自 1 递降——水平段 '─'、事件处垂直抬升（下降）'│'、
> 删失（censored）时刻落 '+' 刻度。统计口径：S(t) = ∏(1 - d_i/n_i)，d_i 为
> 事件数、n_i 为处于风险的个体数（time ≥ t_i），同刻事件合并一步；times 与
> events 按较短者取齐，非有限时间剔除。y 域固定 [0, 1]。

- `struct SurvivalOptions`
- `class SurvivalChart final : public Chart`

### `chart/text/progress.hpp`

> progress.hpp —— 进度条组（横向条形，多值并列）
>
> **与 indicator/progress_ring 的区分**（规格注明，勿混）：本图是**横向
> 条形组**（多个 0..1 值并列，行式读数），progress_ring 是**单值环形**
> 读数。二者服务不同场景，不互相替代。
>
> 每行：标签 + 液条（█ 已完成 / ░ 未完成）+ 百分比。fraction 收敛到
> [0,1]，非有限值按 0 处理，不崩溃。

- `struct ProgressOptions`
- `class Progress final : public Chart`

### `chart/text/table.hpp`

> table.hpp —— 数据表格
>
> 列宽 = 表头与该列所有单元格的最大**显示宽度**（util/wcwidth.hpp，CJK 宽
> 2 列）。总宽超出画布时整列裁掉（从右往左丢弃，至少保一列），单元格文本
> 超列宽按显示宽度截断（绝不切在组合记号中间）。表头加粗，表头下画一条
> ├─┤ 风格的分隔线。

- `class Table final : public Chart`

### `chart/text/tag_cloud.hpp`

> tag_cloud.hpp —— 标签云：流式排版
>
> 与 wordcloud 的分工：wordcloud 用螺线做二维自由布局，本图是**一维流式**
> 排版（像文本换行：从左到右，放不下换行）。标签宽度按**显示宽度**计算
> （util/wcwidth.hpp：CJK 宽 2 列）；权重决定顺序（降序）与醒目程度
> （前若干词加粗、按排名分档配色）。

- `struct TagCloudOptions`
- `class TagCloud final : public Chart`

### `chart/text/wordcloud.hpp`

> wordcloud.hpp —— 词云：螺旋布局
>
> 布局策略：词按权重降序，从画布中心沿阿基米德螺线找第一个无碰撞的落点。
> 碰撞检测按**显示宽度**逐格判定（util/wcwidth.hpp：中日韩全宽字符占 2 列），
> 因此中文词不会互相覆盖；螺线纵坐标除以 2 补偿字符格约 1:2 的宽高比。
> 布局是字符格粒度的（坐标按格吸附），不做子像素，不做假精度。
> "字号"用属性位与颜色表达：权重最高的若干词加粗、按排名分档配色。

- `struct WordcloudOptions`
- `class Wordcloud final : public Chart`

## chart3d（L12）

### `chart3d/common.hpp`

> common.hpp —— chart3d 模块共享的 3D 着色/取景辅助（header-only）
>
> scatter3d 与 surface3d 都用得到：深度→颜色着色（shade）与按包围盒自动取景
> （make_camera）。抽到独立头，保证 scatter.hpp / surface.hpp 都能独立 include。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `chart3d/projector.hpp`

> projector.hpp —— chart3d 的投影缝：3D 世界坐标 → 画布坐标的多态接口
>
> 为什么抽接口：scatter3d / surface3d 原先直接持有 math3d::Camera（Mat4
> 视图×投影），把"怎么看"焊死在"画什么"里。抽 IProjector 之后：
> * Projector3D：现有 Camera 的薄封装（默认实现，行为逐字节不变）；
> * 未来 Projector4D：4D→3D 投影产出 Vec3 序列，再经本接口完成
> 3D→2D——两阶段各自可替换，scatter/surface 与 chart4d 都不用改；
> * 其他投影算法（正交 / 弱透视 / 自定义相机）同样只实现本接口即可注入。
>
> 注入方式：scatter3d / surface3d 的可选尾参数 `projector`，nullptr =
> 按 opts 的 azimuth/elevation/fovy 现算 Projector3D（既有调用方零改动，
> opts 里的视角参数在注入投影器时被忽略——视图归投影器所有）。

- `class TERMCHART_API IProjector`
- `class TERMCHART_API Projector3D final : public IProjector`

### `chart3d/scatter.hpp`

> scatter.hpp —— 3D 散点图：把三维点云投影成 Scene（header-only）
>
> 不做终端专属渲染：产出的是与 2D 图表同构的 scene::Scene，交给已有的
> render_scene → Canvas → Renderer 管线，复用 braille/halfblock 光栅化。深度只用于
> 着色（近亮远暗），立体感由此而来，无需自造 z-buffer。

- `struct Scatter3dOpts`

### `chart3d/surface.hpp`

> surface.hpp —— 3D 曲面/网格图：把 z=f(x,y) 投影成 Scene 线框（header-only）
>
> 与 scatter 同构：只产出 scene::Scene，复用现有终端光栅管线。曲面以**线框**呈现
> （每行/每列相邻的网格点连线），连线颜色按平均深度着色；这也是 3D 曲面最易辨识、
> 最稳的画法，避免在终端上赌多边形填充的清晰度。

- `struct Surface3dOpts`

## chart4d（L12）

### `chart4d/projector4d.hpp`

> projector4d.hpp —— 4D 投影器：实现 chart3d::IProjector，串起两阶段 4D→2D
>
> 数据流（用户口径："Vec4 -> Projector4D -> Vec3 -> Projector3D -> Vec2"）：
> 阶段一（math4d，本类持有）：Vec4 经 4D 旋转（6 平面组合）→ 透视除以 W → Vec3；
> 阶段二（内层 IProjector，通常是 chart3d::Projector3D）：Vec3 → 画布 Projected。
> 两个阶段各自可替换：换旋转/换 4D 相机只动阶段一；换 3D 视图（正交、弱透视、
> 自定义轨道相机）只换内层 IProjector——这正是当初把 chart3d 抽成 IProjector 的目的。
>
> 为什么继承 IProjector 却又多一个 Vec4 入口：IProjector::project 的入参是
> Vec3（3D 缝），4D 点无法穿过它。故本类：
> * 新增 project(const Vec4&, ...) —— 真正的两阶段 4D 入口（线框/超立方体走这条）；
> * 覆写 project(const Vec3&, ...) —— 满足 IProjector：喂 3D 点时等价于内层
> 投影器（4D 旋转/透视不参与），使本类也能直接插进 scatter3d/surface3d。

- `class TERMCHART_API Projector4D final : public chart3d::IProjector`

### `chart4d/wire.hpp`

> wire.hpp —— 4D 线框/点云 → scene::Scene（header-only，chart3d 的 4D 对应物）
>
> 与 chart3d/scatter.hpp 同构：发射器只管几何与图元，视图全权委托 Projector4D
> （两阶段 4D→2D）。每条边的两端经投影器落到画布，两端均可见才画；顶点可选画
> 成按深度着色的小方块。边按两端平均深度着色，给旋转中的 4D 形体以深度提示。

- `struct Wire4dOpts`

## graph（L13）

### `graph/arrow.hpp`

> arrow.hpp —— 箭头头部（字符与占格几何）

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/attributes.hpp`

> attributes.hpp —— 节点/边的键值元数据（header-only 值类型）
>
> 图元数据是开放的（label、color、shape、rank、weight 等由各层约定），因此
> 这里用字符串键值袋而不是固定字段：新增元数据不需要改图的数据结构。
> 用透明比较器，查找可直接接受 std::string_view，避免临时构造 std::string。

- `class Attributes`

### `graph/crossing.hpp`

> crossing.hpp —— 边交叉计数（布局质量度量）

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/diagrams/adjacency_matrix.hpp`

> adjacency_matrix.hpp —— 邻接矩阵（行列标尺 + 值分级的块字符）
>
> 字符化策略：首行放列名标尺、每行行首放行名（均按显示宽度截断对齐），格子
> 按值相对最大值的比例分级为 █（强）▓（中）░（弱），零值画 ·——格粒度下
> "深浅"只有离散三档，不做假精度。

- `class Canvas`
- `struct AdjacencyMatrixOptions`

### `graph/diagrams/arc_diagram.hpp`

> arc_diagram.hpp —— 弧线图（节点一排 + 跨度定高的弧）
>
> 字符化策略：节点按登记顺序沿底行一字排开（标签居中于各自的列），连接画成
> 底行上方的二次贝塞尔弧——跨度（两端列距）越大弧顶越高，高度封顶防出界；
> 弧体逐格按走向取 ─ │ ╱ ╲。同一节点的自连接画成小拱。

- `class Canvas`
- `struct ArcNode`
- `struct ArcLink`
- `struct ArcDiagramOptions`

### `graph/diagrams/chord.hpp`

> chord.hpp —— 和弦图（实体环形排布 + 弧带连接，值定带宽）
>
> 字符化策略：实体沿椭圆环等分排布（横向半径按字符格 1:2 宽高比放大为纵向的
> 两倍，环本身复用 graph/shape 的 Ellipse 轮廓——格粒度台阶不做假精度）；弦
> 画成弯向圆心的二次贝塞尔弧，逐格按走向取 ─ │ ╱ ╲；连接值越大带越宽，做法
> 是把锚点沿环的切向展开成若干条平行弧。

- `class Canvas`
- `struct ChordEntity`
- `struct ChordLink`
- `struct ChordOptions`

### `graph/diagrams/circuit.hpp`

> circuit.hpp —— 电路图（元件字符符号 + 引脚正交连线）
>
> 字符化策略：元件画成固定尺寸的字符符号——电阻是带内标的矩形框、电容是两块
> 平行板（─│ │─，板间断开）、电池是长板+短板、地是逐级收窄的横杠（│ + ─── +
> ─ + ·）；引脚连线走正交走线，转折收口成 ┌ ┐ └ ┘，中段逐格避让元件框
> （找不到空档才按 L 形直连，宁可直穿也不消失——与 route/ 的退化口径一致）。

- `class Canvas`
- `enum class ComponentKind`
- `struct CircuitComponent`
- `struct CircuitPin`
- `struct CircuitWire`
- `struct CircuitOptions`

### `graph/diagrams/dag.hpp`

> dag.hpp —— 有向无环图渲染入口
>
> 入口先做环检测（Kahn 拓扑排序，见 is_acyclic），渲染策略：
> * 无环：分层布局（层 = 拓扑深度，所有边一律从左往右跨层）+ 正交路由。
> * 有环：**不报错、不丢边**——退化为普通分层布局（layout_layered 自带破环，
> 环上节点挂到已定层前驱的下一层），环的回边照常画出并带箭头。调用方可先
> 用 is_acyclic() 自行决定是否提示用户。

- `class Canvas`
- `struct DagOptions`

### `graph/diagrams/dependency.hpp`

> dependency.hpp —— 依赖图渲染入口（建在 dag 之上）
>
> 约定：边 A→B 表示"A 依赖 B"。布局与 dag 相同（分层 + 依赖方向一致：所有边
> 从左往右跨层推进，被依赖者位于更右的层）。在 dag 的基础上多一件事：
> **依赖环标记**——mark_cycles 打开时，处于环上的边（两端在同一强连通分量，
> 自环也算）被改写 ArrowStyle::Both（两端箭头，⇄ 观感）以提示互相依赖；
> 这是会改写图的绘制状态的操作，关闭选项则不碰任何 ArrowStyle。
> 环判定用"逐边反向 BFS"（边数 * (V+E)），终端图规模下足够；不引入 Tarjan。

- `class Canvas`
- `struct DependencyOptions`

### `graph/diagrams/er_diagram.hpp`

> er_diagram.hpp —— ER 图（实体表框 + 鸭掌记号基数 + 关系名标签）
>
> 字符化策略：实体画成**表框**——实体名居中一行、├─┤ 分隔、字段左对齐（关系
> 模式即"表"）；关系按正交折线绘制，两端用**鸭掌记号**（crow's foot）标注基数，
> 记号画在紧贴实体边框的那段线上：
> * 恰一（One）     → 两道垂直短杠 `││`
> * 多（Many）      → 鸭掌 `<` `>` `∧` `∨`（按进框方向选）
> * 零或一（0..1）  → 圆圈 `○` + 一道短杠 `│`
> 关系名作边标签，用 label_place 避让实体框（放不下宁可不画）。
>
> 模型约定（沿用 graph::Graph）：
> * Node::label() 是实体名；Node::attributes() 的 "fields" 是字段清单，
> 多行以 '\n' 连接；
> * Edge::label() 是关系名；Edge::attributes() 的 "card_from" / "card_to"
> 是两端基数，取值 "1"（恰一）、"N"/"many"/"*"（多）、"0..1"（零或一），
> 缺省按恰一处理。

- `class Canvas`
- `enum class ErCardinality : std::uint8_t`
- `struct ErDiagramOptions`

### `graph/diagrams/flowchart.hpp`

> flowchart.hpp —— 流程图（图 --> 画布 的一步到位入口）
>
> 组合关系固定为：分层布局 -> 路由（正交/直线）-> 绘制形状、标签、箭头。
> 需要单独控制某一步时，直接调用 graph/layout、graph/route、graph/shape。

- `class Canvas`
- `struct FlowchartOptions`

### `graph/diagrams/knowledge_graph.hpp`

> knowledge_graph.hpp —— 知识图谱（实体-关系网络）
>
> 字符化策略：实体画带边框的盒子并居中标名，关系画直线连线（可带箭头与
> 关系标签）；布局复用力导向（layout_force），绘制复用节点-连线视图
> （render_node_link）。本层只做"布局 + 视图"的选项组合，不新增绘制逻辑。

- `class Canvas`
- `struct KnowledgeGraphOptions`

### `graph/diagrams/metro_map.hpp`

> metro_map.hpp —— 地铁线路图（线路 = 有序站点序列 + 线色）
>
> 字符化策略：站点画带边框的小盒并居中标名（同名站点合并为换乘站），线路
> 沿 45° 走线并按线色着色（复用 layout_metro + route_metro，不画箭头）。

- `class Canvas`
- `struct MetroLine`
- `struct MetroMapOptions`

### `graph/diagrams/mindmap.hpp`

> mindmap.hpp —— 思维导图 + 通用"节点-连线"视图
>
> render_node_link 是**不做布局**的通用渲染：调用方先自行布局（layout_force /
> layout_grid / ...），它只负责把节点与边画出来。放在本头文件，是因为思维导图
> 正是这类视图的典型形态；力导向图（`examples/20_force_graph`）复用它。
>
> render_mindmap = layout_tree(left_right) + render_node_link（默认不画边框，
> 标签直接作为节点，连线即分支）。

- `class Canvas`
- `struct NodeLinkOptions`
- `struct MindmapOptions`

### `graph/diagrams/orgchart.hpp`

> orgchart.hpp —— 组织架构图（层级树 + 共享树干 + ├/└ 接点）
>
> 与思维导图的区别（两者都用树）：
> * 思维导图把根摆在中间、子树左右分列，连线按序号错开成"扇形"；
> * 组织架构图是**单向层级**：深度沿 x 向右增长，同父的兄弟共用一条**树干**
> （一条竖线），每个子节点从树干上"分叉"出去，接点画成 `├` / `└` / `┌`
> 而不是把直角拆成两个独立字符 —— 这正是终端里 `tree` 类工具的形状，
> 在字符网格上比逐条正交路由（互相穿插的 Z 形）好读得多。
>
> 节点内容就是 `Node::label()`（居中一行）。要显示"姓名 + 职务"这类两行内容，
> 请把职务并进标签（例如 "CTO / 平台"）——本图刻意不做第二行，避免与边框争夺行数。

- `class Canvas`
- `struct OrgChartOptions`

### `graph/diagrams/petri_net.hpp`

> petri_net.hpp —— Petri 网（库所 / 变迁 / 有向弧 + token）
>
> 字符化策略：库所画椭圆、token 画 ●（超过上限改画数字），变迁画窄高矩形，
> 弧走正交折线带箭头；节点按弧可达深度分列（库所偶数列、变迁奇数列），
> 名称画在形状下方。

- `class Canvas`
- `struct PetriNet`
- `struct PetriNetOptions`

### `graph/diagrams/sankey.hpp`

> sankey.hpp —— 桑基图（节点 + 流，按 value 定横条高度、按阶段分层）
>
> 字符化策略：节点画 █ 实心竖条（高 ∝ 流量合计），流画 ░ 丝带（源条右缘到
> 目标条左缘，上下沿线性过渡）；阶段按流的可达深度分列，可按内置调色板
> 着色（节点取调色板色，丝带继承源节点色）。

- `class Canvas`
- `struct SankeyNode`
- `struct SankeyFlow`
- `struct SankeyOptions`

### `graph/diagrams/social_network.hpp`

> social_network.hpp —— 社交网络图（环形布局的节点-连线视图）
>
> 字符化策略：人画成带边框的盒子并居中标名，按注册顺序均分圆环摆位，
> 关系画直线连线（社交关系无向，默认不带箭头）。环形布局在本层内联实现
> （等弦间隔防重叠），绘制复用 diagrams 的共享骨架。

- `class Canvas`
- `struct SocialNetworkOptions`

### `graph/diagrams/state_machine.hpp`

> state_machine.hpp —— 状态机图（分层布局 + 转移标签）
>
> 与流程图的差别：状态画圆角框、**转移事件名画在边上**（复用 label_place 避让），
> 并按节点属性标注起止状态：
> attributes["initial"] == "1" → 左侧画 ● 与引入短线
> attributes["final"]   == "1" → 右侧画 ◎

- `class Canvas`
- `struct StateMachineOptions`

### `graph/diagrams/syntax_tree.hpp`

> syntax_tree.hpp —— 语法树（(label, children) 递归节点 + tree 风格连接线）
>
> 字符化策略：根在左、深度沿列向右增长，逐层用 ├ └ ─ 画连接、祖先未完的层画
> │ 延续——即终端 tree 命令的形态；一行一个节点，超宽标签由画布裁剪。遍历用
> 显式栈（深树不会爆调用栈）。

- `class Canvas`
- `struct SyntaxNode`
- `struct SyntaxTreeOptions`

### `graph/diagrams/uml_activity.hpp`

> uml_activity.hpp —— UML 活动图（圆角动作框 + 菱形判定 + ●/◎ 起止标记）
>
> 字符化策略：动作 = 圆角框；判定节点 = **菱形**（节点属性 "kind" == "decision"，
> 守卫条件走边标签，如 "[yes]"）；起止不占整框——没有显式 "initial"/"final"
> 属性时，自动把**首个人度 0 的节点**标为起点（框外左侧 `●` + 引入线）、
> **首个出度 0 的节点**标为终点（框外右侧 `◎`），标记方案与 state_machine 一致。
>
> 模型约定（沿用 graph::Graph）：Node::label() 是动作/判定名；节点属性
> "kind" == "decision" 判定，其余为动作；"initial"/"final" == "1" 显式指定起止。

- `class Canvas`
- `struct UmlActivityOptions`

### `graph/diagrams/uml_class.hpp`

> uml_class.hpp —— UML 类图（三栏类框 + 空心三角泛化 + 正交连线）
>
> 字符化策略：类框画成**三栏矩形**——类名居中、属性/操作左对齐，两栏之间用
> ├─┤ 分隔线（UML 分类格在字符网格上的等价表达，画法同 examples/17_uml_class）；
> 泛化边（默认）按正交折线绘制，并在**基类（from）端的边框格**上画空心三角
> `◁`（UML ▷ 语义指向基类）；边属性 "kind" == "assoc" 的边退化为普通箭头。
>
> 模型约定（沿用 graph::Graph，无需新容器）：
> * Node::label() 是类名；
> * Node::attributes() 的 "attrs" / "ops" 分别是属性栏与操作栏内容，
> 多行以 '\n' 连接，每行自带 UML 可见性前缀（+ 公开 / - 私有 / # 保护）。

- `class Canvas`
- `struct UmlClassOptions`

### `graph/diagrams/uml_sequence.hpp`

> uml_sequence.hpp —— UML 时序图（参与者头框 + 生命线 + 消息箭头 + 便签 + 激活条）
>
> 字符化策略：参与者画成顶部一排**头框**（矩形 + 居中名），框中心列向下引
> `│` 生命线；消息画在两条生命线之间的水平线上——实线 `─` / 虚线 `┄`，箭头按
> 语义选形：实心 ▶/◀、开口 ▷/◁、叉 ×（丢失消息）；Note 按 left/right/over
> 落在生命线旁或跨在两条生命线之间，整段用 [方括号] 包住与消息文本区分；
> 激活条画成生命线上的 `█` 段。
>
> 本层自持模型（graph(13) 不得 include dsl(17)，故不复用 AstSequence；
> dsl/mermaid_sequence 的 AstSequence 可机械映射到本模型，由上层适配：
> SequenceParticipant/SequenceMessage/SequenceNote 的字段一一对应）。
> 参与者按 key（字符串）引用；渲染时未知 key 的消息跳过、便签退化为 over 单人。

- `class Canvas`
- `enum class SeqArrowKind : std::uint8_t`
- `struct SeqParticipant`
- `struct SeqMessage`
- `struct SeqNote`
- `struct SeqModel`
- `struct UmlSequenceOptions`

### `graph/diagrams/uml_usecase.hpp`

> uml_usecase.hpp —— UML 用例图（参与者小人 + 椭圆用例 + 无箭头关联线）
>
> 字符化策略：参与者画成三行**小人**（O 头 / /|\ 身 / / \ 腿，名字在下），
> 用例画成**椭圆**（ShapeKind::Ellipse + 居中名）；参与者—用例之间连**无箭头**
> 直线（关联）。参与者站两翼：节点属性 "side" == "left"/"right" 显式指定，
> 缺省按边方向判（只作为 from → 左；只作为 to → 右；混合 → 左）。
>
> 模型约定（沿用 graph::Graph）：Node::attributes() 的 "kind" == "actor" 为
> 参与者，其余节点一律按用例画椭圆。

- `class Canvas`
- `struct UmlUsecaseOptions`

### `graph/edge.hpp`

> edge.hpp —— 图的有向边（端点 + 标签 + 元数据 + 路由折线）

- `enum class ArrowStyle`
- `class Edge`

### `graph/graph.hpp`

> graph.hpp —— 图容器（节点 + 有向边 + 邻接查询）
>
> Graph 是图可视化的语义层：只管"有哪些节点、怎么连"，不管布局与绘制。
> 布局（graph/layout）、路由（graph/route）、形状（graph/shape）与图元
> （graph/diagrams）都在这之上构建，因此同一张 Graph 可以被多种渲染消费。
>
> 注意：add_node 返回的是 vector 内元素的引用，后续 add_node 可能使引用失效；
> 需要长期持有请用 id() 再 find()。

- `class Graph`

### `graph/label_place.hpp`

> label_place.hpp —— 标签避让（在锚点周围挑一个不与已有矩形相交的位置）

- `struct LabelPlacement`

### `graph/layout/circular.hpp`

> circular.hpp —— 圆周布局（所有节点均匀分布在一个圆上）
>
> 与 radial 的“按父子关系分环”不同，圆周布局不看边，只按**节点登记顺序**把
> 全部节点均匀摆在一个圆上：第 i 个节点的角度为 θ_i = start_angle + 2π·i/n。
> 它适合“没有层次、只想看一圈并列节点”的场景（如维度环、雷达式清单）。
>
> 半径由节点数与节点尺寸推出，保证相邻节点框不重叠：
> 相邻中心在格坐标系里的最小距离为 d_min = 2·(R/aspect)·sin(π/n)
> （y 方向被压缩了 1/aspect，最紧的方向在 x 轴附近，故分母带 aspect）。
> 以节点框的外接圆半径 ρ_i = √(w²+h²)/2 作为“两框不重叠”的**充分**判据：
> d_min ≥ ρ_i + ρ_j + gap  ⇒  外接圆互不相交  ⇒  矩形互不相交。
> 于是 R = aspect·(max 相邻 ρ_i+ρ_j + gap) / (2·sin(π/n))，再用 min_radius 兜底。
> · 用外接圆是刻意取保守（“推一个足够的半径”）：它忽略矩形朝向，绝不会算小；
> · gap 还额外吸收落格取整带来的约 ±1 格误差，默认 2 足够。
> n = 1 时半径取 0（单节点落在圆心）；n ≥ 2 时半径才由上式与 min_radius 共同决定。

- `struct CircularOptions`

### `graph/layout/cluster.hpp`

> cluster.hpp —— 聚类布局（按组内聚、组间分离地摆放节点）
>
> 分组规则：节点带 attributes["cluster"]（字符串）时按属性值分组；没带属性的
> 节点按**无向连通分量**自动归组。每个聚类内部用网格（列数取 ceil(√n)）摆放，
> 整个聚类的包围盒再按网格排布（列数取 ceil(√k)），聚类之间留 cluster_gap。
> 返回所有节点外框的并集；不同聚类的包围盒保证互不重叠。

- `struct ClusterOptions`

### `graph/layout/force.hpp`

> force.hpp —— 力导向布局（Fruchterman-Reingold 简化版）
>
> 两条力：节点两两斥力 k^2/d，边方向弹簧引力 d^2/k。每轮按"温度"限制单步
> 位移并逐步降温，收敛后取整落格。
>
> 确定性：初始摆放固定为圆环（不用随机数），迭代与浮点运算顺序固定，因此同一
> 张图两次布局结果完全一致——这样布局才能进 golden 与回归测试。

- `struct ForceOptions`

### `graph/layout/kamada_kawai.hpp`

> kamada_kawai.hpp —— KK 全局能量布局（弹簧模型）
>
> 以图的最短路径（跳数）为目标距离做弹簧能量最小化：每轮选能量残差最大的节点
> 对，对其一端做 Newton 迭代解局部能量极小，直到所有残差低于 tolerance 或迭代
> 耗尽。初始摆放固定为圆环（不用随机数），同一张图两次布局结果完全一致。
>
> 退化口径：N=0 返回空矩形；N=1 只落一个节点框；N=2 直接按目标距离水平对放，
> 不进迭代；非连通图按连通分量各自独立布局后从左到右并排摆放（分量间无弹簧
> 约束，最短路径距离不存在）。

- `struct KamadaKawaiOptions`

### `graph/layout/layout.hpp`

> layout.hpp —— 图布局（把节点摆到画布格上，写入 Node::set_box）
>
> 布局只做几何：给定图与选项，为每个节点算出外框。它不画任何东西，也不
> 关心节点形状与连线，因此同一份布局结果可以被字符画、SVG 或 GUI 复用。

- `struct LayoutOptions`

### `graph/layout/metro.hpp`

> metro.hpp —— 地铁布局（线网 45° 走线）
>
> 真实地铁路网的画法约定：站点吸附到八线栅格（0°/45°/90°/135°），线路沿
> 45° 斜线与轴对齐段走线。本文件的实现分两层：
>
> **layout_metro（布局）**：把图按无向邻接贪心分解成若干条"线"（链，覆盖全部
> 节点，孤立节点是长度 1 的链），每条线在自己的水平带内沿 45° 之字排列——
> 链上相邻节点的外框原点恰好相差 (step, ±step)（换向处相差 (step, 0)），
> 因此锚点直连天然就是 45° 斜线或水平段。step 会被抬到不小于最大节点框的
> 宽与高，保证相邻框不相交：45° 段只在框外空档里走，**链内走线不穿框**由
> 几何直接保证，不依赖栅格检查。
>
> **route_metro（路由）**：与布局配套的 45° 走线。每条边先试锚点直连（仅当
> 恰为 45° 或轴对齐），再试 45° 步进走线（斜着走到对齐、再轴对齐直进），逐步
> 在栅格上查障碍；不通则退化为正交路由（复用 route_edges 的 Z 形策略），最后
> 退化为直线。跨线边（链与链之间）没有布局层的几何保证，靠这里的栅格检查兜底。

- `struct MetroOptions`
- `struct MetroRouteOptions`

### `graph/layout/radial.hpp`

> radial.hpp —— 径向树布局（极坐标 + 子树扇区均分）
>
> 思路与经典“径向树（radial tree）”一致：先把有向图按 BFS 摊成一棵根树（与
> tree_layout 同法：入度为 0 的第一个节点为根，全有入边时取第一个节点；只沿出边、
> 首次访问为准，因此有环图也能摆），再给每个节点分配一个角扇区：
> · 根独占整圆 [start_angle, start_angle + 2π)；
> · 父节点把自己的扇区**按子节点数均分**给每个孩子（每个孩子再继续细分）。
> 于是同一父下的兄弟角度间隔相等；兄弟的扇区互不相交 → 以兄弟为根的子树占据
> 互不相交的角度范围，子树之间天然不交叉。
>
> 半径只由层号决定：radius(level) = ring_start + level * ring_step。
> 为什么半径按层线性取：径向树的“环”本就是等距同心圆，层号相同 → 半径相同、
> 一眼能看出层次；层号越大 → 离根越远。ring_step 取“节点高 + 间距”量级即可，
> 默认 12：竖直方向经宽高比压缩后为 12/2 = 6 ≥ 节点最小高 4 + 2，相邻环不挤。
>
> 字符格不是正方形（宽 ≈ 高的 2 倍），正圆直接落格会显扁。落格时按
> canvas/transform.hpp::aspect_correction 的同一口径把 y 压缩 1/aspect：
> gx = cx + r·cos θ，gy = cy + r·sin θ / aspect。
> 因此在格坐标里量到的距离要按同口径还原（dx、dy·aspect）才等于几何半径 r，
> 本布局的“层半径”指的就是这个还原后的真实半径。
>
> 重叠：同一层里角度极近的两个节点，取整落格后可能落在同一格。本布局**允许**
> 这种重叠，但保证**确定**——纯几何、无随机，同输入逐格一致。要减少重叠可把
> min_angle 调大：它给同一父的相邻子扇区之间留出最小角间隙。

- `struct RadialOptions`

### `graph/layout/sugiyama.hpp`

> sugiyama.hpp —— 四步分层布局（Sugiyama 框架）
>
> 在 layout_layered 的"分层 + 坐标分配"之上补上第二步"同层顺序交叉最小化"
> （median / barycenter 启发式），让复杂 DAG 的同层节点不再按登记顺序简单堆叠
> 而交叉凌乱。四步：① 最长路径分层（含破环）② 同层交叉最小化 ③ 坐标分配
> ④ 边路由（由调用方在布局后调用 route，本函数只做几何）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/layout/tree_layout.hpp`

> tree_layout.hpp —— 根树布局（层级 + 兄弟子树行槽）
>
> 做法：从根开始 BFS 建树（首次访问为准，回边与重复边忽略，因此有环图也能摆），
> 后序分配"行槽"——叶子各占一行，父节点的行取子树中心；列由深度决定。
> left_right 模式把根的子树按登记顺序分成两半，一半向右生长、一半镜像向左，
> 根落在中间，用于思维导图。

- `struct TreeOptions`

### `graph/node.hpp`

> node.hpp —— 图节点（标识 + 标签 + 元数据 + 布局外框）
>
> Node 只持有"语义"与"布局结果"两类信息：语义（id/label/attributes）由调用方
> 填写，外框（box）由 graph/layout 写入，供 route/ 与渲染消费。节点不持有任何
> 绘制状态，因此同一张图可以用字符画、SVG 或 GUI 三种方式渲染。

- `class Node`

### `graph/port.hpp`

> port.hpp —— 节点外框上的连接端口（连线路由的锚点）

- `enum class PortSide`

### `graph/route/arc.hpp`

> arc.hpp —— 弧线边路由（含自环）
>
> 输出与其它路由一致：格坐标折线，且是 **8 连通格序列**（相邻格只差一行/一列/一格
> 对角），斜向步由既有 segment_char 渲染成 ╱ ╲，因此这里只产出几何、不产出字符。
>
> 两种形态：
> * 普通边（两端是不同节点）：三次贝塞尔弧——控制点沿弦方向外推一格距（保证离开
> 锚点时先朝框**外**走，不会先贴着边框列切进节点）、再按垂直于弦的方向隆起，
> 隆起量约为弦长的一半（"跨列半圆"的观感）。
> * 自环边（起终点同一节点）：绕到节点框**外侧**的一圈。锚点取在同一个节点相邻
> 两条边上（高框取东侧上下两个锚点，矮框取东侧 + 南侧绕右下角），先水平离框，
> 再把外侧折角用二次贝塞尔抹圆；因为整圈都在框外，节点标签（画在框内）不会被
> 覆盖。
>
> 退化策略（刻意，且可从"不穿框"反推）：候选弧一旦压到任何节点框内部或已占用栅格
> 就作废，依次尝试更小的隆起量 / 两侧方向；都不通则退化为直线（放不下时宁可直连，
> 也不让弧线斜穿节点）。自环放不下时退化为未抹圆的正交外形，再不行退化为锚点直连。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/route/bundling.hpp`

> bundling.hpp —— 边捆绑（共享目标节点的边在末段并束）
>
> 逐边独立路由时，多条边各占一条走廊涌入目标节点，观感杂乱。边捆绑把
> **共享同一目标节点**的边分成一组：组内先在目标锚点外侧 bundle_gap 格处定一个
> 公共"并束点"，每条边先以正交 Z 形走到并束点，再共享从并束点到锚点的**末段**
> 进框。整组只占用目标的一个锚点与一条末段走廊。
>
> 并束方向：从目标中心指向组内各源点中心的质心，取主轴侧作为进入侧；
> 并束点必须落在所有节点框之外，否则逐步加大距离重试，仍不行则该组放弃捆绑，
> 逐边按普通正交路由。自环边不参与捆绑（锚点重合，没有"末段"可言）。

- `struct BundlingOptions`

### `graph/route/junction.hpp`

> junction.hpp —— 轴对齐折线的交汇口字符
>
> 为什么把它抽成公共件：这套"按这一格有上/下/左/右哪几个方向来选方块字符"的规则
> 原先在 `src/graph/diagrams/orgchart.cpp` 与 `examples/17_uml_class.cpp` 里**各有一份**
> （各写一张 switch 表）。地铁线路图会是第三个消费者 —— 三份复制必然漂移，所以在
> 第三个出现之前把它收到这里，规则只留一份。
>
> 与 `segment_char` 的分工：`segment_char`（diagrams 骨架内部）判的是**单段线**该用
> `─`/`│`/`╱`/`╲`；本函数判的是**交汇口**（横竖相接处）该用哪个 T 形/十字字符，
> 否则折角会渲染成两个独立字符拼出的"断角"。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/route/obstacle.hpp`

> obstacle.hpp —— 障碍避让路由（外部矩形障碍绕行）
>
> 画布上常有不属于图的元素（图例、表格、已画好的面板），连线必须绕开。
> 把"外部障碍"（std::vector<Rect>，画布坐标系）登记进 WireGrid，再走
> route_edges 的正交路由（Z 形逐级绕行，失败退化为直线）。
>
> 约定：障碍四周外扩 clearance 格禁行；负坐标障碍裁剪到 0（WireGrid 是
> 无符号栅格）；grid 为空时自建覆盖全部节点、障碍与绕行余量的栅格。

- `struct ObstacleOptions`

### `graph/route/route.hpp`

> route.hpp —— 连线路由（为每条边算出折线）
>
> 路由的输入是"已布局"的图（节点外框已就位）；输出写进 Edge::set_route。
> 直线是最小实现；正交路由走"Z 形"，并可借助 WireGrid 避让已有连线（先是
> 中点，其次向两侧逐格绕行；全部失败则退化为直线，不占用栅格）。
> 端点在同一行/同一列时，Z 形的共线中间点会被化简掉，结果是一条轴对齐直线段
> （这本身就是正交路由的合法退化，不是错误）。
> 两条约定：同侧多条边按 (节点, 侧) 分组均匀分配锚点，避免共用同一格；锚点
> 吸附到整数格（字符网格上没有半格），绕行中点同样取整。

- `enum class RouteStyle`
- `struct RouteOptions`

### `graph/route/spline.hpp`

> spline.hpp —— 平滑曲线边路由（Catmull-Rom 穿过正交途经点）
>
> 与 route.hpp 的直线/正交一样，输出仍是"格坐标折线"，只是把折线换成平滑曲线：
> 以锚点之间的正交途经点当控制多边形，按弦长自适应采样 Catmull-Rom 曲线，再逐格
> 取整成 **8 连通格序列**（相邻格只差一行/一列/一格对角）。斜向步由既有
> graph/diagrams/internal.hpp 的 segment_char 渲染成 ╱ ╲，横向/纵向渲染成 ─ │，
> 因此本文件只产出几何、不产出字符（与 primitives.hpp 的分工一致）。
>
> 三条刻意约定：
> * **宁可折线也不穿框**：曲线一旦压到任何节点框内部（或已占用栅格），该候选直接
> 作废，回退到同一控制多边形对应的正交折线；正交也不通时回退直线。曲线在格粒度
> 下必然有台阶，回退折线比斜穿节点好。
> * 首末点严格等于锚点：控制多边形以锚点起止，采样后又显式钉死首末格，取整漂移
> 不会传到端点。
> * grid 为空时自建障碍栅格：不穿框是硬约定，不能因为调用方没给栅格就放弃避让
> （这点与 route_edges 不同）。
>
> 自环边（A→A）的锚点会重合，spline 不处理自环，请改用 graph/route/arc.hpp。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `graph/shape/shape.hpp`

> shape.hpp —— 节点形状（字符单元格粒度的轮廓）
>
> 形状只描述"哪些格构成边框、用什么字符"，不含填充与样式决策；绘制由 draw_shape
> 完成。轮廓格同时服务于两件事：绘制，以及占用/命中判定。
>
> 精度说明：这里全部是**字符格粒度**的解析轮廓（逐行算左右边界），不是子像素
> 光栅化。曲线（椭圆/胶囊端）在格粒度下必然有台阶，这是终端字符网格的固有限制，
> 不做假精度；需要更平滑的曲线请走 raster/（盲文/象限）那条路径。

- `class Canvas`
- `enum class ShapeKind`
- `TERMCHART_API void draw_shape(canvas::Canvas& canvas, ShapeKind kind, Rect box, const Style& style)`

### `graph/wiregrid.hpp`

> wiregrid.hpp —— 连线占位栅格（避免路由互相压线）
>
> 路由算法把候选线段先放进 WireGrid 探测：整段可用才真正占用。占用是
> **原子**的——中途碰到已占用的格，整段都不占用并返回 0，便于调用方直接换
> 下一条候选路径重试，不会留下半截占用。

- `class WireGrid`

## render（L14）

### `render/compositor.hpp`

> compositor.hpp —— 多图层合成（把若干 CellBuffer 按偏移叠加成最终帧）
>
> GUI / 重叠元素 / 多 Pass 渲染都需要"图层"概念：本组件把任意数量的图层
> （每个是一个 CellBuffer + 左上角偏移）合成到一张目标缓冲。后绘制的图层覆盖
> 先绘制的；transparent 时，空格单元不覆盖下层（透出底色）。

- `struct Layer`
- `class Compositor`

### `render/damage.hpp`

> damage.hpp —— 脏区域跟踪
>
> 与 diff.hpp 的分工：
> * diff.hpp 回答"两个 buffer 之间哪里变了"（纯函数，可测）；
> * 本文件维护"本帧累计的脏区"（有状态），供 renderer 决定重绘范围与
> 将来 canvas 的部分重绘。
> 存储形态：每行若干条**已合并**的半开区间 [begin, end)。

- `class DamageTracker`

### `render/diff.hpp`

> diff.hpp —— 差分重绘
>
> 纯函数层：输入两个 CellBuffer，输出"变化的连续段"。
> 合并规则：同一行内相邻/重叠的变化段合并成一段（多写一个字节的跳转序列
> 比多写十个字节的样式重置便宜得多）。

- `struct DiffRegion`
- `TERMCHART_API bool identical(const CellBuffer& previous, const CellBuffer& current) noexcept`
- `TERMCHART_API std::vector<DiffRegion> diff(const CellBuffer& previous, const CellBuffer& current, std::uint32_t merge_gap = 4) noexcept`

### `render/renderer.hpp`

> renderer.hpp —— 终端实时渲染管线（输出点）
>
> 终端实时路径的最后一环：CellBuffer → 增量字节流。
> * 持有 prev 状态与 damage 跟踪，因此**一帧只输出变化的部分**；
> * 与 snapshot 导出（backend/snapshot/*）的区别就在"有 prev 状态、有 diff"；
> * 本类只生成字节，不写 stdout —— 写由 api/print.hpp 或调用方负责，
> 于是渲染在 CI / headless / 单元测试里完全可用。

- `class Renderer`

### `render/sgr.hpp`

> sgr.hpp —— SGR（颜色 / 属性）序列的**唯一生成点**
>
> 阶段 1 的硬决策（ROADMAP R2）：所有 SGR 只从这里出去。
> * 终端实时路径（render/renderer.hpp）用它；
> * snapshot 导出（backend/snapshot/ansi_backend.hpp）也用它 —— 为此登记了
> 唯一收窄边 `backend/snapshot/ansi_backend > render/sgr`（见 cmake/TermchartModules.cmake）。
> 后果：改序列化规则只需改本文件，所有路径同步生效，不会出现"两套 SGR"。

- `TERMCHART_API void append_style(std::string& out, const Style& style, ColorDepth depth) noexcept`
- `TERMCHART_API std::string style_sequence(const Style& style, ColorDepth depth) noexcept`
- `TERMCHART_API void append_style_delta(std::string& out, const Style& from, const Style& to, ColorDepth depth) noexcept`
- `TERMCHART_API std::string reset() noexcept`
- `TERMCHART_API void append_attr_off(std::string& out, AttrMask off_bits) noexcept`
- `TERMCHART_API void append_attr_on(std::string& out, AttrMask on_bits) noexcept`
- `TERMCHART_API void append_color_params(std::string& out, const Color& color, bool foreground, ColorDepth depth) noexcept`

## interaction（L15）

### `interaction/brush.hpp`

> brush.hpp —— 刷选（数据点集的"笔刷式"吸附多选）
>
> 职责切分（为什么单开一个文件）：
> * 只做"指针途经了哪些点"这一件事：给定 (数据坐标, payload) 点集，按吸附
> 半径把落笔/拖动途经的点收进选中集。不碰终端、不碰渲染、不认识图表类型，
> 只依赖 L0 的 Point —— 与 zoom/pan/select 同族的纯几何交互组件。
> * "事件 → 手势"的路由（按下/拖动/松开映射到 begin/stroke/end）由上层或
> immediate.hpp 完成；本类只吃几何量，可在无终端的单测里完整驱动。
>
> 手势模型（三段：落笔 → 途经 → 提笔）：
> * begin(p)    进入刷选态，并按 p 吸附一次；
> * stroke(p)   仅刷选态有效：途经吸附（未 begin 时是 no-op，防呆）；
> * end()       退出刷选态；选中集**保留**供查询（像画笔提笔，画不擦）。
>
> 不变量（构造后与每次状态变更后都维持）：
> * selected() 的顺序 = 各 payload 的**首次吸附顺序**，且无重复：同一
> payload 对应的多个点、或同一点被反复途经，都只记一次；
> * 选中集只增不减（begin 不清空上一笔——"刷"是累积语义，多笔画选多个
> 区域后统一读出），唯一的减少途径是 clear()；
> * set_items() 随时可换点集，不改变当前选中集与手势状态（选中集按
> payload 记录，与点集解耦；被移除点的 payload 仍留在选中集里）。
>
> 退化输入（明确不吸附、不改选中集；无需特判，比较语义天然给出）：
> * radius 非有限（NaN/Inf）：半径平方为 NaN，所有距离比较为 false，
> 永不吸附；
> * radius = 0：仅与点精确重合时吸附（精确拾取）；radius < 0：平方后与
> |radius| 等价；
> * 指针坐标或点坐标非有限：距离比较为 false，不吸附、不崩溃。

- `struct BrushOptions`
- `class Brush`

### `interaction/crosshair.hpp`

> crosshair.hpp —— 十字准线（悬停读数覆盖层）
>
> 职责：把一次"悬停/定位"落成画布上的水平 + 垂直准线（可选轴侧读数），并在
> 下一次绘制或 clear() 时**精确还原**被改写的格 —— 否则悬停一次就永久污染画布
> （覆盖层必须能在撤销时把原字符与样式逐格放回去）。这是本类唯一的核心不变量。
>
> 与既有模块的关系：
> * 只依赖 L0/L10（core + canvas），不碰 render / term / 终端；
> * 不拥有画布，只在调用方传入的 Canvas 上叠加，故可在无终端单测里完整驱动。
>
> 叠加口径（默认"保留数据字符、只改样式/背景"）：
> * preserve_data == true（默认）：准线经过的格**保留 cp**，只把样式换成
> "原样式叠加 line_style"（line_style 里未指定的前景/背景保持原值，属性位按
> 或并入）。于是折线/柱状的数据字符不会被准线吃掉，只被高亮。
> * preserve_data == false：写入连接字符（交叉点 cross_cp，横线 horizontal_cp，
> 竖线 vertical_cp），整格换成 line_style。
>
> 裁剪与退化口径：
> * 准线**严格**裁剪在 plot_rect 内（半开区间 [x, x+w) × [y, y+h)）：区外一格不写；
> * at 落在 plot_rect 外，或 plot_rect 任一方向不足 2 格（退化成点/线段，失去
> "十字"语义）→ 明确不画、返回 false（对应"绘图区只有 1 格"的退化输入）；
> * readout 是**唯一**允许落在绘图区外的元素（画在轴侧，即绘图区下边框的下方），
> 仍由画布裁剪兜底；默认 show_readout == false，需要读数时显式打开——
> 这样"draw 只改绘图区内的格"在默认配置下是硬不变量。

- `struct CrosshairOptions`
- `class Crosshair`

### `interaction/detail/overlay_cells.hpp`

> overlay_cells.hpp —— 覆盖层的"改写格簿记"共享实现（interaction 模块私有 detail）
>
> 为什么单独抽出来：crosshair 与 tooltip 都要"记住被覆盖的格 → 精确还原"，此前
> 各自复制了一份（~25 行）。还原口径一旦分叉，就会出现"某个覆盖层悬浮一次永久污染
> 画布"的隐形 bug，且很难被单测发现。把唯一一份算法放这里，两个覆盖层共用。
>
> 为什么用模板而不是具体类型：两个覆盖层各自持有**私有嵌套**的 Patch 结构体
> （仅存 x/y/原格），以 Patch 作模板参数即可在**不改动各自公开头**的前提下复用；
> 本头从而不引入任何具体类型、不产生外部符号（纯模板 + inline）。
>
> 物理布局：本头位于 include/.../detail/，按仓库约定 header-only、无同名 .cpp。
> tools/check_header_only.py 只豁免**顶层** detail/，故本头已登记在 cmake/header_only.txt。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `interaction/dispatcher.hpp`

> dispatcher.hpp —— 事件分发（订阅顺序即优先级，被消费即停）

- `class Dispatcher`

### `interaction/event.hpp`

> event.hpp —— 输入事件变体（header-only）
>
> Event 用 std::variant 表达"按键 / 鼠标 / 尺寸 / 焦点 / 粘贴"五类事件；
> variant 的 index() 即 EventKind，与 EventKind 的枚举顺序一一对应。

- `struct ResizeEvent`
- `struct FocusEvent`
- `struct PasteEvent`
- `enum class EventKind : std::uint8_t`

### `interaction/focus.hpp`

> focus.hpp —— 焦点管理（在给定的可聚焦顺序上前后移动）

- `class FocusChain`

### `interaction/highlight.hpp`

> highlight.hpp —— 悬停高亮 + 锚点放大（纯状态，不吃 Event）
>
> 职责切分（为什么单开一个文件）：
> * "悬停到哪个数据点 + 以该点为中心看多大范围"是纯几何/纯状态问题，与终端、
> 渲染管线、具体图表类型统统无关，只依赖 L0 的 Point/Rect。
> * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
> set_hover/clear 调用。payload 是**不透明 id**（同 chart::HitRegion 的
> 契约），本文件只原样存取，不解释含义。
>
> 不变量（每次状态变更后都维持）：
> * active() == true 当且仅当最近一次 set_hover 之后未 clear；
> * active() == false 时 payload() 恒为 0（悬停无对象即无 id）；
> * options().magnify **原样保存**，不归一化——"<=1 或非有限视为不放大"
> 是 magnified_around 的判定规则，不是存储规则。
>
> 退化输入（明确不改状态）：
> * set_hover 传入非有限坐标：视为无效悬停，**不改任何状态**
> （此前的悬停保持有效，不会因一条坏事件被清掉）；
> * magnified_around：magnify <= 1 / 非有限、visible 为空矩形、或当前未
> 悬停（无锚点即无放大中心）时，**原样返回 visible**。

- `struct HighlightOptions`
- `class Highlight`

### `interaction/hit_test.hpp`

> hit_test.hpp —— 命中测试（只做几何，禁图表名）
>
> 约束（ROADMAP 阶段 3）：本文件**不得出现任何图表类型名**。它只调用
> chart::HitRegion 的几何接口（形状判定封装在 HitRegion::contains 内），
> 因此新增图表类型不需要改这里——这正是"通用 HitRegion"的意义。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `interaction/immediate.hpp`

> immediate.hpp —— 即时模式绑定（Event 流 → 交互组件的最小路由层）
>
> 职责切分（为什么单开一个文件）：
> * 只做路由与簿记：维护最近鼠标位置与按键按下集合，把 Event 流翻译成
> Viewport（滚轮缩放 + 拖拽平移）与 BoxSelect（框选三段）的状态变更。
> 不持有终端、不绘制、不认识图表类型；组件状态都在调用方手里
> （参照 Dear ImGui 的"每帧重放"：本类自身不沉淀任何图表状态）。
> * 复用既有组件而非重写：平移语义交给 PanGesture（drag_into 的
> "内容跟随指针"与未拖拽 no-op 被原样尊重），框选三段交给
> BoxSelect（select.hpp，begin/update/end），本文件只补"按键归属
> 哪个手势"的判定。
>
> 滚轮策略（为什么用滚轮动作而不是 Key '+'/'-'）：
> MouseEvent 没有 wheel 数值字段，但有 MouseAction::ScrollUp/ScrollDown
> —— 它们就是滚轮事件本身，比 Key 事件 '+'/'-' 更贴近 wheel_step
> "滚轮缩放因子"的语义，故采用滚轮动作：
> * ScrollUp   → zoom_at(+wheel_step, 鼠标位置)   （放大，看得更细）
> * ScrollDown → zoom_at(1/wheel_step, 鼠标位置)  （缩小，回看全局）
>
> 按键编号：以 mouse.hpp 的 MouseButton 为准（None=0/Left=1/Middle=2/
> Right=3）。bindings 的缺省值是刻意的：pan_button=1（左键拖拽平移），
> select_button=0（= None，即**缺省不启用框选**）——两者都缺省到左键会
> 让拖拽与框选互相打架，调用方要框选须显式给 select_button 分配按键
> （通常右键 3）。button == None 的 Press/Release 事件被整体忽略。
>
> 坐标系约定：feed() 不做"终端格坐标 → 数据坐标"换算，鼠标格坐标被原样
> 当作数据坐标喂给 Viewport/BoxSelect（与 pan.hpp 的纯几何约定一致）；
> 两者尺度不同时由调用方先行换算。
>
> 不变量：
> * mouse() 是最近一次鼠标事件的坐标（未收到过为 {0,0}）；
> * 按键按下集合只由 Press/Release 翻转，Move/Scroll 不改变按键状态；
> * 非鼠标事件（Key/Resize/Focus/Paste）不改变任何状态。
>
> 退化输入：
> * wheel_step 非有限或 <= 0：滚轮事件被整体忽略（不调用 zoom_at，
> 即使 zoom_at 自身也能拒绝这些值——双保险，见契约"非有限值不调用"）；
> * 未按下对应按键时，Move 不驱动视口/框选（PanGesture::drag_to 返回
> nullopt、BoxSelect::update no-op 的语义被尊重）。

- `struct ImmediateBindings`
- `class ImmediateContext`

### `interaction/input.hpp`

> input.hpp —— 原始输入字节缓冲（攒字节，解析交给 InputParser）

- `class Input`

### `interaction/input_parser.hpp`

> input_parser.hpp —— 字节流状态机：终端输入字节 → Event
>
> 解析 ASCII、UTF-8、CSI（方向键/功能键/Home/End/PageUp…）、SS3（F1–F4）、
> SGR 鼠标（ESC [ < b ; x ; y M/m）、OSC（跳过）、Alt+字符、Ctrl+字母。
> 不完整序列留在内部缓冲，等后续字节——这是流式解析的必备性质（read() 可能
> 把一条序列切成两半）。
>
> 但缓冲是**有上限的**（`kMaxPendingBytes`）：积压超过上限说明收到的不是"还没收完
> 的一条序列"，而是垃圾或恶意输入（例如 `ESC ]` 之后跟着无穷多个字节，永远等不到
> OSC 终止符）。此时**丢弃缓冲并跳到下一个 ESC 重新同步**，避免内存无界增长，也避免
> 把被丢弃序列的中段当成按键产出伪事件。
>
> 代价要说清楚：超过上限的**合法**长序列也会被丢弃（例如 iTerm2 内联图像的 base64
> 负载本身就可能大于 256 KiB）。本库的解析器本来就不产出 OSC 内容事件，所以常规用法
> 不受影响；若你的应用需要接收这种超长 OSC，请在应用层单独处理，或调大这个常量。

- `class InputParser`

### `interaction/key.hpp`

> key.hpp —— 按键定义（header-only 值类型）

- `enum class Key`
- `struct KeyEvent`

### `interaction/legend_toggle.hpp`

> legend_toggle.hpp —— 图例点击切换
>
> 职责切分：图例的**画**归渲染侧（chart/backend 想怎么画都行），本文件只管
> "点了图例之后系列可见性怎么变"。交互层不知道"系列"是什么——它只维护
> id → 可见性 的映射，而 id 的语义由登记方（把图例项的 HitRegion payload
> 设成 series id 的人）决定。这与 hit_test 的"不透明 payload"约定一致。
>
> 不变量：
> * 未登记的 id 视为可见（visible() 返回 true）——这样新增系列不需要先登记；
> * click() 只在 id 已登记且状态真的翻转时返回 true（供调用方决定要不要重绘）；
> * hidden() 的顺序与登记顺序一致（渲染侧可以稳定地画"已隐藏"角标）。
>
> 退化输入：payload == 0 是 HitRegion 的默认值，**不**当作合法系列 id
> （登记时 id 为 0 会被拒绝），避免"点中了没登记的图元却切掉了某个系列"。

- `class LegendToggle`

### `interaction/mouse.hpp`

> mouse.hpp —— 鼠标事件（header-only 值类型）

- `enum class MouseButton`
- `enum class MouseAction`
- `struct MouseEvent`

### `interaction/pan.hpp`

> pan.hpp —— 平移手势（纯几何拖拽状态机）
>
> 为什么与 zoom.hpp 分开：缩放是"换视口"的状态变换，拖拽是"把指针运动折算成位移"
> 的输入状态机，两者生命周期与依赖都不同（拖拽要记按下/释放）。
>
> 只吃几何量：本文件不碰终端、不碰渲染，也不做"终端格坐标 → 数据坐标"换算——
> 那步由上层用 Viewport 的映射函数完成。这样手势可在无终端的单测里完整驱动。
>
> 方向约定：拖拽采用"内容跟随指针"（像抓住地图拖动）。指针位移 (dx,dy) 对应视口
> 平移 (-dx,-dy)；drag_to() 返回**原始指针增量**，drag_into() 已封装这个取反。

- `class PanGesture`

### `interaction/select.hpp`

> select.hpp —— 框选状态机（按下 → 拖动 → 松开/取消，纯几何，不吃 Event）
>
> 职责切分（为什么单开一个文件）：
> * "拖出一个正矩形并回报框内数据点的 id"与终端、渲染管线、具体图表类型
> 统统无关，只依赖 L0 的 Point/Rect 与不透明 payload id。
> * 本文件不直接消费 Event：由上层（immediate/调用方）把鼠标事件翻译成
> begin/update/end/cancel 调用。
>
> 状态机（两态：空闲 / 拖拽中）：
> * begin(start)      空闲 → 拖拽中；**拖拽中重复 begin 视为重新起笔**，
> 起点重置、已积累的矩形作废；
> * update(current)   仅拖拽中有效：更新当前点并使矩形可查询；
> 空闲时是 **no-op**（防呆：不允许凭空"继续拖"）；
> * end()             拖拽中 → 空闲；矩形**保留**供查询（松开后仍要高亮框内点）；
> 空闲时是 no-op；
> * cancel()          任何态 → 空闲并**清空矩形**（ESC 语义：什么都别选）。
>
> 不变量：
> * has_rect() == true 当且仅当"拖拽中发生过至少一次 update"且此后未 cancel
> （纯点击、只 begin 未拖动不算矩形——零面积矩形没有框选意义）；
> * rect() 始终是归一化正矩形（x/y 为左上角、宽高非负），与 Rect 约定一致；
> * active() == true 时 has_rect() 可以为 false（刚 begin 还没动）。
>
> 边界判定：payloads_inside 与 Rect::contains 的闭-开语义一致
> （含左/上边界，不含右/下边界），从而与 HitRegion 的矩形命中可互推。

- `class BoxSelect`

### `interaction/tooltip.hpp`

> tooltip.hpp —— 悬停数据标签（带边框的小浮层）
>
> 职责：给定锚点格 + 文本，在画布上画一个带边框、可多行的浮层；并在 clear() 或
> 下一次 draw() 时**精确还原**被改写的格（否则悬浮一次就永久污染画布）。
>
> 定位规则（复用 layout/label_avoid 的既有思路/实现）：
> * 候选顺序：右上 → 左上 → 右下 → 左下（角位，而非 label_avoid 的边位）；
> * "可用"的判定**直接调用** layout::place_label_avoiding —— 把候选矩形反推成它
> 的"右"候选来做探测，因此重叠判定、gap、半开区间与越界规则完全复用，不另写一套；
> * 锚点格恒计入占用集，故浮层**永不覆盖锚点**；
> * 四角都不可用 → 返回 placed=false 且**不改画布**（对应"宽度不足以放任何浮层"
> 与"锚点在画布外"的退化输入），而不是硬塞。
>
> 宽度：按 util::display_width（宽字符算 2 列）计算；超过 max_width 时按显示宽度
> 截断并追加省略号（用 prefix_by_width，绝不切在组合记号中间），既有边框也不会被撑破。

- `struct TooltipOptions`
- `struct TooltipPlacement`
- `class Tooltip`

### `interaction/zoom.hpp`

> zoom.hpp —— 视口（数据坐标里的可见矩形）与缩放
>
> 职责切分（为什么单开一个文件）：
> * 图表的"缩放"本质是"在数据坐标里换一个更小/更大的可见矩形"，这与终端、
> 渲染管线、具体图表类型统统无关，故只依赖 L0 的 Point/Rect，谁都能用。
> * 平移（含拖拽手势）另见 pan.hpp；本文件只负责"视口 + 缩放"这一半。
>
> 不变量（构造后与每次状态变更后都维持）：
> * visible() 始终完全落在 data_bounds 内 —— 不会露出界外空白；
> * 任一方向的可见跨度 ∈ [min(min_span, data 该方向跨度), data 该方向跨度]。
> 即 data_bounds 比 min_span 还窄时以 data_bounds 为准（数据界优先于最小跨度）。

- `struct ViewportOptions`
- `class Viewport`

## backend（L16）

### `backend/backend.hpp`

> backend.hpp —— 所有后端的抽象基类（begin/end 生命周期）
>
> 后端的铁律（见 ROADMAP 阶段 3 约束）：**不做跨渲染模型转换**。
> 输入是 CellBuffer、Scene 还是 Document，由子接口在类型上钉死：
> * backend/snapshot/*    ← CellBuffer
> * backend/vector/*      ← Scene
> * backend/structured/*  ← Document
> 因此 backend 不需要认识 chart 类型（backend > chart 是禁止边）。

- `class Backend`

### `backend/snapshot/ansi_backend.hpp`

> backend/snapshot/ansi_backend.hpp —— CellBuffer → 完整 ANSI 字节流
>
> **唯一**允许 include render/sgr 的 backend 文件（收窄边，见 TermchartModules.cmake）。
> 一次性、无 diff、无 prev 状态、可重入；不自建 SGR——序列化规则改动只在 sgr.hpp。

- `class AnsiBackend final : public SnapshotBackend`

### `backend/snapshot/backend.hpp`

> backend/snapshot/backend.hpp —— 快照物种子接口（CellBuffer → bytes）
>
> 快照导出是一次性的：无 diff、无 prev 状态、可重入。所有具体后端（ansi/text/
> html_pre/json）都只是把同一个 CellBuffer 用不同字节流表达。

- `struct SnapshotOptions`
- `class SnapshotBackend : public Backend`

### `backend/snapshot/html_pre_backend.hpp`

> backend/snapshot/html_pre_backend.hpp —— CellBuffer → <pre> 包装的 HTML
>
> 只做最小转义（& < >），颜色/属性留白——需要彩色 HTML 用 backend/structured。

- `class HtmlPreBackend final : public SnapshotBackend`

### `backend/snapshot/json_backend.hpp`

> backend/snapshot/json_backend.hpp —— CellBuffer + 属性 → 结构化 JSON
>
> 逐格序列化：码点、UTF-8 字符、前/背景（含 kind/index/rgb）、属性位掩码。
> 供机器消费（比如把终端产物喂给前端做 diff 比对）。

- `class JsonBackend final : public SnapshotBackend`

### `backend/snapshot/png_screenshot.hpp`

> backend/snapshot/png_screenshot.hpp —— CellBuffer → PNG 截图
>
> 复用 image/png_encode 的 encode_png（零依赖 stored deflate），不重复实现
> PNG 编码。每个字符格映射为 cell_w×cell_h 的像素块：整块先填背景色
> （Style 未设色用 ResolveOptions 的默认背景），非空格再整块覆盖前景色。
> 前景是"块状示意"而非字形光栅化——逐字形渲染需要字体栅格化器，超出
> 本后端职责（要带字形请走 protocol/ 的 sixel/kitty 路径）。
>
> 注：继承 SnapshotOptions 里的 depth / trim_trailing_blank 对 PNG 无意义，
> 保留只是为了共用快照物种的生命周期与 render_snapshot 便捷入口。

- `class PngScreenshotBackend final : public SnapshotBackend`

### `backend/snapshot/text_backend.hpp`

> backend/snapshot/text_backend.hpp —— CellBuffer → 纯文本（无 SGR）

- `class TextBackend final : public SnapshotBackend`

### `backend/structured/backend.hpp`

> backend/structured/backend.hpp —— 结构化物种子接口（Document → bytes）
>
> 结构化导出把 Document（语义块）翻成 HTML/Markdown 等"带结构"的文本格式。
> 输入是 Document 而非 chart：backend > chart 是禁止边，这就是它成立的原因。

- `struct StructuredOptions`
- `class StructuredBackend : public Backend`

### `backend/structured/html_backend.hpp`

> backend/structured/html_backend.hpp —— Document → HTML

- `class HtmlBackend final : public StructuredBackend`

### `backend/structured/markdown_backend.hpp`

> backend/structured/markdown_backend.hpp —— Document → Markdown

- `class MarkdownBackend final : public StructuredBackend`

### `backend/vector/backend.hpp`

> backend/vector/backend.hpp —— 矢量物种子接口（Scene → bytes）
>
> 矢量导出把 Scene（非完备图形 IR）翻成某种向量格式。与 snapshot 不同，这里
> 输入是**保留几何**的 Scene，因此能无损缩放（SVG viewBox / TikZ 坐标）。
> 后端不做跨渲染模型转换：Scene 进来，字节流出去。

- `struct VectorOptions`
- `class VectorBackend : public Backend`

### `backend/vector/latex_backend.hpp`

> backend/vector/latex_backend.hpp —— CellBuffer → LaTeX（TikZ）
>
> 与 svg_backend 同款接口形态：Options 结构 + render 入口 + bytes()/str() 返回
> 字符串 + 便捷函数 render_latex（对齐 render_vector / render_snapshot）。
> 差异只在输入物种：按 backend/backend.hpp 的铁律"不做跨渲染模型转换"，
> 快照导出的输入恒为 CellBuffer，因此这里不继承 VectorBackend（它的 render
> 把输入钉死在 Scene），而是直接实现 Backend 生命周期。
>
> 映射口径：
> * 坐标：格 (x, y) → TikZ (x, -y)；单位由 tikzpicture 的 x/y 选项定成
> scale pt，y 翻转让第一行落在最上方（TikZ 坐标向上为正）。
> * 非空格（空白格与宽字符续接格不输出）→ \node，文字经 LaTeX 转义。
> * 横向连续的 U+2500（─）→ \draw 直线段（格中心连线）；
> U+2588（█）→ \fill 实心矩形（左右各扩半格，视觉上无缝）。
> * 颜色：Style 前景色经 color::resolve 解析成 RGB，用 xcolor 的
> {rgb,255:red,R;green,G;blue,B} 模型内联；None 不发色键，沿用默认黑。

- `struct LatexOptions`
- `class LatexBackend final : public Backend`

### `backend/vector/pdf_backend.hpp`

> backend/vector/pdf_backend.hpp —— Scene → PDF（极简 PDF 1.4）
>
> 设计目标：与 svg_backend 同款接口形态（Options + render + bytes()/str()）。
> 范围：
> * Scene 的子集：MoveTo / LineTo / RectOp (fill+stroke) / FillOp / StrokeOp /
> TextOp（仅 ASCII / Helvetica 字族，不嵌入字体）；
> * 真彩 RGB（颜色类型 / PDF Color Operators rg/RG）；
> * 内嵌 PNG（走 image::Bitmap → encode_png → /Filter /FlateDecode）；
> * 单页（多页留给后续切片，初始只支持 1 页）；
> * 文本流：仅 BaseFont / Helvetica（PDF 14 标准字体之一，零嵌入）。
>
> 故意不写：
> * 圆弧 / 贝塞尔（PDF 支持但与 Scene 圆弧表示不一致，留 TODO）；
> * 透明度 / 混合模式；
> * 字体嵌入（用标准字体零成本，但 ASCII only）。
>
> 输出：完整 PDF 字节流（%PDF-1.4 + objects + xref + trailer），可在浏览器/Acrobat 打开。

- `struct PdfOptions`
- `class PdfBackend final : public VectorBackend`

### `backend/vector/svg_backend.hpp`

> backend/vector/svg_backend.hpp —— Scene → SVG

- `class SvgBackend final : public VectorBackend`

### `backend/vector/svg_entities.hpp`

> svg_entities.hpp —— SVG 序列化原语（转义 / 颜色 / 数字格式）
>
> header-only：纯函数，无状态。颜色复用 color/ 的调色板换算（from_index_16 /
> from_index_256），因此 SVG 与终端两条路径对同一 Color 的观感一致。

- （无命名空间级声明，或为纯宏 / 伞形头）

## dsl（L17）

### `dsl/ast.hpp`

> ast.hpp —— DSL 解析结果的中立模型（L17）
>
> 设计要点：AST **不依赖任何绘图类型**，但字段与 `graph::Graph` 一一对应，
> 于是"方言 → AST → Graph"是一条没有信息损失的单向路（见 to_graph）。
>
> 为什么不让方言直接吐 Graph：
> 1. 同一张 AST 可以被吐成图、表格、树三种产物（table_markdown / csv_sniff 也要用）；
> 2. 解析期的诊断需要带原文位置，而 Graph 里没有位置概念；
> 3. 后续 DSL（plantuml / d2）的差异可以只在 AST 上体现，下游零改动。

- `enum class ArrowKind : std::uint8_t`
- `struct AstAttribute`
- `struct AstNode`
- `struct AstEdge`
- `struct AstGraph`

### `dsl/csv_sniff.hpp`

> csv_sniff.hpp —— CSV 解析与列类型推断（L17）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 自定义单字符 delimiter；表头可选（has_header=false 时列名为 col1..colN）
> ✅ 引号字段 "..."：内含分隔符与换行；"" 表示一个引号；未闭合 → ParseError
> ✅ 开头 UTF-8 BOM 跳过（首个列名不带 BOM）
> ✅ 列类型推断：Boolean → Integer → Real → Date → Text；整列空 → Empty
> ⛔ 认出但忽略：闭合引号后到分隔符/行尾的剩余字符按字面并入字段（宽松，不报错）；
> 引号字段内的 CR/LF 原样保留
> ⛔ 本切片不支持：多字符分隔符、注释行、`sep=` 指令、反斜杠转义
> （CSV 只认引号翻倍）、delimiter 之外的方言自动识别
>
> 行/列策略：完全空白的行被跳过；推断时缺失的列按"空"计。
> 类型判定顺序（先命中先定；列内所有非空样本须全部满足）：
> Empty（无非空样本）→ Boolean → Integer → Real → Date → Text。
> Boolean：true/false/yes/no/0/1（大小写不敏感）；
> Integer：可选正负号 + 纯数字；Real：十进制 / 科学计数法；
> Date：YYYY-MM-DD，可选 (空格|T) HH:MM(:SS)。
> 样本前先裁剪首尾空白；sniff_rows==0 表示用全部数据行，否则只用前 N 个数据行。
>
> 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic；
> 未闭合引号的位置指向**起始引号**。

- `enum class ColumnType : std::uint8_t`
- `struct ColumnProfile`
- `struct CsvOptions`

### `dsl/d2.hpp`

> d2.hpp —— D2 语法（L17）
>
> ✅ 支持
> `name: Type` 节点声明（`Type` 映射到图元形状，原值记进节点属性 `d2_shape`）
> `name: { … }` 块：容器节点（属性 `container=true`），块内子节点记 `parent`
> `label: 文字` / `shape: 圆` / `width|height|near|tooltip|icon|link|classes: v`
> —— 作用于当前块（或顶层最近声明的节点）
> `name.prop: value` 形式（同上，但显式指定节点）
> 连接：`a -> b`、`a <- b`、`a -- b`（无箭头）、链 `a -> b -> c`、`a -> b: 标签`
> `direction: right|left|up|down` → AstGraph::direction（LR/RL/BT/TB）
> ⛔ 认出但忽略：`style: { … }` 块（连同其 `}`）、`#` 注释、未知键值对
> ⛔ 不支持：`...` 常量节点、泛型 `**a**` 强调语法、SQL/class/markdown 等嵌入块、
> `a -> b: { label: x }` 这种"边上的属性块"（按普通边解析，标签取不到）
>
> 与 graphviz_dot 的关系：两者都是"文本 → 图"，但 D2 的**默认形状语义**不同
> （`name: Type` 是形状声明而不是属性），因此不复用 DOT 的解析器。

- `struct D2ParseOptions`

### `dsl/dsl_registry.hpp`

> dsl_registry.hpp —— 方言注册表（L17）
>
> 与 chart / raster / scale 注册表同构（docs/design/registry.md，五注册表统一模式）：
> * `instance()` 用 **function-local static**，规避静态初始化顺序问题；
> * 注册表不持有对象所有权，只持有**函数指针 + 字符串视图**（方言入口是纯函数）；
> * 注册的唯一安全时机是 `App` 构造之前；`App` 构造时做浅拷贝快照并冻结；
> * 内置方言由 `register_builtin_dsls()` 在静态初始化期登记（幂等，std::call_once 守卫）。
>
> 与另四个注册表的差别：方言入口签名是"纯函数"，因此描述符是 trivially copyable 的，
> 快照即一次 vector 拷贝——没有工厂对象生命周期问题。

- `struct DslOptions`
- `struct DslDescriptor`
- `class DslRegistry`
- `TERMCHART_API bool register_builtin_dsls() noexcept`

### `dsl/graphviz_dot.hpp`

> graphviz_dot.hpp —— Graphviz DOT 语法（L17）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 头部 `digraph [名字] {` / `graph [名字] {`（`strict` 前缀忽略）；`}` 收尾（缺失 → ParseError）
> ✅ 方向：`rankdir=LR|TB|BT|RL` / `rankdir="LR"`，以及属性行 `graph [rankdir=LR]`
> （值统一转大写写进 AstGraph::direction；DOT 默认 TB，故未写时留空，由渲染侧取默认）
> ✅ 节点：`a;`、`a [label="文字", shape=box]`、多条属性列表 `a [k=v][k2="v2"]`
> （`label`→AstNode::label；`shape` 认 box/ellipse/diamond/hexagon/cylinder/round，
> 其余 shape 原样落进 AstNode::attributes，不静默丢弃）
> ✅ 边：`a -> b`（有向）、`a -- b`（无向）、链 `a -> b -> c`、`a -> b [label="x"]`
> （边属性 `style=dashed|dotted`→line_style="dashed"，`style=bold`→"thick"，
> `dir=both`→ArrowKind::Both）
> ✅ 并列节点集：`a -> {b c}`、`{a b} -> c`、`a -> {b c} -> d`（按笛卡尔积展开为多条边）
> ⛔ 认出但**忽略**（不产生节点/边）：
> `strict`；`graph/node/edge [attr]` 默认属性行（`graph` 行里的 rankdir 仍会被取用）；
> `subgraph 名称 { … }` 块（块头与 `}` 被吞掉，块内语句按顶层继续解析，不建分组属性）；
> `id = value` 形式的图属性赋值（仅 `rankdir` 有效，其余丢弃）
> ⛔ **不支持**（不做或降级，且不假装支持）：
> HTML 风格标签 `<…>`（会被当作标点拆散，含它的语句报 ParseError）；
> 端口 `a:port`（端口被跳过、port 名丢失）；一个文件里多张图（只解析第一张）；
> `->` 之外带属性语义的复杂边（如 `dir=none` 之外的方向微调）
>
> 为什么这样切：DOT 的属性列表允许任意 k=v 且值可含空格/标点/中文，按 token 拼接极易出错，
> 因此属性一律用**原文切片**读取（`[` 到配对 `]` 之间），再在切片上按逗号/等号切分——
> 于是 `label="a -> b"` 这类"引号里长得像箭头"的内容天然不会被误解。
>
> 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
> 可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。

- `struct DotParseOptions`

### `dsl/json.hpp`

> dsl/json.hpp —— JSON 解析入口（compat shim，L17）
>
> 实现已于 2026-10-21 下沉收编至 io/json.hpp（L2，"数据入口归 io"）。本头保留
> 原 dsl 命名空间的入口语义：dsl::JsonValue / dsl::JsonOptions / dsl::parse_json
> 均为 io 命名空间对应符号的别名，既有调用点与测试零改动。
> 新代码请直接 include "termchart/io/json.hpp"。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `dsl/lexer.hpp`

> lexer.hpp —— DSL 词法分析器（L17）
>
> 各 DSL（mermaid / dot / plantuml / d2）的**词法骨架是同一个**：
> 标识符、字符串、数字、箭头、标点、换行、注释。差异只有两处，都以参数表达：
> * 关键字集合（方言自定义）；
> * 注释前缀（mermaid "%%"、dot "//" 与 "#"、plantuml "'"）。
>
> 契约：
> * **永不抛异常**；非法输入产出 TokenKind::Invalid 并带位置，由 parser 决定是否致命；
> * 位置（行列）从 1 开始，用于诊断信息，不参与语义；
> * 长度为 0 的 token 只可能是 End（防止调用方死循环）。

- `enum class TokenKind : std::uint8_t`
- `struct Token`
- `class KeywordSet`
- `struct LexerOptions`
- `class Lexer`

### `dsl/mermaid.hpp`

> mermaid.hpp —— Mermaid 总入口（L17）
>
> 职责只有两件：
> 1. **识别**首行指令，判断这是哪种图（docs/guide/dsl_reference.md 的表格口径）；
> 2. 分发到对应方言解析器。
>
> 分发去处的产物类型不同，这是刻意的：
> flowchart / classDiagram / stateDiagram → `AstGraph`（可 to_graph 落到 graph::Graph）
> sequenceDiagram                        → **不在这里**：序列图是时间轴模型，
> 见 mermaid_sequence.hpp 的 `parse_sequence()`；本入口对它返回 `Unsupported`
> 并在诊断里说明原因，**绝不假装解析成功**。
>
> 自动识别规则（首个非空、非注释行）：
> flowchart / graph   → Flowchart
> sequenceDiagram     → Sequence
> classDiagram        → Class
> stateDiagram(-v2)   → State
> 其它                → Unknown（由调用方决定是报错还是继续探测别的方言）

- `enum class MermaidDiagram : std::uint8_t`
- `struct MermaidOptions`

### `dsl/mermaid_class.hpp`

> mermaid_class.hpp —— Mermaid classDiagram（L17）
>
> 类图是"图 + 节点内容"，因此产物仍是 `AstGraph`：
> * 类 = 节点，类体与方法逐行原文进节点属性 `member`（多条）；
> * 关系 = 边，**语义写进边属性 `relation`**：inheritance / composition /
> aggregation / association / dependency / realization；
> * 布局仍交给 `graph::layout`，渲染交给 `graph/diagrams`（本层不画东西）。
>
> ✅ 支持
> `classDiagram` 头部；`direction LR`
> `class 名称`、`class 名称 { … }`、`class 名称 as 别名`、`class 名称["显示名"]`
> `A <|-- B`、`A --|> B`、`A *-- B`、`A o-- B`、`A --> B`、`A ..> B`、`A ..|> B`
> `A --> B : 标签`、`A "1" --> "*" B`（多重度进边属性 `from_multiplicity` / `to_multiplicity`）
> `A : +成员()`（单行成员补写）
> `<<interface>>` / `<<abstract>>` 构造型 → 节点属性 `stereotype`
> ⛔ 认出但忽略：`note for`、`style`/`classDef`/`class` 着色语句、`namespace`/`package`（记 `namespace` 属性）
> ⛔ 不支持：泛型 `~T~` 的语义解释（原文保留在成员文本里）、关系方向的双箭头细节
>
> 方向语义（最容易搞反，测试锁死）：
> `A <|-- B` → B 继承 A，边 from=B to=A（三角指向父类 A）
> `A --|> B` → A 继承 B，边 from=A to=B

- `struct ClassParseOptions`

### `dsl/mermaid_flow.hpp`

> mermaid_flow.hpp —— Mermaid flowchart 语法（L17）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 头部 `flowchart TD` / `graph LR` / `graph G { }`（图名与方向）
> ✅ 节点：裸引用 `A`、`A[标签]`、`A(标签)`、`A([标签])`、`A((标签))`、
> `A{标签}`、`A{{标签}}`、`A>标签]`
> ✅ 边：`-->` `---` `-.->` `-.-` `==>` `===` `<-->` `--x` `--o`
> ✅ 边标签两种写法：`A -->|文字| B` 与 `A -- 文字 --> B`
> ✅ 边链：`A --> B --> C`
> ✅ 子图：`subgraph 名称` … `end`（子图名记为节点属性 `group`，不做嵌套布局）
> ⛔ 忽略但**不报错**的指令：`classDef` / `class` / `click` / `style` / `linkStyle` /
> `direction`（后者的值仍会被记录到 AstGraph::direction）
> ⛔ 本切片不支持的：节点 `:::class` 后缀、`&` 并列节点、`--o`/`--x` 的
> 方向语义细节（统一按 AtTarget 处理）
>
> 诊断：解析失败返回解析器的错误码，可选地把首个诊断写到 out_diagnostic。

- `struct FlowchartParseOptions`

### `dsl/mermaid_sequence.hpp`

> mermaid_sequence.hpp —— Mermaid sequenceDiagram（L17）
>
> **为什么这里不是 AstGraph**：序列图的时间轴语义（消息顺序、激活、生命线）
> 无法用"节点 + 边"无损表达——硬塞进去会得到一个既不是图、也不能画序列图的
> 半成品。这与本库对"三物种后端不做跨渲染模型转换"的态度一致：
> 模型不匹配就承认，不硬套。
>
> 因此本方言导出**自己的类型** `AstSequence`，并且**不登记进 DslRegistry**
> （注册表的契约是 `Result<AstGraph>`，见 dsl_registry.hpp）。序列图渲染器落地时
> 直接消费 `AstSequence` 即可，不需要改动本层。
>
> ✅ 支持
> `sequenceDiagram` 头部
> `participant A as 别名` / `actor A` / `database A`
> `A->>B: 文本`、`A-->>B: 文本`（虚线）、`A-)B`（开口）、`A-xB`（叉）、
> `A->B` / `A-->B`（无箭头），后接 `+` / `-` 的激活简写
> `activate A` / `deactivate A`（记到**上一条消息**的 `activate`/`deactivate`）
> `Note left of A: 文本` / `Note right of A: …` / `Note over A,B: …`
> `%%` 注释
> ⛔ 认出但忽略：`loop`/`alt`/`else`/`opt`/`par`/`end` 等块语句（不展开嵌套），
> `autonumber`、`title`、`box`、`link`、`links`、`participant` 的样式参数
> ⛔ 不支持：`create`/`destroy` 参与者、`break`、递归激活栈的精确配对

- `enum class MessageKind : std::uint8_t`
- `struct SequenceParticipant`
- `struct SequenceMessage`
- `struct SequenceNote`
- `struct AstSequence`
- `struct SequenceParseOptions`

### `dsl/mermaid_state.hpp`

> mermaid_state.hpp —— Mermaid stateDiagram（L17）
>
> 状态机天然是**图**（状态=节点、迁移=边），因此本方言的产物就是 `AstGraph`，
> 可以直接 `to_graph()` 落到 `graph::Graph` 复用分层布局与正交路由。
>
> ✅ 支持
> `stateDiagram` / `stateDiagram-v2` 头部、`direction LR` 等方向
> `A --> B`、`A --> B : 事件[守卫]/动作`、`A -- B`（等价于 -->）
> `[*] --> A`（A 记为初始态，节点属性 `initial=true`）
> `A --> [*]`（A 记为终态，节点属性 `final=true`）
> `state X { … }` 组合态：X 记 `composite=true`，块内状态记 `parent=X`（不做嵌套布局）
> `state X as 显示名`、`state "显示名" as X`
> `note left of X : …` / `note right of X : …` → 节点属性 `note`
> ⛔ 认出但忽略（不影响图结构）：`note` 的其它形态、`classDef`/`class`/`style`/`click`
> ⛔ 不支持：`--` 上的并发区域（`--` 分行）、`<<choice>>`/`<<fork>>` 等构造型语义

- `struct StateParseOptions`

### `dsl/parser_base.hpp`

> parser_base.hpp —— 递归下降解析框架（L17）
>
> 所有方言共用同一套"读 token 的手感"：peek/advance/match/skip_line + 诊断记录。
> 设计取向（与 core/error.hpp 的错误策略一致）：
> * 解析**不抛异常**：错误记录进 ParseDiagnostic，由方言入口转成 Result 的错误码；
> * 只保留**第一个**错误（后续错误往往是它引发的连锁噪声）；
> * 关键不变量：任何循环都必须保证 index 前进，否则 fuzz 输入会让解析器挂死。

- `class ParserBase`

### `dsl/plantuml.hpp`

> plantuml.hpp —— PlantUML 类图语法（L17）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 包裹：`@startuml [名字]` … `@enduml`（任一端缺失 → ParseError）
> ✅ 注释：`'` 行注释、单行 `/' … '/` 块注释（多行块注释见"不支持"）
> ✅ 类声明：`class 名称`、`class 名称 as 别名`、`class 名称 { …体… }`，
> 另把 `interface` / `enum` / `abstract class` 当同类处理（都建一个节点）
> ✅ 类体：`{` … `}` 内**每一行原文**落进该节点 attributes（键 "member"，可多条），
> **不解析**成员语法（可见性/类型/方法一律原样保留）
> ✅ 关系：`-->`、`--`、`--|>`、`<|--`、`..>`、`..|>`、`*--`、`o--`（另收 `--*`/`--o`/`<|..`）
> —— 语义写进 AstEdge::attributes 键 "relation"：
> inheritance / composition / aggregation / association / dependency；
> 虚线（`..`）写进 AstEdge::line_style="dashed"；箭头统一用 ArrowKind；
> `A <|-- B` 这类"箭头在左端"的写法会**交换** from/to（B 继承 A ⇒ from=B,to=A）
> ✅ 关系标签 `A --> B : label`（取 `:` 到行尾的原文）
> ✅ 多重度 `A "1" --> "*" B`（写进 edge attributes 键 from_multiplicity / to_multiplicity）
> ⛔ 认出但**忽略**（不改变图结构）：`note`（单行 `note … : …` 与块 `note …`/`end note`）、
> `skinparam`、`title`/`caption`/`legend`、`package`/`namespace` 块头（块内语句仍按顶层解析）、
> 以及一切既不是类声明也不是已识别关系的行——**整行跳过、继续**
> ⛔ **不支持**：多行 `/' … '/` 块注释（词法层按行注释处理，内部行会被当作未识别行）、
> 泛型 `Foo<T>`、构造型 `<<…>>`、`hide/show` 的过滤语义、组件/时序/活动等其他 PlantUML 图族
>
> 为什么宽松：PlantUML 方言极杂且大量语法与布局有关。这里只承诺"类图的关系骨架"，
> 其余一律不猜——**认不出就整行跳过**，绝不让未识别的行静默改变已解析出的节点/边。
>
> 诊断：失败返回解析器错误码（ParseError 语法错 / OutOfRange 超 max_nodes），
> 可选地把首个诊断写到 out_diagnostic；任何路径都**不抛异常**。

- `struct PlantUmlParseOptions`

### `dsl/table_markdown.hpp`

> table_markdown.hpp —— GitHub-Flavored Markdown 管道表格（L17）
>
> 支持范围（**明确列出，避免"以为支持"**）：
> ✅ 表头行 + 分隔行（`---` / `:---` / `---:` / `:---:`）+ 若干数据行
> ✅ 行首尾的 `|` 可省；单元格前后空白被裁剪
> ✅ 单元格内 `\|` 还原为字面 `|`；反引号代码段内的 `|` 不作列分隔
> ✅ 对齐信息（None/Left/Center/Right）经 TableAlignment 出参返回
> ⛔ 认出但忽略：除 `\|` 与 `\\` 外的反斜杠转义按原文保留，不做解释；
> 未闭合的反引号按普通文本处理（不报错）
> ⛔ 本切片不支持：HTML `<table>`、单元格内换行、colspan/rowspan、嵌套表格
>
> 列数策略：**以表头列数为准**。数据行多出的列被裁剪、缺失的列补空串；
> 分隔行必须与表头列数一致，否则按解析错误处理（GFM 要求相等）。
> 表格在首个空行或首个不含 `|` 的行处结束。
>
> 诊断：解析失败返回错误码，可选地把**首个**诊断写到 out_diagnostic。

- `struct TableAlignment`
- `struct MarkdownTableOptions`

## widget（L18）

### `widget/border.hpp`

> border.hpp —— 即时模式边框绘制（header-only，无状态）
>
> 与 immediate_widget.hpp 同风格：不持有状态，调用方传入 canvas + rect +
> 主题，每次调用重描完整边框。box-drawing 字符取自 Unicode，配色用
> ThemePalette.border。矩形不足 2 格宽/高时退化为单边（不崩、不画四角）。

- `enum class BorderStyle`

### `widget/container.hpp`

> container.hpp —— 容器基类：控件树中枢（保留模式几何 + 命中测试）
>
> 与 widget.hpp 的设计一致：Widget 只管身份/几何/命中测试，绘制走 immediate_widget
> 的即时函数。Container 在此基础上持有子控件（裸指针，生命周期由调用方管理），
> 提供递归命中测试与统一的 layout 契约。它是布局控件的公共基类。

- `enum class Alignment`
- `class Container : public Widget`

### `widget/controls.hpp`

> controls.hpp —— 无状态即时组件：下拉框 / 单行文本框 / 单选组
>
> 模型与 immediate_widget.hpp（按钮/复选框）完全一致：
> * **不持有状态**：开合、高亮项、光标、选中项全部由调用方持有，每帧传入；
> * **返回"意图"不返回"沿"**：hovered+pressed 即报告点击/提交；调用方比对
> 自己上一帧收到的返回值即可得到沿；
> * **事件与几何正交**：退化几何只跳过绘制，不影响返回值；
> * 文本一律按 UTF-8 码点操作（光标按码点移动、退格删整个码点），
> 宽度按主码点数估（宽字符按 1 格计）。

- `enum class DropdownIntent : std::uint8_t`
- `struct DropdownInput`
- `struct DropdownResult`
- `struct TextBoxInput`
- `struct TextBoxResult`

### `widget/dock.hpp`

> dock.hpp —— 五区停靠布局（top/bottom/left/right/center）

- `enum class DockRegion`
- `class Dock : public Container`

### `widget/grid_layout.hpp`

> grid_layout.hpp —— 网格布局（子控件按行列均分单元格）

- `class GridLayout : public Container`

### `widget/hbox.hpp`

> hbox.hpp —— 水平盒子布局（子控件自左向右排列，主轴 = x）

- `class HBox : public Container`

### `widget/immediate_widget.hpp`

> immediate_widget.hpp —— 无状态即时组件：按钮 / 复选框
>
> 组件模型（即时模式，参照 interaction/immediate.hpp 的路由思路但不依赖终端）：
> * **不持有任何状态**：输入旗标（hovered/pressed/...）由调用方每帧从自己的
> 事件流推导后传入，组件每次调用都重画完整外观；
> * **返回值是"意图"不是"沿"**：hovered && pressed && enabled 即报告
> clicked/toggled。不做按下→松开的沿检测——那需要记忆，属于状态，
> 调用方比对自己上一帧收到的返回值即可得到沿；
> * **事件与几何正交**：返回意图只由输入旗标决定（调用方负责用同一个
> rect 做命中测试），几何只决定绘制——退化几何不绘制、不崩、不影响
> 返回值。这样组件没有"画不出来就算没点中"的隐藏耦合。
>
> 标签按 UTF-8 码点截断（宽度按主码点数估，宽字符按 1 格计；需要精确
> East Asian 宽度时调用方应自行截断后再传入）。

- `struct ControlState`

### `widget/panel.hpp`

> panel.hpp —— 带边框的容器（Container + Border）
>
> Panel 在 Container 之上加一层边框：content_rect() 给出减去边框厚度后的内容区，
> 子控件应布局在其中（典型做法是内部再放一个 VBox/HBox）。draw_border() 用
> 即时风格把边框描到 canvas（与 immediate_widget 一致）。Panel 不做子控件打包，
> layout() 仅把子控件整体铺进内容区——嵌套布局容器即可获得真实排版。

- `class Panel : public Container`

### `widget/reactive.hpp`

> reactive.hpp —— 响应式值：变更通知（订阅/发布）
>
> 线程约束（选型）：**单线程，不加锁**。理由：
> 1) widget 树与它的 Observable 归属同一 UI 线程，锁没有保护对象；
> 2) 加锁只会给"回调里再 set 同一个值"的重入路径以虚假的安全感——
> 通知顺序与重入语义应该由本文件写明的契约管住，而不是靠锁掩盖。
> 跨线程推送：调用方自行把新值排到 UI 线程再 set()。
>
> 契约：
> * 值未变（operator== 判等）不通知；通知顺序 = 订阅顺序；
> * 回调里再 set 同一 Observable 是**允许**的：重入请求被挂起，
> 等本轮通知走完后再补一次判定与通知（重入多次取最后一次的值）；
> * 通知遍历**快照**：回调中订阅/退订（含自毁）只影响下一次通知，
> 不会悬空、不会跳过本轮已在快照里的其他监听者。

- `class ListenerBook`
- `class Subscription`
- `class ListenerBook`

### `widget/scroll.hpp`

> scroll.hpp —— 可滚动容器（带视口偏移，裁剪由调用方负责）
>
> ScrollArea 持有一个"内容坐标空间"，子控件按各自 bounds 留在内容空间里，
> layout() 把每个子控件按其坐标减去 offset 摆到视口（= bounds）坐标系。offset
> 会被夹在 [0, content - viewport] 内（内容小于视口则不滚）。真正的视口裁剪
> （只显示 bounds 覆盖的格）由调用方负责——例如借 Compositor 按偏移合成，或
> 在绘制前对 canvas 做范围判断。

- `class ScrollArea : public Container`

### `widget/style_sheet.hpp`

> style_sheet.hpp —— 类名 → WidgetStyle 映射（header-only，无全局状态）
>
> 即时模式下没有"控件持有自己样式"的说法，样式按类名查表取用：调用方给控件
> 一个 class 名（如 "primary-button"），渲染时从 StyleSheet 取对应 WidgetStyle。
> 与 CSS 的类选择器同构，但只做精确名匹配（不含继承/层叠）。

- `class StyleSheet`

### `widget/styles.hpp`

> styles.hpp —— 控件样式值类型 + 基于 ThemePalette 的样式构造（header-only）
>
> WidgetStyle 是一组按交互状态分槽的 Style（normal/hover/active/disabled/
> focused）。它与绘制解耦：即时组件（immediate_widget）或应用层把 WidgetStyle
> 按当前 ControlState 选一份 Style 后交给 canvas。全部 constexpr/inline，无全局
> 状态，可按值传递。

- `enum class WidgetState`
- `struct WidgetStyle`

### `widget/theme.hpp`

> theme.hpp —— 主题聚合 + 按名取用（header-only）
>
> Theme 把名字与 ThemePalette 绑在一起，theme_by_name() 提供按名取用的薄入口
> （dark/light/mono），语义色板常量来自 theme_dark/light/mono。无注册表、无全局
> 状态——要扩展主题就再加一张 constexpr 表并在这个 switch 里加一行。

- `struct Theme`

### `widget/theme_dark.hpp`

> theme_dark.hpp —— 深色主题：值语义色板常量表（header-only，不可变）
>
> 无全局状态、无注册表：要用就按值取用这张 constexpr 表。色值取向是
> 深底浅字的终端默认观感（背景接近纯黑、强调色取终端常见的蓝）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `widget/theme_light.hpp`

> theme_light.hpp —— 浅色主题：值语义色板常量表（header-only，不可变）
>
> 与 theme_dark 同构，槽位一致；背景亮、文字暗。浅底下的"高亮底色"
> 用淡蓝而不是加深前景，保证与 background 的对比方向一致。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `widget/theme_mono.hpp`

> theme_mono.hpp —— 单色主题：值语义色板常量表（header-only，不可变）
>
> 只用灰阶（r==g==b），给无真彩/无 16 色降级路径与黑白终端用。
> success/danger 与前景同白：亮度编码不了语义，语义差异由组件自己的
> 字符形态承担（如 checkbox 的 'x'/' '、按钮的按压反白）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `widget/vbox.hpp`

> vbox.hpp —— 垂直盒子布局（子控件自上而下堆叠，主轴 = y）

- `class VBox : public Container`

### `widget/widget.hpp`

> widget.hpp —— widget 模块主头：保留式控件基类 + 主题色板值类型
>
> 模块定位（结构.md 二十二，层号 L18 见 TermchartModules.cmake）：
> * Widget 是控件树的最小基类：身份（id）+ 几何（bounds，格坐标，半开区间）
> + 状态旗标（可见/可用/可聚焦/已聚焦）+ 可覆写的命中测试。
> 它**不定义事件系统**——事件路由归 interaction/，无状态组件见
> immediate_widget.hpp（即时模式），响应式值见 reactive.hpp。
> * ThemePalette 是值语义色板：theme_dark/light/mono 各提供一张
> inline constexpr 表；不可变、无全局状态，可按值传递、可编译期取用。

- `struct ThemePalette`
- `class Widget`

## plugin（L19）

### `plugin/dynamic.hpp`

> dynamic.hpp —— 动态库加载（可选能力）
>
> 平台差异全部落在 src/plugin/dynamic.cpp（公开头里没有 #ifdef，与 term/ 的
> 平台隔离约定一致）。本模块是**可选能力**：静态插件与注册表路径不依赖它，
> 在禁用动态加载的环境下照常工作。
>
> 入口点约定：动态库导出
> extern "C" const termchart::plugin::Manifest* termchart_plugin_entry();
> 返回**静态存储期**的 Manifest（不得为 nullptr）；宿主据其 name/version
> 决定是否加载。Manifest 指向宿主不会修改的静态数据，因此指针语义安全。

- `class DynamicLibrary`

### `plugin/loader.hpp`

> loader.hpp —— 静态插件注册与加载
>
> 两种静态供给方式：
> 1. 直接向 PluginRegistry::instance().add(...) 登记描述符；
> 2. 用 add_static(bootstrap) 登记"引导函数"（往下游注册表做一次性注入、
> 一次性初始化），由 load_static_plugins() 统一执行。
>
> 引导函数只执行一次（执行后即移出队列，失败的也不重试）：重复调用
> load_static_plugins() 只会执行新登记的引导。
>
> 图表插件：register_chart_plugin() 把"一个 Chart 工厂"包装成插件——
> 加载时把工厂注入 chart::ChartRegistry，同时插件本身可按名字 create()。
> chart::Chart 的子类即可注册（examples/31"注册自定义图表"走这条路）。
> plugin(L19) → chart(L12) 的依赖由分层规则允许：插件的本质就是往
> L12–L18 的注册表里注入能力。

- `class ChartPlugin final : public Plugin`
- `TERMCHART_API bool register_chart_plugin(std::string_view name, std::string_view version, ChartFactory factory)`
- `TERMCHART_API bool add_static(BootstrapFn bootstrap)`
- `TERMCHART_API std::size_t load_static_plugins() noexcept`

### `plugin/manifest.hpp`

> manifest.hpp —— 插件元数据
>
> 纯值类型：可拷贝、可比较。注册表快照（App 构造期冻结）就是一次深拷贝，
> 因此 Manifest 不持有任何句柄或引用。

- `struct Manifest`

### `plugin/plugin.hpp`

> plugin.hpp —— 插件基类与插件注册表（L19）
>
> 注册时序协议（与 chart / raster / scale / dsl 注册表同构，docs/design/registry.md
> 的统一模式；api/quick.hpp 顶部"注册时机另有协议"指的就是这一条）：
> * PluginRegistry::instance() 是 **function-local static**，规避静态初始化顺序问题；
> * 注册的唯一安全时机是 **App 构造之前**（单线程）：App 构造时对注册表做
> 浅拷贝快照并冻结，之后注册的插件对已构造的 App 不可见；
> * 注册的是**工厂而非实例**：create() 每次返回新实例，生命周期归调用方，
> 没有原型 clone、没有共享所有权；
> * 本注册表不跨线程：注册与创建都假定发生在宿主线程（跨线程数据走
> app/post.hpp，不经过注册表）。

- `class Plugin`
- `struct PluginDescriptor`
- `class PluginRegistry`

## app（L20）

### `app/animation.hpp`

> animation.hpp —— 动画与缓动
>
> 口径（全模块统一，调用方不需要再猜）：
> * 输入 t 与输出都在 [0,1]；ease() 先把 t 钳位到 [0,1] 再过缓动曲线；
> 非有限 t（NaN/Inf）不可比较，一律视为界外 → 0；
> * 时间单位是**秒**（double），与 util/timer.hpp 一致；
> * 时钟可注入（AnimationClock）：单测与 headless 不依赖真实时间流逝，
> 真实时间只是缺省实现（steady_clock，单调，见 util/timer.hpp 的理由）。

- `enum class Easing : std::uint8_t`
- `class Animation`

### `app/app.hpp`

> app.hpp —— App：应用壳（构造时冻结注册表快照）
>
> 职责边界：App 是宿主与库之间的**编排骨架**，不画图、不碰终端、不依赖
> widget——GUI 栈会重写这层（docs/design/gui_stack 的分工），终端栈由
> ImmediateApp / HeadlessSession 补足。
>
> 快照语义（plugin/plugin.hpp 顶部注册时序协议的宿主侧落点）：
> * 构造时对 PluginRegistry 做一次浅拷贝快照并**冻结**；
> * 此后（包括 App 存活期内）的注册对**本实例**不可见；
> * create_plugin 只查自己的快照，不回查全局注册表——宿主看到的插件
> 集合在构造那一刻就确定了，不会因为别的代码注册新插件而漂移。

- `class App`

### `app/config.hpp`

> config.hpp —— 全局配置（键值对，线程安全）
>
> global() 是 function-local static（与其他注册表同一套契约）；独立实例
> 用于测试与多实例场景。
>
> 类型口径：值是 variant，**读什么类型得什么类型**——跨类型读取回退默认值，
> 不做隐式转换（字符串 "1" 不会当 bool 用）。配置错误应当显式暴露，
> 而不是被静默宽容掉。

- `class Config`
- `TERMCHART_API void set_config(std::string_view key, ConfigValue value)`

### `app/headless.hpp`

> headless.hpp —— 无终端模式
>
> 提供"渲染到 CellBuffer 而**不碰 stdout、不触碰 term/**"的入口封装：
> Canvas（格粒度绘制）→ CellBuffer → render::Renderer → 字节串
>
> 与实时路径（Terminal → Renderer → stdout）共享同一个 Renderer，因此在
> CI / headless / 单元测试里得到的字节流与真终端一致；差异只剩"谁来写
> stdout"——这里谁都不写，字节串直接交给调用方（golden、断言、存文件）。

- `class HeadlessSession`

### `app/immediate_app.hpp`

> immediate_app.hpp —— 即时模式主循环
>
> Dear ImGui 式"每帧重放"：**所有状态都在 frame 回调的捕获里**，本类不沉淀
> 任何图表状态，只负责两件事——节奏（时钟、帧数上限、delta，复用 Loop 的
> 语义）与渲染管线接入（经 HeadlessSession，全程不碰 stdout；换到真终端时
> 只需把渲染段换成实时 Renderer，frame 回调原样保留）。
>
> 每帧的顺序固定：清画布 → frame 回调作画 → 增量渲染进 frames_。
> 清画布是即时模式的定义：画面完全由本帧的回调决定，前一帧不残留。

- `class ImmediateApp`

### `app/loop.hpp`

> loop.hpp —— 主事件循环
>
> 节奏由**注入时钟**驱动（缺省 steady_clock 秒，同 util/timer.hpp 的理由）：
> 每帧先读时钟算 delta（首帧 delta = 0），再依次调用 tick 回调。
>
> delta 会被钳制到 max_delta（掉帧保护）：一次 10 秒的卡顿不应该让动画
> 直接跳到终点。上限缺省 0.1 秒，可用 set_max_delta 调整（非有限或 <=0
> 的值被忽略——"不限速"不是合法配置，那等于关掉保护）。
>
> tick 回调里可以安全调用 stop() 与 remove_tick()：每帧在回调列表的副本上
> 迭代，增删不影响当前帧。帧数极小（每秒几十），副本开销可忽略。

- `class Loop`

### `app/post.hpp`

> post.hpp —— 跨线程数据投递
>
> 模型：生产线程 post()，消费线程（通常是主循环）take()/drain()。
> 一把互斥锁护一个队列——这不是 lock-free 队列：投递频率是"用户事件"级
> （键盘、网络回调、其他线程的刷新请求），正确性与可读性优先，吞吐不是目标。
>
> 两个原语各管一种形态：
> * PostQueue  —— 闭包队列：每个消息自带"怎么处理"，适合事件投递；
> * Mailbox<T> —— 类型化单值信箱：最新值语义（后写覆盖未消费旧值），
> 适合"最新状态"型数据（鼠标位置、当前尺寸）。模板部分只能住在头文件。

- `class PostQueue`

## api（L21）

### `api/chart.hpp`

> chart.hpp —— 一行出图（chart 侧的 api 门面）
>
> 为什么要有它：examples/03 的"标准姿势"有五步（建 Canvas → 建 Chart → 喂 Series
> → chart.draw({canvas}) → Renderer::render_full → 写 stdout），其中四步是纯样板，
> 且**每一步都能写错**（忘了 render_full 会拿到空帧、忘了 flush 会丢半屏）。
> `render_chart` 把它们收成一次调用：你只负责数据与图表类型。
>
> 与 `quick::render`（CellBuffer → 字节）的分工：quick 面向"我已经有一块 buffer"，
> 这里面向"我只有一个图表"。
>
> 尺寸规则是**刻板**的（见 cpp 里的实现注释）：不自动探测终端。理由——
> 自动探测会让同一段代码在 CI/headless 与真终端里产出不同尺寸，测试和文档都没法
> 复现；要自适应请显式传 `opts.width(height_of_terminal)`（另见 CODEBUDDY.md 坑 11：
> 画布大于可视区会折行 + 滚动，图像错位）。

- （无命名空间级声明，或为纯宏 / 伞形头）

### `api/opts.hpp`

> opts.hpp —— ApiOptions 链式配置（旧名 `Opts`，见文件末尾的兼容别名）
>
> 面向"一行出图"的入口：所有可选参数都在这一处，且**每个都有默认值**，
> 于是 tc::quick::hello() 之类的调用可以完全零配置。
> 语义：未设置的项（std::optional 为空）表示"由库自行决定"。
>
> 命名：全库其余位置一律 `XxxOptions`（MetroOptions / SankeyOptions / …），
> 这里原名 `Opts` 是唯一的例外；为"同一语义同一命名"收敛成 `ApiOptions`，
> 旧名以别名保留，因此**不破坏**任何既有调用点。

- `class ApiOptions`

### `api/print.hpp`

> print.hpp —— stdout 输出封装
>
> 为什么单独一个文件：输出是**唯一会触碰进程全局状态的动作**。
> 把它收在一处，headless / CI / 测试就能整体替换（或直接不调用）。
> 约定：不做 locale 相关转换；写的是渲染管线产出的原始字节。

- `TERMCHART_API Status write_to(std::FILE* stream, std::string_view bytes) noexcept`
- `TERMCHART_API Status write_stdout(std::string_view bytes) noexcept`
- `TERMCHART_API Status write_stderr(std::string_view bytes) noexcept`
- `TERMCHART_API Status flush(std::FILE* stream) noexcept`
- `TERMCHART_API Status flush_stdout() noexcept`
- `TERMCHART_API std::string escape_for_debug(std::string_view bytes)`

### `api/quick.hpp`

> quick.hpp —— 总入口
>
> 两件事：
> 1. init_core()：进程内一次性初始化（**std::call_once 守卫**，可重复调用）。
> 初始化内容是"全局副作用"的最后容身处：日志级别、默认能力快照。
> 注意：**注册表不在这里** —— 注册时机另有协议（见 docs/design/registry.md）。
> 2. 一行出图的便捷接口：给定文本与样式，走完整的
> CellBuffer → Renderer → stdout 链路。

- `TERMCHART_API void init_core() noexcept`
- `struct PlotOpts`

## gui（L22）

### `gui/font.hpp`

> font.hpp —— GUI 文本的唯一出口（对调用方透明的混合字体后端）
>
> 渲染后端分工（接口不变，底层实现见 font.cpp）：
> * ASCII 0x20..0x7E：内置 5×7 点阵。零成本、零外部依赖，且"ASCII 字形
> 落像素"的像素契约由测试钉住；
> * 其余码点（CJK / Latin-1 扩展 / 全角符号…）：惰性加载系统 TTF 字体
> （Windows 自动尝试 C:/Windows/Fonts/msyh.ttc、simhei.ttf 等；可用
> 环境变量 TERMCHART_GUI_FONT 显式指定字体文件路径），找不到字体或
> 解析失败时 fail-safe 退回点阵占位框——不抛异常、不崩溃。
> 等宽语义不变：列数由 util 的显示宽度决定（CJK 占 2 列），每格 advance
> 恒等于 cell_w；TTF 只决定格内像素，不参与排版步进（无 shaping/hinting，
> 不存在字距把布局挤歪的问题）。
>
> 字形是 5×7 列位点阵（数据与位序约定见 font.cpp 的 kFont5x7），渲染时映射进
> cell_w×cell_h 的整数格：字形画在前 5/6 格宽、前 7/8 格高（末列/末行留作
> 字距），保证 advance == cell_w 整数对齐。TTF 也没有覆盖到的码点画"虚线
> 占位框"而不是留白——中文标签掉成空白比掉成虚框更难排查，可见且可度量是底线。

- `struct TextMetrics`
- `class TERMCHART_API FontBackend`

### `gui/hover_chart.hpp`

> hover_chart.hpp —— GUI 悬停层：命中 → 把浮层翻译成 Scene 图元
>
> 定位：GUI 平行栈的交互呈现。核心设计是"**悬停不改渲染器，只改 Scene**"——
> gui::SceneRenderer 根本不知道交互存在，它只是又消费了一份 Scene。
>
> 复用面（都是既有件，不是新写的状态机）：
> * chart::HitRegion         —— 图表渲染时登记的命中形状 + 不透明 payload，
> 是 chart → interaction 的唯一契约；
> * interaction::topmost_hit —— 纯几何判定（最后登记者在上），禁图表名；
> * scene::SceneBuilder      —— 浮层就是普通的 RectOp / TextOp 图元。
>
> 与终端侧 interaction::Tooltip 的关系：**刻意不共用实现**。终端 tooltip 是
> cell 补丁语义（记住被改写的格、clear() 后逐格还原），因为它画在字符画布上、
> 覆盖即破坏；GUI 每帧重建整份 Scene，浮层只是"这一帧多塞几个图元"，既不需要
> patch/restore，也不该把"字符格"这个概念带进像素域。二者共享的只有上游的
> HitRegion / topmost_hit，以及"贴边要翻方向"这条直觉。
>
> 坐标系：HitRegion 与浮层坐标都在 **Scene 坐标系**里，与底图同一坐标系。
> 调用方负责把窗口/鼠标像素换算成 Scene 坐标；autofit=false 时二者恒等。

- `class HoverLayer`

### `gui/scene_renderer.hpp`

> scene_renderer.hpp —— Scene → Bitmap（GUI 平行栈的第一块落地）
>
> 定位（docs/design/gui_stack.md）：GUI 与终端是平行栈，共享到 `scene`（L4）
> 为止。本文件是 GUI 侧的 `render/scene_renderer`——消费与终端/SVG **同一份**
> Scene 指令集、**同一份** StatefulVisitor 时序；区别只在"往哪画"：
> 终端往字符格画，这里往 image::Bitmap 画。
>
> 渲染质量决策（为什么这样画）：
> * 几何（线/弧/矩形）走 **S×S 超采样 + 盒式降采样**得到抗锯齿——实现小、
> 质量对得起"生产级"，且不需要解析覆盖率这类容易写错的数学；
> * 文本**不参与超采样**：gui/font 是位图点阵字体，超采样只会把它糊掉，
> 所以文本在下采样后按最终分辨率直绘，且**永远在几何之上**
> （图表标签/图例压在网格线上是生产惯例）；
> * 线宽语义与 SVG 同构：StrokeOp.width 是 Scene 单位，随 autofit 缩放。
>
> 不做的事（边界）：不解析 ImageOp 的像素（Scene 不持有位图，画占位棋盘格）；
> 不做渐变/裁剪/图层（Scene 刻意不是完备 IR，见 scene/command.hpp）。

- `struct RenderOptions`
- `class SceneRenderer final : private scene::StatefulVisitor`

