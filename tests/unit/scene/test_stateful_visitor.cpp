// test_stateful_visitor.cpp —— Scene 消费者的**时序**契约
//
// canvas 的 ScenePainter 与 backend/vector 的 SvgVisitor 共用这份时序。这里用
// 一个只记日志的派生类把它钉住：改时序（例如给 end_path 加 `if (path_open_)`
// 守卫、或忘了在 RectOp 前收口）会在这里先红，而不是等到 golden 字节变了才发现。
//
// 注意：下面是**时序**断言，不是输出断言。"画成什么样"由 golden 管。
#include <doctest.h>

#include <string>
#include <vector>

#include "termchart/scene/stateful_visitor.hpp"

namespace {

class RecordingSink final : public termchart::scene::StatefulVisitor {
public:
    std::vector<std::string> log;

protected:
    void begin_path(const termchart::Point&) override { log.push_back("begin"); }
    void extend_path(const termchart::Point&, const termchart::Point&) override {
        log.push_back("extend");
    }
    void end_path() override { log.push_back("end"); }
    void emit_filled_rect(const termchart::Rect&, const termchart::Color&) override {
        log.push_back("filled_rect");
    }
    void emit_stroked_rect(const termchart::Rect&, const termchart::Color&, double) override {
        log.push_back("stroked_rect");
    }
    void emit_arc(const termchart::scene::ArcOp&, const termchart::Color&, double) override {
        log.push_back("arc");
    }
    void emit_text(const termchart::scene::TextOp&) override { log.push_back("text"); }
    void emit_image(const termchart::scene::ImageOp&) override { log.push_back("image"); }
};

}  // namespace

TEST_CASE("StatefulVisitor: Fill/Stroke 先收口再更新") {
    using namespace termchart;

    scene::Scene s;
    s.push(scene::MoveTo{{0.0, 0.0}});
    s.push(scene::LineTo{{1.0, 1.0}});
    s.push(scene::LineTo{{2.0, 2.0}});
    s.push(scene::StrokeOp{Color::ansi16(1), 1.0});
    s.push(scene::LineTo{{3.0, 3.0}});
    s.push(scene::FillOp{Color::ansi16(2)});
    s.push(scene::RectOp{{0.0, 0.0, 5.0, 5.0}});

    RecordingSink sink;
    sink.run(s);

    const std::vector<std::string> expected{
        "begin", "extend", "extend",  // 前两条 LineTo：首个触发 begin
        "end",                        // StrokeOp 收口（此时 stroke 还是旧值）
        "begin", "extend",            // StrokeOp 后首个 LineTo 重新 begin
        "end",                        // FillOp 收口
        // RectOp 自己也要先收口：它前面那条 open path 必须按**旧 stroke_** 刷出，
        // 否则 canvas 侧会把矩形之后的描边一并按新色光栅化，SVG 侧会把 L 段接进
        // 同一个 <path d>。这正是 Fill/Stroke「只作用于其后图元」的必然结果。
        "end",
        "filled_rect",                // RectOp（fill 非空 → 实心分支）
        // run() 收尾：最后那个图元若是**描边矩形**（只累积、自己不 flush），
        // 这次 end_path() 就是它唯一的刷出点——不是可优化的空调用。
        "end",
    };
    CHECK(sink.log == expected);
}

TEST_CASE("StatefulVisitor: MoveTo 不收口，多子路径合并") {
    using namespace termchart;

    scene::Scene s;
    s.push(scene::MoveTo{{0.0, 0.0}});
    s.push(scene::LineTo{{1.0, 1.0}});
    s.push(scene::MoveTo{{5.0, 5.0}});
    s.push(scene::LineTo{{6.0, 6.0}});

    RecordingSink sink;
    sink.run(s);

    // MoveTo 处**没有** end：SVG 靠这一点把多段子路径合成一条 <path>。
    // 但 path_open_ 被重置，所以第二个 LineTo 会重新 begin_path。
    const std::vector<std::string> expected{
        "begin", "extend",  // 第一段
        "begin", "extend",  // MoveTo 后重新 begin
        "end",              // run() 收尾
    };
    CHECK(sink.log == expected);
}

TEST_CASE("StatefulVisitor: 非路径图元后紧跟 LineTo 会重新 begin") {
    using namespace termchart;

    scene::Scene s;
    s.push(scene::MoveTo{{0.0, 0.0}});
    s.push(scene::LineTo{{1.0, 1.0}});
    s.push(scene::RectOp{{2.0, 2.0, 3.0, 3.0}});  // 无填充 → 走描边分支
    s.push(scene::LineTo{{4.0, 4.0}});

    RecordingSink sink;
    sink.run(s);

    // 这条是原 svg_backend 的隐藏 bug 回归：RectOp 之后若没重置 path_open_，
    // 末尾这个 LineTo 就不会发 "M"，产出的 <path d="L ..."> 起点丢失。
    const std::vector<std::string> expected{
        "begin", "extend",   // 第一段折线
        "end",               // RectOp 前的收口
        "stroked_rect",      // 无填充 → 描边矩形
        "begin", "extend",   // RectOp 后重新 begin（修复点）
        "end",               // run() 收尾
    };
    CHECK(sink.log == expected);
}

TEST_CASE("StatefulVisitor: 无填充时 Rect 走描边分支") {
    using namespace termchart;

    scene::Scene s;
    s.push(scene::RectOp{{1.0, 1.0, 4.0, 4.0}});
    s.push(scene::FillOp{Color::ansi16(3)});
    s.push(scene::RectOp{{2.0, 2.0, 5.0, 5.0}});

    RecordingSink sink;
    sink.run(s);

    const std::vector<std::string> expected{
        "end", "stroked_rect",       // 默认无填充
        "end",                       // FillOp 收口
        "end", "filled_rect",        // 有填充 → 实心
        "end",                       // run() 收尾
    };
    CHECK(sink.log == expected);
}
