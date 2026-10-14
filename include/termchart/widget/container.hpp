// container.hpp —— 容器基类：控件树中枢（保留模式几何 + 命中测试）
//
// 与 widget.hpp 的设计一致：Widget 只管身份/几何/命中测试，绘制走 immediate_widget
// 的即时函数。Container 在此基础上持有子控件（裸指针，生命周期由调用方管理），
// 提供递归命中测试与统一的 layout 契约。它是布局控件的公共基类。
#ifndef TERMCHART_WIDGET_CONTAINER_HPP
#define TERMCHART_WIDGET_CONTAINER_HPP

#include <span>
#include <string>
#include <vector>

#include "termchart/core/rect.hpp"
#include "termchart/widget/widget.hpp"

namespace termchart::widget {

// 子控件相对容器主轴的对齐方式（交叉轴同理）。
enum class Alignment {
    Start,    // 靠起始边
    Center,   // 居中
    End,      // 靠结束边
    Stretch,  // 拉伸填满交叉轴
};

// 容器基类。持有子控件并提供递归命中测试与 layout 契约。
class Container : public Widget {
public:
    explicit Container(std::string id);
    ~Container() override;

    // 加入子控件。引用版本存地址，指针版本直接存；空指针或重复添加被忽略。
    void add(Widget& child);
    void add(Widget* child);

    // 移除：成功返回 true；不在树中返回 false（no-op，不报错）。
    [[nodiscard]] bool remove(Widget& child) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::span<Widget* const> children() const noexcept { return children_; }

    // 递归命中测试：任一可见子控件命中即命中；否则回退到自身 bounds。
    [[nodiscard]] bool hit_test(Point p) const noexcept override;

    // layout 契约（纯虚）：依据自身 bounds() 重排子控件几何。基类 Widget 不定义
    // layout（那是容器的事），所以这里声明一个新的纯虚函数，子类用 override 覆写。
    virtual void layout() = 0;

    // 整树重排：先排自己，再递归子容器（叶子控件是 Widget::relayout 的空实现）。
    void relayout() noexcept;

    void set_dirty(bool on = true) noexcept { dirty_ = on; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

protected:
    std::vector<Widget*> children_;
    bool dirty_ = true;
};

}  // namespace termchart::widget

#endif  // TERMCHART_WIDGET_CONTAINER_HPP
