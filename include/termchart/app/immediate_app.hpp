// immediate_app.hpp —— 即时模式主循环
//
// Dear ImGui 式"每帧重放"：**所有状态都在 frame 回调的捕获里**，本类不沉淀
// 任何图表状态，只负责两件事——节奏（时钟、帧数上限、delta，复用 Loop 的
// 语义）与渲染管线接入（经 HeadlessSession，全程不碰 stdout；换到真终端时
// 只需把渲染段换成实时 Renderer，frame 回调原样保留）。
//
// 每帧的顺序固定：清画布 → frame 回调作画 → 增量渲染进 frames_。
// 清画布是即时模式的定义：画面完全由本帧的回调决定，前一帧不残留。
#ifndef TERMCHART_APP_IMMEDIATE_APP_HPP
#define TERMCHART_APP_IMMEDIATE_APP_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "termchart/app/headless.hpp"
#include "termchart/app/loop.hpp"
#include "termchart/canvas/canvas.hpp"
#include "termchart/export.hpp"

namespace termchart::app {

// 每帧作画回调：拿到的画布刚被清空；delta_seconds 为帧间隔（已钳制）。
using FrameFn = std::function<void(canvas::Canvas& frame, double delta_seconds)>;

class ImmediateApp {
public:
    // frame 为空 → 每帧渲染空画布（合法：纯节奏测试）。
    ImmediateApp(std::uint32_t width, std::uint32_t height, FrameFn frame,
                 Loop::ClockFn clock = Loop::ClockFn{});

    // 跑 max_frames 帧（0 → 0 帧），返回实际帧数。
    // 每帧：Loop 的一帧（算 delta）→ 清画布 → frame() → 增量渲染。
    TERMCHART_API std::uint64_t run(std::uint64_t max_frames);

    // 停止标志：当前 run() 在本帧回调后退出（转发给内部 Loop）。
    TERMCHART_API void stop() noexcept;

    // 每次 run() 追加（跨 run 保留），便于外部取帧做断言或存 golden。
    [[nodiscard]] const std::vector<std::string>& frames() const noexcept { return frames_; }

    // 累计帧数（跨多次 run()）。
    [[nodiscard]] std::uint64_t frame_count() const noexcept { return loop_.iterations(); }

    [[nodiscard]] Loop& loop() noexcept { return loop_; }
    [[nodiscard]] HeadlessSession& session() noexcept { return session_; }

private:
    void do_frame(double delta_seconds);

    HeadlessSession session_;
    Loop loop_;
    FrameFn frame_;
    std::vector<std::string> frames_;
};

}  // namespace termchart::app

#endif  // TERMCHART_APP_IMMEDIATE_APP_HPP
