#ifndef TERMCHART_CAPI_H
#define TERMCHART_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* termchart C ABI —— 稳定 ABI 适配层（Python / Rust / Java 桥的共同前置）。
 *
 * 设计约束（见 docs/design/ffi_plan.md §1）：
 *   * 全用 POD / 不透明句柄 / 回调；绝不把 C++ 异常抛过 C 边界。
 *   * 字符串由 C 侧分配、调用方用 tc_free_string 释放；不得跨 ABI 传 std::string。
 *   * 失败一律走 tc_error_handle + tc_status，不从函数直接抛错。
 *
 * 暴露面（与 C++ 便捷入口 api:: 等价，即"新手一行出图"那一层）：
 *   文本：render_line / print_line / hello（可带 Style + ApiOptions）
 *   图表：折线 / 散点 / 柱状 → ANSI 字节流，或直接写 stdout
 *   输出：write_stdout / write_stderr / flush_stdout
 *   自动：auto_plot（读 CSV/JSONL → AutoViz 选图 → 出图）
 *   （render_chart / chart_to_svg / save_image 依赖 chart::Chart、image::Bitmap 等
 *     重类型，暂不在 C 面展开；需要时按同一句柄 + 错误协议逐步追加。）
 */

/* 版本字符串（"1.0.0" 之类）。 */
const char* tc_version_string(void);

/* 错误 / 状态：返回 0 = OK；非 0 = 错误类别。人类可读消息走 tc_error_handle。 */
typedef struct tc_error* tc_error_handle;
typedef int tc_status;

/* 状态码（tc_status 的取值）。**只增不改**：已编译的调用方按数值判断仍然成立。 */
typedef enum tc_status_code {
    TC_OK = 0,
    TC_ERR_INVALID_HANDLE = 1,   /* 句柄为空或已释放 */
    TC_ERR_INVALID_ARG = 2,      /* 参数非法（含长度不符、域退化） */
    TC_ERR_RENDER = 3,           /* 数据 / 渲染阶段失败 */
    TC_ERR_NOMEM = 4,            /* 内存不足 */
    TC_ERR_IO = 5,               /* 写 stdout / 文件失败 */
    TC_ERR_BUFFER_TOO_SMALL = 6, /* 调用方缓冲不足：*out_len 回写所需长度 */
    TC_ERR_UNSUPPORTED = 7       /* 该图种 / 该属性不支持：显式报错，不静默降级 */
} tc_status_code;

/* 图表句柄（不透明）。 */
typedef struct tc_chart* tc_chart_handle;

/* 颜色深度：**严格对齐** core::ColorDepth（None=0, Ansi16=1, Ansi256=2, TrueColor=3）。
 * 祖先版本曾错写为 16=0，导致"16 色"实际映射到无色；此处已更正。 */
typedef enum {
    TC_DEPTH_NONE = 0,
    TC_DEPTH_16 = 1,
    TC_DEPTH_256 = 2,
    TC_DEPTH_TRUECOLOR = 3
} tc_color_depth;

/* 出图选项（POD，全部可缺省；0 / NULL = 取库默认）。 */
typedef struct tc_plot_opts {
    uint32_t width;       /* 字符列数，0 = 默认 80 */
    uint32_t height;      /* 字符行数，0 = 默认 24 */
    const char* title;    /* 可为 NULL */
    tc_color_depth depth; /* 0 = 取库默认（TrueColor） */
    uint8_t show_legend;  /* 0/1，默认 0 */
} tc_plot_opts;

/* ── 颜色 / 样式（对齐 core::Color / core::Style）────────────────────────── */
typedef enum {
    TC_COLOR_NONE = 0,   /* 未指定：沿用终端默认色 */
    TC_COLOR_ANSI16 = 1,
    TC_COLOR_INDEXED = 2,
    TC_COLOR_RGB = 3
} tc_color_kind;

typedef struct tc_color {
    tc_color_kind kind;
    uint8_t index;    /* ANSI16 / INDEXED 时用（0..255） */
    uint8_t r, g, b;  /* RGB 时用 */
} tc_color;

/* 文本样式（对齐 core::Style）。attrs 为属性位掩码：
 *   1=Bold, 2=Dim, 4=Italic, 8=Underline, 16=Reverse（对齐 core::Attr）。 */
typedef struct tc_style {
    tc_color fg;      /* kind=TC_COLOR_NONE → 终端默认前景 */
    tc_color bg;
    uint16_t attrs;
} tc_style;

/* 文本入口选项（对齐 api::ApiOptions）。宽度/高度 0 = 由库决定。 */
typedef struct tc_api_opts {
    uint32_t width;        /* 0 = 不指定 */
    uint32_t height;       /* 0 = 不指定 */
    tc_color_depth depth;
    uint8_t has_depth;     /* 非 0 才应用 depth */
    uint8_t unicode;       /* 0 = ASCII，1 = Unicode */
    uint8_t newline;       /* 1 = 末尾补换行 */
    uint8_t reset_at_end;  /* 1 = 帧尾复位 SGR */
} tc_api_opts;

/* 便捷默认值（务必用它初始化后再按需覆盖；比裸零值更接近库默认）。
 *   tc_api_opts_default(): width/height=0, depth=TrueColor, has_depth=1,
 *                          unicode=newline=reset_at_end=1
 *   tc_style_default():    全 plain（无色、无属性） */
tc_api_opts tc_api_opts_default(void);
tc_style tc_style_default(void);

/* 幂等初始化：进程内一次性副作用（日志级别、默认能力快照）。可重复调用。 */
void tc_init(void);
uint8_t tc_core_initialized(void);

/* ── 文本渲染（对齐 api::render_line / print_line / hello）──────────────────
 * style 可为 NULL（等价 plain）；opts 可为 NULL（等价 tc_api_opts_default()）。
 * tc_render_line：成功 *out_ansi 指向 C 侧堆串（tc_free_string 释放）。
 * tc_print_line / tc_hello：直接写 stdout，返回写入状态。 */
tc_status tc_render_line(const char* text, const tc_style* style, const tc_api_opts* opts,
                         char** out_ansi, tc_error_handle* err);
tc_status tc_print_line(const char* text, const tc_style* style, const tc_api_opts* opts,
                        tc_error_handle* err);
tc_status tc_hello(const tc_api_opts* opts, tc_error_handle* err);

/* ── 图表创建 ────────────────────────────────────────────────────────────────
 * tc_chart_create：按 kind 字符串建任意已登记图种；未知名返回 NULL（并填 err）。
 * tc_chart_kinds：两段式枚举全部可用 kind（空格分隔，字典序），供绑定/CLI 发现能力：
 *   1) 先传 cap=0（buf 可 NULL）→ *out_len 回写所需字节数；
 *   2) 再按 *out_len 分配缓冲重调 → 写入全部 kind 名。
 * 建图只有这一条路——没有 per-kind 工厂函数。 */
tc_chart_handle tc_chart_create(const char* kind, tc_error_handle* err);
tc_status tc_chart_kinds(char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err);
void tc_chart_free(tc_chart_handle chart);

