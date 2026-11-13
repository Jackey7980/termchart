// jni_demo.cpp —— JNI 后端的 native 侧胶水（Java 见 java/TermchartEmbed.java）
//
// 只做四件事：create（构造后端 + 控件树 + Mount）、render（paint_all → 回写 int[]）、
// mouse（注入并派发）、destroy。Java 侧持有 int[] 当窗口表面，自己 setRGB/drawBitmap。
// 全部走 JNIEXPORT extern "C"，Android 的 System.loadLibrary 与桌面 Java 同一套符号。
#include <jni.h>

#include <cstdint>
#include <memory>

#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/widget_tree/container.hpp"
#include "termchart_gui/widget_tree/label.hpp"
#include "termchart_gui/widget_tree/mount.hpp"
#include "termchart_gui/widget_tree/progress_bar.hpp"
#include "termchart_gui/widget_tree/slider.hpp"
#include "termchart_gui_jni/jni_backend.hpp"

namespace {

struct Embed {
    std::unique_ptr<termchart_gui_jni::JniBackend> backend;
    std::unique_ptr<termchart_gui::Container> root;
    std::unique_ptr<termchart_gui::Mount> mount;
};

Embed* as_embed(jlong handle) { return reinterpret_cast<Embed*>(static_cast<std::uintptr_t>(handle)); }

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_TermchartEmbed_nativeCreate(JNIEnv* env, jclass, jintArray pixels,
                                                         jint width, jint height) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return 0;
    }
    auto e = std::make_unique<Embed>();
    const std::uint32_t w = static_cast<std::uint32_t>(width);
    const std::uint32_t h = static_cast<std::uint32_t>(height);
    e->backend = std::make_unique<termchart_gui_jni::JniBackend>(env, pixels, w, h);
    if (!e->backend->valid()) {
        return 0;
    }
    e->root = std::make_unique<termchart_gui::Container>("root");
    e->root->set_bounds(termchart_gui::Rect{0, 0, static_cast<double>(w), static_cast<double>(h)});
    e->root->emplace_child<termchart_gui::Label>("title", "Take 2 - JNI host",
                                                termchart_gui::Rgb{240, 240, 245},
                                                termchart_gui::Rgb{28, 32, 40});
    auto& slider = e->root->emplace_child<termchart_gui::Slider>("slider");
    auto& bar = e->root->emplace_child<termchart_gui::ProgressBar>("bar");
    slider.set_on_change([&bar](double v) { bar.set_value(v); });
    e->mount = std::make_unique<termchart_gui::Mount>(*e->root, *e->backend);
    e->mount->paint_all();
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(e.release()));
}

JNIEXPORT void JNICALL Java_TermchartEmbed_nativeRender(JNIEnv*, jclass, jlong handle) {
    Embed* e = as_embed(handle);
    if (e != nullptr && e->mount != nullptr) {
        e->mount->paint_all();
    }
}

JNIEXPORT void JNICALL Java_TermchartEmbed_nativeMouse(JNIEnv*, jclass, jlong handle, jint x,
                                                       jint y, jint button) {
    Embed* e = as_embed(handle);
    if (e == nullptr || e->backend == nullptr || e->mount == nullptr) {
        return;
    }
    e->backend->inject_event(termchart_gui::tk_event_map::mouse_press(
        static_cast<double>(x), static_cast<double>(y), static_cast<int>(button)));
    termchart_gui::Event ev;
    while (e->backend->poll_event(ev)) {
        (void)e->mount->dispatch_event(ev);
    }
    e->mount->paint_all();
}

JNIEXPORT void JNICALL Java_TermchartEmbed_nativeDestroy(JNIEnv*, jclass, jlong handle) {
    Embed* e = as_embed(handle);
    delete e;  // unique_ptr 逆序析构：mount → root → backend（backend 最后释放 global ref）
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) { return JNI_VERSION_1_6; }

}  // extern "C"
