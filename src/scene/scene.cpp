#include "termchart/scene/scene.hpp"

namespace termchart::scene {

void Scene::clear() noexcept { commands_.clear(); }

void Scene::reserve(std::size_t count) { commands_.reserve(count); }

void Scene::push(Command command) { commands_.push_back(std::move(command)); }

Rect Scene::bounds() const noexcept {
    // 手写 min/max，不用 Rect::union_with：后者把 w/h 为 0 的矩形当"空"直接
    // 返回对方，而"包围盒退化为单个点"恰恰是合法结果。
    bool have = false;
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;

    auto add_point = [&](double x, double y) {
        if (!have) {
            min_x = max_x = x;
            min_y = max_y = y;
            have = true;
            return;
        }
        min_x = x < min_x ? x : min_x;
        min_y = y < min_y ? y : min_y;
        max_x = x > max_x ? x : max_x;
        max_y = y > max_y ? y : max_y;
    };
    auto add_rect = [&](const Rect& r) {
        if (r.empty()) {
            return;
        }
        add_point(r.x, r.y);
        add_point(r.right(), r.bottom());
    };

    for (const Command& command : commands_) {
        switch (kind_of(command)) {
            case CommandKind::MoveTo: {
                const Point& p = std::get<MoveTo>(command).to;
                add_point(p.x, p.y);
                break;
            }
            case CommandKind::LineTo: {
                const Point& p = std::get<LineTo>(command).to;
                add_point(p.x, p.y);
                break;
            }
            case CommandKind::Rect: {
                add_rect(std::get<RectOp>(command).rect);
                break;
            }
            case CommandKind::Arc: {
                // 保守外接方框：不解析起止角度，避免为"部分圆弧"引入三角函数。
                const ArcOp& arc = std::get<ArcOp>(command);
                add_rect(Rect{arc.center.x - arc.radius, arc.center.y - arc.radius,
                              arc.radius * 2.0, arc.radius * 2.0});
                break;
            }
            case CommandKind::Image: {
                add_rect(std::get<ImageOp>(command).rect);
                break;
            }
            case CommandKind::Curve: {
                // 贝塞尔控制点可能落在端点之外，故连同端点一起纳入保守包围盒；
                // 起点（from = 当前点）由前面的 MoveTo/LineTo 的 add_point 已捕获。
                const CurveTo& c = std::get<CurveTo>(command);
                add_point(c.ctrl1.x, c.ctrl1.y);
                add_point(c.ctrl2.x, c.ctrl2.y);
                add_point(c.end.x, c.end.y);
                break;
            }
            // 以下指令不贡献几何（字体度量不在 IR 层算、分组/裁剪/样式/变换只改
            // 绘制行为而非新增内容范围），故不影响包围盒。显式列出每个枚举值，
            // 不用 default，确保扩编 CommandKind 时 -Wswitch 强制补到此处。
            case CommandKind::Text:
            case CommandKind::Fill:
            case CommandKind::Stroke:
            case CommandKind::Group:
            case CommandKind::PushTransform:
            case CommandKind::PopTransform:
            case CommandKind::Clip:
            case CommandKind::BeginPath:
            case CommandKind::ClosePath:
            case CommandKind::LinearGradient:
            case CommandKind::RadialGradient:
            case CommandKind::GlobalAlpha:
            case CommandKind::Dash:
            case CommandKind::SetLineWidth:
            case CommandKind::SetStrokeCap:
            case CommandKind::BlendMode:
                break;
        }
    }

    return have ? Rect{min_x, min_y, max_x - min_x, max_y - min_y} : Rect{};
}

bool Scene::operator==(const Scene& other) const noexcept { return commands_ == other.commands_; }

bool Scene::operator!=(const Scene& other) const noexcept { return !(*this == other); }

}  // namespace termchart::scene
