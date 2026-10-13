// 52_chord.cpp —— 和弦图：实体环形排布 + 弧带连接（规格示例）
//
// 消费 graph/diagrams/chord.hpp：render_chord。五个实体、七条弦（值定带宽，
// 自弦/零值弦库内自动跳过）；环走 graph/shape 的椭圆轮廓，弦为二次贝塞尔弧。
#include <cstddef>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/style.hpp"
#include "termchart/graph/diagrams/chord.hpp"
#include "termchart/render/renderer.hpp"

namespace {

using namespace termchart;

}  // namespace

int main() {
    const std::vector<graph::ChordEntity> entities{
        {"app"}, {"db"}, {"cache"}, {"queue"}, {"auth"},
    };
    const std::vector<graph::ChordLink> links{
        {0, 1, 5.0},  // app -> db
        {1, 0, 3.0},  // db -> app
        {0, 2, 2.0},  // app -> cache
        {0, 3, 3.0},  // app -> queue
        {3, 1, 1.5},  // queue -> db
        {0, 4, 2.5},  // app -> auth
        {4, 2, 1.0},  // auth -> cache
    };

    graph::ChordOptions opts;
    opts.radius = 7.0;
    opts.max_band = 3;
    opts.ring_style = Style{}.set_fg(Color::ansi16(8));
    opts.band_style = Style{}.set_fg(Color::ansi16(6));
    opts.label_style = Style{}.set_fg(Color::ansi16(7));

    canvas::Canvas canvas(64, 20);
    (void)graph::render_chord(entities, links, canvas, opts);

    render::Renderer renderer;
    const Status status = api::write_stdout(renderer.render_full(canvas.to_buffer()) + "\n");
    api::flush_stdout();
    return status == ErrorCode::Ok ? 0 : 1;
}
