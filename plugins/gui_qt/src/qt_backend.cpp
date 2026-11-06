// qt_backend.cpp —— Qt 后端实现（Qt5/Qt6，PIMPL 隔离 Qt 类型）
#include "termchart_gui_qt/qt_backend.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <QApplication>
#include <QCloseEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWidget>
#include <QWheelEvent>
#include <QtGlobal>

namespace termchart_gui_qt {
namespace {

// 修饰键 → EventModifier 位。
std::uint32_t modifiers_of(Qt::KeyboardModifiers m) {
    std::uint32_t bits = 0;
    if (m & Qt::ShiftModifier) {
        bits |= termchart_gui::kModShift;
    }
    if (m & Qt::ControlModifier) {
        bits |= termchart_gui::kModCtrl;
    }
    if (m & Qt::AltModifier) {
        bits |= termchart_gui::kModAlt;
    }
    return bits;
}

// 鼠标坐标：Qt6 用 position()（pos()/localPos() 自 6.0 起标记 deprecated），
// Qt5 没有 position()、只有 localPos()。统一在此收口，避免每个事件各写一遍 #if。
QPointF mouse_local_pos(const QMouseEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->localPos();
#endif
}

// 滚轮坐标：Qt6 用 position()，Qt5 用 posF()。**注意 QWheelEvent 从来没有过
// localPos()**——按鼠标事件的写法照抄 e->localPos() 在 Qt5/Qt6 上都编不过
// （此前的实现正是这么写的，从未编译验证过）。
QPointF wheel_local_pos(const QWheelEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->posF();
#endif
}

// 绘制 surface 的 QWidget：paintEvent 把 BitmapCanvas 的 RGBA 直贴。
// Qt 的原子是 QImage（像素矩形真彩），与 TkCanvas 契约一致。
//
// 刻意**不加 Q_OBJECT**：本类不使用信号/槽，事件一律经 std::function 回调外发，
// 因此不需要 moc。相应地源文件末尾也不能 `#include "qt_backend.moc"`——AUTOMOC
// 只为含 Q_OBJECT 的 TU 生成 .moc，无 Q_OBJECT 却去 include 会直接编译失败。
class CanvasWidget final : public QWidget {
public:
    CanvasWidget(termchart_gui::BitmapCanvas& canvas, QWidget* parent = nullptr)
        : QWidget(parent), canvas_(canvas) {
        setMouseTracking(true);  // 无按键拖动也要 MouseMove
        setFocusPolicy(Qt::StrongFocus);
    }

    void set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook) {
        render_hook_ = std::move(hook);
    }

    void set_event_sink(std::function<void(termchart_gui::Event)> sink) {
        event_sink_ = std::move(sink);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        const termchart_gui::Bitmap& px = canvas_.pixels();
        // Format_RGBA8888：内存布局 RGBA 与 Bitmap(channels=4) 一致。
        // Bitmap 行按通道紧致对齐（row_bytes = w * channels），正好可直接当
        // QImage 的 bytesPerLine 用。QImage 引用外部缓冲、不拷贝，生命周期覆盖
        // paintEvent 全程即可。
        const auto* data = reinterpret_cast<const uchar*>(px.data());
        const QImage image(data, static_cast<int>(px.width()), static_cast<int>(px.height()),
                           static_cast<int>(px.row_bytes()), QImage::Format_RGBA8888);
        painter.drawImage(QPoint{0, 0}, image);
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        const auto w = static_cast<std::uint32_t>(e->size().width());
        const auto h = static_cast<std::uint32_t>(e->size().height());
        if (render_hook_) {
            render_hook_(w, h);  // 应用可就地重排版 + 重绘，避免拖动时闪白
        }
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::Resize;
        ev.width = w;
        ev.height = h;
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        const QPointF pos = mouse_local_pos(e);
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MousePress;
        ev.x = pos.x();
        ev.y = pos.y();
        ev.button = static_cast<int>(e->button());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        const QPointF pos = mouse_local_pos(e);
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MouseRelease;
        ev.x = pos.x();
        ev.y = pos.y();
        ev.button = static_cast<int>(e->button());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        const QPointF pos = mouse_local_pos(e);
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MouseMove;
        ev.x = pos.x();
        ev.y = pos.y();
        ev.button = static_cast<int>(e->buttons());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        const QPointF pos = wheel_local_pos(e);
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::Wheel;
        ev.x = pos.x();
        ev.y = pos.y();
        // 归一化为"格"：Qt 原生是 1/8 度（angleDelta），多数鼠标一步 = 120 = 1 格。
        const QPoint delta = e->angleDelta();
        ev.wheel_delta = static_cast<double>(delta.y()) / 120.0;
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void keyPressEvent(QKeyEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::KeyPress;
        ev.key = e->key();
        ev.text = e->text().toStdString();
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void closeEvent(QCloseEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::Quit;
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
        QWidget::closeEvent(e);
    }

private:
    termchart_gui::BitmapCanvas& canvas_;
    std::function<void(std::uint32_t, std::uint32_t)> render_hook_;
    std::function<void(termchart_gui::Event)> event_sink_;
};

// TkWindow 实现：对 QWidget 的薄包装。
class QtWindow final : public termchart_gui::TkWindow {
public:
    explicit QtWindow(QWidget* widget) : widget_(widget) {}

    void set_title(std::string title) override {
        if (widget_ != nullptr) {
            widget_->setWindowTitle(QString::fromStdString(title));
        }
    }
    void resize(std::uint32_t w, std::uint32_t h) override {
        if (widget_ != nullptr) {
            widget_->resize(static_cast<int>(w), static_cast<int>(h));
        }
    }
    [[nodiscard]] std::uint32_t width() const override {
        return widget_ != nullptr ? static_cast<std::uint32_t>(widget_->width()) : 0u;
    }
    [[nodiscard]] std::uint32_t height() const override {
        return widget_ != nullptr ? static_cast<std::uint32_t>(widget_->height()) : 0u;
    }
    void close() override {
        if (widget_ != nullptr) {
            widget_->close();
        }
    }

private:
    QWidget* widget_;  // 不持有：生命周期归 Qt 对象树
};

}  // namespace

struct QtBackend::Impl {
    std::unique_ptr<termchart_gui::BitmapCanvas> canvas;
    std::unique_ptr<CanvasWidget> widget;
    bool running = false;
    bool quit_requested = false;

