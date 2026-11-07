// qt_backend.hpp —— 原生 Qt 后端（可选插件，默认不编进核心）
//
// 定位：termchart_gui 平行栈的**真实窗口后端**之一（与 gui_win32 并列）。
// 实现策略与 Win32 后端同构：控件/布局/绘制零改动（只面向 TkCanvas 抽象），
// 本后端持一块离屏 image::Bitmap 当画布（复用 BitmapCanvas），QWidget 的
// paintEvent 把位图整块贴进窗口（RGBA → QImage::Format_RGBA8888 直 Compatibility）。
//
// 为什么它是插件而非核心：Qt 是重量级外部依赖（多模块二进制），违背核心
// "零强制依赖、header-only"硬约束。故独立目标、CMake 选项默认 OFF；
// 显式开启但找不到 Qt 时**配置期报错**（fail-loud，不静默跳过）。
//
// 平台隔离：Qt 头只出现在 src/qt_backend.cpp；本公开头不泄漏任何 Qt
// 类型（QApplication/QWidget 全部藏进 PIMPL）。Qt5 / Qt6 均可（cpp 内按
// QT_VERSION 分支）。
//
// 许可边界（重要）：Qt 是 LGPLv3 / GPLv3 / 商业 三重授权，termchart 本体是
// 宽松许可。二者组合同样受约束：
//   - **动态链接** Qt 时，只要随分发物附上 LGPLv3 全文与 Qt 源码获取途径、
//     且不阻止用户替换 Qt 库本身，即可保持本项目的宽松许可（§4 合规）；
//   - **静态链接** Qt 会触发 §4(d) 的「安装信息 / 可重链接」义务，需向用户
//     提供目标文件或等效手段，与本项目定位冲突 —— CMakeLists 已 fail-loud
//     拦截静态 Qt；
//   - **不要**为了省事把本项目改成 GPLv3：那会让所有使用者被传染，且与
//     APACHE-2.0/MIT 的既有声明也不一致。
// 本后端**不 vendoring 任何 Qt 二进制**，一切经 find_package 由使用者自备。
#ifndef TERMCHART_GUI_QT_BACKEND_HPP
#define TERMCHART_GUI_QT_BACKEND_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"

namespace termchart_gui_qt {

class QtBackend final : public termchart_gui::Backend {
public:
    // Impl 定义在 .cpp（PIMPL，持 QApplication/QWidget）。
    struct Impl;

    // width/height 为初始客户区尺寸（像素）；title 为窗口标题。
    QtBackend(std::uint32_t width, std::uint32_t height, std::string title);
    ~QtBackend() override;
    QtBackend(const QtBackend&) = delete;
    QtBackend& operator=(const QtBackend&) = delete;

    // QApplication / 窗口是否就绪（无显示环境时构造失败）。
    [[nodiscard]] bool valid() const noexcept;

    // Backend 接口。
    [[nodiscard]] std::unique_ptr<termchart_gui::TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) override;
    [[nodiscard]] termchart_gui::TkCanvas& canvas() override;
    [[nodiscard]] bool poll_event(termchart_gui::Event& out) override;
    void present() override;
    void run_loop(termchart_gui::EventLoop& loop) override;

    // 请求关闭窗口（触发 closeEvent → Quit 事件 + 退出消息循环）。
    void request_close();

    // "尺寸变化时同步重渲染"钩子（与 Win32 同款语义）：resizeEvent 里就地
    // 回调（传新客户区宽高），应用可立刻重排版 + 重绘，避免拖动卡顿。
    void set_render_hook(std::function<void(std::uint32_t, std::uint32_t)> hook);

    // 供应用层主动泵一次 Qt 事件（不依赖 run_loop 的自持循环模式也用得上）。
    void process_events();

    // 显式把 QWidget 显示出来。Qt 不像 Win32/Tk 那样构造即可见，必须 show()
    // 窗口才会弹出；run_loop() 内部也会 show()，二者不冲突（幂等）。
    // 适用「应用自持 process_events 循环」模式（见 qt_demo）。
    void show() noexcept;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart_gui_qt

#endif  // TERMCHART_GUI_QT_BACKEND_HPP
