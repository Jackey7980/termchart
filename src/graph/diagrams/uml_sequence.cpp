// uml_sequence.cpp —— UML 时序图渲染：头框 + 生命线 + 消息 + 便签 + 激活条
#include "termchart/graph/diagrams/uml_sequence.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "termchart/canvas/canvas.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/util/wcwidth.hpp"

namespace termchart::graph {

std::size_t SeqModel::index_of(std::string_view key) const noexcept {
    for (std::size_t i = 0; i < participants.size(); ++i) {
        if (participants[i].key == key) {
            return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

namespace {

constexpr char32_t kLifeline = U'\u2502';      // │
constexpr char32_t kActiveBar = U'\u2588';     // █ 激活条
constexpr char32_t kSolidLine = U'\u2500';     // ─
constexpr char32_t kDottedLine = U'\u2504';    // ┄
constexpr char32_t kHeadRight = U'\u25B6';     // ▶ 实心右
constexpr char32_t kHeadLeft = U'\u25C0';      // ◀ 实心左
constexpr char32_t kOpenRight = U'\u25B7';     // ▷ 开口右
constexpr char32_t kOpenLeft = U'\u25C1';      // ◁ 开口左
constexpr char32_t kCross = U'\u00D7';         // × 叉
constexpr char32_t kSelfTurn = U'\u2510';      // ┐ 自消息去程折角
constexpr char32_t kSelfBack = U'\u2518';      // ┘ 自消息回程折角

constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);

int text_width(const std::string& text) { return util::display_width(text); }

char32_t arrow_head(SeqArrowKind kind, bool rightward) noexcept {
    switch (kind) {
        case SeqArrowKind::Solid:
        case SeqArrowKind::Dotted:
            return rightward ? kHeadRight : kHeadLeft;
        case SeqArrowKind::Open:
        case SeqArrowKind::OpenDotted:
            return rightward ? kOpenRight : kOpenLeft;
        case SeqArrowKind::Cross:
        case SeqArrowKind::CrossDotted:
            return kCross;
    }
    return kHeadRight;
}

char32_t line_char(SeqArrowKind kind) noexcept {
    return (kind == SeqArrowKind::Dotted || kind == SeqArrowKind::OpenDotted ||
            kind == SeqArrowKind::CrossDotted)
               ? kDottedLine
               : kSolidLine;
}

struct Item {
    bool is_message = true;
    std::size_t index = 0;  // messages / notes 的下标
};

// 把便签按 after 归位到消息序列里：after == k 的便签插在第 k 条消息之后，
// after 越界（含 npos）的排到最后。返回"时间轴条目"序列。
std::vector<Item> build_timeline(const SeqModel& model) {
    std::vector<Item> items;
    items.reserve(model.messages.size() + model.notes.size());
    // 桶：key = after；npos 与越界统一落尾桶。同桶按登记顺序。
    std::map<std::size_t, std::vector<std::size_t>> buckets;
    for (std::size_t i = 0; i < model.notes.size(); ++i) {
        std::size_t key = model.notes[i].after;
        if (key >= model.messages.size()) {
            key = kNoIndex;
        }
        buckets[key].push_back(i);
    }
    const auto flush = [&](std::size_t key) {
        const auto it = buckets.find(key);
        if (it == buckets.end()) {
            return;
        }
        for (const std::size_t note_index : it->second) {
            items.push_back(Item{false, note_index});
        }
    };
    for (std::size_t k = 0; k < model.messages.size(); ++k) {
        flush(k == 0 ? kNoIndex : k - 1);
        items.push_back(Item{true, k});
    }
    flush(model.messages.empty() ? kNoIndex : model.messages.size() - 1);
    return items;
}

}  // namespace

Rect render_uml_sequence(const SeqModel& model, canvas::Canvas& canvas,
                         const UmlSequenceOptions& opts) {
    if (model.participants.empty()) {
        return Rect{};
    }

    // ---- 列布局：头框依次排开，生命线取头框中心列 ----
    const std::size_t n = model.participants.size();
    std::vector<double> box_x(n, 0.0);
    std::vector<double> box_w(n, 0.0);
    std::vector<double> lifeline_x(n, 0.0);
    double x = opts.margin;
    double width_used = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::string& label =
            model.participants[i].label.empty() ? model.participants[i].key
                                                : model.participants[i].label;
        box_w[i] = static_cast<double>(std::max(6, text_width(label) + 4));
        box_x[i] = x;
        lifeline_x[i] = std::floor(box_x[i] + box_w[i] / 2.0);
        x += box_w[i] + opts.h_gap;
        width_used = x - opts.h_gap;
    }

    const std::vector<Item> items = build_timeline(model);
    constexpr double kHeaderBottom = 3.0;  // 头框占 0..2 行
    const std::size_t count = items.size();
    const double last_line =
        count == 0 ? kHeaderBottom : kHeaderBottom + 1.0 + 2.0 * static_cast<double>(count - 1);
    const double bottom = last_line + 2.0;

    // ---- 激活段：先按消息收集，画生命线时查段 ----
    std::map<std::size_t, std::vector<std::pair<double, double>>> activations;
    std::map<std::size_t, double> active_since;  // 参与者 → 激活起始行
    const auto close_activation = [&](std::size_t p, double end_row) {
        const auto it = active_since.find(p);
        if (it == active_since.end()) {
            return;
        }
        if (end_row > it->second) {
            activations[p].emplace_back(it->second, end_row - 1.0);
        }
        active_since.erase(it);
    };
    for (const Item& item : items) {
        if (!item.is_message) {
            continue;
        }
        const SeqMessage& msg = model.messages[item.index];
        const std::size_t from = model.index_of(msg.from);
        const std::size_t to = model.index_of(msg.to);
        const double line_row = kHeaderBottom + 1.0 + 2.0 * static_cast<double>(item.index);
        if (msg.deactivate_source && from != kNoIndex) {
            close_activation(from, line_row);
        }
        if (msg.activate_target && to != kNoIndex && active_since.find(to) == active_since.end()) {
            active_since.emplace(to, line_row);
        }
    }
    for (const auto& [p, since] : active_since) {
        activations[p].emplace_back(since, bottom);
    }

    // ---- 1) 头框 + 生命线（生命线先画，消息/便签压在其上） ----
    for (std::size_t i = 0; i < n; ++i) {
        const Rect box{box_x[i], 0.0, box_w[i], 3.0};
        draw_shape(canvas, ShapeKind::Box, box, opts.header_style);
        const std::string& label =
            model.participants[i].label.empty() ? model.participants[i].key
                                                : model.participants[i].label;
        const int label_w = text_width(label);
        canvas.draw_text(
            Point{box_x[i] + std::max(2.0, (box_w[i] - static_cast<double>(label_w)) / 2.0), 1.0},
            label, opts.header_style);

        const auto segments = activations.find(i);
        for (double row = kHeaderBottom; row <= bottom; row += 1.0) {
            bool active = false;
            if (segments != activations.end()) {
                for (const auto& [lo, hi] : segments->second) {
                    if (row >= lo && row <= hi) {
                        active = true;
                        break;
                    }
                }
            }
            canvas.put(Point{lifeline_x[i], row}, active ? kActiveBar : kLifeline,
                       opts.lifeline_style);
        }
    }

    // ---- 2) 消息与便签 ----
    for (std::size_t slot = 0; slot < count; ++slot) {
        const Item& item = items[slot];
        const double text_row = kHeaderBottom + 2.0 * static_cast<double>(slot);
        const double line_row = text_row + 1.0;
        if (item.is_message) {
            const SeqMessage& msg = model.messages[item.index];
            const std::size_t from = model.index_of(msg.from);
            const std::size_t to = model.index_of(msg.to);
            if (from == kNoIndex || to == kNoIndex) {
                continue;
            }
            const double a = lifeline_x[from];
            const double b = lifeline_x[to];
            const char32_t line = line_char(msg.kind);
            if (a == b) {
                // 自消息：右出 → 下折 → 左回收，箭头指回生命线。
                canvas.put(Point{a + 1.0, line_row}, line, opts.message_style);
                canvas.put(Point{a + 2.0, line_row}, line, opts.message_style);
                canvas.put(Point{a + 3.0, line_row}, kSelfTurn, opts.message_style);
                canvas.put(Point{a + 3.0, line_row + 1.0}, kSelfBack, opts.message_style);
                canvas.put(Point{a + 2.0, line_row + 1.0}, line, opts.message_style);
                canvas.put(Point{a + 1.0, line_row + 1.0},
                           arrow_head(msg.kind, false), opts.message_style);
            } else {
                const double lo = std::min(a, b);
                const double hi = std::max(a, b);
                for (double cx = lo + 1.0; cx <= hi - 1.0; cx += 1.0) {
                    canvas.put(Point{cx, line_row}, line, opts.message_style);
                }
                canvas.put(Point{b - (a < b ? 1.0 : -1.0), line_row},
                           arrow_head(msg.kind, a < b), opts.message_style);
            }
            if (!msg.text.empty()) {
                const int w = text_width(msg.text);
                canvas.draw_text(Point{(a + b) / 2.0 - static_cast<double>(w) / 2.0, text_row},
                                 msg.text, opts.message_style);
            }
        } else {
            const SeqNote& note = model.notes[item.index];
            const std::string bracketed = "[" + note.text + "]";
            const double w = static_cast<double>(text_width(bracketed));
            double anchor_x = 0.0;
            const std::size_t pa = model.index_of(note.anchor);
            const std::size_t pb = model.index_of(note.anchor_b);
            switch (note.placement) {
                case SeqNote::Placement::Left:
                    if (pa != kNoIndex) {
                        anchor_x = lifeline_x[pa] - 2.0 - w;
                    }
                    break;
                case SeqNote::Placement::Right:
                    if (pa != kNoIndex) {
                        anchor_x = lifeline_x[pa] + 2.0;
                    }
                    break;
                case SeqNote::Placement::Over:
                    if (pa != kNoIndex && pb != kNoIndex) {
                        anchor_x = (lifeline_x[pa] + lifeline_x[pb]) / 2.0 - w / 2.0;
                    } else if (pa != kNoIndex) {
                        anchor_x = lifeline_x[pa] - w / 2.0;
                    }
                    break;
            }
            if (pa == kNoIndex && note.placement != SeqNote::Placement::Over) {
                continue;
            }
            if (pa == kNoIndex && pb == kNoIndex) {
                continue;
            }
            canvas.draw_text(Point{anchor_x, text_row}, bracketed, opts.note_style);
        }
    }

    return Rect{opts.margin, 0.0, width_used - opts.margin, bottom + 1.0};
}

}  // namespace termchart::graph