    std::mutex queue_mutex;
    std::deque<termchart_gui::Event> queue;
    std::function<void(std::uint32_t, std::uint32_t)> render_hook;

    void push(termchart_gui::Event e) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.push_back(std::move(e));
    }
};

// QApplication 是进程级单例：首个后端创建它，之后复用。
static QApplication* ensure_qapplication() {
    if (QApplication::instance() != nullptr) {
        return qobject_cast<QApplication*>(QApplication::instance());
    }
    static int argc = 1;
    static char arg0[] = "termchart_gui";
    static char* argv[] = {arg0, nullptr};
    // 注意：QApplication 必须活过所有 QWidget；function-local static 保证。
    static QApplication app(argc, argv);
    return &app;
}

QtBackend::QtBackend(std::uint32_t width, std::uint32_t height, std::string title) {
    auto impl = std::make_unique<Impl>();
    QApplication* app = ensure_qapplication();
    if (app == nullptr) {
        return;  // 无 QApplication（非 GUI 环境），valid() 为 false
    }

    impl->canvas = std::make_unique<termchart_gui::BitmapCanvas>(width, height);
    impl->widget = std::make_unique<CanvasWidget>(*impl->canvas);
    impl->widget->setWindowTitle(QString::fromStdString(title));
    impl->widget->resize(static_cast<int>(width), static_cast<int>(height));
    impl->widget->set_render_hook([impl = impl.get()](std::uint32_t w, std::uint32_t h) {
        if (impl->render_hook) {
            impl->render_hook(w, h);
        }
    });
    impl->widget->set_event_sink([impl = impl.get()](termchart_gui::Event e) {
        // 只有 Quit 才退出消息循环。此前这里对**任何**事件都调 quit()，
        // 导致 run_loop 模式下第一次鼠标移动/尺寸变化就整个进程退出。
        const bool is_quit = e.type == termchart_gui::EventType::Quit;
        impl->push(std::move(e));
        if (is_quit && impl->running) {
            impl->quit_requested = true;
            QApplication::quit();
        }
    });
    impl_ = std::move(impl);
}

QtBackend::~QtBackend() {
    if (impl_ != nullptr && impl_->widget != nullptr) {
        impl_->widget.reset();  // 先于 canvas 销毁（widget 持有 canvas 引用）
    }
}

bool QtBackend::valid() const noexcept {
    return impl_ != nullptr && impl_->widget != nullptr;
}

std::unique_ptr<termchart_gui::TkWindow> QtBackend::create_window(std::uint32_t w,
                                                                  std::uint32_t h,
                                                                  std::string title) {
    auto window = std::make_unique<QtWindow>(impl_->widget.get());
    window->resize(w, h);
    window->set_title(std::move(title));
    return window;
}

termchart_gui::TkCanvas& QtBackend::canvas() { return *impl_->canvas; }

bool QtBackend::poll_event(termchart_gui::Event& out) {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    if (impl_->queue.empty()) {
        return false;
    }
    out = impl_->queue.front();
    impl_->queue.pop_front();
    return true;
}

void QtBackend::present() {
    if (impl_->widget != nullptr) {
        impl_->widget->update();  // 异步排一次 paintEvent
    }
}

void QtBackend::run_loop(termchart_gui::EventLoop& loop) {
    (void)loop;  // Qt 的消息泵即循环；loop 的 post/定时器语义由应用自行桥接
    if (impl_->widget != nullptr) {
        impl_->widget->show();
    }
    impl_->running = true;
    impl_->quit_requested = false;
    QApplication::exec();
    impl_->running = false;
}

void QtBackend::request_close() {
    if (impl_->widget != nullptr) {
        impl_->widget->close();
    }
    QApplication::quit();
}

void QtBackend::set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook) {
    impl_->render_hook = std::move(hook);
}

void QtBackend::process_events() {
    QApplication::processEvents();
}

}  // namespace termchart_gui_qt
