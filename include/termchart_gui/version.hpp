// version.hpp —— termchart_gui 平行栈版本宏
//
// 与 include/termchart/ 下的 GUI 渲染核心（termchart::gui）不同，本目录是
// 独立的平行栈（widget_tree / native_loop / layout / backend_tk），命名空间
// 用 termchart_gui 以避开 termchart::gui。
#ifndef TERMCHART_GUI_VERSION_HPP
#define TERMCHART_GUI_VERSION_HPP

#include <cstdint>
#include <string>

namespace termchart_gui {

constexpr std::uint32_t kVersionMajor = 0;
constexpr std::uint32_t kVersionMinor = 1;
constexpr std::uint32_t kVersionPatch = 0;

struct Version {
    std::uint32_t major = kVersionMajor;
    std::uint32_t minor = kVersionMinor;
    std::uint32_t patch = kVersionPatch;
};

constexpr Version version() noexcept { return {}; }

inline std::string version_string() {
    return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." +
           std::to_string(kVersionPatch);
}

}  // namespace termchart_gui

#endif  // TERMCHART_GUI_VERSION_HPP
