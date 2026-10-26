// scene_animation.cpp —— 见 chart/scene_animation.hpp
//
// 两遍扫描：
//   1. 按组收集几何特征（路径点 x 的范围、矩形共享边）；
//   2. 重建指令序列：组内按进度改写，组外原样拷贝。
//
// 为什么是"重建"而不是就地改：X 截断会**减少**指令数（跨界的线段之后整段
// 丢弃），而 Scene 只暴露 `std::span`，删不了元素。重建一次比"改坐标再
// 把尾部挪上来"更不容易写错，代价只是一次 vector 分配。
#include "termchart/chart/scene_animation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "termchart/core/point.hpp"
#include "termchart/core/rect.hpp"

namespace termchart::chart {
namespace {

using scene::ArcOp;
using scene::Command;
using scene::FillOp;
using scene::GroupOp;
using scene::ImageOp;
using scene::LineTo;
using scene::MoveTo;
using scene::RectOp;
using scene::StrokeOp;
using scene::TextOp;

constexpr double kFull = 1.0;

// 一个数据组的几何特征。
struct GroupInfo {
    bool has_point = false;  // 组里有路径点（折线/散点连线）→ X 截断才有意义
    double min_x = 0.0;
    double max_x = 0.0;
    bool has_baseline = false;
    double baseline = 0.0;   // 柱子的生长起点（0 值那条线）
};

void note_point(GroupInfo& info, double x) noexcept {
    if (!info.has_point) {
        info.has_point = true;
        info.min_x = x;
        info.max_x = x;
        return;
    }
    info.min_x = x < info.min_x ? x : info.min_x;
    info.max_x = x > info.max_x ? x : info.max_x;
}

// 基线 = 组内矩形边中出现次数最多的那条。
// 这条判据不是猜的：柱状图的每根柱都**一端贴 0 值线**，所以那端的坐标在
// 组内重复 n 次；另一端各不相同。正值柱贴的是底边、负值柱贴的是顶边，
// 两者在混合图里指向**同一条**线，于是这条规则对正负柱同时成立。
//
// 退化：只有一根柱（或每组矩形边互不共享）时频次并列，取最靠下的边
// ——视觉上仍是"从底部长出"，只是负值柱会变成"从最底端往上缩"。
void resolve_baseline(GroupInfo& info, std::vector<double>& edges) {
    if (edges.empty()) {
        return;
    }
    std::sort(edges.begin(), edges.end());
    double best = edges.front();
    std::size_t best_run = 0;
    std::size_t run = 0;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i == 0 || edges[i] == edges[i - 1]) {
            ++run;
        } else {
            run = 1;
        }
        if (run > best_run) {
            best_run = run;
            best = edges[i];
        }
    }
    if (best_run < 2) {
        best = edges.back();  // 没有共享边：固定最底边
    }
    info.has_baseline = true;
    info.baseline = best;
}

struct GroupContext {
    double progress = kFull;
    double cut = 0.0;
    bool cut_enabled = false;
    const GroupInfo* info = nullptr;
    bool path_open = false;  // 已发出 MoveTo 且未跨界
    bool path_done = false;  // 已截断；后续 LineTo 一律丢弃，直到下一个 MoveTo
    Point last{};
};

