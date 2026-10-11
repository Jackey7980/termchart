// scene_renderer.hpp —— Scene → Canvas，**终端路径上的唯一桥接点**
//
// 为什么只有这一处能解释 Scene 指令：
//   * vector 后端（backend/vector/*）直接消费 Scene，不经过 Canvas；
//   * 终端路径必须先把矢量指令离散成格，这一层转换只应存在一份。
// 若第二个文件开始 switch Command 的类别，两份实现对同一 Scene 的解释迟早分叉。
#ifndef TERMCHART_CANVAS_SCENE_RENDERER_HPP
#define TERMCHART_CANVAS_SCENE_RENDERER_HPP

#include "termchart/canvas/canvas.hpp"
#include "termchart/export.hpp"
#include "termchart/scene/scene.hpp"

namespace termchart::canvas {

// 把 Scene 画到 Canvas。无返回、不抛异常：画不下的部分静默丢弃。
TERMCHART_API void render_scene(const scene::Scene& scene, Canvas& canvas) noexcept;

}  // namespace termchart::canvas

#endif  // TERMCHART_CANVAS_SCENE_RENDERER_HPP
