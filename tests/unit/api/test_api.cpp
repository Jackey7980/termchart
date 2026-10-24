// test_api.cpp —— api 层（Opts / 渲染入口 / 输出封装）
//
// api 是唯一允许碰全部模块的层，也是 GUI 平行栈将来要对齐的"对外契约面"。
// 关键契约三条：
//   1. Opts 每项都有默认值，零配置可用；
//   2. render_line **只产出字节，不写 stdout**（headless / 测试 / GUI 内嵌都依赖）；
//   3. 参数非法返回 Result 错误，不抛异常。
#include <doctest.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "termchart/api/autoviz_plot.hpp"
#include "termchart/api/chart.hpp"
#include "termchart/api/opts.hpp"
#include "termchart/api/print.hpp"
#include "termchart/api/quick.hpp"
#include "termchart/chart/cartesian/line_chart.hpp"
#include "termchart/chart/series.hpp"
#include "termchart/core/error.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/util/logging.hpp"

namespace {

using namespace termchart;
using namespace termchart::api;

}  // namespace

TEST_CASE("api: Opts 默认全空且链式可写") {
    const Opts defaults;
    CHECK_FALSE(defaults.width().has_value());
    CHECK_FALSE(defaults.height().has_value());
    CHECK_FALSE(defaults.color_depth().has_value());
    CHECK(defaults.unicode());
    CHECK(defaults.newline());
    CHECK(defaults.reset_at_end());

    Opts opts;
    opts.width(80).height(24).color_depth(ColorDepth::Ansi256).ascii_only().newline(false);
    CHECK(opts.width() == std::optional<std::uint16_t>(80));
    CHECK(opts.height() == std::optional<std::uint16_t>(24));
    CHECK(opts.color_depth() == ColorDepth::Ansi256);
    CHECK_FALSE(opts.unicode());
    CHECK_FALSE(opts.newline());
}

TEST_CASE("api: render_line 一行文本 → 带样式的字节流") {
    const Result<std::string> out = render_line("hi", Style{}.set_fg(Color::rgb(1, 2, 3)));
    REQUIRE(out.has_value());
    CHECK(out->find("hi") != std::string::npos);
    CHECK(out->find("\x1b[") != std::string::npos);
    CHECK(out->back() == '\n');  // 默认补换行
}

