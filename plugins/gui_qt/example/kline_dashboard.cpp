// kline_dashboard.cpp —— termchart_gui 英雄 Demo（Qt 版，Take 2：宿主拥有窗口）：恒生指数日 K 线
//
// 与 Win32 版 candlestick_demo 同一份 widget_tree（ChartWidget「万能插座」+ HsiKLineRenderer
// 插头），只把窗口/事件泵换成 Take 2 宿主 QWidget：宿主自建 QApplication 与 QWidget，paintEvent
// 里调 backend.blit 贴帧，并把鼠标/滚轮/键盘事件经 tk_event_map 注入后端；每帧 QTimer 抽干
// 事件路由到控件树。演示层零平台代码——同一棵控件树可挂在 Win32 / Qt / Tk / X11 / Cocoa / JNI 任一后端。
// 滚轮缩放 / 拖拽平移 / 悬停十字轴 + OHLC 信息框 / 底部成交量副图 / 'T' 一键换肤 / 窗口可
// 自由缩放（resize 回流）/ 实时行情模拟（每 ~250ms 给最右蜡烛打 tick、定期开新蜡烛）全部复用。
//
// 跑法：kline_dashboard            → 开窗口交互（ESC / 关闭按钮退出）
//       kline_dashboard --smoke    → 无交互冒烟：泵数帧事件后自动关闭（CI 用）
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QResizeEvent>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <ctime>
#include <sstream>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/layout/flex.hpp"
#include "termchart_gui/theme.hpp"
#include "termchart_gui/widget_tree/chart_widget.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/hsi_kline_renderer.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui_qt/qt_backend.hpp"
#include "termchart/io/json.hpp"          // network 模块拉回的 JSON 解析
#include "termchart/network/http.hpp"     // 真实恒指行情的网络拉取
#include "termchart/gui/font.hpp"         // 运行时指定 GUI 字体（替代环境变量）

using namespace termchart_gui;
using namespace termchart::io;
using namespace termchart::network::http;