std::vector<Command> rebuild(scene::Scene& scene,
                             const std::unordered_map<std::uint32_t, std::size_t>& index_of,
                             const std::vector<GroupInfo>& infos,
                             std::unordered_map<std::uint32_t, double> progress_by_group,
                             const SceneAnimationOptions& options) {
    std::vector<Command> out;
    out.reserve(scene.size());

    std::vector<GroupContext> stack;
    for (const Command& command : scene.commands()) {
        if (const auto* group = std::get_if<GroupOp>(&command)) {
            if (group->begin) {
                GroupContext context;
                const auto it = progress_by_group.find(group->id);
                double progress = it == progress_by_group.end() ? kFull : it->second;
                if (std::isnan(progress)) {
                    progress = kFull;  // 脏进度当"已完成"，不把图擦空
                }
                context.progress = std::clamp(progress, 0.0, 1.0);

                const auto info_it = index_of.find(group->id);
                if (info_it != index_of.end()) {
                    context.info = &infos[info_it->second];
                    if (options.clip_lines && context.progress < kFull &&
                        context.info->has_point) {
                        const double cut = context.info->min_x +
                                           (context.info->max_x - context.info->min_x) *
                                               context.progress;
                        context.cut_enabled = true;
                        context.cut = cut;
                    }
                }
                stack.push_back(context);
            } else if (!stack.empty()) {
                stack.pop_back();
            }
            out.push_back(command);  // 组标记自己永远保留：交互层要能按组命中
            continue;
        }

        if (stack.empty()) {
            out.push_back(command);  // 组外 = 轴/网格/标题/图例：动画不许动它们
            continue;
        }

        const GroupContext& context = stack.back();
        if (context.progress >= kFull) {
            out.push_back(command);
            continue;
        }
        if (context.progress <= 0.0) {
            continue;  // 进度 0：组内几何全空，只留组标记
        }

        if (const auto* move = std::get_if<MoveTo>(&command)) {
            if (!context.cut_enabled) {
                out.push_back(command);
                continue;
            }
            // 起点已在截断线右侧：这段路径整体不可见（下一个 MoveTo 重新判定）。
            if (move->to.x <= context.cut) {
                out.push_back(command);
                auto& mutable_context = stack.back();
                mutable_context.path_open = true;
                mutable_context.path_done = false;
                mutable_context.last = move->to;
            }
            continue;
        }

        if (const auto* line = std::get_if<LineTo>(&command)) {
            if (!context.cut_enabled) {
                out.push_back(command);
                continue;
            }
            GroupContext& mutable_context = stack.back();
            if (!mutable_context.path_open || mutable_context.path_done) {
                continue;
            }
            if (line->to.x <= context.cut) {
                out.push_back(command);
                mutable_context.last = line->to;
                continue;
            }
            // 跨界段：插值出落在截断线上的端点，然后这条路到此为止。
            const double dx = line->to.x - context.last.x;
            if (dx > 0.0) {
                const double t = (context.cut - context.last.x) / dx;
                const Point edge{context.cut, context.last.y + (line->to.y - context.last.y) * t};
                if (edge.x > context.last.x) {
                    out.emplace_back(LineTo{edge});
                }
            }
            mutable_context.path_done = true;
            continue;
        }

        if (const auto* rect = std::get_if<RectOp>(&command)) {
            if (!options.grow_bars || context.info == nullptr ||
                !context.info->has_baseline) {
                out.push_back(command);
                continue;
            }
            // 基线侧固定，另一侧按进度缩：y' = base + (y - base) * p，h' = h * p。
            // 这条式子对正柱（底边=base）与负柱（顶边=base）都成立。
            const double base = context.info->baseline;
            Rect grown = rect->rect;
            grown.y = base + (rect->rect.y - base) * context.progress;
            grown.h = rect->rect.h * context.progress;
            out.emplace_back(RectOp{grown});
            continue;
        }

        if (const auto* text = std::get_if<TextOp>(&command)) {
            // 数据点标记（●）属于数据：截断线右边的不该还挂着。
            if (options.clip_texts && context.cut_enabled && text->at.x > context.cut) {
                continue;
            }
            out.push_back(command);
            continue;
        }

        // 圆弧 / 位图 / 填充描边状态：不做生长，原样保留。
        // （圆点标记的"长出来"需要圆心与半径语义，那属于图种，不属于通用拦截器。）
        out.push_back(command);
    }
    return out;
}

template <typename Lookup>
void apply_impl(scene::Scene& scene, const Lookup& lookup,
                const SceneAnimationOptions& options) {
    if (scene.empty()) {
        return;
    }

    std::vector<GroupInfo> infos;
    std::vector<std::vector<double>> edges;  // 与 infos 同下标
    std::unordered_map<std::uint32_t, std::size_t> index_of;
    std::unordered_map<std::uint32_t, double> progress_by_group;
    std::vector<std::uint32_t> stack;
    bool any_animating = false;

    // ── 第一遍：组特征 ────────────────────────────────────────────────
    for (const Command& command : scene.commands()) {
        if (const auto* group = std::get_if<GroupOp>(&command)) {
            if (group->begin) {
                const auto [it, inserted] = index_of.try_emplace(group->id, infos.size());
                if (inserted) {
                    infos.emplace_back();
                    edges.emplace_back();
                }
                progress_by_group.emplace(group->id, lookup(group->id));
                stack.push_back(group->id);
                if (progress_by_group[group->id] < kFull) {
                    any_animating = true;
                }
            } else if (!stack.empty()) {
                stack.pop_back();
            }
            continue;
        }
        if (stack.empty()) {
            continue;
        }
        const auto it = index_of.find(stack.back());
        if (it == index_of.end()) {
            continue;
        }
        GroupInfo& info = infos[it->second];
        if (const auto* move = std::get_if<MoveTo>(&command)) {
            note_point(info, move->to.x);
        } else if (const auto* line = std::get_if<LineTo>(&command)) {
            note_point(info, line->to.x);
        } else if (const auto* rect = std::get_if<RectOp>(&command)) {
            if (rect->rect.h > 0.0) {
                edges[it->second].push_back(rect->rect.y);
                edges[it->second].push_back(rect->rect.bottom());
            }
        }
    }

    if (!any_animating) {
        return;  // 全部 1.0：逐位原样返回，不做一次无意义的重建
    }

    for (std::size_t i = 0; i < infos.size(); ++i) {
        resolve_baseline(infos[i], edges[i]);
    }

    // ── 第二遍：重建 ──────────────────────────────────────────────────
    std::vector<Command> rebuilt =
        rebuild(scene, index_of, infos, std::move(progress_by_group), options);

    scene.clear();
    scene.reserve(rebuilt.size());
    for (Command& command : rebuilt) {
        scene.push(std::move(command));
    }
}

}  // namespace

void apply_animation(scene::Scene& scene, double progress,
                     const SceneAnimationOptions& options) {
    if (!(progress < kFull)) {
        return;  // >=1 或 NaN：原样返回
    }
    apply_impl(scene, [progress](std::uint32_t) { return progress; }, options);
}

void apply_animation(scene::Scene& scene, std::span<const GroupProgress> groups,
                     const SceneAnimationOptions& options) {
    if (groups.empty()) {
        return;
    }
    apply_impl(
        scene,
        [groups](std::uint32_t id) {
            for (const GroupProgress& entry : groups) {
                if (entry.group == id) {
                    return entry.progress;
                }
            }
            return kFull;  // 表里没有的组：与 Chart::progress_of 口径一致
        },
        options);
}

}  // namespace termchart::chart