/* 折线 / 散点：喂入 (x[i], y[i])，n 为点数。 */
tc_status tc_chart_add_points(tc_chart_handle chart, const double* x, const double* y,
                              uint64_t n);

/* 柱状：values[i]；labels 可为 NULL（按序号），n_labels 为标签数。 */
tc_status tc_chart_add_bar(tc_chart_handle chart, const double* values, uint64_t n,
                           const char* const* labels, uint64_t n_labels);

/* 通用"值 + 类别标签"入口（柱状 / 饼图 / 迷你图共用）。labels 可 NULL。
 * 这是对 tc_chart_add_bar 的泛化：返回 tc_status 且对不适用图种报
 * TC_ERR_UNSUPPORTED，而不是静默丢弃。 */
tc_status tc_chart_set_values(tc_chart_handle chart, const double* values, uint64_t n,
                              const char* const* labels, uint64_t n_labels,
                              tc_error_handle* err);

/* 渲染成 ANSI 字节流（UTF-8 + 真彩 SGR），不写 stdout。
 * 成功：*out_ansi 指向 C 侧分配、须用 tc_free_string 释放的堆串；*err 不变。
 * 失败：返回非 0，*out_ansi 不变，*err 被填充（用 tc_error_message 读、
 *       tc_error_free 释放）。
 */
tc_status tc_chart_render_ansi(tc_chart_handle chart, const tc_plot_opts* opts, char** out_ansi,
                               tc_error_handle* err);

/* 渲染并直接写 stdout（对齐 api::plot / scatter / bar）。 */
tc_status tc_chart_print(tc_chart_handle chart, const tc_plot_opts* opts, tc_error_handle* err);

/* ── 扩展面：多系列 / 属性通道 / 矢量导出 ─────────────────────────────────────
 *
 * 切法（为什么不是"每项配置一个函数"）：C 没有重载、可选参数、变体。若给每个
 * chart 配置项开一个函数，函数数 = 图种 × 配置项，且每加一项都要改头文件、
 * 改实现、改每个语言绑定——这正是"牵一发而动全身"。
 *
 * 所以模型属性收成**一条通道**：
 *
 *     tc_chart_set_prop(chart, TC_PROP_XXX, &value)
 *
 * 新增可配置项 = 新增一个 tc_chart_prop 枚举值 + 实现里一个 case；
 * **函数签名与已有结构体一律不动**，已编译调用方与语言绑定不受影响。
 *
 * 分工不重复：宽 / 高 / 标题 / 图例 / 色彩深度属于「展示」，仍在 tc_plot_opts
 * 里按次传入；只有「模型」属性（域、刻度、零线强调）走 tc_chart_set_prop。 */

/* 通用值载体。零初始化（tc_value v = {0}）或 kind=TC_VAL_NONE 表示"清除该属性"。 */
typedef enum tc_value_kind {
    TC_VAL_NONE = 0,          /* 未设置 / 清除 */
    TC_VAL_BOOL = 1,          /* 用 i（0 / 非 0） */
    TC_VAL_INT = 2,           /* 用 i */
    TC_VAL_DOUBLE = 3,        /* 用 d */
    TC_VAL_STRING = 4,        /* 用 s + n（UTF-8；n=0 时按 strlen(s)） */
    TC_VAL_DOUBLE_ARRAY = 5   /* 用 p + n */
} tc_value_kind;

typedef struct tc_value {
    tc_value_kind kind;
    int64_t i;
    double d;
    const char* s;
    uint64_t n;        /* 字符串字节数 / 数组元素个数 */
    const double* p;   /* kind=TC_VAL_DOUBLE_ARRAY 时的数据 */
} tc_value;

/* 一条系列。批量入口：一次调用传入 count 条，点集一次性构造（不经逐点 setter）。 */
typedef struct tc_series {
    const char* name;  /* 可为 NULL → 自动命名 "series N" */
    tc_color color;    /* kind=TC_COLOR_NONE → 图表默认色 */
    const double* x;   /* n>0 时必须非 NULL */
    const double* y;
    uint64_t n;        /* 点数；0 = 非法 */
} tc_series;

/* 追加系列（append 语义，与库内 add_* 约定一致）。仅 Series / Points 形状图种可用。
 * 柱状等 Values 形状用 tc_chart_add_bar / set_values；矩阵图种用 tc_chart_set_matrix。 */
tc_status tc_chart_add_series(tc_chart_handle chart, const tc_series* series, uint64_t count,
                              tc_error_handle* err);

/* 二维行式数据入口（Records 形状：contour/corr/mosaic/violin/pca/parallel 及
 * candlestick/bubble/boxplot/bullet/forest/… 等专属结构图种，按列语义解释）。
 * rows[i] 指向第 i 行 col_count 个 double；row_labels/col_labels 可 NULL。 */
tc_status tc_chart_set_matrix(tc_chart_handle chart, const double* const* rows,
                              uint64_t row_count, uint64_t col_count,
                              const char* const* row_labels, uint64_t n_row_labels,
                              const char* const* col_labels, uint64_t n_col_labels,
                              tc_error_handle* err);

/* 单值入口（gauge / progress_ring / star_rating / thermometer / status_light；
 * status_light 的 value 按 0=Ok,1=Warning,2=Error,3=Unknown 映射）。 */
tc_status tc_chart_set_scalar(tc_chart_handle chart, double value, tc_error_handle* err);

/* 树入口（treemap/sunburst/icicle/dendrogram/pack/tree_view）。
 * parents[i] 为父节点索引（-1 为根），names[i]/values[i] 为节点名与值。 */
tc_status tc_chart_set_tree(tc_chart_handle chart, const int* parents,
                            const char* const* names, const double* values, uint64_t n,
                            tc_error_handle* err);

/* 文本入口（table / gauge_kpi / geo_map）。rows 为行指针数组，col_counts[i] 为
 * 第 i 行列数；blob 为 GeoJSON 文本（仅 geo_map 用），可 NULL。 */
tc_status tc_chart_set_text(tc_chart_handle chart, const char* const* const* rows,
                            uint64_t row_count, const uint64_t* col_counts,
                            const char* blob, tc_error_handle* err);

/* 区域入口（choropleth / dot_density / geo_map 数值覆盖）。
 * cell_starts[i]/cell_counts[i] 给出第 i 个区域在 cell_x/cell_y 扁平数组中的段。 */
tc_status tc_chart_set_regions(tc_chart_handle chart,
                               const char* const* names, const double* values, uint64_t n_regions,
                               const double* cell_x, const double* cell_y,
                               const uint64_t* cell_starts, const uint64_t* cell_counts,
                               uint64_t n_cells, tc_error_handle* err);

