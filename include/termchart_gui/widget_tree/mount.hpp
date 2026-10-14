// widget_tree/mount.hpp —— 把控件树挂到后端窗口
//
// Mount 是 widget_tree 与 backend_tk 之间的胶水：paint_all 触发整树重排+绘制并
// 上屏；dispatch_event 把事件路由到控件树。它不持有树或后端（引用语义）。
#ifndef TERMCHART_GUI_WIDGET_TREE_MOUNT_HPP
#define TERMCHART_GUI_WIDGET_TREE_MOUNT_HPP

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/fwd.hpp"
#include "termchart_gui/native_loop/dispatcher.hpp"
#include "termchart_gui/widget_tree/container.hpp"

namespace termchart_gui {

class Mount {
public:
    Mount(Container& root, Backend& backend) noexcept : root_(root), backend_(backend) {}

    // 整帧：清屏 → 重排整树 → 绘制 → 上屏。
    void paint_all() {
        backend_.canvas().clear(Rgb{20, 20, 20});
        root_.relayout();
        root_.paint(backend_.canvas());
        backend_.present();
    }

    // 把事件路由到控件树，返回是否被消费。
    [[nodiscard]] bool dispatch_event(const Event& e) {
        Dispatcher d(root_);
        return d.dispatch(e);
    }

    [[nodiscard]] Container& root() noexcept { return root_; }
    [[nodiscard]] Backend& backend() noexcept { return backend_; }

private:
    Container& root_;
    Backend& backend_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_MOUNT_HPP
