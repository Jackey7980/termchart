// native_loop/invalidate.hpp —— 脏区域跟踪
//
// 重绘只重画变化区域：add 累积矩形，take 取出并清空（交给后端做局部 blit）。
// 全局重绘用 add_all(viewport)。
#ifndef TERMCHART_GUI_NATIVE_LOOP_INVALIDATE_HPP
#define TERMCHART_GUI_NATIVE_LOOP_INVALIDATE_HPP

#include <vector>

#include "termchart_gui/fwd.hpp"

namespace termchart_gui {

class Invalidator {
public:
    void add(const Rect& r) { rects_.push_back(r); }

    // 整视口失效：清空已有脏区，记录整块。
    void add_all(const Rect& viewport) {
        rects_.clear();
        rects_.push_back(viewport);
    }

    // 取出并清空脏区。
    [[nodiscard]] std::vector<Rect> take() {
        std::vector<Rect> out = std::move(rects_);
        rects_.clear();
        return out;
    }

    [[nodiscard]] bool empty() const noexcept { return rects_.empty(); }
    void clear() noexcept { rects_.clear(); }

private:
    std::vector<Rect> rects_;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_NATIVE_LOOP_INVALIDATE_HPP
