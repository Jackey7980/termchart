// 17_uml_class.cpp —— UML 类图（三栏类框 + 空心三角泛化箭头 + 总线接点）
//
// 阶段 4 的收尾项之一。它与 14/15/16 的图种不同：**节点框里要放多栏内容**
// （类名 / 属性 / 方法），所以不吃 `graph/diagrams` 的骨架（那套骨架只画居中标签），
// 而是用 `graph` 的模型 + 布局 + 形状原语，自己画框内内容与泛化（继承）箭头。
//
// 画法约定（UML 的通用语义在字符网格上的等价表达）：
//   * 分层：`layout_layered` 把"层"当作**列**，于是基类在最左、派生类依次向右；
//   * 泛化箭头：`派生 ──▷ 基类`，箭头指向基类 —— 这里用 `◁`（U+25C1 空心三角）
//     画在基类右边框旁边；
//   * 连线走**总线**：一个基类的所有派生类共用一条竖线，接点用 ├ ┤ ┌ ┐ └ ┘ 收口，
//     而不是把直角拆成两个字符（与 `graph/diagrams/orgchart.cpp` 同一套收口规则，
//     只是方向相反：那边是左右向、这里是上下向）。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/graph.hpp"
#include "termchart/graph/layout/layout.hpp"
#include "termchart/graph/route/junction.hpp"
#include "termchart/graph/shape/shape.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/util/wcwidth.hpp"

namespace {

using namespace termchart;

// 一个类框的三栏内容。属性/方法自带 UML 可见性前缀（+ 公开 / - 私有 / # 保护）。
struct ClassDef {
    std::string name;
    std::vector<std::string> attributes;
    std::vector<std::string> methods;
};

constexpr char32_t kHorizontal = U'\u2500';      // ─
constexpr char32_t kVertical = U'\u2502';        // │
constexpr char32_t kTeeLeftOnly = U'\u2524';     // ┤：类框分隔线右端 / 父类侧接点
constexpr char32_t kTeeRightOnly = U'\u251C';    // ├：类框分隔线左端
constexpr char32_t kGeneralization = U'\u25C1';  // ◁ 指向基类的空心三角

// 接点字符的规则来自 `graph/route/junction.hpp`（公共件，规则只有一份）。
//
// 这里刻意**不**分两步画（先父类侧接点、再子类侧接点）：父子两侧接点落在同一格时，
// 后写的会覆盖先写的，于是 `──◁──┤` 变成 `──◁──┌`，横线看着断了一格。
// 四个方向一次算出来才不会漏 —— 这才是"由折线自己算接点"的意义。
constexpr char32_t junction_glyph(bool up, bool down, bool left, bool right) noexcept {
    return graph::junction_char(up, down, left, right);
}

std::uint32_t content_width(const ClassDef& def) {
    std::size_t widest = static_cast<std::size_t>(util::display_width(def.name));
    for (const std::string& line : def.attributes) {
        widest = std::max(widest, static_cast<std::size_t>(util::display_width(line)));
    }
    for (const std::string& line : def.methods) {
        widest = std::max(widest, static_cast<std::size_t>(util::display_width(line)));
    }
    return static_cast<std::uint32_t>(widest + 4u);  // 左右各留 2 列
}

std::uint32_t content_height(const ClassDef& def) {
    // 上边框 + 类名 + 分隔 + 属性 +（有方法时）分隔 + 方法 + 下边框
    const std::uint32_t method_separator = def.methods.empty() ? 0u : 1u;
    return 3u + static_cast<std::uint32_t>(def.attributes.size()) + method_separator +
           static_cast<std::uint32_t>(def.methods.size()) + 1u;
}

void put_cell(canvas::Canvas& canvas, double x, double y, char32_t ch, const Style& style) {
    if (x < 0.0 || y < 0.0) {
        return;
    }
    canvas.put(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), ch, style);
}

// 三栏类框：边框 + 居中类名 + 两条分隔线 + 左对齐的属性与方法。
void draw_class_box(canvas::Canvas& canvas, const Rect& box, const ClassDef& def,
                    const Style& border, const Style& name_style, const Style& member_style) {
    graph::draw_shape(canvas, graph::ShapeKind::Box, box, border);

    const auto left = static_cast<std::uint32_t>(box.x);
    const auto top = static_cast<std::uint32_t>(box.y);
    const auto width = static_cast<std::uint32_t>(box.w);

    const auto name_w = static_cast<std::uint32_t>(util::display_width(def.name));
    canvas.draw_text(Point{box.x + static_cast<double>((width - name_w) / 2u), box.y + 1.0},
                     def.name, name_style);

    // 分隔线：左 ├ + 横线 + 右 ┤，与左右边框连成一体。
    const auto separator = [&](std::uint32_t row) {
        canvas.put(left, row, kTeeRightOnly, border);
        for (std::uint32_t i = 1; i + 1 < width; ++i) {
            canvas.put(left + i, row, kHorizontal, border);
        }
        canvas.put(left + width - 1, row, kTeeLeftOnly, border);
    };

    std::uint32_t row = top + 2;
    separator(row);
    ++row;
    for (const std::string& line : def.attributes) {
        canvas.draw_text(Point{box.x + 2.0, static_cast<double>(row)}, line, member_style);
        ++row;
    }
    if (!def.methods.empty()) {
        separator(row);
        ++row;
        for (const std::string& line : def.methods) {
            canvas.draw_text(Point{box.x + 2.0, static_cast<double>(row)}, line, member_style);
            ++row;
        }
    }
}

