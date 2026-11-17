// termchart_jni.cpp —— Java 高层出图桥（JNI glue，包 termchart_capi）
//
// 设计：本 TU 只做三件事——把 jdoubleArray 读成 C 数组、调 tc_* C ABI、把 C 结果
// 转成 jstring / 抛 TermchartException。所有 C++ 复杂度都关在 capi 后面，这里不 include
// 任何 termchart C++ 头（只 include termchart_capi.h + JNI 生成头）。
//
// JNI 方法签名由 `javac -h`（CMake 里）生成到 Termchart.h，本文件 include 它，
// 避免手写函数签名出错。
#include <jni.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Termchart.h"
#include "termchart_capi.h"

namespace {

// 从 tc_error_handle 读出消息并抛 TermchartException；s 非 0 时调用。
void throw_if_error(JNIEnv* env, tc_status s, tc_error_handle err) {
    if (s == TC_OK) {
        if (err) tc_error_free(err);
        return;
    }
    std::string msg = "termchart C ABI error";
    if (err) {
        char buf[512] = {};
        tc_error_message(err, buf, sizeof(buf));
        if (buf[0]) msg = buf;
        tc_error_free(err);
    }
    jclass cls = env->FindClass("TermchartException");
    if (cls != nullptr) {
        env->ThrowNew(cls, msg.c_str());
        env->DeleteLocalRef(cls);
    }
}

// 读 jdoubleArray → std::vector<double>（JNI 只读拷贝；数组元素可能是 null）。
std::vector<double> read_doubles(JNIEnv* env, jdoubleArray arr) {
    const jsize n = arr ? env->GetArrayLength(arr) : 0;
    std::vector<double> out(static_cast<std::size_t>(n));
    if (n > 0 && arr) {
        env->GetDoubleArrayRegion(arr, 0, n, out.data());
    }
    return out;
}

// 把 C 侧堆串（tc_free_string 释放）转成 jstring 并释放。
jstring to_jstring(JNIEnv* env, char* cstr) {
    if (!cstr) return nullptr;
    jstring result = env->NewStringUTF(cstr);
    tc_free_string(cstr);
    return result;
}

// 用折线/散点入口出图：create → add_points → render_ansi → 释放。
jstring plot_impl(JNIEnv* env, jdoubleArray x, jdoubleArray y, bool scatter) {
    const std::vector<double> xv = read_doubles(env, x);
    const std::vector<double> yv = read_doubles(env, y);
    if (xv.empty() || yv.empty() || xv.size() != yv.size()) {
        jclass cls = env->FindClass("TermchartException");
        if (cls != nullptr) {
            env->ThrowNew(cls, "plot expects non-empty x/y of equal length");
            env->DeleteLocalRef(cls);
        }
        return nullptr;
    }

    tc_error_handle err = nullptr;
    tc_chart_handle chart = tc_chart_create(scatter ? "scatter" : "line", &err);
    if (!chart) {
        throw_if_error(env, TC_ERR_INVALID_HANDLE, err);
        return nullptr;
    }

    tc_status s = tc_chart_add_points(chart, xv.data(), yv.data(),
                                      static_cast<uint64_t>(xv.size()));
    if (s != TC_OK) {
        tc_chart_free(chart);
        throw_if_error(env, s, nullptr);
        return nullptr;
    }

    char* ansi = nullptr;
    s = tc_chart_render_ansi(chart, nullptr, &ansi, &err);
    tc_chart_free(chart);
    if (s != TC_OK) {
        throw_if_error(env, s, err);
        return nullptr;
    }
    return to_jstring(env, ansi);
}

// 柱状：values（+可选 labels）→ C 指针数组 → render_ansi。
jstring bar_impl(JNIEnv* env, jdoubleArray values, jobjectArray labels) {
    const std::vector<double> vals = read_doubles(env, values);
    if (vals.empty()) {
        jclass cls = env->FindClass("TermchartException");
        if (cls != nullptr) {
            env->ThrowNew(cls, "bar expects non-empty values");
            env->DeleteLocalRef(cls);
        }
        return nullptr;
    }

    std::vector<std::string> label_storage;
    std::vector<const char*> label_ptrs;
    const jsize nlabels = labels ? env->GetArrayLength(labels) : 0;
    if (nlabels > 0) {
        label_storage.reserve(static_cast<std::size_t>(nlabels));
        for (jsize i = 0; i < nlabels; ++i) {
            jstring jl = static_cast<jstring>(env->GetObjectArrayElement(labels, i));
            if (jl) {
                const char* chars = env->GetStringUTFChars(jl, nullptr);
                label_storage.emplace_back(chars ? chars : "");
                if (chars) env->ReleaseStringUTFChars(jl, chars);
                env->DeleteLocalRef(jl);
            } else {
                label_storage.emplace_back();
            }
        }
        for (const auto& s : label_storage) {
            label_ptrs.push_back(s.c_str());
        }
    }

    tc_chart_handle chart = tc_chart_create("bar", nullptr);
    if (!chart) {
        throw_if_error(env, TC_ERR_INVALID_HANDLE, nullptr);
        return nullptr;
    }
    tc_status s = tc_chart_add_bar(chart, vals.data(), static_cast<uint64_t>(vals.size()),
                                   label_ptrs.empty() ? nullptr : label_ptrs.data(),
                                   static_cast<uint64_t>(label_ptrs.size()));
    if (s != TC_OK) {
        tc_chart_free(chart);
        throw_if_error(env, s, nullptr);
        return nullptr;
    }

    tc_error_handle err = nullptr;
    char* ansi = nullptr;
    s = tc_chart_render_ansi(chart, nullptr, &ansi, &err);
    tc_chart_free(chart);
    if (s != TC_OK) {
        throw_if_error(env, s, err);
        return nullptr;
    }
    return to_jstring(env, ansi);
}

}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL Java_Termchart_version(JNIEnv* env, jclass) {
    return env->NewStringUTF(tc_version_string());
}