/* 模型属性。不支持该属性的图种返回 TC_ERR_UNSUPPORTED（不静默忽略）。 */
typedef enum tc_chart_prop {
    TC_PROP_Y_DOMAIN = 1,       /* TC_VAL_DOUBLE_ARRAY 长度 2 = {lo, hi}，要求有限且 hi>lo；
                                   折线 / 柱状（散点无此能力 → Unsupported）；清除 =
                                   恢复自动域。柱状须自行把 0 并进域内。 */
    TC_PROP_AUTO_DOMAIN = 2,    /* bool；折线 / 散点（柱状恒为自动域 → Unsupported） */
    TC_PROP_INCLUDE_ZERO = 3,   /* bool；折线 / 散点，仅在 AUTO_DOMAIN 开启时生效 */
    TC_PROP_EMPHASIZE_ZERO = 4, /* bool；仅折线 */
    TC_PROP_SHADE_NEGATIVE = 5, /* bool；仅折线 */
    TC_PROP_X_TICKS = 6,        /* TC_VAL_DOUBLE_ARRAY 显式 x 刻度（时间轴用）；仅折线，
                                   空数组 = 恢复自动刻度 */
    TC_PROP_X_LABEL = 7,        /* TC_VAL_STRING：X 轴标签。C-ABI 层后置绘制，不碰 C++ 核心 */
    TC_PROP_Y_LABEL = 8         /* TC_VAL_STRING：Y 轴标签。C-ABI 层后置绘制，不碰 C++ 核心 */
} tc_chart_prop;

tc_status tc_chart_set_prop(tc_chart_handle chart, tc_chart_prop prop, const tc_value* value,
                            tc_error_handle* err);

/* 矢量导出（SVG）。**两段式缓冲**，不跨 ABI 传 std::string、不由库分配内存：
 *   1) 先传 cap=0（buf 可为 NULL）→ *out_len 回写所需字节数；
 *   2) 再按 *out_len 分配缓冲重新调用 → 写入并再次回写实际长度。
 * 缓冲不足返回 TC_ERR_BUFFER_TOO_SMALL，且 *out_len 仍是所需长度。
 * 尺寸取 opts->width/height（0 = 默认 80×24）。 */
tc_status tc_chart_to_svg(tc_chart_handle chart, const tc_plot_opts* opts, char* buf,
                          uint64_t cap, uint64_t* out_len, tc_error_handle* err);

/* 把图渲染成 PNG 字节（与 to_svg 同款两段式缓冲）。
 * 链路全在核心库内：Canvas（格粒度绘制）→ CellBuffer → gui::render_cells_to_bitmap
 * （字符格栅格化为像素，含盲文/半块语义）→ image::encode(Png)。**不牵任何 GUI
 * 窗口/控件后端**，字体光栅化是核心的一部分。opts 的 width/height 即格数
 * （默认 80×24）；每格像素尺寸与底色取 gui::CellBitmapOptions 默认（9×16、深色底）。 */
tc_status tc_chart_to_png(tc_chart_handle chart, const tc_plot_opts* opts, char* buf,
                          uint64_t cap, uint64_t* out_len, tc_error_handle* err);

/* ── 输出（对齐 api::write_stdout / write_stderr / flush_stdout）───────────── */
tc_status tc_write_stdout(const char* bytes, uint64_t n, tc_error_handle* err);
tc_status tc_write_stderr(const char* bytes, uint64_t n, tc_error_handle* err);
tc_status tc_flush_stdout(tc_error_handle* err);

/* ── AutoViz 一行出图（对齐 api::auto_plot）─────────────────────────────────
 * 读 filepath（首个非空白字符为 '{' 走 JSONL，否则 CSV），引擎选图并出图到 stdout。
 * 仅表头无数据 → 非 0；Unix 时间戳数字串不自动识别为时间列（同引擎口径）。 */
tc_status tc_auto_plot(const char* filepath, tc_error_handle* err);

/* ── data 门面（对齐 data::DataFrame 容错 CSV 读取）───────────────────────────
 * C 面补上「内存文本 → 容错 DataFrame」入口：与 tc_auto_plot 的文件路径版对称，
 * 让绑定层不必先落盘再读。DataFrame 是 fail-safe 读取器——脏值保留原文并打标，
 * 整表绝不报错（与 io/csv 的 fail-fast 刻意不同）。
 *
 * 句柄模式与 chart/graph 一致：create → query → free；列/行索引从 0 起。 */
typedef struct tc_data* tc_data_handle;

/* 分隔符 0 = 自动嗅探；has_header=0 时表头合成 col0..；comment=0 关闭注释行。 */
typedef struct tc_data_csv_opts {
    uint8_t delimiter;
    uint8_t has_header;
    uint8_t comment;
} tc_data_csv_opts;

typedef enum tc_cell_kind {
    TC_CELL_EMPTY = 0,
    TC_CELL_NUMERIC = 1,
    TC_CELL_TEXT = 2,
    TC_CELL_INVALID = 3
} tc_cell_kind;

/* 单元格质量标记位（与 data::CellFlag 一一对应，只增不改）。 */
typedef enum tc_cell_flag {
    TC_CELL_FLAG_NONE = 0,
    TC_CELL_FLAG_HAS_COMMA = 1,
    TC_CELL_FLAG_ACCOUNTING_NEGATIVE = 2,
    TC_CELL_FLAG_PERCENT = 4,
    TC_CELL_FLAG_FULL_WIDTH = 8,
    TC_CELL_FLAG_CURRENCY = 16,
    TC_CELL_FLAG_MISSING = 32,
    TC_CELL_FLAG_RAGGED = 64,
    TC_CELL_FLAG_UNPARSED = 128
} tc_cell_flag;

/* 容错解析内存 CSV 文本；text 非空即可，空文本返回 0 行 0 列的空表。 */
tc_data_handle tc_data_parse_csv(const char* text, const tc_data_csv_opts* opts,
                                 tc_error_handle* err);
void tc_data_free(tc_data_handle data);

uint64_t tc_data_row_count(tc_data_handle data);
uint64_t tc_data_col_count(tc_data_handle data);

/* 表头/单元格原文写两段式缓冲：cap=0 只问长度；不跨 ABI 传 std::string。 */
tc_status tc_data_header(tc_data_handle data, uint64_t col, char* buf, uint64_t cap,
                         uint64_t* out_len, tc_error_handle* err);
tc_status tc_data_cell_raw(tc_data_handle data, uint64_t row, uint64_t col, char* buf,
                           uint64_t cap, uint64_t* out_len, tc_error_handle* err);
/* 数值型单元格的数值；非数值或越界返回 TC_ERR_UNSUPPORTED（不静默给 NaN）。 */
tc_status tc_data_cell_number(tc_data_handle data, uint64_t row, uint64_t col, double* out,
                              tc_error_handle* err);
tc_status tc_data_cell_info(tc_data_handle data, uint64_t row, uint64_t col,
                            tc_cell_kind* kind, uint8_t* flags, tc_error_handle* err);

/* 内存文本一行出图：与 tc_auto_plot(filepath) 同引擎，text 首个非空白为 '{' 走 JSONL。 */
tc_status tc_auto_plot_text(const char* text, tc_error_handle* err);

