// embed_demo_qt.cpp -- Take 2 句柄注入演示：宿主拥有 QWidget，Qt 后端只当像素渲染器
//
// 与 qt_demo（Take 1：库自建窗口）相对照：这里窗口由宿主自己创建、自己销毁；
// 库侧 QtBackend 用 NativeSurface{ (void*)widget } 构造，只持一块离线画布，
// 宿主在 paintEvent 里调 backend.blit(painter) 把当前帧画进窗口，并把鼠标事件
// inject_event 回传。证明「句柄注入 + 适配器」模型在 Qt 上同样成立。
//
// 跑法：embed_demo_qt            -> 开窗口交互（ESC / 关闭按钮退出）
//       embed_demo_qt --smoke    -> 无交互冒烟（自动退出）

#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_qt/qt_backend.hpp"

using namespace termchart_gui;
using namespace termchart_gui_qt;  // embed_demo_qt 直接用 QtBackend（此前漏了此 using，导致它从未编译过）

namespace {

class HostWidget final : public QWidget {
public:
    explicit HostWidget(QtBackend* backend, QWidget* parent = nullptr)
        : QWidget(parent), backend_(backend) {
        setMouseTracking(true);
        setFixedSize(480, 320);
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
            backend_->inject_event(
                tk_event_map::mouse_press(e->pos().x(), e->pos().y(), 0));
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (backend_ != nullptr) {
            backend_->inject_event(
                tk_event_map::mouse_release(e->pos().x(), e->pos().y(), 0));
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (backend_ != nullptr) {
            backend_->inject_event(tk_event_map::mouse_move(e->pos().x(), e->pos().y()));
        }
    }
    void closeEvent(QCloseEvent* e) override {
        if (backend_ != nullptr) {
            backend_->inject_event(tk_event_map::quit());
        }
        QWidget::closeEvent(e);
    }

private:
    QtBackend* backend_;
};

}  // namespace

int main(int argc, char** argv) {
    const bool smoke = (argc > 1 && std::string(argv[1]) == "--smoke");

    QApplication app(argc, argv);

    NativeSurface host{};
    host.width = 480;
    host.height = 320;

    termchart_gui_qt::QtBackend backend(host);  // Take 2：此时尚无宿主窗口指针
    if (!backend.valid()) {
        std::fprintf(stderr, "QtBackend(Take2): 无法初始化（需要图形环境）\n");
        return 2;
    }

    HostWidget window(&backend);
    host.surface = &window;  // 回填宿主窗口指针（后端在 blit 时不需要，仅作契约记录）

    Container root("root");
    root.set_bounds(Rect{0, 0, 480, 320});
    root.emplace_child<Label>("title", "Take 2 - Qt host-owned widget", Rgb{240, 240, 245},
                              Rgb{28, 32, 40});
    auto& slider = root.emplace_child<Slider>("slider");
    slider.set_on_change([&](double v) { (void)v; });

    Mount mount(root, backend);

    if (smoke) {
        mount.paint_all();
        app.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 0;
    }

    window.show();
    return app.exec();
}
