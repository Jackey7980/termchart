// dynamic.hpp —— 动态库加载（可选能力）
//
// 平台差异全部落在 src/plugin/dynamic.cpp（公开头里没有 #ifdef，与 term/ 的
// 平台隔离约定一致）。本模块是**可选能力**：静态插件与注册表路径不依赖它，
// 在禁用动态加载的环境下照常工作。
//
// 入口点约定：动态库导出
//   extern "C" const termchart::plugin::Manifest* termchart_plugin_entry();
// 返回**静态存储期**的 Manifest（不得为 nullptr）；宿主据其 name/version
// 决定是否加载。Manifest 指向宿主不会修改的静态数据，因此指针语义安全。
#ifndef TERMCHART_PLUGIN_DYNAMIC_HPP
#define TERMCHART_PLUGIN_DYNAMIC_HPP

#include <string_view>

#include "termchart/core/expected.hpp"
#include "termchart/export.hpp"
#include "termchart/plugin/manifest.hpp"

namespace termchart::plugin {

class DynamicLibrary {
public:
    DynamicLibrary() noexcept = default;
    TERMCHART_API ~DynamicLibrary();

    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // 打开动态库。空路径 → InvalidArgument；打不开 → PlatformError。
    [[nodiscard]] TERMCHART_API static Result<DynamicLibrary> open(std::string_view path) noexcept;

    // 符号查找；未打开或未找到 → nullptr。
    [[nodiscard]] TERMCHART_API void* symbol(const char* name) const noexcept;

    [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }

    // 幂等：未打开时是 no-op。析构自动调用。
    TERMCHART_API void close() noexcept;

    // 入口点符号名（见文件头约定）。
    static constexpr const char* kEntrySymbol = "termchart_plugin_entry";

private:
    explicit DynamicLibrary(void* handle) noexcept : handle_(handle) {}

    void* handle_ = nullptr;
};

// 从已打开的动态库读取插件清单：未打开 / 找不到入口点 → Unsupported；
// 入口点返回 nullptr 或清单 name 为空 → InvalidArgument。
[[nodiscard]] TERMCHART_API Result<Manifest> read_manifest(const DynamicLibrary& library) noexcept;

}  // namespace termchart::plugin

#endif  // TERMCHART_PLUGIN_DYNAMIC_HPP
