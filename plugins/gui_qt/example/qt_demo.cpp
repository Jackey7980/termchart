// qt_demo.cpp —— 原生 Qt 后端可运行演示（Take 2：宿主拥有窗口）
//
// 与 win32_demo 同一套控件接线（widget_tree/Mount），但窗口与事件泵归宿主所有：
// 宿主自建 QWidget + QApplication，paintEvent 里调 backend.blit(painter) 把当前帧贴进
// 窗口，并把鼠标/滚轮/键盘事件经 tk_event_map 注入 backend（backend.inject_event），
// 由每帧 QTimer 抽干事件并路由到控件树。Take 2 的好处：QApplication 由宿主创建，故
// 可在 main 顶部自设 QT_QPA_PLATFORM=offscreen（--smoke 时），无需手动导出该变量。
//
// 跑法：qt_demo            → 开窗口交互（ESC / 关闭按钮退出）
//       qt_demo --smoke    → 无交互冒烟：泵数帧事件、重绘后自动关闭（CI 用）
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
#include <cstdio>
#include <string>
#include <thread>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/gui.hpp"
#include "termchart_gui/widget_tree/button.hpp"
#include "termchart_gui/widget_tree/checkbox.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_qt/qt_backend.hpp"

using namespace termchart_gui;

namespace {

// Qt 修饰键 → termchart EventModifier 位组合（演示只用到 Shift，但一并映射干净）。
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
        setMouseTracking(true);  // 悬停需要无按键也收 move
        dpr_ = devicePixelRatioF();          // 高 DPI：记下设备像素比
        if (backend_ != nullptr) {
            backend_->set_device_pixel_ratio(dpr_);
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        if (backend_ != nullptr) {
            backend_->blit(painter);  // 把离线画布整块贴进本窗口
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
            // 归一化为"格"：Qt 原生是 1/8 度，一步 = 120 = 1 格（与 CanvasWidget 同约定）。
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

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    // Take 2：宿主自持 QApplication。冒烟时自设离屏平台——省去手动导出 QT_QPA_PLATFORM。
    // （QTDIR 与 QT_QPA_PLATFORM_PLUGIN_PATH 运行期并不需要：Qt 按安装前缀经 QLibraryInfo
    //  自动定位插件；本机只需把 Qt bin 目录置于 PATH 首位即可，该活由 ctest 的 ENVIRONMENT 包掉。）
    if (smoke) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication app(argc, argv);

    const std::uint32_t W = 480, H = 320;
    NativeSurface host{};
    host.width = W;
    host.height = H;

    termchart_gui_qt::QtBackend backend(host);  // Take 2：此时尚无宿主窗口指针
    if (!backend.valid()) {
        std::fprintf(stderr, "QtBackend(Take2): 无法初始化（需要图形环境）\n");
        return 2;
    }

    HostWidget window(&backend);
    host.surface = &window;  // 回填宿主窗口指针（仅作契约记录）

    Container root("root");
    root.set_bounds(Rect{0.0, 0.0, static_cast<double>(W), static_cast<double>(H)});
    root.emplace_child<Label>("title", "Native Qt GUI (Take 2)", Rgb{240, 240, 245},
                              Rgb{28, 32, 40});
    auto& slider = root.emplace_child<Slider>("slider");
    auto& bar = root.emplace_child<ProgressBar>("bar");
    auto& val = root.emplace_child<Label>("val", "0%");
    auto& btn = root.emplace_child<Button>("btn", "Count: 0");
    auto& chk = root.emplace_child<Checkbox>("chk", "Show progress");

    int counter = 0;
    slider.set_on_change([&](double v) {
        bar.set_value(v);
        val.set_text(std::to_string(static_cast<int>(v * 100.0)) + "%");
    });
    btn.set_on_click([&] {
        ++counter;
        btn.set_label("Count: " + std::to_string(counter));
    });
    chk.set_on_toggle([&](bool on) { bar.set_visible(on); });

    Mount mount(root, backend);
    mount.paint_all();  // paint_all 内含 present()（清屏→重排→绘制→上屏）

    // 尺寸变化同步重渲染（宿主 resizeEvent → backend.resize 重排画布缓冲并回调）。
    backend.set_render_hook([&](std::uint32_t w, std::uint32_t h) {
        root.set_bounds(Rect{0.0, 0.0, static_cast<double>(w), static_cast<double>(h)});
        mount.paint_all();
        window.update();  // 触发一次 paintEvent → blit
    });

    auto drain = [&](bool& running) -> bool {
        bool dirty = false;
        Event e;
        while (backend.poll_event(e)) {
            if (e.type == EventType::Quit) {
                running = false;
                QApplication::quit();  // Take 2：宿主拥有 QApplication，自行退出
                return dirty;
            }
            (void)mount.dispatch_event(e);
            dirty = true;
        }
        return dirty;
    };

    if (smoke) {
        // 冒烟：泵几帧 + 触发一次真实 paintEvent（练 blit 路径）后干净退出。
        bool running_smoke = true;
        for (int i = 0; i < 5; ++i) {
            backend.process_events();
            drain(running_smoke);
            mount.paint_all();
            window.update();
            app.processEvents();  // 排到的 paintEvent → blit
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        backend.resize(720, 460);  // 练 Take 2 缩放回流（不依赖事件循环，直接触发 render_hook）
        app.processEvents();
        std::fprintf(stderr, "smoke: frame rendered, window closed cleanly\n");
        return 0;
    }

    // 交互窗口：宿主用 QTimer 每帧（~8ms）抽干原生事件并路由到控件树，状态变化则重绘。
    bool running = true;
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&]() {
        const bool dirty = drain(running);
        if (dirty) {
            mount.paint_all();
            window.update();
        }
    });
    ticker.start(8);
    window.resize(static_cast<int>(W), static_cast<int>(H));  // 显式逻辑尺寸，触发 DPR 物理画布
    window.show();
    return app.exec();
}