namespace {

inline std::uint32_t mods_of(Qt::KeyboardModifiers m) {
    std::uint32_t r = 0;
    if (m & Qt::ShiftModifier) r |= kModShift;
    if (m & Qt::ControlModifier) r |= kModCtrl;
    if (m & Qt::AltModifier) r |= kModAlt;
    return r;
}

// 宿主窗口：拥有 QWidget，把所有原生事件翻译成 termchart Event 注入后端。
class HostWidget final : public QWidget {
public:
    explicit HostWidget(termchart_gui_qt::QtBackend* backend, QWidget* parent = nullptr)
        : QWidget(parent), backend_(backend) {
        setMouseTracking(true);
        dpr_ = devicePixelRatioF();          // 高 DPI：记下设备像素比
        if (backend_ != nullptr) {
            backend_->set_device_pixel_ratio(dpr_);
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        if (backend_ != nullptr) {
            backend_->blit(painter);
        }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (backend_ != nullptr) {
            Event ev = tk_event_map::mouse_press(
                static_cast<int>(std::lround(e->pos().x() * dpr_)),
                static_cast<int>(std::lround(e->pos().y() * dpr_)),
                static_cast<int>(e->button()));
            ev.modifiers = mods_of(e->modifiers());
            backend_->inject_event(ev);
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (backend_ != nullptr) {
            Event ev = tk_event_map::mouse_release(
                static_cast<int>(std::lround(e->pos().x() * dpr_)),
                static_cast<int>(std::lround(e->pos().y() * dpr_)),
                static_cast<int>(e->button()));
            ev.modifiers = mods_of(e->modifiers());
            backend_->inject_event(ev);
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (backend_ != nullptr) {
            Event ev = tk_event_map::mouse_move(
            static_cast<int>(std::lround(e->pos().x() * dpr_)),
            static_cast<int>(std::lround(e->pos().y() * dpr_)));
            ev.modifiers = mods_of(e->modifiers());
            backend_->inject_event(ev);
        }
    }
    void wheelEvent(QWheelEvent* e) override {
        if (backend_ != nullptr) {
            Event ev{};
            ev.type = EventType::Wheel;
            ev.x = e->position().x() * dpr_;
            ev.y = e->position().y() * dpr_;
            ev.wheel_delta = static_cast<double>(e->angleDelta().y()) / 120.0;
            ev.modifiers = mods_of(e->modifiers());
            backend_->inject_event(ev);
        }
    }
    void keyPressEvent(QKeyEvent* e) override {
        if (backend_ != nullptr) {
            const QByteArray t = e->text().toUtf8();
            backend_->inject_event(
                tk_event_map::key_press(e->key(), std::string(t.constData(),
                                                             static_cast<std::size_t>(t.size()))));
        }
    }
    void closeEvent(QCloseEvent* e) override {
        if (backend_ != nullptr) {
            backend_->inject_event(tk_event_map::quit());
        }
        QWidget::closeEvent(e);
    }
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        dpr_ = devicePixelRatioF();
        if (backend_ != nullptr) {
            backend_->set_device_pixel_ratio(dpr_);
            // 按设备像素比把离屏画布渲染到物理分辨率，避免位图被拉伸发糊。
            backend_->resize(
                static_cast<std::uint32_t>(std::lround(e->size().width() * dpr_)),
                static_cast<std::uint32_t>(std::lround(e->size().height() * dpr_)));
        }
    }

private:
    termchart_gui_qt::QtBackend* backend_;
    double dpr_ = 1.0;  // 设备像素比（高 DPI 屏 >1）
};

}  // namespace

// 简易高斯噪声（Box-Muller），演示用。
static double gauss(double sigma) {
    static std::mt19937 rng(20260911);
    double u = 1.0 - (static_cast<double>(rng()) + 0.5) / (rng.max() + 1.0);
    double v = (static_cast<double>(rng()) + 0.5) / (rng.max() + 1.0);
    return sigma * std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * 3.141592653589793 * v);
}

// 用 network 模块拉真实恒指（^HSI）日K；失败（离线 / TLS 后端缺失 / 解析错误）回退模拟种子。
struct HsiSeries {
    bool ok = false;
    std::vector<HsiKLineRenderer::Candle> candles;
    std::vector<double> vols;
    std::vector<std::string> labels;
};

static std::string format_hsi_date(std::int64_t unix_secs) {
    std::time_t t = static_cast<std::time_t>(unix_secs);
    const std::tm* tm = std::gmtime(&t);
    if (tm == nullptr) return std::string();
    char buf[16];
    std::strftime(buf, sizeof(buf), "%m-%d", tm);
    return std::string(buf);
}

static HsiSeries fetch_hsi_yahoo() {
    HsiSeries out;
    constexpr const char* url =
        "https://query1.finance.yahoo.com/v8/finance/chart/%5EHSI?interval=1d&range=3mo";
    HttpOptions opts;
    opts.socket.connect_timeout_ms = 1000;
    opts.socket.io_timeout_ms = 2000;  // 离线时快速回退模拟
    Request req;
    req.method = "GET";
    auto uri = Uri::parse(url);
    if (!uri) { std::fprintf(stderr, "kline: 内部错误（URL 解析失败）\n"); return out; }
    req.uri = std::move(*uri);
    req.headers.emplace_back(
        "User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36");
    auto resp = execute(req, opts);
    if (!resp) { std::fprintf(stderr, "kline: 网络拉取失败，回退模拟数据\n"); return out; }
    if (resp->status != 200) {
        std::fprintf(stderr, "kline: HTTP %u，回退模拟数据\n", resp->status);
        return out;
    }
    auto j = parse_json(resp->body);
    if (!j) { std::fprintf(stderr, "kline: JSON 解析失败，回退模拟数据\n"); return out; }
    const auto* chart = j->find("chart");
    if (chart == nullptr) return out;
    const JsonValue& result_arr = chart->at("result");
    if (!result_arr.is_array() || result_arr.size() == 0) return out;
    const JsonValue& res0 = result_arr.at(0);
    const JsonValue& ts = res0.at("timestamp");
    const auto* ind = res0.find("indicators");
    if (ind == nullptr) return out;
    const JsonValue& quote_arr = ind->at("quote");
    if (!quote_arr.is_array() || quote_arr.size() == 0) return out;
    const JsonValue& q = quote_arr.at(0);
    const JsonValue& opens = q.at("open");
    const JsonValue& highs = q.at("high");
    const JsonValue& lows = q.at("low");
    const JsonValue& closes = q.at("close");
    const JsonValue& volumes = q.at("volume");
    const std::size_t n = ts.size();
    if (n == 0) return out;
    out.candles.reserve(n);
    out.vols.reserve(n);
    out.labels.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double o = opens.at(i).as_number(0.0);
        const double h = highs.at(i).as_number(0.0);
        const double l = lows.at(i).as_number(0.0);
        const double c = closes.at(i).as_number(0.0);
        const double v = volumes.at(i).as_number(0.0);
        if (o == 0.0 && h == 0.0 && l == 0.0 && c == 0.0) continue;  // 停牌 / null
        out.candles.push_back(HsiKLineRenderer::Candle{o, h, l, c});
        out.vols.push_back(v);
        out.labels.push_back(format_hsi_date(static_cast<std::int64_t>(ts.at(i).as_number(0.0))));
    }
    if (out.candles.empty()) return out;
    out.ok = true;
    std::fprintf(stderr, "kline: Yahoo 拉取成功，共 %zu 根日K\n", out.candles.size());
    return out;
}

