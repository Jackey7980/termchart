// widget_tree/node.hpp —— 控件树节点（保留模式几何树，像素坐标）
//
// 平行栈的 widget 层重写（不复用终端 widget/）：终端的 1:2 字符格比与 tty 焦点
// 模型对位图渲染器不适用。这里是一棵纯几何/命中测试的树，绘制交给 TkCanvas。
#ifndef TERMCHART_GUI_WIDGET_TREE_NODE_HPP
#define TERMCHART_GUI_WIDGET_TREE_NODE_HPP

#include <memory>
#include <string>
#include <vector>

#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

class Node {
public:
    explicit Node(std::string id) : id_(std::move(id)) {}
    virtual ~Node() = default;

    const std::string& id() const noexcept { return id_; }

    Node* parent() noexcept { return parent_; }
    const Node* parent() const noexcept { return parent_; }

    const std::vector<std::unique_ptr<Node>>& children() const noexcept { return children_; }
    std::vector<std::unique_ptr<Node>>& children() noexcept { return children_; }

    void add_child(std::unique_ptr<Node> child) {
        if (!child) {
            return;
        }
        child->parent_ = this;
        children_.push_back(std::move(child));
    }

    std::unique_ptr<Node> remove_child(Node* child) {
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if (it->get() == child) {
                auto p = std::move(*it);
                children_.erase(it);
                p->parent_ = nullptr;
                return p;
            }
        }
        return nullptr;
    }

    // 便捷构造并挂接子节点，返回引用。
    template <class T, class... A>
    T& emplace_child(A&&... args) {
        auto c = std::make_unique<T>(std::forward<A>(args)...);
        T* p = c.get();
        add_child(std::move(c));
        return *p;
    }

    bool visible() const noexcept { return visible_; }
    void set_visible(bool v) noexcept { visible_ = v; }

    bool enabled() const noexcept { return enabled_; }
    void set_enabled(bool e) noexcept { enabled_ = e; }

    Rect bounds() const noexcept { return bounds_; }  // 相对父左上角
    void set_bounds(Rect r) noexcept { bounds_ = r; }

    Point pos() const noexcept { return Point{bounds_.x, bounds_.y}; }
    void set_pos(Point p) noexcept {
        bounds_.x = p.x;
        bounds_.y = p.y;
    }
    double width() const noexcept { return bounds_.w; }
    double height() const noexcept { return bounds_.h; }
    void set_size(double w, double h) noexcept {
        bounds_.w = w;
        bounds_.h = h;
    }

    // 全局坐标：累加父链。
    [[nodiscard]] Rect global_bounds() const noexcept {
        Rect g = bounds_;
        if (parent_) {
            const Rect pg = parent_->global_bounds();
            g.x += pg.x;
            g.y += pg.y;
        }
        return g;
    }

    // 命中测试（全局坐标）：可见+启用+在范围内；子优先，否则自身命中。
    [[nodiscard]] virtual bool hit_test(const Point& global) const {
        if (!visible_ || !enabled_) {
            return false;
        }
        if (!global_bounds().contains(global)) {
            return false;
        }
        for (const auto& c : children_) {
            if (c->hit_test(global)) {
                return true;
            }
        }
        return true;
    }

    // 类型下探钩子（避免 RTTI）：默认 nullptr，Widget/Container 覆写。
    [[nodiscard]] virtual Widget* as_widget() noexcept { return nullptr; }
    [[nodiscard]] virtual Container* as_container() noexcept { return nullptr; }

protected:
    std::string id_;
    Node* parent_ = nullptr;
    std::vector<std::unique_ptr<Node>> children_;
    Rect bounds_{};
    bool visible_ = true;
    bool enabled_ = true;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_NODE_HPP