/* ── 一行出图（对齐 api::render_plot / render_scatter / render_bar）──────────
 *
 * 这是 C++ 新手向 API 在 C 上的**投影**，而不是另写一套：绑定层若自己拼
 * Chart，系列名、配色、图例默认值迟早和 C++ 那一版对不上（实测就差了图例
 * 和配色）。所以 Python 的 plot/scatter/bar 必须走这里，输出才与
 * `termchart::api::plot(...)` 逐字节相同。
 */
typedef enum tc_quick_kind {
    TC_QUICK_LINE = 0,   /* api::render_plot */
    TC_QUICK_SCATTER = 1 /* api::render_scatter */
} tc_quick_kind;

tc_status tc_quick_xy(tc_quick_kind kind, const double* x, const double* y, uint64_t n,
                      const tc_plot_opts* opts, char* buf, uint64_t cap, uint64_t* out_len,
                      tc_error_handle* err);

tc_status tc_quick_bar(const double* values, const char* const* labels, uint64_t n,
                       const tc_plot_opts* opts, char* buf, uint64_t cap, uint64_t* out_len,
                       tc_error_handle* err);

/* ── 图像（对齐 image::Bitmap 与 image::encode / decode_image）────────────────
 *
 * 句柄背后是 image::Bitmap（3/4 通道、行紧致）。所有像素操作都**一一对应**
 * C++ 同名成员（fill / fill_rect / blit_over / scaled_to / with_alpha /
 * without_alpha），不在绑定侧重算像素——这是"Python 效果与 C++ 逐字节一致"
 * 的前提，也是图像能力只做一条桥的理由。
 *
 * 像素进出都走字节缓冲拷贝（不跨 ABI 传 std::vector，也不让库分配内存后再
 * 交给调用方释）：tc_image_pixels / tc_image_encode 都是两段式——先 cap=0
 * 问所需长度，再按长度分配重调。
 *
 * 解码支持的格式由构建开关决定：PNG / BMP 原生零依赖；JPEG / GIF / WebP 需
 * TERMCHART_ENABLE_STB_IMAGE，未启用时返回 TC_ERR_UNSUPPORTED（不猜着解）。
 */
typedef struct tc_image* tc_image_handle;

/* 空图（全透明/全零）。channels 只接受 3 或 4（其余按 4）。 */
tc_image_handle tc_image_create(uint32_t width, uint32_t height, uint8_t channels,
                                tc_error_handle* err);
/* 从内存字节解码（按魔数识别格式）。 */
tc_image_handle tc_image_decode(const uint8_t* data, uint64_t size, tc_error_handle* err);
/* 从文件解码（UTF-8 路径，Windows 下同样安全）。 */
tc_image_handle tc_image_load(const char* path, tc_error_handle* err);
void tc_image_free(tc_image_handle image);

/* 尺寸查询（句柄为空返回 0）。 */
uint32_t tc_image_width(tc_image_handle image);
uint32_t tc_image_height(tc_image_handle image);
uint8_t tc_image_channels(tc_image_handle image);

/* 像素读出：行紧致 width*height*channels 字节。两段式（同 SVG 导出）。 */
tc_status tc_image_pixels(tc_image_handle image, uint8_t* buf, uint64_t cap, uint64_t* out_len,
                          tc_error_handle* err);

/* 派生新图（不修改源，返回新句柄，调用方负责 tc_image_free）。 */
tc_image_handle tc_image_scaled(tc_image_handle image, uint32_t width, uint32_t height,
                                tc_error_handle* err);
tc_image_handle tc_image_with_alpha(tc_image_handle image, uint8_t alpha, tc_error_handle* err);
tc_image_handle tc_image_without_alpha(tc_image_handle image, tc_error_handle* err);