TEST_CASE("api: 显式宽度 0 是参数错误，不猜也不钳位") {
    Opts zero;
    zero.width(0);
    const Result<std::string> out = render_line("hi", Style{}, zero);
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("api: 宽度不足时截断而不是抛异常") {
    Opts narrow;
    narrow.width(2);
    const Result<std::string> out = render_line("hello", Style{}, narrow);
    REQUIRE(out.has_value());
    CHECK(out->find("he") != std::string::npos);
    CHECK(out->find("hello") == std::string::npos);
}

TEST_CASE("api: 空文本退化成一格而不是报错") {
    const Result<std::string> out = render_line("", Style{});
    REQUIRE(out.has_value());
    CHECK_FALSE(out->empty());
}

TEST_CASE("api: 深度降级由 render/sgr 统一接管") {
    Opts opts;
    opts.color_depth(ColorDepth::Ansi16);
    const Result<std::string> out = render_line("x", Style{}.set_fg(Color::rgb(200, 10, 10)), opts);
    REQUIRE(out.has_value());
    CHECK(out->find("38;2;200") == std::string::npos);  // 真彩参数不得出现
    CHECK(out->find("\x1b[") != std::string::npos);
    CHECK(out->find("m") != std::string::npos);
}

TEST_CASE("api: 一行出图 render_chart（Chart → 字节）") {
    chart::LineChart chart;
    chart.set_title("one-liner");
    chart::Series series;
    series.name = "y = x";
    for (int i = 0; i < 10; ++i) {
        series.points.push_back(Point{static_cast<double>(i), static_cast<double>(i)});
    }
    chart.add_series(std::move(series));

    Opts opts;
    opts.width(60).height(16);
    const Result<std::string> out = render_chart(chart, opts);
    REQUIRE(out.has_value());
    CHECK_FALSE(out->empty());
    CHECK(out->find("\x1b[") != std::string::npos);  // 有 SGR（轴/网格有颜色）

    // 零配置也要能出图（默认 80×24），不依赖任何外部状态。
    const Result<std::string> zero = render_chart(chart);
    REQUIRE(zero.has_value());
    CHECK_FALSE(zero->empty());
}

TEST_CASE("api: render_chart 显式尺寸 0 是参数错误") {
    chart::LineChart chart;
    chart::Series series;
    series.points.push_back(Point{0.0, 0.0});
    series.points.push_back(Point{1.0, 1.0});
    chart.add_series(std::move(series));

    Opts zero_w;
    zero_w.width(0);
    const Result<std::string> bad_w = render_chart(chart, zero_w);
    REQUIRE_FALSE(bad_w.has_value());
    CHECK(bad_w.error() == ErrorCode::InvalidArgument);

    Opts zero_h;
    zero_h.height(0);
    const Result<std::string> bad_h = render_chart(chart, zero_h);
    REQUIRE_FALSE(bad_h.has_value());
    CHECK(bad_h.error() == ErrorCode::InvalidArgument);
}

TEST_CASE("api: None 档位仍产出纯文本") {
    Opts opts;
    opts.color_depth(ColorDepth::None);
    const Result<std::string> out = render_line("plain", Style{}.set_fg(Color::rgb(1, 2, 3)), opts);
    REQUIRE(out.has_value());
    CHECK(out->find("plain") != std::string::npos);
    CHECK(out->find("38;2") == std::string::npos);
}

TEST_CASE("api: init_core 幂等且可查询") {
    init_core();
    init_core();
    CHECK(core_initialized());
}

TEST_CASE("api: write_to 写成功可读回，空写与空流都是明确结果") {
    std::FILE* stream = std::tmpfile();
    REQUIRE(stream != nullptr);

    CHECK(write_to(stream, "") == ErrorCode::Ok);  // 空写不是错误
    CHECK(write_to(stream, "abc") == ErrorCode::Ok);

    std::rewind(stream);
    char buffer[8] = {};
    const std::size_t got = std::fread(buffer, 1, sizeof(buffer) - 1, stream);
    CHECK(got == 3);
    CHECK(std::string(buffer, got) == "abc");

    CHECK(write_to(nullptr, "abc") == ErrorCode::InvalidArgument);
    CHECK(flush(stream) == ErrorCode::Ok);
    CHECK(flush(nullptr) == ErrorCode::InvalidArgument);
    std::fclose(stream);
}

TEST_CASE("api: escape_for_debug 让不可打印字节可读且不回显控制符") {
    const std::string escaped = escape_for_debug(std::string("\x1b[31m\n", 6));
    CHECK(escaped.find('\x1b') == std::string::npos);
    CHECK(escaped.find('\n') == std::string::npos);
    CHECK(escaped.find("<ESC>") != std::string::npos);
    CHECK(escaped.find("<LF>") != std::string::npos);
    CHECK(escaped.find("31m") != std::string::npos);
}

TEST_CASE("api: auto_plot 薄壳——地狱 CSV 出图、空路径/不存在文件 fail-loud") {
    // 落一个临时文件：auto_plot 的入口是文件路径，测试走同一条真实路径。
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "termchart_test_autoviz.csv";
    {
        std::FILE* out = std::fopen(path.string().c_str(), "wb");
        REQUIRE(out != nullptr);
        static const char kCsv[] =
            "ts,region,revenue\n"
            "2024-01-15 08:30:00,华东,\"1,234.50\"\n"
            "2024-01-16 09:00:00,华北,(890.25)\n"
            "2024-02-01 10:15:30,华南,¥2,000.00\n";
        std::fputs(kCsv, out);
        std::fclose(out);
    }

    CHECK(auto_plot("") == ErrorCode::InvalidArgument);            // 空路径
    CHECK(auto_plot("no/such/file.csv") == ErrorCode::InvalidArgument);  // 打不开
    CHECK(auto_plot(path.string()) == ErrorCode::Ok);              // 真出图（写 stdout）

    // 仅表头（0 数据行）：引擎层 InvalidArgument 穿透薄壳，不编造空图。
    const std::filesystem::path header_only =
        std::filesystem::temp_directory_path() / "termchart_test_autoviz_header.csv";
    {
        std::FILE* out = std::fopen(header_only.string().c_str(), "wb");
        REQUIRE(out != nullptr);
        std::fputs("a,b\n", out);
        std::fclose(out);
    }
    CHECK(auto_plot(header_only.string()) == ErrorCode::InvalidArgument);

    std::filesystem::remove(path);
    std::filesystem::remove(header_only);
}

TEST_CASE("api: auto_plot 对表头口径存疑列发 WARN（只提示，不改出图）") {
    // 故意错位：amount 列装的是支付方式、method 列装的是金额。
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "termchart_test_autoviz_suspect.csv";
    {
        std::FILE* out = std::fopen(path.string().c_str(), "wb");
        REQUIRE(out != nullptr);
        std::fputs("payment_id,pay_time,amount,method\n"
                   "P001,2026-03-01T08:00:00Z,ALIPAY,1234.50\n"
                   "P002,2026-03-01T09:00:00Z,WECHAT,200.00\n"
                   "P003,2026-03-01T10:00:00Z,CARD,99.90\n"
                   "P004,2026-03-01T11:00:00Z,ALIPAY,1500.00\n"
                   "P005,2026-03-01T12:00:00Z,WECHAT,75.25\n",
                   out);
        std::fclose(out);
    }

    termchart::util::capture_log_begin();
    const termchart::Status status = auto_plot(path.string());
    const std::vector<std::string> logs = termchart::util::capture_log_end();

    CHECK(status == ErrorCode::Ok);  // 提示不阻断出图
    REQUIRE(logs.size() == 2);       // 恰好两条（amount / method），不误报其他列
    CHECK(logs[0].find("warn") != std::string::npos);
    CHECK(logs[0].find("amount") != std::string::npos);
    CHECK(logs[0].find("numeric") != std::string::npos);
    CHECK(logs[1].find("method") != std::string::npos);
    CHECK(logs[1].find("categorical") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("api: quick::render_plot 一行折线（零配置 + 标题 + 尺寸）") {
    const std::vector<double> x{0, 1, 2, 3, 4};
    const std::vector<double> y{0, 1, 4, 9, 16};

    const Result<std::string> zero = render_plot(x, y);  // 默认 80×24
    REQUIRE(zero.has_value());
    CHECK_FALSE(zero->empty());
    CHECK(zero->find("\x1b[") != std::string::npos);  // 轴/网格有颜色

    PlotOpts po;
    po.width = 60;
    po.height = 16;
    po.title = "parabola";
    const Result<std::string> t = render_plot(x, y, po);
    REQUIRE(t.has_value());
    CHECK(t->find("parabola") != std::string::npos);  // 标题渲染进画面
}

TEST_CASE("api: quick::render_scatter / render_bar 一行出图") {
    const std::vector<double> x{0, 1, 2};
    const std::vector<double> y{2, 1, 3};
    const Result<std::string> sc = render_scatter(x, y);
    REQUIRE(sc.has_value());
    CHECK_FALSE(sc->empty());
    CHECK(sc->find("\x1b[") != std::string::npos);

    const std::vector<double> vals{3, 5, 2, 8};
    const std::vector<std::string> labels{"a", "b", "c", "d"};
    const Result<std::string> br = render_bar(vals, labels);
    REQUIRE(br.has_value());
    CHECK_FALSE(br->empty());
    CHECK(br->find("\x1b[") != std::string::npos);
}

TEST_CASE("api: quick::render_plot 尺寸 0 是参数错误，不钳位") {
    const std::vector<double> x{0, 1};
    const std::vector<double> y{0, 1};
    PlotOpts po;
    po.width = 0;
    const Result<std::string> bad = render_plot(x, y, po);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error() == ErrorCode::InvalidArgument);
}
