// weather_demo.cpp —— 城市生活气象看板（"万能插座"演示 · 真实 Open-Meteo 数据）
//
// 大众场景：手机天气 App 里每天几亿人看的东西。复用 ChartWidget（万能插座）四联屏：
//  · 左上 = 24 小时温度：平滑折线 + 半透明面积 + 最高温高亮点（ComboRenderer，1 折线+面积）。
//  · 右上 = AQI 仪表盘（GaugeRenderer 原生插头：270° 分段色带 + 指针）。
//  · 左下 = 未来 7 天：降水概率柱（左轴）+ 最高/最低温折线（右轴，双 Y 轴 ComboRenderer）。
//  · 右下 = 24h 风玫瑰：风向(角度)×风速(半径) 极坐标折线（PolarRenderer 原生插头）。
// 数据：Open-Meteo（免费、无需 API Key、HTTPS）。后台线程 WinInet 拉取，主循环零卡顿；
// 拉取失败/超时则用内嵌的真实样本（北京 2026-10-17 实拉数据），离线也能跑。
//
// 跑法：weather_demo          → 开窗口交互（移动鼠标看悬停十字轴 + tooltip）
//       weather_demo --smoke  → 无交互冒烟：渲染数帧后自动关闭
// 零第三方依赖：网络走系统 WinInet，JSON 用本文件内极简提取（无需 nlohmann/json / httplib）。
// 注：GUI 字体仅含 95 个 ASCII 点阵字形，故所有标签用 ASCII（中文会显示为占位框）。
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/chart_widget.hpp"
#include "termchart_gui/widget_tree/combo_renderer.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/gauge_renderer.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/polar_renderer.hpp"
#include "termchart_gui_win32/win32_backend.hpp"

namespace {

// 内嵌真实样本（Open-Meteo 实拉：北京 2026-10-17 起）。离线/无网时回退到此。
const char* kCity = "Beijing";
const char* kUrl =
    "https://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074"
    "&hourly=temperature_2m,precipitation_probability,wind_speed_10m,wind_direction_10m"
    "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&forecast_days=7&timezone=Asia%2FShanghai";
const char* kAqiUrl =
    "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=39.9042&longitude=116.4074"
    "&hourly=pm2_5,us_aqi&timezone=Asia%2FShanghai";

// 24h 温度（今日 00:00–23:00，°C）
const std::vector<std::string> kH24Labels = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11",
    "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23"};
const std::vector<double> kH24Temp = {
    15.4, 14.6, 13.8, 13.1, 12.8, 12.8, 13.1, 13.2, 13.3, 14.1, 14.2, 13.9,
    13.8, 14.0, 14.7, 15.1, 15.2, 15.4, 15.6, 15.6, 15.7, 15.7, 15.3, 15.1};
const std::vector<double> kH24Precip = {
    46, 74, 94, 100, 98, 96, 96, 96, 96, 97, 99, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 96, 73, 41, 16};
const std::vector<double> kH24WindSpd = {
    6.7, 7.5, 9.8, 9.5, 9.9, 9.8, 8.8, 9.5, 8.3, 6.3, 9.1, 12.2, 11.7, 13.8,
    13.7, 10.7, 13.1, 13.1, 12.1, 10.6, 11.4, 18.5, 20.3, 19.8};
const std::vector<double> kH24WindDir = {8,  352, 20,  358, 345, 8,   12,  10,  1,   336, 338,
                                         340, 356, 354, 1,   355, 339, 330, 349, 341, 344, 338,
                                         340, 340};
const double kAqi = 165.0;  // 当前 US AQI（实拉 2026-10-17 00:00）

// 未来 7 天（MM-DD）
const std::vector<std::string> kD7Labels = {"10-17", "10-18", "10-19", "10-20",
                                            "10-21", "10-22", "10-23"};
