#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "termchart/api/print.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/core/rect.hpp"
#include "termchart/core/style.hpp"
#include "termchart/interaction/event.hpp"
#include "termchart/interaction/input_parser.hpp"
#include "termchart/interaction/pan.hpp"
#include "termchart/interaction/zoom.hpp"
#include "termchart/render/renderer.hpp"
#include "termchart/term/mouse_mode.hpp"
#include "termchart/term/raw_mode.hpp"
#include "termchart/term/size.hpp"

namespace {
using namespace termchart;

enum TileType : char {
    OCEAN = '~',
    LAND  = '#',
    ICE   = '*',
    MOUNT = '^'
};

struct WorldMap {
    // 改为 size_t，彻底解决无符号/有符号转换警告
    std::size_t width = 360;
    std::size_t height = 180;
    std::vector<char> tiles;

    WorldMap() : tiles(width * height, OCEAN) {
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                double nx = (static_cast<double>(x) - static_cast<double>(width) / 2.0) / static_cast<double>(width);
                double ny = (static_cast<double>(y) - static_cast<double>(height) / 2.0) / static_cast<double>(height);
                if (std::sin(nx * 10) * std::cos(ny * 8) > 0.2) {
                    tiles[y * width + x] = LAND;
                }
                if (std::abs(ny) > 0.8) {
                    tiles[y * width + x] = ICE;
                }
            }
        }
    }

    char at(int x, int y) const {
        if (x < 0 || x >= static_cast<int>(width) || y < 0 || y >= static_cast<int>(height)) return OCEAN;
        return tiles[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
    }
};

// 把之前的 to_screen 删掉了，因为地图是“从屏幕找数据”（data_at），用不到它。

Point data_at(Point cell, const Rect& area, const interaction::Viewport& viewport) {
    const double nx = (cell.x - area.x) / (area.w > 1.0 ? area.w - 1.0 : 1.0);
    const double ny = 1.0 - (cell.y - area.y) / (area.h > 1.0 ? area.h - 1.0 : 1.0);
    const Rect visible = viewport.visible();
    return Point{visible.x + nx * visible.w, visible.y + ny * visible.h};
}

void draw_map(canvas::Canvas& canvas, const WorldMap& map, const interaction::Viewport& viewport) {
    const double w = static_cast<double>(canvas.width());
    const double h = static_cast<double>(canvas.height());
    const Rect area{0.0, 0.0, w, h};
    
    const Style col_land = Style{}.set_fg(Color::ansi16(2)).set_bg(Color::ansi16(2));
    const Style col_ice  = Style{}.set_fg(Color::ansi16(15)).set_bg(Color::ansi16(15));
    const Style col_ocean= Style{}.set_fg(Color::ansi16(4)).set_bg(Color::ansi16(4));

    // 全部使用无符号和 static_cast
    for (std::uint32_t sy = 0; sy < canvas.height(); ++sy) {
        for (std::uint32_t sx = 0; sx < canvas.width(); ++sx) {
            Point data = data_at(Point{static_cast<double>(sx), static_cast<double>(sy)}, area, viewport);
            
            int tile_x = static_cast<int>(data.x) % static_cast<int>(map.width); 
            if (tile_x < 0) tile_x += static_cast<int>(map.width);
            int tile_y = static_cast<int>(data.y) % static_cast<int>(map.height); 
            if (tile_y < 0) tile_y += static_cast<int>(map.height);

            char tile = map.at(tile_x, tile_y);
            Style style = col_ocean;
            char32_t glyph = U' ';

            if (tile == LAND) { style = col_land; }
            if (tile == ICE)  { style = col_ice; }

            canvas.put(sx, sy, glyph, style);
        }
    }
}

bool handle_event(const interaction::Event& event, interaction::Viewport& viewport,
                  interaction::PanGesture& gesture, const Rect& area) {
    if (const auto* key = std::get_if<interaction::KeyEvent>(&event)) {
        if (key->key == interaction::Key::Escape || key->ch == U'q') return false;
    }
    if (const auto* mouse = std::get_if<interaction::MouseEvent>(&event)) {
        const Point cell{static_cast<double>(mouse->x), static_cast<double>(mouse->y)};
        switch (mouse->action) {
            case interaction::MouseAction::ScrollUp:
                viewport.zoom_at(1.25, data_at(cell, area, viewport));
                break;
            case interaction::MouseAction::ScrollDown:
                viewport.zoom_at(1.0 / 1.25, data_at(cell, area, viewport));
                break;
            case interaction::MouseAction::Press:
                if (mouse->button == interaction::MouseButton::Left) gesture.press(cell);
                break;
            case interaction::MouseAction::Move: {
                auto delta = gesture.drag_to(cell);
                if (delta) {
                    double per_cell_x = viewport.visible().w / area.w;
                    double per_cell_y = viewport.visible().h / area.h;
                    viewport.pan_by(-delta->x * per_cell_x, delta->y * per_cell_y);
                }
                break;
            }
            case interaction::MouseAction::Release:
                gesture.release();
                break;
        }
    }
    return true;
}
} // namespace

int main() {
    WorldMap map;
    interaction::Viewport viewport(
        Rect{0, 0, static_cast<double>(map.width), static_cast<double>(map.height)}, 
        1e-6, false);
    
    auto size = term::query_size();
    std::uint32_t width = size.cols > 40 ? size.cols : 40;
    std::uint32_t height = size.rows > 12 ? size.rows - 2 : 12;

    term::RawMode raw;
    raw.enter();
    if (!raw.active()) {
        std::cout << "请在一个真实的终端中运行此程序以查看交互式地图。\n";
        return 0;
    }
    
    (void)api::write_stdout(term::enable_mouse(term::MouseMode::AnyMotion, term::MouseEncoding::Sgr));

    interaction::InputParser parser;
    interaction::PanGesture gesture;
    render::Renderer renderer;
    const Rect area{0, 0, static_cast<double>(width), static_cast<double>(height)};

    bool running = true;
    while (running) {
        canvas::Canvas canvas(width, height);
        draw_map(canvas, map, viewport);
        (void)api::write_stdout(renderer.render(canvas.to_buffer()));

        char chunk[64] = {};
        std::size_t got = std::fread(chunk, 1, sizeof(chunk), stdin);
        if (got == 0) break;
        
        auto events = parser.feed(std::string_view(chunk, got));
        for (const auto& e : events) {
            running = handle_event(e, viewport, gesture, area);
            if (!running) break;
        }
    }

    (void)api::write_stdout(term::disable_all_mouse(term::MouseEncoding::Sgr));
    return 0;
}