// 泛化箭头与总线：基类右边框 → 竖总线 → 每个派生类左边框。
// 总线列取"基类右边框之右"与"最左派生类左边框之左"的中点，两侧各留至少 1 格横线。
void draw_generalization(canvas::Canvas& canvas, const Rect& base,
                         const std::vector<Rect>& derived, const Style& style) {
    if (derived.empty()) {
        return;
    }
    const double base_y = std::floor(base.y + base.h / 2.0);

    double left_bound = base.right();
    double right_bound = derived.front().x - 1.0;
    for (const Rect& child : derived) {
        right_bound = std::min(right_bound, child.x - 1.0);
    }
    if (right_bound < left_bound) {
        right_bound = left_bound;  // 间距过窄：退化成紧贴
    }
    const double bus_x = std::min(
        std::max(std::floor((left_bound + right_bound) / 2.0), left_bound), right_bound);

    std::vector<double> child_y;
    child_y.reserve(derived.size());
    for (const Rect& child : derived) {
        child_y.push_back(std::floor(child.y + child.h / 2.0));
    }
    double top_y = base_y;
    double bottom_y = base_y;
    for (const double y : child_y) {
        top_y = std::min(top_y, y);
        bottom_y = std::max(bottom_y, y);
    }

    // 1) 基类 → 总线 的横线（接点格留给第 2 步）。
    for (double x = base.right() + 1.0; x < bus_x; x += 1.0) {
        put_cell(canvas, x, base_y, kHorizontal, style);
    }
    // 2) 竖总线：每一格按"上/下/左/右哪几个方向存在"一次性选字符。
    const auto has_child_row = [&child_y](double y) {
        return std::any_of(child_y.begin(), child_y.end(),
                           [y](double row) { return row == y; });
    };
    for (double y = top_y; y <= bottom_y; y += 1.0) {
        const bool up = y > top_y;
        const bool down = y < bottom_y;
        const bool left = y == base_y;       // 父类侧的横线接进来
        const bool right = has_child_row(y); // 往派生类那边分出去
        put_cell(canvas, bus_x, y, junction_glyph(up, down, left, right), style);
    }
    // 3) 每个派生类：到框前的横线。
    for (const Rect& child : derived) {
        const double y = std::floor(child.y + child.h / 2.0);
        for (double x = bus_x + 1.0; x < child.x; x += 1.0) {
            put_cell(canvas, x, y, kHorizontal, style);
        }
    }
    // 4) 空心三角紧贴基类右边框（箭头指向基类）。放在最后，避免被接点覆盖。
    put_cell(canvas, base.right(), base_y, kGeneralization, style);
}

}  // namespace

int main() {
    // ---- 类模型：一个抽象基类 + 两个派生类，其中一个再有派生类 ----
    const std::vector<ClassDef> classes{
        ClassDef{"Shape", {"# name : string"}, {"+ area() : double", "+ show() : void"}},
        ClassDef{"Circle", {"- r : double"}, {"+ area() : double"}},
        ClassDef{"Rect", {"- w : double", "- h : double"}, {"+ area() : double"}},
        ClassDef{"Square", {"- side : double"}, {"+ area() : double"}},
    };
    constexpr std::size_t kShape = 0;
    constexpr std::size_t kCircle = 1;
    constexpr std::size_t kRect = 2;
    constexpr std::size_t kSquare = 3;

    graph::Graph model;
    std::vector<graph::NodeId> ids;
    ids.reserve(classes.size());
    for (const ClassDef& def : classes) {
        ids.push_back(model.add_node(def.name).id());
    }
    // 边方向 = 基类 → 派生类：`layout_layered` 按最长路径定层，于是基类落在最左列。
    (void)model.add_edge(ids[kShape], ids[kCircle]);
    (void)model.add_edge(ids[kShape], ids[kRect]);
    (void)model.add_edge(ids[kRect], ids[kSquare]);

    // ---- 布局：所有类框取同一尺寸（内容最宽/最高者决定），看起来才整齐 ----
    graph::LayoutOptions options;
    options.min_width = 0.0;
    options.min_height = 0.0;
    for (const ClassDef& def : classes) {
        options.min_width = std::max(options.min_width, static_cast<double>(content_width(def)));
        options.min_height = std::max(options.min_height, static_cast<double>(content_height(def)));
    }
    options.h_gap = 8.0;  // 留出画总线与三角的空间
    options.v_gap = 1.0;
    options.margin = 1.0;

    canvas::Canvas canvas(88, 18);
    (void)graph::layout_layered(model, options);

    const Style border = Style{}.set_fg(Color::ansi16(8));
    const Style name_style = Style{}.set_fg(Color::ansi16(6)).set_attr(Attr::Bold);
    const Style member_style = Style{}.set_fg(Color::ansi16(7));
    const Style edge = Style{}.set_fg(Color::ansi16(8));

    // ---- 先连线，再画框 ----
    draw_generalization(canvas, model.nodes()[kShape].box(),
                        {model.nodes()[kCircle].box(), model.nodes()[kRect].box()}, edge);
    draw_generalization(canvas, model.nodes()[kRect].box(),
                        {model.nodes()[kSquare].box()}, edge);

    for (std::size_t i = 0; i < classes.size(); ++i) {
        draw_class_box(canvas, model.nodes()[i].box(), classes[i], border, name_style,
                       member_style);
    }

    render::Renderer renderer;
    const std::string bytes = renderer.render_full(canvas.to_buffer());
    const Status status = api::write_stdout(bytes);
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
