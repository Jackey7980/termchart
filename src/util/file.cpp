// file.cpp —— util/file.hpp 的同名实现锚点
//
// 平台差异在 file_win32.cpp / file_posix.cpp；这里放两者共用的参数校验，
// 既满足「一个公开头一个同名实现」的自洽约定，也免去两处重复判断。
#include "termchart/util/file.hpp"

namespace termchart::util::detail {

bool open_args_valid(const std::string& path, const char* mode) noexcept {
    return !path.empty() && mode != nullptr && *mode != '\0';
}

}  // namespace termchart::util::detail