const std::vector<double> kD7Max = {15.9, 16.7, 17.6, 16.2, 14.6, 14.4, 15.0};
const std::vector<double> kD7Min = {12.7, 10.1, 7.0, 7.3, 4.2, 3.3, 4.7};
const std::vector<double> kD7Precip = {100, 5, 0, 22, 0, 2, 4};

// —— 极简 JSON 提取（只够 Open-Meteo 这一结构，零第三方依赖）——
std::size_t find_nth(const std::string& s, const std::string& sub, int n) {
    std::size_t pos = 0;
    for (int k = 0; k < n; ++k) {
        pos = s.find(sub, pos);
        if (pos == std::string::npos) return std::string::npos;
        pos += sub.size();
    }
    return pos - (n > 0 ? sub.size() : 0);
}
std::vector<double> parse_doubles(const std::string& s, std::size_t start) {
    std::vector<double> out;
    std::size_t i = s.find('[', start);
    if (i == std::string::npos) return out;
    ++i;
    while (i < s.size() && s[i] != ']') {
        while (i < s.size() && s[i] != ']' && s[i] != '-' &&
               !std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '.') {
            ++i;
        }
        if (i >= s.size() || s[i] == ']') break;
        std::size_t e = i;
        if (s[e] == '-') ++e;
        while (e < s.size() &&
               (std::isdigit(static_cast<unsigned char>(s[e])) || s[e] == '.')) {
            ++e;
        }
        try {
            out.push_back(std::stod(s.substr(i, e - i)));
        } catch (...) {
        }
        i = e;
    }
    return out;
}
std::vector<std::string> parse_strings(const std::string& s, std::size_t start) {
    std::vector<std::string> out;
    std::size_t i = s.find('[', start);
    if (i == std::string::npos) return out;
    ++i;
    while (i < s.size() && s[i] != ']') {
        std::size_t q = s.find('"', i);
        if (q == std::string::npos) break;
        std::size_t q2 = s.find('"', q + 1);
        if (q2 == std::string::npos) break;
        out.push_back(s.substr(q + 1, q2 - q - 1));
        i = q2 + 1;
    }
    return out;
}

struct WeatherData {
    std::vector<std::string> h_time, d_time;
    std::vector<double> temp, precip, tmax, tmin, pmax;
    std::vector<double> wind_sp, wind_dir;
    double aqi = 0.0;
    bool ok = false;
};

bool parse_weather(const std::string& body, WeatherData& w) {
    w.h_time = parse_strings(body, find_nth(body, "\"time\":[", 1));
    w.temp = parse_doubles(body, body.find("\"temperature_2m\":["));
    w.precip = parse_doubles(body, body.find("\"precipitation_probability\":["));
    w.wind_sp = parse_doubles(body, body.find("\"wind_speed_10m\":["));
    w.wind_dir = parse_doubles(body, body.find("\"wind_direction_10m\":["));
    w.d_time = parse_strings(body, find_nth(body, "\"time\":[", 2));
    w.tmax = parse_doubles(body, body.find("\"temperature_2m_max\":["));
    w.tmin = parse_doubles(body, body.find("\"temperature_2m_min\":["));
    w.pmax = parse_doubles(body, body.find("\"precipitation_probability_max\":["));
    w.ok = !w.temp.empty() && !w.tmax.empty();
    return w.ok;
}
bool parse_aqi(const std::string& body, double& aqi) {
    std::vector<double> v = parse_doubles(body, body.find("\"us_aqi\":["));
    if (v.empty()) return false;
    aqi = v.front();
    return true;
}