// Stooq：免费日线 CSV（比 Yahoo 对脚本更友好），"YYYY-MM-DD,Open,High,Low,Close,Volume"。
static HsiSeries fetch_hsi_stooq() {
    HsiSeries out;
    constexpr const char* url = "https://stooq.com/q/d/l/?s=^hsi&i=d";
    HttpOptions opts;
    opts.socket.connect_timeout_ms = 1000;
    opts.socket.io_timeout_ms = 2000;
    Request req;
    req.method = "GET";
    auto uri = Uri::parse(url);
    if (!uri) { std::fprintf(stderr, "kline: 内部错误（URL 解析失败）\n"); return out; }
    req.uri = std::move(*uri);
    req.headers.emplace_back(
        "User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36");
    auto resp = execute(req, opts);
    if (!resp || resp->status != 200) return out;
    std::istringstream iss(resp->body);
    std::string line;
    bool header = true;
    while (std::getline(iss, line)) {
        if (header) { header = false; continue; }  // 跳过 CSV 表头
        if (line.empty()) continue;
        std::vector<std::string> f;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == ',') {
                f.push_back(line.substr(start, i - start));
                start = i + 1;
            }
        }
        if (f.size() < 5) continue;
        const double o = std::atof(f[1].c_str());
        const double h = std::atof(f[2].c_str());
        const double l = std::atof(f[3].c_str());
        const double c = std::atof(f[4].c_str());
        const double v = (f.size() >= 6) ? std::atof(f[5].c_str()) : 0.0;
        if (o == 0.0 && h == 0.0 && l == 0.0 && c == 0.0) continue;
        out.candles.push_back(HsiKLineRenderer::Candle{o, h, l, c});
        out.vols.push_back(v);
        out.labels.push_back(f[0].size() >= 10 ? f[0].substr(5, 5) : f[0]);  // "MM-DD"
    }
    if (out.candles.empty()) return out;
    out.ok = true;
    std::fprintf(stderr, "kline: Stooq 拉取成功，共 %zu 根日K\n", out.candles.size());
    return out;
}