/* 原地绘制（越界裁剪 / 越界忽略；空句柄为无操作，不崩）。 */
void tc_image_fill(tc_image_handle image, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void tc_image_fill_rect(tc_image_handle image, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a);
/* 源 alpha 做 source-over 叠加到 dst；src 无 alpha 时无操作（先 with_alpha）。 */
void tc_image_blit_over(tc_image_handle dst, tc_image_handle src, int32_t dx, int32_t dy);

/* 编码成字节流。format："png" / "jpeg" / "gif" / "webp"（mp4 需多帧，此处不支持）。
 * quality ∈ [1,100]，无损格式忽略。两段式缓冲。 */
tc_status tc_image_encode(tc_image_handle image, const char* format, uint8_t quality, char* buf,
                          uint64_t cap, uint64_t* out_len, tc_error_handle* err);
/* 编码并写文件（扩展名无关，格式由 format 决定；format 为空则按路径后缀推断）。 */
tc_status tc_image_save(tc_image_handle image, const char* path, const char* format,
                        uint8_t quality, tc_error_handle* err);

/* ── 终端图像协议（对齐 protocol/：sixel / kitty / iTerm2 / 半块）─────────────
 *
 * 一张 tc_image_handle 可以用四种方式写进终端：像素级协议（sixel / kitty /
 * iTerm2）或半块字符降级。编码结果**不含光标定位**（与 C++ 一致：放哪儿由
 * 调用方决定），也不写 stdout——拿到字节自己 write / 塞进 Document 即可。
 *
 * 选择协议不在这里发明第二套规则：TC_PROTO_AUTO 直接走
 * protocol::detect_protocol_caps() + protocol::pick_protocol()，与 C++ 同一条
 * 降级链（Kitty > iTerm2 > Sixel > 半块）。全部不可用时返回
 * TC_ERR_UNSUPPORTED，而不是悄悄降成空串。
 */
typedef enum tc_protocol_kind {
    TC_PROTO_AUTO = 0,      /* 按本机终端能力挑（同 C++ 的降级链） */
    TC_PROTO_SIXEL = 1,
    TC_PROTO_KITTY = 2,
    TC_PROTO_ITERM2 = 3,
    TC_PROTO_HALFBLOCK = 4  /* 半块字符降级：只要有 truecolor 就能显示 */
} tc_protocol_kind;

typedef struct tc_protocol_opts {
    uint32_t cell_width;   /* 单元格像素宽，0 = 默认 10（kitty / sixel 用来算格数） */
    uint32_t cell_height;  /* 单元格像素高，0 = 默认 20 */
    uint8_t rle;           /* 非 0 = 开启 sixel 游程压缩（默认开） */
    uint8_t reserved;      /* 对齐占位，传 0 */
} tc_protocol_opts;

/* 编码成终端协议字节流（两段式缓冲，同 tc_image_encode）。
 * 显式指定的 kind 一律照做（即便终端不支持，也便于落文件 / 转发）；
 * TC_PROTO_AUTO 才会按环境探测并可能返回 TC_ERR_UNSUPPORTED。 */
tc_status tc_image_to_protocol(tc_image_handle image, tc_protocol_kind kind,
                               const tc_protocol_opts* opts, char* buf, uint64_t cap,
                               uint64_t* out_len, tc_error_handle* err);

/* 本机终端探测：写入推荐协议名——"kitty" / "iterm2" / "sixel" / "halfblock" / "none"。
 * 两段式缓冲（结果很短，可先按 32 字节给 cap）。 */
tc_status tc_protocol_detect(char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err);

/* ── 图表 → 终端协议（一步到位，省掉 tc_image_* 中转）────────────────────────
 * 等价于「渲染成 PNG 再 tc_image_to_protocol」，但像素只在内存里流转一次。
 * 供 Python 的 ``plt.show_image()`` 这类"直接贴进终端"的用法。 */
tc_status tc_chart_to_protocol(tc_chart_handle chart, tc_protocol_kind kind,
                               const tc_plot_opts* opts, const tc_protocol_opts* proto_opts,
                               char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err);

/* ── 关系图 / 示意图（对齐 graph::diagrams 的 12 个 (Graph&, Canvas&, opts) 入口）
 *
 * 切法与图表完全一样：**一个句柄 + 一条属性通道**，没有 per-图种工厂。
 *
 *   tc_graph_create(kind)                        空图；kind 决定布局 + 渲染器
 *   tc_graph_from_text(text, format, kind)        文本方言 → 同一张 Graph（走 dsl 注册表）
 *   tc_graph_add_node / tc_graph_add_edge        建图
 *   tc_graph_set_node_attr / set_edge_attr        图种专属标记（如状态机的 initial/final）
 *   tc_graph_set_prop / set_style                 通用外观
 *   tc_graph_render_ansi / print / to_png / to_protocol   出图
 *
 * 为什么值得这么切：mermaid / dot / d2 / plantuml 的解析器、Sugiyama 分层、
 * 正交路由、箭头与标签避让全在 C++ 里。绑定层只需要「Text → 字节流」一条
 * 通道，Python 就能拿到与 C++ 逐字节相同的流程图——而不是在 Python 里重画一遍。
 *
 * 登记的 kind（{Graph&, Canvas&, opts} 一族；参数选装见 tc_graph_prop 的能力表）：
 *   "flowchart" / "state_machine" / "mindmap" / "orgchart" /
 *   "dag" / "dependency" / "er_diagram" / "knowledge_graph" /
 *   "social_network" / "uml_activity" / "uml_class" / "uml_usecase"
 *
 * 刻意**不**登记 "node_link"：它只画不布局，单独暴露只会得到叠在原点的空框。
 * chord / sankey / metro_map / syntax_tree / petri_net 等需要专属数据结构的
 * 图种同样不在此列——它们不是「节点 + 边」模型，硬塞进来就得发明私货 API。
 */
typedef struct tc_graph* tc_graph_handle;

/* 节点形状（一一对应 graph::ShapeKind，值即枚举序，勿改顺序）。 */
typedef enum tc_graph_shape {
    TC_SHAPE_BOX = 0,
    TC_SHAPE_ROUNDED = 1,
    TC_SHAPE_DIAMOND = 2,
    TC_SHAPE_ELLIPSE = 3,
    TC_SHAPE_STADIUM = 4,
    TC_SHAPE_HEXAGON = 5,
    TC_SHAPE_CYLINDER = 6,
    TC_SHAPE_ICON = 7
} tc_graph_shape;

/* 路由策略（一一对应 graph::RouteStyle）。 */
typedef enum tc_graph_route {
    TC_ROUTE_STRAIGHT = 0,
    TC_ROUTE_ORTHOGONAL = 1
} tc_graph_route;

/* 通用属性。语义与 tc_chart_set_prop 一致：kind 不支持该属性时返回
 * TC_ERR_UNSUPPORTED（fail-loud，不静默丢弃——见下方能力表）。 */
typedef enum tc_graph_prop {
    TC_GRAPH_PROP_SHAPE = 1,        /* TC_VAL_INT：tc_graph_shape */
    TC_GRAPH_PROP_ROUTE = 2,        /* TC_VAL_INT：tc_graph_route */
    TC_GRAPH_PROP_EDGE_LABELS = 3,  /* TC_VAL_BOOL：边上标注关系名 */
    TC_GRAPH_PROP_ARROWS = 4,       /* TC_VAL_BOOL：画箭头 */
    TC_GRAPH_PROP_NODE_BORDER = 5,  /* TC_VAL_BOOL：节点带边框 */
    TC_GRAPH_PROP_MIN_WIDTH = 6,    /* TC_VAL_DOUBLE：节点框最小宽（>0 生效） */
    TC_GRAPH_PROP_MIN_HEIGHT = 7,   /* TC_VAL_DOUBLE */
    TC_GRAPH_PROP_H_GAP = 8,        /* TC_VAL_DOUBLE：同层水平间距（>0 生效） */
    TC_GRAPH_PROP_V_GAP = 9,        /* TC_VAL_DOUBLE：层间垂直间距（>0 生效） */
    TC_GRAPH_PROP_MARK_CYCLES = 10  /* TC_VAL_BOOL：环上的边改成双向箭头（仅 dependency） */
} tc_graph_prop;

/* 建一张空图。kind 不在登记表里 → 返回 NULL 并写 err（不做"猜一个相近的"）。
 * kind 传 NULL / "" 等价于 "flowchart"（C++ 示例与文档的默认图种）。 */
tc_graph_handle tc_graph_create(const char* kind, tc_error_handle* err);

/* 文本方言 → 图。format：
 *   "mermaid"（含 flowchart / classDiagram / stateDiagram 自动识别）/ "dot" /
 *   "d2" / "plantuml" / 以及 dsl 注册表里其余名字；
 *   传 NULL 或 "" = 依次尝试注册表里的方言，第一个解析成功者胜出。
 * kind 传 NULL = "flowchart"（与 C++ 示例一致）。解析失败返回 NULL + err（含
 * 失败方言名与 ErrorCode；契约与 C++ 注册表的 Result 完全一致，不在这里另造一套
 * 更"详尽"的诊断）。 */
tc_graph_handle tc_graph_from_text(const char* text, const char* format, const char* kind,
                                   tc_error_handle* err);
void tc_graph_free(tc_graph_handle graph);

/* 加节点 / 加边。节点返回 id（0 = 失败：空图 / 句柄已释放）。
 * 边端点不存在 → TC_ERR_INVALID_ARG。label 可为 NULL（无标注）。 */
uint64_t tc_graph_add_node(tc_graph_handle graph, const char* label);
tc_status tc_graph_add_edge(tc_graph_handle graph, uint64_t from, uint64_t to, const char* label,
                            tc_error_handle* err);

/* 图种专属标记（属性袋的通用写入口，不新增专用函数）。
 * 例：mindmap / 状态机用 key="kind"、value="initial" / "final"。 */
tc_status tc_graph_set_node_attr(tc_graph_handle graph, uint64_t node, const char* key,
                                 const char* value, tc_error_handle* err);
tc_status tc_graph_set_edge_attr(tc_graph_handle graph, uint64_t from, uint64_t to,
                                 const char* key, const char* value, tc_error_handle* err);

tc_status tc_graph_set_prop(tc_graph_handle graph, tc_graph_prop prop, const tc_value* value,
                            tc_error_handle* err);
/* 节点 / 边主色。两端都可为 NULL（NULL = 保持默认）。 */
tc_status tc_graph_set_style(tc_graph_handle graph, const tc_style* node_style,
                             const tc_style* edge_style, tc_error_handle* err);

/* 规模查询（用于 Python 侧断言 / 调试）。 */
uint64_t tc_graph_node_count(tc_graph_handle graph);
uint64_t tc_graph_edge_count(tc_graph_handle graph);

/* 出图。opts->width / height 为 0 时按包围盒自适应（上限 200×100），给了就
 * 按给的尺寸画（超出部分由 canvas 裁剪——与 C++ 一致，不偷偷缩放）。
 * 同一次调用即完成布局，因此同一句柄反复渲染结果稳定。 */
tc_status tc_graph_render_ansi(tc_graph_handle graph, const tc_plot_opts* opts, char* buf,
                               uint64_t cap, uint64_t* out_len, tc_error_handle* err);
tc_status tc_graph_print(tc_graph_handle graph, const tc_plot_opts* opts, tc_error_handle* err);
tc_status tc_graph_to_png(tc_graph_handle graph, const tc_plot_opts* opts, char* buf, uint64_t cap,
                          uint64_t* out_len, tc_error_handle* err);
tc_status tc_graph_to_protocol(tc_graph_handle graph, tc_protocol_kind kind,
                               const tc_plot_opts* opts, const tc_protocol_opts* proto_opts,
                               char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err);

/* ── 文档（对齐 document/ + backend/structured：HTML / Markdown / 纯文本）──────
 *
 * 文档是"块序列"模型：标题 / 段落 / 代码 / 表格 / 图表。这里只做块的追加与
 * 一次性导出，排版与转义全在 backend::structured 里（与 C++ 同一份实现）。
 * 因此 Python 拿到的 HTML 与 C++ examples/36 逐字节相同。
 */
typedef struct tc_doc* tc_doc_handle;

tc_doc_handle tc_doc_create(tc_error_handle* err);
void tc_doc_free(tc_doc_handle doc);

tc_status tc_doc_add_heading(tc_doc_handle doc, const char* text, uint32_t level,
                             tc_error_handle* err);
tc_status tc_doc_add_paragraph(tc_doc_handle doc, const char* text, tc_error_handle* err);
tc_status tc_doc_add_code(tc_doc_handle doc, const char* text, const char* language,
                          tc_error_handle* err);
/* 表格：header 为列名（n_cols 个，可为 NULL），rows 为 row_mask 行、每行 n_cols 个单元
 * （单元可为 NULL → 空串）。二维指针形状与 tc_chart_set_text 一致。 */
tc_status tc_doc_add_table(tc_doc_handle doc, const char* const* header, uint64_t n_cols,
                           const char* const* const* rows, uint64_t row_count,
                           tc_error_handle* err);
/* 图表块：把 tc_chart_handle 转成矢量场景（走 api::chart_to_scene，与 SVG 同一份
 * 几何），再作为文档里的一张图。图种未迁移到 Scene 时返回 TC_ERR_UNSUPPORTED。 */
tc_status tc_doc_add_chart(tc_doc_handle doc, tc_chart_handle chart, uint32_t width,
                           uint32_t height, tc_error_handle* err);

/* 一次性导出。format："html" / "markdown" / "plain"（其余 → TC_ERR_INVALID_ARG）。
 * title / css_class 语义与 backend::structured::StructuredOptions 逐字段相同
 * （title = 文档标题，css_class = body 的 class），不为 NULL 才覆盖默认值。
 * 两段式缓冲。 */
tc_status tc_doc_export(tc_doc_handle doc, const char* format, const char* title,
                        const char* css_class, char* buf, uint64_t cap, uint64_t* out_len,
                        tc_error_handle* err);
/* 导出并写文件（扩展名不参与推断，format 语义同 tc_doc_export）。 */
tc_status tc_doc_save(tc_doc_handle doc, const char* path, const char* format,
                      const char* title, const char* css_class, tc_error_handle* err);

/* ══════════════════════════════════════════════════════════════════════════
 * 终端控制 / 自绘画布 / 增量渲染 / 输入与命中 / 导出物种 / 时序图 /
 * 多帧编码 / 富文本 / 3D·4D。
 * 骨架规则不变：下面全部是新增入口，已有签名与状态码不动。
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── 终端控制（term/：C 面把 Terminal 的 RAII 变成显式 handle）────────
 * tc_term_create 构造并 enter（失败返回 NULL）；tc_term_free 负责 leave。
 * 进程崩溃/abort 不走 free，需要可靠恢复请用 C++ 侧 Terminal 或注册 atexit。
 */
typedef struct tc_term* tc_term_handle;

typedef struct tc_term_opts {
    uint8_t raw_mode;      /* 1 = 关行缓冲/回显，按键即刻到达 */
    uint8_t alt_screen;    /* 1 = 备用屏幕 */
    uint8_t hide_cursor;   /* 1 = 隐藏光标 */
    uint8_t mouse;         /* 1 = 启用鼠标（SGR 编码 + ButtonMotion） */
    uint8_t reserved[4];
} tc_term_opts;

tc_term_opts tc_term_opts_default(void);
tc_term_handle tc_term_create(const tc_term_opts* opts, tc_error_handle* err);
void tc_term_free(tc_term_handle term);

typedef struct tc_term_size { uint16_t cols; uint16_t rows; } tc_term_size;

/* query_size 从 stdout 查询；失败时 err 给出原因，out 不变。 */
tc_status tc_term_size_query(tc_term_size* out, tc_error_handle* err);
/* size_from_env 只读环境变量（COLUMNS/LINES），永不失败，缺省 80×24。 */
tc_term_size tc_term_size_from_env(void);

typedef struct tc_term_caps {
    uint8_t unicode;       /* 1 = 支持 Unicode */
    uint8_t color_depth;   /* tc_color_depth */
    uint8_t mouse;
    uint8_t alt_screen;
    uint8_t title_setting;
    uint8_t reserved[3];
} tc_term_caps;

/* force_tty=1 时按 tty 探测；0 时按 stdout_is_tty 的实际结果。 */
tc_status tc_term_caps_detect(tc_term_caps* out, uint8_t force_tty);
uint8_t tc_term_stdout_is_tty(void);
uint8_t tc_term_stdin_is_tty(void);

/* 以下四条直接写 stdout（与 C++ 同名函数行为一致），返回首条错误。
 * clipboard_set 用 OSC 52 的 'c'（系统剪贴板），_primary 变体用 'p'（X11 主选择区）。
 * mode: 0=Off, 9=X10, 1000=Normal, 1002=ButtonMotion, 1003=AnyMotion。
 * encoding: 0=Default, 1005=UTF8, 1006=SGR, 1015=Urxvt。enable=0 时忽略 mode/encoding。
 */
tc_status tc_term_clipboard_set(const char* text, tc_error_handle* err);
tc_status tc_term_clipboard_set_primary(const char* text, tc_error_handle* err);
tc_status tc_term_hyperlink(const char* uri, const char* id,
                            const char* text, tc_error_handle* err);
tc_status tc_term_hyperlink_close(tc_error_handle* err);
tc_status tc_term_mouse(uint8_t enable, uint16_t mode,
                        uint16_t encoding, tc_error_handle* err);

/* ── 自绘画布（canvas/）+ 增量渲染（render/）────────────────────────────
 * Canvas 负责落格，Renderer 负责与上一帧比较后输出差量字节。
 * 两者分开：一个 Renderer 可服务多个画布（如多标签页）。
 */
typedef struct tc_canvas* tc_canvas_handle;

tc_canvas_handle tc_canvas_create(uint32_t width, uint32_t height, tc_error_handle* err);
void tc_canvas_free(tc_canvas_handle canvas);
/* resize 保留左上角内容；0 = 成功，-1 = 尺寸非法（宽高为 0）。不写 err。 */
int32_t tc_canvas_resize(tc_canvas_handle canvas, uint32_t width, uint32_t height);
uint32_t tc_canvas_width(tc_canvas_handle canvas);
uint32_t tc_canvas_height(tc_canvas_handle canvas);
void tc_canvas_clear(tc_canvas_handle canvas);
/* 下面三个返回 1 = 已落格，0 = 被裁剪；style=NULL 用默认样式。 */
uint8_t tc_canvas_put(tc_canvas_handle canvas, int32_t x, int32_t y,
                      uint32_t codepoint, const tc_style* style);
uint8_t tc_canvas_text(tc_canvas_handle canvas, int32_t x, int32_t y,
                       const char* utf8, const tc_style* style);
uint8_t tc_canvas_line(tc_canvas_handle canvas,
                       int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                       uint32_t codepoint, const tc_style* style);
void tc_canvas_fill_rect(tc_canvas_handle canvas,
                         int32_t x, int32_t y, uint32_t w, uint32_t h,
                         uint32_t codepoint, const tc_style* style);

/* 统一输出描述：一个格式枚举 + 一个 opts，避免每种格式各写一对函数。
 * format 取 tc_output_format；color_depth 用于 ANSI/TEXT/HTML_PRE；
 * protocol 用于 TC_OUT_PROTOCOL；width/height=0 表示沿用对象自身尺寸。
 */
typedef enum tc_output_format {
    TC_OUT_ANSI = 0,
    TC_OUT_TEXT = 1,
    TC_OUT_HTML_PRE = 2,
    TC_OUT_JSON = 3,
    TC_OUT_PNG = 4,
    TC_OUT_SVG = 5,
    TC_OUT_PROTOCOL = 6,
    TC_OUT_PDF = 7,
    TC_OUT_EPS = 8,
    TC_OUT_LATEX = 9
} tc_output_format;

typedef struct tc_output_opts {
    uint8_t format;        /* tc_output_format */
    uint8_t color_depth;   /* tc_color_depth */
    uint8_t protocol;      /* tc_protocol_kind */
    uint8_t reserved;
    uint32_t width;        /* 0 = 沿用对象自身尺寸 */
    uint32_t height;
    uint32_t cell_width;   /* TC_OUT_PROTOCOL：0 = 10 */
    uint32_t cell_height;  /* TC_OUT_PROTOCOL：0 = 20 */
} tc_output_opts;

tc_output_opts tc_output_opts_default(void);

/* 画布/时序图/3D 的统一两段式导出（cap=0 只查询长度）。 */
tc_status tc_canvas_render(tc_canvas_handle canvas,
                           const tc_output_opts* out_opts,
                           char* buf, uint64_t cap, uint64_t* out_len,
                           tc_error_handle* err);

typedef struct tc_renderer* tc_renderer_handle;

tc_renderer_handle tc_renderer_create(uint8_t color_depth, tc_error_handle* err);
void tc_renderer_free(tc_renderer_handle renderer);
void tc_renderer_reset(tc_renderer_handle renderer);
/* 与内部 prev 比较，首帧/尺寸变化自动全量；cap=0 只查询长度。 */
tc_status tc_renderer_frame(tc_renderer_handle renderer, tc_canvas_handle canvas,
                            char* buf, uint64_t cap, uint64_t* out_len,
                            tc_error_handle* err);

/* ── 输入解析与命中测试 ────────────────────────────────────────────────
 * InputParser 有跨 feed 的状态（UTF-8 半包、转义序列半包），所以是句柄。
 * Event 里的 text 指向 parser 句柄内部，下次 feed/free 前有效。
 */
typedef enum tc_event_kind {
    TC_EVENT_KEY = 0, TC_EVENT_MOUSE = 1, TC_EVENT_RESIZE = 2,
    TC_EVENT_FOCUS = 3, TC_EVENT_PASTE = 4, TC_EVENT_UNKNOWN = 5
} tc_event_kind;

typedef enum tc_key {
    TC_KEY_UNKNOWN = 0, TC_KEY_CHAR = 1, TC_KEY_ENTER = 2, TC_KEY_TAB = 3,
    TC_KEY_BACKSPACE = 4, TC_KEY_ESCAPE = 5, TC_KEY_UP = 6, TC_KEY_DOWN = 7,
    TC_KEY_LEFT = 8, TC_KEY_RIGHT = 9, TC_KEY_HOME = 10, TC_KEY_END = 11,
    TC_KEY_PAGE_UP = 12, TC_KEY_PAGE_DOWN = 13, TC_KEY_INSERT = 14,
    TC_KEY_DELETE = 15, TC_KEY_F1 = 16, TC_KEY_F2 = 17, TC_KEY_F3 = 18,
    TC_KEY_F4 = 19, TC_KEY_F5 = 20, TC_KEY_F6 = 21, TC_KEY_F7 = 22,
    TC_KEY_F8 = 23, TC_KEY_F9 = 24, TC_KEY_F10 = 25, TC_KEY_F11 = 26,
    TC_KEY_F12 = 27
} tc_key;

typedef enum tc_mouse_button {
    TC_MOUSE_NONE = 0, TC_MOUSE_LEFT = 1, TC_MOUSE_MIDDLE = 2, TC_MOUSE_RIGHT = 3
} tc_mouse_button;

typedef enum tc_mouse_action {
    TC_MOUSE_MOVE = 0, TC_MOUSE_PRESS = 1, TC_MOUSE_RELEASE = 2,
    TC_MOUSE_SCROLL_UP = 3, TC_MOUSE_SCROLL_DOWN = 4
} tc_mouse_action;

typedef struct tc_event {
    uint8_t kind;          /* tc_event_kind */
    uint8_t key;           /* tc_key（kind=KEY） */
    uint8_t button;        /* tc_mouse_button（kind=MOUSE） */
    uint8_t action;        /* tc_mouse_action（kind=MOUSE） */
    uint8_t mods;          /* shift=1 / alt=2 / ctrl=4 */
    uint8_t focused;       /* kind=FOCUS */
    uint8_t reserved[2];
    uint32_t codepoint;    /* kind=KEY 且 key=CHAR */
    int32_t x, y;          /* kind=MOUSE，1-based 终端坐标 */
    int32_t cols, rows;    /* kind=RESIZE */
    const char* text;      /* kind=PASTE，parser 句柄持有 */
    uint64_t text_len;
} tc_event;

typedef struct tc_input* tc_input_handle;

tc_input_handle tc_input_create(tc_error_handle* err);
void tc_input_free(tc_input_handle input);
void tc_input_reset(tc_input_handle input);
/* cap=0 只查询事件数；out_count 填实际写入数。 */
tc_status tc_input_feed(tc_input_handle input, const uint8_t* bytes, uint64_t n,
                        tc_event* out, uint64_t cap, uint64_t* out_count,
                        tc_error_handle* err);

/* 命中区域：C++ 的 chart 不自动发布 HitRegion，调用方按坐标系自己登记。
 * 返回 payload（即调用方想绑定的索引），hit 结果按登记顺序自下而上返回。
 */
typedef struct tc_regions* tc_regions_handle;

tc_regions_handle tc_regions_create(tc_error_handle* err);
void tc_regions_free(tc_regions_handle regions);
void tc_regions_clear(tc_regions_handle regions);
uint64_t tc_regions_count(tc_regions_handle regions);
uint64_t tc_regions_add_rect(tc_regions_handle regions,
                             int32_t x, int32_t y, uint32_t w, uint32_t h,
                             uint32_t payload);
uint64_t tc_regions_add_circle(tc_regions_handle regions,
                               int32_t cx, int32_t cy, double radius,
                               uint32_t payload);
uint64_t tc_regions_add_polygon(tc_regions_handle regions,
                                const int32_t* xy, uint64_t point_count,
                                uint32_t payload, tc_error_handle* err);
/* cap=0 只查询命中数量。 */
tc_status tc_regions_hit(tc_regions_handle regions, double x, double y,
                         uint32_t* payloads, uint64_t cap, uint64_t* out_count);

/* ── 更多导出物种 ──────────────────────────────────────────────────────
 * tc_chart_export 覆盖 to_svg/to_png 之外的物种；SVG/PNG 仍可继续用老入口。
 * tc_chart_to_image 把图渲染成 image 句柄，之后任意 image 编码格式都可用。
 */
tc_status tc_chart_export(tc_chart_handle chart,
                          const tc_output_opts* out_opts,
                          const tc_plot_opts* plot_opts,
                          char* buf, uint64_t cap, uint64_t* out_len,
                          tc_error_handle* err);
tc_image_handle tc_chart_to_image(tc_chart_handle chart,
                                  const tc_plot_opts* opts,
                                  tc_error_handle* err);

/* ── 时序图（dsl::parse_sequence → graph::render_uml_sequence）─────────
 * 与 tc_graph_* 平行：输入是 mermaid sequenceDiagram 文本，不是节点/边模型。
 */
typedef struct tc_seq* tc_seq_handle;

tc_seq_handle tc_seq_from_text(const char* mermaid_text, tc_error_handle* err);
void tc_seq_free(tc_seq_handle seq);
uint64_t tc_seq_participant_count(tc_seq_handle seq);
uint64_t tc_seq_message_count(tc_seq_handle seq);
tc_status tc_seq_render(tc_seq_handle seq, const tc_output_opts* out_opts,
                        char* buf, uint64_t cap, uint64_t* out_len,
                        tc_error_handle* err);

/* ── 多帧编码（GIF 动画 / MP4 视频 / AV1 IVF）───────────────────────────
 * frames 为 count 个 image 句柄；format 取 "gif" / "mp4" / "av1"（别名 ivf/avif）。
 * AV1 各帧须尺寸一致、宽高取 64 的倍数且至少一维为 64，编码为 KEY 帧序列。
 * quality 与 fps 对齐 image::EncodeOptions；AV1 使用其已验证的帧内基线参数。
 */
tc_status tc_image_encode_frames(const tc_image_handle* frames, uint64_t count,
                                 const char* format, uint8_t quality, uint32_t fps,
                                 char* buf, uint64_t cap, uint64_t* out_len,
                                 tc_error_handle* err);

/* ── 富文本段落（document::Span）───────────────────────────────────────
 * 与 tc_doc_add_paragraph 并列：传 spans 数组，每个 span 自带样式。
 */
typedef struct tc_span {
    const char* text;
    tc_style style;
} tc_span;

tc_status tc_doc_add_rich_paragraph(tc_doc_handle doc,
                                    const tc_span* spans, uint64_t count,
                                    tc_error_handle* err);

/* ── 3D / 4D 图（chart3d / chart4d）─────────────────────────────────────
 * kind: "scatter3d" / "surface3d" / "wire4d"。
 * scatter3d 的点是 3 元组 (x,y,z)；wire4d 是 4 元组 (x,y,z,w)。
 * surface3d 用 set_grid 传 z 网格 + set_range 传 x/y 范围，内部双线性采样。
 * wire4d 不传 edges 时按顶点输入顺序 i→i+1 自动连线。
 */
typedef struct tc_scene3d* tc_scene3d_handle;

tc_scene3d_handle tc_scene3d_create(const char* kind, tc_error_handle* err);
void tc_scene3d_free(tc_scene3d_handle scene);
tc_status tc_scene3d_set_points(tc_scene3d_handle scene, const double* coords,
                                uint64_t count, tc_error_handle* err);
tc_status tc_scene3d_set_grid(tc_scene3d_handle scene, const double* z,
                              uint32_t cols, uint32_t rows, tc_error_handle* err);
tc_status tc_scene3d_set_edges(tc_scene3d_handle scene, const uint32_t* pairs,
                               uint64_t count, tc_error_handle* err);
tc_status tc_scene3d_set_view(tc_scene3d_handle scene,
                              double azimuth_deg, double elevation_deg,
                              double fovy_deg);
/* axis: 0 = x, 1 = y；仅 surface3d 使用。lo >= hi 报 TC_ERR_INVALID_ARG。 */
tc_status tc_scene3d_set_range(tc_scene3d_handle scene, uint8_t axis,
                               double lo, double hi, tc_error_handle* err);
/* wire4d 的 4D 旋转平面角度（度）；缺省 20/15。 */
tc_status tc_scene3d_set_rotation4d(tc_scene3d_handle scene,
                                    double xw_deg, double yz_deg);
tc_status tc_scene3d_render(tc_scene3d_handle scene,
                            const tc_output_opts* out_opts,
                            char* buf, uint64_t cap, uint64_t* out_len,
                            tc_error_handle* err);

/* 释放 render 返回的 C 侧字符串。 */
void tc_free_string(char* s);

/* 取错误消息写入 buf（最多 cap-1 字符 + '\0'）。 */
void tc_error_message(tc_error_handle err, char* buf, uint64_t cap);
void tc_error_free(tc_error_handle err);

#ifdef __cplusplus
}
#endif

#endif /* TERMCHART_CAPI_H */