// 安全取值：越界返回 0.0 兜底。Open-Meteo 偶发 null 会让某些数组比 time 数组短，
// 直接下标必崩（_n < size()）；所有 live 数据访问都走这里。
double safe_at(const std::vector<double>& v, std::size_t i) {
    return (i < v.size()) ? v[i] : 0.0;
}

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
// 系统 WinInet 拉取（HTTPS 由 INTERNET_FLAG_SECURE 接管，无需 OpenSSL）。
std::string fetch_http(const std::string& url) {
    std::string result;
    HINTERNET hInet = InternetOpenA("termchart-weather/1.0", INTERNET_OPEN_TYPE_DIRECT,
                                    nullptr, nullptr, 0);
    if (!hInet) return result;
    HINTERNET hUrl = InternetOpenUrlA(hInet, url.c_str(), nullptr, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (hUrl) {
        char buf[8192];
        DWORD rd = 0;
        while (InternetReadFile(hUrl, buf, sizeof(buf) - 1, &rd) && rd > 0) {
            buf[rd] = '\0';
            result.append(buf, rd);
        }
        InternetCloseHandle(hUrl);
    }
    InternetCloseHandle(hInet);
    return result;
}
#endif

}  // namespace

int main(int argc, char** argv) {
    using namespace termchart_gui;
    bool smoke = false, simulate = false;
    std::string feed_path, dump_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--smoke") smoke = true;
        else if (a == "--simulate") simulate = true;
        else if (a == "--feed" && i + 1 < argc) feed_path = argv[++i];
        else if (a == "--dump" && i + 1 < argc) dump_path = argv[++i];
    }

    // 构建时间戳横幅：跑起来第一行的 stderr 会打印，便于确认你跑的是不是带修复的新鲜 exe。
    std::fprintf(stderr, "weather_demo build %s %s\n", __DATE__, __TIME__);

    // --dump <file>：只抓真实 Open-Meteo 响应落盘、不渲染（因此不会崩），便于离线复现崩溃。
#ifdef _WIN32
    if (!dump_path.empty()) {
        std::string body = fetch_http(kUrl);
        {
            std::ofstream f(dump_path, std::ios::binary);
            f << body;
        }
        std::string abody = fetch_http(kAqiUrl);
        std::string af = dump_path + ".aqi";
        {
            std::ofstream f(af, std::ios::binary);
            f << abody;
        }
        std::fprintf(stderr, "weather: dumped %zu bytes -> %s  (aqi %zu bytes -> %s)\n",
                     body.size(), dump_path.c_str(), abody.size(), af.c_str());
        return 0;
    }
