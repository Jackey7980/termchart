// jni_backend.hpp —— JNI 后端（Take 2：Java/Android 宿主拥有窗口）
//
// 定位：Android 与桌面 Java 共用同一套像素契约。Java 侧持有一个 int[]（ARGB）当
// "窗口表面"，把 (JNIEnv*, jintArray, w, h) 注入本后端；present() 把 BitmapCanvas
// 的 RGBA 重排成 ARGB 一次性 SetIntArrayRegion 写回该数组，Java 侧再
// BufferedImage.setRGB / Canvas.drawBitmap 贴到自己的 View。
//
// 兼容性：
//   * Android  → 同一 .cpp 直接编进 NDK（jni.h 由 NDK 提供），Java 侧把 int[] 交给
//               Bitmap 即可；事件由 Android View 的 onTouchEvent 经 inject_event 回传。
//   * 桌面 Java → Swing/AWT 的 JPanel 用 BufferedImage.setRGB 呈现，同样走本后端。
// 不创建/不销毁任何 Java 窗口；窗口（Activity / JFrame）归宿主。
//
// 平台隔离：<jni.h> 只出现在 src/jni_backend.cpp；公开头零 JNI 类型（PIMPL）。
#ifndef TERMCHART_GUI_JNI_BACKEND_HPP
#define TERMCHART_GUI_JNI_BACKEND_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/native_surface.hpp"

namespace termchart_gui_jni {

class JniBackend final : public termchart_gui::Backend {
public:
    struct Impl;

    // env = JNIEnv*，pixels = jintArray（长度 ≥ w*h 的 ARGB 数组）。后端会 NewGlobalRef
    // 持有该数组，析构时释放。注意：env 必须与后续 present/inject 处于同一 JVM 线程
    // （Android 主线程 / Swing EDT 均满足；跨线程需自行 AttachCurrentThread）。
    JniBackend(void* env, void* pixels, std::uint32_t width, std::uint32_t height);

    // 句柄注入形态：surface = jintArray，display = JNIEnv*，便于统一走 NativeSurface。
    explicit JniBackend(const termchart_gui::NativeSurface& host);

    ~JniBackend() override;
    JniBackend(const JniBackend&) = delete;
    JniBackend& operator=(const JniBackend&) = delete;

    [[nodiscard]] bool valid() const noexcept;  // env 与 pixels 均非空
    [[nodiscard]] bool is_external() const noexcept;

    void resize(std::uint32_t width, std::uint32_t height);

    // Backend 接口。
    [[nodiscard]] std::unique_ptr<termchart_gui::TkWindow> create_window(
        std::uint32_t w, std::uint32_t h, std::string title) override;
    [[nodiscard]] termchart_gui::TkCanvas& canvas() override;
    [[nodiscard]] bool poll_event(termchart_gui::Event& out) override;
    void present() override;
    void run_loop(termchart_gui::EventLoop& loop) override;
    void inject_event(termchart_gui::Event e) override;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace termchart_gui_jni

#endif  // TERMCHART_GUI_JNI_BACKEND_HPP
