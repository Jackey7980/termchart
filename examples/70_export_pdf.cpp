// examples/70_export_pdf.cpp —— Scene → PDF 后端（写文件 / 写 stdout）
//
// 用法：70_export_pdf [output_path]
//   默认：out.pdf（写文件）；传 --stdout 则把 PDF 字节流写到 stdout。
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "termchart/api/print.hpp"
#include "termchart/backend/vector/pdf_backend.hpp"
#include "termchart/core/point.hpp"
#include "termchart/core/style.hpp"
#include "termchart/scene/command.hpp"
#include "termchart/scene/scene.hpp"

int main(int argc, char** argv) {
    using namespace termchart;

    scene::Scene scene;
    const double w = 400.0;
    const double h = 300.0;

    // 边框
    scene.push(scene::StrokeOp{Color::rgb(80, 80, 80), 1.0});
    scene.push(scene::RectOp{Rect{2.0, 2.0, w - 4.0, h - 4.0}});

    // 三条彩色填充矩形
    scene.push(scene::FillOp{Color::rgb(220, 60, 60)});
    scene.push(scene::RectOp{Rect{20.0, 30.0, 100.0, 60.0}});
    scene.push(scene::FillOp{Color::rgb(60, 180, 75)});
    scene.push(scene::RectOp{Rect{140.0, 30.0, 100.0, 60.0}});
    scene.push(scene::FillOp{Color::rgb(50, 100, 220)});
    scene.push(scene::RectOp{Rect{260.0, 30.0, 100.0, 60.0}});

    // 正弦折线
    scene.push(scene::StrokeOp{Color::rgb(20, 20, 20), 1.5});
    scene.push(scene::MoveTo{Point{20.0, 200.0}});
    for (int i = 0; i <= 200; ++i) {
        const double x = 20.0 + (w - 40.0) * static_cast<double>(i) / 200.0;
        const double y = 200.0 - 60.0 * std::sin(x / 30.0);
        scene.push(scene::LineTo{Point{x, y}});
    }

    // 文本
    scene.push(scene::TextOp{Point{20.0, 250.0}, "termchart PDF backend",
                              Style{}.set_fg(Color::rgb(40, 40, 40))});

    backend::vector::PdfBackend backend;
    backend::vector::PdfOptions popts;
    popts.width_pt  = 612.0;
    popts.height_pt = 792.0;
    backend.set_options(popts);

    const std::string bytes = backend::vector::render_pdf(backend, scene);

    if (argc > 1 && std::string(argv[1]) != "--stdout") {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        std::fprintf(stderr, "wrote %zu bytes to %s\n", bytes.size(), argv[1]);
    } else {
        api::write_stdout(bytes);
        api::flush_stdout();
    }
    return 0;
}