#endif

    const std::uint32_t W = 960;
    const std::uint32_t H = 700;

    termchart_gui_win32::Win32Backend backend(
        W, H, std::string("termchart · 城市气象看板 · ") + kCity);
    if (!backend.valid()) {
        std::fprintf(stderr, "Win32Backend: 无法创建窗口（需要交互桌面会话）\n");
        return 2;
    }

    Container root("root");
    root.set_bounds(Rect{0, 0, static_cast<double>(W), static_cast<double>(H)});
    root.set_layout(nullptr);  // 用手写布局（见 layout_dashboard），避免 Flex 权重未接线踩坑

    // 左上：24h 温度（平滑折线+面积+最高温高亮）+ 降水概率作为第二条折线。
    auto& temp_chart = root.emplace_child<ChartWidget>("temp24h");
    auto temp_renderer = std::make_shared<ComboRenderer>();
    temp_renderer->set_series(kH24Labels, {}, {kH24Temp, kH24Precip},
                              {"Temp C", "Prec %"});
    temp_renderer->set_line_colors({{255, 138, 76}, {90, 170, 230}});
    temp_renderer->set_area(0, {255, 138, 76});  // 温度线下半透明面积
    temp_renderer->set_marker(0, {255, 210, 120});  // 最高温圆点
    temp_renderer->set_show_bars(false);
    temp_renderer->set_title("24h Temperature (C) - " + std::string(kCity));
    temp_chart.set_renderer(temp_renderer);

    // 右上：AQI 仪表盘（原生 GaugeRenderer）。
    auto& gauge_chart = root.emplace_child<ChartWidget>("aqi");
    auto gauge_renderer = std::make_shared<GaugeRenderer>();
    gauge_renderer->set_value(kAqi);
    gauge_renderer->set_max(300.0);
    gauge_renderer->set_title("AQI");
    gauge_chart.set_renderer(gauge_renderer);

    // 左下：未来 7 天 降水概率柱（左轴）+ 最高/最低温折线（右轴，双 Y 轴）。
    auto& fc_chart = root.emplace_child<ChartWidget>("forecast7d");
    auto fc_renderer = std::make_shared<ComboRenderer>();
    fc_renderer->set_series(kD7Labels, kD7Precip, {kD7Max, kD7Min}, {"High", "Low"});
    fc_renderer->set_line_colors({{255, 120, 90}, {80, 160, 255}});
    fc_renderer->set_bar_color({70, 130, 230});
    fc_renderer->set_bar_name("Prec %");
    fc_renderer->set_area(-1, {0, 0, 0});
    fc_renderer->set_show_bars(true);
    fc_renderer->set_right_axis(true, {0, 1});  // 温度折线走右轴量纲（不与降水%挤一根轴）
    fc_renderer->set_title("7-Day High/Low Temp & Precip");
    fc_chart.set_renderer(fc_renderer);

    // 右下：24h 风玫瑰（原生 PolarRenderer）。
    auto& wind_chart = root.emplace_child<ChartWidget>("wind");
    auto wind_renderer = std::make_shared<PolarRenderer>();
    {
        std::vector<termchart::Point> wp;
        wp.reserve(kH24WindDir.size());
        for (std::size_t i = 0; i < kH24WindDir.size(); ++i) {
            wp.push_back(termchart::Point{kH24WindDir[i], kH24WindSpd[i]});
        }
        wind_renderer->set_points(std::move(wp));
    }
    wind_renderer->set_title("24h Wind");
    wind_chart.set_renderer(wind_renderer);

    // 手写四联屏布局（左上/右上/左下/右下）。W/H 变化（resize）时重算。
    auto layout_dashboard = [&](double w, double h) {
        const double gap = 8.0;
        const double topH = (h - 2.0 * gap) * 0.36;
        const double midH = (h - 2.0 * gap) * 0.36;
        const double botH = h - topH - midH - 2.0 * gap;
        const double halfW = (w - gap) / 2.0;
        temp_chart.set_bounds(Rect{0.0, 0.0, halfW, topH});            // 左上：温度
        gauge_chart.set_bounds(Rect{halfW + gap, 0.0, w - halfW - gap, topH});  // 右上：AQI
        fc_chart.set_bounds(Rect{0.0, topH + gap, w, midH});            // 左下：7 天
        wind_chart.set_bounds(
            Rect{halfW + gap, topH + midH + 2.0 * gap, w - halfW - gap, botH});  // 右下：风
    };
    layout_dashboard(static_cast<double>(W), static_cast<double>(H));

    Mount mount(root, backend);
    mount.paint_all();

    // 后台拉取真实数据；拿到后由主循环安全写入 renderer 并重绘（避免 UI 线程阻塞）。
    // --simulate：跳过联网，注入"故意不等长"的假数据，确定性验证 apply_live 防御 + 渲染器
    // 补零（复现 Open-Meteo 因 null 丢值导致 temp/precip/wind_sp/tmax 比 time 数组短的场景）。
    std::atomic<bool> pending_ready{false};
    WeatherData pending;
    std::mutex pending_mtx;  // 防御性加固：即使有人把 flag 改成 relaxed 也不 race

    auto make_simulated = []() -> WeatherData {
        WeatherData w;
        w.h_time.assign(24, std::string("2026-10-17T00:00"));
        w.temp.assign(20, 12.0);    // 比 24 短 4（温度 null 丢值）
        w.precip.assign(22, 30.0);  // 比 24 短 2
        w.wind_dir.assign(24, 90.0);
        w.wind_sp.assign(18, 5.0);  // 比 24 短 6（风速 null 丢值）
        w.d_time.assign(7, std::string("2026-10-17"));
        w.tmax.assign(5, 20.0);     // 比 7 短 2
        w.tmin.assign(7, 10.0);
        w.pmax.assign(7, 40.0);
        w.aqi = 165.0;
        return w;
    };

    if (simulate) {
        {
            std::lock_guard<std::mutex> lock(pending_mtx);
            pending = make_simulated();
        }
        pending_ready.store(true, std::memory_order_release);
        std::fprintf(stderr,
                     "weather: --simulate injected mismatched data "
                     "(h_time24/temp20/precip22/wind_dir24/wind_sp18/d_time7/tmax5)\n");
    } else if (!feed_path.empty()) {
        // 离线复现：把一份真实 Open-Meteo JSON 直接喂给 parse_weather → apply_live，
        // 走完整"解析→渲染"链路（无需联网），用含 null 丢值的样本复现原始崩溃场景。
        std::ifstream f(feed_path, std::ios::binary);
        std::string body;
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            body = std::move(ss).str();
        }
        WeatherData w;
        const bool ok = !body.empty() && parse_weather(body, w);
        if (ok) {
            std::fprintf(stderr,
                         "weather: --feed parsed %s (%zu hourly temps / %zu daily max)\n",
                         feed_path.c_str(), w.temp.size(), w.tmax.size());
            std::lock_guard<std::mutex> lock(pending_mtx);
            pending = std::move(w);
            pending_ready.store(true, std::memory_order_release);
        } else {
            std::fprintf(stderr, "weather: --feed failed to read/parse %s\n", feed_path.c_str());
        }
    } else {
    std::thread([&]() {
#ifdef _WIN32
        try {
            WeatherData w;
            std::string body = fetch_http(kUrl);
            parse_weather(body, w);
            std::string abody = fetch_http(kAqiUrl);
            if (!abody.empty()) {
                double a = 0.0;
                if (parse_aqi(abody, a)) w.aqi = a;
            }
            // 拿到非空响应才覆盖内置样本；body 为空（网络失败/超时）则保持内嵌真实样本，
            // 平滑降级，不替换、不崩。解析出的数组长度不齐由 apply_live 的 safe_at 兜底。
            if (!body.empty()) {
                std::lock_guard<std::mutex> lock(pending_mtx);
                pending = std::move(w);
                pending_ready.store(true, std::memory_order_release);
            } else {
                std::fprintf(
                    stderr,
                    "weather: live fetch failed (no response) — falling back to embedded sample\n");
            }
        } catch (const std::exception& ex) {
            std::fprintf(stderr,
                         "weather: fetch thread exception (%s) — embedded sample kept\n",
                         ex.what());
        } catch (...) {
            std::fprintf(stderr,
                         "weather: fetch thread unknown exception — embedded sample kept\n");
        }
#endif
    }).detach();
    }

    auto apply_live = [&]() {
        {
            std::lock_guard<std::mutex> lock(pending_mtx);
            // 24h 温度 + 降水：以各数组实际长度为准，越界一律兜底 0（Open-Meteo 偶发 null
            // 会让 temp/precip 比 h_time 短，直接下标必崩）。
            if (pending.h_time.size() >= 24) {
                const std::size_t n = 24;
                std::vector<std::string> hlab(n);
                std::vector<double> htemp(n), hprec(n);
                for (std::size_t i = 0; i < n; ++i) {
                    const std::string& t = pending.h_time[i];
                    const std::size_t p = t.find('T');
                    hlab[i] = (p != std::string::npos) ? t.substr(p + 1, 5) : t;
                    htemp[i] = safe_at(pending.temp, i);
                    hprec[i] = safe_at(pending.precip, i);
                }
                temp_renderer->set_series(hlab, {}, {htemp, hprec}, {"Temp C", "Prec %"});
            }
            // 7 天：把各数组统一裁剪/补齐到标签数，避免 bars/lines 下标越界。
            if (!pending.d_time.empty()) {
                std::vector<std::string> dlab;
                dlab.reserve(pending.d_time.size());
                for (const auto& t : pending.d_time) {
                    dlab.push_back(t.size() >= 5 ? t.substr(t.size() - 5) : t);
                }
                const std::size_t n = dlab.size();
                std::vector<double> pmax(n), tmax(n), tmin(n);
                for (std::size_t i = 0; i < n; ++i) {
                    pmax[i] = safe_at(pending.pmax, i);
                    tmax[i] = safe_at(pending.tmax, i);
                    tmin[i] = safe_at(pending.tmin, i);
                }
                fc_renderer->set_series(dlab, pmax, {tmax, tmin}, {"High", "Low"});
            }
            if (pending.aqi > 0.0) gauge_renderer->set_value(pending.aqi);
            // 风玫瑰：wind_dir 与 wind_sp 都需 >= 24，互不假设等长，缺一个就跳过。
            if (pending.wind_dir.size() >= 24 && pending.wind_sp.size() >= 24) {
                std::vector<termchart::Point> wp;
                wp.reserve(24);
                for (std::size_t i = 0; i < 24; ++i) {
                    wp.push_back(termchart::Point{pending.wind_dir[i], pending.wind_sp[i]});
                }
                wind_renderer->set_points(std::move(wp));
            }
        }
        temp_chart.invalidate();
        fc_chart.invalidate();
        gauge_chart.invalidate();
        wind_chart.invalidate();
    };

    backend.set_render_hook([&](std::uint32_t rw, std::uint32_t rh) {
        temp_chart.set_dragging(true);
        fc_chart.set_dragging(true);
        gauge_chart.set_dragging(true);
        wind_chart.set_dragging(true);
        root.set_bounds(Rect{0, 0, static_cast<double>(rw), static_cast<double>(rh)});
        layout_dashboard(static_cast<double>(rw), static_cast<double>(rh));
        temp_chart.request_snap();
        fc_chart.request_snap();
        gauge_chart.request_snap();
        wind_chart.request_snap();
        mount.paint_all();
    });

    auto handle = [&](const Event& e, bool& running) -> bool {
        switch (e.type) {
            case EventType::Quit:
                running = false;
                return false;
            case EventType::Resize:
                temp_chart.set_dragging(false);
                fc_chart.set_dragging(false);
                gauge_chart.set_dragging(false);
                wind_chart.set_dragging(false);
                layout_dashboard(static_cast<double>(e.width), static_cast<double>(e.height));
                return true;
            default:
                return mount.dispatch_event(e);
        }
    };

    if (smoke) {
        bool running = true;
        for (int i = 0; i < 5 && running; ++i) {
            Event e;
            while (backend.poll_event(e)) (void)handle(e, running);
            if (pending_ready.exchange(false, std::memory_order_acquire)) apply_live();
            mount.paint_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        backend.request_close();
        for (int i = 0; i < 100; ++i) {
            bool r = true;
            Event e;
            while (backend.poll_event(e)) (void)handle(e, r);
            if (!r) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::fprintf(stderr, "smoke: weather dashboard rendered, window closed cleanly\n");
        return 0;
    }

    auto last = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (handle(e, running)) dirty = true;
        }
        if (pending_ready.exchange(false, std::memory_order_acquire)) {
            apply_live();  // 真实数据到达 → 平滑过渡到真实曲线
            dirty = true;
        }
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (temp_chart.animating() || fc_chart.animating() || gauge_chart.animating() ||
            wind_chart.animating()) {
            temp_chart.tick(dt);
            fc_chart.tick(dt);
            gauge_chart.tick(dt);
            wind_chart.tick(dt);
            dirty = true;
        }
        if (dirty) {
            mount.paint_all();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
    return 0;
}
