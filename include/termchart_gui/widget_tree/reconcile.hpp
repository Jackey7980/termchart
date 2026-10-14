// widget_tree/reconcile.hpp —— 两棵树按 id 差异更新
//
// 平行栈的"声明式刷新"原语：把旧树与新树（同一逻辑树的不同快照）比较，产出
// 增/删/改清单。这里只比较结构（id 出现与否）与几何（global_bounds 是否变化），
// 属性 diff 由调用方在 Update 后自行应用。
#ifndef TERMCHART_GUI_WIDGET_TREE_RECONCILE_HPP
#define TERMCHART_GUI_WIDGET_TREE_RECONCILE_HPP

#include <map>
#include <string>
#include <vector>

#include "termchart_gui/fwd.hpp"
#include "termchart_gui/widget_tree/node.hpp"

namespace termchart_gui {

enum class ReconcileKind { Add, Remove, Update };

struct ReconcileOp {
    ReconcileKind kind;
    std::string id;
    Rect after{};  // Update 后的全局几何（Add 时同）
};

namespace detail {

inline void collect(const Node& n, std::map<std::string, Rect>& out) {
    out[n.id()] = n.global_bounds();
    for (const auto& c : n.children()) {
        collect(*c, out);
    }
}

}  // namespace detail

// 比较 old_tree 与 new_tree（按 id），返回变更列表（顺序：先删后加再改）。
[[nodiscard]] inline std::vector<ReconcileOp> reconcile(const Node& old_tree,
                                                       const Node& new_tree) {
    std::map<std::string, Rect> old_map;
    std::map<std::string, Rect> new_map;
    detail::collect(old_tree, old_map);
    detail::collect(new_tree, new_map);

    std::vector<ReconcileOp> ops;
    for (const auto& kv : old_map) {
        if (new_map.find(kv.first) == new_map.end()) {
            ops.push_back(ReconcileOp{ReconcileKind::Remove, kv.first, {}});
        }
    }
    for (const auto& kv : new_map) {
        auto it = old_map.find(kv.first);
        if (it == old_map.end()) {
            ops.push_back(ReconcileOp{ReconcileKind::Add, kv.first, kv.second});
        } else if (it->second != kv.second) {
            ops.push_back(ReconcileOp{ReconcileKind::Update, kv.first, kv.second});
        }
    }
    return ops;
}

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_WIDGET_TREE_RECONCILE_HPP
