// qt_backend.cpp —— Qt 后端实现（Qt5/Qt6，PIMPL 隔离 Qt 类型）
#include "termchart_gui_qt/qt_backend.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QTimer>

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

// 绘制 surface 的 QWidget：paintEvent 把 BitmapCanvas 的 RGBA 直贴。
// Qt 的原子是 QImage（像素矩形真彩），与 TkCanvas 契约一致。
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
        // Format_RGBA8888（Qt5 为 Format_RGBA8888；内存布局 RGBA 与 Bitmap 一致）。
        // 注意 Bitmap 行宽未必 4 字节对齐：按 bytesPerLine 直建，由 QImage 引用外部
        // 缓冲（不拷贝），只用于本次绘制，生命周期覆盖 paintEvent 全程。
        const auto* data = reinterpret_cast<const uchar*>(px.data());
        const QImage image(data, static_cast<int>(px.width()), static_cast<int>(px.height()),
                           static_cast<int>(px.row_bytes()), QImage::Format_RGBA8888);
        painter.drawImage(QPoint{0, 0}, image);
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (render_hook_) {
            render_hook_(static_cast<std::uint32_t>(e->size().width()),
                         static_cast<std::uint32_t>(e->size().height()));
        }
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::Resize;
        ev.width = static_cast<std::uint32_t>(e->size().width());
        ev.height = static_cast<std::uint32_t>(e->size().height());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MousePress;
        ev.x = static_cast<double>(e->localPos().x());
        ev.y = static_cast<double>(e->localPos().y());
        ev.button = static_cast<int>(e->button());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MouseRelease;
        ev.x = static_cast<double>(e->localPos().x());
        ev.y = static_cast<double>(e->localPos().y());
        ev.button = static_cast<int>(e->button());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::MouseMove;
        ev.x = static_cast<double>(e->localPos().x());
        ev.y = static_cast<double>(e->localPos().y());
        ev.button = static_cast<int>(e->buttons());
        ev.modifiers = modifiers_of(e->modifiers());
        if (event_sink_) {
            event_sink_(std::move(ev));
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        termchart_gui::Event ev{};
        ev.type = termchart_gui::EventType::Wheel;
        ev.x = static_cast<double>(e->localPos().x());
        ev.y = static_cast<double>(e->localPos().y());
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
    QWidget* widget_;  // 不持有：生命周期归 QApplication 对象树
};

}  // namespace

struct QtBackend::Impl {
    std::unique_ptr<termchart_gui::BitmapCanvas> canvas;
    std::unique_ptr<CanvasWidget> widget;
    bool running = false;
    bool owns_app = false;  // 进程内首个后端负责创建 QApplication

    std::mutex queue_mutex;
    std::deque<termchart_gui::Event> queue;
    std::function<void(std::uint32_t, std::uint32_t)> render_hook;

    void push(termchart_gui::Event e) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        queue.push_back(std::move(e));
    }
};

// QApplication 是进程级单例：首个后端创建它，之后复用 static qApp。
static QApplication* ensure_qapplication() {
    if (QApplication::instance() != nullptr) {
        return static_cast<QApplication*>(QApplication::instance());
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
    impl->owns_app = true;

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
        impl->push(std::move(e));
        if (impl->running) {
            QApplication::quit();  // 收到 Quit 事件时退出 exec()
        }
    });
    impl_ = std::move(impl);
}

QtBackend::~QtBackend() {
    if (impl_ != nullptr && impl_->widget != nullptr) {
        impl_->widget.reset();
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
    (void)loop;  // Qt 的消息泵即循环；loop 提供 post/定时器语义由应用自行桥接
    if (impl_->widget != nullptr) {
        impl_->widget->show();
    }
    impl_->running = true;
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

// CanvasWidget 是 Q_OBJECT 子类且定义在本 TU：AUTOMOC 为含 Q_OBJECT 的 cpp
// 生成同名 .moc，此处显式拉入（qmake 风格约定）。
#include "qt_backend.moc"

}  // namespace termchart_gui_qt