// 演示 network 模块：依次尝试 Stooq → Yahoo 拉真实恒指（^HSI）日K；全部失败回退模拟种子。
static HsiSeries fetch_hsi() {
    HsiSeries out = fetch_hsi_stooq();
    if (out.ok) return out;
    return fetch_hsi_yahoo();
}

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    // 运行时指定 GUI 字体文件路径（替代设置环境变量的繁琐方式）：--font <path>。
    // 须在首次绘制前设定；为空则回落到系统字体候选清单 / TERMCHART_GUI_FONT。
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            termchart::gui::set_font_file(argv[++i]);
            std::fprintf(stderr, "kline: 运行时指定字体 %s\n", argv[i]);
        }
    }

    // Take 2：宿主自持 QApplication；冒烟时自设离屏平台，省去手动导出 QT_QPA_PLATFORM。
    if (smoke) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);

    // 英雄 Demo 默认走 GitHub 深色主题（一键 'T' 可切 DefaultDark / AppleLight）。
    set_theme(GitHubDark());

    const int N = 25;
    std::vector<std::string> labels;
    std::vector<HsiKLineRenderer::Candle> candles;
    std::vector<double> vols;
    double close = 25200.0;
    for (int i = 0; i < N; ++i) {
        const double open = close;
        const double drift = gauss(90.0) + (i > 0 ? (candles.back().close - open) * 0.15 : 0.0);
        const double c = open + drift;
        const double high = std::max(open, c) + std::fabs(gauss(45.0)) + 20.0;
        const double low = std::min(open, c) - std::fabs(gauss(45.0)) - 20.0;
        candles.push_back({open, high, low, c});
        char buf[16];
        std::snprintf(buf, sizeof(buf), "09-%02d", 1 + (i % 28));
        labels.push_back(buf);
        const double range = (high - low);
        vols.push_back((0.8 + 0.4 * std::fabs(std::sin(i * 0.7))) * (600000.0 + range * 4000.0));
        close = c;
    }

    // 演示 network 模块：尝试用 termchart::network::http 拉真实恒指（^HSI）日K；
    // 失败（离线 / TLS 后端缺失 / 解析错误）则保留上面的模拟种子，标题会标明数据源。
    bool using_network = false;
    if (!smoke) {
        const HsiSeries net = fetch_hsi();
        if (net.ok) {
            using_network = true;
            candles = std::move(net.candles);
            vols = std::move(net.vols);
            labels = std::move(net.labels);
        }
    }

    const std::uint32_t W = 900, H = 500;
    NativeSurface host{};
    host.width = W;
    host.height = H;

    termchart_gui_qt::QtBackend backend(host);  // Take 2：宿主拥有窗口
    if (!backend.valid()) {
        std::fprintf(stderr, "QtBackend(Take2): 无法初始化（需要图形环境）\n");
        return 2;
    }

    HostWidget window(&backend);
    host.surface = &window;
    window.setWindowTitle(QString::fromUtf8(
        using_network ? "恒生指数日K线 · 数据源：网络(恒指实时)"
                      : "恒生指数日K线 · 数据源：模拟(离线)"));

    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});
    auto fl = std::make_unique<FlexLayout>();
    fl->direction = FlexDirection::Column;
    fl->cross_align = FlexAlign::Stretch;
    fl->gap = 0.0;
    root.set_layout(std::move(fl));

    const GuiTheme th0 = current_theme();
    auto& title = root.emplace_child<Label>(
        "title", "HSI Daily — Pan/Zoom: wheel & drag · 'T' theme",
        th0.foreground, th0.background);
    title.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), 22.0});
    title.set_hpolicy(SizePolicy::Fixed);
    title.set_vpolicy(SizePolicy::Fixed);

    auto& chart = root.emplace_child<ChartWidget>("chart");
    auto hsi = std::make_shared<HsiKLineRenderer>();
    hsi->set_data(labels, candles);
    hsi->set_volumes(vols);
    chart.set_renderer(hsi);

    auto& demo = root.emplace_child<Label>(
        "demo", "termchart_gui · K-line hero demo (Qt)", th0.text_dim, th0.background);
    demo.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), 18.0});
    demo.set_hpolicy(SizePolicy::Fixed);
    demo.set_vpolicy(SizePolicy::Fixed);

    Mount mount(root, backend);
    mount.paint_all();

    // 尺寸变化同步重渲染（宿主 resizeEvent → backend.resize 重排画布缓冲并回调）。
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        chart.set_pixel_scale(backend.device_pixel_ratio());  // 高 DPI：文本格随 dpr 放大
        chart.set_dragging(false);
        root.set_bounds(Rect{0.0, 0.0, static_cast<double>(w), static_cast<double>(h)});
        chart.request_snap();
        mount.paint_all();
        window.update();
    });

    bool running = true;
    bool dirty = true;
    bool data_dirty = false;  // 实时行情 tick 置脏，驱动重绘
    auto last = std::chrono::steady_clock::now();

    auto drain = [&](bool& running_out) -> bool {
        bool changed = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) {
                running_out = false;
                QApplication::quit();  // Take 2：宿主拥有 QApplication，自行退出
                return changed;
            }
            if (e.type == EventType::KeyPress && (e.key == 't' || e.key == 'T')) {
                cycle_theme();  // 一键换肤：渲染器与控件都读 current_theme()
                const GuiTheme th = current_theme();
                title.set_fg(th.foreground);
                title.set_bg(th.background);
                demo.set_fg(th.text_dim);
                demo.set_bg(th.background);
                changed = true;
                continue;
            }
            if (mount.dispatch_event(e)) {
                changed = true;  // 悬停/缩放/平移等交互置脏
            }
        }
        return changed;
    };

    if (smoke) {
        for (int i = 0; i < 5; ++i) {
            backend.process_events();
            drain(running);
            chart.tick(0.016);
            mount.paint_all();
            window.update();
            app.processEvents();  // 排到的 paintEvent → blit
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        backend.resize(720, 460);  // 练 Take 2 缩放回流（不依赖事件循环，直接触发 render_hook）
        app.processEvents();
        std::fprintf(stderr, "smoke: HSI K-line (Qt) rendered, no crash\n");
        return 0;
    }

    // ===== 实时行情模拟 =====
    // 每 ~250ms 给最后一根蜡烛打一笔 tick（随机游走），每 ~10 笔收一根并开新蜡烛，
    // 使最右蜡烛实时跳动、新蜡烛陆续长出——演示 K 线“实时”更新（非静态截图）。
    std::int32_t sim_month = 9, sim_day = 28;
    int ticks_in_candle = 0;
    auto next_label = [&]() -> std::string {
        if (++sim_day > 28) { sim_day = 1; if (++sim_month > 12) sim_month = 1; }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d-%02d", sim_month, sim_day);
        return std::string(buf);
    };
    QTimer feed;
    QObject::connect(&feed, &QTimer::timeout, [&]() {
        const double prev_close = candles.back().close;
        HsiKLineRenderer::Candle& c = candles.back();
        const double tick = gauss(35.0);
        c.close += tick;
        if (c.close > c.high) c.high = c.close;
        if (c.close < c.low) c.low = c.close;
        vols.back() += std::fabs(tick) * 1500.0 + 2000.0;
        if (++ticks_in_candle >= 10) {  // 收一根，开新蜡烛
            ticks_in_candle = 0;
            candles.push_back({prev_close, prev_close, prev_close, prev_close});
            vols.push_back(600000.0);
            labels.push_back(next_label());
            if (static_cast<int>(candles.size()) > 80) {  // 上限保护：多出从头部丢弃并保持视窗稳定
                labels.erase(labels.begin());
                candles.erase(candles.begin());
                vols.erase(vols.begin());
                ViewWindow v = hsi->view();
                if (v.lo > 0) { --v.lo; if (v.hi > 0) --v.hi; hsi->set_view(v); }
            }
        }
        hsi->set_data(labels, candles);
        hsi->set_volumes(vols);
        chart.invalidate();  // 废掉底图缓存，下帧按新数据重算几何并重绘
        data_dirty = true;
    });
    feed.start(250);

    // 交互窗口：宿主用 QTimer 每帧（~8ms）驱动——动画几何指数趋近 + 抽干事件路由到控件树。
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&]() {
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (chart.animating()) {  // 缩放/平移几何指数趋近，帧率解耦
            chart.tick(dt);
            dirty = true;
        }
        if (drain(running)) {
            dirty = true;
        }
        if (data_dirty) {  // 实时行情 tick
            dirty = true;
            data_dirty = false;
        }
        if (dirty) {
            mount.paint_all();
            window.update();
            dirty = false;
        }
    });
    ticker.start(8);
    window.resize(static_cast<int>(W), static_cast<int>(H));  // 显式逻辑尺寸，触发 DPR 物理画布
    window.show();
    return app.exec();
}
