#include "termchart/app/headless.hpp"

#include <utility>

namespace termchart::app {

Result<std::string> render_once(std::uint32_t width, std::uint32_t height, const DrawFn& draw,
                                ColorDepth depth) {
    if (width == 0 || height == 0) {
        return Unexpected<ErrorCode>(ErrorCode::InvalidArgument);
    }
    HeadlessSession session(width, height, depth);
    if (draw) {
        draw(session.canvas());
    }
    // 一次性渲染没有"上一帧"，全量即正确形态。
    return session.frame_full();
}

HeadlessSession::HeadlessSession(std::uint32_t width, std::uint32_t height, ColorDepth depth)
    : canvas_(width, height), renderer_(depth) {}

std::string HeadlessSession::frame() {
    // to_buffer() 返回副本：Renderer 持有 prev 状态，不能把引用交给它管理。
    return renderer_.render(canvas_.to_buffer());
}

std::string HeadlessSession::frame_full() {
    return renderer_.render_full(canvas_.to_buffer());
}

void HeadlessSession::reset() noexcept {
    renderer_.reset();
}

}  // namespace termchart::app
