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
 * 下面五个 tc_chart_create_* 是兼容胶水，等价于 tc_chart_create("...")。 */
tc_chart_handle tc_chart_create(const char* kind, tc_error_handle* err);
tc_status tc_chart_kinds(char* buf, uint64_t cap, uint64_t* out_len, tc_error_handle* err);

tc_chart_handle tc_chart_create_line(void);
tc_chart_handle tc_chart_create_scatter(void);
tc_chart_handle tc_chart_create_bar(void);
tc_chart_handle tc_chart_create_pie(void);       /* 饼图：值 + 类别标签 */
tc_chart_handle tc_chart_create_sparkline(void); /* 迷你图：单行趋势 */
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

/* 二维矩阵入口（仅 Matrix 形状图种，如 contour/corr/mosaic/violin/pca/parallel）。
 * rows[i] 指向第 i 行 col_count 个 double；row_labels/col_labels 可 NULL。 */
tc_status tc_chart_set_matrix(tc_chart_handle chart, const double* const* rows,
                              uint64_t row_count, uint64_t col_count,
                              const char* const* row_labels, uint64_t n_row_labels,
                              const char* const* col_labels, uint64_t n_col_labels,
                              tc_error_handle* err);

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

/* 释放 render 返回的 C 侧字符串。 */
void tc_free_string(char* s);

/* 取错误消息写入 buf（最多 cap-1 字符 + '\0'）。 */
void tc_error_message(tc_error_handle err, char* buf, uint64_t cap);
void tc_error_free(tc_error_handle err);

#ifdef __cplusplus
}
#endif

#endif /* TERMCHART_CAPI_H */