JNIEXPORT jboolean JNICALL Java_Termchart_init(JNIEnv*, jclass) {
    tc_init();
    return tc_core_initialized() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_Termchart_plot(JNIEnv* env, jclass, jdoubleArray x,
                                              jdoubleArray y) {
    return plot_impl(env, x, y, false);
}

JNIEXPORT jstring JNICALL Java_Termchart_scatter(JNIEnv* env, jclass, jdoubleArray x,
                                                 jdoubleArray y) {
    return plot_impl(env, x, y, true);
}

JNIEXPORT jstring JNICALL Java_Termchart_bar(JNIEnv* env, jclass, jdoubleArray values) {
    return bar_impl(env, values, nullptr);
}

JNIEXPORT jstring JNICALL Java_Termchart_barWithLabels(JNIEnv* env, jclass,
                                                       jdoubleArray values,
                                                       jobjectArray labels) {
    return bar_impl(env, values, labels);
}

JNIEXPORT jstring JNICALL Java_Termchart_renderLine(JNIEnv* env, jclass, jstring text) {
    const char* chars = text ? env->GetStringUTFChars(text, nullptr) : nullptr;
    tc_error_handle err = nullptr;
    char* ansi = nullptr;
    const tc_status s = tc_render_line(chars ? chars : "", nullptr, nullptr, &ansi, &err);
    if (chars) env->ReleaseStringUTFChars(text, chars);
    if (s != TC_OK) {
        throw_if_error(env, s, err);
        return nullptr;
    }
    return to_jstring(env, ansi);
}

JNIEXPORT jstring JNICALL Java_Termchart_kinds(JNIEnv* env, jclass) {
    tc_error_handle err = nullptr;
    uint64_t len = 0;
    tc_status s = tc_chart_kinds(nullptr, 0, &len, &err);
    if (s != TC_OK && s != TC_ERR_BUFFER_TOO_SMALL) {
        throw_if_error(env, s, err);
        return nullptr;
    }
    if (err) {
        tc_error_free(err);
        err = nullptr;
    }
    std::vector<char> buf(static_cast<std::size_t>(len) + 1, '\0');
    s = tc_chart_kinds(buf.data(), len, &len, &err);
    if (s != TC_OK) {
        throw_if_error(env, s, err);
        return nullptr;
    }
    return env->NewStringUTF(buf.data());
}

}  // extern "C"
