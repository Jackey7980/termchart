// backend_tk/native_surface.hpp —— 外部宿主表面描述（Take 2：用户拥有窗口）
//
// 平台无关的「句柄注入」载体：用户自己创建并持有窗口（HWND / X11 Window / NSView /
// Java 像素缓冲……），把它的原生句柄经本结构交给后端。后端据此把渲染好的 Bitmap
// 贴到该表面，**不创建、不拥有**该窗口；事件由宿主的消息泵剥壳后经 inject_event 回传。
//
// 本头刻意只含 void* / 整数，零平台类型、零第三方依赖——核心与具体后端都不因此
// 泄漏 <windows.h>/<X11/Xlib.h>/<AppKit/AppKit.h>，与 Take 1（库拥有窗口）完全并存。
#ifndef TERMCHART_GUI_BACKEND_TK_NATIVE_SURFACE_HPP
#define TERMCHART_GUI_BACKEND_TK_NATIVE_SURFACE_HPP

#include <cstdint>

namespace termchart_gui {

// surface：目标表面句柄。各后端约定：
//   Win32  → HWND（贴客户区）
//   X11    → Window(XID)，配合 display 使用
//   Cocoa  → NSView*（ARC 下按 __bridge 语义使用，见 adapter）
//   JNI    → jintArray（ARGB 像素缓冲）或 Android Surface（见 adapter_jni）
// display：辅助句柄，目前仅 X11 用（Display*）；其它平台忽略。
struct NativeSurface {
    void* surface = nullptr;
    void* display = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_BACKEND_TK_NATIVE_SURFACE_HPP
