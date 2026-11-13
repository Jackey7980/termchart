// jni_backend.cpp —— JNI 后端实现（平台胶水，<jni.h> 只在本文件出现）
//
// 表面 = jintArray（ARGB）。present() 把 BitmapCanvas 的 RGBA 重排成 ARGB 后一次
// SetIntArrayRegion 写回，避免逐像素 JNI 调用（跨语言边界的开销是数量级的）。
#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "termchart_gui/backend_tk/backend.hpp"
#include "termchart_gui/backend_tk/tk_event_map.hpp"
#include "termchart_gui/backend_tk/tk_window.hpp"
#include "termchart_gui/native_loop/loop.hpp"
#include "termchart_gui_jni/jni_backend.hpp"

namespace termchart_gui_jni {

struct JniBackend::Impl {
    JNIEnv* env = nullptr;
    jintArray arr = nullptr;  // 全局引用（跨 native 调用保活）
    std::uint32_t w = 1;
    std::uint32_t h = 1;
    bool external = true;
    termchart_gui::BitmapCanvas canvas;
    std::deque<termchart_gui::Event> queue;

    Impl(JNIEnv* e, jintArray a, std::uint32_t width, std::uint32_t height)
        : env(e), arr(a), w(width > 0 ? width : 1), h(height > 0 ? height : 1),
          canvas(width > 0 ? width : 1, height > 0 ? height : 1) {}

    // 构造后再调用（NewGlobalRef 需要 env 有效；放到构造函数体内也可，但显式更清楚）。
    void retain() {
        if (env != nullptr && arr != nullptr) {
            arr = static_cast<jintArray>(env->NewGlobalRef(arr));
        }
    }

    ~Impl() {
        if (env != nullptr && arr != nullptr) {
            env->DeleteGlobalRef(arr);
            arr = nullptr;
        }
        env = nullptr;
    }

    void paint() {
        if (env == nullptr || arr == nullptr) {
            return;
        }
        const termchart_gui::Bitmap& b = canvas.pixels();
        const std::uint32_t bw = b.width();
        const std::uint32_t bh = b.height();
        const jsize len = env->GetArrayLength(arr);
        if (len <= 0) {
            return;
        }
        const std::uint32_t cols = std::min(w, bw);
        const std::uint32_t rows = std::min(h, bh);
        const jsize need = static_cast<jsize>(cols * rows);
        if (need <= 0 || need > len) {
            return;
        }
        std::vector<jint> buf(static_cast<std::size_t>(need));
        const std::uint8_t* p = b.data();
        std::size_t di = 0;
        for (std::uint32_t y = 0; y < rows; ++y) {
            for (std::uint32_t x = 0; x < cols; ++x) {
                const std::uint8_t* s =
                    p + (static_cast<std::size_t>(y) * static_cast<std::size_t>(bw) +
                         static_cast<std::size_t>(x)) *
                            4;
                // Bitmap 是 RGBA；Java int[] 约定 ARGB（0xAARRGGBB）。
                const jint argb = (static_cast<jint>(255) << 24) |
                                  (static_cast<jint>(s[0]) << 16) |
                                  (static_cast<jint>(s[1]) << 8) |
                                  static_cast<jint>(s[2]);
                buf[di++] = argb;
            }
        }
        env->SetIntArrayRegion(arr, 0, need, buf.data());
    }
};

JniBackend::JniBackend(void* env, void* pixels, std::uint32_t width, std::uint32_t height) {
    impl_ = std::make_unique<Impl>(static_cast<JNIEnv*>(env), static_cast<jintArray>(pixels),
                                   width, height);
    impl_->retain();
}

JniBackend::JniBackend(const termchart_gui::NativeSurface& host)
    : JniBackend(host.display, host.surface, host.width, host.height) {}

JniBackend::~JniBackend() = default;

bool JniBackend::valid() const noexcept {
    return impl_ != nullptr && impl_->env != nullptr && impl_->arr != nullptr;
}

bool JniBackend::is_external() const noexcept { return impl_ != nullptr && impl_->external; }

void JniBackend::resize(std::uint32_t width, std::uint32_t height) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->w = width > 0 ? width : 1;
    impl_->h = height > 0 ? height : 1;
    impl_->canvas.resize(impl_->w, impl_->h);
    impl_->queue.push_back(termchart_gui::tk_event_map::resize(impl_->w, impl_->h));
}

std::unique_ptr<termchart_gui::TkWindow> JniBackend::create_window(std::uint32_t,
                                                                   std::uint32_t, std::string) {
    return nullptr;  // 窗口归 Java/Android 宿主
}

termchart_gui::TkCanvas& JniBackend::canvas() { return impl_->canvas; }

bool JniBackend::poll_event(termchart_gui::Event& out) {
    if (impl_ == nullptr || impl_->queue.empty()) {
        return false;
    }
    out = impl_->queue.front();
    impl_->queue.pop_front();
    return true;
}

void JniBackend::present() {
    if (impl_ != nullptr) {
        impl_->paint();
    }
}

void JniBackend::run_loop(termchart_gui::EventLoop& loop) { loop.run(); }

void JniBackend::inject_event(termchart_gui::Event e) {
    if (impl_ != nullptr) {
        impl_->queue.push_back(std::move(e));
    }
}

}  // namespace termchart_gui_jni
