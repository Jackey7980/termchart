// 32_quick_one_liner.cpp —— 零配置一行出图
//
// 展示 api/quick.hpp 的定位：不需要了解 CellBuffer、Renderer、SGR，
// 一行调用即得到正确（且按终端能力降级过的）输出。
#include <cstdio>

#include "termchart/api/quick.hpp"
#include "termchart/core/style.hpp"

int main() {
    // init_core() 内部由 std::call_once 守卫，重复调用无害。
    termchart::api::init_core();

    if (const termchart::Status status = termchart::api::hello();
        status != termchart::ErrorCode::Ok) {
        std::fprintf(stderr, "输出失败: %s\n", termchart::describe(status));
        return 1;
    }

    // 同一接口的"带样式"用法：只多一行 Style。
    termchart::Style warning;
    warning.set_fg(termchart::Color::rgb(246, 193, 90)).set_attr(termchart::Attr::Bold);
    return termchart::api::print_line("one-liner done", warning) == termchart::ErrorCode::Ok ? 0 : 1;
}
