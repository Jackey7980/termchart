// src/wasm/termchart_wasm.cpp —— Emscripten WASM 目标：把 image::Bitmap 推到网页 <canvas>。
//
// 本文件**只在 Emscripten 工具链下编译**（顶层 CMakeLists 用
// `if(EMSCRIPTEN AND TERMCHART_BUILD_WASM)` 门控），桌面/CI 构建不会碰它，故可安全
// #include <emscripten/...>。普通构建里 src/wasm/ 不是模块目录，也不会被任何 glob 误编。
//
// 出口（C，供 JS 侧 Module.ccall 调用，见 examples/wasm/shell.html）：
//   * termchart_blit_canvas(rgba, w, h, channels)：把一段 RGB(A) 像素推到 id="canvas"
//     的 2D 上下文（EM_ASM 调 putImageData，零 embind 依赖）。
//   * termchart_render_demo()：画一张示例 Bitmap（平滑渐变 + 正弦曲线）并推送，作首屏 demo。
#include <cmath>
#include <cstdint>

#include <emscripten/emscripten.h>

#include "termchart/media/image/bitmap.hpp"

using namespace termchart;

// 本地 smoothstep（GLSL 同形），避免为了一个示例再引 math::interpolate。
static double smooth(double x) {
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return x * x * (3.0 - 2.0 * x);
}

// 把像素缓冲推到页面 <canvas>（id="canvas"）。rgba 指向 Emscripten 堆上的连续像素，
// 行步长 = w * channels（Bitmap 无行对齐填充），故可直接整体拷进 ImageData。
extern "C" EMSCRIPTEN_KEEPALIVE
void termchart_blit_canvas(const uint8_t* rgba, int w, int h, int channels) {
    EM_ASM({
        // 注意：EM_ASM 是普通可变参宏，预处理只按圆括号配对、不认花括号——花括号内
        // 的裸逗号会切断宏参数（code 只拿到第一个逗号之前的内容，其余被当 C++ 解析）。
        // 故这里一行只声明一个变量；需要逗号的表达式一律包在圆括号里（见下方 for）。
        var w = $1;
        var h = $2;
        var c = $3;
        var canvas = document.getElementById('canvas');
        if (!canvas) return;
        canvas.width = w;
        canvas.height = h;
        var ctx = canvas.getContext('2d');
        var img = ctx.createImageData(w, h);
        var ptr = $0;
        var n = w * h * c;
        for (var i = 0, j = 0; i < n; i += c, j += 4) {
            img.data[j]     = HEAPU8[ptr + i];
            img.data[j + 1] = HEAPU8[ptr + i + 1];
            img.data[j + 2] = HEAPU8[ptr + i + 2];
            img.data[j + 3] = (c === 4) ? HEAPU8[ptr + i + 3] : 255;
        }
        ctx.putImageData(img, 0, 0);
    }, rgba, w, h, channels);
}

// 示例：平滑渐变背景 + 一条正弦曲线，渲染进 RGBA Bitmap 后推送。
// 展示"任意 chart/图像 → Bitmap → <canvas>"的通用管线；用户可替换成自己的 Bitmap。
extern "C" EMSCRIPTEN_KEEPALIVE
void termchart_render_demo() {
    const int w = 480, h = 240;
    image::Bitmap bmp(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h), 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double u = static_cast<double>(x) / w;
            const double v = static_cast<double>(y) / h;
            const std::uint8_t r = static_cast<std::uint8_t>(255.0 * smooth(v));
            const std::uint8_t g =
                static_cast<std::uint8_t>(255.0 * smooth(1.0 - std::fabs(u - 0.5) * 2.0));
            const std::uint8_t b = static_cast<std::uint8_t>(255.0 * smooth(1.0 - v));
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, r);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, g);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, b);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 3, 255);
        }
    }
    // 正弦曲线（白线）。
    for (int x = 0; x < w; ++x) {
        const double t = static_cast<double>(x) / w * 6.283185307179586;
        const int y = static_cast<int>((0.5 - 0.35 * std::sin(t)) * h);
        if (y >= 0 && y < h) {
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 0, 255);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1, 255);
            bmp.set(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 2, 255);
        }
    }
    termchart_blit_canvas(bmp.data(), w, h, 4);
}

// Emscripten 下 main 只负责首屏 demo（实际调用多由 JS 触发）；非 Emscripten 不编此文件。
#ifdef __EMSCRIPTEN__
int main() {
    termchart_render_demo();
    return 0;
}
#endif
