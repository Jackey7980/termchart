// internal.hpp —— protocol/ 各 .cpp 共享的小工具（非公开，仅 src 内）
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "termchart/util/base64.hpp"

namespace termchart::protocol {

inline constexpr char kEsc = '\x1b';

// base64 的**唯一实现**在 util/base64.hpp（L1）：term/ 的 OSC 52 剪贴板也要用，
// 而 term（L2）在 protocol（L9）之下、不能反向依赖，所以实现必须沉到 util。
// 这里只是转引，让 kitty / iterm2 的调用点继续写作 protocol::base64_encode。
using util::base64_encode;

}  // namespace termchart::protocol
