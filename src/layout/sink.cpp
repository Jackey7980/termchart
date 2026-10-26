// sink.cpp —— Sink 的虚析构锚点
//
// CanvasSink 定义在 canvas_sink.hpp（header-only 地转发到 canvas），本文件只
// 负责把 Sink 的虚表/析构固定在一个 TU 里，避免每个包含者各生成一份。
#include "termchart/layout/sink.hpp"

namespace termchart::layout {

Sink::~Sink() = default;

}  // namespace termchart::layout
